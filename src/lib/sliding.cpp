/* BurrTools
 *
 * Sliding-tray helpers: start/goal colour maps and one-cell slide search.
 */
#include "sliding.h"

#include "assembly.h"
#include "disassembly.h"
#include "gridtype.h"
#include "problem.h"
#include "puzzle.h"
#include "voxel.h"

#include <algorithm>
#include <map>
#include <queue>
#include <set>
#include <sstream>
#include <string>
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

void fillTray(voxel_c * v) {
  for (unsigned int y = 0; y < v->getY(); y++)
    for (unsigned int x = 0; x < v->getX(); x++)
      v->setState(x, y, 0, voxel_c::VX_FILLED);
}

void clearColors(voxel_c * v) {
  for (unsigned int i = 0; i < v->getXYZ(); i++)
    if (v->getState(i) != voxel_c::VX_EMPTY)
      v->setColor(i, 0);
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
  int x, y;
  unsigned char trans;
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

bool placementValid(const voxel_c & tray,
                    const std::vector<const voxel_c *> & pieces,
                    const SlideState & s,
                    unsigned int moving) {
  /* Occupancy grid for non-moving pieces, then test the mover. */
  std::set<std::pair<int, int>> occ;
  for (unsigned int i = 0; i < s.places.size(); i++) {
    if (i == moving)
      continue;
    const voxel_c * p = pieces[i];
    int hx = (int)p->getHx();
    int hy = (int)p->getHy();
    /* Pieces keep their assembly transform; for sliding we only translate.
     * The voxel we hold is already in the assembly orientation. */
    for (unsigned int y = 0; y < p->getY(); y++)
      for (unsigned int x = 0; x < p->getX(); x++) {
        if (p->getState(x, y, 0) == voxel_c::VX_EMPTY)
          continue;
        int tx = s.places[i].x + (int)x - hx;
        int ty = s.places[i].y + (int)y - hy;
        if (!isFloor(tray, tx, ty))
          return false;
        occ.insert({tx, ty});
      }
  }

  const voxel_c * p = pieces[moving];
  int hx = (int)p->getHx();
  int hy = (int)p->getHy();
  for (unsigned int y = 0; y < p->getY(); y++)
    for (unsigned int x = 0; x < p->getX(); x++) {
      if (p->getState(x, y, 0) == voxel_c::VX_EMPTY)
        continue;
      int tx = s.places[moving].x + (int)x - hx;
      int ty = s.places[moving].y + (int)y - hy;
      if (!isFloor(tray, tx, ty))
        return false;
      if (occ.count({tx, ty}))
        return false;
    }
  return true;
}

bool goalsSatisfied(const problem_c & prob,
                    const std::vector<const voxel_c *> & pieces,
                    const SlideState & s) {
  const voxel_c * tray = prob.resultValid() ? getResultShape(prob) : nullptr;
  const bool startGoal = tray && isStartGoalShape(tray);

  if (startGoal) {
    std::vector<unsigned int> partOf(pieces.size());
    {
      unsigned int pc = 0;
      for (unsigned int part = 0; part < prob.getNumberOfParts(); part++)
        for (unsigned int j = 0; j < prob.getPartMaximum(part); j++)
          partOf[pc++] = part;
    }

    /* Empty cells are walls. A piece may slide across a variable cell, and
     * the finished position must leave every variable cell empty. Only
     * normal cells are legal places to stop, including a goal stamp. */
    for (unsigned int i = 0; i < pieces.size(); i++) {
      const voxel_c * p = pieces[i];
      if (!p)
        return false;
      int hx = (int)p->getHx();
      int hy = (int)p->getHy();
      for (unsigned int y = 0; y < p->getY(); y++)
        for (unsigned int x = 0; x < p->getX(); x++) {
          if (p->getState(x, y, 0) == voxel_c::VX_EMPTY)
            continue;
          int tx = s.places[i].x + (int)x - hx;
          int ty = s.places[i].y + (int)y - hy;
          if (!isFloor(*tray, tx, ty))
            return false;
          if (tray->getState(tx, ty, 0) == voxel_c::VX_VARIABLE)
            return false;
        }
    }

    for (unsigned int i = 0; i < pieces.size(); i++) {
      unsigned int shapeId = prob.getShapeIdOfPart(partOf[i]);
      unsigned int col = shapeId + 1;
      bool has = false;
      for (unsigned int gi = 0; gi < tray->getXYZ(); gi++)
        if (tray->getGoalPiece(gi) == col) {
          has = true;
          break;
        }
      if (!has)
        continue;

      const voxel_c * p = pieces[i];
      int hx = (int)p->getHx();
      int hy = (int)p->getHy();
      for (unsigned int y = 0; y < p->getY(); y++)
        for (unsigned int x = 0; x < p->getX(); x++) {
          if (p->getState(x, y, 0) == voxel_c::VX_EMPTY)
            continue;
          int tx = s.places[i].x + (int)x - hx;
          int ty = s.places[i].y + (int)y - hy;
          if (!isFloor(*tray, tx, ty) || tray->getGoalPiece(tx, ty, 0) != col)
            return false;
        }
    }
    return true;
  }

  if (!prob.goalValid())
    return true;
  const voxel_c * goal = prob.getGoalShape();

  /* Map each placement index back to its part so we can look up colours. */
  std::vector<unsigned int> partOf(pieces.size());
  {
    unsigned int pc = 0;
    for (unsigned int part = 0; part < prob.getNumberOfParts(); part++)
      for (unsigned int j = 0; j < prob.getPartMaximum(part); j++)
        partOf[pc++] = part;
  }

  for (unsigned int i = 0; i < pieces.size(); i++) {
    unsigned int shapeId = prob.getShapeIdOfPart(partOf[i]);
    unsigned int col = pieceColor(prob, shapeId);
    if (col == 0)
      col = partOf[i] + 1;

    bool has = false;
    for (unsigned int gi = 0; gi < goal->getXYZ(); gi++)
      if (goal->getColor(gi) == col) {
        has = true;
        break;
      }
    if (!has)
      continue;

    const voxel_c * p = pieces[i];
    if (!p)
      return false;
    int hx = (int)p->getHx();
    int hy = (int)p->getHy();
    for (unsigned int y = 0; y < p->getY(); y++)
      for (unsigned int x = 0; x < p->getX(); x++) {
        if (p->getState(x, y, 0) == voxel_c::VX_EMPTY)
          continue;
        int tx = s.places[i].x + (int)x - hx;
        int ty = s.places[i].y + (int)y - hy;
        if (!isFloor(*goal, tx, ty) || goal->getColor(tx, ty, 0) != col)
          return false;
      }
  }
  return true;
}

/* One piece stepping one cell, or no single mover. */
static bool unitStep(const SlideState & prev, const SlideState & next,
                     int * piece, int * dx, int * dy) {
  if (prev.places.size() != next.places.size())
    return false;
  int mover = -1;
  int mx = 0;
  int my = 0;
  for (size_t i = 0; i < prev.places.size(); i++) {
    int sx = next.places[i].x - prev.places[i].x;
    int sy = next.places[i].y - prev.places[i].y;
    if (sx == 0 && sy == 0 && prev.places[i].trans == next.places[i].trans)
      continue;
    if (mover >= 0)
      return false;
    mover = (int)i;
    mx = sx;
    my = sy;
  }
  if (mover < 0)
    return false;
  if (!((mx == 0 && (my == 1 || my == -1)) || (my == 0 && (mx == 1 || mx == -1))))
    return false;
  *piece = mover;
  *dx = mx;
  *dy = my;
  return true;
}

/* Consecutive one-cell steps of one piece along one direction are one slide.
 * A turn, or another piece moving, starts a new move. */
static void collapseStraightSlides(std::vector<SlideState> & path) {
  if (path.size() < 3)
    return;
  std::vector<SlideState> kept;
  kept.push_back(path[0]);
  int runPiece = -1;
  int runDx = 0;
  int runDy = 0;
  for (size_t i = 1; i < path.size(); i++) {
    int piece = -1;
    int dx = 0;
    int dy = 0;
    bool step = unitStep(path[i - 1], path[i], &piece, &dx, &dy);
    if (step && piece == runPiece && dx == runDx && dy == runDy) {
      kept.back() = path[i];
      continue;
    }
    kept.push_back(path[i]);
    if (step) {
      runPiece = piece;
      runDx = dx;
      runDy = dy;
    } else {
      runPiece = -1;
      runDx = 0;
      runDy = 0;
    }
  }
  path.swap(kept);
}

} // namespace

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

unsigned int pieceColor(const problem_c & prob, unsigned int shapeId) {
  if (shapeId >= prob.getPuzzle().getNumberOfShapes())
    return 0;
  const voxel_c * piece = prob.getPuzzle().getShape(shapeId);
  for (unsigned int i = 0; i < piece->getXYZ(); i++)
    if (piece->getState(i) != voxel_c::VX_EMPTY && piece->getColor(i) != 0)
      return piece->getColor(i);
  return 0;
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

std::unique_ptr<separation_c> findSlidePath(const problem_c & prob,
                                            const assembly_c & start,
                                            unsigned int maxStates) {
  if (!prob.resultValid())
    return nullptr;

  const voxel_c * tray = getResultShape(prob);
  unsigned int n = start.placementCount();
  if (n == 0)
    return nullptr;

  std::vector<const voxel_c *> pieces(n, nullptr);
  SlideState initial;
  initial.places.resize(n);

  unsigned int pc = 0;
  for (unsigned int part = 0; part < prob.getNumberOfParts(); part++) {
    for (unsigned int j = 0; j < prob.getPartMaximum(part); j++) {
      if (!start.isPlaced(pc)) {
        for (auto * p : pieces)
          delete p;
        return nullptr;
      }
      voxel_c * oriented = prob.getPuzzle().getGridType()->getVoxel(
          *prob.getPartShape(part));
      if (!oriented->transform(start.getTransformation(pc))) {
        delete oriented;
        for (auto * p : pieces)
          delete p;
        return nullptr;
      }
      pieces[pc] = oriented;
      initial.places[pc].x = start.getX(pc);
      initial.places[pc].y = start.getY(pc);
      initial.places[pc].trans = start.getTransformation(pc);
      pc++;
    }
  }

  if (goalsSatisfied(prob, pieces, initial)) {
    std::vector<unsigned int> pcs(n);
    for (unsigned int i = 0; i < n; i++)
      pcs[i] = i;
    auto sep = std::make_unique<separation_c>(nullptr, nullptr, pcs);
    auto st = std::make_unique<state_c>(n);
    for (unsigned int i = 0; i < n; i++)
      st->set(i, initial.places[i].x, initial.places[i].y, start.getZ(i),
              initial.places[i].trans);
    sep->addstate(std::move(st));
    for (auto * p : pieces)
      delete p;
    return sep;
  }

  std::queue<SlideState> q;
  std::map<std::string, std::pair<std::string, unsigned int>> parent;
  /* parent[key] = {prevKey, movedPiece}; movedPiece==n means root. */

  std::string startKey = stateKey(initial);
  q.push(initial);
  parent[startKey] = {"", n};

  unsigned int visited = 0;
  std::string goalKey;
  bool found = false;

  while (!q.empty() && visited < maxStates) {
    SlideState cur = q.front();
    q.pop();
    visited++;

    for (unsigned int pi = 0; pi < n; pi++) {
      for (int d = 0; d < 4; d++) {
        SlideState next = cur;
        next.places[pi].x += DX[d];
        next.places[pi].y += DY[d];
        if (!placementValid(*tray, pieces, next, pi))
          continue;
        std::string key = stateKey(next);
        if (parent.count(key))
          continue;
        parent[key] = {stateKey(cur), pi};
        if (goalsSatisfied(prob, pieces, next)) {
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

  for (auto * p : pieces)
    delete p;

  if (!found)
    return nullptr;

  /* Reconstruct the path. States keep the assembly placement of each piece
   * so the move slider can drop them straight onto the board. */
  std::vector<SlideState> path;
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
      }
      path.push_back(st);
      auto it = parent.find(k);
      if (it == parent.end() || it->second.second == n)
        break;
      k = it->second.first;
    }
  }
  std::reverse(path.begin(), path.end());
  collapseStraightSlides(path);

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
