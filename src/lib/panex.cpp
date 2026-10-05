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
 * The Panex Solver. A breadth-first search of rod transfers built for large
 * Panex towers:
 *
 *  - A stacking is a 128-bit key: the discs on each rod, bottom to top,
 *    five bits a disc, with a separator between rods.
 *  - The search runs level by level, a level being every stacking a given
 *    number of moves from where it began. A transfer can always be undone,
 *    so the next level is everything one move from the current level that is
 *    in neither it nor the level before. Only those three levels need to
 *    stay in memory, as sorted arrays.
 *  - While the levels are few it keeps them all and traces the path back
 *    through them. Otherwise it solves start to meeting point and meeting
 *    point to goal the same way: each half is a far smaller search.
 *  - It searches from the start and from the goal, always growing the
 *    smaller frontier, and stops the first time they meet: that path is a
 *    shortest one. When the goal is the start's mirror image, as in the
 *    classic tower swap, the search from the goal is the mirror image of the
 *    search from the start, so only one search runs. If swapping each disc
 *    for its same-size partner then maps the start to itself, a level holds
 *    just one stacking of each such twin pair.
 *  - Each level is expanded by every core at once.
 */

#include "panex.h"
#include "helperpool.h"

#include "problem.h"
#include "puzzle.h"
#include "stacking.h"
#include "sliding.h"
#include "sysmemory.h"
#include "disassembly.h"

#include <algorithm>
#include <fstream>
#include <filesystem>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <thread>
#include <vector>

namespace panex {

namespace {

const unsigned int MAX_RODS = 8;
/* Five-bit symbols: discs 0..30, and 31 between rods. */
const unsigned int MAX_DISCS = 31;
const unsigned int SEPARATOR = 31;
/* 128 bits hold 25 symbols: the discs plus one separator per rod but one. */
const unsigned int MAX_SYMBOLS = 25;

struct pkey_c {
  uint64_t hi = 0;
  uint64_t lo = 0;
};
inline bool operator<(const pkey_c & a, const pkey_c & b) {
  return a.hi < b.hi || (a.hi == b.hi && a.lo < b.lo);
}
inline bool operator==(const pkey_c & a, const pkey_c & b) {
  return a.hi == b.hi && a.lo == b.lo;
}
inline bool operator!=(const pkey_c & a, const pkey_c & b) {
  return !(a == b);
}

using layer_t = std::vector<pkey_c>;

/* The discs on each rod, bottom to top. */
struct state_c {
  unsigned char count[MAX_RODS] = {};
  unsigned char disc[MAX_RODS][MAX_DISCS + 1] = {};
};

/* The rod set's rules, laid out for speed. They are the stacking rules
 * (stacking.cpp, moveAllowed): Panex columns, or plain rods with or without
 * the size rule; the tests check this search and findStackPath agree. */
class rules_c {
public:
  unsigned int rods = 0;
  unsigned int discs = 0;
  /* Panex columns; otherwise plain rods, where sizeRule keeps a larger disc
   * off a smaller one. */
  bool panex = true;
  bool sizeRule = false;
  unsigned char size[MAX_DISCS] = {};
  /* How deep each disc may sit in a Panex column: its size, at most the rod
   * height. On plain rods, its size. */
  unsigned char depth[MAX_DISCS] = {};
  unsigned char cap[MAX_RODS] = {};
  bool pocket[MAX_RODS] = {};
  int position[MAX_RODS] = {};
  bool distance = false;
  /* The rods mirror onto each other end to end: no pocket. */
  bool mirrorable = false;

  /* The column's top disc is raised into the bridge, blocking moves over it. */
  bool raised(const state_c & s, unsigned int r) const {
    if (!panex || pocket[r])
      return false;
    const unsigned int n = s.count[r];
    for (unsigned int i = 0; i < n; i++)
      if (depth[s.disc[r][i]] < n - i)
        return true;
    return false;
  }

  /* Calls f with each stacking one transfer away; s is restored after. */
  template <class F>
  void moves(state_c & s, F f) const {
    bool up[MAX_RODS];
    for (unsigned int r = 0; r < rods; r++)
      up[r] = raised(s, r);
    for (unsigned int from = 0; from < rods; from++) {
      if (s.count[from] == 0)
        continue;
      for (unsigned int to = 0; to < rods; to++) {
        if (to == from || s.count[to] >= cap[to])
          continue;
        /* Every disc there goes one deeper, which needs it not raised. */
        if (!pocket[to] && up[to])
          continue;
        if (sizeRule && s.count[to] &&
            size[s.disc[from][s.count[from] - 1]] > size[s.disc[to][s.count[to] - 1]])
          continue;
        const int a = position[from];
        const int b = position[to];
        const int lo = std::min(a, b);
        const int hi = std::max(a, b);
        bool blocked = false;
        for (unsigned int r = 0; r < rods && !blocked; r++)
          if (r != from && r != to && !pocket[r] && position[r] > lo && position[r] < hi && up[r])
            blocked = true;
        if (blocked || (distance && hi - lo != 1))
          continue;
        const unsigned char d = s.disc[from][--s.count[from]];
        s.disc[to][s.count[to]++] = d;
        f(s);
        s.count[to]--;
        s.disc[from][s.count[from]++] = d;
      }
    }
  }

  pkey_c encode(const state_c & s) const {
    pkey_c k;
    auto push = [&k](unsigned int v) {
      k.hi = (k.hi << 5) | (k.lo >> 59);
      k.lo = (k.lo << 5) | v;
    };
    for (unsigned int r = 0; r < rods; r++) {
      if (r)
        push(SEPARATOR);
      for (unsigned int i = 0; i < s.count[r]; i++)
        push(s.disc[r][i]);
    }
    return k;
  }

  void decode(pkey_c k, state_c & s) const {
    for (unsigned int r = 0; r < rods; r++)
      s.count[r] = 0;
    unsigned int r = rods - 1;
    for (unsigned int i = 0; i < discs + rods - 1; i++) {
      const unsigned int v = (unsigned int)(k.lo & 31);
      k.lo = (k.lo >> 5) | (k.hi << 59);
      k.hi >>= 5;
      if (v == SEPARATOR)
        r--;
      else
        s.disc[r][s.count[r]++] = (unsigned char)v;
    }
    /* Read top first: turn each rod back to bottom first. */
    for (unsigned int q = 0; q < rods; q++)
      std::reverse(s.disc[q], s.disc[q] + s.count[q]);
  }

  void mirrorState(const state_c & s, state_c & m) const {
    for (unsigned int r = 0; r < rods; r++) {
      const unsigned int q = rods - 1 - r;
      m.count[q] = s.count[r];
      std::copy(s.disc[r], s.disc[r] + s.count[r], m.disc[q]);
    }
  }

  pkey_c mirror(pkey_c k) const {
    state_c s, m;
    decode(k, s);
    mirrorState(s, m);
    return encode(m);
  }

  state_c fromStacking(const stacking::stacking_t & st) const {
    state_c s;
    for (unsigned int r = 0; r < rods; r++) {
      s.count[r] = (unsigned char)st[r].size();
      for (unsigned int i = 0; i < st[r].size(); i++)
        s.disc[r][i] = (unsigned char)st[r][i];
    }
    return s;
  }

  stacking::stacking_t toStacking(const state_c & s) const {
    stacking::stacking_t st(rods);
    for (unsigned int r = 0; r < rods; r++)
      st[r].assign(s.disc[r], s.disc[r] + s.count[r]);
    return st;
  }
};

unsigned int threadCount(unsigned int asked) {
  if (asked > 0)
    return asked;
  /* the limit of the application, BURRTOOLS_THREADS, or every core */
  return solveThreadBudget();
}

/* Runs f(t) on threads 0..n-1 and waits for all of them. */
template <class F>
void parallel(unsigned int n, F f) {
  if (n <= 1) {
    f(0u);
    return;
  }
  std::vector<std::thread> pool;
  for (unsigned int t = 0; t < n; t++)
    pool.emplace_back(f, t);
  for (auto & th : pool)
    th.join();
}

/* Drop from sorted, unique a every key that is in sorted b. Both run
 * upwards, so each key of a is looked for from where the last was found, in
 * doubling steps and then by halving: a few steps close by, where a search
 * of all the rest of b would jump about memory. */
void subtract(layer_t & a, const layer_t & b) {
  size_t w = 0;
  const pkey_c * it = b.data();
  const pkey_c * const end = b.data() + b.size();
  for (size_t i = 0; i < a.size(); i++) {
    size_t step = 1;
    const pkey_c * lo = it;
    while (lo + step < end && lo[step] < a[i]) {
      lo += step;
      step *= 2;
    }
    it = std::lower_bound(lo, std::min(lo + step + 1, end), a[i]);
    if (it == end || *it != a[i])
      a[w++] = a[i];
  }
  a.resize(w);
}

/* Sort keys of at most bits significant bits: by 11 bits at a time from the
 * low end, each pass keeping the order of the one before. tmp is scratch.
 * Several times quicker than a comparison sort on the hundreds of thousands
 * of keys one batch of a level makes. */
void radixSort(layer_t & keys, layer_t & tmp, unsigned int bits) {
  if (keys.size() < 2048) {
    std::sort(keys.begin(), keys.end());
    return;
  }
  const unsigned int DIGIT = 11;
  tmp.resize(keys.size());
  pkey_c * from = keys.data();
  pkey_c * to = tmp.data();
  const size_t n = keys.size();
  for (unsigned int shift = 0; shift < bits; shift += DIGIT) {
    size_t count[1u << DIGIT] = {};
    auto digit = [shift](const pkey_c & k) {
      const uint64_t v = shift >= 64 ? k.hi >> (shift - 64)
                         : shift == 0 ? k.lo
                         : (k.lo >> shift) | (k.hi << (64 - shift));
      return (size_t)(v & ((1u << DIGIT) - 1));
    };
    for (size_t i = 0; i < n; i++)
      count[digit(from[i])]++;
    /* Every key alike in this digit: nothing to move. */
    if (count[digit(from[0])] == n)
      continue;
    size_t at = 0;
    for (size_t d = 0; d < (1u << DIGIT); d++) {
      const size_t c = count[d];
      count[d] = at;
      at += c;
    }
    for (size_t i = 0; i < n; i++)
      to[count[digit(from[i])]++] = from[i];
    std::swap(from, to);
  }
  if (from != keys.data())
    keys.swap(tmp);
}

/* Merge two sorted, unique runs into one. */
layer_t mergeTwo(const pkey_c * a, const pkey_c * aEnd, const pkey_c * b, const pkey_c * bEnd) {
  layer_t out((size_t)(aEnd - a) + (size_t)(bEnd - b));
  auto end = std::merge(a, aEnd, b, bEnd, out.begin());
  out.resize((size_t)(std::unique(out.begin(), end) - out.begin()));
  return out;
}

/* Merge sorted, unique runs into one, pairwise, on this thread. Empties runs. */
layer_t mergeRuns(std::vector<layer_t> & runs) {
  if (runs.empty())
    return layer_t();
  while (runs.size() > 1) {
    std::vector<layer_t> next((runs.size() + 1) / 2);
    for (size_t i = 0; i < next.size(); i++) {
      if (2 * i + 1 >= runs.size()) {
        next[i].swap(runs[2 * i]);
        continue;
      }
      layer_t & a = runs[2 * i];
      layer_t & b = runs[2 * i + 1];
      next[i] = mergeTwo(a.data(), a.data() + a.size(), b.data(), b.data() + b.size());
      layer_t().swap(a);
      layer_t().swap(b);
    }
    runs.swap(next);
  }
  layer_t out;
  out.swap(runs[0]);
  return out;
}

/* Merge sorted, unique parts into one sorted, unique layer. Empties parts.
 * With threads the keys are cut into as many stretches, by value, and each
 * thread merges one stretch of every part: no step is left to one thread,
 * as the last of a pairwise merge would be. */
layer_t mergeParts(std::vector<layer_t> & parts, unsigned int threads) {
  size_t total = 0;
  for (const layer_t & p : parts)
    total += p.size();
  if (threads <= 1 || parts.size() <= 1 || total < 200000)
    return mergeRuns(parts);

  /* Cut points: evenly spaced keys of a sample of every part. */
  layer_t sample;
  for (const layer_t & p : parts) {
    const size_t step = std::max<size_t>(1, p.size() / 64);
    for (size_t i = step / 2; i < p.size(); i += step)
      sample.push_back(p[i]);
  }
  std::sort(sample.begin(), sample.end());
  layer_t cut(threads - 1);
  for (unsigned int t = 0; t + 1 < threads; t++)
    cut[t] = sample[(size_t)(t + 1) * sample.size() / threads];

  /* The first round reads the parts and writes new runs; the parts go
   * before the later rounds, so no more is held than a pairwise merge holds. */
  std::vector<std::vector<layer_t>> runs(threads);
  parallel(threads, [&](unsigned int t) {
    std::vector<std::pair<const pkey_c *, const pkey_c *>> mine;
    for (const layer_t & p : parts) {
      const pkey_c * lo = t ? std::lower_bound(p.data(), p.data() + p.size(), cut[t - 1]) : p.data();
      const pkey_c * hi = t + 1 < threads ? std::lower_bound(p.data(), p.data() + p.size(), cut[t])
                                          : p.data() + p.size();
      if (lo < hi)
        mine.push_back({lo, hi});
    }
    for (size_t i = 0; i < mine.size(); i += 2) {
      if (i + 1 < mine.size())
        runs[t].push_back(mergeTwo(mine[i].first, mine[i].second, mine[i + 1].first, mine[i + 1].second));
      else
        runs[t].emplace_back(mine[i].first, mine[i].second);
    }
  });
  std::vector<layer_t>().swap(parts);

  std::vector<layer_t> stretch(threads);
  parallel(threads, [&](unsigned int t) { stretch[t] = mergeRuns(runs[t]); });

  total = 0;
  for (const layer_t & p : stretch)
    total += p.size();
  layer_t out;
  out.reserve(total);
  for (layer_t & p : stretch) {
    out.insert(out.end(), p.begin(), p.end());
    layer_t().swap(p);
  }
  return out;
}

/* Sort and drop duplicates, on every thread. */
void parallelSort(layer_t & keys, unsigned int threads, unsigned int bits) {
  if (threads <= 1 || keys.size() < 100000) {
    layer_t scratch;
    radixSort(keys, scratch, bits);
    keys.erase(std::unique(keys.begin(), keys.end()), keys.end());
    return;
  }
  std::vector<layer_t> parts(threads);
  const size_t chunk = (keys.size() + threads - 1) / threads;
  parallel(threads, [&](unsigned int t) {
    const size_t lo = std::min(keys.size(), t * chunk);
    const size_t hi = std::min(keys.size(), lo + chunk);
    parts[t].assign(keys.begin() + (ptrdiff_t)lo, keys.begin() + (ptrdiff_t)hi);
    layer_t scratch;
    radixSort(parts[t], scratch, bits);
    parts[t].erase(std::unique(parts[t].begin(), parts[t].end()), parts[t].end());
  });
  layer_t().swap(keys);
  keys = mergeParts(parts, threads);
}

/* The first key in both sorted layers. */
bool meet(const layer_t & a, const layer_t & b, pkey_c & at) {
  auto i = a.begin();
  auto j = b.begin();
  while (i != a.end() && j != b.end()) {
    if (*i < *j)
      ++i;
    else if (*j < *i)
      ++j;
    else {
      at = *i;
      return true;
    }
  }
  return false;
}

/* One end of a search: its two newest levels, and every level while the
 * search is small enough to keep them for tracing the path back. */
struct side_c {
  layer_t prev;
  layer_t cur;
  unsigned int depth = 0;
  std::vector<layer_t> kept;
};

/* Where two searches met: the stacking, how far it is from each end, and
 * the whole path when it could be traced. */
struct meet_c {
  pkey_c at;
  unsigned int fromStart = 0;
  unsigned int fromGoal = 0;
  std::vector<pkey_c> path;
};

/* A search's frontier and progress, as saved for resume. */
struct savedState_c {
  uint64_t fingerprint = 0;
  uint32_t fwdDepth = 0;
  uint32_t bwdDepth = 0;
  uint32_t interval = 1;
  uint64_t found = 0;
  /* Milliseconds spent solving so far, over every run of this search. */
  uint64_t ms = 0;
  /* Once the ends have met: where, and how far from each end. Then only
   * tracing the path back is left, and the frontier is not saved. */
  uint32_t met = 0;
  uint32_t fromStart = 0;
  uint32_t fromGoal = 0;
  pkey_c meetAt;
  layer_t fwdPrev, fwdCur, bwdPrev, bwdCur, mirrorPrev;
};

const char STATE_MAGIC[8] = {'B', 'T', 'P', 'A', 'N', 'E', 'X', '4'};
/* Version 3 was the same but kept whole seconds where 4 keeps ms. */
const char STATE_MAGIC_SECONDS[8] = {'B', 'T', 'P', 'A', 'N', 'E', 'X', '3'};

/* One puzzle's saved search, in its own folder: every few levels of each
 * end ("f12.lvl", "b0.lvl"), sorted keys, and the frontier ("state.bin").
 * The levels kept are those a multiple of interval apart; when they outgrow
 * the disk budget, interval doubles and every other one goes. */
class store_c {
public:
  std::filesystem::path dir;
  unsigned int interval = 1;
  unsigned long long budget = 0;
  unsigned long long used = 0;

  bool open(const std::filesystem::path & d, unsigned long long diskBudget) {
    dir = d;
    budget = diskBudget;
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    if (ec)
      return false;
    used = 0;
    for (const auto & e : std::filesystem::directory_iterator(dir, ec))
      if (e.path().extension() == ".lvl")
        used += e.file_size(ec);
    return true;
  }

  void wipe(void) {
    std::error_code ec;
    for (const auto & e : std::filesystem::directory_iterator(dir, ec))
      std::filesystem::remove(e.path(), ec);
    used = 0;
    interval = 1;
  }

  void remove(void) {
    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
  }

  bool writeLevel(char side, unsigned int level, const layer_t & keys) {
    if (!keepsLevel(level))
      return true;
    if (!writeKeys(levelPath(side, level), keys.data(), keys.size()))
      return false;
    wrote(keys.size());
    return true;
  }

  /* writeLevel in three steps, for writing a level on another thread:
   * keepsLevel says whether it is one to save, writeKeys(levelFile(...))
   * writes it, and wrote counts it against the budget. */
  bool keepsLevel(unsigned int level) const { return level % interval == 0; }

  std::filesystem::path levelFile(char side, unsigned int level) const {
    return levelPath(side, level);
  }

  static bool writeKeys(const std::filesystem::path & path, const pkey_c * keys, size_t count) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (count)
      out.write(reinterpret_cast<const char *>(keys), (std::streamsize)(count * sizeof(pkey_c)));
    return (bool)out;
  }

  void wrote(size_t keys) {
    used += keys * sizeof(pkey_c);
    while (used > budget && interval < (1u << 30))
      thin();
  }

  bool hasLevel(char side, unsigned int level) const {
    std::error_code ec;
    return std::filesystem::exists(levelPath(side, level), ec);
  }

  /* The first key of sorted probe that level holds, read as a stream. */
  bool findIn(char side, unsigned int level, const layer_t & probe, pkey_c & match) const {
    std::ifstream in(levelPath(side, level), std::ios::binary);
    if (!in)
      return false;
    layer_t chunk(1 << 20);
    size_t p = 0;
    while (p < probe.size()) {
      in.read(reinterpret_cast<char *>(chunk.data()), (std::streamsize)(chunk.size() * sizeof(pkey_c)));
      const size_t n = (size_t)in.gcount() / sizeof(pkey_c);
      if (n == 0)
        return false;
      for (size_t i = 0; i < n && p < probe.size();) {
        if (chunk[i] < probe[p])
          i++;
        else if (probe[p] < chunk[i])
          p++;
        else {
          match = probe[p];
          return true;
        }
      }
    }
    return false;
  }

  bool saveState(const savedState_c & st) const {
    const std::filesystem::path tmp = dir / "state.tmp";
    {
      std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
      out.write(STATE_MAGIC, sizeof(STATE_MAGIC));
      put(out, st.fingerprint);
      put(out, st.fwdDepth);
      put(out, st.bwdDepth);
      put(out, st.interval);
      put(out, st.found);
      put(out, st.ms);
      put(out, st.met);
      put(out, st.fromStart);
      put(out, st.fromGoal);
      put(out, st.meetAt);
      for (const layer_t * l : {&st.fwdPrev, &st.fwdCur, &st.bwdPrev, &st.bwdCur, &st.mirrorPrev}) {
        put(out, (uint64_t)l->size());
        if (!l->empty())
          out.write(reinterpret_cast<const char *>(l->data()), (std::streamsize)(l->size() * sizeof(pkey_c)));
      }
      if (!out)
        return false;
    }
    /* Replace the old state only once the new one is whole. */
    std::error_code ec;
    std::filesystem::rename(tmp, dir / "state.bin", ec);
    return !ec;
  }

  /* With layers false, only the header: enough to describe the search. */
  bool loadState(savedState_c & st, uint64_t fingerprint, bool layers) const {
    std::ifstream in(dir / "state.bin", std::ios::binary);
    char magic[sizeof(STATE_MAGIC)];
    in.read(magic, sizeof(magic));
    const bool inSeconds = std::equal(magic, magic + sizeof(magic), STATE_MAGIC_SECONDS);
    if (!in || !(inSeconds || std::equal(magic, magic + sizeof(magic), STATE_MAGIC)))
      return false;
    get(in, st.fingerprint);
    get(in, st.fwdDepth);
    get(in, st.bwdDepth);
    get(in, st.interval);
    get(in, st.found);
    get(in, st.ms);
    if (inSeconds)
      st.ms *= 1000;
    get(in, st.met);
    get(in, st.fromStart);
    get(in, st.fromGoal);
    get(in, st.meetAt);
    if (!in || st.fingerprint != fingerprint || st.interval == 0)
      return false;
    if (!layers)
      return true;
    for (layer_t * l : {&st.fwdPrev, &st.fwdCur, &st.bwdPrev, &st.bwdCur, &st.mirrorPrev}) {
      uint64_t n = 0;
      get(in, n);
      if (!in)
        return false;
      l->resize((size_t)n);
      if (n)
        in.read(reinterpret_cast<char *>(l->data()), (std::streamsize)(n * sizeof(pkey_c)));
      if (!in)
        return false;
    }
    return true;
  }

private:
  std::filesystem::path levelPath(char side, unsigned int level) const {
    return dir / (std::string(1, side) + std::to_string(level) + ".lvl");
  }

  /* Keep every other saved level. Level 0 is a multiple of any interval. */
  void thin(void) {
    interval *= 2;
    used = 0;
    std::error_code ec;
    std::vector<std::filesystem::path> drop;
    for (const auto & e : std::filesystem::directory_iterator(dir, ec)) {
      if (e.path().extension() != ".lvl")
        continue;
      const std::string name = e.path().stem().string();
      const unsigned long level = std::strtoul(name.c_str() + 1, nullptr, 10);
      if (level % interval)
        drop.push_back(e.path());
      else
        used += e.file_size(ec);
    }
    for (const auto & d : drop)
      std::filesystem::remove(d, ec);
  }

  template <class T>
  static void put(std::ofstream & out, T v) {
    out.write(reinterpret_cast<const char *>(&v), sizeof(v));
  }
  template <class T>
  static void get(std::ifstream & in, T & v) {
    in.read(reinterpret_cast<char *>(&v), sizeof(v));
  }
};

class searcher_c {
public:
  searcher_c(const rules_c & rules, panexSearch_c & search, unsigned int threads,
             unsigned long long budget)
      : rules(rules), search(search), threads(threads), budget(budget) {}

  /* Save the top search here, and carry on from resumeFrom when it is set. */
  store_c * store = nullptr;
  const savedState_c * resumeFrom = nullptr;
  /* The two ends, for levels the store does not hold. */
  pkey_c startKey;
  pkey_c goalKey;
  uint64_t fingerprint = 0;
  /* Milliseconds the saved search had used, and when this run began. */
  uint64_t priorMs = 0;
  std::chrono::steady_clock::time_point began = std::chrono::steady_clock::now();

  uint64_t msSoFar(void) const {
    return priorMs + (uint64_t)std::chrono::duration_cast<std::chrono::milliseconds>(
                              std::chrono::steady_clock::now() - began).count();
  }

  /* For the classic swap: the board's mirror image with each disc swapped
   * for its partner of the same size maps the start to itself, and the goal
   * too. Then a level holds one stacking of each such pair. */
  void findSigma(pkey_c start, pkey_c goal) {
    state_c s, m;
    rules.decode(start, s);
    rules.decode(rules.mirror(start), m);
    for (unsigned int d = 0; d < rules.discs; d++)
      perm[d] = 0xff;
    for (unsigned int r = 0; r < rules.rods; r++)
      for (unsigned int i = 0; i < s.count[r]; i++) {
        const unsigned char from = m.disc[r][i];
        const unsigned char to = s.disc[r][i];
        if (rules.depth[from] != rules.depth[to] || (perm[from] != 0xff && perm[from] != to))
          return;
        perm[from] = to;
      }
    std::vector<bool> hit(rules.discs, false);
    for (unsigned int d = 0; d < rules.discs; d++) {
      if (perm[d] == 0xff || hit[perm[d]])
        return;
      hit[perm[d]] = true;
    }
    sigma = true;
    if (twin(start) != start || twin(goal) != goal)
      sigma = false;
  }

  /* A shortest path from a to b, both ends included. */
  bool shortestPath(pkey_c a, pkey_c b, bool top, std::vector<pkey_c> & out) {
    if (a == b) {
      out.assign(1, a);
      return true;
    }
    meet_c m;
    if (!meetSearch(a, b, top, m))
      return false;
    if (!m.path.empty()) {
      out.swap(m.path);
      return true;
    }
    /* Too big to keep the levels and nowhere to save them: solve the two
     * halves the same way. */
    std::vector<pkey_c> left, right;
    if (!shortestPath(a, m.at, false, left) || !shortestPath(m.at, b, false, right))
      return false;
    out.swap(left);
    out.insert(out.end(), right.begin() + 1, right.end());
    return true;
  }

private:
  const rules_c & rules;
  panexSearch_c & search;
  const unsigned int threads;
  const unsigned long long budget;
  bool sigma = false;
  unsigned char perm[MAX_DISCS] = {};

  pkey_c twin(pkey_c k) const {
    state_c s;
    rules.decode(k, s);
    return twinOf(s);
  }

  /* The twin's key, written straight from s: the rods end to end, each
   * disc swapped for its partner. */
  pkey_c twinOf(const state_c & s) const {
    pkey_c k;
    auto push = [&k](unsigned int v) {
      k.hi = (k.hi << 5) | (k.lo >> 59);
      k.lo = (k.lo << 5) | v;
    };
    for (unsigned int q = 0; q < rules.rods; q++) {
      const unsigned int r = rules.rods - 1 - q;
      if (q)
        push(SEPARATOR);
      for (unsigned int i = 0; i < s.count[r]; i++)
        push(perm[s.disc[r][i]]);
    }
    return k;
  }

  bool stopped(void) const {
    return search.stop && search.stop->load(std::memory_order_relaxed);
  }

  /* Starting threads costs more than a small level: about one per 20,000 stackings. */
  unsigned int threadsFor(size_t keys) const {
    return (unsigned int)std::min<size_t>(threads, keys / 20000 + 1);
  }

  /* Everything one move from cur that is in neither cur nor prev, through
   * key(state); false when stopped or when room keys would not hold it. */
  template <class Key>
  bool nextLevel(const layer_t & cur, const layer_t & prev, Key key, size_t room, layer_t & next) {
    const unsigned int n = threadsFor(cur.size());
    std::vector<std::vector<layer_t>> runs(n);
    std::atomic<bool> abandon{false};
    std::atomic<size_t> runKeys{0};
    std::atomic<size_t> nextChunk{0};
    /* Several batches a thread, so none waits on another; at most 65,536
     * stackings, so the raw moves stay small. */
    const size_t batch = std::max<size_t>(256, std::min<size_t>(1 << 16, cur.size() / (8 * n) + 1));
    const unsigned int keyBits = 5 * (rules.discs + rules.rods - 1);
    parallel(n, [&](unsigned int t) {
      layer_t out, scratch;
      state_c s;
      for (;;) {
        const size_t lo = nextChunk.fetch_add(batch);
        if (lo >= cur.size() || stopped() || abandon.load(std::memory_order_relaxed))
          break;
        const size_t hi = std::min(cur.size(), lo + batch);
        out.clear();
        for (size_t i = lo; i < hi; i++) {
          rules.decode(cur[i], s);
          rules.moves(s, [&](const state_c & st) { out.push_back(key(st)); });
        }
        radixSort(out, scratch, keyBits);
        out.erase(std::unique(out.begin(), out.end()), out.end());
        subtract(out, cur);
        subtract(out, prev);
        if (runKeys.fetch_add(out.size()) + out.size() > room)
          abandon.store(true, std::memory_order_relaxed);
        runs[t].emplace_back(out.begin(), out.end());
      }
    });
    if (stopped() || abandon.load())
      return false;
    std::vector<layer_t> parts;
    for (auto & r : runs)
      for (auto & p : r)
        parts.emplace_back(std::move(p));
    next = mergeParts(parts, n);
    return true;
  }

  /* A shortest path from the saved end ('f' the start, 'b' the goal) to x,
   * depth moves from it, end first. Going back one saved level at a time:
   * every stacking within reach of the current one, as many moves out as
   * the saved level is nearer the end, until one of them is in that level. */
  template <class Canon>
  bool traceStored(char side, pkey_c x, unsigned int depth, Canon canon, std::vector<pkey_c> & out) {
    std::vector<pkey_c> back{x};
    pkey_c cur = x;
    const size_t room = (size_t)(budget / sizeof(pkey_c) / 2);
    auto identity = [&](const state_c & st) { return rules.encode(st); };
    for (unsigned int d = depth; d > 0;) {
      if (stopped()) {
        search.outcome = PANEX_STOPPED;
        return false;
      }
      const unsigned int c = ((d - 1) / store->interval) * store->interval;
      /* The saved level is missing -- an export left the levels out, say:
       * solve the rest of the way to the end by halves instead. */
      if (!store->hasLevel(side, c)) {
        std::vector<pkey_c> fromEnd;
        if (!shortestPath(side == 'f' ? startKey : goalKey, cur, false, fromEnd))
          return false;
        back.insert(back.end(), fromEnd.rbegin() + 1, fromEnd.rend());
        break;
      }
      const unsigned int r = d - c;
      std::vector<layer_t> ball{layer_t{cur}};
      size_t held = 1;
      for (unsigned int i = 1; i <= r; i++) {
        layer_t next;
        const layer_t empty;
        if (!nextLevel(ball[i - 1], i >= 2 ? ball[i - 2] : empty, identity,
                       room > held ? room - held : 0, next)) {
          if (search.outcome != PANEX_STOPPED)
            search.outcome = stopped() ? PANEX_STOPPED : PANEX_MEMORY;
          return false;
        }
        held += next.size();
        ball.push_back(std::move(next));
      }
      /* The ball's outer level, as the saved level holds it. */
      std::vector<std::pair<pkey_c, pkey_c>> outer;
      outer.reserve(ball[r].size());
      for (const pkey_c & k : ball[r])
        outer.push_back({canon(k), k});
      std::sort(outer.begin(), outer.end(),
                [](const std::pair<pkey_c, pkey_c> & a, const std::pair<pkey_c, pkey_c> & b) {
                  return a.first < b.first;
                });
      layer_t probe;
      probe.reserve(outer.size());
      for (const auto & o : outer)
        probe.push_back(o.first);
      pkey_c hit;
      if (!store->findIn(side, c, probe, hit))
        return traceFailed();
      const auto at = std::lower_bound(outer.begin(), outer.end(), std::make_pair(hit, hit),
                                       [](const std::pair<pkey_c, pkey_c> & a,
                                          const std::pair<pkey_c, pkey_c> & b) {
                                         return a.first < b.first;
                                       });
      /* From that stacking back through the ball to cur. */
      std::vector<pkey_c> chain{at->second};
      for (unsigned int i = r - 1; i >= 1; i--) {
        state_c s;
        rules.decode(chain.back(), s);
        bool ok = false;
        rules.moves(s, [&](const state_c & n) {
          if (ok)
            return;
          const pkey_c k = rules.encode(n);
          if (std::binary_search(ball[i].begin(), ball[i].end(), k)) {
            chain.push_back(k);
            ok = true;
          }
        });
        if (!ok)
          return traceFailed();
      }
      back.insert(back.end(), chain.rbegin(), chain.rend());
      cur = at->second;
      d = c;
      if (search.traced)
        search.traced->fetch_add(r, std::memory_order_relaxed);
    }
    out.assign(back.rbegin(), back.rend());
    return true;
  }

  /* Searches from a and from b until they meet. With top set and b the
   * mirror image of a, only the search from a runs. */
  bool meetSearch(pkey_c a, pkey_c b, bool top, meet_c & m) {
    const bool mirrored = top && rules.mirrorable && rules.mirror(a) == b;
    const bool useTwin = mirrored && sigma;
    auto canon = [&](pkey_c k) {
      if (!useTwin)
        return k;
      pkey_c t = twin(k);
      return t < k ? t : k;
    };
    /* The same, for a stacking at hand: no decoding. */
    auto canonOf = [&](const state_c & st) {
      const pkey_c k = rules.encode(st);
      if (!useTwin)
        return k;
      const pkey_c t = twinOf(st);
      return t < k ? t : k;
    };
    store_c * saveTo = top ? store : nullptr;

    side_c fwd, bwd;
    /* The search from b, when it is the mirror image: its level before the
     * forward search's current one, as canonical keys. */
    layer_t mirrorPrev;
    /* Keep every level for tracing back while they take a quarter of the
     * budget; then the saved levels on disk take over. */
    bool keep = true;
    bool onDisk = false;
    size_t keptKeys = 0;
    const size_t keepCap = search.keepLimit ? (size_t)search.keepLimit
                                            : (size_t)(budget / sizeof(pkey_c) / 4);

    bool found = false;
    if (top && resumeFrom && resumeFrom->met) {
      m.at = resumeFrom->meetAt;
      m.fromStart = resumeFrom->fromStart;
      m.fromGoal = resumeFrom->fromGoal;
      search.found = resumeFrom->found;
      keep = false;
      onDisk = true;
      found = true;
      if (search.depth)
        search.depth->store(m.fromStart + m.fromGoal, std::memory_order_relaxed);
    } else if (top && resumeFrom) {
      fwd.prev = resumeFrom->fwdPrev;
      fwd.cur = resumeFrom->fwdCur;
      fwd.depth = resumeFrom->fwdDepth;
      bwd.prev = resumeFrom->bwdPrev;
      bwd.cur = resumeFrom->bwdCur;
      bwd.depth = resumeFrom->bwdDepth;
      mirrorPrev = resumeFrom->mirrorPrev;
      search.found = resumeFrom->found;
      keep = false;
      onDisk = true;
    } else {
      fwd.cur.push_back(canon(a));
      if (!mirrored)
        bwd.cur.push_back(b);
      fwd.kept.push_back(fwd.cur);
      if (!mirrored)
        bwd.kept.push_back(bwd.cur);
      if (mirrored)
        mirrorPrev.push_back(canon(rules.mirror(a)));
    }

    auto inMemory = [&](size_t extra) {
      size_t keys = fwd.prev.size() + fwd.cur.size() + bwd.prev.size() + bwd.cur.size() +
                    mirrorPrev.size() + keptKeys + extra;
      unsigned long long bytes = (unsigned long long)keys * sizeof(pkey_c);
      search.peakMemory = std::max(search.peakMemory, bytes);
      return bytes;
    };

    /* Write the kept levels to the store and go on from disk. */
    auto toDisk = [&]() {
      if (onDisk)
        return true;
      for (size_t i = 0; i < fwd.kept.size(); i++)
        if (!saveTo->writeLevel('f', (unsigned int)i, fwd.kept[i]))
          return false;
      for (size_t i = 0; i < bwd.kept.size(); i++)
        if (!saveTo->writeLevel('b', (unsigned int)i, bwd.kept[i]))
          return false;
      std::vector<layer_t>().swap(fwd.kept);
      std::vector<layer_t>().swap(bwd.kept);
      keptKeys = 0;
      keep = false;
      onDisk = true;
      return true;
    };
    auto diskFailed = [&]() {
      search.outcome = PANEX_ERROR;
      search.error = "Could not save the search in " + saveTo->dir.string() + ". The disk may be full.";
      return false;
    };
    /* The newest level is written to disk on a thread of its own while the
     * search goes on to the next: the level is only read from then on, and
     * the write is waited for before anything depends on the file. */
    struct levelWriter_c {
      std::thread thread;
      bool ok = true;
      size_t keys = 0;
      ~levelWriter_c() {
        if (thread.joinable())
          thread.join();
      }
    } writer;
    auto finishWrite = [&]() {
      if (!writer.thread.joinable())
        return true;
      writer.thread.join();
      if (!writer.ok)
        return false;
      saveTo->wrote(writer.keys);
      return true;
    };
    /* Save where the search stands, so Continue can carry it on. */
    auto saveState = [&]() {
      if (!saveTo || !finishWrite() || !toDisk())
        return false;
      savedState_c st;
      st.fingerprint = fingerprint;
      st.fwdDepth = fwd.depth;
      st.bwdDepth = bwd.depth;
      st.interval = saveTo->interval;
      st.found = search.found;
      st.ms = msSoFar();
      st.fwdPrev = fwd.prev;
      st.fwdCur = fwd.cur;
      st.bwdPrev = bwd.prev;
      st.bwdCur = bwd.cur;
      st.mirrorPrev = mirrorPrev;
      return saveTo->saveState(st);
    };
    /* How long a path both ends together have ruled out. */
    auto deep = [&]() {
      return mirrored ? 2ul * fwd.depth : (unsigned long)(fwd.depth + bwd.depth);
    };
    /* The search stopped short: save it if it can be carried on. */
    auto haltAndSave = [&](outcome_e why) {
      search.outcome = why;
      search.depthReached = deep();
      if (top && saveTo)
        search.saved = saveState();
      return false;
    };
    auto lastSave = std::chrono::steady_clock::now();

    while (!found) {
      if (stopped() || (top && search.stopAtDepth && deep() >= search.stopAtDepth))
        return haltAndSave(PANEX_STOPPED);
      side_c & side = (mirrored || fwd.cur.size() <= bwd.cur.size()) ? fwd : bwd;
      layer_t next;
      const unsigned long long base = inMemory(0);
      const size_t room = base >= budget ? 0 : (size_t)((budget - base) / sizeof(pkey_c) / 2);
      if (!nextLevel(side.cur, side.prev, canonOf, room, next))
        return haltAndSave(stopped() ? PANEX_STOPPED : PANEX_MEMORY);
      if (!finishWrite())
        return diskFailed();
      if (next.empty()) {
        /* Everything this end can reach is searched without meeting the other. */
        search.outcome = PANEX_NO_PATH;
        return false;
      }
      if (keep) {
        keptKeys += next.size();
        if (keptKeys > keepCap) {
          if (saveTo) {
            if (!toDisk())
              return diskFailed();
          } else {
            keep = false;
            keptKeys = 0;
            std::vector<layer_t>().swap(fwd.kept);
            std::vector<layer_t>().swap(bwd.kept);
          }
        } else {
          side.kept.push_back(next);
        }
      }
      layer_t().swap(side.prev);
      side.prev.swap(side.cur);
      side.cur.swap(next);
      side.depth++;
      if (onDisk && saveTo->keepsLevel(side.depth)) {
        /* The level's memory stays where it is until it is two levels old,
         * well after finishWrite, whichever vector then holds it. */
        const pkey_c * keys = side.cur.data();
        const size_t count = side.cur.size();
        const std::filesystem::path file = saveTo->levelFile(&side == &fwd ? 'f' : 'b', side.depth);
        writer.ok = true;
        writer.keys = count;
        bool * ok = &writer.ok;
        writer.thread = std::thread([ok, keys, count, file]() {
          *ok = store_c::writeKeys(file, keys, count);
        });
      }
      search.found += side.cur.size() * (useTwin ? 2 : 1);
      if (search.progress)
        search.progress->store((unsigned long)search.found, std::memory_order_relaxed);
      if (std::getenv("BURRTOOLS_PANEX_LOG"))
        fprintf(stderr, "panex: %s level %u: %zu stackings, %llu in all\n",
                top ? (&side == &fwd ? "start" : "goal") : (&side == &fwd ? "part start" : "part goal"),
                side.depth, side.cur.size(), search.found);

      if (mirrored) {
        /* The mirrored search stands one level behind, then level with it. */
        if (meet(fwd.cur, mirrorPrev, m.at)) {
          found = true;
          m.fromStart = fwd.depth;
          m.fromGoal = fwd.depth - 1;
          break;
        }
        layer_t mirrorCur(fwd.cur.size());
        const unsigned int mt = threadsFor(fwd.cur.size());
        parallel(mt, [&](unsigned int t) {
          const size_t chunk = (fwd.cur.size() + mt - 1) / mt;
          const size_t lo = std::min(fwd.cur.size(), t * chunk);
          const size_t hi = std::min(fwd.cur.size(), lo + chunk);
          state_c st, mi;
          for (size_t i = lo; i < hi; i++) {
            rules.decode(fwd.cur[i], st);
            rules.mirrorState(st, mi);
            mirrorCur[i] = canonOf(mi);
          }
        });
        parallelSort(mirrorCur, mt, 5 * (rules.discs + rules.rods - 1));
        if (top && search.depth)
          search.depth->store(2ul * fwd.depth, std::memory_order_relaxed);
        if (meet(fwd.cur, mirrorCur, m.at)) {
          found = true;
          m.fromStart = fwd.depth;
          m.fromGoal = fwd.depth;
          break;
        }
        mirrorPrev.swap(mirrorCur);
      } else {
        if (top && search.depth)
          search.depth->store((unsigned long)(fwd.depth + bwd.depth), std::memory_order_relaxed);
        const side_c & other = (&side == &fwd) ? bwd : fwd;
        if (meet(side.cur, other.cur, m.at)) {
          found = true;
          m.fromStart = fwd.depth;
          m.fromGoal = bwd.depth;
        }
      }
      inMemory(0);

      /* An autosave also moves levels still kept in memory to disk. */
      if (saveTo && search.autosaveMinutes &&
          std::chrono::steady_clock::now() - lastSave >= std::chrono::minutes(search.autosaveMinutes)) {
        if (!saveState())
          return diskFailed();
        lastSave = std::chrono::steady_clock::now();
      }
    }
    if (!finishWrite())
      return diskFailed();
    search.depthReached = m.fromStart + m.fromGoal;

    if (onDisk) {
      /* Free the frontier, then trace back between the saved levels. The
       * mirrored half is the forward search's, read in the mirror. */
      layer_t().swap(fwd.prev);
      layer_t().swap(fwd.cur);
      layer_t().swap(bwd.prev);
      layer_t().swap(bwd.cur);
      layer_t().swap(mirrorPrev);
      /* Note where the ends met, so a stop while tracing loses no search. */
      savedState_c st;
      st.fingerprint = fingerprint;
      st.fwdDepth = m.fromStart;
      st.bwdDepth = m.fromGoal;
      st.interval = saveTo->interval;
      st.found = search.found;
      st.ms = msSoFar();
      st.met = 1;
      st.fromStart = m.fromStart;
      st.fromGoal = m.fromGoal;
      st.meetAt = m.at;
      if (!saveTo->saveState(st))
        return diskFailed();
      auto stopTrace = [&]() {
        if (search.outcome == PANEX_STOPPED || search.outcome == PANEX_MEMORY)
          search.saved = true;
        return false;
      };
      auto same = [](pkey_c k) { return k; };
      std::vector<pkey_c> toStart;
      if (!traceStored('f', m.at, m.fromStart, canon, toStart))
        return stopTrace();
      std::vector<pkey_c> toGoal;
      if (mirrored) {
        std::vector<pkey_c> fromMirror;
        if (!traceStored('f', rules.mirror(m.at), m.fromGoal, canon, fromMirror))
          return stopTrace();
        for (const pkey_c & k : fromMirror)
          toGoal.push_back(rules.mirror(k));
      } else if (!traceStored('b', m.at, m.fromGoal, same, toGoal)) {
        return stopTrace();
      }
      /* toStart runs start..meet, toGoal goal..meet. */
      m.path.swap(toStart);
      m.path.insert(m.path.end(), toGoal.rbegin() + 1, toGoal.rend());
      return true;
    }

    if (!keep)
      return true;

    /* Trace back through the kept levels: each step a neighbour one level
     * nearer the end. */
    auto stepBack = [&](const layer_t & level, bool viaMirror, pkey_c from, pkey_c & to) {
      state_c s;
      rules.decode(from, s);
      bool ok = false;
      rules.moves(s, [&](const state_c & n) {
        if (ok)
          return;
        const pkey_c k = rules.encode(n);
        if (std::binary_search(level.begin(), level.end(), canon(viaMirror ? rules.mirror(k) : k))) {
          to = k;
          ok = true;
        }
      });
      return ok;
    };
    std::vector<pkey_c> keys{m.at};
    for (unsigned int level = m.fromStart; level-- > 0;) {
      pkey_c prev;
      if (!stepBack(fwd.kept[level], false, keys.back(), prev))
        return traceFailed();
      keys.push_back(prev);
    }
    std::reverse(keys.begin(), keys.end());
    for (unsigned int level = m.fromGoal; level-- > 0;) {
      pkey_c next;
      if (!stepBack(mirrored ? fwd.kept[level] : bwd.kept[level], mirrored, keys.back(), next))
        return traceFailed();
      keys.push_back(next);
    }
    /* With twins, the ends found may be the twins of a and b; they are
     * a and b themselves because both are their own twins. */
    m.path.swap(keys);
    return true;
  }

  bool traceFailed(void) {
    search.outcome = PANEX_ERROR;
    search.error = "The Panex Solver could not trace its path back.";
    return false;
  }
};

/* The rules, start and goal of a problem the Panex Solver can take. */
bool setup(const problem_c & prob, rules_c & rules, pkey_c & startKey, pkey_c & goalKey) {
  if (!unsupported(prob, true).empty())
    return false;
  const stacking::rodSet_c & board = prob.getPuzzle().getRodSet(prob.getRodSetId());
  stacking::stacking_t startSt, goalSt;
  std::vector<unsigned int> sizes;
  if (!stacking::searchInput(prob, startSt, goalSt, sizes))
    return false;
  rules.rods = stacking::totalRods(board);
  rules.discs = (unsigned int)sizes.size();
  const unsigned int height = stacking::rodHeight(board, rules.discs);
  rules.panex = board.panexColumns;
  rules.sizeRule = !board.panexColumns && board.sizeMatters;
  for (unsigned int d = 0; d < rules.discs; d++) {
    rules.size[d] = (unsigned char)std::min(sizes[d], 255u);
    rules.depth[d] = rules.panex ? (unsigned char)std::min(sizes[d], height) : rules.size[d];
  }
  rules.mirrorable = true;
  for (unsigned int r = 0; r < rules.rods; r++) {
    rules.cap[r] = (unsigned char)std::min(stacking::rodCapacity(board, r, rules.discs), MAX_DISCS);
    rules.pocket[r] = stacking::isPocket(board, r);
    rules.position[r] = stacking::rodPosition(board, r);
    if (rules.pocket[r])
      rules.mirrorable = false;
  }
  rules.distance = board.distanceMatters && !board.canMoveOver;
  startKey = rules.encode(rules.fromStacking(startSt));
  goalKey = rules.encode(rules.fromStacking(goalSt));
  return true;
}

/* The classic tower: three plain rods under the size rule, every disc a
 * different size and room for them all on any rod, the whole tower on one
 * rod at the start and on another at the goal. The well-known recursion --
 * all but the largest disc to the spare rod, the largest across, the rest
 * on top of it -- is then a shortest path, 2^n - 1 transfers, and needs no
 * search. Fills path and returns true when the puzzle is that one and the
 * path is not too long to hold. BURRTOOLS_NO_TOWER_RULE=1 turns it off. */
bool classicTower(const rules_c & rules, pkey_c startKey, pkey_c goalKey,
                  std::vector<stacking::stacking_t> & path) {
  /* 2^22 stackings in the path is about what the path's own memory allows. */
  const unsigned int MOST_DISCS = 22;
  const unsigned int n = rules.discs;
  if (std::getenv("BURRTOOLS_NO_TOWER_RULE") || rules.panex || !rules.sizeRule || rules.distance ||
      rules.rods != 3 || n == 0 || n > MOST_DISCS)
    return false;
  state_c s, g;
  rules.decode(startKey, s);
  rules.decode(goalKey, g);
  int from = -1, to = -1;
  for (unsigned int r = 0; r < 3; r++) {
    if (rules.cap[r] < n)
      return false;
    if (s.count[r] == n)
      from = (int)r;
    if (g.count[r] == n)
      to = (int)r;
  }
  if (from < 0 || to < 0 || from == to)
    return false;
  for (unsigned int i = 0; i + 1 < n; i++)
    if (rules.size[s.disc[from][i]] <= rules.size[s.disc[from][i + 1]])
      return false;
  for (unsigned int i = 0; i < n; i++)
    if (g.disc[to][i] != s.disc[from][i])
      return false;

  path.reserve((size_t)1 << n);
  path.push_back(rules.toStacking(s));
  auto move = [&](unsigned int a, unsigned int b) {
    s.disc[b][s.count[b]++] = s.disc[a][--s.count[a]];
    path.push_back(rules.toStacking(s));
  };
  /* tower(k, a, b): the top k discs of rod a to rod b. Kept as a stack of
   * things still to do, so a tall tower does not nest calls. */
  struct todo_c { unsigned int k, a, b; bool single; };
  std::vector<todo_c> todo{{n, (unsigned int)from, (unsigned int)to, false}};
  while (!todo.empty()) {
    const todo_c t = todo.back();
    todo.pop_back();
    if (t.single || t.k == 1) {
      move(t.a, t.b);
      continue;
    }
    const unsigned int spare = 3 - t.a - t.b;
    todo.push_back({t.k - 1, spare, t.b, false});
    todo.push_back({1, t.a, t.b, true});
    todo.push_back({t.k - 1, t.a, spare, false});
  }
  return true;
}

/* Names a puzzle's saved search: the rules, the start and the goal. */
uint64_t fingerprintOf(const rules_c & rules, pkey_c start, pkey_c goal) {
  uint64_t h = 1469598103934665603ull;
  auto mix = [&h](uint64_t v) {
    for (int i = 0; i < 8; i++) {
      h ^= (v >> (8 * i)) & 0xff;
      h *= 1099511628211ull;
    }
  };
  mix(2);  // the saved format
  mix(rules.panex);
  mix(rules.sizeRule);
  mix(rules.rods);
  mix(rules.discs);
  for (unsigned int d = 0; d < rules.discs; d++)
    mix(rules.depth[d]);
  for (unsigned int r = 0; r < rules.rods; r++) {
    mix(rules.cap[r]);
    mix(rules.pocket[r]);
    mix((uint64_t)(int64_t)rules.position[r]);
  }
  mix(rules.distance);
  mix(start.hi);
  mix(start.lo);
  mix(goal.hi);
  mix(goal.lo);
  return h;
}

std::filesystem::path folderFor(uint64_t fingerprint, const std::string & workDir) {
  std::filesystem::path base;
  if (!workDir.empty())
    base = workDir;
  else if (const char * e = std::getenv("BURRTOOLS_PANEX_DIR"))
    base = e;
  else {
    const std::string cache = userCacheDirectory();
    if (cache.empty())
      return std::filesystem::path();
    base = std::filesystem::path(cache) / "panex";
  }
  char name[17];
  snprintf(name, sizeof(name), "%016llx", (unsigned long long)fingerprint);
  return base / name;
}

std::string shortCount(double n) {
  char buf[32];
  if (n >= 1e9)
    snprintf(buf, sizeof(buf), "%.1fB", n / 1e9);
  else if (n >= 1e6)
    snprintf(buf, sizeof(buf), "%.1fM", n / 1e6);
  else
    snprintf(buf, sizeof(buf), "%.0f", n);
  return buf;
}

} // namespace

std::string unsupported(const problem_c & prob, bool anyRules) {
  if (!stacking::isStacking(prob))
    return "The Panex Solver is for stacking puzzles.";
  std::string err = stacking::setupError(prob);
  if (!err.empty())
    return err;
  const stacking::rodSet_c & board = prob.getPuzzle().getRodSet(prob.getRodSetId());
  if (!anyRules && !board.panexColumns)
    return "The Panex Solver needs Panex Style Columns: turn it on for this rod set in the Entities tab.";
  const unsigned int rods = stacking::totalRods(board);
  if (rods > MAX_RODS)
    return "The Panex Solver handles at most " + std::to_string(MAX_RODS) + " rods.";
  stacking::stacking_t start, goal;
  std::vector<unsigned int> sizes;
  if (!stacking::searchInput(prob, start, goal, sizes))
    return "The puzzle is not ready to solve.";
  if (sizes.size() > MAX_DISCS || sizes.size() + rods - 1 > MAX_SYMBOLS)
    return "The Panex Solver handles at most " + std::to_string(MAX_SYMBOLS + 1 - rods) +
           " discs on " + std::to_string(rods) + " rods.";
  return "";
}

std::string searchFolder(const problem_c & prob, const std::string & workDir) {
  rules_c rules;
  pkey_c start, goal;
  if (!setup(prob, rules, start, goal))
    return "";
  return folderFor(fingerprintOf(rules, start, goal), workDir).string();
}

std::string savedSearch(const problem_c & prob, const std::string & workDir) {
  rules_c rules;
  pkey_c start, goal;
  if (!setup(prob, rules, start, goal))
    return "";
  const uint64_t fp = fingerprintOf(rules, start, goal);
  const std::filesystem::path dir = folderFor(fp, workDir);
  std::error_code ec;
  if (dir.empty() || !std::filesystem::exists(dir / "state.bin", ec))
    return "";
  store_c store;
  store.dir = dir;
  savedState_c st;
  if (!store.loadState(st, fp, false))
    return "";
  const bool mirrored = rules.mirrorable && rules.mirror(start) == goal && !st.met;
  const unsigned long deep = mirrored ? 2ul * st.fwdDepth : (unsigned long)(st.fwdDepth + st.bwdDepth);
  return "a saved search " + std::to_string(deep) + " moves deep, " +
         shortCount((double)st.found) + " stackings";
}

unsigned long long savedMs(const problem_c & prob, const std::string & workDir) {
  rules_c rules;
  pkey_c start, goal;
  if (!setup(prob, rules, start, goal))
    return 0;
  const uint64_t fp = fingerprintOf(rules, start, goal);
  store_c store;
  store.dir = folderFor(fp, workDir);
  savedState_c st;
  if (store.dir.empty() || !store.loadState(st, fp, false))
    return 0;
  return st.ms;
}

void discardSaved(const problem_c & prob, const std::string & workDir) {
  rules_c rules;
  pkey_c start, goal;
  if (!setup(prob, rules, start, goal))
    return;
  const std::filesystem::path dir = folderFor(fingerprintOf(rules, start, goal), workDir);
  std::error_code ec;
  if (!dir.empty())
    std::filesystem::remove_all(dir, ec);
}

std::unique_ptr<separation_c> solve(const problem_c & prob, panexSearch_c & search) {
  search.outcome = PANEX_NO_PATH;
  search.error.clear();
  search.found = 0;
  search.peakMemory = 0;
  search.saved = false;
  search.diskBytes = 0;
  search.depthReached = 0;
  if (search.traced)
    search.traced->store(0);

  search.error = unsupported(prob, search.anyRules);
  rules_c rules;
  pkey_c startKey, goalKey;
  if (!search.error.empty() || !setup(prob, rules, startKey, goalKey)) {
    search.outcome = PANEX_ERROR;
    if (search.error.empty())
      search.error = "The puzzle is not ready to solve.";
    return nullptr;
  }

  {
    std::vector<stacking::stacking_t> tower;
    if (classicTower(rules, startKey, goalKey, tower)) {
      search.outcome = PANEX_FOUND;
      search.found = tower.size();
      search.depthReached = (unsigned long)tower.size() - 1;
      if (search.depth)
        search.depth->store(search.depthReached, std::memory_order_relaxed);
      if (search.progress)
        search.progress->store((unsigned long)search.found, std::memory_order_relaxed);
      return stacking::pathSeparation(prob, tower);
    }
  }

  unsigned long long budget = sliding::SEARCH_MEMORY_BYTES;
  if (search.highMemory)
    budget = std::max(budget, physicalMemoryBytes() / 2);

  searcher_c searcher(rules, search, threadCount(search.threads), budget);
  searcher.startKey = startKey;
  searcher.goalKey = goalKey;
  if (rules.mirrorable && rules.mirror(startKey) == goalKey)
    searcher.findSigma(startKey, goalKey);

  /* The folder for saving the search. Without one, it still solves: by
   * halves once the levels do not fit in memory, but it cannot be resumed. */
  const uint64_t fp = fingerprintOf(rules, startKey, goalKey);
  searcher.fingerprint = fp;
  store_c store;
  savedState_c resumeState;
  const std::filesystem::path dir = folderFor(fp, search.workDir);
  unsigned long long disk = search.diskBudget;
  if (!disk) {
    if (const char * e = std::getenv("BURRTOOLS_PANEX_DISK_GB"))
      disk = (unsigned long long)(std::atof(e) * 1e9);
  }
  if (!dir.empty() && store.open(dir, 1)) {
    if (!disk) {
      std::error_code ec;
      const std::filesystem::space_info space = std::filesystem::space(dir, ec);
      disk = ec ? 0 : std::min<unsigned long long>(space.available / 2, 200000000000ull);
    }
    store.budget = std::max<unsigned long long>(disk, 1);
    if (search.resume && store.loadState(resumeState, fp, true)) {
      store.interval = resumeState.interval;
      searcher.resumeFrom = &resumeState;
      searcher.priorMs = resumeState.ms;
    } else {
      store.wipe();
    }
    searcher.store = &store;
  }

  std::vector<pkey_c> keys;
  const bool ok = searcher.shortestPath(startKey, goalKey, true, keys);
  search.diskBytes = store.used;
  search.levelInterval = store.interval;
  if (!ok) {
    /* A finished search, found or not, needs nothing saved. */
    if (searcher.store && !search.saved && search.outcome != PANEX_ERROR)
      store.remove();
    return nullptr;
  }
  if (searcher.store)
    store.remove();

  std::vector<stacking::stacking_t> path;
  path.reserve(keys.size());
  for (const pkey_c & k : keys) {
    state_c s;
    rules.decode(k, s);
    path.push_back(rules.toStacking(s));
  }
  search.outcome = PANEX_FOUND;
  return stacking::pathSeparation(prob, path);
}

} // namespace panex

