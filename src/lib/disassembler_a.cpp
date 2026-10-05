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
#include "disassembler_a.h"

#include "bt_assert.h"
#include "problem.h"
#include "grouping.h"
#include "disassemblernode.h"
#include "movementanalysator.h"
#include "assembly.h"
#include "disassembly.h"
#include "rotationmoves_0.h"
#include "disassemblerhashes.h"
#include "helperpool.h"

#include <chrono>
#include <cstdlib>
#include <unordered_set>

namespace {

unsigned long long nowUs(void) {
  using namespace std::chrono;
  return (unsigned long long)duration_cast<microseconds>(steady_clock::now().time_since_epoch()).count();
}

void dropNode(disassemblerNode_c * n) {
  if (n->decRefCount())
    delete n;
}

/* BURRTOOLS_NO_DISASM_PAR=1: search every level on the calling thread */
bool levelThreadsAllowed(void) {
  static const bool allowed = []() {
    const char * e = getenv("BURRTOOLS_NO_DISASM_PAR");
    return !(e && e[0] && e[0] != '0');
  }();
  return allowed;
}

}

namespace {
/* see setLevelThreadCostUs */
std::atomic<double> levelThreadCostUs{400.0};
}

void disassembler_a_c::setLevelThreadCostUs(double us) {
  levelThreadCostUs.store(us, std::memory_order_relaxed);
}

/* What the search of one position of a level gave, when the level is spread
 * over threads. */
struct disassembler_a_c::expansion_c {
  /* the positions it leads to that neither the old, the current nor (as it
   * was when the threads set out) the new front has, in the order found */
  std::vector<disassemblerNode_c *> cands;
  /* moves taken from find() to get them */
  unsigned int raw = 0;
  /* searched, not passed over or stopped */
  bool expanded = false;
  /* the last of cands separates the puzzle */
  bool separation = false;
  /* the search was left before find() ran out */
  bool more = false;

  void reset(void) {
    cands.clear();
    raw = 0;
    expanded = separation = more = false;
  }
};

disassembler_a_c::disassembler_a_c(const problem_c & puz, bool enableRotations,
                                   solverType_e solverType) :
  disassembler_c(), puzzle(puz), groups(std::make_unique<grouping_c>()), abort(false),
  rotationsEnabled(enableRotations), solverKind(solverType) {

  /* Initialise the grouping class */
  for (unsigned int i = 0; i < puz.getNumberOfParts(); i++)
    for (unsigned int j = 0; j < puz.getNumberOfPartGroups(i); j++)
      groups->addPieces(puz.getShapeIdOfPart(i),
                        puz.getPartGroupId(i, j),
                        puz.getPartGroupCount(i, j));

  /* initialize piece 2 shape transformation */
  piece2shape.resize(puz.getNumberOfPieces());
  int p = 0;
  for (unsigned int i = 0; i < puz.getNumberOfParts(); i++)
    for (unsigned int j = 0; j < puz.getPartMaximum(i); j++)
      piece2shape[p++] = i;

  analyse = std::make_unique<movementAnalysator_c>(puzzle, enableRotations, solverType);
  analyse->setStopFlag(&abort);
}

void disassembler_a_c::setCheckRotations(bool enable) {
  rotationsEnabled = enable;
  analyse->setCheckRotations(enable);
  std::lock_guard<std::mutex> lock(helperMutex);
  for (std::unique_ptr<movementAnalysator_c> & h : helpers)
    if (h)
      h->setCheckRotations(enable);
}

unsigned long long disassembler_a_c::getRotationSearchUs(void) const {
  unsigned long long us = analyse ? analyse->getRotationSearchUs() : 0;
  std::lock_guard<std::mutex> lock(helperMutex);
  for (const std::unique_ptr<movementAnalysator_c> & h : helpers)
    if (h)
      us += h->getRotationSearchUs();
  return us;
}

unsigned long long disassembler_a_c::getLinearSearchUs(void) const {
  unsigned long long us = analyse ? analyse->getLinearSearchUs() : 0;
  std::lock_guard<std::mutex> lock(helperMutex);
  for (const std::unique_ptr<movementAnalysator_c> & h : helpers)
    if (h)
      us += h->getLinearSearchUs();
  return us;
}

void disassembler_a_c::setHelperPool(helperPool_c * p) {
  pool = levelThreadsAllowed() ? p : nullptr;
}

movementAnalysator_c & disassembler_a_c::analysatorFor(unsigned int slot) {

  if (slot == 0)
    return *analyse;

  std::lock_guard<std::mutex> lock(helperMutex);
  if (helpers.size() < slot)
    helpers.resize(slot);
  if (!helpers[slot - 1]) {
    helpers[slot - 1] = std::make_unique<movementAnalysator_c>(puzzle, rotationsEnabled, solverKind);
    helpers[slot - 1]->setStopFlag(&abort);
  }
  return *helpers[slot - 1];
}

bool disassembler_a_c::getProgress(disassemblyProgress_c & p) const {

  p = disassemblyProgress_c();

  const unsigned long long start = progStartUs.load(std::memory_order_relaxed);
  if (start == 0)
    return true;

  const unsigned long long now = nowUs();
  p.active = true;
  p.elapsedMs = now > start ? (now - start) / 1000 : 0;
  p.pieces = progPieces.load(std::memory_order_relaxed);
  p.searchPieces = progSearchPieces.load(std::memory_order_relaxed);
  p.separations = progSeparations.load(std::memory_order_relaxed);
  p.depth = progDepth.load(std::memory_order_relaxed);
  p.level = progLevel.load(std::memory_order_relaxed);
  p.levelDone = progLevelDone.load(std::memory_order_relaxed);
  p.levelSize = progLevelSize.load(std::memory_order_relaxed);
  if (p.levelDone > p.levelSize)
    p.levelDone = p.levelSize;
  p.nextLevelSize = progNextSize.load(std::memory_order_relaxed);
  p.nodes = progNodes.load(std::memory_order_relaxed);
  p.threads = progThreads.load(std::memory_order_relaxed);
  return true;
}

void disassembler_a_c::expandNode(movementAnalysator_c & an, disassemblerNode_c * node,
                                  const std::vector<unsigned int> & pieces,
                                  const nodeHash & oldFront, const nodeHash & curFront, const nodeHash & newFront,
                                  unsigned int maxSuccessors, expansion_c & e) {

  /* a position can be reached from this one in more than one way */
  std::unordered_set<disassemblerNode_c *, disassemblerNodePtrHash, disassemblerNodePtrEqual> seen;
  unsigned int added = 0;

  an.init_find(node, pieces);

  disassemblerNode_c * st;

  while ((st = an.find())) {

    e.raw++;

    if (oldFront.contains(st) || curFront.contains(st) || newFront.contains(st) ||
        !seen.insert(st).second) {
      dropNode(st);
      continue;
    }

    e.cands.push_back(st);

    if (st->is_separation()) {
      e.separation = true;
      e.more = true;
      break;
    }

    if (maxSuccessors && ++added >= maxSuccessors) {
      e.more = true;
      break;
    }
  }

  /* The analysator keeps hold of positions it has handed out. Let go now,
   * while this thread is the only one to touch this position's count of
   * references. */
  an.endFind();

  e.expanded = !aborted();
}

/* This is the search of the disassemblers. It is a breadth first search
 * through the positions: first all that can be reached with one move, then
 * with 2 moves and so on, until a position is found that separates the
 * puzzle into 2 parts.
 *
 * Closed positions are kept only as long as they can be on a shortest path
 * to something still to come: in 3 fronts, each all the positions at one
 * distance from the start. A position found from the current front that is
 * in the old one is a step back, one in the current front a step sideways,
 * one in the new front one we already have a way to. Only what none of them
 * has is new. When the current front is through, the old one is let go
 * (reference counting frees what nothing points to any more), the current
 * becomes the old and the new the current.
 */
separation_c * disassembler_a_c::searchLevels(const std::vector<unsigned int> & pieces, disassemblerNode_c * start,
                                              unsigned int maxSuccessors) {

  struct depth_c {
    std::atomic<unsigned int> & d;
    explicit depth_c(std::atomic<unsigned int> & depth) : d(depth) { d.fetch_add(1, std::memory_order_relaxed); }
    ~depth_c() { d.fetch_sub(1, std::memory_order_relaxed); }
  } depth(progDepth);

  progSearchPieces.store((unsigned int)pieces.size(), std::memory_order_relaxed);

  nodeHash closed[3];
  int oldFront = 0;
  int curFront = 1;
  int newFront = 2;

  /* the positions of the current front, in the order found, and what is
   * found for the next; both are kept alive by the fronts */
  std::vector<disassemblerNode_c *> level, next;

  closed[curFront].insert(start);
  level.push_back(start);

  std::vector<expansion_c> exp;
  disassemblerNode_c * found = 0;
  unsigned int levelNo = 0;

  /* what one position takes to search, to tell whether a level is worth
   * waking other threads for; 0 until one has been timed */
  double usPerNode = 0;

  while (!level.empty() && !found) {

    progLevel.store(levelNo, std::memory_order_relaxed);
    progLevelSize.store((unsigned long)level.size(), std::memory_order_relaxed);
    progLevelDone.store(0, std::memory_order_relaxed);
    progNextSize.store(0, std::memory_order_relaxed);

    size_t pos = 0;

    while (pos < level.size() && !found) {

      if (aborted())
        return 0;

      const size_t remaining = level.size() - pos;
      size_t batch = 1;
      if (pool && remaining >= 2 &&
          usPerNode * (double)remaining >= levelThreadCostUs.load(std::memory_order_relaxed)) {
        batch = (size_t)pool->cores() * 32;
        if (batch > remaining)
          batch = remaining;
      }

      const unsigned long long t0 = nowUs();
      size_t done = 0;
      unsigned int used = 1;

      if (batch == 1) {

        /* one position, on this thread: what it leads to goes straight
         * into the new front */
        disassemblerNode_c * node = level[pos];
        unsigned int added = 0;
        disassemblerNode_c * st;

        analyse->init_find(node, pieces);

        while ((st = analyse->find())) {

          if (closed[oldFront].contains(st) || closed[curFront].contains(st) || closed[newFront].insert(st)) {
            /* known: a longer or equally long way to it */
            dropNode(st);
            continue;
          }

          if (st->is_separation()) {
            found = st;
            break;
          }

          next.push_back(st);
          dropNode(st);

          if (maxSuccessors && ++added >= maxSuccessors)
            break;
        }

        done = 1;

      } else {

        /* Several positions at once. Each thread takes the next one not
         * taken and notes what it leads to; nothing shared is changed. */
        if (exp.size() < batch)
          exp.resize(batch);
        for (size_t i = 0; i < batch; i++)
          exp[i].reset();

        std::atomic<size_t> nextIdx{0};
        /* the first position found to lead to a separation: the ones
         * after it will not be wanted */
        std::atomic<size_t> sepIdx{batch};

        const nodeHash & oF = closed[oldFront];
        const nodeHash & cF = closed[curFront];
        const nodeHash & nF = closed[newFront];
        disassemblerNode_c * const * nodes = &level[pos];

        used = pool->run((unsigned int)(batch < pool->cores() ? batch : pool->cores()),
                         [&](unsigned int slot) {
          movementAnalysator_c & an = analysatorFor(slot);
          for (;;) {
            const size_t i = nextIdx.fetch_add(1, std::memory_order_relaxed);
            if (i >= batch || i > sepIdx.load(std::memory_order_relaxed) || aborted())
              break;
            expandNode(an, nodes[i], pieces, oF, cF, nF, maxSuccessors, exp[i]);
            progLevelDone.fetch_add(1, std::memory_order_relaxed);
            if (exp[i].separation) {
              size_t s = sepIdx.load(std::memory_order_relaxed);
              while (i < s && !sepIdx.compare_exchange_weak(s, i, std::memory_order_relaxed)) {}
            }
          }
        });

        /* Put together what they found, position by position in the order
         * of the level: exactly what the search on one thread would have
         * put into the new front, and in that order. */
        for (; done < batch && exp[done].expanded && !found && !aborted(); done++) {

          expansion_c & e = exp[done];
          unsigned int added = 0;
          bool full = false;
          size_t k = 0;

          while (k < e.cands.size()) {
            disassemblerNode_c * st = e.cands[k++];

            if (closed[newFront].insert(st)) {
              dropNode(st);
              continue;
            }

            if (st->is_separation()) {
              found = st;
              break;
            }

            next.push_back(st);
            dropNode(st);

            if (maxSuccessors && ++added >= maxSuccessors) {
              full = true;
              break;
            }
          }

          for (; k < e.cands.size(); k++)
            dropNode(e.cands[k]);
          e.cands.clear();

          if (!found && !full && e.more) {

            /* Its thread left this position early, at the most new
             * positions one may give or at a separation, counting some
             * that another position turned out to have given first. Go on
             * from where it left. */
            unsigned int skip = e.raw;
            disassemblerNode_c * st;

            analyse->init_find(nodes[done], pieces);

            while ((st = analyse->find())) {

              if (skip) {
                skip--;
                dropNode(st);
                continue;
              }

              if (closed[oldFront].contains(st) || closed[curFront].contains(st) || closed[newFront].insert(st)) {
                dropNode(st);
                continue;
              }

              if (st->is_separation()) {
                found = st;
                break;
              }

              next.push_back(st);
              dropNode(st);

              if (maxSuccessors && ++added >= maxSuccessors)
                break;
            }
          }
        }

        /* what was searched for nothing: after a separation, or a stop */
        for (size_t i = done; i < batch; i++) {
          for (disassemblerNode_c * st : exp[i].cands)
            dropNode(st);
          exp[i].cands.clear();
        }
      }

      analyse->endFind();

      if (aborted()) {
        if (found)
          dropNode(found);
        return 0;
      }

      bt_assert(done > 0);

      pos += done;
      progNodes.fetch_add(done, std::memory_order_relaxed);
      progLevelDone.store((unsigned long)pos, std::memory_order_relaxed);
      progNextSize.store((unsigned long)next.size(), std::memory_order_relaxed);
      progThreads.store(used, std::memory_order_relaxed);

      const double us = (double)(nowUs() - t0) * (double)used / (double)done;
      usPerNode = usPerNode > 0 ? 0.75 * usPerNode + 0.25 * us : us;
    }

    if (found)
      break;

    /* the current front is through: open up the next */
    closed[oldFront].clear();
    oldFront = curFront;
    curFront = newFront;
    newFront = (newFront + 1) % 3;

    level.swap(next);
    next.clear();
    levelNo++;
  }

  if (!found)
    return 0;

  /* a position that separates the puzzle: on into the two parts, which
   * calls this search again for each */
  separation_c * res = checkSubproblems(found, pieces);

  dropNode(found);

  return res;
}

disassembler_a_c::~disassembler_a_c() = default;

/* create all the necessary parameters for one of the two possible subproblems
 * our current problems divides into
 */
static void create_new_params(const disassemblerNode_c * st, disassemblerNode_c ** n, std::vector<unsigned int> & pn, const std::vector<unsigned int> & pieces, int part, bool cond) {

  *n = new disassemblerNode_c(part);

  int num = 0;

  for (unsigned int i = 0; i < pieces.size(); i++)
    if (st->is_piece_removed(i) == cond) {
      // we take the data from the node before the current, because the current node contains
      // the disassembled positions, which are not interesting, we want to have the position
      // before the puzzle falls apart but just take the halve of the pieces that matter
      (*n)->set(num,
                st->getComefrom()->getX(i),
                st->getComefrom()->getY(i),
                st->getComefrom()->getZ(i),
                st->getComefrom()->getTrans(i));
      pn.push_back(pieces[i]);
      num++;
    }

  bt_assert(num == part);
}

separation_c * disassembler_a_c::checkSubproblem(int pieceCount, const std::vector<unsigned int> & pieces, const disassemblerNode_c * st, bool left, bool * ok) {

  separation_c * res = 0;

  if (pieceCount == 1) {
    *ok = true;
  } else if (subProbGroup(st, pieces, left)) {
    *ok = true;
  } else {

    disassemblerNode_c *n;
    std::vector<unsigned int> pn;
    create_new_params(st, &n, pn, pieces, pieceCount, left);
    res = disassemble_rec(pn, n);

    if (n->decRefCount())
      delete n;

    /* A search that was stopped has not shown that the pieces cannot come
     * apart, so they are not to be taken for a group either. */
    *ok = res || (!aborted() && subProbGrouping(pn));
  }

  return res;
}

separation_c * disassembler_a_c::checkSubproblems(const disassemblerNode_c * st, const std::vector<unsigned int> &pieces) {

  /* if we get here we have found a node that separated the puzzle into
   * 2 pieces. So we recursively solve the subpuzzles and create a tree
   * with them that needs to be returned
   */
  separation_c * erg = 0;

  progSeparations.fetch_add(1, std::memory_order_relaxed);

  /* count the pieces in both parts */
  int part1 = 0, part2 = 0;

  for (unsigned int i = 0; i < pieces.size(); i++)
    if (st->is_piece_removed(i))
      part2++;
    else
      part1++;

  /* each subpart must contain at least 1 piece,
   * otherwise there is something wrong
   */
  bt_assert((part1 > 0) && (part2 > 0));

  separation_c * left, *remove;
  bool left_ok = false;
  bool remove_ok = false;
  left = remove = 0;

  /* all right, the following thing come twice, maybe I should
   * put it into a function, anyway:
   * if the subproblem to check has only one piece, it's solved
   * if all the pieces belong to the same group, we can stop
   * else try to disassemble, if that fails, try to
   * group the involved pieces into an identical group
   */
  remove = checkSubproblem(part1, pieces, st, false, &remove_ok);

  /* only check the left over part, when the removed part is OK */
  if (remove_ok)
    left = checkSubproblem(part2, pieces, st, true, &left_ok);

  /* if both subproblems are either trivial or solvable, return the
   * result, otherwise return 0
   */
  if (remove_ok && left_ok) {

    /* both subproblems are solvable -> construct tree */
    erg = new separation_c(left, remove, pieces);

    const disassemblerNode_c * st2 = st;

    do {
      auto s = std::make_unique<state_c>(pieces.size());

      for (unsigned int i = 0; i < pieces.size(); i++) {

        if (st2->is_piece_removed(i)) {
          /* when the piece is removed in here there must be a
           * predecessor node
           */
          bt_assert(st2->getComefrom());

          s->set(i, st2->getComefrom()->getX(i) + 20000*st2->getX(i),
              st2->getComefrom()->getY(i) + 20000*st2->getY(i),
              st2->getComefrom()->getZ(i) + 20000*st2->getZ(i),
              st2->getComefrom()->getTrans(i));

        } else
          s->set(i, st2->getX(i), st2->getY(i), st2->getZ(i), st2->getTrans(i));
      }

      if (st2->isRotationMove()) {
        unsigned int code = st2->getDirection() - ROTATION_DIR_BASE;
        s->setRotationArrival(st2->getRotPiece(),
                              st2->getRotPivotX(), st2->getRotPivotY(), st2->getRotPivotZ(),
                              code / 2, code % 2);
      }

      erg->addstate(std::move(s));

      st2 = st2->getComefrom();
    } while (st2);

  } else {

    /* one of the subproblems was unsolvable in this case the whole
     * puzzle is unsolvable, so we can as well stop here
     */
    if (left) delete left;
    if (remove) delete remove;
  }

  return erg;
}

unsigned short disassembler_a_c::subProbGroup(const disassemblerNode_c * st, const std::vector<unsigned int> & pn, bool cond) {

  unsigned short group = 0;

  for (unsigned int i = 0; i < pn.size(); i++)
  {
    if (st->is_piece_removed(i) == cond)
    {
      if (puzzle.getNumberOfPartGroups(piece2shape[pn[i]]) != 1)
      {
        return 0;
      }
      else if (group == 0)
      {
        group = puzzle.getPartGroupId(piece2shape[pn[i]], 0);
      }
      else if (group != puzzle.getPartGroupId(piece2shape[pn[i]], 0))
      {
        return 0;
      }
    }
  }

  return group;
}

bool disassembler_a_c::subProbGrouping(const std::vector<unsigned int> & pn) {

  groups->newSet();

  for (unsigned int i = 0; i < pn.size(); i++)
    if (!groups->addPieceToSet(piece2shape[pn[i]]))
      return false;

  return true;
}

std::unique_ptr<separation_c> disassembler_a_c::disassemble(const assembly_c * assembly) {

  bt_assert(puzzle.getNumberOfPieces() == assembly->placementCount());
  groups->reSet();

  disassemblerNode_c * start = new disassemblerNode_c(assembly);

  if (start->getPiecenumber() < 2) {
    delete start;
    return nullptr;
  }

  /* create pieces field. This field contains the
   * names of all present pieces. Because at the start
   * all pieces are still there we fill the array
   * with all the numbers
   */
  std::vector<unsigned int> pieces;
  for (unsigned int j = 0; j < assembly->placementCount(); j++)
    if (assembly->isPlaced(j))
      pieces.push_back(j);

  /* for those who watch: from here until the end this take-apart runs */
  struct running_c {
    std::atomic<unsigned long long> & startUs;
    explicit running_c(std::atomic<unsigned long long> & s) : startUs(s) {
      startUs.store(nowUs() | 1, std::memory_order_relaxed);
    }
    ~running_c() { startUs.store(0, std::memory_order_relaxed); }
  };

  progPieces.store((unsigned int)pieces.size(), std::memory_order_relaxed);
  progSeparations.store(0, std::memory_order_relaxed);
  progNodes.store(0, std::memory_order_relaxed);
  progLevel.store(0, std::memory_order_relaxed);
  progLevelDone.store(0, std::memory_order_relaxed);
  progLevelSize.store(0, std::memory_order_relaxed);
  progNextSize.store(0, std::memory_order_relaxed);

  separation_c * s;
  {
    running_c running(progStartUs);
    s = disassemble_rec(pieces, start);
  }

  if (start->decRefCount())
    delete start;

  return std::unique_ptr<separation_c>(s);
}

