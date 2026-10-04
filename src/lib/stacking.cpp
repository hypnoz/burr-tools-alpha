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
#include "stacking.h"

#include "assembly.h"
#include "bt_assert.h"
#include "disassembly.h"
#include "gridtype.h"
#include "problem.h"
#include "puzzle.h"
#include "sliding.h"
#include "voxel.h"

#include "../tools/xml.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <map>
#include <queue>
#include <sstream>
#include <string>
#include <unordered_map>

namespace stacking {

namespace {

unsigned int diskSizeOf(const voxel_c * v) {
  unsigned int s = v->getDiskSize();
  return s == 0 ? 1 : s;
}

struct piece_c {
  unsigned int shapeId;
  unsigned int instance;
  unsigned int size;
};

std::vector<piece_c> piecesOf(const problem_c & prob) {
  std::vector<piece_c> out;
  for (unsigned int p = 0; p < prob.getNumberOfParts(); p++) {
    unsigned int n = prob.getPartMaximum(p);
    unsigned int sz = diskSizeOf(prob.getPartShape(p));
    unsigned int sid = prob.getShapeIdOfPart(p);
    for (unsigned int i = 0; i < n; i++)
      out.push_back(piece_c{sid, i, sz});
  }
  return out;
}

int findPiece(const std::vector<piece_c> & pieces, const diskRef_c & ref) {
  for (unsigned int i = 0; i < pieces.size(); i++)
    if (pieces[i].shapeId == ref.shapeId && pieces[i].instance == ref.instance)
      return (int)i;
  return -1;
}

const stackMap_c & mapOf(const problem_c & prob, bool goal) {
  return goal ? prob.goalStacks() : prob.startStacks();
}

stackMap_c & mapOf(problem_c & prob, bool goal) {
  return goal ? prob.editGoal() : prob.editStart();
}

bool refOnMap(const stackMap_c & map, const diskRef_c & ref, unsigned int * rodOut) {
  for (unsigned int r = 0; r < map.rods.size(); r++)
    for (const diskRef_c & d : map.rods[r])
      if (d == ref) {
        if (rodOut)
          *rodOut = r;
        return true;
      }
  return false;
}

unsigned int capacityOf(const rodSet_c & board, unsigned int pieceCount) {
  if (board.growHeight)
    return pieceCount == 0 ? 1 : pieceCount;
  return board.definedHeight;
}

/* How deep a disc may go in a Panex column: its size, but no deeper than
 * the column. */
unsigned int depthOf(const rodSet_c & board, const piece_c & p, unsigned int pieceCount) {
  return std::min(p.size, capacityOf(board, pieceCount));
}

/* A Panex column, bottom to top, as depths. The k-th disc from the top sits
 * at least k - 1 places down (k - 1 == 0 is the raised place in the
 * bridge), so it needs a depth of k - 1 or more. The index of the first
 * disc that cannot go that deep, or -1. */
int panexTooDeep(const std::vector<unsigned int> & depth) {
  const size_t n = depth.size();
  for (size_t i = 0; i < n; i++)
    if (depth[i] + 1 < n - i)
      return (int)i;
  return -1;
}

/* The column cannot keep every disc below the bridge: its top disc is
 * raised into it, and blocks moves that pass over the column. */
bool panexRaised(const std::vector<unsigned int> & depth) {
  const size_t n = depth.size();
  for (size_t i = 0; i < n; i++)
    if (depth[i] < n - i)
      return true;
  return false;
}

/* Height above the base of each disc on one rod, bottom to top. Discs
 * stack from the base, except in a Panex column, where they hang from the
 * bridge as high as they go: the top one in the bridge when it is raised. */
std::vector<int> discHeights(const rodSet_c & board, const std::vector<piece_c> & pieces,
                             const std::vector<unsigned int> & column, unsigned int rod) {
  std::vector<int> z(column.size());
  const unsigned int n = (unsigned int)pieces.size();
  if (!board.panexColumns || isPocket(board, rod)) {
    for (size_t h = 0; h < column.size(); h++)
      z[h] = (int)h;
    return z;
  }
  std::vector<unsigned int> depth;
  for (unsigned int d : column)
    depth.push_back(depthOf(board, pieces[d], n));
  const int top = (int)capacityOf(board, n) - (panexRaised(depth) ? 0 : 1);
  for (size_t h = 0; h < column.size(); h++)
    z[h] = top - (int)(column.size() - 1 - h);
  return z;
}

int spacingOf(const std::vector<piece_c> & pieces) {
  unsigned int maxR = 1;
  for (const piece_c & p : pieces)
    if (p.size > maxR)
      maxR = p.size;
  return (int)(2 * maxR + 1);
}

std::string diskLabel(const problem_c & prob, const diskRef_c & ref) {
  std::string out = "S" + std::to_string(ref.shapeId + 1);
  if (ref.shapeId < prob.getPuzzle().getNumberOfShapes()) {
    const std::string & name = prob.getPuzzle().getShape(ref.shapeId)->getName();
    if (!name.empty())
      out += " (" + name + ")";
  }
  if (prob.getShapeMaximum(ref.shapeId) > 1)
    out += " copy " + std::to_string(ref.instance + 1);
  return out;
}

std::string checkStack(const problem_c & prob, const rodSet_c & board,
                       const std::vector<piece_c> & pieces,
                       const std::vector<diskRef_c> & rod, unsigned int rodNo,
                       const char * which) {
  const unsigned int n = (unsigned int)pieces.size();
  unsigned int cap = rodCapacity(board, rodNo, n);
  if (rod.size() > cap) {
    std::string name = rodName(board, rodNo);
    name[0] = (char)std::toupper((unsigned char)name[0]);
    return name + " of " + which + " holds " + std::to_string(rod.size()) +
           " discs, but its height only allows " + std::to_string(cap) + ".";
  }
  if (isPocket(board, rodNo))
    return "";

  if (board.panexColumns) {
    std::vector<unsigned int> depth;
    for (const diskRef_c & d : rod) {
      int idx = findPiece(pieces, d);
      depth.push_back(idx < 0 ? capacityOf(board, n) : depthOf(board, pieces[(size_t)idx], n));
    }
    int bad = panexTooDeep(depth);
    if (bad < 0)
      return "";
    const unsigned int fromTop = (unsigned int)(rod.size() - (size_t)bad);
    const unsigned int size = depth[(size_t)bad];
    return "On " + rodName(board, rodNo) + " of " + which + ", " + diskLabel(prob, rod[(size_t)bad]) +
           " is disc " + std::to_string(fromTop) + " from the top, but in a Panex column a size " +
           std::to_string(size) + " disc can only be among the top " + std::to_string(size + 1) + ".";
  }

  if (!board.sizeMatters)
    return "";

  for (unsigned int i = 1; i < rod.size(); i++) {
    int below = findPiece(pieces, rod[i - 1]);
    int above = findPiece(pieces, rod[i]);
    if (below < 0 || above < 0)
      continue;
    if (pieces[above].size > pieces[below].size)
      return "On " + rodName(board, rodNo) + " of " + which + ", " +
             diskLabel(prob, rod[i]) + " (size " + std::to_string(pieces[above].size) +
             ") sits on the smaller " + diskLabel(prob, rod[i - 1]) + " (size " +
             std::to_string(pieces[below].size) + "), but size matters is on.";
  }
  return "";
}

using Config = std::vector<std::vector<unsigned int>>;

bool configsEqual(const Config & a, const Config & b) {
  return a == b;
}

bool moveAllowed(const rodSet_c & board, const std::vector<piece_c> & pieces,
                 const Config & cfg, unsigned int from, unsigned int to) {
  if (from == to || cfg[from].empty())
    return false;
  const unsigned int n = (unsigned int)pieces.size();
  if (cfg[to].size() >= rodCapacity(board, to, n))
    return false;

  unsigned int mover = cfg[from].back();
  const int a = rodPosition(board, from);
  const int b = rodPosition(board, to);
  if (board.panexColumns) {
    /* A disc raised into the bridge blocks every move that passes over it. */
    const int lo = std::min(a, b);
    const int hi = std::max(a, b);
    std::vector<unsigned int> depth;
    for (unsigned int r = 0; r < cfg.size(); r++) {
      const int p = rodPosition(board, r);
      if (r == from || r == to || p <= lo || p >= hi || isPocket(board, r))
        continue;
      depth.clear();
      for (unsigned int d : cfg[r])
        depth.push_back(depthOf(board, pieces[d], n));
      if (panexRaised(depth))
        return false;
    }
    if (!isPocket(board, to)) {
      depth.clear();
      for (unsigned int d : cfg[to])
        depth.push_back(depthOf(board, pieces[d], n));
      depth.push_back(depthOf(board, pieces[mover], n));
      if (panexTooDeep(depth) >= 0)
        return false;
    }
  } else if (board.sizeMatters && !cfg[to].empty()) {
    unsigned int top = cfg[to].back();
    if (pieces[mover].size > pieces[top].size)
      return false;
  }

  int gap = a - b;
  if (gap < 0)
    gap = -gap;
  if (board.distanceMatters && !board.canMoveOver && gap != 1)
    return false;
  return true;
}

Config configFromMap(const problem_c & prob, bool goal, const std::vector<piece_c> & pieces,
                     bool * ok) {
  const rodSet_c & board = prob.getPuzzle().getRodSet(prob.getRodSetId());
  Config cfg(totalRods(board));
  const stackMap_c & map = mapOf(prob, goal);
  *ok = true;
  if (map.rods.size() != totalRods(board)) {
    *ok = false;
    return cfg;
  }
  for (unsigned int r = 0; r < totalRods(board); r++) {
    for (const diskRef_c & ref : map.rods[r]) {
      int idx = findPiece(pieces, ref);
      if (idx < 0) {
        *ok = false;
        return cfg;
      }
      cfg[r].push_back((unsigned int)idx);
    }
  }
  return cfg;
}

/* An attribute as a count from 0 to most; fallback when missing or not a
 * number in range (an edited file can hold anything). */
/* Rods, or discs a rod holds, that a file may ask for. */
const unsigned int MAX_FILE_COUNT = 1000;

unsigned int countAttribute(xmlParser_c & pars, const char * name, unsigned int fallback,
                            unsigned int most) {
  const std::string s = pars.getAttributeValue(name);
  if (s.empty())
    return fallback;
  char * end = nullptr;
  const long v = std::strtol(s.c_str(), &end, 10);
  if (end == s.c_str() || v < 0 || (unsigned long)v > most)
    return fallback;
  return (unsigned int)v;
}

/* What the animation needs to place a disc: the board, its discs and where each rod stands. */
struct drawBoard_c {
  drawBoard_c(const rodSet_c & b, const std::vector<piece_c> & p, const std::vector<int> & x)
      : board(b), pieces(p), rodX(x) {}
  const rodSet_c & board;
  const std::vector<piece_c> & pieces;
  const std::vector<int> & rodX;
};

void writeState(state_c * st, const Config & cfg, const drawBoard_c & draw) {
  for (unsigned int r = 0; r < cfg.size(); r++) {
    std::vector<int> z = discHeights(draw.board, draw.pieces, cfg[r], r);
    for (unsigned int h = 0; h < cfg[r].size(); h++) {
      unsigned int piece = cfg[r][h];
      st->set(piece, draw.rodX[r], 0, z[h], 0);
    }
  }
}

std::unique_ptr<state_c> stateFor(const Config & before, const Config * after,
                                  int frame, const drawBoard_c & draw, unsigned int n,
                                  int liftZ) {
  auto st = std::make_unique<state_c>(n);
  if (!after || frame == 0) {
    writeState(st.get(), before, draw);
    return st;
  }

  unsigned int mover = 0;
  unsigned int src = 0;
  unsigned int dst = 0;
  bool found = false;
  for (unsigned int r = 0; r < before.size() && !found; r++) {
    for (unsigned int piece : before[r]) {
      bool still = false;
      for (unsigned int q : (*after)[r])
        if (q == piece)
          still = true;
      if (!still) {
        mover = piece;
        src = r;
        found = true;
        break;
      }
    }
  }
  for (unsigned int r = 0; r < after->size(); r++)
    for (unsigned int piece : (*after)[r])
      if (piece == mover)
        dst = r;

  Config shown = before;
  shown[src].pop_back();
  writeState(st.get(), shown, draw);

  /* Cruise in the first cell above the drawn peg so the disc clears every rod. */
  int z = liftZ;
  if (z < 1)
    z = 1;
  if (frame == 1)
    st->set(mover, draw.rodX[src], 0, z, 0);
  else if (frame == 2)
    st->set(mover, draw.rodX[dst], 0, z, 0);
  else
    writeState(st.get(), *after, draw);
  return st;
}

std::string encodeRod(const std::vector<diskRef_c> & rod) {
  std::string s;
  for (unsigned int i = 0; i < rod.size(); i++) {
    if (i)
      s += ',';
    s += std::to_string(rod[i].shapeId);
    s += ':';
    s += std::to_string(rod[i].instance);
  }
  return s;
}

std::vector<diskRef_c> decodeRod(const std::string & s) {
  std::vector<diskRef_c> rod;
  if (s.empty())
    return rod;
  std::stringstream in(s);
  while (in.good()) {
    unsigned int shape = 0;
    unsigned int inst = 0;
    char colon = 0;
    in >> shape >> colon >> inst;
    if (!in.fail() && colon == ':')
      rod.push_back(diskRef_c{shape, inst});
    if (in.peek() == ',')
      in.get();
  }
  return rod;
}

} // namespace

bool isStacking(const puzzle_c & puz) {
  return puz.getGridType()->getType() == gridType_c::GT_STACKING;
}

bool isStacking(const problem_c & prob) {
  return isStacking(prob.getPuzzle());
}

unsigned int totalRods(const rodSet_c & board) {
  return board.rodCount + ((board.panexColumns && board.pocketColumn) ? 1 : 0);
}

bool isPocket(const rodSet_c & board, unsigned int rod) {
  return board.panexColumns && board.pocketColumn && rod == board.rodCount;
}

int rodPosition(const rodSet_c & board, unsigned int rod) {
  return isPocket(board, rod) ? -1 : (int)rod;
}

std::string rodName(const rodSet_c & board, unsigned int rod) {
  return isPocket(board, rod) ? "the pocket column" : "rod " + std::to_string(rod + 1);
}

unsigned int rodHeight(const rodSet_c & board, unsigned int discCount) {
  return capacityOf(board, discCount);
}

unsigned int rodCapacity(const rodSet_c & board, unsigned int rod, unsigned int discCount) {
  if (isPocket(board, rod))
    return board.pocketHeight;
  unsigned int cap = capacityOf(board, discCount);
  return board.panexColumns ? cap + 1 : cap;
}

void generateDisk(voxel_c * v, unsigned int size) {
  if (!v)
    return;
  if (size < 1)
    size = 1;

  unsigned int color = 0;
  for (unsigned int i = 0; i < v->getXYZ(); i++) {
    if (v->getState(i) != voxel_c::VX_EMPTY) {
      color = v->getColor(i);
      break;
    }
  }

  unsigned int d = 2 * size + 1;
  v->resize(d, d, 1, voxel_c::VX_EMPTY);
  for (unsigned int y = 0; y < d; y++) {
    for (unsigned int x = 0; x < d; x++) {
      if (x == size && y == size) {
        v->set(x, y, 0, voxel_c::VX_EMPTY);
        continue;
      }
      v->set(x, y, 0, voxel_c::VX_FILLED);
      if (color)
        v->setColor(x, y, 0, color);
    }
  }
  v->setHotspot((int)size, (int)size, 0);
  v->setDiskSize(size);
}

void ensureHotspot(voxel_c * v) {
  if (!v || v->getDiskSize() == 0)
    return;
  int r = (int)v->getDiskSize();
  v->setHotspot(r, r, 0);
}

unsigned int addDisk(puzzle_c & puz, unsigned int size) {
  if (size < 1)
    size = 1;
  unsigned int d = 2 * size + 1;
  unsigned int idx = puz.addShape(d, d, 1);
  generateDisk(puz.getShape(idx), size);
  return idx;
}

void saveRodSets(const puzzle_c & puz, xmlWriter_c & xml) {
  if (!isStacking(puz) && puz.rodSetCount() == 0)
    return;
  xml.newTag("rodSets");
  for (unsigned int i = 0; i < puz.rodSetCount(); i++) {
    const rodSet_c & r = puz.getRodSet(i);
    xml.newTag("rodSet");
    if (!r.name.empty())
      xml.newAttrib("name", r.name);
    xml.newAttrib("rods", r.rodCount);
    xml.newAttrib("grow", r.growHeight ? 1u : 0u);
    xml.newAttrib("height", r.definedHeight);
    xml.newAttrib("sizeMatters", r.sizeMatters ? 1u : 0u);
    xml.newAttrib("distanceMatters", r.distanceMatters ? 1u : 0u);
    xml.newAttrib("moveOver", r.canMoveOver ? 1u : 0u);
    if (r.panexColumns)
      xml.newAttrib("panex", 1u);
    if (r.pocketColumn)
      xml.newAttrib("pocket", r.pocketHeight);
    xml.endTag("rodSet");
  }
  xml.endTag("rodSets");
}

void loadRodSets(puzzle_c & puz, xmlParser_c & pars) {
  do {
    int state = pars.nextTag();
    if (state == xmlParser_c::END_TAG)
      break;
    pars.require(xmlParser_c::START_TAG, "");
    if (pars.getName() == "rodSet") {
      rodSet_c r;
      r.name = pars.getAttributeValue("name");
      r.rodCount = std::max(1u, countAttribute(pars, "rods", r.rodCount, MAX_FILE_COUNT));
      r.growHeight = countAttribute(pars, "grow", r.growHeight, 1) != 0;
      r.definedHeight = std::max(1u, countAttribute(pars, "height", r.definedHeight, MAX_FILE_COUNT));
      r.sizeMatters = countAttribute(pars, "sizeMatters", r.sizeMatters, 1) != 0;
      r.distanceMatters = countAttribute(pars, "distanceMatters", r.distanceMatters, 1) != 0;
      r.canMoveOver = countAttribute(pars, "moveOver", r.canMoveOver, 1) != 0;
      r.panexColumns = countAttribute(pars, "panex", r.panexColumns, 1) != 0;
      const unsigned int pocket = countAttribute(pars, "pocket", 0, MAX_FILE_COUNT);
      if (pocket > 0) {
        r.pocketColumn = true;
        r.pocketHeight = pocket;
      }
      puz.addRodSet(r);
      pars.skipSubTree();
    } else {
      pars.skipSubTree();
    }
  } while (true);
  pars.require(xmlParser_c::END_TAG, "rodSets");
}

static void saveMap(xmlWriter_c & xml, const char * tag, const stackMap_c & map) {
  xml.newTag(tag);
  for (const auto & rod : map.rods) {
    xml.newTag("rod");
    std::string body = encodeRod(rod);
    if (!body.empty())
      xml.newAttrib("pieces", body);
    xml.endTag("rod");
  }
  xml.endTag(tag);
}

static stackMap_c loadMap(xmlParser_c & pars, const char * tag) {
  stackMap_c map;
  do {
    int state = pars.nextTag();
    if (state == xmlParser_c::END_TAG)
      break;
    pars.require(xmlParser_c::START_TAG, "");
    if (pars.getName() == "rod") {
      map.rods.push_back(decodeRod(pars.getAttributeValue("pieces")));
      pars.skipSubTree();
    } else {
      pars.skipSubTree();
    }
  } while (true);
  pars.require(xmlParser_c::END_TAG, tag);
  return map;
}

void saveProblem(const problem_c & prob, xmlWriter_c & xml) {
  if (!prob.rodSetValid() && prob.startStacks().rods.empty() && prob.goalStacks().rods.empty())
    return;
  xml.newTag("stacking");
  if (prob.rodSetValid())
    xml.newAttrib("rodSet", prob.getRodSetId());
  saveMap(xml, "start", prob.startStacks());
  saveMap(xml, "goal", prob.goalStacks());
  xml.endTag("stacking");
}

void loadProblem(problem_c & prob, xmlParser_c & pars) {
  std::string id = pars.getAttributeValue("rodSet");
  if (!id.empty())
    prob.setRodSetId((unsigned int)atoi(id.c_str()));
  do {
    int state = pars.nextTag();
    if (state == xmlParser_c::END_TAG)
      break;
    pars.require(xmlParser_c::START_TAG, "");
    if (pars.getName() == "start")
      prob.editStart() = loadMap(pars, "start");
    else if (pars.getName() == "goal")
      prob.editGoal() = loadMap(pars, "goal");
    else
      pars.skipSubTree();
  } while (true);
  pars.require(xmlParser_c::END_TAG, "stacking");
}

static void retarget(stackMap_c & map, unsigned int removed) {
  for (auto & rod : map.rods) {
    rod.erase(std::remove_if(rod.begin(), rod.end(),
                             [removed](const diskRef_c & d) { return d.shapeId == removed; }),
              rod.end());
    for (diskRef_c & d : rod)
      if (d.shapeId > removed)
        d.shapeId--;
  }
}

void noteShapeRemoved(problem_c & prob, unsigned int shapeId) {
  retarget(prob.editStart(), shapeId);
  retarget(prob.editGoal(), shapeId);
}

static void swapIds(stackMap_c & map, unsigned int a, unsigned int b) {
  for (auto & rod : map.rods)
    for (diskRef_c & d : rod) {
      if (d.shapeId == a)
        d.shapeId = b;
      else if (d.shapeId == b)
        d.shapeId = a;
    }
}

void noteShapeSwap(problem_c & prob, unsigned int a, unsigned int b) {
  swapIds(prob.editStart(), a, b);
  swapIds(prob.editGoal(), a, b);
}

void trimStacks(problem_c & prob) {
  auto trim = [&prob](stackMap_c & map) {
    for (auto & rod : map.rods) {
      rod.erase(std::remove_if(rod.begin(), rod.end(),
                               [&prob](const diskRef_c & d) {
                                 if (d.shapeId >= prob.getPuzzle().getNumberOfShapes())
                                   return true;
                                 return d.instance >= prob.getShapeMaximum(d.shapeId);
                               }),
                rod.end());
    }
  };
  trim(prob.editStart());
  trim(prob.editGoal());
}

void syncMaps(problem_c & prob) {
  if (!prob.rodSetValid())
    return;
  unsigned int n = totalRods(prob.getPuzzle().getRodSet(prob.getRodSetId()));
  /* Rods past the count keep their discs, so lowering the rod count and
   * raising it again gives them back. setupError reports them meanwhile. */
  auto fit = [n](stackMap_c & map) {
    if (map.rods.size() < n)
      map.rods.resize(n);
    while (map.rods.size() > n && map.rods.back().empty())
      map.rods.pop_back();
  };
  fit(prob.editStart());
  fit(prob.editGoal());
}

std::string setupError(const problem_c & prob) {
  if (!isStacking(prob))
    return "This problem is not a stacking puzzle.";
  if (!prob.rodSetValid())
    return "Choose a rod set with Set Start/Goal Rods.";
  const rodSet_c & board = prob.getPuzzle().getRodSet(prob.getRodSetId());
  if (board.rodCount < 1)
    return "The rod set needs at least one rod.";

  std::vector<piece_c> pieces = piecesOf(prob);
  if (pieces.empty())
    return "Add the discs that move on the rods.";

  /* Rule breaks come first: they are what an Entities tab edit causes. */
  auto checkRules = [&](bool goal, const char * which) -> std::string {
    const stackMap_c & map = mapOf(prob, goal);
    for (unsigned int r = totalRods(board); r < map.rods.size(); r++)
      if (!map.rods[r].empty())
        return std::string("The ") + which + " has discs on rod " + std::to_string(r + 1) +
               ", but the rod set only has " + std::to_string(board.rodCount) +
               (board.rodCount == 1 ? " rod." : " rods.");
    for (unsigned int r = 0; r < map.rods.size() && r < totalRods(board); r++) {
      std::string bad = checkStack(prob, board, pieces, map.rods[r], r,
                                   goal ? "the goal" : "the start");
      if (!bad.empty())
        return bad;
    }
    return "";
  };

  std::string err = checkRules(false, "start");
  if (err.empty())
    err = checkRules(true, "goal");
  if (!err.empty())
    return err;

  for (unsigned int p = 0; p < prob.getNumberOfParts(); p++) {
    if (prob.getPartMinimum(p) != prob.getPartMaximum(p))
      return "S" + std::to_string(prob.getShapeIdOfPart(p) + 1) +
             " has a min/max range, but stacking uses a fixed count of each disc.";
  }

  /* Start and goal must use the same discs. Compare whole sets, so the
   * message can name every disc that is on one side only. */
  auto countsOf = [&](bool goal, std::string * bad) {
    std::vector<int> seen(pieces.size(), 0);
    const stackMap_c & map = mapOf(prob, goal);
    for (const auto & rod : map.rods)
      for (const diskRef_c & ref : rod) {
        int idx = findPiece(pieces, ref);
        if (idx < 0) {
          if (bad->empty())
            *bad = std::string("The ") + (goal ? "goal" : "start") +
                   " names a disc that is not in the problem.";
          continue;
        }
        seen[idx]++;
      }
    return seen;
  };
  for (bool goal : {false, true})
    if (mapOf(prob, goal).rods.size() < totalRods(board))
      return std::string("The ") + (goal ? "goal" : "start") +
             " does not match the number of rods.";
  std::vector<int> onStart = countsOf(false, &err);
  std::vector<int> onGoal = countsOf(true, &err);
  if (!err.empty())
    return err;

  for (unsigned int i = 0; i < pieces.size(); i++) {
    diskRef_c ref{pieces[i].shapeId, pieces[i].instance};
    if (onStart[i] > 1)
      return diskLabel(prob, ref) + " is on the start more than once.";
    if (onGoal[i] > 1)
      return diskLabel(prob, ref) + " is on the goal more than once.";
  }

  /* Count by shape: copies of one shape are interchangeable to the user. */
  std::map<unsigned int, int> startShapes, goalShapes;
  for (unsigned int i = 0; i < pieces.size(); i++) {
    startShapes[pieces[i].shapeId] += onStart[i];
    goalShapes[pieces[i].shapeId] += onGoal[i];
  }
  auto listMissing = [&](const std::map<unsigned int, int> & have,
                         const std::map<unsigned int, int> & want) {
    std::vector<std::string> names;
    for (const auto & w : want) {
      int lack = w.second - have.at(w.first);
      if (lack <= 0)
        continue;
      std::string n = diskLabel(prob, diskRef_c{w.first, 0});
      /* diskLabel names copy 1 when there are several; here it is a count. */
      std::string::size_type copy = n.find(" copy ");
      if (copy != std::string::npos)
        n.erase(copy);
      if (lack > 1)
        n += " x" + std::to_string(lack);
      names.push_back(n);
    }
    std::string out;
    for (unsigned int i = 0; i < names.size(); i++) {
      if (i)
        out += (i + 1 == names.size()) ? " and " : ", ";
      out += names[i];
    }
    return out;
  };
  std::string goalLacks = listMissing(goalShapes, startShapes);
  std::string startLacks = listMissing(startShapes, goalShapes);
  if (!goalLacks.empty() || !startLacks.empty()) {
    std::string msg = "The start and goal use different discs:";
    if (!goalLacks.empty())
      msg += " the goal is missing " + goalLacks;
    if (!goalLacks.empty() && !startLacks.empty())
      msg += ";";
    if (!startLacks.empty())
      msg += " the start is missing " + startLacks;
    return msg + ".";
  }

  for (unsigned int i = 0; i < pieces.size(); i++) {
    diskRef_c ref{pieces[i].shapeId, pieces[i].instance};
    if (onStart[i] == 0 && onGoal[i] == 0)
      return diskLabel(prob, ref) + " is in the problem but on neither the start nor the goal.";
    /* Same count per shape but different copies: only an edited file does this. */
    if (onStart[i] != onGoal[i])
      return diskLabel(prob, ref) + " is on the " + (onStart[i] ? "start" : "goal") +
             " but not on the " + (onStart[i] ? "goal" : "start") + ".";
  }
  return "";
}

std::string placeDisk(problem_c & prob, bool goal, unsigned int shapeId, unsigned int rod,
                      bool enforceRules) {
  if (!prob.rodSetValid())
    return "Set a rod set as the result";
  syncMaps(prob);
  const rodSet_c & board = prob.getPuzzle().getRodSet(prob.getRodSetId());
  stackMap_c & map = mapOf(prob, goal);
  if (rod >= map.rods.size())
    return "That rod is not on this board";

  unsigned int count = prob.getShapeMaximum(shapeId);
  if (count == 0)
    return "Add the disc to the problem first";

  int instance = -1;
  for (unsigned int i = 0; i < count; i++) {
    if (!refOnMap(map, diskRef_c{shapeId, i}, nullptr)) {
      instance = (int)i;
      break;
    }
  }
  if (instance < 0)
    return "Every copy of that disc is already on a rod";

  if (!enforceRules) {
    map.rods[rod].push_back(diskRef_c{shapeId, (unsigned int)instance});
    return "";
  }

  std::vector<piece_c> pieces = piecesOf(prob);
  const unsigned int n = (unsigned int)pieces.size();
  if (map.rods[rod].size() >= rodCapacity(board, rod, n))
    return "That rod is full";

  if (board.panexColumns) {
    if (isPocket(board, rod)) {
      map.rods[rod].push_back(diskRef_c{shapeId, (unsigned int)instance});
      return "";
    }
    std::vector<unsigned int> depth;
    for (const diskRef_c & d : map.rods[rod]) {
      int idx = findPiece(pieces, d);
      depth.push_back(idx < 0 ? capacityOf(board, n) : depthOf(board, pieces[(size_t)idx], n));
    }
    int mover = findPiece(pieces, diskRef_c{shapeId, (unsigned int)instance});
    depth.push_back(mover < 0 ? capacityOf(board, n) : depthOf(board, pieces[(size_t)mover], n));
    if (panexTooDeep(depth) >= 0)
      return "A disc on that rod would be pushed deeper than its size allows";
  } else if (board.sizeMatters && !map.rods[rod].empty()) {
    int top = findPiece(pieces, map.rods[rod].back());
    int mover = findPiece(pieces, diskRef_c{shapeId, (unsigned int)instance});
    if (top >= 0 && mover >= 0 && pieces[mover].size > pieces[top].size)
      return "A larger disc cannot sit on a smaller one";
  }

  map.rods[rod].push_back(diskRef_c{shapeId, (unsigned int)instance});
  return "";
}

static void eraseAndRenumber(stackMap_c & map, unsigned int shapeId, unsigned int instance) {
  for (auto & rod : map.rods) {
    for (auto it = rod.begin(); it != rod.end(); ) {
      if (it->shapeId == shapeId && it->instance == instance)
        it = rod.erase(it);
      else {
        if (it->shapeId == shapeId && it->instance > instance)
          it->instance--;
        ++it;
      }
    }
  }
}

void dropInstance(problem_c & prob, unsigned int shapeId, unsigned int instance) {
  eraseAndRenumber(prob.editStart(), shapeId, instance);
  eraseAndRenumber(prob.editGoal(), shapeId, instance);
}

bool moveDisk(problem_c & prob, bool goal, unsigned int rod, unsigned int index, int delta) {
  syncMaps(prob);
  stackMap_c & map = mapOf(prob, goal);
  if (rod >= map.rods.size())
    return false;
  std::vector<diskRef_c> & stack = map.rods[rod];
  if (index >= stack.size())
    return false;
  int next = (int)index + delta;
  if (next < 0 || next >= (int)stack.size())
    return false;
  std::swap(stack[index], stack[(unsigned int)next]);
  return true;
}

std::string liftTop(problem_c & prob, bool goal, unsigned int rod) {
  syncMaps(prob);
  stackMap_c & map = mapOf(prob, goal);
  if (rod >= map.rods.size())
    return "That rod is not on this board";
  if (map.rods[rod].empty())
    return "That rod is empty";
  map.rods[rod].pop_back();
  return "";
}

/* Height used when drawing pegs. Grow has no fixed length, so an empty
 * board is still five voxels tall. The solver capacity is unchanged. */
unsigned int drawnPegHeight(const rodSet_c & board, unsigned int pieceCount) {
  unsigned int h = capacityOf(board, pieceCount);
  if (board.growHeight && h < 5)
    h = 5;
  if (h < 1)
    h = 1;
  return h;
}

boardLayout_c layoutBoard(const problem_c & prob, bool goal) {
  boardLayout_c lay;
  if (!prob.rodSetValid())
    return lay;
  const rodSet_c & board = prob.getPuzzle().getRodSet(prob.getRodSetId());
  std::vector<piece_c> pieces = piecesOf(prob);
  lay.spacing = spacingOf(pieces);
  lay.rodCount = totalRods(board);
  lay.rodHeight = drawnPegHeight(board, (unsigned int)pieces.size());
  lay.panex = board.panexColumns;
  lay.pocket = board.panexColumns && board.pocketColumn;
  for (const piece_c & p : pieces)
    lay.maxSize = std::max(lay.maxSize, p.size);
  /* The pocket stands left of rod 1, so every x stays at 0 or more. */
  const int shift = (board.panexColumns && board.pocketColumn) ? 1 : 0;
  lay.rodX.resize(lay.rodCount);
  lay.rodHeights.resize(lay.rodCount);
  for (unsigned int r = 0; r < lay.rodCount; r++) {
    lay.rodX[r] = (rodPosition(board, r) + shift) * lay.spacing;
    lay.rodHeights[r] = isPocket(board, r) ? std::max(1u, board.pocketHeight) : lay.rodHeight;
  }

  lay.disks.assign(pieces.size(), hotspot_c{});
  const stackMap_c & map = mapOf(prob, goal);
  unsigned int unplaced = 0;
  for (unsigned int i = 0; i < pieces.size(); i++) {
    unsigned int rod = 0;
    if (refOnMap(map, diskRef_c{pieces[i].shapeId, pieces[i].instance}, &rod) &&
        rod < lay.rodX.size()) {
      std::vector<unsigned int> column;
      unsigned int h = 0;
      for (const diskRef_c & d : map.rods[rod]) {
        int idx = findPiece(pieces, d);
        if (idx < 0)
          continue;
        if ((unsigned int)idx == i)
          h = (unsigned int)column.size();
        column.push_back((unsigned int)idx);
      }
      lay.disks[i].x = lay.rodX[rod];
      lay.disks[i].y = 0;
      lay.disks[i].z = discHeights(board, pieces, column, rod)[h];
      lay.disks[i].placed = true;
    } else {
      lay.disks[i].x = (int)unplaced * lay.spacing;
      lay.disks[i].y = -2 * lay.spacing;
      lay.disks[i].z = 0;
      lay.disks[i].placed = false;
      unplaced++;
    }
  }
  return lay;
}

namespace {

/* Memory one stacking takes in the search: a hash node holding its key and
 * its parent's, its bucket and its share of the queue. A packed key is the
 * same 8 bytes as a slide search's; a text key needs room for two strings. */
const unsigned long PACKED_STATE_BYTES = 64;
const unsigned long TEXT_STATE_BYTES = 160;

/* Breadth-first search over stackings, each held as a Key that enc and dec
 * turn to and from a Config. Fills path from start to goal when it finds
 * one. Moves are tried rod by rod, so the path is the first shortest one. */
template <class Key, class Encode, class Decode>
void stackBfs(const rodSet_c & board, const std::vector<piece_c> & pieces,
              const Config & start, const Config & goal, Encode enc, Decode dec,
              unsigned long stateBytes, stackSearch_c & search, std::vector<Config> & path) {
  const unsigned long limit = search.maxStates
      ? search.maxStates
      : sliding::memoryStates(stateBytes, search.highMemory);
  search.memoryStates = limit;
  const unsigned int rods = (unsigned int)start.size();

  const Key startKey = enc(start);
  const Key goalKey = enc(goal);
  std::unordered_map<Key, Key> parent;
  parent.emplace(startKey, startKey);
  std::queue<Key> q;
  q.push(startKey);

  unsigned long visited = 0;
  bool found = false;
  while (!q.empty() && !found) {
    if (parent.size() >= limit) {
      search.outcome = STACK_MEMORY;
      break;
    }
    if ((visited & 1023) == 0) {
      if (search.progress)
        search.progress->store(visited, std::memory_order_relaxed);
      if (search.stop && search.stop->load(std::memory_order_relaxed)) {
        search.outcome = STACK_STOPPED;
        break;
      }
    }
    const Key cur = q.front();
    q.pop();
    visited++;
    Config cfg = dec(cur);
    for (unsigned int from = 0; from < rods && !found; from++)
      for (unsigned int to = 0; to < rods; to++) {
        if (!moveAllowed(board, pieces, cfg, from, to))
          continue;
        cfg[to].push_back(cfg[from].back());
        cfg[from].pop_back();
        const Key next = enc(cfg);
        cfg[from].push_back(cfg[to].back());
        cfg[to].pop_back();
        if (!parent.emplace(next, cur).second)
          continue;
        if (next == goalKey) {
          found = true;
          break;
        }
        q.push(next);
      }
  }

  search.visited = visited;
  if (search.progress)
    search.progress->store(visited, std::memory_order_relaxed);
  if (!found)
    return;
  for (Key k = goalKey; ; k = parent[k]) {
    path.push_back(dec(k));
    if (k == startKey)
      break;
  }
  std::reverse(path.begin(), path.end());
}

} // namespace

std::unique_ptr<separation_c> findStackPath(const problem_c & prob, unsigned int maxStates) {
  stackSearch_c search;
  search.maxStates = maxStates;
  return findStackPath(prob, search);
}

std::unique_ptr<separation_c> findStackPath(const problem_c & prob, stackSearch_c & search) {
  search.outcome = STACK_NO_PATH;
  search.visited = 0;
  if (!setupError(prob).empty())
    return nullptr;

  const rodSet_c & board = prob.getPuzzle().getRodSet(prob.getRodSetId());
  std::vector<piece_c> pieces = piecesOf(prob);
  unsigned int n = (unsigned int)pieces.size();

  bool ok = false;
  Config start = configFromMap(prob, false, pieces, &ok);
  if (!ok)
    return nullptr;
  Config goal = configFromMap(prob, true, pieces, &ok);
  if (!ok)
    return nullptr;
  const unsigned int rods = (unsigned int)start.size();

  std::vector<Config> path;
  if (configsEqual(start, goal)) {
    path.push_back(start);
  } else {
    /* A stacking written out rod by rod: each rod's discs bottom to top,
     * then a separator, n. Packed into 64 bits when the symbols fit. */
    unsigned int bits = 1;
    while ((1u << bits) <= n)
      bits++;
    const unsigned int symbols = n + rods - 1;
    if ((unsigned long)bits * symbols <= 64) {
      auto enc = [bits, n](const Config & cfg) {
        uint64_t key = 0;
        for (unsigned int r = 0; r < cfg.size(); r++) {
          if (r)
            key = (key << bits) | n;
          for (unsigned int d : cfg[r])
            key = (key << bits) | d;
        }
        return key;
      };
      auto dec = [bits, n, rods, symbols](uint64_t key) {
        Config cfg(rods);
        unsigned int r = rods - 1;
        const uint64_t mask = (uint64_t(1) << bits) - 1;
        for (unsigned int i = 0; i < symbols; i++, key >>= bits) {
          unsigned int v = (unsigned int)(key & mask);
          if (v == n)
            r--;
          else
            cfg[r].insert(cfg[r].begin(), v);
        }
        return cfg;
      };
      stackBfs<uint64_t>(board, pieces, start, goal, enc, dec, PACKED_STATE_BYTES, search, path);
    } else {
      auto enc = [n](const Config & cfg) {
        std::string key;
        for (unsigned int r = 0; r < cfg.size(); r++) {
          if (r)
            key += (char)n;
          for (unsigned int d : cfg[r])
            key += (char)d;
        }
        return key;
      };
      auto dec = [n, rods](const std::string & key) {
        Config cfg(rods);
        unsigned int r = 0;
        for (char c : key) {
          if ((unsigned char)c == n)
            r++;
          else
            cfg[r].push_back((unsigned char)c);
        }
        return cfg;
      };
      stackBfs<std::string>(board, pieces, start, goal, enc, dec, TEXT_STATE_BYTES, search, path);
    }
    if (path.empty())
      return nullptr;
  }
  search.outcome = STACK_FOUND;
  return pathSeparation(prob, path);
}

bool searchInput(const problem_c & prob, stacking_t & start, stacking_t & goal,
                 std::vector<unsigned int> & sizes) {
  if (!setupError(prob).empty())
    return false;
  std::vector<piece_c> pieces = piecesOf(prob);
  bool ok = false;
  start = configFromMap(prob, false, pieces, &ok);
  if (!ok)
    return false;
  goal = configFromMap(prob, true, pieces, &ok);
  if (!ok)
    return false;
  sizes.clear();
  for (const piece_c & p : pieces)
    sizes.push_back(p.size);
  return true;
}

std::unique_ptr<separation_c> pathSeparation(const problem_c & prob,
                                             const std::vector<stacking_t> & path) {
  const rodSet_c & board = prob.getPuzzle().getRodSet(prob.getRodSetId());
  std::vector<piece_c> pieces = piecesOf(prob);
  const unsigned int n = (unsigned int)pieces.size();
  std::vector<unsigned int> names(n);
  for (unsigned int i = 0; i < n; i++)
    names[i] = i;
  auto sep = std::make_unique<separation_c>(nullptr, nullptr, names);
  if (path.empty())
    return sep;

  boardLayout_c lay = layoutBoard(prob, false);
  /* Cruise above the drawn pegs, and above a disc raised into the bridge. */
  int liftZ = (int)lay.rodHeight + (board.panexColumns ? 1 : 0);
  const drawBoard_c draw(board, pieces, lay.rodX);
  std::vector<std::unique_ptr<state_c>> frames;
  frames.push_back(stateFor(path[0], nullptr, 0, draw, n, liftZ));
  for (unsigned int step = 0; step + 1 < path.size(); step++) {
    frames.push_back(stateFor(path[step], &path[step + 1], 1, draw, n, liftZ));
    frames.push_back(stateFor(path[step], &path[step + 1], 2, draw, n, liftZ));
    frames.push_back(stateFor(path[step], &path[step + 1], 3, draw, n, liftZ));
  }
  for (int i = (int)frames.size() - 1; i >= 0; i--)
    sep->addstate(std::move(frames[i]));
  return sep;
}

unsigned int logicalMoves(const separation_c & path) {
  unsigned int steps = path.getMoves();
  return steps / STEPS_PER_MOVE;
}

std::unique_ptr<assembly_c> startAssembly(const problem_c & prob) {
  auto assm = std::make_unique<assembly_c>(prob.getPuzzle().getGridType());
  boardLayout_c lay = layoutBoard(prob, false);
  for (const hotspot_c & h : lay.disks) {
    if (h.placed)
      assm->addPlacement(0, h.x, h.y, h.z);
    else
      assm->addNonPlacement();
  }
  return assm;
}

unsigned int boardSpan(const problem_c & prob) {
  boardLayout_c lay = layoutBoard(prob, false);
  if (lay.rodCount == 0)
    return 4;
  /* The pocket is the last rod but stands first: take the widest. */
  unsigned int span = (unsigned int)(*std::max_element(lay.rodX.begin(), lay.rodX.end()) + lay.spacing);
  if (span < 4)
    span = 4;
  return span;
}

} // namespace stacking
