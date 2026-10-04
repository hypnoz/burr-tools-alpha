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
#include "rotationmoves_0.h"

#include "bt_assert.h"
#include "disassemblernode.h"
#include "movementcache.h"
#include "problem.h"
#include "puzzle.h"
#include "gridtype.h"
#include "symmetries.h"
#include "voxel.h"

#include <cstdlib>
#include <vector>

/* Cube orientation indices for ±90° about X/Y/Z (see tabs_0/rotmatrix.inc) */
static const unsigned char ROT_X_P90 = 1;
static const unsigned char ROT_X_M90 = 3;
static const unsigned char ROT_Y_P90 = 12;
static const unsigned char ROT_Y_M90 = 4;
static const unsigned char ROT_Z_P90 = 16;
static const unsigned char ROT_Z_M90 = 20;

namespace {

static void rotateVectorLocal(int * x, int * y, int * z, unsigned int axis, unsigned int sense) {

  int ox = *x, oy = *y, oz = *z;

  if (axis == 0) {
    if (sense == 0) { *x = ox; *y = -oz; *z = oy; }
    else            { *x = ox; *y = oz;  *z = -oy; }
  } else if (axis == 1) {
    if (sense == 0) { *x = oz;  *y = oy; *z = -ox; }
    else            { *x = -oz; *y = oy; *z = ox; }
  } else {
    if (sense == 0) { *x = -oy; *y = ox; *z = oz; }
    else            { *x = oy;  *y = -ox; *z = oz; }
  }
}

static bool rotateDoubledPoint(int * x, int * y, int * z,
                               const rotationRules_c::pivot_t & pivot,
                               unsigned int axis, unsigned int sense) {

  int dx = (*x) * 2 - pivot.hx;
  int dy = (*y) * 2 - pivot.hy;
  int dz = (*z) * 2 - pivot.hz;

  rotateVectorLocal(&dx, &dy, &dz, axis, sense);

  int nx = pivot.hx + dx;
  int ny = pivot.hy + dy;
  int nz = pivot.hz + dz;
  if ((nx | ny | nz) & 1)
    return false;

  *x = nx / 2;
  *y = ny / 2;
  *z = nz / 2;
  return true;
}

/* BURRTOOLS_NO_ROT_FAST=1: check every candidate from scratch with
 * rotationRules_c::allowRotation, as before, for A/B runs. The moves found
 * and their order are the same either way. */
static bool rotFast(void) {
  static const bool on = getenv("BURRTOOLS_NO_ROT_FAST") == nullptr;
  return on;
}

} // namespace

rotationMoves_0_c::rotationMoves_0_c(const problem_c & puz, movementCache_c * cache_) :
  cache(cache_),
  sym(puz.getPuzzle().getGridType()->getSymmetries()),
  searchnode(0),
  pieces(0),
  nextsubset(1),
  nextpivot(0),
  nextaxis(0),
  nextsense(0),
  active(false),
  anchor(getenv("BURRTOOLS_NO_ROT_ANCHOR") == nullptr)
{
}

void rotationMoves_0_c::rotateVector(int * x, int * y, int * z, unsigned int axis, unsigned int sense) {

  int ox = *x, oy = *y, oz = *z;

  if (axis == 0) {
    if (sense == 0) { *x = ox; *y = -oz; *z = oy; }   /* +90 X */
    else            { *x = ox; *y = oz;  *z = -oy; }  /* -90 X */
  } else if (axis == 1) {
    if (sense == 0) { *x = oz;  *y = oy; *z = -ox; }  /* +90 Y */
    else            { *x = -oz; *y = oy; *z = ox; }   /* -90 Y */
  } else {
    if (sense == 0) { *x = -oy; *y = ox; *z = oz; }   /* +90 Z */
    else            { *x = oy;  *y = -ox; *z = oz; }  /* -90 Z */
  }
}

bool rotationMoves_0_c::rotateDoubled(int * x, int * y, int * z,
                                      const rotationRules_c::pivot_t & pivot,
                                      unsigned int axis, unsigned int sense) {
  return rotateDoubledPoint(x, y, z, pivot, axis, sense);
}

unsigned char rotationMoves_0_c::rotationTransformId(unsigned int axis, unsigned int sense) {

  static const unsigned char ids[3][2] = {
    { ROT_X_P90, ROT_X_M90 },
    { ROT_Y_P90, ROT_Y_M90 },
    { ROT_Z_P90, ROT_Z_M90 }
  };
  bt_assert(axis < 3 && sense < 2);
  return ids[axis][sense];
}

unsigned int rotationMoves_0_c::nextSubsetMask(unsigned int mask, unsigned int n) {

  const unsigned int allMask = (n >= 32) ? 0xFFFFFFFFu : ((1u << n) - 1u);

  do {
    mask++;
    if (mask > allMask)
      return 0;
  } while (mask == allMask); /* rotating every piece preserves the assembly */

  return mask;
}

void rotationMoves_0_c::collectWorldCells(unsigned int pieceIdx, std::vector<rotationRules_c::cell_t> & out) const {

  out.clear();

  unsigned int pieceId = (*pieces)[pieceIdx];
  unsigned int shapeId = cache->getShapeOfPiece(pieceId);
  unsigned char trans = (unsigned char)searchnode->getTrans(pieceIdx);
  const voxel_c * sh = cache->getTransformedShape(shapeId, trans);

  int px = searchnode->getX(pieceIdx);
  int py = searchnode->getY(pieceIdx);
  int pz = searchnode->getZ(pieceIdx);
  int hx = (int)sh->getHx();
  int hy = (int)sh->getHy();
  int hz = (int)sh->getHz();

  for (unsigned int z = 0; z < sh->getZ(); z++)
    for (unsigned int y = 0; y < sh->getY(); y++)
      for (unsigned int x = 0; x < sh->getX(); x++)
        if (sh->isFilled(x, y, z))
          out.push_back(rotationRules_c::cell_t(px - hx + (int)x, py - hy + (int)y, pz - hz + (int)z));
}

/* The moving subset's cells and everyone else's, from the node's piece
 * cells. Once per subset: they are the same for every axis, pivot and sense. */
void rotationMoves_0_c::loadSubset(unsigned int subsetMask) {

  subsetStart.clear();
  subsetOccupied.clear();
  for (unsigned int i = 0; i < pieceCells.size(); i++) {
    std::vector<rotationRules_c::cell_t> & to = (subsetMask & (1u << i)) ? subsetStart : subsetOccupied;
    to.insert(to.end(), pieceCells[i].begin(), pieceCells[i].end());
  }

  if (rotFast() && subsetStart.size() > 1)
    rules.setBodies(subsetOccupied, subsetStart);
}

void rotationMoves_0_c::rebuildPivotCells(unsigned int axis) {

  pivotCells.clear();

  const std::vector<rotationRules_c::cell_t> & moving = subsetStart;

  /* Nothing to turn, or a lone unit cube, which cannot free itself by
   * spinning in place. */
  if (moving.size() <= 1)
    return;

  /* Pinched or walled in on this axis whatever the pivot: no candidates. */
  if (rotFast() && rules.preparedAxisBlocked(axis))
    return;

  int umin, umax, vmin, vmax, amin;
  if (axis == 0) {
    umin = umax = moving[0].y;
    vmin = vmax = moving[0].z;
    amin = moving[0].x;
  } else if (axis == 1) {
    umin = umax = moving[0].x;
    vmin = vmax = moving[0].z;
    amin = moving[0].y;
  } else {
    umin = umax = moving[0].x;
    vmin = vmax = moving[0].y;
    amin = moving[0].z;
  }

  for (unsigned int c = 1; c < moving.size(); c++) {
    const rotationRules_c::cell_t & p = moving[c];
    int u = (axis == 0) ? p.y : p.x;
    int v = (axis == 2) ? p.y : p.z;
    int a = (axis == 0) ? p.x : ((axis == 1) ? p.y : p.z);
    if (u < umin) umin = u;
    if (u > umax) umax = u;
    if (v < vmin) vmin = v;
    if (v > vmax) vmax = v;
    if (a < amin) amin = a;
  }

  /* In-plane half-grid covering voxel centres, empty bbox cells, faces, edges,
   * and corners. Axis coordinate is the mid-layer of the moving piece. */
  const int du0 = 2 * umin - 1;
  const int du1 = 2 * umax + 1;
  const int dv0 = 2 * vmin - 1;
  const int dv1 = 2 * vmax + 1;
  const int da = 2 * amin;

  for (int du = du0; du <= du1; du++) {
    for (int dv = dv0; dv <= dv1; dv++) {
      /* A quarter turn lands on the grid only about a cell centre or a
       * corner: in-plane coordinates both even or both odd. */
      if (rotFast() && ((du ^ dv) & 1))
        continue;
      rotationRules_c::pivot_t p;
      if (axis == 0) { p.hx = da; p.hy = du; p.hz = dv; }
      else if (axis == 1) { p.hx = du; p.hy = da; p.hz = dv; }
      else { p.hx = du; p.hy = dv; p.hz = da; }
      pivotCells.push_back(p);
    }
  }
}

disassemblerNode_c * rotationMoves_0_c::tryCurrentCandidate(void) {

  if ((unsigned int)nextpivot >= pivotCells.size())
    return 0;

  const unsigned int subsetMask = nextsubset;
  rotationRules_c::pivot_t pivot = pivotCells[nextpivot];

  if (rotFast()) {

    if (!rules.allowPrepared(pivot, nextaxis, nextsense, true))
      return 0;

  } else {

    const std::vector<rotationRules_c::cell_t> & combinedStart = subsetStart;

    std::vector<rotationRules_c::cell_t> combinedEnd;
    combinedEnd.reserve(combinedStart.size());

    for (unsigned int i = 0; i < combinedStart.size(); i++) {
      rotationRules_c::cell_t endCell;
      if (!rotationRules_c::rotateCell(combinedStart[i], pivot, nextaxis, nextsense, endCell))
        return 0;
      combinedEnd.push_back(endCell);
    }

    if (!rules.allowRotation(subsetOccupied, combinedStart, combinedEnd, pivot, nextaxis, nextsense))
      return 0;
  }

  /* Turning a subset one way about a pivot and turning everything else the
   * other way about it leave the pieces in the same places relative to each
   * other. With anchor set the node keeps the first piece's orientation
   * fixed and turns whichever side does not hold it, so an arrangement is
   * one node however the whole puzzle is turned in space, not up to 24. The
   * rules above still judged the subset as it was asked. */
  unsigned int moveMask = subsetMask;
  unsigned int sense = nextsense;
  if (anchor && (subsetMask & 1u)) {
    moveMask = presentMask & ~subsetMask;
    sense = 1 - nextsense;
  }

  unsigned int primaryPiece = 0;
  while (primaryPiece < pieces->size() && !(moveMask & (1u << primaryPiece)))
    primaryPiece++;
  bt_assert(primaryPiece < pieces->size());

  unsigned char rotId = rotationTransformId(nextaxis, sense);
  bool changed = false;

  unsigned int dir = ROTATION_DIR_BASE + nextaxis * 2 + sense;
  disassemblerNode_c * n = new disassemblerNode_c(pieces->size(), searchnode, (int)dir, 1);
  n->setRotationInfo(primaryPiece, pivot.hx, pivot.hy, pivot.hz);

  for (unsigned int i = 0; i < pieces->size(); i++) {
    if (searchnode->is_piece_removed(i)) {
      n->set(i,
             searchnode->getX(i),
             searchnode->getY(i),
             searchnode->getZ(i),
             searchnode->getTrans(i));
      continue;
    }

    if (moveMask & (1u << i)) {
      unsigned char oldTrans = (unsigned char)searchnode->getTrans(i);
      unsigned char newTrans = sym->transAdd(oldTrans, rotId);

      int px = searchnode->getX(i);
      int py = searchnode->getY(i);
      int pz = searchnode->getZ(i);
      if (!rotateDoubled(&px, &py, &pz, pivot, nextaxis, sense)) {
        if (n->decRefCount())
          delete n;
        return 0;
      }

      if (px != searchnode->getX(i) || py != searchnode->getY(i) ||
          pz != searchnode->getZ(i) || newTrans != oldTrans)
        changed = true;

      n->set(i, px, py, pz, newTrans);
    } else {
      n->set(i,
             searchnode->getX(i),
             searchnode->getY(i),
             searchnode->getZ(i),
             searchnode->getTrans(i));
    }
  }

  if (!changed) {
    if (n->decRefCount())
      delete n;
    return 0;
  }

  return n;
}

void rotationMoves_0_c::init_find(disassemblerNode_c * nd, const std::vector<unsigned int> & pcs) {

  searchnode = nd;
  pieces = &pcs;
  nextsubset = (pcs.size() > 0) ? 1u : 0u;
  nextpivot = 0;
  nextaxis = 0;
  nextsense = 0;
  active = pcs.size() > 0;

  /* Each piece's cells at this node, once; a removed piece has none. */
  pieceCells.resize(pcs.size());
  presentMask = 0;
  for (unsigned int i = 0; i < pcs.size(); i++) {
    if (nd->is_piece_removed(i)) {
      pieceCells[i].clear();
    } else {
      collectWorldCells(i, pieceCells[i]);
      presentMask |= 1u << i;
    }
  }

  if (active) {
    loadSubset(nextsubset);
    rebuildPivotCells(nextaxis);
  }
}

disassemblerNode_c * rotationMoves_0_c::find(void) {

  if (!active)
    return 0;

  const unsigned int n = (unsigned int)pieces->size();

  while (true) {

    disassemblerNode_c * node = tryCurrentCandidate();

    nextsense++;
    if (nextsense >= 2 || pivotCells.empty()) {
      nextsense = 0;
      nextpivot++;
      if ((unsigned int)nextpivot >= pivotCells.size()) {
        nextpivot = 0;
        nextaxis++;
        if (nextaxis >= 3) {
          nextaxis = 0;
          nextsubset = nextSubsetMask(nextsubset, n);
          if (nextsubset == 0) {
            active = false;
            if (node) return node;
            return 0;
          }
          loadSubset(nextsubset);
        }
        rebuildPivotCells(nextaxis);
      }
    }

    if (node)
      return node;
  }
}
