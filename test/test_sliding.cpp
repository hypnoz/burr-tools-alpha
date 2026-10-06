#include "catch2/catch_test_macros.hpp"
#include "test_helpers.h"

#include "lib/bt_assert.h"
#include "lib/sliding.h"
#include "lib/slidelevels.h"
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

#include "tools/xml.h"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

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

TEST_CASE("sliding: a turn is one fluid move", "[sliding]") {
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
  /* Round the corner without stopping: one move. */
  REQUIRE(path->getMoves() == 1);
  REQUIRE(path->getState(1)->getX(0) == 1);
  REQUIRE(path->getState(1)->getY(0) == 1);

  /* The route has one corner, and the animation follows it rather than
   * cutting across the diagonal. */
  std::vector<unsigned int> movers;
  auto route = sliding::slideRoute(*pr, *path, 0, &movers);
  REQUIRE(movers == std::vector<unsigned int>{0});
  REQUIRE(route.size() == 3);
  CHECK(route.front() == std::make_pair(0, 0));
  CHECK(route.back() == std::make_pair(1, 1));

  disasmToMoves_c mv(path.get(), 5, 1);
  sliding::applySlideRoutes(*pr, *path, mv);
  mv.setStep(0.5f, false, true);
  /* Halfway along a two-cell L, the piece is at the corner, not (0.5, 0.5). */
  CHECK(mv.getX(0) == (float)route[1].first);
  CHECK(mv.getY(0) == (float)route[1].second);
  mv.setStep(1, false, true);
  CHECK(mv.getX(0) == 1);
  CHECK(mv.getY(0) == 1);
}

TEST_CASE("sliding: a piece following another is not a caterpillar", "[sliding]") {
  /* A 6x1 corridor: b at 1 slides to 5, then a, behind it at 0, slides to 3.
   * Counting unit steps, every order in which a stays behind b is equally
   * short, so the search could alternate them a cell at a time (7 moves).
   * Counting whole moves of one piece, each goes the whole way once. */
  puzzle_c puz(new gridType_c(gridType_c::GT_SLIDING));
  sliding::ensureSetup(puz, 6, 1);

  unsigned int a = puz.addShape(1, 1, 1);
  puz.getShape(a)->setState(0, 0, 0, voxel_c::VX_FILLED);
  unsigned int b = puz.addShape(1, 1, 1);
  puz.getShape(b)->setState(0, 0, 0, voxel_c::VX_FILLED);
  problem_c * pr = puz.getProblem(0);
  pr->setShapeMaximum(a, 1);
  pr->setShapeMaximum(b, 1);
  REQUIRE(sliding::placeStart(*pr, a, 0, 0));
  REQUIRE(sliding::placeStart(*pr, b, 1, 0));
  REQUIRE(sliding::placeGoal(*pr, a, 3, 0));
  REQUIRE(sliding::placeGoal(*pr, b, 5, 0));

  assembly_c start(puz.getGridType());
  start.addPlacement(0, 0, 0, 0);
  start.addPlacement(0, 1, 0, 0);
  auto path = sliding::findSlidePath(*pr, start);
  REQUIRE(path != nullptr);
  REQUIRE(path->getMoves() == 2);
  /* b goes the full four cells, then a goes the full three. */
  CHECK(path->getState(1)->getX(1) == 5);
  CHECK(path->getState(1)->getX(0) == 0);
  CHECK(path->getState(2)->getX(0) == 3);
}

TEST_CASE("sliding: a piece nested in a pocket rides along only with nested slides", "[sliding]") {
  /* A 2x4 tray. The outer piece is a C, open to the tray's left edge:
   *   ##
   *   .#
   *   ##
   * The inner 1x1 sits in its pocket. Both must rise one row. Alone, neither
   * can move: the outer piece's bottom arm hits the inner piece, and the
   * inner piece hits the top arm. Together they rise in one move. */
  puzzle_c puz(new gridType_c(gridType_c::GT_SLIDING));
  sliding::ensureSetup(puz, 2, 4);

  unsigned int outer = puz.addShape(2, 3, 1);
  for (auto c : std::vector<std::pair<int, int>>{{0, 0}, {1, 0}, {1, 1}, {0, 2}, {1, 2}})
    puz.getShape(outer)->setState(c.first, c.second, 0, voxel_c::VX_FILLED);
  unsigned int inner = puz.addShape(1, 1, 1);
  puz.getShape(inner)->setState(0, 0, 0, voxel_c::VX_FILLED);
  problem_c * pr = puz.getProblem(0);
  pr->setShapeMaximum(outer, 1);
  pr->setShapeMaximum(inner, 1);
  /* Goals first: a goal stamp may not cover a cell already stamped as a start. */
  REQUIRE(sliding::placeGoal(*pr, outer, 0, 0));
  REQUIRE(sliding::placeGoal(*pr, inner, 0, 1));
  REQUIRE(sliding::placeStart(*pr, outer, 0, 1));
  REQUIRE(sliding::placeStart(*pr, inner, 0, 2));

  assembly_c start(puz.getGridType());
  start.addPlacement(0, 0, 1, 0);
  start.addPlacement(0, 0, 2, 0);

  REQUIRE(sliding::findSlidePath(*pr, start) == nullptr);

  auto path = sliding::findSlidePath(*pr, start, sliding::SEARCH_STATES, true);
  REQUIRE(path != nullptr);
  REQUIRE(path->getMoves() == 1);
  CHECK(path->getState(1)->getY(0) == 0);
  CHECK(path->getState(1)->getY(1) == 1);

  std::vector<unsigned int> movers;
  auto route = sliding::slideRoute(*pr, *path, 0, &movers);
  CHECK(movers == std::vector<unsigned int>{0, 1});
  CHECK(route.size() == 2);
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
  /* A 2x2 tray, piece p and a blocker q. A piece goes anywhere it can reach
   * in one move, so a longer path needs q to step aside first. */
  puzzle_c puz(new gridType_c(gridType_c::GT_SLIDING));
  sliding::ensureSetup(puz, 2, 2);

  unsigned int p = puz.addShape(1, 1, 1);
  puz.getShape(p)->setState(0, 0, 0, voxel_c::VX_FILLED);
  unsigned int q = puz.addShape(1, 1, 1);
  puz.getShape(q)->setState(0, 0, 0, voxel_c::VX_FILLED);
  problem_c * pr = puz.getProblem(0);
  pr->setShapeMaximum(p, 1);
  pr->setShapeMaximum(q, 1);
  REQUIRE(sliding::placeGoal(*pr, p, 1, 1));
  REQUIRE(sliding::placeGoal(*pr, q, 0, 1));

  /* addSolution requires a solve in progress. The callback is reached
   * through assembler_cb so the test can offer starts in a known order. */
  auto eng = puz.getGridType()->findAssembler(*pr, true);
  REQUIRE(eng != nullptr);
  REQUIRE(pr->setAssembler(std::move(eng)) == assembler_c::ERR_NONE);

  solveThread_c solver(*pr, solveThread_c::PAR_DISASSM);
  assembler_cb & cb = solver;
  const gridType_c * gt = puz.getGridType();

  auto at = [gt](int px, int py, int qx, int qy) {
    auto a = std::make_unique<assembly_c>(gt);
    a->addPlacement(0, px, py, 0);
    a->addPlacement(0, qx, qy, 0);
    return a;
  };

  /* p (0,0), q (1,1): q moves to (0,1), then p goes round to (1,1).
   * Two moves. That is solution 0. */
  REQUIRE(cb.assembly(at(0, 0, 1, 1)));
  REQUIRE(pr->getNumSolutions() == 1);
  REQUIRE(pr->getSavedSolution(0)->getSolutionNumber() == 0);
  REQUIRE(pr->getSavedSolution(0)->getDisassembly()->getMoves() == 2);

  /* p (1,0), q (0,1) is the same picture in one move. The count stays 1
   * and the stored number stays 0. The Solver tab shows that number plus one. */
  REQUIRE(cb.assembly(at(1, 0, 0, 1)));
  REQUIRE(pr->getNumSolutions() == 1);
  REQUIRE(pr->getNumberOfSavedSolutions() == 1);
  REQUIRE(pr->getSavedSolution(0)->getSolutionNumber() == 0);
  REQUIRE(pr->getSavedSolution(0)->getDisassembly()->getMoves() == 1);

  /* A different picture is the next solution. A shorter path to it keeps
   * that second number, and does not collide with the first. */
  sliding::clearGoal(*pr, p);
  REQUIRE(sliding::placeGoal(*pr, p, 0, 0));
  REQUIRE(cb.assembly(at(1, 1, 1, 0)));
  REQUIRE(pr->getNumSolutions() == 2);
  REQUIRE(cb.assembly(at(1, 0, 0, 1)));
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

TEST_CASE("sliding: the test sliding puzzle solves to one 81-move path", "[sliding][solver]") {
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
  CHECK(path->getMoves() == 81);
}

/* test/test_sliding_nested.xmpuzzle is a 12x8 tray with four 3x3 blocks, a
 * C-shaped "Cave" open to the left, and a 3x3 "Smile" in the Cave's pocket.
 * The blocks start along the top and the Cave and Smile along the bottom;
 * the goal swaps them. The Smile cannot leave the pocket (holes beside the
 * opening), and the Cave cannot rise or fall past it alone, so the puzzle
 * needs the Cave to carry what sits in its pocket. */
static separation_c * solveNested(problem_c * pr, int extra) {
  pr->removeAllSolutions();
  solveThread_c solver(*pr, solveThread_c::PAR_DISASSM | extra);
  REQUIRE(solver.start());
  solver.waitUntilFinished();
  REQUIRE(solver.currentAction() == solveThread_c::ACT_FINISHED);
  if (pr->getNumberOfSavedSolutions() == 0)
    return nullptr;
  return const_cast<separation_c *>(pr->getSavedSolution(0)->getDisassembly());
}

TEST_CASE("sliding: the nested puzzle has no solution without nested slides", "[sliding][solver]") {
  std::unique_ptr<puzzle_c> puzzle = puzzle_c::load("test/test_sliding_nested.xmpuzzle");
  REQUIRE(puzzle != nullptr);
  REQUIRE(puzzle->getGridType()->getType() == gridType_c::GT_SLIDING);
  problem_c * pr = puzzle->getProblem(0);

  CHECK(solveNested(pr, 0) == nullptr);
  CHECK(pr->getNumAssemblies() == 1);
  CHECK(pr->getNumSolutions() == 0);

  /* The search proved it: every arrangement was visited well inside the
   * budget, so the deep solver finds nothing either. */
  CHECK(solveNested(pr, solveThread_c::PAR_DEEP_SEARCH) == nullptr);
}

TEST_CASE("sliding: the nested puzzle solves with nested slides", "[sliding][solver]") {
  std::unique_ptr<puzzle_c> puzzle = puzzle_c::load("test/test_sliding_nested.xmpuzzle");
  REQUIRE(puzzle != nullptr);
  problem_c * pr = puzzle->getProblem(0);

  separation_c * path = solveNested(pr, solveThread_c::PAR_NESTED_SLIDES);
  REQUIRE(path != nullptr);
  CHECK(pr->getNumSolutions() == 1);
  /* Fewest moves when a move is one piece, or a piece and what is nested
   * in it, going anywhere it can reach. */
  CHECK(path->getMoves() == 47);

  /* Every move is one rigid group with a route, and at least one carries
   * more than one piece. The animation needs the route; a move whose
   * pieces shift by different amounts would have none. */
  unsigned int carried = 0;
  for (unsigned int step = 0; step < path->getMoves(); step++) {
    std::vector<unsigned int> movers;
    auto route = sliding::slideRoute(*pr, *path, step, &movers);
    INFO("step " << step);
    REQUIRE(route.size() >= 2);
    if (movers.size() > 1)
      carried++;
  }
  CHECK(carried > 0);

  /* The Smile ends in the Cave's pocket, as the goal map asks. */
  const state_c * last = path->getState(path->getMoves());
  const state_c * first = path->getState(0);
  CHECK((last->getX(4) - first->getX(4)) == (last->getX(5) - first->getX(5)));
  CHECK((last->getY(4) - first->getY(4)) == (last->getY(5) - first->getY(5)));
}

/* A 3x2 tray, two layers deep, and a 1x1 piece two layers tall. The bottom
 * layer is all floor; the top layer has a wall at (1,0), and with `blocked`
 * also at (1,1). Checking only the bottom layer, the piece would slide
 * straight from (0,0) to its goal at (2,0). With both layers it must go
 * round through row 1, or cannot go at all. */
static std::unique_ptr<puzzle_c> twoLayerTray(bool blocked) {
  std::string top1 = blocked ? "#_#" : "###";
  std::string xml =
      "<?xml version=\"1.0\"?>\n"
      "<puzzle version=\"2\"><gridType type=\"5\"/>"
      "<colors><color red=\"40\" green=\"70\" blue=\"100\"/>"
      "<color red=\"70\" green=\"100\" blue=\"40\"/></colors><shapes>"
      "<voxel x=\"3\" y=\"2\" z=\"2\" name=\"sg:1:\" goal=\"0,0,2,0,0,0,0,0,2,0,0,0\" type=\"0\">"
      "#2######2_#" + top1 + "</voxel>"
      "<voxel x=\"1\" y=\"1\" z=\"2\" name=\"A\" type=\"0\">#2#2</voxel>"
      "</shapes><problems><problem name=\"START/GOAL 1\" state=\"0\"><shapes>"
      "<shape id=\"1\" count=\"1\"/></shapes><result id=\"0\"/><bitmap/>"
      "</problem></problems><comment/></puzzle>";
  std::stringstream in(xml);
  xmlParser_c pars(in);
  return std::make_unique<puzzle_c>(pars);
}

TEST_CASE("sliding: every layer of a deeper tray blocks a move", "[sliding]") {
  std::unique_ptr<puzzle_c> open = twoLayerTray(false);
  problem_c * pr = open->getProblem(0);
  REQUIRE(pr->resultValid());
  assembly_c start(open->getGridType());
  start.addPlacement(0, 0, 0, 0);

  auto path = sliding::findSlidePath(*pr, start);
  REQUIRE(path != nullptr);
  REQUIRE(path->getMoves() == 1);
  CHECK(path->getState(1)->getX(0) == 2);
  CHECK(path->getState(1)->getY(0) == 0);
  /* Round the top-layer wall: down, across, up. */
  std::vector<unsigned int> movers;
  auto route = sliding::slideRoute(*pr, *path, 0, &movers);
  CHECK(route.size() == 4);

  std::unique_ptr<puzzle_c> shut = twoLayerTray(true);
  CHECK(sliding::findSlidePath(*shut->getProblem(0), start) == nullptr);
}

TEST_CASE("sliding: a search says whether it finished, hit its limit or was stopped", "[sliding]") {
  std::unique_ptr<puzzle_c> puzzle = puzzle_c::load("test/test_sliding_nested.xmpuzzle");
  REQUIRE(puzzle != nullptr);
  problem_c * pr = puzzle->getProblem(0);
  sliding::refreshStartLocks(*pr);

  /* The start layout from the file: four blocks along the top, the Cave
   * and the Smile in its pocket below. Parts are the blocks, Cave, Smile. */
  assembly_c start(puzzle->getGridType());
  for (auto p : std::vector<std::pair<int, int>>{{0, 0}, {3, 0}, {6, 0}, {9, 0}, {3, 3}, {3, 4}})
    start.addPlacement(0, p.first, p.second, 0);

  sliding::slideSearch_c full;
  full.maxStates = sliding::FULL_SEARCH;
  std::atomic<unsigned long> progress{0};
  full.progress = &progress;
  CHECK(sliding::findSlidePath(*pr, start, full) == nullptr);
  CHECK(full.outcome == sliding::SLIDE_NO_PATH);
  CHECK(full.visited > 0);
  CHECK(progress.load() == full.visited);

  sliding::slideSearch_c small;
  small.maxStates = 10;
  CHECK(sliding::findSlidePath(*pr, start, small) == nullptr);
  CHECK(small.outcome == sliding::SLIDE_LIMIT);

  std::atomic<bool> stop{true};
  sliding::slideSearch_c stopped;
  stopped.maxStates = sliding::FULL_SEARCH;
  stopped.stop = &stop;
  CHECK(sliding::findSlidePath(*pr, start, stopped) == nullptr);
  CHECK(stopped.outcome == sliding::SLIDE_STOPPED);
}

TEST_CASE("sliding: a full search stops at its memory limit", "[sliding]") {
  std::unique_ptr<puzzle_c> puzzle = puzzle_c::load("test/test_sliding_nested.xmpuzzle");
  REQUIRE(puzzle != nullptr);
  problem_c * pr = puzzle->getProblem(0);
  sliding::refreshStartLocks(*pr);

  assembly_c start(puzzle->getGridType());
  for (auto p : std::vector<std::pair<int, int>>{{0, 0}, {3, 0}, {6, 0}, {9, 0}, {3, 3}, {3, 4}})
    start.addPlacement(0, p.first, p.second, 0);

  sliding::slideSearch_c tight;
  tight.maxStates = sliding::FULL_SEARCH;
  tight.maxMemoryStates = 10;
  CHECK(sliding::findSlidePath(*pr, start, tight) == nullptr);
  CHECK(tight.outcome == sliding::SLIDE_MEMORY);
  CHECK(tight.memoryStates == 10);

  /* The memory budget: about 2 GB normally, never less with high memory. */
  CHECK(sliding::memoryStates(200, false) == 10000000);
  CHECK(sliding::memoryStates(200, true) >= sliding::memoryStates(200, false));

  sliding::slideSearch_c normal;
  normal.maxStates = sliding::FULL_SEARCH;
  CHECK(sliding::findSlidePath(*pr, start, normal) == nullptr);
  CHECK(normal.outcome == sliding::SLIDE_NO_PATH);

  sliding::slideSearch_c high;
  high.maxStates = sliding::FULL_SEARCH;
  high.highMemory = true;
  CHECK(sliding::findSlidePath(*pr, start, high) == nullptr);
  CHECK(high.outcome == sliding::SLIDE_NO_PATH);
  CHECK(high.memoryStates >= normal.memoryStates);
  /* The position-table search packs more than 10 million into 2 GB. */
  if (!std::getenv("BURRTOOLS_SLIDE_LEGACY"))
    CHECK(normal.memoryStates > 10000000);
}

TEST_CASE("sliding: the solver says when no solution is proven", "[sliding][solver]") {
  std::unique_ptr<puzzle_c> puzzle = puzzle_c::load("test/test_sliding_nested.xmpuzzle");
  REQUIRE(puzzle != nullptr);
  problem_c * pr = puzzle->getProblem(0);
  pr->removeAllSolutions();

  solveThread_c solver(*pr, solveThread_c::PAR_DISASSM | solveThread_c::PAR_FULL_SEARCH);
  REQUIRE(solver.start());
  solver.waitUntilFinished();
  REQUIRE(solver.currentAction() == solveThread_c::ACT_FINISHED);
  CHECK(pr->getNumSolutions() == 0);
  const std::string note = solver.getSolverNote();
  CHECK(note.find("Every reachable arrangement was searched") != std::string::npos);
  CHECK(note.find("Allow Nested Slides") != std::string::npos);
}

/* Not part of the suite (hidden tag): a throughput and memory benchmark for
 * the position-table search. A 4x4 tray with fifteen unit pieces and the
 * last two swapped in the goal, which no sliding can reach, so the search
 * runs until its limit. Run with
 *   /usr/bin/time -l ./build/test_burrtools "[.bench]"
 */
TEST_CASE("sliding: benchmark, fifteen pieces in a 4x4 tray", "[.bench][sliding]") {
  puzzle_c puz(new gridType_c(gridType_c::GT_SLIDING));
  unsigned int trayId = sliding::addStartGoalShape(puz, 4, 4);
  voxel_c * tray = puz.getShape(trayId);
  for (unsigned int k = 0; k < 15; k++) {
    unsigned int id = puz.addShape(1, 1, 1);
    puz.getShape(id)->setState(0, 0, 0, voxel_c::VX_FILLED);
    tray->setColor(k % 4, k / 4, 0, id + 1);
    const unsigned int g = k == 13 ? 14 : k == 14 ? 13 : k;
    tray->setGoalPiece((unsigned)tray->getIndex(g % 4, g / 4, 0), id + 1);
  }
  sliding::syncSlidingProblems(puz);
  problem_c * pr = puz.getProblem(0);

  assembly_c start(puz.getGridType());
  for (unsigned int k = 0; k < 15; k++)
    start.addPlacement(0, k % 4, k / 4, 0);

  sliding::slideSearch_c search;
  search.maxStates = 3000000;
  CHECK(sliding::findSlidePath(*pr, start, search) == nullptr);
  /* The legacy search (BURRTOOLS_SLIDE_LEGACY) runs out of memory first. */
  CHECK((search.outcome == sliding::SLIDE_LIMIT || search.outcome == sliding::SLIDE_MEMORY));
}

namespace {

/* Set or clear an environment switch. MinGW does not declare setenv; an
 * empty value removes the variable on Windows. */
void slideEnv(const char * name, bool on) {
#ifdef _WIN32
  (void)_putenv_s(name, on ? "1" : "");
#else
  if (on) setenv(name, "1", 1);
  else unsetenv(name);
#endif
}

/* A sliding puzzle built from rectangles: each piece is its own shape,
 * w x h, with its top-left cell at (x, y); goal says where it must end, or
 * is negative when it may end anywhere. */
struct slideBlock_c {
  unsigned int w, h;
  int x, y;
  int goalX, goalY;
};

struct slideFixture_c {
  std::unique_ptr<puzzle_c> puz;
  std::unique_ptr<assembly_c> start;
  problem_c * pr = nullptr;
};

slideFixture_c slideFixture(unsigned int trayW, unsigned int trayH,
                            const std::vector<slideBlock_c> & blocks) {
  slideFixture_c f;
  f.puz = std::make_unique<puzzle_c>(new gridType_c(gridType_c::GT_SLIDING));
  unsigned int trayId = sliding::addStartGoalShape(*f.puz, trayW, trayH);
  voxel_c * tray = f.puz->getShape(trayId);
  f.start = std::make_unique<assembly_c>(f.puz->getGridType());
  for (const slideBlock_c & b : blocks) {
    unsigned int id = f.puz->addShape(b.w, b.h, 1);
    for (unsigned int y = 0; y < b.h; y++)
      for (unsigned int x = 0; x < b.w; x++) {
        f.puz->getShape(id)->setState(x, y, 0, voxel_c::VX_FILLED);
        tray->setColor(b.x + x, b.y + y, 0, id + 1);
        if (b.goalX >= 0)
          tray->setGoalPiece((unsigned)tray->getIndex(b.goalX + x, b.goalY + y, 0), id + 1);
      }
    f.start->addPlacement(0, b.x, b.y, 0);
  }
  sliding::syncSlidingProblems(*f.puz);
  f.pr = f.puz->getProblem(0);
  return f;
}

/* Every step of a slide path moves exactly one piece. */
bool oneMoverPerStep(const separation_c & path) {
  for (unsigned int step = 0; step < path.getMoves(); step++) {
    unsigned int moved = 0;
    for (unsigned int i = 0; i < path.getPieceNumber(); i++)
      if (path.getState(step)->getX(i) != path.getState(step + 1)->getX(i) ||
          path.getState(step)->getY(i) != path.getState(step + 1)->getY(i))
        moved++;
    if (moved != 1)
      return false;
  }
  return true;
}

/* Klotski, the classic "Heng Dao Li Ma" start: a 4x5 tray, the 2x2 block
 * to bring to the bottom centre past four upright 1x2, one flat 2x1 and
 * four unit pieces. */
const std::vector<slideBlock_c> KLOTSKI = {
  {2, 2, 1, 0, 1, 3},
  {1, 2, 0, 0, -1, -1}, {1, 2, 3, 0, -1, -1}, {1, 2, 0, 2, -1, -1}, {1, 2, 3, 2, -1, -1},
  {2, 1, 1, 2, -1, -1},
  {1, 1, 1, 3, -1, -1}, {1, 1, 2, 3, -1, -1}, {1, 1, 0, 4, -1, -1}, {1, 1, 3, 4, -1, -1},
};

} // namespace

/* The published answer for Klotski, counting one piece going anywhere it
 * can reach as one move, is 81; the puzzle has 25,955 arrangements when
 * like pieces are not told apart. */
TEST_CASE("sliding: Klotski takes 81 moves, like pieces searched as one", "[sliding]") {
  /* Ten million text keys when the pieces are told apart: too much for the
   * legacy search in a test. */
  if (std::getenv("BURRTOOLS_SLIDE_LEGACY"))
    return;
  slideFixture_c f = slideFixture(4, 5, KLOTSKI);
  sliding::slideSearch_c search;
  search.maxStates = sliding::FULL_SEARCH;
  std::unique_ptr<separation_c> path = sliding::findSlidePath(*f.pr, *f.start, search);
  REQUIRE(path != nullptr);
  CHECK(path->getMoves() == 81);
  CHECK(oneMoverPerStep(*path));
  if (!std::getenv("BURRTOOLS_NO_SLIDE_SYMMETRY"))
    CHECK(search.visited <= 25955);
  /* The path starts where the pieces were put and ends at the goal. */
  for (unsigned int i = 0; i < KLOTSKI.size(); i++) {
    CHECK(path->getState(0)->getX(i) == KLOTSKI[i].x);
    CHECK(path->getState(0)->getY(i) == KLOTSKI[i].y);
  }
  CHECK(path->getState(81)->getX(0) == 1);
  CHECK(path->getState(81)->getY(0) == 3);
}

/* Searching like pieces as one must not change the answer: same number of
 * moves as telling them apart, and as the legacy search. */
TEST_CASE("sliding: like pieces searched as one give the same shortest path", "[sliding]") {
  /* A 4x3 tray: a 2x1 bar to take from the top left to the bottom right
   * through six unit pieces, two of them with goals of their own. */
  const std::vector<slideBlock_c> blocks = {
    {2, 1, 0, 0, 2, 2},
    {1, 1, 2, 0, -1, -1}, {1, 1, 3, 0, -1, -1}, {1, 1, 0, 1, -1, -1}, {1, 1, 1, 1, -1, -1},
    {1, 1, 2, 1, 0, 0}, {1, 1, 0, 2, 3, 0},
  };
  unsigned int moves[3] = {0, 0, 0};
  unsigned long visited[3] = {0, 0, 0};
  const char * switches[3] = {nullptr, "BURRTOOLS_NO_SLIDE_SYMMETRY", "BURRTOOLS_SLIDE_LEGACY"};
  /* A switch the run was started with stays on throughout. */
  const bool legacyAsked = std::getenv("BURRTOOLS_SLIDE_LEGACY") != nullptr;
  const bool apartAsked = std::getenv("BURRTOOLS_NO_SLIDE_SYMMETRY") != nullptr;
  const bool asked[3] = {false, apartAsked, legacyAsked};
  for (int k = 0; k < 3; k++) {
    slideFixture_c f = slideFixture(4, 3, blocks);
    if (switches[k])
      slideEnv(switches[k], true);
    sliding::slideSearch_c search;
    search.maxStates = sliding::FULL_SEARCH;
    std::unique_ptr<separation_c> path = sliding::findSlidePath(*f.pr, *f.start, search);
    if (switches[k] && !asked[k])
      slideEnv(switches[k], false);
    REQUIRE(path != nullptr);
    CHECK(oneMoverPerStep(*path));
    moves[k] = path->getMoves();
    visited[k] = search.visited;
  }
  CHECK(moves[0] == moves[1]);
  CHECK(moves[0] == moves[2]);
  if (!legacyAsked && !apartAsked)
    CHECK(visited[0] < visited[1]);
}

/* Hidden: Klotski searched to the end, with like pieces as one and told
 * apart. Run with ./build/test_burrtools "[.bench][klotski]". */
TEST_CASE("sliding: benchmark, every Klotski arrangement", "[.bench][sliding][klotski]") {
  for (int k = 0; k < 2; k++) {
    slideFixture_c f = slideFixture(4, 5, KLOTSKI);
    slideEnv("BURRTOOLS_NO_SLIDE_SYMMETRY", k == 1);
    sliding::slideSearch_c search;
    search.maxStates = sliding::FULL_SEARCH;
    const auto t0 = std::chrono::steady_clock::now();
    std::unique_ptr<separation_c> path = sliding::findSlidePath(*f.pr, *f.start, search);
    const double s = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    printf("klotski %s: %u moves, %lu arrangements visited, %.3f s\n",
           k ? "pieces told apart" : "like pieces as one",
           path ? path->getMoves() : 0, search.visited, s);
  }
  slideEnv("BURRTOOLS_NO_SLIDE_SYMMETRY", false);
}

/* Thirteen pieces with 32 places each need 65 bits to number an
 * arrangement: one more than a 64-bit key holds. The table search then runs
 * on 128-bit keys and must agree with the legacy search. */
TEST_CASE("sliding: a tray too big for 64-bit keys is searched with 128-bit ones", "[sliding]") {
  std::vector<slideBlock_c> blocks = {
    {1, 1, 0, 0, 7, 3}, {1, 1, 7, 3, 0, 0},
  };
  for (int k = 0; k < 11; k++)
    blocks.push_back({1, 1, 1 + k % 6, 1 + k / 6, -1, -1});
  const bool legacyAsked = std::getenv("BURRTOOLS_SLIDE_LEGACY") != nullptr;

  slideFixture_c f = slideFixture(8, 4, blocks);
  sliding::slideSearch_c search;
  search.maxStates = sliding::FULL_SEARCH;
  std::unique_ptr<separation_c> path = sliding::findSlidePath(*f.pr, *f.start, search);
  REQUIRE(path != nullptr);
  CHECK(oneMoverPerStep(*path));
  CHECK(path->getMoves() == 3);
  if (!legacyAsked)
    CHECK(search.memoryStates == sliding::memoryStates(3 * 16, false));

  slideFixture_c g = slideFixture(8, 4, blocks);
  slideEnv("BURRTOOLS_SLIDE_LEGACY", true);
  sliding::slideSearch_c legacy;
  legacy.maxStates = sliding::FULL_SEARCH;
  std::unique_ptr<separation_c> old = sliding::findSlidePath(*g.pr, *g.start, legacy);
  if (!legacyAsked)
    slideEnv("BURRTOOLS_SLIDE_LEGACY", false);
  REQUIRE(old != nullptr);
  CHECK(old->getMoves() == path->getMoves());
}

TEST_CASE("sliding: a key set finds what was put in it, on disk and in memory", "[sliding]") {
  const std::filesystem::path dir =
      std::filesystem::temp_directory_path() / "burrtools_test_keyset";
  std::filesystem::create_directories(dir);
  for (int onDisk = 0; onDisk < 2; onDisk++) {
    /* Keys spread over all 128 bits, a few close together, sorted. */
    std::vector<unsigned __int128> keys;
    uint64_t x = 88172645463325252ull;
    auto rnd = [&x]() {
      x ^= x << 13;
      x ^= x >> 7;
      x ^= x << 17;
      return x;
    };
    for (int i = 0; i < 5000; i++) {
      unsigned __int128 k = ((unsigned __int128)rnd() << 64) | rnd();
      if (i % 3 == 0)
        k = keys.empty() ? 0 : keys.back() + 1 + rnd() % 300;
      keys.push_back(k);
    }
    std::sort(keys.begin(), keys.end());
    keys.erase(std::unique(keys.begin(), keys.end()), keys.end());

    sliding::levels::keySet_c<unsigned __int128> set;
    REQUIRE(set.create(onDisk ? dir / "keys" : std::filesystem::path()));
    std::vector<unsigned __int128> in;
    std::vector<unsigned __int128> out;
    for (size_t i = 0; i < keys.size(); i++)
      (i % 2 ? out : in).push_back(keys[i]);
    REQUIRE(set.addAll(in.begin(), in.end()));
    REQUIRE(set.finish());
    CHECK(set.size() == in.size());

    std::vector<unsigned __int128> back;
    REQUIRE(set.load(back));
    CHECK(back == in);

    /* Every key looked up at once: those put in are found, the others not. */
    std::vector<char> found(keys.size(), 0);
    sliding::levels::keySet_c<unsigned __int128>::reader_c reader(set);
    REQUIRE(set.find(keys.data(), keys.size(), found.data(), reader));
    for (size_t i = 0; i < keys.size(); i++)
      CHECK((found[i] != 0) == (i % 2 == 0));
  }
  std::filesystem::remove_all(dir);
}

namespace {

/* A 5x5 tray with a C-shaped piece open to the right, a unit piece A in
 * its pocket, three more unit pieces and two 2x2 blocks:
 *   CCCu.
 *   CA..u
 *   CCC..
 *   BBDDu
 *   BBDD.
 * Carrying A, the C can close round a unit piece it passes, and then the
 * three move together: the C cannot move back without it. The C's goal
 * takes in the variable bottom right cell, where no piece may stop, so no
 * search finds a path and every one searches all there is. */
std::unique_ptr<puzzle_c> oneWayPuzzle(problem_c *& pr, std::unique_ptr<assembly_c> & start) {
  auto puz = std::make_unique<puzzle_c>(new gridType_c(gridType_c::GT_SLIDING));
  voxel_c * tray = puz->getShape(sliding::addStartGoalShape(*puz, 5, 5));
  start = std::make_unique<assembly_c>(puz->getGridType());
  struct piece_c {
    std::vector<std::pair<int, int>> cells;
    int x, y;
  };
  const std::vector<std::pair<int, int>> unit = {{0, 0}};
  const std::vector<std::pair<int, int>> block = {{0, 0}, {1, 0}, {0, 1}, {1, 1}};
  const std::vector<piece_c> pieces = {
    {{{0, 0}, {1, 0}, {2, 0}, {0, 1}, {0, 2}, {1, 2}, {2, 2}}, 0, 0},
    {unit, 1, 1}, {unit, 4, 1}, {block, 2, 3}, {unit, 3, 0}, {unit, 4, 3}, {block, 0, 3},
  };
  for (const piece_c & pc : pieces) {
    unsigned int id = puz->addShape(3, 3, 1);
    for (auto c : pc.cells) {
      puz->getShape(id)->setState(c.first, c.second, 0, voxel_c::VX_FILLED);
      tray->setColor(pc.x + c.first, pc.y + c.second, 0, id + 1);
      /* The C's goal: the bottom right corner. */
      if (&pc == &pieces[0])
        tray->setGoalPiece((unsigned)tray->getIndex(2 + c.first, 2 + c.second, 0), id + 1);
    }
    start->addPlacement(0, pc.x, pc.y, 0);
  }
  tray->setState(4, 4, 0, voxel_c::VX_VARIABLE);
  sliding::syncSlidingProblems(*puz);
  pr = puz->getProblem(0);
  return puz;
}

} // namespace

/* The full search looks for an arrangement reached by a move that cannot be
 * made backwards in every level before, the others only in the last two.
 * It must find exactly the arrangements the queue search does, which keeps
 * them all. */
TEST_CASE("sliding: the full search counts the same arrangements with one-way moves", "[sliding]") {
  problem_c * pr = nullptr;
  std::unique_ptr<assembly_c> start;
  std::unique_ptr<puzzle_c> puz = oneWayPuzzle(pr, start);
  const bool queueAsked = std::getenv("BURRTOOLS_SLIDE_QUEUE") != nullptr;

  unsigned long visited[2] = {0, 0};
  for (int nested = 0; nested < 2; nested++)
    for (int k = 0; k < 2; k++) {
      if (k == 1)
        slideEnv("BURRTOOLS_SLIDE_QUEUE", true);
      sliding::slideSearch_c search;
      search.maxStates = sliding::FULL_SEARCH;
      search.nested = nested != 0;
      CHECK(sliding::findSlidePath(*pr, *start, search) == nullptr);
      if (k == 1 && !queueAsked)
        slideEnv("BURRTOOLS_SLIDE_QUEUE", false);
      CHECK(search.outcome == sliding::SLIDE_NO_PATH);
      visited[k] = search.visited;
      /* With nested slides the puzzle has one-way moves that lead back to
       * arrangements reached two or more moves before. */
      if (k == 0 && nested && !queueAsked)
        CHECK(search.oneWayRepeats > 0);
      if (k == 1) {
        CHECK(visited[0] == visited[1]);
        CHECK(visited[0] > 100);
      }
    }
}

/* With nowhere to write, the levels stay in memory; with no disk to spare,
 * the search says so. */
TEST_CASE("sliding: the full search keeps its levels where it can", "[sliding]") {
  problem_c * pr = nullptr;
  std::unique_ptr<assembly_c> start;
  std::unique_ptr<puzzle_c> puz = oneWayPuzzle(pr, start);
  if (std::getenv("BURRTOOLS_SLIDE_QUEUE"))
    return;
  const std::filesystem::path dir =
      std::filesystem::temp_directory_path() / "burrtools_test_slidedir";
  std::filesystem::remove_all(dir);

  sliding::slideSearch_c onDisk;
  onDisk.maxStates = sliding::FULL_SEARCH;
  onDisk.nested = true;
  onDisk.workDir = dir.string();
  CHECK(sliding::findSlidePath(*pr, *start, onDisk) == nullptr);
  CHECK(onDisk.outcome == sliding::SLIDE_NO_PATH);
  CHECK(onDisk.diskBytes > 0);
  CHECK(onDisk.depth > 2);
  /* The search's own folder goes when it ends. */
  CHECK(std::filesystem::is_empty(dir));

  /* A file where the folder should be: nowhere to write. */
  std::filesystem::remove_all(dir);
  { std::ofstream(dir.string()) << "x"; }
  sliding::slideSearch_c inMemory;
  inMemory.maxStates = sliding::FULL_SEARCH;
  inMemory.nested = true;
  inMemory.workDir = dir.string();
  CHECK(sliding::findSlidePath(*pr, *start, inMemory) == nullptr);
  CHECK(inMemory.outcome == sliding::SLIDE_NO_PATH);
  CHECK(inMemory.diskBytes == 0);
  CHECK(inMemory.visited == onDisk.visited);
  std::filesystem::remove_all(dir);

  sliding::slideSearch_c full;
  full.maxStates = sliding::FULL_SEARCH;
  full.nested = true;
  full.workDir = dir.string();
  full.diskBudget = 1;
  CHECK(sliding::findSlidePath(*pr, *start, full) == nullptr);
  CHECK(full.outcome == sliding::SLIDE_DISK);
  CHECK(!full.error.empty());
  std::filesystem::remove_all(dir);
}

/* The full search runs on every core, but finds the same path each time:
 * as many moves as the queue search, through the same arrangements. */
TEST_CASE("sliding: the full search finds the same path on any number of threads", "[sliding]") {
  std::vector<std::vector<int>> paths;
  for (unsigned int threads : {1u, 3u, 8u}) {
    slideFixture_c f = slideFixture(4, 5, KLOTSKI);
    sliding::slideSearch_c search;
    search.maxStates = sliding::FULL_SEARCH;
    search.threads = threads;
    std::unique_ptr<separation_c> path = sliding::findSlidePath(*f.pr, *f.start, search);
    REQUIRE(path != nullptr);
    CHECK(path->getMoves() == 81);
    CHECK(oneMoverPerStep(*path));
    std::vector<int> places;
    for (unsigned int s = 0; s <= path->getMoves(); s++)
      for (unsigned int i = 0; i < path->getPieceNumber(); i++) {
        places.push_back(path->getState(s)->getX(i));
        places.push_back(path->getState(s)->getY(i));
      }
    paths.push_back(places);
  }
  CHECK(paths[0] == paths[1]);
  CHECK(paths[0] == paths[2]);
}


/* Hidden: random trays with a C-shaped piece that can close round others,
 * each searched to the end, or to the goal, by the queue search and the
 * full search. They must agree on every arrangement there is, or on the
 * moves to the goal. Run with ./build/test_burrtools "[.fuzz][sliding]";
 * BT_SLIDE_FUZZ_TRIALS sets how many. */
TEST_CASE("sliding: fuzz, the full search agrees with the queue search", "[.fuzz][sliding]") {
  const char * e = std::getenv("BT_SLIDE_FUZZ_TRIALS");
  const int trials = e ? std::atoi(e) : 300;
  uint64_t x = 1234567;
  auto rnd = [&x](int n) {
    x ^= x << 13;
    x ^= x >> 7;
    x ^= x << 17;
    return (int)(x % (uint64_t)n);
  };
  const bool queueAsked = std::getenv("BURRTOOLS_SLIDE_QUEUE") != nullptr;
  int compared = 0, found = 0, repeats = 0;
  for (int trial = 0; trial < trials; trial++) {
    const int W = 5 + rnd(3), H = 4 + rnd(3);
    const bool reachable = rnd(2) == 0;
    auto puz = std::make_unique<puzzle_c>(new gridType_c(gridType_c::GT_SLIDING));
    voxel_c * tray = puz->getShape(sliding::addStartGoalShape(*puz, W, H));
    auto start = std::make_unique<assembly_c>(puz->getGridType());
    std::vector<std::vector<int>> occ((size_t)W, std::vector<int>((size_t)H, 0));
    struct piece_c {
      std::vector<std::pair<int, int>> cells;
      int x, y;
    };
    std::vector<piece_c> pieces = {
      {{{0, 0}, {1, 0}, {2, 0}, {0, 1}, {0, 2}, {1, 2}, {2, 2}}, 0, 0},
      {{{0, 0}}, 1, 1},
    };
    for (const piece_c & pc : pieces)
      for (auto c : pc.cells)
        occ[(size_t)(pc.x + c.first)][(size_t)(pc.y + c.second)] = 1;
    const std::vector<std::vector<std::pair<int, int>>> shapes = {
      {{0, 0}}, {{0, 0}, {1, 0}}, {{0, 0}, {0, 1}}, {{0, 0}, {1, 0}, {0, 1}, {1, 1}}};
    for (int extra = 2 + rnd(5); extra > 0; extra--) {
      const auto & shape = shapes[(size_t)rnd(4)];
      for (int tries = 0; tries < 50; tries++) {
        const int px = rnd(W), py = rnd(H);
        bool fits = true;
        for (auto c : shape)
          if (px + c.first >= W || py + c.second >= H || occ[(size_t)(px + c.first)][(size_t)(py + c.second)])
            fits = false;
        if (!fits)
          continue;
        for (auto c : shape)
          occ[(size_t)(px + c.first)][(size_t)(py + c.second)] = 1;
        pieces.push_back({shape, px, py});
        break;
      }
    }
    for (size_t i = 0; i < pieces.size(); i++) {
      unsigned int id = puz->addShape(3, 3, 1);
      for (auto c : pieces[i].cells) {
        puz->getShape(id)->setState(c.first, c.second, 0, voxel_c::VX_FILLED);
        tray->setColor(pieces[i].x + c.first, pieces[i].y + c.second, 0, id + 1);
        if (i == 0)
          tray->setGoalPiece((unsigned)tray->getIndex(W - 3 + c.first, H - 3 + c.second, 0), id + 1);
      }
      start->addPlacement(0, pieces[i].x, pieces[i].y, 0);
    }
    /* A variable cell in the C's goal: no path, and both search it all. */
    if (!reachable) {
      if (occ[(size_t)(W - 1)][(size_t)(H - 1)])
        continue;
      tray->setState(W - 1, H - 1, 0, voxel_c::VX_VARIABLE);
    }
    sliding::syncSlidingProblems(*puz);
    problem_c * pr = puz->getProblem(0);

    sliding::slideSearch_c level;
    level.maxStates = sliding::FULL_SEARCH;
    level.nested = true;
    level.threads = 1 + (unsigned)rnd(4);
    level.maxMemoryStates = 3000000;
    std::unique_ptr<separation_c> levelPath = sliding::findSlidePath(*pr, *start, level);
    if (level.outcome == sliding::SLIDE_MEMORY)
      continue;

    slideEnv("BURRTOOLS_SLIDE_QUEUE", true);
    sliding::slideSearch_c queue;
    queue.maxStates = sliding::FULL_SEARCH;
    queue.nested = true;
    queue.maxMemoryStates = 3000000;
    std::unique_ptr<separation_c> queuePath = sliding::findSlidePath(*pr, *start, queue);
    if (!queueAsked)
      slideEnv("BURRTOOLS_SLIDE_QUEUE", false);
    if (queue.outcome == sliding::SLIDE_MEMORY)
      continue;

    INFO("trial " << trial);
    CHECK(level.outcome == queue.outcome);
    if (level.outcome == sliding::SLIDE_NO_PATH)
      CHECK(level.visited == queue.visited);
    if (levelPath && queuePath) {
      CHECK(levelPath->getMoves() == queuePath->getMoves());
      found++;
    }
    compared++;
    if (level.oneWayRepeats > 0)
      repeats++;
  }
  printf("slide fuzz: %d compared, %d with a path, %d with one-way repeats\n", compared, found, repeats);
}

/* The Start/Goal tab places a whole piece at a time, and says the shape is
 * valid once a piece has both a start and a goal, each the whole piece. */
TEST_CASE("sliding: a whole piece is placed as a start or a goal", "[sliding]") {
  /* A 5x3 tray, an L of three cells and a unit piece, no stamps yet. */
  slideFixture_c f = slideFixture(5, 3, {});
  voxel_c * tray = f.puz->getShape(0);
  REQUIRE(sliding::isStartGoalShape(tray));
  const unsigned int L = f.puz->addShape(2, 2, 1);
  f.puz->getShape(L)->setState(0, 0, 0, voxel_c::VX_FILLED);
  f.puz->getShape(L)->setState(1, 0, 0, voxel_c::VX_FILLED);
  f.puz->getShape(L)->setState(0, 1, 0, voxel_c::VX_FILLED);
  const unsigned int U = f.puz->addShape(1, 1, 1);
  f.puz->getShape(U)->setState(0, 0, 0, voxel_c::VX_FILLED);
  const voxel_c * l = f.puz->getShape(L);
  const voxel_c * u = f.puz->getShape(U);

  CHECK(!sliding::startGoalError(*f.puz, 0).empty());

  /* The L's top-left cell goes where it is put: the cell above its corner. */
  CHECK(sliding::stampPiece(tray, l, L, 1, 1, false).empty());
  CHECK(sliding::stampAt(tray, 1, 0, false) == L);
  CHECK(sliding::stampAt(tray, 2, 0, false) == L);
  CHECK(sliding::stampAt(tray, 1, 1, false) == L);
  CHECK(sliding::stampAt(tray, 2, 1, false) == (unsigned int)-1);
  /* A start alone is not enough. */
  CHECK(!sliding::startGoalError(*f.puz, 0).empty());

  /* It is exactly there; one cell along it is not, and placing it there
   * moves it, over cells it covered itself. */
  CHECK(sliding::stampIsAt(tray, l, L, 1, 1, false));
  CHECK(!sliding::stampIsAt(tray, l, L, 2, 1, false));
  CHECK(sliding::stampFits(tray, l, L, 2, 1, false).empty());
  CHECK(sliding::stampPiece(tray, l, L, 2, 1, false).empty());
  CHECK(sliding::stampAt(tray, 1, 0, false) == (unsigned int)-1);
  CHECK(sliding::stampAt(tray, 3, 0, false) == L);

  /* Placing it again moves it. */
  CHECK(sliding::stampPiece(tray, l, L, 0, 2, false).empty());
  CHECK(sliding::stampAt(tray, 1, 0, false) == (unsigned int)-1);
  CHECK(sliding::stampAt(tray, 0, 2, false) == L);

  /* Off the tray, or onto another piece's start: nothing changes. */
  CHECK(!sliding::stampPiece(tray, l, L, 4, 0, false).empty());
  CHECK(sliding::stampPiece(tray, u, U, 4, 1, false).empty());
  CHECK(!sliding::stampPiece(tray, l, L, 3, 2, false).empty());
  CHECK(sliding::stampAt(tray, 0, 1, false) == L);

  /* A goal on a variable cell is refused: no piece may stop there. */
  tray->setState(4, 0, 0, voxel_c::VX_VARIABLE);
  CHECK(!sliding::stampPiece(tray, u, U, 4, 0, true).empty());

  /* A goal without a start, then both: valid. */
  CHECK(sliding::clearStamp(tray, U, false));
  CHECK(sliding::stampPiece(tray, u, U, 3, 0, true).empty());
  CHECK(sliding::startGoalError(*f.puz, 0).find("no start") != std::string::npos);
  CHECK(sliding::stampPiece(tray, u, U, 4, 2, false).empty());
  CHECK(sliding::startGoalError(*f.puz, 0).empty());

  /* A start that is only part of the piece is not valid. */
  tray->setColor(1, 1, 0, 0);
  CHECK(sliding::startGoalError(*f.puz, 0).find("not the whole piece") != std::string::npos);
}

/* Sliding shapes lie flat: a piece or a tray with more layers keeps only
 * the first. */
TEST_CASE("sliding: a piece of more than one layer is made flat", "[sliding]") {
  slideFixture_c f = slideFixture(4, 3, {{1, 1, 0, 0, 3, 2}});
  voxel_c * tray = f.puz->getShape(0);
  tray->resize(4, 3, 2, voxel_c::VX_FILLED);
  const unsigned int deep = f.puz->addShape(2, 1, 3);
  voxel_c * v = f.puz->getShape(deep);
  v->setState(0, 0, 0, voxel_c::VX_FILLED);
  v->setState(1, 0, 2, voxel_c::VX_FILLED);

  const std::vector<unsigned int> changed = sliding::flattenPieces(*f.puz);
  CHECK(changed == std::vector<unsigned int>{0, deep});
  CHECK(v->getZ() == 1);
  CHECK(v->getState(0, 0, 0) == voxel_c::VX_FILLED);
  CHECK(v->getState(1, 0, 0) == voxel_c::VX_EMPTY);
  CHECK(tray->getZ() == 1);
  CHECK(sliding::isStartGoalShape(tray));
  CHECK(f.puz->getShape(1)->getZ() == 1);
  CHECK(sliding::flattenPieces(*f.puz).empty());
}

/* A piece whose shape changes loses its start and goal; one that is only
 * moved in its grid keeps them. */
TEST_CASE("sliding: a piece that changes shape loses its start and goal", "[sliding]") {
  slideFixture_c f = slideFixture(4, 3, {});
  voxel_c * tray = f.puz->getShape(0);
  const unsigned int bar = f.puz->addShape(3, 2, 1);
  voxel_c * v = f.puz->getShape(bar);
  v->setState(0, 0, 0, voxel_c::VX_FILLED);
  v->setState(1, 0, 0, voxel_c::VX_FILLED);
  REQUIRE(sliding::stampPiece(tray, v, bar, 0, 0, false).empty());
  REQUIRE(sliding::stampPiece(tray, v, bar, 2, 2, true).empty());

  /* moved one row up in its own grid: still the same shape */
  v->setState(0, 0, 0, voxel_c::VX_EMPTY);
  v->setState(1, 0, 0, voxel_c::VX_EMPTY);
  v->setState(0, 1, 0, voxel_c::VX_FILLED);
  v->setState(1, 1, 0, voxel_c::VX_FILLED);
  CHECK(sliding::dropChangedStamps(*f.puz, bar).empty());
  CHECK(sliding::stampAt(tray, 0, 0, false) == bar);

  /* a cell longer: both go */
  v->setState(2, 1, 0, voxel_c::VX_FILLED);
  CHECK(sliding::dropChangedStamps(*f.puz, bar) == std::vector<unsigned int>{0});
  CHECK(sliding::stampAt(tray, 0, 0, false) == (unsigned int)-1);
  CHECK(sliding::stampAt(tray, 2, 2, true) == (unsigned int)-1);
}
