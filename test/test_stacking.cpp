#include "catch2/catch_test_macros.hpp"

#include "lib/assembly.h"
#include "lib/disassembly.h"
#include "lib/gridtype.h"
#include "lib/problem.h"
#include "lib/puzzle.h"
#include "lib/solution.h"
#include "lib/solvethread.h"
#include "lib/stacking.h"
#include "lib/panex.h"
#include "lib/blockpack.h"
#include "lib/voxel.h"
#include "gui/shapehistory.h"

#include "tools/xml.h"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <algorithm>
#include <random>
#include <memory>
#include <sstream>

using namespace stacking;

/* Every folder these tests write goes under one scratch folder of this
 * process, removed at exit. Fixed names in the temp folder let two test
 * runs at once (CI's meson and a rerun, or parallel runs on a desktop)
 * wipe each other's saved searches. A search without a workDir saves to
 * BURRTOOLS_PANEX_DIR, so that points in here too, not at the user's cache. */
namespace {
class scratchRoot_c {
  public:
    scratchRoot_c(void) {
      std::random_device rd;
      root = std::filesystem::temp_directory_path() /
             ("burrtools-stacking-" + std::to_string(rd()) + "-" +
              std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
      std::filesystem::create_directories(root);
      const std::string cache = (root / "cache").string();
#ifdef _WIN32
      (void)_putenv_s("BURRTOOLS_PANEX_DIR", cache.c_str());
#else
      setenv("BURRTOOLS_PANEX_DIR", cache.c_str(), 1);
#endif
    }
    ~scratchRoot_c(void) {
      std::error_code ec;
      std::filesystem::remove_all(root, ec);
    }
    std::filesystem::path root;
};
const scratchRoot_c scratchRoot;

std::filesystem::path scratch(const char * name) { return scratchRoot.root / name; }
}

static puzzle_c makeBoard(void) {
  return puzzle_c(new gridType_c(gridType_c::GT_STACKING));
}

static void addSized(puzzle_c & puz, problem_c & prob, unsigned int size) {
  unsigned int id = addDisk(puz, size);
  prob.setShapeMaximum(id, 1);
  prob.setShapeMinimum(id, 1);
}

TEST_CASE("stacking: a disc is one voxel thick and scales with its size", "[stacking]") {
  puzzle_c puz = makeBoard();
  unsigned int id = addDisk(puz, 2);
  const voxel_c * v = puz.getShape(id);
  REQUIRE(v->getDiskSize() == 2);
  REQUIRE(v->getZ() == 1);
  REQUIRE(v->getX() == 5);
  REQUIRE(v->getY() == 5);
  REQUIRE(!v->isFilled(2, 2, 0));
  REQUIRE(v->isFilled(0, 0, 0));
  REQUIRE(v->isFilled(1, 2, 0));

  generateDisk(puz.getShape(id), 1);
  REQUIRE(puz.getShape(id)->getX() == 3);
  REQUIRE(puz.getShape(id)->getY() == 3);
  REQUIRE(!puz.getShape(id)->isFilled(1, 1, 0));
  REQUIRE(puz.getShape(id)->isFilled(0, 0, 0));
  REQUIRE(puz.getShape(id)->isFilled(0, 1, 0));

  generateDisk(puz.getShape(id), 2);
  unsigned int other = addDisk(puz, 2);
  REQUIRE(puz.getShape(id)->getX() == puz.getShape(other)->getX());
  REQUIRE(puz.getShape(id)->getDiskSize() == puz.getShape(other)->getDiskSize());
}

TEST_CASE("stacking: three-disc Hanoi is seven transfers", "[stacking]") {
  puzzle_c puz = makeBoard();
  REQUIRE(puz.rodSetCount() == 1);
  problem_c * pr = puz.getProblem(puz.addProblem());
  addSized(puz, *pr, 3);
  addSized(puz, *pr, 2);
  addSized(puz, *pr, 1);
  pr->setRodSetId(0);
  syncMaps(*pr);

  /* Largest on the bottom of rod 0. Goal is the same tower on the last rod. */
  REQUIRE(placeDisk(*pr, false, 0, 0).empty());
  REQUIRE(placeDisk(*pr, false, 1, 0).empty());
  REQUIRE(placeDisk(*pr, false, 2, 0).empty());
  REQUIRE(placeDisk(*pr, true, 0, 2).empty());
  REQUIRE(placeDisk(*pr, true, 1, 2).empty());
  REQUIRE(placeDisk(*pr, true, 2, 2).empty());
  REQUIRE(setupError(*pr).empty());

  std::unique_ptr<separation_c> path = findStackPath(*pr);
  REQUIRE(path);
  REQUIRE(logicalMoves(*path) == 7);
  REQUIRE(path->getMoves() == 21);
}

TEST_CASE("stacking: distance limits a jump to the neighbouring rod", "[stacking]") {
  puzzle_c puz = makeBoard();
  rodSet_c & board = puz.getRodSet(0);
  board.sizeMatters = false;
  board.distanceMatters = true;
  board.canMoveOver = false;

  problem_c * pr = puz.getProblem(puz.addProblem());
  addSized(puz, *pr, 1);
  pr->setRodSetId(0);
  syncMaps(*pr);
  REQUIRE(placeDisk(*pr, false, 0, 0).empty());
  REQUIRE(placeDisk(*pr, true, 0, 2).empty());

  std::unique_ptr<separation_c> path = findStackPath(*pr);
  REQUIRE(path);
  REQUIRE(logicalMoves(*path) == 2);

  board.canMoveOver = true;
  path = findStackPath(*pr);
  REQUIRE(path);
  REQUIRE(logicalMoves(*path) == 1);
}

TEST_CASE("stacking: a full rod and a larger disc are refused", "[stacking]") {
  puzzle_c puz = makeBoard();
  rodSet_c & board = puz.getRodSet(0);
  board.growHeight = false;
  board.definedHeight = 1;
  board.sizeMatters = false;

  problem_c * pr = puz.getProblem(puz.addProblem());
  addSized(puz, *pr, 1);
  addSized(puz, *pr, 1);
  pr->setRodSetId(0);
  syncMaps(*pr);

  REQUIRE(placeDisk(*pr, false, 0, 0).empty());
  REQUIRE_FALSE(placeDisk(*pr, false, 1, 0).empty());

  board.growHeight = true;
  board.sizeMatters = true;
  liftTop(*pr, false, 0);
  unsigned int small = addDisk(puz, 1);
  unsigned int large = addDisk(puz, 3);
  pr->setShapeMaximum(small, 1);
  pr->setShapeMinimum(small, 1);
  pr->setShapeMaximum(large, 1);
  pr->setShapeMinimum(large, 1);
  REQUIRE(placeDisk(*pr, false, small, 0).empty());
  REQUIRE_FALSE(placeDisk(*pr, false, large, 0).empty());
  REQUIRE(placeDisk(*pr, false, large, 1).empty());
}

TEST_CASE("stacking: an unenforced placement is kept and setupError names the rule", "[stacking]") {
  puzzle_c puz = makeBoard();
  problem_c * pr = puz.getProblem(puz.addProblem());
  addSized(puz, *pr, 1);
  addSized(puz, *pr, 3);
  pr->setRodSetId(0);
  syncMaps(*pr);

  /* Size 3 on size 1 breaks the size rule, but the editor may build it. */
  REQUIRE(placeDisk(*pr, false, 0, 0, false).empty());
  REQUIRE(placeDisk(*pr, false, 1, 0, false).empty());
  REQUIRE(pr->startStacks().rods[0].size() == 2);
  std::string err = setupError(*pr);
  REQUIRE(err.find("rod 1 of the start") != std::string::npos);
  REQUIRE(err.find("smaller") != std::string::npos);

  /* Turning the rule off makes the start legal; the goal is still empty. */
  puz.getRodSet(0).sizeMatters = false;
  err = setupError(*pr);
  REQUIRE(err.find("the goal is missing S1 and S2") != std::string::npos);

  REQUIRE(placeDisk(*pr, true, 0, 2).empty());
  REQUIRE(placeDisk(*pr, true, 1, 2).empty());
  REQUIRE(setupError(*pr).empty());

  /* A fixed height lower than the stack is reported, not cleared. */
  puz.getRodSet(0).growHeight = false;
  puz.getRodSet(0).definedHeight = 1;
  err = setupError(*pr);
  REQUIRE(err.find("height") != std::string::npos);
  REQUIRE(pr->startStacks().rods[0].size() == 2);
}

TEST_CASE("stacking: start and goal must use the same discs", "[stacking]") {
  puzzle_c puz = makeBoard();
  puz.getRodSet(0).sizeMatters = false;
  problem_c * pr = puz.getProblem(puz.addProblem());
  unsigned int a = addDisk(puz, 1);
  unsigned int b = addDisk(puz, 2);
  pr->setRodSetId(0);
  syncMaps(*pr);

  /* Two copies of a on the start, one copy of a and one b on the goal. */
  pr->setShapeMaximum(a, 2);
  pr->setShapeMinimum(a, 2);
  pr->setShapeMaximum(b, 1);
  pr->setShapeMinimum(b, 1);
  REQUIRE(placeDisk(*pr, false, a, 0).empty());
  REQUIRE(placeDisk(*pr, false, a, 0).empty());
  REQUIRE(placeDisk(*pr, true, a, 1).empty());
  REQUIRE(placeDisk(*pr, true, b, 1).empty());

  std::string err = setupError(*pr);
  REQUIRE(err == "The start and goal use different discs: the goal is missing S1; "
                 "the start is missing S2.");

  REQUIRE(placeDisk(*pr, true, a, 2).empty());
  REQUIRE(placeDisk(*pr, false, b, 2).empty());
  REQUIRE(setupError(*pr).empty());

  /* Emptying both sides leaves discs counted in the problem but placed nowhere. */
  for (auto & rod : pr->editStart().rods) rod.clear();
  for (auto & rod : pr->editGoal().rods) rod.clear();
  err = setupError(*pr);
  REQUIRE(err.find("on neither the start nor the goal") != std::string::npos);
}

TEST_CASE("stacking: lowering the rod count keeps the discs on the dropped rods", "[stacking]") {
  puzzle_c puz = makeBoard();
  puz.getRodSet(0).sizeMatters = false;
  problem_c * pr = puz.getProblem(puz.addProblem());
  addSized(puz, *pr, 1);
  pr->setRodSetId(0);
  syncMaps(*pr);
  REQUIRE(placeDisk(*pr, false, 0, 0).empty());
  REQUIRE(placeDisk(*pr, true, 0, 2).empty());
  REQUIRE(setupError(*pr).empty());

  puz.getRodSet(0).rodCount = 2;
  syncMaps(*pr);
  REQUIRE(pr->goalStacks().rods.size() == 3);
  REQUIRE(pr->startStacks().rods.size() == 2);
  std::string err = setupError(*pr);
  REQUIRE(err.find("rod 3") != std::string::npos);
  REQUIRE_FALSE(findStackPath(*pr));

  puz.getRodSet(0).rodCount = 3;
  syncMaps(*pr);
  REQUIRE(setupError(*pr).empty());
  REQUIRE(pr->goalStacks().rods[2].size() == 1);
}

TEST_CASE("stacking: rod sets and disc size survive a save and load", "[stacking]") {
  puzzle_c puz = makeBoard();
  puz.getRodSet(0).name = "Hanoi";
  puz.getRodSet(0).distanceMatters = true;
  unsigned int id = addDisk(puz, 4);
  puz.getShape(id)->setName("large");

  std::stringstream saved;
  {
    xmlWriter_c xml(saved);
    puz.save(xml);
  }
  std::stringstream in(saved.str());
  xmlParser_c pars(in);
  puzzle_c loaded(pars);

  REQUIRE(loaded.getGridType()->getType() == gridType_c::GT_STACKING);
  REQUIRE(loaded.rodSetCount() == 1);
  REQUIRE(loaded.getRodSet(0).name == "Hanoi");
  REQUIRE(loaded.getRodSet(0).distanceMatters);
  REQUIRE(loaded.getNumberOfShapes() == 1);
  REQUIRE(loaded.getShape(0)->getDiskSize() == 4);
  REQUIRE(loaded.getShape(0)->getName() == "large");
  REQUIRE(loaded.getShape(0)->getZ() == 1);
}

TEST_CASE("stacking: the Hanoi fixture solves in seven transfers", "[stacking][solver]") {
  /* test/test_stacking_hanoi.xmpuzzle: three discs, largest at the bottom,
   * on the first of three rods; the goal is the same tower on the last rod. */
  std::unique_ptr<puzzle_c> puzzle = puzzle_c::load("test/test_stacking_hanoi.xmpuzzle");
  REQUIRE(puzzle != nullptr);
  REQUIRE(isStacking(*puzzle));
  problem_c * pr = puzzle->getProblem(0);
  REQUIRE(setupError(*pr).empty());

  solveThread_c solver(*pr, solveThread_c::PAR_DISASSM);
  REQUIRE(solver.start());
  solver.waitUntilFinished();
  REQUIRE(solver.currentAction() == solveThread_c::ACT_FINISHED);
  REQUIRE(pr->getNumSolutions() == 1);
  const separation_c * path = pr->getSavedSolution(0)->getDisassembly();
  REQUIRE(path != nullptr);
  CHECK(logicalMoves(*path) == 7);
  CHECK(path->getMoves() == 7 * STEPS_PER_MOVE);
}

/* Panex Jr: six discs on two Panex columns six deep, plus a one-deep pocket
 * beside the first. A disc of size s goes at most s places down, so the
 * tower starts with size 6 at the bottom and size 1 at the top. The same
 * puzzle as a sliding tray takes 143 moves, and the stacking model
 * reaches exactly the same resting arrangements. */
static problem_c * makePanexJr(puzzle_c & puz) {
  rodSet_c & board = puz.getRodSet(0);
  board.rodCount = 2;
  board.growHeight = false;
  board.definedHeight = 6;
  board.sizeMatters = false;
  board.panexColumns = true;
  board.pocketColumn = true;
  board.pocketHeight = 1;
  problem_c * pr = puz.getProblem(puz.addProblem());
  for (unsigned int size = 6; size >= 1; size--)
    addSized(puz, *pr, size);
  pr->setRodSetId(0);
  syncMaps(*pr);
  for (unsigned int id = 0; id < 6; id++) {
    REQUIRE(placeDisk(*pr, false, id, 0).empty());
    REQUIRE(placeDisk(*pr, true, id, 1).empty());
  }
  return pr;
}

TEST_CASE("stacking: Panex Jr with a pocket column is 143 transfers", "[stacking][solver]") {
  puzzle_c puz = makeBoard();
  problem_c * pr = makePanexJr(puz);
  REQUIRE(totalRods(puz.getRodSet(0)) == 3);
  REQUIRE(isPocket(puz.getRodSet(0), 2));
  REQUIRE(setupError(*pr).empty());

  stackSearch_c search;
  std::unique_ptr<separation_c> path = findStackPath(*pr, search);
  REQUIRE(path);
  CHECK(search.outcome == STACK_FOUND);
  CHECK(logicalMoves(*path) == 143);

  /* Without the pocket the tower cannot move at all. */
  puz.getRodSet(0).pocketColumn = false;
  syncMaps(*pr);
  REQUIRE(setupError(*pr).empty());
  stackSearch_c none;
  CHECK_FALSE(findStackPath(*pr, none));
  CHECK(none.outcome == STACK_NO_PATH);
}

TEST_CASE("stacking: a Panex column limits how deep a disc goes", "[stacking]") {
  puzzle_c puz = makeBoard();
  problem_c * pr = makePanexJr(puz);

  /* Ids 0..5 are sizes 6..1. Free the size 1 and size 2 discs. */
  REQUIRE(liftTop(*pr, false, 0).empty());
  REQUIRE(liftTop(*pr, false, 0).empty());

  /* The pocket takes any one disc; a second does not fit. */
  REQUIRE(placeDisk(*pr, false, 4, 2).empty());
  REQUIRE(liftTop(*pr, false, 0).empty());
  CHECK(placeDisk(*pr, false, 3, 2) == "That rod is full");

  /* On rod 2 the size 1 disc may go one down, with the size 3 disc raised
   * into the bridge above it, but not two down. */
  REQUIRE(placeDisk(*pr, false, 5, 1).empty());
  REQUIRE(placeDisk(*pr, false, 3, 1).empty());
  REQUIRE(liftTop(*pr, false, 0).empty());
  CHECK_FALSE(placeDisk(*pr, false, 2, 1).empty());

  /* An unenforced placement is kept, and setupError names the rule. */
  REQUIRE(placeDisk(*pr, false, 2, 1, false).empty());
  CHECK(setupError(*pr).find("Panex column") != std::string::npos);
}

TEST_CASE("stacking: Panex discs rest as low as they can go", "[stacking]") {
  puzzle_c puz = makeBoard();
  problem_c * pr = makePanexJr(puz);

  /* Ids 0..5 are sizes 6..1. Take the size 1 and size 2 discs off: the
   * rest stay at the bottom of rod 1 instead of rising to the bridge. */
  REQUIRE(liftTop(*pr, false, 0).empty());
  REQUIRE(liftTop(*pr, false, 0).empty());
  /* On rod 2 the size 2 disc falls two below the bridge, and the size 1
   * disc on it rests one below. */
  REQUIRE(placeDisk(*pr, false, 4, 1).empty());
  REQUIRE(placeDisk(*pr, false, 5, 1).empty());

  const boardLayout_c lay = layoutBoard(*pr, false);
  for (unsigned int id = 0; id < 4; id++)
    CHECK(lay.disks[id].z == (int)id);
  CHECK(lay.disks[4].z == 4);
  CHECK(lay.disks[5].z == 5);
}

TEST_CASE("stacking: Panex settings survive a save and load", "[stacking]") {
  puzzle_c puz = makeBoard();
  puz.getRodSet(0).panexColumns = true;
  puz.getRodSet(0).pocketColumn = true;
  puz.getRodSet(0).pocketHeight = 2;
  puz.getRodSet(0).outsideChannel = true;

  std::stringstream saved;
  {
    xmlWriter_c xml(saved);
    puz.save(xml);
  }
  std::stringstream in(saved.str());
  xmlParser_c pars(in);
  puzzle_c loaded(pars);
  REQUIRE(loaded.rodSetCount() == 1);
  CHECK(loaded.getRodSet(0).panexColumns);
  CHECK(loaded.getRodSet(0).pocketColumn);
  CHECK(loaded.getRodSet(0).pocketHeight == 2);
  CHECK(loaded.getRodSet(0).outsideChannel);
}

/* The classic Panex swap: three Panex columns n deep, a tower of n discs on
 * each outer column, and the goal is the two towers swapped. */
static problem_c * makePanexSwap(puzzle_c & puz, unsigned int n) {
  rodSet_c & board = puz.getRodSet(0);
  board.rodCount = 3;
  board.growHeight = false;
  board.definedHeight = n;
  board.sizeMatters = false;
  board.panexColumns = true;
  problem_c * pr = puz.getProblem(puz.addProblem());
  for (unsigned int side = 0; side < 2; side++)
    for (unsigned int size = n; size >= 1; size--)
      addSized(puz, *pr, size);
  pr->setRodSetId(0);
  syncMaps(*pr);
  for (unsigned int i = 0; i < n; i++) {
    REQUIRE(placeDisk(*pr, false, i, 0).empty());
    REQUIRE(placeDisk(*pr, false, n + i, 2).empty());
    REQUIRE(placeDisk(*pr, true, n + i, 0).empty());
    REQUIRE(placeDisk(*pr, true, i, 2).empty());
  }
  REQUIRE(setupError(*pr).empty());
  return pr;
}

TEST_CASE("panex: the Panex Solver finds the published swap lengths", "[stacking][panex]") {
  /* 3, 4 and 5 discs a tower swap in 42, 128 and 343 transfers. */
  const unsigned int moves[] = {42, 128, 343};
  for (unsigned int n = 3; n <= 5; n++) {
    puzzle_c puz = makeBoard();
    problem_c * pr = makePanexSwap(puz, n);
    REQUIRE(panex::unsupported(*pr).empty());
    panex::panexSearch_c search;
    std::unique_ptr<separation_c> path = panex::solve(*pr, search);
    REQUIRE(path);
    CHECK(search.outcome == panex::PANEX_FOUND);
    CHECK(logicalMoves(*path) == moves[n - 3]);

    /* Solving each half, instead of keeping every level, finds as short a path. */
    panex::panexSearch_c halves;
    halves.keepLimit = 10;
    std::unique_ptr<separation_c> halved = panex::solve(*pr, halves);
    REQUIRE(halved);
    CHECK(logicalMoves(*halved) == moves[n - 3]);
  }
}

TEST_CASE("panex: the Panex Solver agrees with the Stacking Solver", "[stacking][panex]") {
  /* Panex Jr has a pocket, so the search runs from both ends. */
  puzzle_c puz = makeBoard();
  problem_c * pr = makePanexJr(puz);
  std::unique_ptr<separation_c> stacked = findStackPath(*pr);
  REQUIRE(stacked);
  panex::panexSearch_c search;
  search.keepLimit = 50;
  std::unique_ptr<separation_c> path = panex::solve(*pr, search);
  REQUIRE(path);
  CHECK(logicalMoves(*path) == logicalMoves(*stacked));
  CHECK(logicalMoves(*path) == 143);

  /* The last frame is the goal: the same disc placements as the other solver's. */
  const state_c * a = path->getState(path->getMoves());
  const state_c * b = stacked->getState(stacked->getMoves());
  for (unsigned int i = 0; i < path->getPieceNumber(); i++) {
    CHECK(a->getX(i) == b->getX(i));
    CHECK(a->getZ(i) == b->getZ(i));
  }
}

/* The two solvers keep the Panex rules separately (stacking::moveAllowed and
 * panex rules_c); on the swap, with no pocket, discs raised into the bridge
 * block moves, so both rules are exercised. */
TEST_CASE("panex: the two solvers agree on the Panex swap", "[stacking][panex]") {
  for (unsigned int n = 2; n <= 3; n++) {
    INFO(n << " discs a tower");
    puzzle_c puz = makeBoard();
    problem_c * pr = makePanexSwap(puz, n);
    std::unique_ptr<separation_c> stacked = findStackPath(*pr);
    REQUIRE(stacked);
    panex::panexSearch_c search;
    std::unique_ptr<separation_c> path = panex::solve(*pr, search);
    REQUIRE(path);
    CHECK(logicalMoves(*path) == logicalMoves(*stacked));
  }
}

/* The outside channel lets a disc go between the outer rods while one is
 * raised on the middle rod, which makes the swap shorter; both solvers
 * keep the rule. */
TEST_CASE("panex: the outside channel shortens the Panex swap", "[stacking][panex]") {
  for (unsigned int n = 2; n <= 4; n++) {
    INFO(n << " discs a tower");
    puzzle_c puz = makeBoard();
    problem_c * pr = makePanexSwap(puz, n);
    panex::panexSearch_c plain;
    std::unique_ptr<separation_c> without = panex::solve(*pr, plain);
    REQUIRE(without);

    puz.getRodSet(0).outsideChannel = true;
    REQUIRE(hasOutsideChannel(puz.getRodSet(0)));
    panex::panexSearch_c search;
    std::unique_ptr<separation_c> path = panex::solve(*pr, search);
    REQUIRE(path);
    CHECK(logicalMoves(*path) < logicalMoves(*without));
    if (n <= 3) {
      std::unique_ptr<separation_c> stacked = findStackPath(*pr);
      REQUIRE(stacked);
      CHECK(logicalMoves(*path) == logicalMoves(*stacked));
    }
  }
}

TEST_CASE("stacking: the outside channel needs Panex columns and three rods", "[stacking]") {
  rodSet_c board;
  board.panexColumns = true;
  board.outsideChannel = true;
  board.rodCount = 4;
  CHECK(hasOutsideChannel(board));
  CHECK(viaOutsideChannel(board, 0, 3));
  CHECK(viaOutsideChannel(board, 3, 0));
  CHECK_FALSE(viaOutsideChannel(board, 0, 2));
  board.rodCount = 2;
  CHECK_FALSE(hasOutsideChannel(board));
  board.rodCount = 3;
  board.panexColumns = false;
  CHECK_FALSE(hasOutsideChannel(board));
}

TEST_CASE("panex: the Panex Solver needs Panex columns", "[stacking][panex]") {
  puzzle_c puz = makeBoard();
  problem_c * pr = makePanexSwap(puz, 3);
  puz.getRodSet(0).panexColumns = false;
  puz.getRodSet(0).sizeMatters = false;
  CHECK(panex::unsupported(*pr).find("Panex Style Columns") != std::string::npos);
  panex::panexSearch_c search;
  CHECK_FALSE(panex::solve(*pr, search));
  CHECK(search.outcome == panex::PANEX_ERROR);
}

/* Every placement of a stacking path is one disc on one rod's top moving to
 * another rod's top: the three frames of each transfer agree with the start
 * and end of it. */
static void checkTransfers(const separation_c & path) {
  const unsigned int n = path.getPieceNumber();
  for (unsigned int m = 0; m + 1 <= logicalMoves(path); m++) {
    const state_c * a = path.getState(m * STEPS_PER_MOVE);
    const state_c * b = path.getState((m + 1) * STEPS_PER_MOVE);
    unsigned int moved = 0;
    for (unsigned int i = 0; i < n; i++)
      if (a->getX(i) != b->getX(i))
        moved++;
    CHECK(moved == 1);
  }
}

TEST_CASE("panex: tracing back between saved levels", "[stacking][panex]") {
  /* A tiny memory share and disk budget: the levels go to disk, only every
   * few of them stay, and the path is traced back between those. */
  const std::string dir = scratch("panex-test").string();
  puzzle_c puz = makeBoard();
  problem_c * pr = makePanexSwap(puz, 5);
  panex::panexSearch_c search;
  search.workDir = dir;
  search.keepLimit = 10;
  search.diskBudget = 200000;
  std::unique_ptr<separation_c> path = panex::solve(*pr, search);
  REQUIRE(path);
  CHECK(logicalMoves(*path) == 343);
  CHECK(search.levelInterval > 1);
  checkTransfers(*path);
  /* A finished search leaves nothing saved. */
  CHECK(panex::savedSearch(*pr, dir).empty());

  /* Panex Jr runs from both ends, and so saves both. */
  puzzle_c jr = makeBoard();
  problem_c * jp = makePanexJr(jr);
  panex::panexSearch_c both;
  both.workDir = dir;
  both.keepLimit = 10;
  both.diskBudget = 20000;
  std::unique_ptr<separation_c> jpath = panex::solve(*jp, both);
  REQUIRE(jpath);
  CHECK(logicalMoves(*jpath) == 143);
  checkTransfers(*jpath);
  std::filesystem::remove_all(dir);
}

TEST_CASE("panex: a stopped search carries on where it stopped", "[stacking][panex]") {
  const std::string dir = scratch("panex-resume").string();
  std::filesystem::remove_all(dir);
  puzzle_c puz = makeBoard();
  problem_c * pr = makePanexSwap(puz, 5);

  panex::panexSearch_c first;
  first.workDir = dir;
  first.keepLimit = 10;
  first.stopAtDepth = 200;
  CHECK_FALSE(panex::solve(*pr, first));
  CHECK(first.outcome == panex::PANEX_STOPPED);
  CHECK(first.saved);
  CHECK(panex::savedSearch(*pr, dir).find("200 moves deep") != std::string::npos);

  /* Saves from before Time used kept milliseconds (BTPANEX3, whole seconds)
   * still load: rewrite this one in that form and read it back. */
  {
    std::filesystem::path state;
    for (const auto & e : std::filesystem::recursive_directory_iterator(dir))
      if (e.path().filename() == "state.bin")
        state = e.path();
    REQUIRE_FALSE(state.empty());
    std::fstream f(state, std::ios::in | std::ios::out | std::ios::binary);
    const std::streamoff msAt = 8 + 8 + 4 + 4 + 4 + 8;
    f.seekp(7);
    f.put('3');
    f.seekp(msAt);
    const uint64_t seconds = 7;
    f.write(reinterpret_cast<const char *>(&seconds), sizeof(seconds));
    f.close();
    CHECK(panex::savedMs(*pr, dir) == 7000);
    CHECK(panex::savedSearch(*pr, dir).find("200 moves deep") != std::string::npos);
  }

  /* Without resume it starts over; with it, it carries on to the same answer. */
  panex::panexSearch_c second;
  second.workDir = dir;
  second.resume = true;
  std::unique_ptr<separation_c> path = panex::solve(*pr, second);
  REQUIRE(path);
  CHECK(logicalMoves(*path) == 343);
  /* It went on from the saved count, not from nothing. */
  CHECK(second.found > first.found);
  checkTransfers(*path);
  CHECK(panex::savedSearch(*pr, dir).empty());

  /* Discarding a saved search leaves none. */
  panex::panexSearch_c third;
  third.workDir = dir;
  third.keepLimit = 10;
  third.stopAtDepth = 100;
  CHECK_FALSE(panex::solve(*pr, third));
  CHECK_FALSE(panex::savedSearch(*pr, dir).empty());
  panex::discardSaved(*pr, dir);
  CHECK(panex::savedSearch(*pr, dir).empty());
  std::filesystem::remove_all(dir);
}

/* The Stacking Solver now runs on the Panex search when the puzzle fits it,
 * so the two searches must agree on plain rods too: the size rule, the
 * distance rule, and neither. */
TEST_CASE("panex: the search agrees with findStackPath on plain rods", "[stacking][panex]") {
  for (int rules = 0; rules < 4; rules++) {
    for (unsigned int n = 3; n <= 6; n++) {
      INFO("rules " << rules << ", " << n << " discs");
      puzzle_c puz = makeBoard();
      rodSet_c & board = puz.getRodSet(0);
      board.sizeMatters = rules != 2;
      board.distanceMatters = rules == 1 || rules == 3;
      problem_c * pr = puz.getProblem(puz.addProblem());
      for (unsigned int size = n; size >= 1; size--)
        addSized(puz, *pr, size);
      pr->setRodSetId(0);
      syncMaps(*pr);
      for (unsigned int i = 0; i < n; i++) {
        REQUIRE(placeDisk(*pr, false, i, 0).empty());
        REQUIRE(placeDisk(*pr, true, i, 2).empty());
      }
      REQUIRE(setupError(*pr).empty());
      REQUIRE(panex::unsupported(*pr, true).empty());

      std::unique_ptr<separation_c> plain = findStackPath(*pr);
      REQUIRE(plain);
      panex::panexSearch_c search;
      search.anyRules = true;
      search.workDir = scratch("panex-plain").string();
      std::unique_ptr<separation_c> path = panex::solve(*pr, search);
      REQUIRE(path);
      CHECK(logicalMoves(*path) == logicalMoves(*plain));
      checkTransfers(*path);
    }
  }
}

/* An export may leave the saved levels out: the search still carries on,
 * and finds its path back by solving the rest by halves. */
TEST_CASE("panex: a saved search without its levels still finds the path", "[stacking][panex]") {
  const std::string dir = scratch("panex-nolevels").string();
  std::filesystem::remove_all(dir);
  puzzle_c puz = makeBoard();
  problem_c * pr = makePanexSwap(puz, 5);

  panex::panexSearch_c first;
  first.workDir = dir;
  first.keepLimit = 10;
  first.stopAtDepth = 200;
  CHECK_FALSE(panex::solve(*pr, first));
  REQUIRE(first.saved);

  /* Keep only the state, as an export without levels does. */
  const std::filesystem::path folder = panex::searchFolder(*pr, dir);
  REQUIRE(std::filesystem::exists(folder / "state.bin"));
  for (const auto & e : std::filesystem::directory_iterator(folder))
    if (e.path().extension() == ".lvl")
      std::filesystem::remove(e.path());

  panex::panexSearch_c second;
  second.workDir = dir;
  second.resume = true;
  std::unique_ptr<separation_c> path = panex::solve(*pr, second);
  REQUIRE(path);
  CHECK(logicalMoves(*path) == 343);
  checkTransfers(*path);
  std::filesystem::remove_all(dir);
}

TEST_CASE("stacking: the board span covers every rod, the pocket too", "[stacking]") {
  puzzle_c puz = makeBoard();
  problem_c * pr = makePanexJr(puz);
  const boardLayout_c lay = layoutBoard(*pr, false);
  REQUIRE(lay.rodX.size() == 3);
  /* The pocket is the last rod but is drawn first, at x 0. */
  CHECK(lay.rodX.back() == 0);
  CHECK(boardSpan(*pr) == (unsigned int)(lay.rodX[1] + lay.spacing));
}

TEST_CASE("stacking: a rod set with impossible numbers in the file loads sanely", "[stacking]") {
  std::stringstream in("<rodSets><rodSet rods=\"-3\" grow=\"0\" height=\"-1\" pocket=\"x\"/></rodSets>");
  xmlParser_c pars(in);
  REQUIRE(pars.nextTag() == xmlParser_c::START_TAG);
  puzzle_c puz = makeBoard();
  const unsigned int before = puz.rodSetCount();
  loadRodSets(puz, pars);
  REQUIRE(puz.rodSetCount() == before + 1);
  const rodSet_c & r = puz.getRodSet(before);
  CHECK(r.rodCount == 3);
  CHECK(r.definedHeight == 8);
  CHECK_FALSE(r.pocketColumn);
}

/* Undo puts back the discs' places on the rods: restoring the part counts
 * goes through zero, which takes the discs off the rods. */
TEST_CASE("stacking: undo of a disc edit keeps the stacks", "[stacking]") {
  puzzle_c puz = makeBoard();
  problem_c * pr = makePanexJr(puz);
  const stackMap_c start = pr->startStacks();
  const stackMap_c goal = pr->goalStacks();
  REQUIRE_FALSE(start.rods.empty());

  shapeHistory_c history;
  history.reset(&puz);
  /* A structural edit: one more copy of the first disc. */
  pr->setShapeMaximum(pr->getShapeIdOfPart(0), 2);
  history.record(&puz, shapeHistory_c::AK_STRUCTURAL, 0);
  REQUIRE(history.canUndo());
  history.undo(&puz);

  CHECK(pr->getShapeMaximum(pr->getShapeIdOfPart(0)) == 1);
  CHECK(pr->startStacks().rods == start.rods);
  CHECK(pr->goalStacks().rods == goal.rods);
  CHECK(setupError(*pr).empty());
}

/* Hidden: the Panex swap of 6 discs a tower (881 transfers), at one thread
 * and at all of them.   ./build/test_burrtools "[.bench][panex]" */
TEST_CASE("panex: benchmark, the six-disc swap", "[.bench][panex]") {
  /* BT_PANEX_BENCH_N=7 for the seven-disc swap (2189 transfers). */
  const char * asked = std::getenv("BT_PANEX_BENCH_N");
  const unsigned int n = asked ? (unsigned int)std::atoi(asked) : 6;
  for (unsigned int threads : {1u, 0u}) {
    puzzle_c puz = makeBoard();
    problem_c * pr = makePanexSwap(puz, n);
    panex::panexSearch_c search;
    search.threads = threads;
    const auto t0 = std::chrono::steady_clock::now();
    std::unique_ptr<separation_c> path = panex::solve(*pr, search);
    const double s = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    REQUIRE(path);
    printf("panex %u, %s: %u transfers, %llu stackings, peak %llu MB, %.2f s\n",
           n, threads ? "1 thread" : "all threads", logicalMoves(*path),
           (unsigned long long)search.found, (unsigned long long)(search.peakMemory >> 20), s);
  }
}

/* The classic tower is solved by its well-known recursion, with no search:
 * 2^n - 1 transfers, each one legal, the same count the search finds. */
TEST_CASE("panex: the classic tower needs no search", "[stacking][panex]") {
  for (unsigned int n : {1u, 2u, 7u, 10u}) {
    INFO(n << " discs");
    puzzle_c puz = makeBoard();
    puz.getRodSet(0).sizeMatters = true;
    problem_c * pr = puz.getProblem(puz.addProblem());
    for (unsigned int size = n; size >= 1; size--)
      addSized(puz, *pr, size);
    pr->setRodSetId(0);
    syncMaps(*pr);
    for (unsigned int i = 0; i < n; i++) {
      REQUIRE(placeDisk(*pr, false, i, 2).empty());
      REQUIRE(placeDisk(*pr, true, i, 1).empty());
    }
    REQUIRE(setupError(*pr).empty());

    panex::panexSearch_c direct;
    direct.anyRules = true;
    std::unique_ptr<separation_c> path = panex::solve(*pr, direct);
    REQUIRE(path);
    CHECK(direct.outcome == panex::PANEX_FOUND);
    CHECK(logicalMoves(*path) == (1u << n) - 1);
    checkTransfers(*path);
    /* No search: only the path's own stackings were made. */
    CHECK(direct.found == (1ull << n));

#ifndef _WIN32
    setenv("BURRTOOLS_NO_TOWER_RULE", "1", 1);
    panex::panexSearch_c searched;
    searched.anyRules = true;
    searched.workDir = scratch("panex-tower").string();
    std::unique_ptr<separation_c> found = panex::solve(*pr, searched);
    unsetenv("BURRTOOLS_NO_TOWER_RULE");
    REQUIRE(found);
    CHECK(logicalMoves(*found) == logicalMoves(*path));
#endif
  }
}

namespace {

std::string fileBytes(const std::filesystem::path & f) {
  std::ifstream in(f, std::ios::binary);
  return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}

/* Pack f, unpack it again, and check the bytes come back; returns the
 * packed size. */
uint64_t packRoundTrip(const std::filesystem::path & f, bool keys, size_t chunk) {
  std::stringstream packed;
  const uint64_t n = blockpack::pack(f, packed, keys, 4, chunk);
  REQUIRE(n == packed.str().size());
  const std::filesystem::path back = f.string() + ".back";
  std::stringstream in(packed.str());
  REQUIRE(blockpack::unpack(in, n, back, 4));
  const bool same = fileBytes(back) == fileBytes(f);
  CHECK(same);
  std::filesystem::remove(back);
  return n;
}

} // namespace

TEST_CASE("blockpack: files come back unchanged, keys packed small", "[stacking][panex][export]") {
  const std::filesystem::path dir = scratch("blockpack");
  std::filesystem::remove_all(dir);
  std::filesystem::create_directories(dir);
  std::mt19937_64 rng(7);

  /* Sorted 16-byte keys, as a search level holds, over many chunks. */
  std::vector<std::pair<uint64_t, uint64_t>> keys(100000);
  for (auto & k : keys)
    k = {rng() >> 40, rng()};
  std::sort(keys.begin(), keys.end());
  keys.erase(std::unique(keys.begin(), keys.end()), keys.end());
  {
    std::ofstream out(dir / "sorted.lvl", std::ios::binary);
    for (const auto & k : keys) {
      out.write(reinterpret_cast<const char *>(&k.first), 8);
      out.write(reinterpret_cast<const char *>(&k.second), 8);
    }
  }
  const uint64_t raw = std::filesystem::file_size(dir / "sorted.lvl");
  const uint64_t asKeys = packRoundTrip(dir / "sorted.lvl", true, 16 * 997);
  const uint64_t plain = packRoundTrip(dir / "sorted.lvl", false, 16 * 997);
  INFO("raw " << raw << ", as keys " << asKeys << ", plain deflate " << plain);
  printf("blockpack: random sorted keys %llu bytes -> %llu as keys, %llu plain\n",
         (unsigned long long)raw, (unsigned long long)asKeys, (unsigned long long)plain);
  CHECK(asKeys < plain);

  /* Unsorted keys and odd sizes fall back to plain deflate. */
  std::shuffle(keys.begin(), keys.end(), rng);
  {
    std::ofstream out(dir / "unsorted.lvl", std::ios::binary);
    for (const auto & k : keys) {
      out.write(reinterpret_cast<const char *>(&k.first), 8);
      out.write(reinterpret_cast<const char *>(&k.second), 8);
    }
    std::ofstream odd(dir / "odd.bin", std::ios::binary);
    for (int i = 0; i < 12345; i++)
      odd.put((char)(rng() & 0xff));
    std::ofstream empty(dir / "empty.bin", std::ios::binary);
  }
  packRoundTrip(dir / "unsorted.lvl", true, 16 * 997);
  packRoundTrip(dir / "odd.bin", true, 4096);
  packRoundTrip(dir / "empty.bin", true, 4096);

  /* Damage is refused, not written out as data. */
  std::stringstream packed;
  const uint64_t n = blockpack::pack(dir / "sorted.lvl", packed, true, 2, 16 * 997);
  std::string bytes = packed.str();
  bytes[bytes.size() / 2] ^= 0x55;
  std::stringstream damaged(bytes);
  CHECK_FALSE(blockpack::unpack(damaged, n, dir / "damaged.lvl", 2));
  std::filesystem::remove_all(dir);
}

TEST_CASE("blockpack: a real search's saved levels", "[stacking][panex][export]") {
  const std::string dir = scratch("blockpack-panex").string();
  std::filesystem::remove_all(dir);
  puzzle_c puz = makeBoard();
  problem_c * pr = makePanexSwap(puz, 5);
  panex::panexSearch_c search;
  search.workDir = dir;
  search.keepLimit = 10;
  search.stopAtDepth = 200;
  CHECK_FALSE(panex::solve(*pr, search));
  REQUIRE(search.saved);

  uint64_t raw = 0, packedTotal = 0, plainTotal = 0;
  for (const auto & e : std::filesystem::directory_iterator(panex::searchFolder(*pr, dir))) {
    const bool level = e.path().extension() == ".lvl";
    raw += std::filesystem::file_size(e.path());
    packedTotal += packRoundTrip(e.path(), level, 0);
    plainTotal += packRoundTrip(e.path(), false, 0);
  }
  INFO("saved search: raw " << raw << " bytes, packed " << packedTotal);
  printf("blockpack: Panex 5 saved search %llu bytes -> %llu (plain deflate %llu)\n",
         (unsigned long long)raw, (unsigned long long)packedTotal, (unsigned long long)plainTotal);
  CHECK(packedTotal * 2 < raw);
  CHECK(packedTotal < plainTotal);
  std::filesystem::remove_all(dir);
}
