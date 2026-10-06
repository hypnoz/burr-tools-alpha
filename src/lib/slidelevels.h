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
/*
 * Sorted sets of arrangement keys for the full slide search, kept in a file
 * so that a search is limited by the disk, not by memory.
 *
 * A set is written once, in rising order, as blocks of BLOCK_KEYS keys. A
 * block holds the gaps between its keys as varints, so keys close together,
 * as the arrangements of one search are, take a few bytes each. The first
 * key and the file offset of each block make the index: in memory while
 * the set is looked in, at the end of the file otherwise, so that a set
 * read only now and then costs no memory in between.
 */
#ifndef __SLIDELEVELS_H__
#define __SLIDELEVELS_H__

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <memory>
#include <string>
#include <system_error>
#include <vector>

namespace sliding {
namespace levels {

/* Keys a block holds: few enough that one key looked up decodes little,
 * many enough that the index is small beside the keys. */
const unsigned int BLOCK_KEYS = 128;
/* Longest varint of a 128-bit gap. */
const unsigned int MAX_VARINT = 19;
/* Ends a file whose index follows its blocks. */
const char SET_MAGIC[8] = {'B', 'T', 'S', 'L', 'K', 'E', 'Y', '1'};

template <class key_t>
inline unsigned int putVarint(uint8_t * out, key_t v) {
  unsigned int n = 0;
  while (v >= 0x80) {
    out[n++] = (uint8_t)((unsigned int)(v & 0x7f) | 0x80);
    v >>= 7;
  }
  out[n++] = (uint8_t)v;
  return n;
}

template <class key_t>
inline const uint8_t * getVarint(const uint8_t * p, key_t & v) {
  v = 0;
  unsigned int shift = 0;
  while (*p & 0x80) {
    v |= (key_t)(*p++ & 0x7f) << shift;
    shift += 7;
  }
  v |= (key_t)*p++ << shift;
  return p;
}

/* Read len bytes at offset; false when they are not all there. */
inline bool readAt(std::FILE * f, uint64_t offset, void * buf, size_t len) {
#ifdef _WIN32
  if (_fseeki64(f, (long long)offset, SEEK_SET) != 0)
    return false;
#else
  if (fseeko(f, (off_t)offset, SEEK_SET) != 0)
    return false;
#endif
  return std::fread(buf, 1, len, f) == len;
}

struct fileCloser_c {
  void operator()(std::FILE * f) const {
    if (f)
      std::fclose(f);
  }
};
using filePtr_t = std::unique_ptr<std::FILE, fileCloser_c>;

/* One sorted set of unique keys, in a file or, with no file name, in memory. */
template <class key_t>
class keySet_c {
public:
  keySet_c(void) = default;
  keySet_c(const keySet_c &) = delete;
  keySet_c & operator=(const keySet_c &) = delete;
  ~keySet_c(void) { remove(); }

  /* Start writing; an empty path keeps the set in memory. */
  bool create(const std::filesystem::path & p) {
    path = p;
    count = 0;
    firsts.clear();
    offsets.assign(1, 0);
    mem.clear();
    buf.clear();
    if (!path.empty()) {
      out.reset(std::fopen(path.string().c_str(), "wb"));
      if (!out)
        return false;
    }
    return true;
  }

  /* Append a key greater than the last one added. */
  bool add(key_t k) {
    if (count % BLOCK_KEYS == 0) {
      if (!flushBlock())
        return false;
      firsts.push_back(k);
    } else {
      uint8_t v[MAX_VARINT];
      const unsigned int n = putVarint(v, (key_t)(k - last));
      buf.insert(buf.end(), v, v + n);
    }
    last = k;
    count++;
    return true;
  }

  template <class It>
  bool addAll(It b, It e) {
    for (; b != e; ++b)
      if (!add(*b))
        return false;
    return true;
  }

  /* Done writing: the set can be read from now on. A file gets its index
   * written after the blocks. */
  bool finish(void) {
    if (!flushBlock())
      return false;
    firsts.shrink_to_fit();
    offsets.shrink_to_fit();
    mem.shrink_to_fit();
    std::vector<uint8_t>().swap(buf);
    if (!out)
      return true;
    const uint64_t blocks = firsts.size();
    const uint64_t tail[3] = {(uint64_t)count, blocks, offsets.back()};
    bool ok = (blocks == 0 ||
               std::fwrite(firsts.data(), sizeof(key_t), blocks, out.get()) == blocks) &&
              std::fwrite(offsets.data(), sizeof(uint64_t), blocks + 1, out.get()) == blocks + 1 &&
              std::fwrite(tail, sizeof(tail), 1, out.get()) == 1 &&
              std::fwrite(SET_MAGIC, sizeof(SET_MAGIC), 1, out.get()) == 1;
    ok = std::fflush(out.get()) == 0 && ok && !std::ferror(out.get());
    out.reset();
    return ok;
  }

  /* Free the index of a set kept in a file; it is read back when needed. */
  void release(void) {
    if (path.empty() || out)
      return;
    std::vector<key_t>().swap(firsts);
    std::vector<uint64_t>().swap(offsets);
    indexed = false;
  }

  /* Read the index back after release; false when the file is damaged. */
  bool reload(void) {
    if (indexed)
      return true;
    filePtr_t in(std::fopen(path.string().c_str(), "rb"));
    if (!in)
      return false;
    uint64_t tail[3];
    char magic[sizeof(SET_MAGIC)];
    std::error_code ec;
    const uint64_t size = std::filesystem::file_size(path, ec);
    const uint64_t footer = sizeof(tail) + sizeof(magic);
    if (ec || size < footer || !readAt(in.get(), size - footer, tail, sizeof(tail)) ||
        std::fread(magic, sizeof(magic), 1, in.get()) != 1 ||
        std::memcmp(magic, SET_MAGIC, sizeof(magic)) != 0)
      return false;
    const uint64_t blocks = tail[1];
    if (tail[2] + blocks * sizeof(key_t) + (blocks + 1) * sizeof(uint64_t) + footer != size)
      return false;
    firsts.resize(blocks);
    offsets.resize(blocks + 1);
    if ((blocks && !readAt(in.get(), tail[2], firsts.data(), blocks * sizeof(key_t))) ||
        std::fread(offsets.data(), sizeof(uint64_t), blocks + 1, in.get()) != blocks + 1)
      return false;
    count = tail[0];
    indexed = true;
    return true;
  }

  size_t size(void) const { return count; }
  /* Bytes the keys and their index take on disk, or in memory for a set
   * kept there. */
  uint64_t bytes(void) const {
    if (path.empty())
      return dataBytes;
    return dataBytes + blockCount * (sizeof(key_t) + sizeof(uint64_t)) + sizeof(uint64_t) +
           3 * sizeof(uint64_t) + sizeof(SET_MAGIC);
  }
  /* Memory the block index takes while it is in memory. */
  uint64_t indexBytes(void) const {
    return firsts.capacity() * sizeof(key_t) + offsets.capacity() * sizeof(uint64_t);
  }

  /* Delete the file, if there is one. */
  void remove(void) {
    out.reset();
    if (!path.empty()) {
      std::error_code ec;
      std::filesystem::remove(path, ec);
      path.clear();
    }
  }

  /* Reads blocks of one set: one reader per thread. */
  class reader_c {
  public:
    explicit reader_c(const keySet_c & s) : set(s) {
      if (!set.path.empty())
        in.reset(std::fopen(set.path.string().c_str(), "rb"));
    }
    bool ok(void) const { return set.path.empty() || in; }

    /* The keys of block b, into keys; false on a read error. */
    bool block(size_t b, std::vector<key_t> & keys) {
      const uint64_t from = set.offsets[b];
      const size_t len = (size_t)(set.offsets[b + 1] - from);
      const uint8_t * p;
      if (set.path.empty()) {
        p = set.mem.data() + from;
      } else {
        raw.resize(len + 1);
        if (len && (!in || !readAt(in.get(), from, raw.data(), len)))
          return false;
        p = raw.data();
      }
      const size_t n = std::min<size_t>(BLOCK_KEYS, set.count - b * BLOCK_KEYS);
      keys.resize(n);
      key_t k = set.firsts[b];
      keys[0] = k;
      for (size_t i = 1; i < n; i++) {
        key_t gap;
        p = getVarint(p, gap);
        k += gap;
        keys[i] = k;
      }
      return true;
    }

  private:
    const keySet_c & set;
    filePtr_t in;
    std::vector<uint8_t> raw;
  };

  /* All keys, in order, into keys; false on a read error. */
  bool load(std::vector<key_t> & keys) {
    keys.clear();
    if (!reload())
      return false;
    keys.reserve(count);
    reader_c r(*this);
    std::vector<key_t> blk;
    for (size_t b = 0; b < firsts.size(); b++) {
      if (!r.block(b, blk))
        return false;
      keys.insert(keys.end(), blk.begin(), blk.end());
    }
    return r.ok();
  }

  /* Every key in rising order, one block at a time. */
  class cursor_c {
  public:
    explicit cursor_c(const keySet_c & s) : set(s), reader(s) {}
    /* The next key into k; false at the end or on a read error (see failed). */
    bool next(key_t & k) {
      if (at == keys.size()) {
        if (blk >= set.firsts.size())
          return false;
        if (!reader.block(blk++, keys)) {
          bad = true;
          return false;
        }
        at = 0;
      }
      k = keys[at++];
      return true;
    }
    bool failed(void) const { return bad || !reader.ok(); }

  private:
    const keySet_c & set;
    reader_c reader;
    std::vector<key_t> keys;
    size_t blk = 0;
    size_t at = 0;
    bool bad = false;
  };

  /* Set found[i] for each of the sorted keys cand[0..n) in this set. False
   * on a read error. The index must be in memory. */
  bool find(const key_t * cand, size_t n, char * found, reader_c & reader) const {
    std::vector<key_t> keys;
    size_t loaded = (size_t)-1;
    size_t b = 0;
    const size_t blocks = firsts.size();
    for (size_t i = 0; i < n; i++) {
      const key_t c = cand[i];
      if (blocks == 0 || c < firsts[0] || c > last)
        continue;
      /* The block c would be in: the last whose first key is not above it.
       * Keys rise, so look on from the last one, in doubling steps. */
      size_t step = 1;
      size_t hi = b + 1;
      while (hi < blocks && firsts[hi] <= c) {
        b = hi;
        hi = b + step;
        step *= 2;
      }
      b = (size_t)(std::upper_bound(firsts.begin() + (std::ptrdiff_t)b,
                                    firsts.begin() + (std::ptrdiff_t)std::min(hi, blocks), c) -
                   firsts.begin()) - 1;
      if (b != loaded) {
        if (!reader.block(b, keys))
          return false;
        loaded = b;
      }
      if (std::binary_search(keys.begin(), keys.end(), c))
        found[i] = 1;
    }
    return true;
  }

private:
  /* Write the block being built, if there is one: one has been started
   * (firsts) that is not yet written (offsets). */
  bool flushBlock(void) {
    if (firsts.size() < offsets.size())
      return true;
    if (out) {
      if (!buf.empty() && std::fwrite(buf.data(), 1, buf.size(), out.get()) != buf.size())
        return false;
    } else {
      mem.insert(mem.end(), buf.begin(), buf.end());
    }
    offsets.push_back(offsets.back() + buf.size());
    dataBytes = offsets.back();
    blockCount = offsets.size() - 1;
    buf.clear();
    return true;
  }

  std::filesystem::path path;
  filePtr_t out;
  std::vector<uint8_t> mem;
  /* First key of each block, and where each block starts: offsets has one
   * more entry, the end of the last block. */
  std::vector<key_t> firsts;
  std::vector<uint64_t> offsets{0};
  bool indexed = true;
  size_t count = 0;
  uint64_t dataBytes = 0;
  uint64_t blockCount = 0;
  /* While writing: the block being built. After, the greatest key. */
  std::vector<uint8_t> buf;
  key_t last = 0;
};

} // namespace levels
} // namespace sliding

#endif
