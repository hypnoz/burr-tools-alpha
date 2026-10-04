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
 * Sliding-tray helpers: start/goal colour maps and one-cell slide search.
 */
#include "sliding.h"
#include "disasmtomoves.h"

#include "assembly.h"
#include "bt_assert.h"
#include "disassembly.h"
#include "gridtype.h"
#include "problem.h"
#include "puzzle.h"
#include "sysmemory.h"
#include "voxel.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <map>
#include <queue>
#include <set>
#include <sstream>
#include <string>
#include <tuple>
#include <utility>

namespace sliding {

bool isStartGoalShape(const voxel_c * v);

namespace {

static const char * GOAL_SHAPE_NAME = "__sliding_goal__";

static const int DX[4] = { 0, 0, -1, 1 };
static const int DY[4] = { -1, 1, 0, 0 };

bool isFloor(const voxel_c & tray, int x, int y) {
  if (x < 0 || y < 0 || (unsigned)x >= tray.getX() || (unsigned)y >= tray.getY())
    return false;
  return tray.getState(x, y, 0) != voxel_c::VX_EMPTY;
}

bool isFloor(const voxel_c & tray, int x, int y, int z) {
  if (x < 0 || y < 0 || z < 0 || (unsigned)x >= tray.getX() || (unsigned)y >= tray.getY() ||
      (unsigned)z >= tray.getZ())
    return false;
  return tray.getState(x, y, z) != voxel_c::VX_EMPTY;
}

unsigned int findOrAddColor(puzzle_c & puz, unsigned int preferredIndex) {
  /* Prefer 1-based colour indices that match the piece's part order. */
  while (puz.colorNumber() < preferredIndex)
    puz.addColor(40 + 30 * (puz.colorNumber() % 3),
                 40 + 30 * ((puz.colorNumber() + 1) % 3),
                 40 + 30 * ((puz.colorNumber() + 2) % 3));
  return preferredIndex;
}

unsigned int piecePartIndex(const problem_c & prob, unsigned int shapeId) {
  for (unsigned int i = 0; i < prob.getNumberOfParts(); i++)
    if (prob.getShapeIdOfPart(i) == shapeId)
      return i + 1;
  return 1;
}

void fillTray(voxel_c * v) {
  for (unsigned int y = 0; y < v->getY(); y++)
    for (unsigned int x = 0; x < v->getX(); x++)
      v->setState(x, y, 0, voxel_c::VX_FILLED);
}

void paintPieceNeutral(voxel_c * piece) {
  for (unsigned int i = 0; i < piece->getXYZ(); i++)
    if (piece->getState(i) != voxel_c::VX_EMPTY)
      piece->setColor(i, 0);
}

void paintPieceColor(voxel_c * piece, unsigned int color) {
  for (unsigned int i = 0; i < piece->getXYZ(); i++)
    if (piece->getState(i) != voxel_c::VX_EMPTY)
      piece->setColor(i, color);
}

bool footprintFits(const voxel_c & tray, const voxel_c & piece,
                   int ax, int ay, unsigned int ignoreColor = 0) {
  int hx = piece.getHx();
  int hy = piece.getHy();

  for (unsigned int z = 0; z < piece.getZ(); z++)
    for (unsigned int y = 0; y < piece.getY(); y++)
      for (unsigned int x = 0; x < piece.getX(); x++) {
        if (piece.getState(x, y, z) == voxel_c::VX_EMPTY)
          continue;
        int tx = ax + (int)x - hx;
        int ty = ay + (int)y - hy;
        if (!isFloor(tray, tx, ty))
          return false;
        unsigned int c = tray.getColor(tx, ty, 0);
        if (c != 0 && c != ignoreColor)
          return false;
      }
  return true;
}

void paintFootprint(voxel_c * tray, const voxel_c & piece,
                    int ax, int ay, unsigned int color) {
  int hx = piece.getHx();
  int hy = piece.getHy();

  for (unsigned int z = 0; z < piece.getZ(); z++)
    for (unsigned int y = 0; y < piece.getY(); y++)
      for (unsigned int x = 0; x < piece.getX(); x++) {
        if (piece.getState(x, y, z) == voxel_c::VX_EMPTY)
          continue;
        int tx = ax + (int)x - hx;
        int ty = ay + (int)y - hy;
        if (isFloor(*tray, tx, ty))
          tray->setColor(tx, ty, 0, color);
      }
}

void eraseColor(voxel_c * tray, unsigned int color) {
  for (unsigned int i = 0; i < tray->getXYZ(); i++)
    if (tray->getColor(i) == color)
      tray->setColor(i, 0);
}

bool findColorAnchor(const voxel_c & tray, const voxel_c & piece,
                     unsigned int color, int * ax, int * ay) {
  /* Find any coloured cell and back-compute the hotspot position from the
   * first filled relative cell of the piece. */
  int firstPx = -1, firstPy = -1;
  for (unsigned int y = 0; y < piece.getY() && firstPx < 0; y++)
    for (unsigned int x = 0; x < piece.getX() && firstPx < 0; x++)
      if (piece.getState(x, y, 0) != voxel_c::VX_EMPTY) {
        firstPx = (int)x;
        firstPy = (int)y;
      }
  if (firstPx < 0)
    return false;

  for (unsigned int y = 0; y < tray.getY(); y++)
    for (unsigned int x = 0; x < tray.getX(); x++)
      if (tray.getState(x, y, 0) != voxel_c::VX_EMPTY && tray.getColor(x, y, 0) == color) {
        int hx = (int)piece.getHx();
        int hy = (int)piece.getHy();
        *ax = (int)x - firstPx + hx;
        *ay = (int)y - firstPy + hy;
        /* Verify the whole footprint matches. */
        if (footprintFits(tray, piece, *ax, *ay, color)) {
          bool ok = true;
          for (unsigned int py = 0; py < piece.getY() && ok; py++)
            for (unsigned int px = 0; px < piece.getX() && ok; px++) {
              if (piece.getState(px, py, 0) == voxel_c::VX_EMPTY)
                continue;
              int tx = *ax + (int)px - hx;
              int ty = *ay + (int)py - hy;
              if (!isFloor(tray, tx, ty) || tray.getColor(tx, ty, 0) != color)
                ok = false;
            }
          if (ok)
            return true;
        }
      }
  return false;
}

struct Place {
  int x = 0, y = 0;
  unsigned char trans = 0;
  /* Pieces slide in x and y only. z is the layer their hotspot sits on,
   * fixed from the start, so a tray more than one layer deep works too. */
  int z = 0;
};

struct SlideState {
  std::vector<Place> places;
};

std::string stateKey(const SlideState & s) {
  std::ostringstream os;
  for (size_t i = 0; i < s.places.size(); i++) {
    if (i) os << '|';
    os << (int)s.places[i].trans << ':' << s.places[i].x << ',' << s.places[i].y;
  }
  return os.str();
}

/* Whether each piece sits where the goal wants it. Every piece is judged on
 * its own, so a position search can work the answer out once per position. */
class goalCheck_c {
public:
  goalCheck_c(const problem_c & prob, const std::vector<const voxel_c *> & pieces)
      : pieces(pieces) {
    tray = prob.resultValid() ? getResultShape(prob) : nullptr;
    startGoal = tray && isStartGoalShape(tray);
    goal = (!startGoal && prob.goalValid()) ? prob.getGoalShape() : nullptr;

    /* Map each placement index back to its part so we can look up colours. */
    std::vector<unsigned int> partOf(pieces.size());
    {
      unsigned int pc = 0;
      for (unsigned int part = 0; part < prob.getNumberOfParts(); part++)
        for (unsigned int j = 0; j < prob.getPartMaximum(part); j++)
          partOf[pc++] = part;
    }

    color.assign(pieces.size(), 0);
    for (unsigned int i = 0; i < pieces.size(); i++) {
      unsigned int shapeId = prob.getShapeIdOfPart(partOf[i]);
      if (startGoal) {
        unsigned int col = shapeId + 1;
        for (unsigned int gi = 0; gi < tray->getXYZ(); gi++)
          if (tray->getGoalPiece(gi) == col) {
            color[i] = col;
            break;
          }
      } else if (goal) {
        unsigned int col = pieceColor(prob, shapeId);
        if (col == 0)
          col = partOf[i] + 1;
        for (unsigned int gi = 0; gi < goal->getXYZ(); gi++)
          if (goal->getColor(gi) == col) {
            color[i] = col;
            break;
          }
      }
    }
  }

  /** Piece i, placed at place, is where the goal wants it. */
  bool piece(unsigned int i, const Place & place) const {
    const voxel_c * p = pieces[i];
    if (startGoal) {
      if (!p)
        return false;
      /* Empty cells are walls. A piece may slide across a variable cell, and
       * the finished position must leave every variable cell empty. Only
       * normal cells are legal places to stop, including a goal stamp. */
      bool ok = true;
      forCells(p, place, [&](int tx, int ty, int tz) {
        if (!isFloor(*tray, tx, ty, tz) || tray->getState(tx, ty, tz) == voxel_c::VX_VARIABLE ||
            (color[i] && tray->getGoalPiece(tx, ty, tz) != color[i]))
          ok = false;
      });
      return ok;
    }
    if (!goal || !color[i])
      return true;
    if (!p)
      return false;
    bool ok = true;
    forCells(p, place, [&](int tx, int ty, int tz) {
      if (!isFloor(*goal, tx, ty, tz) || goal->getColor(tx, ty, tz) != color[i])
        ok = false;
    });
    return ok;
  }

  bool all(const SlideState & s) const {
    for (unsigned int i = 0; i < pieces.size(); i++)
      if (!piece(i, s.places[i]))
        return false;
    return true;
  }

private:
  template <class F>
  static void forCells(const voxel_c * p, const Place & place, F f) {
    int hx = (int)p->getHx();
    int hy = (int)p->getHy();
    int hz = (int)p->getHz();
    for (unsigned int z = 0; z < p->getZ(); z++)
      for (unsigned int y = 0; y < p->getY(); y++)
        for (unsigned int x = 0; x < p->getX(); x++)
          if (p->getState(x, y, z) != voxel_c::VX_EMPTY)
            f(place.x + (int)x - hx, place.y + (int)y - hy, place.z + (int)z - hz);
  }

  const std::vector<const voxel_c *> & pieces;
  const voxel_c * tray = nullptr;
  const voxel_c * goal = nullptr;
  bool startGoal = false;
  /** The goal colour of each piece; 0 when the goal does not place it. */
  std::vector<unsigned int> color;
};

/* One voxel of a piece, relative to its hotspot. */
struct cell3_c {
  int x, y, z;
};
using cell3List_t = std::vector<cell3_c>;
using cellList_t = std::vector<std::pair<int, int>>;

/* Filled voxels of a piece, every layer, relative to its hotspot. Pieces
 * only ever translate in a sliding search, so these are worked out once. */
cell3List_t cellsOf(const voxel_c & p) {
  cell3List_t out;
  int hx = (int)p.getHx();
  int hy = (int)p.getHy();
  int hz = (int)p.getHz();
  for (unsigned int z = 0; z < p.getZ(); z++)
    for (unsigned int y = 0; y < p.getY(); y++)
      for (unsigned int x = 0; x < p.getX(); x++)
        if (p.getState(x, y, z) != voxel_c::VX_EMPTY)
          out.push_back({(int)x - hx, (int)y - hy, (int)z - hz});
  return out;
}

/* A piece's outline: its own voxels plus every empty voxel that has voxels
 * of the piece on both sides of it in the same row or the same column of
 * its layer, such as the inside of a pocket. Relative to the hotspot. */
cell3List_t outlineOf(const voxel_c & p) {
  cell3List_t out;
  int hx = (int)p.getHx();
  int hy = (int)p.getHy();
  int hz = (int)p.getHz();
  const int sx = (int)p.getX();
  const int sy = (int)p.getY();
  for (int z = 0; z < (int)p.getZ(); z++) {
    auto filled = [&p, z](int x, int y) {
      return p.getState((unsigned int)x, (unsigned int)y, (unsigned int)z) != voxel_c::VX_EMPTY;
    };
    for (int y = 0; y < sy; y++)
      for (int x = 0; x < sx; x++) {
        bool in = filled(x, y);
        if (!in) {
          bool left = false, right = false, up = false, down = false;
          for (int k = 0; k < x; k++) left = left || filled(k, y);
          for (int k = x + 1; k < sx; k++) right = right || filled(k, y);
          for (int k = 0; k < y; k++) up = up || filled(x, k);
          for (int k = y + 1; k < sy; k++) down = down || filled(x, k);
          in = (left && right) || (up && down);
        }
        if (in)
          out.push_back({x - hx, y - hy, z - hz});
      }
  }
  return out;
}

/* Floor voxels, every layer, not covered by any piece outside `movers`. */
struct freeGrid_c {
  int w = 0;
  int h = 0;
  int d = 0;
  std::vector<char> open;
  bool at(int x, int y, int z) const {
    return x >= 0 && y >= 0 && z >= 0 && x < w && y < h && z < d &&
           open[(size_t)((z * h + y) * w + x)];
  }
};

freeGrid_c freeCells(const voxel_c & tray, const std::vector<cell3List_t> & cells,
                     const SlideState & s, const std::vector<unsigned int> & movers) {
  freeGrid_c g;
  g.w = (int)tray.getX();
  g.h = (int)tray.getY();
  g.d = (int)tray.getZ();
  g.open.assign((size_t)(g.w * g.h * g.d), 0);
  for (int z = 0; z < g.d; z++)
    for (int y = 0; y < g.h; y++)
      for (int x = 0; x < g.w; x++)
        g.open[(size_t)((z * g.h + y) * g.w + x)] = isFloor(tray, x, y, z) ? 1 : 0;
  for (unsigned int i = 0; i < s.places.size(); i++) {
    if (std::find(movers.begin(), movers.end(), i) != movers.end())
      continue;
    for (const auto & c : cells[i]) {
      int x = s.places[i].x + c.x;
      int y = s.places[i].y + c.y;
      int z = s.places[i].z + c.z;
      if (x >= 0 && y >= 0 && z >= 0 && x < g.w && y < g.h && z < g.d)
        g.open[(size_t)((z * g.h + y) * g.w + x)] = 0;
    }
  }
  return g;
}

/* The movers fit when shifted by (dx, dy) as one rigid group. */
bool groupFits(const freeGrid_c & g, const std::vector<cell3List_t> & cells,
               const SlideState & s, const std::vector<unsigned int> & movers, int dx, int dy) {
  for (unsigned int m : movers)
    for (const auto & c : cells[m])
      if (!g.at(s.places[m].x + c.x + dx, s.places[m].y + c.y + dy, s.places[m].z + c.z))
        return false;
  return true;
}

/* Every shift the movers can reach as one group while the others stay put.
 * One move of the solver is a jump to any of them, whatever the route. */
cellList_t reachableShifts(const voxel_c & tray, const std::vector<cell3List_t> & cells,
                           const SlideState & s, const std::vector<unsigned int> & movers) {
  freeGrid_c g = freeCells(tray, cells, s, movers);
  std::set<std::pair<int, int>> seen{{0, 0}};
  cellList_t out;
  std::queue<std::pair<int, int>> q;
  q.push({0, 0});
  while (!q.empty()) {
    std::pair<int, int> c = q.front();
    q.pop();
    for (int d = 0; d < 4; d++) {
      std::pair<int, int> n{c.first + DX[d], c.second + DY[d]};
      if (seen.count(n) || !groupFits(g, cells, s, movers, n.first, n.second))
        continue;
      seen.insert(n);
      out.push_back(n);
      q.push(n);
    }
  }
  return out;
}

/* Corner points, as shifts from the start, of a route with the fewest
 * straight runs that takes the movers to shift (tx, ty), start and end
 * included. Empty when unreachable. */
cellList_t fewestTurns(const voxel_c & tray, const std::vector<cell3List_t> & cells,
                       const SlideState & s, const std::vector<unsigned int> & movers,
                       int tx, int ty) {
  freeGrid_c g = freeCells(tray, cells, s, movers);
  const std::pair<int, int> from{0, 0};
  const std::pair<int, int> to{tx, ty};
  std::map<std::pair<int, int>, std::pair<int, int>> came{{from, from}};
  std::queue<std::pair<int, int>> q;
  q.push(from);
  while (!q.empty() && !came.count(to)) {
    std::pair<int, int> c = q.front();
    q.pop();
    for (int d = 0; d < 4; d++) {
      std::pair<int, int> n = c;
      while (true) {
        n.first += DX[d];
        n.second += DY[d];
        if (!groupFits(g, cells, s, movers, n.first, n.second))
          break;
        if (came.count(n))
          continue;
        came[n] = c;
        q.push(n);
      }
    }
  }
  cellList_t route;
  if (!came.count(to))
    return route;
  for (std::pair<int, int> c = to; ; c = came[c]) {
    route.push_back(c);
    if (c == from)
      break;
  }
  std::reverse(route.begin(), route.end());
  return route;
}

/* `outer` and everything nested inside it: pieces whose cells all lie in
 * the outline of a piece already in the group, repeated until no more join.
 * Pieces that merely touch are not nested. */
std::vector<unsigned int> nestedGroup(const std::vector<cell3List_t> & cells,
                                      const std::vector<cell3List_t> & outlines,
                                      const SlideState & s, unsigned int outer) {
  std::vector<unsigned int> group{outer};
  for (size_t k = 0; k < group.size(); k++) {
    unsigned int a = group[k];
    std::set<std::tuple<int, int, int>> inside;
    for (const auto & c : outlines[a])
      inside.insert({s.places[a].x + c.x, s.places[a].y + c.y, s.places[a].z + c.z});
    for (unsigned int b = 0; b < s.places.size(); b++) {
      if (std::find(group.begin(), group.end(), b) != group.end() || cells[b].empty())
        continue;
      bool all = true;
      for (const auto & c : cells[b])
        if (!inside.count({s.places[b].x + c.x, s.places[b].y + c.y, s.places[b].z + c.z})) {
          all = false;
          break;
        }
      if (all)
        group.push_back(b);
    }
  }
  return group;
}

} // namespace

unsigned long memoryStates(unsigned long stateBytes, bool high) {
  if (stateBytes == 0)
    stateBytes = 1;
  unsigned long long states = SEARCH_MEMORY_BYTES / stateBytes;
  if (high)
    states = std::max(states, physicalMemoryBytes() / 2 / stateBytes);
  /* unsigned long is 32 bits on Windows. */
  const unsigned long most = static_cast<unsigned long>(-1);
  return states < most ? static_cast<unsigned long>(states) : most;
}

bool isSliding(const puzzle_c & puz) {
  return puz.getGridType()->getType() == gridType_c::GT_SLIDING;
}

bool isSliding(const problem_c & prob) {
  return isSliding(prob.getPuzzle());
}

static const char * SG_PREFIX = "sg:";
static const char * HIDDEN_PREFIX = "__sggoal:";

bool isStartGoalShape(const voxel_c * v) {
  return v && v->getName().rfind(SG_PREFIX, 0) == 0;
}

bool isHiddenSlidingShape(const voxel_c * v) {
  return v && v->getName().rfind(HIDDEN_PREFIX, 0) == 0;
}

int startGoalNumber(const voxel_c * v) {
  if (!isStartGoalShape(v))
    return 0;
  return atoi(v->getName().c_str() + 3);
}

std::string startGoalUserName(const voxel_c * v) {
  if (!isStartGoalShape(v))
    return v ? v->getName() : std::string();
  const std::string & n = v->getName();
  size_t colon = n.find(':', 3);
  if (colon == std::string::npos || colon + 1 >= n.size())
    return std::string();
  return n.substr(colon + 1);
}

void setStartGoalUserName(voxel_c * v, const std::string & user) {
  if (!v || !isStartGoalShape(v)) {
    if (v) v->setName(user);
    return;
  }
  int num = startGoalNumber(v);
  v->setName(std::string(SG_PREFIX) + std::to_string(num) + ":" + user);
}

std::string displayName(const voxel_c * v) {
  if (!v)
    return std::string();
  if (!isStartGoalShape(v))
    return v->getName();
  std::string user = startGoalUserName(v);
  std::string tag = std::string("(START/GOAL ") + std::to_string(startGoalNumber(v)) + ")";
  if (user.empty())
    return tag;
  return tag + " " + user;
}

unsigned int addStartGoalShape(puzzle_c & puz, unsigned int sx, unsigned int sy) {
  int next = 1;
  for (unsigned int i = 0; i < puz.getNumberOfShapes(); i++) {
    int n = startGoalNumber(puz.getShape(i));
    if (n >= next)
      next = n + 1;
  }
  if (sx < 1) sx = 1;
  if (sy < 1) sy = 1;
  unsigned int id = puz.addShape(sx, sy, 1);
  voxel_c * v = puz.getShape(id);
  for (unsigned int y = 0; y < v->getY(); y++)
    for (unsigned int x = 0; x < v->getX(); x++)
      if (v->validCoordinate(x, y, 0))
        v->setState(x, y, 0, voxel_c::VX_FILLED);
  v->setName(std::string(SG_PREFIX) + std::to_string(next) + ":");
  return id;
}

void noteShapeRemoved(puzzle_c & puz, unsigned int shapeId) {
  unsigned int gone = shapeId + 1;
  for (unsigned int s = 0; s < puz.getNumberOfShapes(); s++) {
    if (!isStartGoalShape(puz.getShape(s)))
      continue;
    voxel_c * v = puz.getShape(s);
    for (unsigned int i = 0; i < v->getXYZ(); i++) {
      unsigned int c = v->getColor(i);
      if (c == gone)
        v->setColor(i, 0);
      else if (c > gone)
        v->setColor(i, c - 1);
      unsigned int g = v->getGoalPiece(i);
      if (g == gone)
        v->setGoalPiece(i, 0);
      else if (g > gone)
        v->setGoalPiece(i, g - 1);
    }
  }
}

void noteShapeSwap(puzzle_c & puz, unsigned int a, unsigned int b) {
  if (a == b)
    return;
  unsigned int ca = a + 1;
  unsigned int cb = b + 1;
  for (unsigned int s = 0; s < puz.getNumberOfShapes(); s++) {
    if (!isStartGoalShape(puz.getShape(s)))
      continue;
    voxel_c * v = puz.getShape(s);
    for (unsigned int i = 0; i < v->getXYZ(); i++) {
      unsigned int c = v->getColor(i);
      if (c == ca) v->setColor(i, cb);
      else if (c == cb) v->setColor(i, ca);
      unsigned int g = v->getGoalPiece(i);
      if (g == ca) v->setGoalPiece(i, cb);
      else if (g == cb) v->setGoalPiece(i, ca);
    }
  }
}

bool toggleCellMark(voxel_c * tray, int x, int y, unsigned int shapeId, bool goal) {
  if (!tray || x < 0 || y < 0 || (unsigned)x >= tray->getX() || (unsigned)y >= tray->getY())
    return false;
  if (!tray->validCoordinate(x, y, 0))
    return false;
  if (tray->getState(x, y, 0) == voxel_c::VX_EMPTY)
    return false;
  unsigned int id = shapeId + 1;
  if (id > 63)
    return false;
  if (goal) {
    unsigned int cur = tray->getGoalPiece(x, y, 0);
    tray->setGoalPiece((unsigned)tray->getIndex(x, y, 0), cur == id ? 0 : id);
  } else {
    unsigned int cur = tray->getColor(x, y, 0);
    tray->setColor(x, y, 0, cur == id ? 0 : id);
  }
  return true;
}

static void paintPieceLock(voxel_c * piece, unsigned int color) {
  for (unsigned int i = 0; i < piece->getXYZ(); i++) {
    if (piece->getState(i) == voxel_c::VX_EMPTY)
      piece->setColor(i, 0);
    else
      piece->setColor(i, color);
  }
}

void refreshStartLocks(problem_c & prob) {
  if (!prob.resultValid())
    return;
  const voxel_c * tray = getResultShape(prob);
  if (!isStartGoalShape(tray))
    return;
  puzzle_c & puz = prob.getPuzzle();
  /* The assembler skips colour checks when the palette is empty. Start stamps
   * are voxel colours, so the palette has to cover them or the start cell
   * does not constrain the initial placement. */
  unsigned int maxStamp = 0;
  for (unsigned int i = 0; i < tray->getXYZ(); i++) {
    if (tray->getColor(i) > maxStamp)
      maxStamp = tray->getColor(i);
    if (tray->getGoalPiece(i) > maxStamp)
      maxStamp = tray->getGoalPiece(i);
  }
  if (maxStamp)
    findOrAddColor(puz, maxStamp);

  for (unsigned int s = 0; s < puz.getNumberOfShapes(); s++) {
    if (isStartGoalShape(puz.getShape(s)) || isHiddenSlidingShape(puz.getShape(s)))
      continue;
    unsigned int id = s + 1;
    bool used = false;
    for (unsigned int i = 0; i < tray->getXYZ() && !used; i++)
      if (tray->getColor(i) == id)
        used = true;
    paintPieceLock(puz.getShape(s), used ? id : 0);
  }
}

bool shapeIsRequired(const problem_c & prob, unsigned int shapeId) {
  if (!prob.resultValid())
    return false;
  const voxel_c * tray = getResultShape(prob);
  if (!tray || !isStartGoalShape(tray))
    return false;
  unsigned int id = shapeId + 1;
  for (unsigned int i = 0; i < tray->getXYZ(); i++)
    if (tray->getColor(i) == id || tray->getGoalPiece(i) == id)
      return true;
  return false;
}

void syncSlidingProblems(puzzle_c & puz) {
  if (!isSliding(puz))
    return;

  /* Drop problems that are not tied to a start/goal shape. */
  for (int p = (int)puz.getNumberOfProblems() - 1; p >= 0; p--) {
    problem_c * pr = puz.getProblem(p);
    if (!pr->resultValid() || !isStartGoalShape(puz.getShape(pr->getResultId())))
      puz.removeProblem(p);
  }

  for (unsigned int s = 0; s < puz.getNumberOfShapes(); s++) {
    if (!isStartGoalShape(puz.getShape(s)))
      continue;
    problem_c * found = nullptr;
    for (unsigned int p = 0; p < puz.getNumberOfProblems(); p++) {
      problem_c * pr = puz.getProblem(p);
      if (pr->resultValid() && pr->getResultId() == s) {
        found = pr;
        break;
      }
    }
    if (!found) {
      unsigned int pi = puz.addProblem();
      found = puz.getProblem(pi);
      found->setResultId(s);
      found->setName(std::string("START/GOAL ") + std::to_string(startGoalNumber(puz.getShape(s))));
    } else if (found->getResultId() != s) {
      found->setResultId(s);
    }

    voxel_c * tray = puz.getShape(s);
    for (unsigned int i = 0; i < puz.getNumberOfShapes(); i++) {
      if (isStartGoalShape(puz.getShape(i)) || isHiddenSlidingShape(puz.getShape(i)))
        continue;
      bool used = false;
      unsigned int id = i + 1;
      for (unsigned int c = 0; c < tray->getXYZ() && !used; c++)
        if (tray->getColor(c) == id || tray->getGoalPiece(c) == id)
          used = true;
      if (used) {
        if (found->getShapeMaximum(i) < 1)
          found->setShapeMaximum(i, 1);
        if (found->getShapeMinimum(i) < 1)
          found->setShapeMinimum(i, 1);
        if (found->getShapeMaximum(i) < found->getShapeMinimum(i))
          found->setShapeMaximum(i, found->getShapeMinimum(i));
      }
    }
    refreshStartLocks(*found);
  }
}

unsigned int floorCells(const voxel_c & tray) {
  unsigned int n = 0;
  for (unsigned int i = 0; i < tray.getXYZ(); i++)
    if (tray.getState(i) != voxel_c::VX_EMPTY)
      n++;
  return n;
}

unsigned int pieceColor(const problem_c & prob, unsigned int shapeId) {
  if (shapeId >= prob.getPuzzle().getNumberOfShapes())
    return 0;
  const voxel_c * piece = prob.getPuzzle().getShape(shapeId);
  for (unsigned int i = 0; i < piece->getXYZ(); i++)
    if (piece->getState(i) != voxel_c::VX_EMPTY && piece->getColor(i) != 0)
      return piece->getColor(i);
  return 0;
}

/* The goal-map model below (a separate goal shape painted in piece colours)
 * came before start/goal shapes. The editor no longer uses it, but older
 * files can hold such goals and the solver still reads them; the tests use
 * these to build such puzzles. */

void ensureSetup(puzzle_c & puz, unsigned int width, unsigned int height) {
  if (!isSliding(puz))
    return;

  if (puz.getNumberOfShapes() == 0) {
    unsigned int trayId = puz.addShape(width, height, 1);
    fillTray(puz.getShape(trayId));
    puz.getShape(trayId)->setName("Tray");

    unsigned int goalId = puz.addShape(width, height, 1);
    fillTray(puz.getShape(goalId));
    puz.getShape(goalId)->setName(GOAL_SHAPE_NAME);

    unsigned int prob = puz.addProblem();
    puz.getProblem(prob)->setResultId(trayId);
    puz.getProblem(prob)->setGoalId(goalId);
    return;
  }

  /* Ensure every problem has a goal map. */
  for (unsigned int pi = 0; pi < puz.getNumberOfProblems(); pi++) {
    problem_c * pr = puz.getProblem(pi);
    if (pr->goalValid())
      continue;

    /* Find an unused shape named as goal, or create one. */
    unsigned int goalId = 0xFFFFFFFF;
    for (unsigned int s = 0; s < puz.getNumberOfShapes(); s++) {
      if (puz.getShape(s)->getName() == GOAL_SHAPE_NAME) {
        bool used = false;
        if (pr->resultValid() && pr->getResultId() == s)
          used = true;
        for (unsigned int p = 0; p < pr->getNumberOfParts() && !used; p++)
          if (pr->getShapeIdOfPart(p) == s)
            used = true;
        if (!used) {
          goalId = s;
          break;
        }
      }
    }
    if (goalId == 0xFFFFFFFF) {
      unsigned int w = 6, h = 6;
      if (pr->resultValid()) {
        w = getResultShape(*pr)->getX();
        h = getResultShape(*pr)->getY();
      }
      goalId = puz.addShape(w, h, 1);
      fillTray(puz.getShape(goalId));
      puz.getShape(goalId)->setName(GOAL_SHAPE_NAME);
    }
    pr->setGoalId(goalId);
  }
}

bool hasStart(const problem_c & prob, unsigned int shapeId) {
  unsigned int col = pieceColor(prob, shapeId);
  if (col == 0 || !prob.resultValid())
    return false;
  const voxel_c * tray = getResultShape(prob);
  for (unsigned int i = 0; i < tray->getXYZ(); i++)
    if (tray->getColor(i) == col)
      return true;
  return false;
}

bool hasGoal(const problem_c & prob, unsigned int shapeId) {
  if (!prob.goalValid())
    return false;
  unsigned int col = piecePartIndex(prob, shapeId);
  /* Goal uses the part-index colour even when the piece is Neutral. */
  if (prob.getPuzzle().colorNumber() < col)
    return false;
  const voxel_c * goal = prob.getGoalShape();
  for (unsigned int i = 0; i < goal->getXYZ(); i++)
    if (goal->getColor(i) == col)
      return true;
  /* Also accept piece's painted colour if present. */
  unsigned int pcol = pieceColor(prob, shapeId);
  if (pcol == 0)
    return false;
  for (unsigned int i = 0; i < goal->getXYZ(); i++)
    if (goal->getColor(i) == pcol)
      return true;
  return false;
}

bool placeStart(problem_c & prob, unsigned int shapeId, int ax, int ay) {
  if (!prob.resultValid())
    return false;
  puzzle_c & puz = prob.getPuzzle();
  voxel_c * tray = getResultShape(prob);
  voxel_c * piece = puz.getShape(shapeId);

  unsigned int col = pieceColor(prob, shapeId);
  if (col == 0) {
    col = findOrAddColor(puz, piecePartIndex(prob, shapeId));
  }

  if (!footprintFits(*tray, *piece, ax, ay, col))
    return false;

  eraseColor(tray, col);
  paintPieceColor(piece, col);
  paintFootprint(tray, *piece, ax, ay, col);
  syncMaxHoles(prob);
  return true;
}

void clearStart(problem_c & prob, unsigned int shapeId) {
  if (!prob.resultValid())
    return;
  unsigned int col = pieceColor(prob, shapeId);
  if (col == 0)
    return;
  voxel_c * tray = getResultShape(prob);
  voxel_c * piece = prob.getPuzzle().getShape(shapeId);
  eraseColor(tray, col);
  paintPieceNeutral(piece);
  /* Keep goal colours: goals may reference the part-index colour. */
  syncMaxHoles(prob);
}

bool placeGoal(problem_c & prob, unsigned int shapeId, int ax, int ay) {
  if (!prob.goalValid() || !prob.resultValid())
    return false;
  puzzle_c & puz = prob.getPuzzle();
  const voxel_c * tray = getResultShape(prob);
  voxel_c * goal = const_cast<voxel_c *>(prob.getGoalShape());
  const voxel_c * piece = puz.getShape(shapeId);

  unsigned int col = pieceColor(prob, shapeId);
  if (col == 0)
    col = findOrAddColor(puz, piecePartIndex(prob, shapeId));

  if (!footprintFits(*tray, *piece, ax, ay, 0))
    return false;

  eraseColor(goal, col);
  paintFootprint(goal, *piece, ax, ay, col);
  return true;
}

void clearGoal(problem_c & prob, unsigned int shapeId) {
  if (!prob.goalValid())
    return;
  unsigned int col = pieceColor(prob, shapeId);
  unsigned int partCol = piecePartIndex(prob, shapeId);
  voxel_c * goal = const_cast<voxel_c *>(prob.getGoalShape());
  if (col)
    eraseColor(goal, col);
  eraseColor(goal, partCol);
}

bool clearStartAt(problem_c & prob, int x, int y) {
  if (!prob.resultValid())
    return false;
  const voxel_c * tray = getResultShape(prob);
  if (!isFloor(*tray, x, y))
    return false;
  unsigned int col = tray->getColor(x, y, 0);
  if (col == 0)
    return false;
  for (unsigned int p = 0; p < prob.getNumberOfParts(); p++) {
    unsigned int sid = prob.getShapeIdOfPart(p);
    if (pieceColor(prob, sid) == col) {
      clearStart(prob, sid);
      return true;
    }
  }
  return false;
}

bool toggleGoal(problem_c & prob, unsigned int shapeId, int ax, int ay) {
  if (!prob.goalValid())
    return false;
  unsigned int col = pieceColor(prob, shapeId);
  if (col == 0)
    col = piecePartIndex(prob, shapeId);

  const voxel_c * piece = prob.getPuzzle().getShape(shapeId);
  const voxel_c * goal = prob.getGoalShape();
  int curAx, curAy;
  if (findColorAnchor(*goal, *piece, col, &curAx, &curAy) &&
      curAx == ax && curAy == ay) {
    clearGoal(prob, shapeId);
    return true;
  }
  return placeGoal(prob, shapeId, ax, ay);
}

void syncMaxHoles(problem_c & prob) {
  if (!prob.resultValid())
    return;
  unsigned int floor = floorCells(*getResultShape(prob));
  unsigned int volume = 0;
  for (unsigned int p = 0; p < prob.getNumberOfParts(); p++) {
    const voxel_c * sh = prob.getPartShape(p);
    unsigned int cnt = prob.getPartMaximum(p);
    unsigned int cells = 0;
    for (unsigned int i = 0; i < sh->getXYZ(); i++)
      if (sh->getState(i) != voxel_c::VX_EMPTY)
        cells++;
    volume += cells * cnt;
  }
  if (volume > floor)
    prob.setMaxHoles(0);
  else
    prob.setMaxHoles(floor - volume);
}

namespace {

/* The original slide search: an arrangement is a text key, and each move
 * works out the free cells afresh. Kept for trays the position tables
 * cannot pack into 128 bits, and for A/B runs (BURRTOOLS_SLIDE_LEGACY=1). */
void legacySearch(const voxel_c & tray, const std::vector<cell3List_t> & cells,
                  const std::vector<cell3List_t> & outlines, bool nested,
                  const SlideState & initial, const goalCheck_c & goals,
                  slideSearch_c & search, std::vector<SlideState> & path) {
  const unsigned int n = (unsigned int)initial.places.size();
  const unsigned long maxStates = search.maxStates;

  std::queue<SlideState> q;
  std::map<std::string, std::pair<std::string, unsigned int>> parent;
  /* parent[key] = {prevKey, movedPiece}; movedPiece==n means root. */

  std::string startKey = stateKey(initial);

  /* Each arrangement held costs a map node with two copies of its key, plus
   * its share of the queue; measured at about 115 + 2 bytes per key
   * character, so this leaves some room. */
  const unsigned long memoryLimit = search.maxMemoryStates
      ? search.maxMemoryStates
      : memoryStates(128 + 3 * (unsigned long)startKey.size(), search.highMemory);
  search.memoryStates = memoryLimit;
  q.push(initial);
  parent[startKey] = {"", n};

  unsigned long visited = 0;
  std::string goalKey;
  bool found = false;

  while (!q.empty()) {
    if (maxStates != FULL_SEARCH && visited >= maxStates) {
      search.outcome = SLIDE_LIMIT;
      break;
    }
    if (parent.size() >= memoryLimit) {
      search.outcome = SLIDE_MEMORY;
      break;
    }
    if ((visited & 1023) == 0) {
      if (search.progress)
        search.progress->store(visited, std::memory_order_relaxed);
      if (search.stop && search.stop->load(std::memory_order_relaxed)) {
        search.outcome = SLIDE_STOPPED;
        break;
      }
    }
    SlideState cur = q.front();
    q.pop();
    visited++;

    /* One move is one piece going anywhere it can reach while the others
     * stay put, round corners included, so the search finds the fewest
     * moves. Counting single-cell steps or straight runs instead leaves
     * equally short paths that alternate pieces, or stop a piece at a
     * corner and come back to it after another piece has moved.
     * With nested slides a piece may also carry what is nested inside it. */
    const std::string curKey = stateKey(cur);
    for (unsigned int pi = 0; pi < n && !found; pi++) {
      std::vector<std::vector<unsigned int>> moverSets{{pi}};
      if (nested) {
        std::vector<unsigned int> group = nestedGroup(cells, outlines, cur, pi);
        if (group.size() > 1)
          moverSets.push_back(group);
      }
      for (const std::vector<unsigned int> & movers : moverSets) {
        for (const std::pair<int, int> & shift : reachableShifts(tray, cells, cur, movers)) {
          SlideState next = cur;
          for (unsigned int m : movers) {
            next.places[m].x += shift.first;
            next.places[m].y += shift.second;
          }
          std::string key = stateKey(next);
          if (parent.count(key))
            continue;
          parent[key] = {curKey, pi};
          if (goals.all(next)) {
            goalKey = key;
            found = true;
            q = {};
            break;
          }
          q.push(next);
        }
        if (found)
          break;
      }
    }
  }

  search.visited = visited;
  if (!found)
    return;

  /* Reconstruct the path. States keep the assembly placement of each piece
   * so the move slider can drop them straight onto the board. */
  {
    std::string k = goalKey;
    while (true) {
      /* Rebuild state from key. */
      SlideState st;
      st.places.resize(n);
      std::istringstream is(k);
      for (unsigned int i = 0; i < n; i++) {
        char colon, comma, bar;
        int t, x, y;
        if (i)
          is >> bar;
        is >> t >> colon >> x >> comma >> y;
        st.places[i].trans = (unsigned char)t;
        st.places[i].x = x;
        st.places[i].y = y;
        st.places[i].z = initial.places[i].z;
      }
      path.push_back(st);
      auto it = parent.find(k);
      if (it == parent.end() || it->second.second == n)
        break;
      k = it->second.first;
    }
  }
  std::reverse(path.begin(), path.end());
}

#ifdef __SIZEOF_INT128__
/* The key of a search whose arrangements do not fit 64 bits. */
__extension__ typedef unsigned __int128 wideKey_t;
#endif

/* Every position one piece can take in the empty tray. Pieces only ever
 * translate, so this is worked out once per search: a piece keyed to a
 * narrow slot, like the stepped back layer of a Panex tray, gets a short
 * list. */
struct posTable_c {
  /* Anchor box holding every position; index maps an anchor to its position. */
  int x0 = 0;
  int y0 = 0;
  int w = 0;
  int h = 0;
  std::vector<int> index;
  std::vector<int> px;
  std::vector<int> py;
  /* The neighbouring position one step along DX/DY, or -1. */
  std::vector<std::array<int, 4>> next;
  /* Tray cells each position covers, `words` 64-bit words per position. */
  std::vector<uint64_t> mask;
  /* With nested slides: the cells of the piece's outline at each position,
   * laid out as mask is. A piece is nested in this one when its mask lies
   * wholly inside. */
  std::vector<uint64_t> outline;
  std::vector<char> atGoal;
  /* Where this piece's position number sits in a packed arrangement. */
  unsigned int shift = 0;
  uint64_t bits = 0;

  int at(int x, int y) const {
    x -= x0;
    y -= y0;
    if (x < 0 || y < 0 || x >= w || y >= h)
      return -1;
    return index[(size_t)(y * w + x)];
  }
};

/* Build the position tables. False when they cannot hold this search: a
 * piece with no cells or nowhere to go, a start off the floor, or more
 * positions than keyBits bits can number. */
bool buildTables(const voxel_c & tray, const std::vector<cell3List_t> & cells,
                 const std::vector<cell3List_t> & outlines, bool nested,
                 const SlideState & initial, const goalCheck_c & goals,
                 unsigned int keyBits, size_t & words, std::vector<posTable_c> & tables) {
  const int W = (int)tray.getX();
  const int H = (int)tray.getY();
  const int D = (int)tray.getZ();
  words = ((size_t)W * H * D + 63) / 64;
  const unsigned int n = (unsigned int)cells.size();
  tables.assign(n, posTable_c());
  unsigned int used = 0;

  for (unsigned int i = 0; i < n; i++) {
    if (cells[i].empty())
      return false;
    int minX = cells[i][0].x, maxX = minX, minY = cells[i][0].y, maxY = minY;
    for (const auto & c : cells[i]) {
      minX = std::min(minX, c.x);
      maxX = std::max(maxX, c.x);
      minY = std::min(minY, c.y);
      maxY = std::max(maxY, c.y);
    }
    posTable_c & t = tables[i];
    t.x0 = -minX;
    t.y0 = -minY;
    t.w = W - maxX - t.x0;
    t.h = H - maxY - t.y0;
    if (t.w <= 0 || t.h <= 0)
      return false;
    t.index.assign((size_t)(t.w * t.h), -1);

    const Place & home = initial.places[i];
    for (int y = t.y0; y < t.y0 + t.h; y++)
      for (int x = t.x0; x < t.x0 + t.w; x++) {
        bool fits = true;
        for (const auto & c : cells[i])
          if (!isFloor(tray, x + c.x, y + c.y, home.z + c.z)) {
            fits = false;
            break;
          }
        if (!fits)
          continue;
        t.index[(size_t)((y - t.y0) * t.w + (x - t.x0))] = (int)t.px.size();
        t.px.push_back(x);
        t.py.push_back(y);
        t.mask.resize(t.mask.size() + words, 0);
        uint64_t * m = &t.mask[t.mask.size() - words];
        for (const auto & c : cells[i]) {
          size_t bit = ((size_t)(home.z + c.z) * H + (size_t)(y + c.y)) * W + (size_t)(x + c.x);
          m[bit / 64] |= uint64_t(1) << (bit % 64);
        }
        if (nested) {
          t.outline.resize(t.outline.size() + words, 0);
          uint64_t * o = &t.outline[t.outline.size() - words];
          for (const auto & c : outlines[i]) {
            const int ox = x + c.x, oy = y + c.y, oz = home.z + c.z;
            if (ox < 0 || oy < 0 || oz < 0 || ox >= W || oy >= H || oz >= D)
              continue;
            size_t bit = ((size_t)oz * H + (size_t)oy) * W + (size_t)ox;
            o[bit / 64] |= uint64_t(1) << (bit % 64);
          }
        }
        t.atGoal.push_back(goals.piece(i, Place{x, y, home.trans, home.z}) ? 1 : 0);
      }

    const size_t count = t.px.size();
    if (count == 0 || t.at(home.x, home.y) < 0)
      return false;
    t.next.resize(count);
    for (size_t k = 0; k < count; k++)
      for (int d = 0; d < 4; d++)
        t.next[k][d] = t.at(t.px[k] + DX[d], t.py[k] + DY[d]);

    unsigned int width = 1;
    while (width < 64 && (uint64_t(1) << width) < count)
      width++;
    if (used + width > keyBits)
      return false;
    t.shift = used;
    t.bits = width == 64 ? ~uint64_t(0) : (uint64_t(1) << width) - 1;
    used += width;
  }
  return true;
}

/* Arrangement -> the arrangement it was reached from, for tableSearch. Open
 * addressing in two flat arrays: no allocation per entry, unlike
 * std::unordered_map, so about twice as fast. */
template <class key_t>
class parentMap_c {
public:
  parentMap_c(void) { rehash(1u << 16); }

  /* Add key with its parent; false when key is there already. */
  bool emplace(key_t key, key_t parent) {
    if (key == EMPTY) {
      if (hasEmptyKey)
        return false;
      hasEmptyKey = true;
      emptyKeyParent = parent;
      count++;
      return true;
    }
    size_t i = slot(key);
    while (keys[i] != EMPTY) {
      if (keys[i] == key)
        return false;
      i = (i + 1) & mask;
    }
    keys[i] = key;
    parents[i] = parent;
    if (++count * 10 > keys.size() * 7)
      rehash(keys.size() * 2);
    return true;
  }

  /* The parent of a key that is there. */
  key_t parentOf(key_t key) const {
    if (key == EMPTY)
      return emptyKeyParent;
    size_t i = slot(key);
    while (keys[i] != key)
      i = (i + 1) & mask;
    return parents[i];
  }

  size_t size(void) const { return count; }

private:
  static constexpr key_t EMPTY = ~key_t(0);

  size_t slot(key_t key) const {
    return (size_t)((fold(key) * 0x9E3779B97F4A7C15ull) >> shift);
  }
  static uint64_t fold(uint64_t key) { return key; }
#ifdef __SIZEOF_INT128__
  static uint64_t fold(wideKey_t key) {
    return (uint64_t)key ^ ((uint64_t)(key >> 64) * 0xC2B2AE3D27D4EB4Full);
  }
#endif

  void rehash(size_t capacity) {
    std::vector<key_t> oldKeys(capacity, EMPTY);
    std::vector<key_t> oldParents(capacity);
    oldKeys.swap(keys);
    oldParents.swap(parents);
    mask = capacity - 1;
    shift = 64;
    for (size_t c = capacity; c > 1; c >>= 1)
      shift--;
    for (size_t j = 0; j < oldKeys.size(); j++)
      if (oldKeys[j] != EMPTY) {
        size_t i = slot(oldKeys[j]);
        while (keys[i] != EMPTY)
          i = (i + 1) & mask;
        keys[i] = oldKeys[j];
        parents[i] = oldParents[j];
      }
  }

  std::vector<key_t> keys;
  std::vector<key_t> parents;
  size_t mask = 0;
  unsigned int shift = 64;
  size_t count = 0;
  /* EMPTY marks a free slot, so that one key is kept aside. */
  bool hasEmptyKey = false;
  key_t emptyKeyParent = 0;
};

/* Memory one arrangement takes in tableSearch, in key sizes: two keys in
 * parentMap_c at a load between 35% and 70%, its share of the queue, and
 * while the table doubles the old and the new table together. Measured at
 * 51 bytes at peak with 8-byte keys on the 4x4 benchmark in
 * test_sliding.cpp; 7 keys leaves some room. */
const unsigned long TABLE_STATE_KEYS = 7;

/* The slide search over position tables. An arrangement is one position
 * number per piece packed into a key_t (64 bits, or 128 for trays that need
 * more), and a move is a flood fill over the
 * piece's own positions against a bitmask of the cells the others cover.
 * It visits arrangements in the same order as legacySearch, so both find
 * the same path. False, having searched nothing, when the tables cannot
 * hold this search. */
template <class key_t>
bool tableSearch(const voxel_c & tray, const std::vector<cell3List_t> & cells,
                 const std::vector<cell3List_t> & outlines, bool nested,
                 const SlideState & initial, const goalCheck_c & goals,
                 slideSearch_c & search, std::vector<SlideState> & path) {
  size_t words = 0;
  std::vector<posTable_c> t;
  if (!buildTables(tray, cells, outlines, nested, initial, goals, 8 * sizeof(key_t), words, t))
    return false;

  const unsigned int n = (unsigned int)initial.places.size();
  const unsigned long maxStates = search.maxStates;
  auto posOf = [&t](key_t key, unsigned int i) {
    return (int)((key >> t[i].shift) & t[i].bits);
  };
  auto decode = [&](key_t key) {
    SlideState st = initial;
    for (unsigned int i = 0; i < n; i++) {
      int p = posOf(key, i);
      st.places[i].x = t[i].px[(size_t)p];
      st.places[i].y = t[i].py[(size_t)p];
    }
    return st;
  };
  auto overlaps = [words](const uint64_t * a, const std::vector<uint64_t> & b) {
    for (size_t w = 0; w < words; w++)
      if (a[w] & b[w])
        return true;
    return false;
  };

  key_t startKey = 0;
  for (unsigned int i = 0; i < n; i++)
    startKey |= (key_t)t[i].at(initial.places[i].x, initial.places[i].y) << t[i].shift;

  /* Pieces that are copies of one another -- same cells, same layer, same
   * goal -- can swap places without changing the puzzle. Each such group is
   * kept with its positions in rising order, so the arrangements that differ
   * only by which copy sits where are searched as one.
   * BURRTOOLS_NO_SLIDE_SYMMETRY=1 turns this off, for A/B runs. */
  std::vector<std::vector<unsigned int>> copies;
  if (!std::getenv("BURRTOOLS_NO_SLIDE_SYMMETRY")) {
    auto sameCells = [](const cell3List_t & a, const cell3List_t & b) {
      return a.size() == b.size() &&
             std::equal(a.begin(), a.end(), b.begin(), [](const cell3_c & p, const cell3_c & q) {
               return p.x == q.x && p.y == q.y && p.z == q.z;
             });
    };
    std::vector<std::vector<unsigned int>> groups;
    for (unsigned int i = 0; i < n; i++) {
      bool placed = false;
      for (auto & g : groups) {
        const unsigned int j = g[0];
        if (initial.places[i].z == initial.places[j].z && sameCells(cells[i], cells[j]) &&
            t[i].px == t[j].px && t[i].py == t[j].py && t[i].atGoal == t[j].atGoal) {
          g.push_back(i);
          placed = true;
          break;
        }
      }
      if (!placed)
        groups.push_back({i});
    }
    for (auto & g : groups)
      if (g.size() > 1)
        copies.push_back(std::move(g));
  }
  auto canon = [&](key_t key) {
    for (const std::vector<unsigned int> & g : copies) {
      /* A move changes one position, so this is nearly sorted already. */
      for (size_t a = 1; a < g.size(); a++)
        for (size_t b = a; b > 0; b--) {
          const posTable_c & lo = t[g[b - 1]];
          const posTable_c & hi = t[g[b]];
          const key_t pl = (key >> lo.shift) & lo.bits;
          const key_t ph = (key >> hi.shift) & hi.bits;
          if (pl <= ph)
            break;
          key = (key & ~((key_t)lo.bits << lo.shift) & ~((key_t)hi.bits << hi.shift)) |
                (ph << lo.shift) | (pl << hi.shift);
        }
    }
    return key;
  };
  const key_t startCanon = canon(startKey);

  const unsigned long memoryLimit = search.maxMemoryStates
      ? search.maxMemoryStates
      : memoryStates(TABLE_STATE_KEYS * sizeof(key_t), search.highMemory);
  search.memoryStates = memoryLimit;

  /* Each arrangement and the one it was reached from; the start is its own. */
  parentMap_c<key_t> parent;
  parent.emplace(startCanon, startCanon);
  std::queue<key_t> q;
  q.push(startCanon);

  unsigned long visited = 0;
  key_t goalKey = 0;
  bool found = false;

  /* A new arrangement: remember it and queue it. True when it is the goal. */
  auto reach = [&](key_t key, key_t from) {
    if (!parent.emplace(key, from))
      return false;
    bool goal = true;
    for (unsigned int i = 0; i < n && goal; i++)
      goal = t[i].atGoal[(size_t)posOf(key, i)] != 0;
    if (goal) {
      goalKey = key;
      return true;
    }
    q.push(key);
    return false;
  };

  std::vector<uint64_t> occupied(words);
  std::vector<uint64_t> others(words);
  std::vector<int> pos(n);
  /* Positions this flood fill has reached: seen[i][p] == epoch. */
  std::vector<std::vector<unsigned int>> seen(n);
  for (unsigned int i = 0; i < n; i++)
    seen[i].assign(t[i].px.size(), 0);
  unsigned int epoch = 0;
  std::vector<int> fill;
  /* The same for the shifts of a nested group, (dx, dy) from -tray to +tray. */
  const int trayW = (int)tray.getX();
  const int trayH = (int)tray.getY();
  std::vector<unsigned int> shiftSeen(nested ? (size_t)(2 * trayW + 1) * (2 * trayH + 1) : 0, 0);
  unsigned int shiftEpoch = 0;
  std::vector<std::pair<int, int>> shiftQueue;

  /* Calls emit with every arrangement one move from cur, until emit returns
   * true; then it returns true itself. Same order as legacySearch: each
   * piece alone, then with what is nested in it; positions in the order a
   * breadth-first fill finds them. */
  auto expand = [&](const key_t cur, auto && emit) {
    std::fill(occupied.begin(), occupied.end(), 0);
    for (unsigned int i = 0; i < n; i++) {
      pos[i] = posOf(cur, i);
      const uint64_t * m = &t[i].mask[(size_t)pos[i] * words];
      for (size_t w = 0; w < words; w++)
        occupied[w] |= m[w];
    }

    for (unsigned int pi = 0; pi < n; pi++) {
      const posTable_c & tp = t[pi];
      const uint64_t * mine = &tp.mask[(size_t)pos[pi] * words];
      for (size_t w = 0; w < words; w++)
        others[w] = occupied[w] & ~mine[w];

      if (++epoch == 0) {
        for (auto & v : seen)
          std::fill(v.begin(), v.end(), 0);
        epoch = 1;
      }
      seen[pi][(size_t)pos[pi]] = epoch;
      fill.assign(1, pos[pi]);
      const key_t clear = cur & ~((key_t)tp.bits << tp.shift);
      for (size_t head = 0; head < fill.size(); head++)
        for (int d = 0; d < 4; d++) {
          int nb = tp.next[(size_t)fill[head]][d];
          if (nb < 0 || seen[pi][(size_t)nb] == epoch ||
              overlaps(&tp.mask[(size_t)nb * words], others))
            continue;
          seen[pi][(size_t)nb] = epoch;
          fill.push_back(nb);
          if (emit(clear | ((key_t)nb << tp.shift)))
            return true;
        }

      if (!nested)
        continue;
      /* pi and everything nested inside it (see nestedGroup): a piece is
       * nested in a member when its cells all lie in that member's outline. */
      unsigned int group[64];
      unsigned int members = 0;
      uint64_t inGroup = uint64_t(1) << pi;
      group[members++] = pi;
      for (unsigned int k = 0; k < members; k++) {
        const unsigned int a = group[k];
        const uint64_t * outer = &t[a].outline[(size_t)pos[a] * words];
        for (unsigned int b = 0; b < n; b++) {
          if ((inGroup >> b) & 1)
            continue;
          const uint64_t * inner = &t[b].mask[(size_t)pos[b] * words];
          bool all = true;
          for (size_t w = 0; w < words && all; w++)
            all = (inner[w] & ~outer[w]) == 0;
          if (all) {
            group[members++] = b;
            inGroup |= uint64_t(1) << b;
          }
        }
      }
      if (members < 2)
        continue;
      others = occupied;
      for (unsigned int g = 0; g < members; g++) {
        const unsigned int m = group[g];
        const uint64_t * gm = &t[m].mask[(size_t)pos[m] * words];
        for (size_t w = 0; w < words; w++)
          others[w] &= ~gm[w];
      }
      /* The group's key at shift (dx, dy), or false when it does not fit. */
      auto shifted = [&](int dx, int dy, key_t & key) {
        key = cur;
        for (unsigned int g = 0; g < members; g++) {
          const unsigned int m = group[g];
          int p = t[m].at(t[m].px[(size_t)pos[m]] + dx, t[m].py[(size_t)pos[m]] + dy);
          if (p < 0 || overlaps(&t[m].mask[(size_t)p * words], others))
            return false;
          key = (key & ~((key_t)t[m].bits << t[m].shift)) | ((key_t)p << t[m].shift);
        }
        return true;
      };
      /* Flood over the shifts the group can make, as for one piece. */
      if (++shiftEpoch == 0) {
        std::fill(shiftSeen.begin(), shiftSeen.end(), 0);
        shiftEpoch = 1;
      }
      auto seenAt = [&](int dx, int dy) -> unsigned int & {
        return shiftSeen[(size_t)((dy + trayH) * (2 * trayW + 1) + (dx + trayW))];
      };
      seenAt(0, 0) = shiftEpoch;
      shiftQueue.assign(1, {0, 0});
      for (size_t head = 0; head < shiftQueue.size(); head++) {
        const std::pair<int, int> c = shiftQueue[head];
        for (int d = 0; d < 4; d++) {
          const std::pair<int, int> nb{c.first + DX[d], c.second + DY[d]};
          /* A shift as wide as the tray fits nothing. */
          if (nb.first < -trayW || nb.first > trayW || nb.second < -trayH || nb.second > trayH)
            continue;
          key_t key;
          if (seenAt(nb.first, nb.second) == shiftEpoch || !shifted(nb.first, nb.second, key))
            continue;
          seenAt(nb.first, nb.second) = shiftEpoch;
          shiftQueue.push_back(nb);
          if (emit(key))
            return true;
        }
      }
    }
    return false;
  };

  while (!q.empty() && !found) {
    if (maxStates != FULL_SEARCH && visited >= maxStates) {
      search.outcome = SLIDE_LIMIT;
      break;
    }
    if (parent.size() >= memoryLimit) {
      search.outcome = SLIDE_MEMORY;
      break;
    }
    if ((visited & 1023) == 0) {
      if (search.progress)
        search.progress->store(visited, std::memory_order_relaxed);
      if (search.stop && search.stop->load(std::memory_order_relaxed)) {
        search.outcome = SLIDE_STOPPED;
        break;
      }
    }
    const key_t cur = q.front();
    q.pop();
    visited++;
    found = expand(cur, [&](key_t key) { return reach(canon(key), cur); });
  }

  search.visited = visited;
  if (!found)
    return true;

  /* The arrangements from goal back to start, as the search kept them. */
  std::vector<key_t> chain;
  for (key_t k = goalKey; ; k = parent.parentOf(k)) {
    chain.push_back(k);
    if (k == startCanon)
      break;
  }
  /* Forward again from the real start. Where copies were swapped about,
   * each step is the move from the arrangement at hand that gives the next
   * one kept, so every piece keeps its own identity along the path. */
  key_t real = startKey;
  path.push_back(decode(real));
  for (size_t s = chain.size() - 1; s-- > 0;) {
    key_t next = chain[s];
    if (!copies.empty()) {
      bt_assert2(expand(real, [&](key_t key) {
        if (canon(key) != chain[s])
          return false;
        next = key;
        return true;
      }));
    }
    real = next;
    path.push_back(decode(real));
  }
  return true;
}

} // namespace

std::unique_ptr<separation_c> findSlidePath(const problem_c & prob,
                                            const assembly_c & start,
                                            unsigned int maxStates,
                                            bool nested) {
  slideSearch_c search;
  search.maxStates = maxStates;
  search.nested = nested;
  return findSlidePath(prob, start, search);
}

std::unique_ptr<separation_c> findSlidePath(const problem_c & prob,
                                            const assembly_c & start,
                                            slideSearch_c & search) {
  const bool nested = search.nested;
  search.outcome = SLIDE_NO_PATH;
  search.visited = 0;
  if (!prob.resultValid())
    return nullptr;

  const voxel_c * tray = getResultShape(prob);
  unsigned int n = start.placementCount();
  if (n == 0)
    return nullptr;

  /* The pieces as placed at the start; owned here, viewed through pieces. */
  std::vector<std::unique_ptr<voxel_c>> owned(n);
  std::vector<const voxel_c *> pieces(n, nullptr);
  SlideState initial;
  initial.places.resize(n);

  unsigned int pc = 0;
  for (unsigned int part = 0; part < prob.getNumberOfParts(); part++) {
    for (unsigned int j = 0; j < prob.getPartMaximum(part); j++) {
      if (pc >= n || !start.isPlaced(pc))
        return nullptr;
      owned[pc].reset(prob.getPuzzle().getGridType()->getVoxel(*prob.getPartShape(part)));
      if (!owned[pc]->transform(start.getTransformation(pc)))
        return nullptr;
      pieces[pc] = owned[pc].get();
      initial.places[pc].x = start.getX(pc);
      initial.places[pc].y = start.getY(pc);
      initial.places[pc].trans = start.getTransformation(pc);
      initial.places[pc].z = start.getZ(pc);
      pc++;
    }
  }

  goalCheck_c goals(prob, pieces);
  if (goals.all(initial)) {
    search.outcome = SLIDE_FOUND;
    std::vector<unsigned int> pcs(n);
    for (unsigned int i = 0; i < n; i++)
      pcs[i] = i;
    auto sep = std::make_unique<separation_c>(nullptr, nullptr, pcs);
    auto st = std::make_unique<state_c>(n);
    for (unsigned int i = 0; i < n; i++)
      st->set(i, initial.places[i].x, initial.places[i].y, start.getZ(i),
              initial.places[i].trans);
    sep->addstate(std::move(st));
    return sep;
  }

  std::vector<cell3List_t> cells(n);
  std::vector<cell3List_t> outlines(n);
  for (unsigned int i = 0; i < n; i++) {
    cells[i] = cellsOf(*pieces[i]);
    if (nested)
      outlines[i] = outlineOf(*pieces[i]);
  }

  /* Start to goal, in order. Empty when no path was found. */
  std::vector<SlideState> path;
  bool searched = false;
  if (!std::getenv("BURRTOOLS_SLIDE_LEGACY")) {
    searched = tableSearch<uint64_t>(*tray, cells, outlines, nested, initial, goals, search, path);
#ifdef __SIZEOF_INT128__
    if (!searched)
      searched = tableSearch<wideKey_t>(*tray, cells, outlines, nested, initial, goals, search, path);
#endif
  }
  if (!searched)
    legacySearch(*tray, cells, outlines, nested, initial, goals, search, path);

  if (search.progress)
    search.progress->store(search.visited, std::memory_order_relaxed);
  if (path.empty())
    return nullptr;
  search.outcome = SLIDE_FOUND;

  std::vector<unsigned int> pcs(n);
  for (unsigned int i = 0; i < n; i++)
    pcs[i] = i;
  auto sep = std::make_unique<separation_c>(nullptr, nullptr, pcs);

  /* The move slider replaces each piece's position with these coordinates,
   * so they are the assembly placement, not a delta from it. */
  std::vector<int> placeZ(n);
  for (unsigned int i = 0; i < n; i++)
    placeZ[i] = start.getZ(i);

  for (int si = (int)path.size() - 1; si >= 0; si--) {
    auto st = std::make_unique<state_c>(n);
    for (unsigned int i = 0; i < n; i++)
      st->set(i, path[si].places[i].x, path[si].places[i].y, placeZ[i],
              path[si].places[i].trans);
    sep->addstate(std::move(st));
  }
  return sep;
}

std::vector<std::pair<int, int>> slideRoute(const problem_c & prob, const separation_c & path,
                                            unsigned int step, std::vector<unsigned int> * movers) {
  std::vector<std::pair<int, int>> none;
  if (movers)
    movers->clear();
  if (!prob.resultValid() || step >= path.getMoves())
    return none;
  const state_c * a = path.getState(step);
  const state_c * b = path.getState(step + 1);
  unsigned int n = path.getPieceNumber();

  /* The pieces that move all shift by the same amount: one piece, or an
   * outer piece and what is nested inside it. */
  std::vector<unsigned int> moving;
  int sx = 0;
  int sy = 0;
  for (unsigned int i = 0; i < n; i++) {
    int dx = b->getX(i) - a->getX(i);
    int dy = b->getY(i) - a->getY(i);
    if (dx == 0 && dy == 0)
      continue;
    if (a->getOrient(i) != b->getOrient(i))
      return none;
    if (!moving.empty() && (dx != sx || dy != sy))
      return none;
    sx = dx;
    sy = dy;
    moving.push_back(i);
  }
  if (moving.empty())
    return none;

  /* Pieces in path order are the problem's parts, copy by copy. */
  std::vector<cell3List_t> cells;
  SlideState st;
  unsigned int pc = 0;
  for (unsigned int part = 0; part < prob.getNumberOfParts() && pc < n; part++)
    for (unsigned int j = 0; j < prob.getPartMaximum(part) && pc < n; j++, pc++) {
      std::unique_ptr<voxel_c> v(prob.getPuzzle().getGridType()->getVoxel(*prob.getPartShape(part)));
      v->transform(a->getOrient(pc));
      cells.push_back(cellsOf(*v));
      st.places.push_back(Place{a->getX(pc), a->getY(pc), (unsigned char)a->getOrient(pc),
                                a->getZ(pc)});
    }
  if (pc != n)
    return none;
  if (movers)
    *movers = moving;
  return fewestTurns(*getResultShape(prob), cells, st, moving, sx, sy);
}

void applySlideRoutes(const problem_c & prob, const separation_c & path, disasmToMoves_c & anim) {
  for (unsigned int step = 0; step < path.getMoves(); step++) {
    std::vector<unsigned int> movers;
    std::vector<std::pair<int, int>> route = slideRoute(prob, path, step, &movers);
    /* A straight run animates correctly without help. */
    if (route.size() < 3)
      continue;
    std::vector<std::pair<float, float>> offsets;
    for (const auto & c : route)
      offsets.push_back({(float)c.first, (float)c.second});
    anim.setRoute(step, movers, offsets);
  }
}

std::string finalPlacementKey(const separation_c & path) {
  const state_c * st = path.getState(path.getMoves());
  std::ostringstream os;
  for (unsigned int i = 0; i < path.getPieceNumber(); i++) {
    if (i)
      os << '|';
    os << st->getX(i) << ',' << st->getY(i) << ',' << st->getZ(i)
       << ',' << st->getOrient(i);
  }
  return os.str();
}

std::string stampOverflowMessage(const problem_c & prob) {
  if (!prob.resultValid())
    return std::string();
  const voxel_c * tray = getResultShape(prob);
  if (!tray || !isStartGoalShape(tray))
    return std::string();

  const puzzle_c & puz = prob.getPuzzle();
  std::ostringstream msg;
  for (unsigned int s = 0; s < puz.getNumberOfShapes(); s++) {
    const voxel_c * piece = puz.getShape(s);
    if (isStartGoalShape(piece) || isHiddenSlidingShape(piece))
      continue;
    unsigned int voxels = 0;
    for (unsigned int i = 0; i < piece->getXYZ(); i++)
      if (piece->getState(i) != voxel_c::VX_EMPTY)
        voxels++;
    unsigned int id = s + 1;
    unsigned int starts = 0;
    unsigned int goals = 0;
    for (unsigned int i = 0; i < tray->getXYZ(); i++) {
      if (tray->getColor(i) == id)
        starts++;
      if (tray->getGoalPiece(i) == id)
        goals++;
    }
    if (starts > voxels)
      msg << "S" << id << " has " << starts
          << " start cells but the piece only has " << voxels
          << (voxels == 1 ? " voxel.\n" : " voxels.\n");
    if (goals > voxels)
      msg << "S" << id << " has " << goals
          << " goal cells but the piece only has " << voxels
          << (voxels == 1 ? " voxel.\n" : " voxels.\n");
  }
  if (msg.str().empty())
    return std::string();
  msg << "A piece cannot cover more cells than it contains.";
  return msg.str();
}

} // namespace sliding
