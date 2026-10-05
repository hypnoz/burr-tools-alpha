/* BurrTools
 *
 * BurrTools is the legal property of its developers, whose
 * names are listed in the COPYRIGHT file, which is included
 * within the source distribution.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */
#include "blockpack.h"
#include "helperpool.h"

#include <zlib.h>

#include <algorithm>
#include <cstring>
#include <fstream>
#include <thread>
#include <vector>

namespace blockpack {

namespace {

const char SIGNATURE[4] = {'B', 'T', 'Z', '1'};
/* Raw bytes a chunk covers: a multiple of the 16-byte key, and small enough
 * that zlib's 32-bit lengths (on Windows) always hold it. */
const size_t CHUNK = 64u << 20;
/* A chunk's packed data may claim at most this; more means damage. */
const uint64_t MOST = 2 * (uint64_t)CHUNK + (1u << 20);

enum { KIND_DEFLATE = 0, KIND_KEYS = 1 };

unsigned int threadsFor(unsigned int asked) {
  if (asked)
    return asked;
  /* the limit of the application, BURRTOOLS_THREADS, or every core */
  return solveThreadBudget();
}

template <class T>
void put(std::ostream & out, T v) {
  unsigned char b[sizeof(T)];
  for (size_t i = 0; i < sizeof(T); i++)
    b[i] = (unsigned char)((uint64_t)v >> (8 * i));
  out.write(reinterpret_cast<const char *>(b), sizeof(T));
}

template <class T>
bool get(std::istream & in, T & v) {
  unsigned char b[sizeof(T)];
  if (!in.read(reinterpret_cast<char *>(b), sizeof(T)))
    return false;
  uint64_t r = 0;
  for (size_t i = 0; i < sizeof(T); i++)
    r |= (uint64_t)b[i] << (8 * i);
  v = (T)r;
  return true;
}

/* A key as the search stores it: two 64-bit words, high first in memory. */
struct key_c {
  uint64_t hi, lo;
};

key_c readKey(const unsigned char * p) {
  key_c k;
  std::memcpy(&k.hi, p, 8);
  std::memcpy(&k.lo, p + 8, 8);
  return k;
}

void writeKey(unsigned char * p, const key_c & k) {
  std::memcpy(p, &k.hi, 8);
  std::memcpy(p + 8, &k.lo, 8);
}

bool less(const key_c & a, const key_c & b) {
  return a.hi < b.hi || (a.hi == b.hi && a.lo < b.lo);
}

/* The gaps between sorted keys, as variable-length numbers: 7 bits a byte,
 * low first, the top bit set while more follow. False when the keys are
 * not in increasing order. */
bool encodeKeys(const unsigned char * raw, size_t bytes, std::vector<unsigned char> & enc) {
  enc.clear();
  enc.reserve(bytes / 4);
  key_c prev{0, 0};
  for (size_t i = 0; i < bytes; i += 16) {
    const key_c k = readKey(raw + i);
    if (i && !less(prev, k))
      return false;
    /* k - prev, in 128 bits */
    uint64_t lo = k.lo - prev.lo;
    uint64_t hi = k.hi - prev.hi - (k.lo < prev.lo ? 1 : 0);
    do {
      unsigned char b = (unsigned char)(lo & 0x7f);
      lo = (lo >> 7) | (hi << 57);
      hi >>= 7;
      if (lo || hi)
        b |= 0x80;
      enc.push_back(b);
    } while (lo || hi);
    prev = k;
  }
  return true;
}

bool decodeKeys(const unsigned char * enc, size_t encBytes, unsigned char * raw, size_t rawBytes) {
  key_c prev{0, 0};
  size_t p = 0;
  for (size_t i = 0; i < rawBytes; i += 16) {
    uint64_t lo = 0, hi = 0;
    unsigned int shift = 0;
    for (;;) {
      if (p >= encBytes || shift >= 128)
        return false;
      const uint64_t b = enc[p++];
      const uint64_t v = b & 0x7f;
      if (shift < 64) {
        lo |= v << shift;
        if (shift > 57)
          hi |= v >> (64 - shift);
      } else {
        hi |= v << (shift - 64);
      }
      shift += 7;
      if (!(b & 0x80))
        break;
    }
    key_c k;
    k.lo = prev.lo + lo;
    k.hi = prev.hi + hi + (k.lo < prev.lo ? 1 : 0);
    writeKey(raw + i, k);
    prev = k;
  }
  return p == encBytes;
}

/* One chunk on its way in or out. */
struct chunk_c {
  unsigned char kind = KIND_DEFLATE;
  std::vector<unsigned char> raw;
  std::vector<unsigned char> enc;     // what is deflated: raw itself, or the key gaps
  std::vector<unsigned char> packed;
  uint64_t rawBytes = 0, encBytes = 0;
  bool ok = true;
};

void packChunk(chunk_c & c, bool keys) {
  c.rawBytes = c.raw.size();
  const unsigned char * src = c.raw.data();
  size_t srcBytes = c.raw.size();
  c.kind = KIND_DEFLATE;
  if (keys && c.raw.size() % 16 == 0 && encodeKeys(c.raw.data(), c.raw.size(), c.enc)) {
    c.kind = KIND_KEYS;
    src = c.enc.data();
    srcBytes = c.enc.size();
  }
  c.encBytes = srcBytes;
  uLongf out = compressBound((uLong)srcBytes);
  c.packed.resize(out);
  c.ok = compress2(c.packed.data(), &out, src, (uLong)srcBytes, Z_DEFAULT_COMPRESSION) == Z_OK;
  c.packed.resize(out);
  std::vector<unsigned char>().swap(c.enc);
}

void unpackChunk(chunk_c & c) {
  if (c.kind != KIND_DEFLATE && c.kind != KIND_KEYS) {
    c.ok = false;
    return;
  }
  std::vector<unsigned char> & dst = c.kind == KIND_KEYS ? c.enc : c.raw;
  dst.resize(c.encBytes);
  uLongf out = (uLongf)c.encBytes;
  c.ok = uncompress(dst.data(), &out, c.packed.data(), (uLong)c.packed.size()) == Z_OK &&
         out == c.encBytes;
  if (c.ok && c.kind == KIND_KEYS) {
    c.raw.resize(c.rawBytes);
    c.ok = c.rawBytes % 16 == 0 && decodeKeys(c.enc.data(), c.enc.size(), c.raw.data(), c.raw.size());
  }
  std::vector<unsigned char>().swap(c.enc);
  std::vector<unsigned char>().swap(c.packed);
}

/* Runs f(i) for i in 0..n-1, one thread each. */
template <class F>
void inParallel(size_t n, F f) {
  if (n <= 1) {
    if (n)
      f(0);
    return;
  }
  std::vector<std::thread> pool;
  for (size_t i = 0; i < n; i++)
    pool.emplace_back(f, i);
  for (auto & t : pool)
    t.join();
}

} // namespace

uint64_t pack(const std::filesystem::path & file, std::ostream & out, bool keys,
              unsigned int threads, size_t chunkBytes) {
  if (chunkBytes == 0 || chunkBytes > CHUNK || chunkBytes % 16)
    chunkBytes = CHUNK;
  std::error_code ec;
  const uint64_t size = std::filesystem::file_size(file, ec);
  std::ifstream in(file, std::ios::binary);
  if (ec || !in)
    return 0;
  const std::streampos start = out.tellp();
  out.write(SIGNATURE, sizeof(SIGNATURE));
  put<uint64_t>(out, size);

  const unsigned int n = threadsFor(threads);
  uint64_t left = size;
  while (left && out) {
    /* Read a chunk for each thread, pack them together, write in order. */
    std::vector<chunk_c> batch;
    while (left && batch.size() < n) {
      chunk_c c;
      c.raw.resize((size_t)std::min<uint64_t>(left, chunkBytes));
      if (!in.read(reinterpret_cast<char *>(c.raw.data()), (std::streamsize)c.raw.size()))
        return 0;
      left -= c.raw.size();
      batch.push_back(std::move(c));
    }
    inParallel(batch.size(), [&](size_t i) { packChunk(batch[i], keys); });
    for (chunk_c & c : batch) {
      if (!c.ok)
        return 0;
      put<uint8_t>(out, c.kind);
      put<uint64_t>(out, c.rawBytes);
      put<uint64_t>(out, c.encBytes);
      put<uint64_t>(out, c.packed.size());
      out.write(reinterpret_cast<const char *>(c.packed.data()), (std::streamsize)c.packed.size());
    }
  }
  if (!out)
    return 0;
  return (uint64_t)(out.tellp() - start);
}

bool unpack(std::istream & in, uint64_t bytes, const std::filesystem::path & file,
            unsigned int threads) {
  char sig[sizeof(SIGNATURE)];
  uint64_t size = 0;
  if (bytes < sizeof(SIGNATURE) + 8 || !in.read(sig, sizeof(sig)) ||
      !std::equal(sig, sig + sizeof(sig), SIGNATURE) || !get(in, size))
    return false;
  uint64_t left = bytes - sizeof(SIGNATURE) - 8;
  std::ofstream out(file, std::ios::binary | std::ios::trunc);
  if (!out)
    return false;

  const unsigned int n = threadsFor(threads);
  uint64_t written = 0;
  while (written < size) {
    std::vector<chunk_c> batch;
    uint64_t batchBytes = 0;
    while (written + batchBytes < size && batch.size() < n) {
      chunk_c c;
      uint8_t kind = 0;
      uint64_t packedBytes = 0;
      if (left < 25 || !get(in, kind) || !get(in, c.rawBytes) || !get(in, c.encBytes) ||
          !get(in, packedBytes))
        return false;
      left -= 25;
      if (c.rawBytes == 0 || c.rawBytes > CHUNK || c.encBytes > MOST || packedBytes > MOST ||
          packedBytes > left)
        return false;
      c.kind = kind;
      c.packed.resize((size_t)packedBytes);
      if (!in.read(reinterpret_cast<char *>(c.packed.data()), (std::streamsize)packedBytes))
        return false;
      left -= packedBytes;
      batchBytes += c.rawBytes;
      batch.push_back(std::move(c));
    }
    inParallel(batch.size(), [&](size_t i) { unpackChunk(batch[i]); });
    for (chunk_c & c : batch) {
      if (!c.ok)
        return false;
      out.write(reinterpret_cast<const char *>(c.raw.data()), (std::streamsize)c.raw.size());
      written += c.raw.size();
    }
    if (!out)
      return false;
  }
  return written == size && left == 0 && (bool)out;
}

}
