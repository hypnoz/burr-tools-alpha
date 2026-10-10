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
#include "symmetries_2.h"

#include "voxel_2.h"

#include "bt_assert.h"
#include "bitfield.h"

#include "tabs_2/tablesizes.inc"

#include <array>
#include <atomic>
#include <cstring>
#include <mutex>

static_assert(symmetries_2_c::TRANSFORMATIONS == NUM_TRANSFORMATIONS_MIRROR,
              "the sphere grid's transformation count is given twice");

namespace {

typedef bitfield_c<NUM_TRANSFORMATIONS_MIRROR> symlist_t;

/* this matrix contains the concatenation of 2 transformations
 * if you first transform the piece around t1 and then around t2
 * you can as well transform around transMult[t1][t2]
 */
const unsigned char transMult[NUM_TRANSFORMATIONS_MIRROR][NUM_TRANSFORMATIONS_MIRROR] = {
#include "tabs_2/transmult.inc"
};

/* this array contains the generated symmetry groups, meaning bitmasks with exactly the bits set
 * that correspond to transformations that reorient the piece so that it looks identical
 */
const symlist_t symmetries[NUM_SYMMETRY_GROUPS] = {
#include "tabs_2/symmetries.inc"
};

const symlist_t unifiedSymmetries[NUM_SYMMETRY_GROUPS] = {
#include "tabs_2/unifiedsym.inc"
};

/* this matrix lets you calculate the orientation with the smallest number that results in an identical looking
 * shape. This requires us to know the symmetry group
 */
const unsigned char transformationMinimizer[NUM_SYMMETRY_GROUPS][NUM_TRANSFORMATIONS_MIRROR] = {
#include "tabs_2/transformmini.inc"
};

const symlist_t uniqueSymmetries[NUM_SYMMETRY_GROUPS] = {
#include "tabs_2/uniquesym.inc"
};

/** one symmetry list with everything the queries need */
struct group_c {
  symlist_t sym;        ///< the transformations that map the shape onto itself
  symlist_t unified;    ///< the same over every orientation of the shape
  symlist_t unique;     ///< one transformation for each distinct orientation
  unsigned char minimizer[NUM_TRANSFORMATIONS_MIRROR] = {}; ///< lowest transformation with the same orientation
};

/**
 * The symmetry lists known so far. The generated ones come first and keep
 * their numbers, lists met at run time are appended.
 *
 * Lookups by number are lock free: a list is stored in a block that is
 * never moved, and the count is published with a release store after the
 * entry is complete. Only appending takes the mutex.
 */
class registry_c {

  public:

    registry_c(void) {
      static_assert(NUM_SYMMETRY_GROUPS <= BLOCK, "the generated lists must fit the first block");
      group_c * first = new group_c[BLOCK];
      for (unsigned int i = 0; i < NUM_SYMMETRY_GROUPS; i++) {
        first[i].sym = symmetries[i];
        first[i].unified = unifiedSymmetries[i];
        first[i].unique = uniqueSymmetries[i];
        memcpy(first[i].minimizer, transformationMinimizer[i], NUM_TRANSFORMATIONS_MIRROR);
      }
      blocks[0].store(first, std::memory_order_release);
      count.store(NUM_SYMMETRY_GROUPS, std::memory_order_release);
    }

    ~registry_c(void) {
      for (std::atomic<group_c *> & b : blocks)
        delete [] b.load(std::memory_order_relaxed);
    }

    registry_c(const registry_c &) = delete;
    registry_c & operator=(const registry_c &) = delete;

    unsigned int size(void) const { return count.load(std::memory_order_acquire); }

    const group_c & get(symmetries_t s) const {
      bt_assert(s < size());
      return entry(s);
    }

    /** the number of the given list, appended when it is new */
    symmetries_t find(const symlist_t & s) {
      unsigned int n = size();
      unsigned int i = scan(s, 0, n);
      if (i < n)
        return (symmetries_t)i;

      std::lock_guard<std::mutex> lock(grow);

      /* another thread may have appended it in the meantime */
      const unsigned int m = count.load(std::memory_order_relaxed);
      i = scan(s, n, m);
      if (i < m)
        return (symmetries_t)i;

      bt_assert(m < symmetryInvalid());
      group_c * block = blocks[m / BLOCK].load(std::memory_order_relaxed);
      if (!block) {
        block = new group_c[BLOCK];
        blocks[m / BLOCK].store(block, std::memory_order_release);
      }
      group_c & g = block[m % BLOCK];
      g.sym = s;
      symmetries_2_c::deriveTables(s, g.unified, g.unique, g.minimizer);
      count.store(m + 1, std::memory_order_release);
      return (symmetries_t)m;
    }

  private:

    static constexpr unsigned int BLOCK = 256;
    /* enough blocks for every value of symmetries_t */
    static constexpr unsigned int BLOCKS = (symmetryInvalid() + BLOCK - 1) / BLOCK;

    const group_c & entry(unsigned int s) const {
      return blocks[s / BLOCK].load(std::memory_order_relaxed)[s % BLOCK];
    }

    /** the first entry in [from, to) equal to s, or to when there is none */
    unsigned int scan(const symlist_t & s, unsigned int from, unsigned int to) const {
      for (unsigned int i = from; i < to; i++)
        if (entry(i).sym == s)
          return i;
      return to;
    }

    std::array<std::atomic<group_c *>, BLOCKS> blocks{};
    std::atomic<unsigned int> count{0};
    std::mutex grow;
};

registry_c & registry(void) {
  static registry_c r;
  return r;
}

} // namespace

void symmetries_2_c::deriveTables(const bitfield_c<TRANSFORMATIONS> & sym,
                                  bitfield_c<TRANSFORMATIONS> & unified,
                                  bitfield_c<TRANSFORMATIONS> & unique,
                                  unsigned char * minimizer) {

  /* the lowest transformation t with sym-transformation followed by t
   * equal to trans gives the same orientation as trans */
  for (unsigned int trans = 0; trans < NUM_TRANSFORMATIONS_MIRROR; trans++) {
    unsigned char m = (unsigned char)trans;
    for (unsigned int t = 0; t < trans && m == trans; t++)
      for (unsigned int t2 = 0; t2 < NUM_TRANSFORMATIONS_MIRROR; t2++)
        if (sym.get(t2) && transMult[t2][t] == trans) {
          m = (unsigned char)t;
          break;
        }
    minimizer[trans] = m;
  }

  /* if r, x, inverse of r is a symmetry of the shape, then x is a
   * symmetry of the shape rotated by r */
  unified = sym;
  for (unsigned int r = 1; r < NUM_TRANSFORMATIONS_MIRROR; r++) {
    unsigned int rinv = 0;
    while (rinv < NUM_TRANSFORMATIONS_MIRROR && transMult[r][rinv] != 0)
      rinv++;
    if (rinv == NUM_TRANSFORMATIONS_MIRROR)
      continue;

    for (unsigned int x = 0; x < NUM_TRANSFORMATIONS_MIRROR; x++) {
      unsigned char res = transMult[r][x];
      if (res == TND) continue;
      res = transMult[res][rinv];
      if (res == TND) continue;
      if (sym.get(res))
        unified.set(x);
    }
  }

  /* one transformation for each orientation: once r is taken, every
   * symmetry followed by r gives the same orientation and is skipped */
  unique.clear();
  symlist_t seen;
  for (unsigned int r = 0; r < NUM_TRANSFORMATIONS_MIRROR; r++) {
    if (seen.get(r))
      continue;
    unique.set(r);
    for (unsigned int r2 = 0; r2 < NUM_TRANSFORMATIONS_MIRROR; r2++)
      if (sym.get(r2) && transMult[r2][r] != TND)
        seen.set(transMult[r2][r]);
  }
}

bitfield_c<symmetries_2_c::TRANSFORMATIONS> symmetries_2_c::symmetryList(const voxel_c * pp) {

  bt_assert(pp);

  symlist_t s;
  s.set(0);

  for (unsigned int j = 1; j < NUM_TRANSFORMATIONS_MIRROR; j++) {
    voxel_2_c v(pp);
    if (v.transform(j) && pp->identicalInBB(&v))
      s.set(j);
  }

  return s;
}

unsigned int symmetries_2_c::numSymmetryLists(void) {
  return registry().size();
}

symmetries_2_c::symmetries_2_c(void) {
}

unsigned int symmetries_2_c::getNumTransformations(void) const { return NUM_TRANSFORMATIONS; }
unsigned int symmetries_2_c::getNumTransformationsMirror(void) const { return NUM_TRANSFORMATIONS_MIRROR; }

bool symmetries_2_c::symmetryContainsMirror(symmetries_t sym) const {

  symlist_t s = registry().get(sym).sym;

  for (int i = 0; i < NUM_TRANSFORMATIONS; i++)
    s.reset(i);

  return s.notNull();
}

unsigned char symmetries_2_c::transAdd(unsigned char t1, unsigned char t2) const {
  bt_assert(t1 < NUM_TRANSFORMATIONS_MIRROR);
  bt_assert(t2 < NUM_TRANSFORMATIONS_MIRROR);
  return transMult[t1][t2];
}

bool symmetries_2_c::symmetrieContainsTransformation(symmetries_t s, unsigned int t) const {

  bt_assert(t < NUM_TRANSFORMATIONS_MIRROR);

  return registry().get(s).sym.get(t);
}

unsigned char symmetries_2_c::minimizeTransformation(symmetries_t s, unsigned char trans) const {

  bt_assert(trans < NUM_TRANSFORMATIONS_MIRROR);

  return registry().get(s).minimizer[trans];
}

bool symmetries_2_c::isTransformationUnique(symmetries_t s, unsigned int t) const {

  bt_assert(t < NUM_TRANSFORMATIONS_MIRROR);

  return registry().get(s).unique.get(t);
}

unsigned int symmetries_2_c::countSymmetryIntersection(symmetries_t res, symmetries_t s2) const {

  const registry_c & reg = registry();
  symlist_t s = reg.get(res).unified & reg.get(s2).sym;

  return s.countbits();
}

bool symmetries_2_c::symmetriesLeft(symmetries_t resultSym, symmetries_t s2) const {

  const registry_c & reg = registry();
  symlist_t s = reg.get(resultSym).sym & reg.get(s2).unified;

  s.reset(0);

  return s.notNull();
}

/* Every list is known: one not in the registry is added by calculateSymmetry(). */
bool symmetries_2_c::symmetryKnown(const voxel_c * /*pp*/) const {
  return true;
}

symmetries_t symmetries_2_c::calculateSymmetry(const voxel_c *pp) const {

  bt_assert(pp);

  /* The generated decision tree the other grids use would answer wrongly
   * for a list outside the generated tables, so the full list is computed
   * and looked up. voxel_c caches the result, so this runs once per shape. */
  return registry().find(symmetryList(pp));
}
