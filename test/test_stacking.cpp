#include "catch2/catch_test_macros.hpp"

#include "lib/assembly.h"
#include "lib/disassembly.h"
#include "lib/gridtype.h"
#include "lib/problem.h"
#include "lib/puzzle.h"
#include "lib/stacking.h"
#include "lib/voxel.h"

#include "tools/xml.h"

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
