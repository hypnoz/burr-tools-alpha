#include <catch2/catch_test_macros.hpp>

#include "lib/bt_assert.h"
#include "lib/disassembly.h"
#include "lib/problem.h"
#include "lib/puzzle.h"
#include "lib/solution.h"
#include "lib/solvethread.h"
#include "lib/solvertype.h"

#include <chrono>
#include <string>

namespace {

/* Re-solve test/puzzles/EnigmaTIC.xmpuzzle with Check Rotations on.
   The file already stores one rotation solution; that copy is cleared
   first so the assertions describe what this solver finds, not what
   was baked into the file. Ordinary fixture loads do not do this:
   opening a puzzle does not filter rotated assemblies. */
void requireEnigmaRotationSolution(solverType_e type) {
  std::unique_ptr<puzzle_c> puzzle = puzzle_c::load("test/puzzles/EnigmaTIC.xmpuzzle");
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
