#include "stacking.h"

#include "assembly.h"
#include "bt_assert.h"
#include "disassembly.h"
#include "gridtype.h"
#include "problem.h"
#include "puzzle.h"
#include "voxel.h"

#include "../tools/xml.h"

#include <algorithm>
#include <map>
#include <queue>
#include <sstream>

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
  unsigned int cap = capacityOf(board, (unsigned int)pieces.size());
  if (rod.size() > cap)
    return "Rod " + std::to_string(rodNo + 1) + " of " + which + " holds " +
           std::to_string(rod.size()) + " discs, but the rod height only allows " +
           std::to_string(cap) + ".";

  if (!board.sizeMatters)
    return "";

  for (unsigned int i = 1; i < rod.size(); i++) {
    int below = findPiece(pieces, rod[i - 1]);
    int above = findPiece(pieces, rod[i]);
    if (below < 0 || above < 0)
      continue;
    if (pieces[above].size > pieces[below].size)
      return "On rod " + std::to_string(rodNo + 1) + " of " + which + ", " +
             diskLabel(prob, rod[i]) + " (size " + std::to_string(pieces[above].size) +
             ") sits on the smaller " + diskLabel(prob, rod[i - 1]) + " (size " +
             std::to_string(pieces[below].size) + "), but size matters is on.";
  }
  return "";
}

using Config = std::vector<std::vector<unsigned int>>;

std::string configKey(const Config & cfg) {
  std::string key;
  for (unsigned int r = 0; r < cfg.size(); r++) {
    if (r)
      key += '|';
    for (unsigned int i = 0; i < cfg[r].size(); i++) {
      if (i)
        key += ',';
      key += std::to_string(cfg[r][i]);
    }
  }
  return key;
}

bool configsEqual(const Config & a, const Config & b) {
  return a == b;
}

bool moveAllowed(const rodSet_c & board, const std::vector<piece_c> & pieces,
                 const Config & cfg, unsigned int from, unsigned int to) {
  if (from == to || cfg[from].empty())
    return false;
  if (cfg[to].size() >= capacityOf(board, (unsigned int)pieces.size()))
    return false;

  unsigned int mover = cfg[from].back();
  if (board.sizeMatters && !cfg[to].empty()) {
    unsigned int top = cfg[to].back();
    if (pieces[mover].size > pieces[top].size)
      return false;
  }

  int gap = (int)from - (int)to;
  if (gap < 0)
    gap = -gap;
  if (board.distanceMatters && !board.canMoveOver && gap != 1)
    return false;
  return true;
}

Config configFromMap(const problem_c & prob, bool goal, const std::vector<piece_c> & pieces,
                     bool * ok) {
  const rodSet_c & board = prob.getPuzzle().getRodSet(prob.getRodSetId());
  Config cfg(board.rodCount);
  const stackMap_c & map = mapOf(prob, goal);
  *ok = true;
  if (map.rods.size() != board.rodCount) {
    *ok = false;
    return cfg;
  }
  for (unsigned int r = 0; r < board.rodCount; r++) {
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

void writeState(state_c * st, const Config & cfg, int spacing) {
  for (unsigned int r = 0; r < cfg.size(); r++) {
    for (unsigned int h = 0; h < cfg[r].size(); h++) {
      unsigned int piece = cfg[r][h];
      st->set(piece, (int)r * spacing, 0, (int)h, 0);
    }
  }
}

std::unique_ptr<state_c> stateFor(const Config & before, const Config * after,
                                  int frame, int spacing, unsigned int n, int liftZ) {
  auto st = std::make_unique<state_c>(n);
  if (!after || frame == 0) {
    writeState(st.get(), before, spacing);
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
  writeState(st.get(), shown, spacing);

  /* Cruise in the first cell above the drawn peg so the disc clears every rod. */
  int z = liftZ;
  if (z < 1)
    z = 1;
  if (frame == 1)
    st->set(mover, (int)src * spacing, 0, z, 0);
  else if (frame == 2)
    st->set(mover, (int)dst * spacing, 0, z, 0);
  else
    writeState(st.get(), *after, spacing);
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
      std::string s;
      r.name = pars.getAttributeValue("name");
      s = pars.getAttributeValue("rods");
      if (!s.empty())
        r.rodCount = (unsigned int)atoi(s.c_str());
      if (r.rodCount < 1)
        r.rodCount = 1;
      s = pars.getAttributeValue("grow");
      if (!s.empty())
        r.growHeight = atoi(s.c_str()) != 0;
      s = pars.getAttributeValue("height");
      if (!s.empty())
        r.definedHeight = (unsigned int)atoi(s.c_str());
      s = pars.getAttributeValue("sizeMatters");
      if (!s.empty())
        r.sizeMatters = atoi(s.c_str()) != 0;
      s = pars.getAttributeValue("distanceMatters");
      if (!s.empty())
        r.distanceMatters = atoi(s.c_str()) != 0;
      s = pars.getAttributeValue("moveOver");
      if (!s.empty())
        r.canMoveOver = atoi(s.c_str()) != 0;
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
  unsigned int n = prob.getPuzzle().getRodSet(prob.getRodSetId()).rodCount;
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
    for (unsigned int r = board.rodCount; r < map.rods.size(); r++)
      if (!map.rods[r].empty())
        return std::string("The ") + which + " has discs on rod " + std::to_string(r + 1) +
               ", but the rod set only has " + std::to_string(board.rodCount) +
               (board.rodCount == 1 ? " rod." : " rods.");
    for (unsigned int r = 0; r < map.rods.size() && r < board.rodCount; r++) {
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
    if (mapOf(prob, goal).rods.size() < board.rodCount)
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
  if (map.rods[rod].size() >= capacityOf(board, (unsigned int)pieces.size()))
    return "That rod is full";

  if (board.sizeMatters && !map.rods[rod].empty()) {
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
  lay.rodCount = board.rodCount;
  lay.rodHeight = drawnPegHeight(board, (unsigned int)pieces.size());
  lay.rodX.resize(board.rodCount);
  for (unsigned int r = 0; r < board.rodCount; r++)
    lay.rodX[r] = (int)r * lay.spacing;

  lay.disks.assign(pieces.size(), hotspot_c{});
  const stackMap_c & map = mapOf(prob, goal);
  unsigned int unplaced = 0;
  for (unsigned int i = 0; i < pieces.size(); i++) {
    unsigned int rod = 0;
    if (refOnMap(map, diskRef_c{pieces[i].shapeId, pieces[i].instance}, &rod) &&
        rod < lay.rodX.size()) {
      unsigned int h = 0;
      if (rod < map.rods.size()) {
        for (const diskRef_c & d : map.rods[rod]) {
          if (d.shapeId == pieces[i].shapeId && d.instance == pieces[i].instance)
            break;
          h++;
        }
      }
      lay.disks[i].x = lay.rodX[rod];
      lay.disks[i].y = 0;
      lay.disks[i].z = (int)h;
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

std::unique_ptr<separation_c> findStackPath(const problem_c & prob, unsigned int maxStates) {
  if (!setupError(prob).empty())
    return nullptr;

  const rodSet_c & board = prob.getPuzzle().getRodSet(prob.getRodSetId());
  std::vector<piece_c> pieces = piecesOf(prob);
  unsigned int n = (unsigned int)pieces.size();
  int spacing = spacingOf(pieces);

  bool ok = false;
  Config start = configFromMap(prob, false, pieces, &ok);
  if (!ok)
    return nullptr;
  Config goal = configFromMap(prob, true, pieces, &ok);
  if (!ok)
    return nullptr;

  std::vector<Config> path;
  if (configsEqual(start, goal)) {
    path.push_back(start);
  } else {
    std::map<std::string, unsigned int> seen;
    std::vector<Config> nodes;
    std::vector<int> parent;
    std::queue<unsigned int> q;

    nodes.push_back(start);
    parent.push_back(-1);
    seen[configKey(start)] = 0;
    q.push(0);

    int found = -1;
    while (!q.empty() && nodes.size() < maxStates && found < 0) {
      unsigned int cur = q.front();
      q.pop();
      for (unsigned int from = 0; from < board.rodCount; from++) {
        for (unsigned int to = 0; to < board.rodCount; to++) {
          if (!moveAllowed(board, pieces, nodes[cur], from, to))
            continue;
          Config next = nodes[cur];
          unsigned int mover = next[from].back();
          next[from].pop_back();
          next[to].push_back(mover);
          std::string key = configKey(next);
          if (seen.count(key))
            continue;
          seen[key] = (unsigned int)nodes.size();
          parent.push_back((int)cur);
          nodes.push_back(next);
          if (configsEqual(next, goal)) {
            found = (int)nodes.size() - 1;
            break;
          }
          q.push((unsigned int)nodes.size() - 1);
        }
        if (found >= 0)
          break;
      }
    }
    if (found < 0)
      return nullptr;

    for (int i = found; i >= 0; i = parent[i])
      path.push_back(nodes[i]);
    std::reverse(path.begin(), path.end());
  }

  std::vector<unsigned int> names(n);
  for (unsigned int i = 0; i < n; i++)
    names[i] = i;
  auto sep = std::make_unique<separation_c>(nullptr, nullptr, names);

  int liftZ = (int)drawnPegHeight(board, n);
  std::vector<std::unique_ptr<state_c>> frames;
  frames.push_back(stateFor(path[0], nullptr, 0, spacing, n, liftZ));
  for (unsigned int step = 0; step + 1 < path.size(); step++) {
    frames.push_back(stateFor(path[step], &path[step + 1], 1, spacing, n, liftZ));
    frames.push_back(stateFor(path[step], &path[step + 1], 2, spacing, n, liftZ));
    frames.push_back(stateFor(path[step], &path[step + 1], 3, spacing, n, liftZ));
  }
  for (int i = (int)frames.size() - 1; i >= 0; i--)
    sep->addstate(std::move(frames[i]));
  return sep;
}

unsigned int logicalMoves(const separation_c & path) {
  unsigned int steps = path.getMoves();
  return steps / 3;
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
  unsigned int span = (unsigned int)(lay.rodX.back() + lay.spacing);
  if (span < 4)
    span = 4;
  return span;
}

} // namespace stacking
