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
#include "lib/voxel.h"

#include "tools/xml.h"

#include <filesystem>
#include <memory>
#include <sstream>

using namespace stacking;

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

TEST_CASE("stacking: Panex settings survive a save and load", "[stacking]") {
  puzzle_c puz = makeBoard();
  puz.getRodSet(0).panexColumns = true;
  puz.getRodSet(0).pocketColumn = true;
  puz.getRodSet(0).pocketHeight = 2;

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
  const std::string dir = (std::filesystem::temp_directory_path() / "burrtools-panex-test").string();
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
  const std::string dir = (std::filesystem::temp_directory_path() / "burrtools-panex-resume").string();
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
      search.workDir = (std::filesystem::temp_directory_path() / "burrtools-panex-plain").string();
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
  const std::string dir = (std::filesystem::temp_directory_path() / "burrtools-panex-nolevels").string();
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
