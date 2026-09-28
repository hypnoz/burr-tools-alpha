#include "catch2/catch_test_macros.hpp"
#include "test_helpers.h"

#include "lib/bt_assert.h"
#include "lib/sliding.h"
#include "lib/assembler.h"
#include "lib/assembly.h"
#include "lib/disassembly.h"
#include "lib/disasmtomoves.h"
#include "lib/gridtype.h"
#include "lib/problem.h"
#include "lib/puzzle.h"
#include "lib/solution.h"
#include "lib/solvethread.h"
#include "lib/voxel.h"

#include <string>

using namespace bttest;

TEST_CASE("sliding: place start and find a one-cell path", "[sliding]") {
  puzzle_c puz(new gridType_c(gridType_c::GT_SLIDING));
  sliding::ensureSetup(puz, 3, 1);

  REQUIRE(puz.getNumberOfProblems() == 1);
  problem_c * pr = puz.getProblem(0);
  REQUIRE(pr->resultValid());
  REQUIRE(pr->goalValid());

  /* One 1x1 piece. */
  unsigned int pieceId = puz.addShape(1, 1, 1);
  puz.getShape(pieceId)->setState(0, 0, 0, voxel_c::VX_FILLED);
  pr->setShapeMaximum(pieceId, 1);

  REQUIRE(sliding::placeStart(*pr, pieceId, 0, 0));
  REQUIRE(sliding::hasStart(*pr, pieceId));
  REQUIRE(sliding::placeGoal(*pr, pieceId, 2, 0));
  REQUIRE(sliding::hasGoal(*pr, pieceId));

  sliding::syncMaxHoles(*pr);

  assembly_c start(puz.getGridType());
  start.addPlacement(0, 0, 0, 0); /* identity transform at (0,0,0) */

  auto path = sliding::findSlidePath(*pr, start);
  REQUIRE(path != nullptr);
  /* Two cells in one direction are a single move. */
  REQUIRE(path->getMoves() == 1);
  REQUIRE(path->getState(0)->getX(0) == 0);
  REQUIRE(path->getState(1)->getX(0) == 2);

  disasmToMoves_c mv(path.get(), 5, 1);
  mv.setStep(0, false, true);
  REQUIRE(mv.getX(0) == 0);
  mv.setStep(0.5f, false, true);
  REQUIRE(mv.getX(0) == 1);
  mv.setStep(1, false, true);
  REQUIRE(mv.getX(0) == 2);

  /* A shorter straight slide to the same cell is also one move. */
  assembly_c closer(puz.getGridType());
  closer.addPlacement(0, 1, 0, 0);
  auto shorter = sliding::findSlidePath(*pr, closer);
  REQUIRE(shorter != nullptr);
  REQUIRE(shorter->getMoves() == 1);
  REQUIRE(sliding::finalPlacementKey(*shorter) == sliding::finalPlacementKey(*path));
}

TEST_CASE("sliding: more restriction cells than the piece has voxels", "[sliding]") {
  puzzle_c puz(new gridType_c(gridType_c::GT_SLIDING));
  unsigned int trayId = sliding::addStartGoalShape(puz, 3, 1);
  voxel_c * tray = puz.getShape(trayId);
  unsigned int pieceId = puz.addShape(1, 1, 1);
  puz.getShape(pieceId)->setState(0, 0, 0, voxel_c::VX_FILLED);
  unsigned int mark = pieceId + 1;
  tray->setColor(0, 0, 0, mark);
  sliding::syncSlidingProblems(puz);
  problem_c * pr = puz.getProblem(0);

  REQUIRE(sliding::stampOverflowMessage(*pr).empty());

  tray->setColor(1, 0, 0, mark);
  std::string startMsg = sliding::stampOverflowMessage(*pr);
  REQUIRE(startMsg.find("S" + std::to_string(mark)) != std::string::npos);
  REQUIRE(startMsg.find("start") != std::string::npos);

  tray->setColor(1, 0, 0, 0);
  tray->setGoalPiece((unsigned)tray->getIndex(0, 0, 0), mark);
  tray->setGoalPiece((unsigned)tray->getIndex(1, 0, 0), mark);
  std::string goalMsg = sliding::stampOverflowMessage(*pr);
  REQUIRE(goalMsg.find("goal") != std::string::npos);
  REQUIRE(goalMsg.find("start") == std::string::npos);
}

TEST_CASE("sliding: a variable cell is not a legal place to finish", "[sliding]") {
  puzzle_c puz(new gridType_c(gridType_c::GT_SLIDING));
  unsigned int trayId = sliding::addStartGoalShape(puz, 3, 1);
  voxel_c * tray = puz.getShape(trayId);
  tray->setState(1, 0, 0, voxel_c::VX_VARIABLE);

  unsigned int pieceId = puz.addShape(1, 1, 1);
  puz.getShape(pieceId)->setState(0, 0, 0, voxel_c::VX_FILLED);
  tray->setColor(0, 0, 0, pieceId + 1);
  /* The goal sits on the far normal cell. The piece crosses the variable cell. */
  tray->setGoalPiece((unsigned)tray->getIndex(2, 0, 0), pieceId + 1);
  sliding::syncSlidingProblems(puz);

  problem_c * pr = puz.getProblem(0);
  assembly_c start(puz.getGridType());
  start.addPlacement(0, 0, 0, 0);

  auto path = sliding::findSlidePath(*pr, start);
  REQUIRE(path != nullptr);
  REQUIRE(path->getMoves() == 1);

  /* A goal stamp on a variable cell is still not a place to stop. */
  tray->setGoalPiece((unsigned)tray->getIndex(2, 0, 0), 0);
  tray->setGoalPiece((unsigned)tray->getIndex(1, 0, 0), pieceId + 1);
  auto blocked = sliding::findSlidePath(*pr, start);
  REQUIRE(blocked == nullptr);
}

TEST_CASE("sliding: a turn stays two moves", "[sliding]") {
  puzzle_c puz(new gridType_c(gridType_c::GT_SLIDING));
  sliding::ensureSetup(puz, 2, 2);

  unsigned int pieceId = puz.addShape(1, 1, 1);
  puz.getShape(pieceId)->setState(0, 0, 0, voxel_c::VX_FILLED);
  problem_c * pr = puz.getProblem(0);
  pr->setShapeMaximum(pieceId, 1);
  REQUIRE(sliding::placeStart(*pr, pieceId, 0, 0));
  REQUIRE(sliding::placeGoal(*pr, pieceId, 1, 1));

  assembly_c start(puz.getGridType());
  start.addPlacement(0, 0, 0, 0);
  auto path = sliding::findSlidePath(*pr, start);
  REQUIRE(path != nullptr);
  REQUIRE(path->getMoves() == 2);
  REQUIRE(path->getState(2)->getX(0) == 1);
  REQUIRE(path->getState(2)->getY(0) == 1);
}

TEST_CASE("sliding: the start stamp is the only initial placement", "[sliding]") {
  puzzle_c puz(new gridType_c(gridType_c::GT_SLIDING));
  unsigned int trayId = sliding::addStartGoalShape(puz, 3, 1);
  voxel_c * tray = puz.getShape(trayId);
  for (unsigned int x = 0; x < 3; x++)
    tray->setState(x, 0, 0, voxel_c::VX_VARIABLE);

  unsigned int pieceId = puz.addShape(1, 1, 1);
  puz.getShape(pieceId)->setState(0, 0, 0, voxel_c::VX_FILLED);
  tray->setColor(0, 0, 0, pieceId + 1);
  tray->setGoalPiece((unsigned)tray->getIndex(2, 0, 0), pieceId + 1);
  sliding::syncSlidingProblems(puz);

  REQUIRE(puz.colorNumber() >= pieceId + 1);

  problem_c * pr = puz.getProblem(0);
  auto assm = puz.getGridType()->findAssembler(*pr);
  REQUIRE(assm->createMatrix(false, false, false, true) == assembler_c::ERR_NONE);

  struct counter : assembler_cb {
    unsigned n = 0;
    int x = -1;
    int y = -1;
    bool assembly(std::unique_ptr<assembly_c> a) override {
      n++;
      x = a->getX(0);
      y = a->getY(0);
      return true;
    }
  } cb;
  assm->assemble(&cb);
  REQUIRE(cb.n == 1);
  REQUIRE(cb.x == 0);
  REQUIRE(cb.y == 0);
}

TEST_CASE("sliding: normal cells may stay empty", "[sliding]") {
  puzzle_c puz(new gridType_c(gridType_c::GT_SLIDING));
  sliding::addStartGoalShape(puz, 3, 1);

  unsigned int pieceId = puz.addShape(1, 1, 1);
  puz.getShape(pieceId)->setState(0, 0, 0, voxel_c::VX_FILLED);
  sliding::syncSlidingProblems(puz);
  problem_c * pr = puz.getProblem(0);
  pr->setShapeMinimum(pieceId, 1);
  pr->setShapeMaximum(pieceId, 1);

  auto assm = puz.getGridType()->findAssembler(*pr);
  REQUIRE(assm->createMatrix(false, false, false, true) == assembler_c::ERR_NONE);

  struct counter : assembler_cb {
    unsigned n = 0;
    bool assembly(std::unique_ptr<assembly_c>) override {
      n++;
      return true;
    }
  } cb;
  assm->assemble(&cb);
  REQUIRE(cb.n >= 1);
}

TEST_CASE("sliding: variable cells are corridors beside normal floor", "[sliding]") {
  puzzle_c puz(new gridType_c(gridType_c::GT_SLIDING));
  unsigned int trayId = sliding::addStartGoalShape(puz, 2, 1);
  voxel_c * tray = puz.getShape(trayId);
  tray->setState(0, 0, 0, voxel_c::VX_VARIABLE);

  unsigned int pieceId = puz.addShape(1, 1, 1);
  puz.getShape(pieceId)->setState(0, 0, 0, voxel_c::VX_FILLED);
  /* No goal stamp. The piece starts on the corridor and must leave it. */
  sliding::syncSlidingProblems(puz);
  problem_c * pr = puz.getProblem(0);
  pr->setShapeMaximum(pieceId, 1);

  assembly_c start(puz.getGridType());
  start.addPlacement(0, 0, 0, 0);

  auto path = sliding::findSlidePath(*pr, start);
  REQUIRE(path != nullptr);
  REQUIRE(path->getMoves() == 1);
}

TEST_CASE("sliding: a shorter path keeps the solution number", "[sliding]") {
  puzzle_c puz(new gridType_c(gridType_c::GT_SLIDING));
  sliding::ensureSetup(puz, 2, 2);

  unsigned int pieceId = puz.addShape(1, 1, 1);
  puz.getShape(pieceId)->setState(0, 0, 0, voxel_c::VX_FILLED);
  problem_c * pr = puz.getProblem(0);
  pr->setShapeMaximum(pieceId, 1);
  REQUIRE(sliding::placeGoal(*pr, pieceId, 1, 1));

  /* addSolution requires a solve in progress. The callback is reached
   * through assembler_cb so the test can offer starts in a known order. */
  auto eng = puz.getGridType()->findAssembler(*pr, true);
  REQUIRE(eng != nullptr);
  REQUIRE(pr->setAssembler(std::move(eng)) == assembler_c::ERR_NONE);

  solveThread_c solver(*pr, solveThread_c::PAR_DISASSM);
  assembler_cb & cb = solver;
  const gridType_c * gt = puz.getGridType();

  auto at = [gt](int x, int y) {
    auto a = std::make_unique<assembly_c>(gt);
    a->addPlacement(0, x, y, 0);
    return a;
  };

  /* (0,0) reaches (1,1) in two moves. That is solution 0. */
  REQUIRE(cb.assembly(at(0, 0)));
  REQUIRE(pr->getNumSolutions() == 1);
  REQUIRE(pr->getSavedSolution(0)->getSolutionNumber() == 0);
  REQUIRE(pr->getSavedSolution(0)->getDisassembly()->getMoves() == 2);

  /* (1,0) is the same picture in one move. The count stays 1 and the
   * stored number stays 0. The Solver tab shows that number plus one. */
  REQUIRE(cb.assembly(at(1, 0)));
  REQUIRE(pr->getNumSolutions() == 1);
  REQUIRE(pr->getNumberOfSavedSolutions() == 1);
  REQUIRE(pr->getSavedSolution(0)->getSolutionNumber() == 0);
  REQUIRE(pr->getSavedSolution(0)->getDisassembly()->getMoves() == 1);

  /* A different picture is the next solution. A shorter path to it keeps
   * that second number, and does not collide with the first. */
  sliding::clearGoal(*pr, pieceId);
  REQUIRE(sliding::placeGoal(*pr, pieceId, 0, 0));
  REQUIRE(cb.assembly(at(1, 1)));
  REQUIRE(pr->getNumSolutions() == 2);
  REQUIRE(cb.assembly(at(0, 1)));
  REQUIRE(pr->getNumSolutions() == 2);
  REQUIRE(pr->getNumberOfSavedSolutions() == 2);

  bool saw0 = false;
  bool saw1 = false;
  for (unsigned int i = 0; i < pr->getNumberOfSavedSolutions(); i++) {
    unsigned int n = pr->getSavedSolution(i)->getSolutionNumber();
    REQUIRE(pr->getSavedSolution(i)->getDisassembly()->getMoves() == 1);
    if (n == 0)
      saw0 = true;
    if (n == 1)
      saw1 = true;
  }
  REQUIRE(saw0);
  REQUIRE(saw1);
}

TEST_CASE("sliding: removing a labelled cell clears start and goal", "[sliding]") {
  puzzle_c puz(new gridType_c(gridType_c::GT_SLIDING));
  unsigned int trayId = sliding::addStartGoalShape(puz, 2, 1);
  voxel_c * tray = puz.getShape(trayId);

  tray->setColor(0, 0, 0, 2);
  tray->setGoalPiece((unsigned)tray->getIndex(0, 0, 0), 2);
  REQUIRE(tray->getColor(0, 0, 0) == 2);
  REQUIRE(tray->getGoalPiece(0, 0, 0) == 2);

  tray->setState(0, 0, 0, voxel_c::VX_EMPTY);
  REQUIRE(tray->getState(0, 0, 0) == voxel_c::VX_EMPTY);
  REQUIRE(tray->getColor(0, 0, 0) == 0);
  REQUIRE(tray->getGoalPiece(0, 0, 0) == 0);

  /* A mark written onto the empty cell must not reappear when it is filled. */
  tray->setGoalPiece((unsigned)tray->getIndex(0, 0, 0), 2);
  tray->setState(0, 0, 0, voxel_c::VX_FILLED);
  REQUIRE(tray->getState(0, 0, 0) == voxel_c::VX_FILLED);
  REQUIRE(tray->getColor(0, 0, 0) == 0);
  REQUIRE(tray->getGoalPiece(0, 0, 0) == 0);
}

TEST_CASE("sliding: One Way or Another solves to one 18-move path", "[sliding][solver]") {
  std::unique_ptr<puzzle_c> puzzle = puzzle_c::load("test/test_sliding_solver.xmpuzzle");
  REQUIRE(puzzle != nullptr);
  REQUIRE(puzzle->getGridType()->getType() == gridType_c::GT_SLIDING);
  problem_c * pr = puzzle->getProblem(0);
  REQUIRE(pr != nullptr);

  /* The file already stores the answer. Clear it so this run is the solver. */
  pr->removeAllSolutions();

  const int par = solveThread_c::PAR_REDUCE | solveThread_c::PAR_DISASSM;
  solveThread_c solver(*pr, par);
  REQUIRE(solver.start());
  solver.waitUntilFinished();

  if (solver.currentAction() == solveThread_c::ACT_ASSERT) {
    const assert_exception & e = solver.getAssertException();
    FAIL(std::string(e.file) + ":" + std::to_string(e.line) + " " + e.what());
  }
  if (solver.currentAction() == solveThread_c::ACT_ERROR) {
    FAIL(std::string("solver error ") + std::to_string(solver.getErrorState()));
  }
  REQUIRE(solver.currentAction() == solveThread_c::ACT_FINISHED);

  REQUIRE(pr->getNumSolutions() == 1);
  REQUIRE(pr->getNumberOfSavedSolutions() == 1);
  const separation_c * path = pr->getSavedSolution(0)->getDisassembly();
  REQUIRE(path != nullptr);
  CHECK(path->getMoves() == 18);
}
