#include <catch2/catch_test_macros.hpp>

#include "lib/bt_assert.h"
#include "lib/disassembly.h"
#include "lib/problem.h"
#include "lib/puzzle.h"
#include "lib/solution.h"
#include "lib/solvethread.h"
#include "lib/solvertype.h"

#include "lib/assembler.h"
#include "lib/assembly.h"
#include "lib/disassembler.h"
#include "lib/disassembler_a.h"
#include "lib/disassembler_factory.h"
#include "lib/gridtype.h"
#include "lib/helperpool.h"
#include "tools/xml.h"

#include <atomic>
#include <chrono>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

namespace {

/* Re-solve test/test_rotation_solvers.xmpuzzle with Check Rotations on.
   The file already stores one rotation solution; that copy is cleared
   first so the assertions describe what this solver finds, not what
   was baked into the file. Ordinary fixture loads do not do this:
   opening a puzzle does not filter rotated assemblies. */
void requireEnigmaRotationSolution(solverType_e type) {
  std::unique_ptr<puzzle_c> puzzle = puzzle_c::load("test/test_rotation_solvers.xmpuzzle");
  REQUIRE(puzzle != nullptr);
  problem_c * pr = puzzle->getProblem(0);
  REQUIRE(pr != nullptr);
  pr->removeAllSolutions();

  const int par = solveThread_c::PAR_REDUCE
                | solveThread_c::PAR_DISASSM
                | solveThread_c::PAR_CHECK_ROTATIONS;
  solveThread_c solver(*pr, par);
  solver.setSolverType(type);

  const auto t0 = std::chrono::steady_clock::now();
  REQUIRE(solver.start());
  solver.waitUntilFinished();
  const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                      std::chrono::steady_clock::now() - t0).count();

  if (solver.currentAction() == solveThread_c::ACT_ASSERT) {
    const assert_exception & e = solver.getAssertException();
    FAIL(std::string(solverTypeLabel(type)) + " asserted at " + e.file + ":"
         + std::to_string(e.line) + " " + e.what());
  }
  if (solver.currentAction() == solveThread_c::ACT_ERROR) {
    FAIL(std::string(solverTypeLabel(type)) + " solver error "
         + std::to_string(solver.getErrorState()));
  }
  REQUIRE(solver.currentAction() == solveThread_c::ACT_FINISHED);

  REQUIRE(pr->getNumberOfSavedSolutions() == 1);
  const separation_c * tree = pr->getSavedSolution(0)->getDisassembly();
  REQUIRE(tree != nullptr);

  /* 3R1.1R1.3R0.2R1 is 9 slides and 3 rotations, 12 steps in total. */
  CHECK(tree->movesText() == "3R1.1R1.3R0.2R1");
  CHECK(tree->sumMoves() == 9);
  CHECK(tree->sumRotations() == 3);
  CHECK(tree->sumSteps() == 12);
  CHECK(ms < 1000);
}

} // namespace

TEST_CASE("EnigmaTIC with Check Rotations: Classic, Crowell, and BurrTools 2 agree",
          "[solver][rotations]") {
  SECTION("BurrTools Classic") { requireEnigmaRotationSolution(SOLVER_CLASSIC); }
  SECTION("Andrew Crowell")   { requireEnigmaRotationSolution(SOLVER_CROWELL); }
  SECTION("BurrTools 2")      { requireEnigmaRotationSolution(SOLVER_BT2); }
}

#include "lib/rotationrules.h"

#include <chrono>
#include <cstdio>
#include <random>

namespace {

/* Every rotation rule answer for many random but consistent cases: a small
 * piece and the cells it turns into about a pivot, among random other
 * cells. The hash pins the rules' behaviour, so a faster implementation
 * must give exactly the same answers. */
uint64_t rotationRulesHash(unsigned int cases, double * seconds) {
  using cell_t = rotationRules_c::cell_t;
  std::mt19937 rng(12345);
  rotationRules_c rules;
  uint64_t h = 1469598103934665603ull;
  const auto t0 = std::chrono::steady_clock::now();
  for (unsigned int k = 0; k < cases; k++) {
    /* A piece: 2-5 cells around a centre cell. */
    const int cx = 4, cy = 4, cz = 4;
    std::vector<cell_t> piece{cell_t(cx, cy, cz)};
    const int n = 2 + (int)(rng() % 4);
    while ((int)piece.size() < n) {
      const cell_t & b = piece[rng() % piece.size()];
      static const int d[6][3] = {{1,0,0},{-1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1}};
      const int * dd = d[rng() % 6];
      cell_t c(b.x + dd[0], b.y + dd[1], b.z + dd[2]);
      bool have = false;
      for (const cell_t & p : piece)
        have = have || (p.x == c.x && p.y == c.y && p.z == c.z);
      if (!have)
        piece.push_back(c);
    }
    /* Others: random cells in a 9x9x9 box, not on the piece. */
    std::vector<cell_t> others;
    const unsigned int density = 5 + rng() % 30;
    for (int x = 0; x < 9; x++)
      for (int y = 0; y < 9; y++)
        for (int z = 0; z < 9; z++) {
          bool onPiece = false;
          for (const cell_t & p : piece)
            onPiece = onPiece || (p.x == x && p.y == y && p.z == z);
          if (!onPiece && rng() % 100 < density)
            others.push_back(cell_t(x, y, z));
        }
    /* Turn about the centre cell, every axis and sense. */
    const rotationRules_c::pivot_t pivot(2 * cx, 2 * cy, 2 * cz);
    for (unsigned int axis = 0; axis < 3; axis++)
      for (unsigned int sense = 0; sense < 2; sense++) {
        std::vector<cell_t> end;
        for (const cell_t & p : piece) {
          int dx = p.x - cx, dy = p.y - cy, dz = p.z - cz, ex = dx, ey = dy, ez = dz;
          const int s = sense ? -1 : 1;
          if (axis == 0) { ey = -s * dz; ez = s * dy; }
          else if (axis == 1) { ex = s * dz; ez = -s * dx; }
          else { ex = -s * dy; ey = s * dx; }
          end.push_back(cell_t(cx + ex, cy + ey, cz + ez));
        }
        const bool ok = rules.allowRotation(others, piece, end, pivot, axis, sense);
        const bool blocked = rules.axisBlocked(others, piece, axis);
        h = (h ^ (ok ? 3u : 1u) ^ (blocked ? 8u : 4u)) * 1099511628211ull;
      }
  }
  *seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
  return h;
}

} // namespace

TEST_CASE("rotation rules: the same answers as before (hash)", "[rotation]") {
  double s = 0;
  const uint64_t h = rotationRulesHash(400, &s);
  INFO("hash " << h);
  CHECK(h == 3649067536594071595ull);
}

/* The prepared path (setBodies, preparedAxisBlocked, allowPrepared) that the
 * Classic generator uses must say what allowRotation says, for pivots on
 * and off the grid. */
TEST_CASE("rotation rules: the prepared path agrees with allowRotation", "[rotation]") {
  using cell_t = rotationRules_c::cell_t;
  std::mt19937 rng(777);
  rotationRules_c rules, reference;
  unsigned int allowed = 0, mismatches = 0;
  for (unsigned int k = 0; k < 600; k++) {
    std::vector<cell_t> piece{cell_t(4, 4, 4)};
    const int n = 2 + (int)(rng() % 5);
    while ((int)piece.size() < n) {
      const cell_t & b = piece[rng() % piece.size()];
      static const int d[6][3] = {{1,0,0},{-1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1}};
      const int * dd = d[rng() % 6];
      cell_t c(b.x + dd[0], b.y + dd[1], b.z + dd[2]);
      bool have = false;
      for (const cell_t & p : piece)
        have = have || (p.x == c.x && p.y == c.y && p.z == c.z);
      if (!have)
        piece.push_back(c);
    }
    std::vector<cell_t> others;
    const unsigned int density = 2 + rng() % 25;
    for (int x = 0; x < 9; x++)
      for (int y = 0; y < 9; y++)
        for (int z = 0; z < 9; z++) {
          bool onPiece = false;
          for (const cell_t & p : piece)
            onPiece = onPiece || (p.x == x && p.y == y && p.z == z);
          if (!onPiece && rng() % 100 < density)
            others.push_back(cell_t(x, y, z));
        }

    rules.setBodies(others, piece);
    for (unsigned int axis = 0; axis < 3; axis++) {
      const bool blocked = rules.preparedAxisBlocked(axis);
      if (blocked != reference.axisBlocked(others, piece, axis))
        mismatches++;
      for (int tries = 0; tries < 6; tries++) {
        const rotationRules_c::pivot_t pivot(5 + (int)(rng() % 7), 5 + (int)(rng() % 7),
                                             5 + (int)(rng() % 7));
        for (unsigned int sense = 0; sense < 2; sense++) {
          std::vector<cell_t> end;
          bool onGrid = true;
          for (const cell_t & p : piece) {
            cell_t e;
            onGrid = onGrid && rotationRules_c::rotateCell(p, pivot, axis, sense, e);
            end.push_back(e);
          }
          const bool want = onGrid && reference.allowRotation(others, piece, end, pivot, axis, sense);
          if (rules.allowPrepared(pivot, axis, sense, false) != want)
            mismatches++;
          /* A blocked axis allows nothing, which is what lets the generator
           * skip its pivots. */
          if (blocked && want)
            mismatches++;
          if (!blocked && rules.allowPrepared(pivot, axis, sense, true) != want)
            mismatches++;
          if (want)
            allowed++;
        }
      }
    }
  }
  CHECK(mismatches == 0);
  /* The cases must exercise both answers. */
  CHECK(allowed > 100);
}

/* Hidden: time the rules on many cases. */
TEST_CASE("rotation rules: benchmark", "[.bench][rotation]") {
  double s = 0;
  const uint64_t h = rotationRulesHash(20000, &s);
  printf("rotation rules: 20000 cases in %.2f s, hash %llu\n", s, (unsigned long long)h);
}

/* The rules must not care how the whole scene is turned in space: a piece
 * among others, and the same scene turned a quarter about Z (which turns
 * the X axis into Y and Y into -X), get the same answer for the matching
 * rotation. The Classic generator's one-node-per-arrangement search depends
 * on it. */
TEST_CASE("rotation rules: a turned scene gets the same answers", "[rotation]") {
  using cell_t = rotationRules_c::cell_t;
  std::mt19937 rng(4242);
  rotationRules_c a, b;
  unsigned int differ = 0, allowed = 0;
  auto turned = [](const cell_t & c) { return cell_t(-c.y - 1, c.x, c.z); };
  for (unsigned int k = 0; k < 500; k++) {
    std::vector<cell_t> piece{cell_t(4, 4, 4)};
    const int n = 2 + (int)(rng() % 6);
    while ((int)piece.size() < n) {
      const cell_t & from = piece[rng() % piece.size()];
      static const int d[6][3] = {{1,0,0},{-1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1}};
      const int * dd = d[rng() % 6];
      cell_t c(from.x + dd[0], from.y + dd[1], from.z + dd[2]);
      bool have = false;
      for (const cell_t & p : piece)
        have = have || (p.x == c.x && p.y == c.y && p.z == c.z);
      if (!have)
        piece.push_back(c);
    }
    std::vector<cell_t> others;
    const unsigned int density = 2 + rng() % 20;
    for (int x = 0; x < 9; x++)
      for (int y = 0; y < 9; y++)
        for (int z = 0; z < 9; z++) {
          bool onPiece = false;
          for (const cell_t & p : piece)
            onPiece = onPiece || (p.x == x && p.y == y && p.z == z);
          if (!onPiece && rng() % 100 < density)
            others.push_back(cell_t(x, y, z));
        }
    std::vector<cell_t> piece2, others2;
    for (const cell_t & c : piece)
      piece2.push_back(turned(c));
    for (const cell_t & c : others)
      others2.push_back(turned(c));
    a.setBodies(others, piece);
    b.setBodies(others2, piece2);
    for (unsigned int axis = 0; axis < 3; axis++)
      for (int t = 0; t < 8; t++) {
        const rotationRules_c::pivot_t pivot(5 + (int)(rng() % 7), 5 + (int)(rng() % 7),
                                             5 + (int)(rng() % 7));
        const rotationRules_c::pivot_t pivot2(-pivot.hy - 2, pivot.hx, pivot.hz);
        for (unsigned int sense = 0; sense < 2; sense++) {
          const unsigned int axis2 = axis == 0 ? 1 : axis == 1 ? 0 : 2;
          const unsigned int sense2 = axis == 1 ? 1 - sense : sense;
          const bool ra = a.allowPrepared(pivot, axis, sense, false);
          if (ra)
            allowed++;
          if (ra != b.allowPrepared(pivot2, axis2, sense2, false))
            differ++;
        }
      }
  }
  CHECK(differ == 0);
  CHECK(allowed > 1000);
}

namespace {

/* every assembly of problem 0 */
std::vector<std::unique_ptr<assembly_c>> allAssemblies(puzzle_c & puzzle) {
  problem_c * pr = puzzle.getProblem(0);
  std::unique_ptr<assembler_c> assm = puzzle.getGridType()->findAssembler(*pr, true, SOLVER_CLASSIC);
  REQUIRE(assm != nullptr);
  assm->setNumThreads(1);
  REQUIRE(assm->createMatrix(false, false, false) == assembler_c::ERR_NONE);
  assm->reduce();
  std::vector<std::unique_ptr<assembly_c>> res;
  assm->assemble([&res](std::unique_ptr<assembly_c> a) {
    res.push_back(std::move(a));
    return true;
  });
  return res;
}

/* a disassembly as it would be saved, or "none" */
std::string disassemblyText(const separation_c * s) {
  if (!s)
    return "none";
  std::ostringstream str;
  {
    xmlWriter_c xml(str);
    s->save(xml, 0, true);
  }
  return str.str();
}

/* resets the threshold a test has changed */
struct levelThreadCost_c {
  explicit levelThreadCost_c(double us) { disassembler_a_c::setLevelThreadCostUs(us); }
  ~levelThreadCost_c() { disassembler_a_c::setLevelThreadCostUs(400.0); }
};

/* Take every assembly apart on one thread and with every level spread over
 * four, and require the very same disassembly from both. */
void requireSameOnThreads(const char * file, bool rotations, solverType_e type, bool comesApart) {
  INFO(file << ", " << solverTypeLabel(type) << (rotations ? ", rotations" : ""));

  std::unique_ptr<puzzle_c> puzzle = puzzle_c::load(file);
  REQUIRE(puzzle != nullptr);
  problem_c * pr = puzzle->getProblem(0);
  REQUIRE(pr != nullptr);
  pr->removeAllSolutions();

  std::vector<std::unique_ptr<assembly_c>> assemblies = allAssemblies(*puzzle);
  REQUIRE(!assemblies.empty());

  helperPool_c pool(4);
  levelThreadCost_c always(0);

  std::unique_ptr<disassembler_c> serial = createDisassembler(*pr, rotations, type);
  std::unique_ptr<disassembler_c> spread = createDisassembler(*pr, rotations, type);
  REQUIRE(serial != nullptr);
  REQUIRE(spread != nullptr);
  spread->setHelperPool(&pool);

  unsigned int found = 0;
  for (size_t i = 0; i < assemblies.size() && i < 40; i++) {
    INFO("assembly " << i);
    std::unique_ptr<separation_c> a = serial->disassemble(assemblies[i].get());
    std::unique_ptr<separation_c> b = spread->disassemble(assemblies[i].get());
    CHECK(disassemblyText(a.get()) == disassemblyText(b.get()));
    if (a)
      found++;
  }
  CHECK((found > 0) == comesApart);
}

} // namespace

TEST_CASE("a take-apart spread over threads gives the same disassembly as on one",
          "[disassembler][threads]") {
  for (solverType_e type : {SOLVER_CLASSIC, SOLVER_CROWELL}) {
    requireSameOnThreads("test/test_rotation_solvers.xmpuzzle", true, type, true);
    /* without rotations the same puzzle does not come apart: the whole
     * search is gone through, on any number of threads */
    requireSameOnThreads("test/test_rotation_solvers.xmpuzzle", false, type, false);
    requireSameOnThreads("examples/PelikanBurr.xmpuzzle", false, type, true);
  }
}

TEST_CASE("a take-apart says how far it is, and stops when told to",
          "[disassembler][threads][progress]") {
  std::unique_ptr<puzzle_c> puzzle = puzzle_c::load("test/test_rotation_solvers.xmpuzzle");
  REQUIRE(puzzle != nullptr);
  problem_c * pr = puzzle->getProblem(0);
  pr->removeAllSolutions();
  std::vector<std::unique_ptr<assembly_c>> assemblies = allAssemblies(*puzzle);
  REQUIRE(!assemblies.empty());

  helperPool_c pool(4);
  levelThreadCost_c always(0);

  SECTION("progress is there while it runs and gone afterwards") {
    std::unique_ptr<disassembler_c> d = createDisassembler(*pr, true, SOLVER_CLASSIC);
    d->setHelperPool(&pool);

    disassemblyProgress_c p;
    REQUIRE(d->getProgress(p));
    CHECK(!p.active);

    std::atomic<bool> done{false};
    std::atomic<unsigned long long> mostNodes{0};
    std::atomic<unsigned int> mostPieces{0};
    std::thread watcher([&]() {
      while (!done.load()) {
        disassemblyProgress_c q;
        if (d->getProgress(q) && q.active) {
          if (q.nodes > mostNodes.load()) mostNodes.store(q.nodes);
          if (q.pieces > mostPieces.load()) mostPieces.store(q.pieces);
          CHECK(q.levelDone <= q.levelSize);
          CHECK(q.separations < q.pieces);
        }
        std::this_thread::yield();
      }
    });

    /* The node count goes up only as a batch is done and back to 0 when the
     * next take-apart starts, so a watcher that gets little time (a busy CI
     * machine) can miss every moment it is above 0. Go round again until it
     * has seen one. */
    bool any = false;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(30);
    do {
      for (size_t i = 0; i < assemblies.size(); i++)
        if (d->disassemble(assemblies[i].get()))
          any = true;
    } while (mostNodes.load() == 0 && std::chrono::steady_clock::now() < deadline);
    done.store(true);
    watcher.join();

    CHECK(any);
    CHECK(mostNodes.load() > 0);
    CHECK(mostPieces.load() == pr->getNumberOfPieces());
    REQUIRE(d->getProgress(p));
    CHECK(!p.active);
  }

  SECTION("a stopped take-apart gives no disassembly") {
    for (solverType_e type : {SOLVER_CLASSIC, SOLVER_CROWELL}) {
      std::unique_ptr<disassembler_c> d = createDisassembler(*pr, true, type);
      d->setHelperPool(&pool);
      d->stop();
      for (size_t i = 0; i < assemblies.size(); i++)
        CHECK(d->disassemble(assemblies[i].get()) == nullptr);
    }
  }
}

/* The picture of a running solve can be asked for at any time; the overall
 * figure never goes back and ends at 1. */
TEST_CASE("solve progress: asked for all through a solve, it only goes forward",
          "[solver][rotations][progress][threads]") {
  for (solverType_e type : {SOLVER_CLASSIC, SOLVER_CROWELL, SOLVER_BT2}) {
    INFO(solverTypeLabel(type));
    std::unique_ptr<puzzle_c> puzzle = puzzle_c::load("test/test_rotation_solvers.xmpuzzle");
    REQUIRE(puzzle != nullptr);
    problem_c * pr = puzzle->getProblem(0);
    pr->removeAllSolutions();

    levelThreadCost_c always(0);

    solveThread_c solver(*pr, solveThread_c::PAR_REDUCE | solveThread_c::PAR_DISASSM |
                              solveThread_c::PAR_CHECK_ROTATIONS);
    solver.setSolverType(type);
    REQUIRE(solver.start());

    float last = 0;
    unsigned int asked = 0;
    for (;;) {
      const unsigned int act = solver.currentAction();
      if (act == solveThread_c::ACT_FINISHED || act == solveThread_c::ACT_PAUSING ||
          act == solveThread_c::ACT_ERROR || act == solveThread_c::ACT_ASSERT)
        break;
      const solveProgress_c p = solver.getProgressSnapshot();
      CHECK(p.overall >= last);
      CHECK(p.overall <= 1.0f);
      CHECK(p.levelFraction <= 1.0f);
      /* the line is made without trouble whatever the stage */
      (void)p.activity();
      (void)solver.getStats();
      last = p.overall;
      asked++;
      std::this_thread::yield();
    }
    solver.waitUntilFinished();

    REQUIRE(solver.currentAction() == solveThread_c::ACT_FINISHED);
    const solveProgress_c end = solver.getProgressSnapshot();
    CHECK(end.stage == solveProgress_c::STAGE_DONE);
    CHECK(end.overall == 1.0f);
    CHECK(end.activity() == "finished");
    CHECK(end.running.empty());
    CHECK(asked > 0);
    CHECK(pr->getNumberOfSavedSolutions() == 1);
  }
}
