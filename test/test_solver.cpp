#include <catch2/catch_test_macros.hpp>

#include "lib/puzzle.h"
#include "lib/problem.h"
#include "lib/assembler.h"
#include "lib/assembler_0.h"
#include "lib/assembler_1.h"
#include "lib/assembly.h"
#include "lib/bt2_assemble.h"
#include "lib/disassembler.h"
#include "lib/disassembler_0.h"
#include "lib/disassembly.h"
#include "lib/gridtype.h"
#include "lib/helperpool.h"
#include "lib/solvethread.h"
#include "lib/solvertype.h"
#include "lib/voxel.h"
#include "tools/xml.h"
#include "tools/gzstream.h"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <memory>
#include <algorithm>
#include <atomic>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

namespace {

class TestAssemblerCallback : public assembler_cb {
public:
  int assemblies{0};
  int solutions{0};
  std::string lastMoveLevel;
  disassembler_c * disassembler{nullptr};
  std::unique_ptr<assembly_c> firstSolutionAssembly;
  std::unique_ptr<assembly_c> firstAssembly;

  explicit TestAssemblerCallback(disassembler_c * d = nullptr) : disassembler(d) {}

  bool assembly(std::unique_ptr<assembly_c> a) override {
    assemblies++;
    if (!firstAssembly) {
      firstAssembly = std::make_unique<assembly_c>(a.get());
    }
    if (disassembler) {
      auto da = disassembler->disassemble(a.get());
      if (da) {
        solutions++;
        lastMoveLevel = da->movesText();
        if (!firstSolutionAssembly) {
          firstSolutionAssembly = std::make_unique<assembly_c>(a.get());
        }
      }
    }
    return true;
  }
};

struct SolveResult {
  int assemblies{0};
  int solutions{0};
  unsigned long long iterations{0};
  std::string moveLevel;
  std::unique_ptr<assembly_c> firstSolutionAssembly;
  std::unique_ptr<assembly_c> firstAssembly;
  const problem_c * problem{nullptr};
};

SolveResult solvePuzzle(const char * path, unsigned int problemIdx = 0, bool disassemble = true) {
  auto p = puzzle_c::load(path);
  REQUIRE(p != nullptr);

  REQUIRE(problemIdx < p->getNumberOfProblems());
  problem_c * problem = p->getProblem(problemIdx);
  REQUIRE(problem != nullptr);

  const gridType_c * gt = problem->getPuzzle().getGridType();
  REQUIRE(gt != nullptr);

  std::unique_ptr<assembler_c> assm = gt->findAssembler(*problem);
  REQUIRE(assm != nullptr);

  REQUIRE(assm->createMatrix(false, false, false) == assembler_c::ERR_NONE);

  std::unique_ptr<disassembler_c> disasm;
  if (disassemble && (gt->getCapabilities() & gridType_c::CAP_DISASSEMBLE)) {
    disasm = std::make_unique<disassembler_0_c>(*problem);
  }

  TestAssemblerCallback cb(disasm.get());
  assm->assemble(&cb);

  return SolveResult{
    cb.assemblies,
    cb.solutions,
    assm->getIterations(),
    cb.lastMoveLevel,
    std::move(cb.firstSolutionAssembly),
    std::move(cb.firstAssembly),
    problem
  };
}

/* MinGW does not declare setenv/unsetenv. An empty value removes the
   variable on Windows, matching unsetenv. */
#ifdef _WIN32
void setNoSimd(bool on) {
  (void)_putenv_s("BURRTOOLS_NO_SIMD", on ? "1" : "");
}
#else
void setNoSimd(bool on) {
  if (on) setenv("BURRTOOLS_NO_SIMD", "1", 1);
  else unsetenv("BURRTOOLS_NO_SIMD");
}
#endif

struct ForceNoSimd {
  ForceNoSimd() { setNoSimd(true); }
  ~ForceNoSimd() { setNoSimd(false); }
};

} // namespace

TEST_CASE("Pelikan Burr solver regression (GT_BRICKS)", "[solver][pelikan]") {
  SolveResult res = solvePuzzle("examples/PelikanBurr.xmpuzzle", 0, true);

  CHECK(res.assemblies == 12);
  CHECK(res.solutions == 1);
  CHECK(res.iterations > 0);
  // fork notation (one segment per single-piece removal); 0.7.1 prints "98.2.4.2"
  CHECK(res.moveLevel == "99.3.4.2");
  REQUIRE(res.firstSolutionAssembly != nullptr);
  CHECK(res.firstSolutionAssembly->placementCount() == 7);
}

TEST_CASE("Dracula's Dental Desaster solver regression (GT_BRICKS)", "[solver][dracula]") {
  SolveResult res = solvePuzzle("examples/DraculasDentalDesaster.xmpuzzle", 0, true);

  CHECK(res.assemblies == 84);
  CHECK(res.solutions == 1);
  CHECK(res.iterations > 0);
  REQUIRE(res.firstSolutionAssembly != nullptr);
  CHECK(res.firstSolutionAssembly->placementCount() == 9);
}

TEST_CASE("Prisgon solver regression (GT_BRICKS)", "[solver][prisgon]") {
  SolveResult res = solvePuzzle("examples/Prisgon.xmpuzzle", 0, true);

  CHECK(res.assemblies == 1);
  CHECK(res.solutions == 1);
  CHECK(res.iterations > 0);
  REQUIRE(res.firstSolutionAssembly != nullptr);
  CHECK(res.firstSolutionAssembly->placementCount() == 9);
}

TEST_CASE("Demo Mirror Paradox solver regression (GT_BRICKS / Assembler 1)", "[solver][mirrorparadox]") {
  SolveResult res = solvePuzzle("examples/DemoMirrorParadox.xmpuzzle", 0, true);

  CHECK(res.assemblies == 1);
  CHECK(res.solutions == 1);
  CHECK(res.iterations > 0);
  REQUIRE(res.firstSolutionAssembly != nullptr);
}

TEST_CASE("Cube in Cage solver regression (GT_BRICKS / Assembler 1)", "[solver][cubeincage]") {
  SolveResult res = solvePuzzle("examples/CubeInCage.xmpuzzle", 0, true);

  CHECK(res.assemblies == 96);
  CHECK(res.solutions == 1);
  CHECK(res.iterations > 0);
  REQUIRE(res.firstSolutionAssembly != nullptr);
}

TEST_CASE("Bermuda solver regression (GT_TRIANGULAR_PRISM)", "[solver][bermuda]") {
  SolveResult res = solvePuzzle("examples/Bermuda.xmpuzzle", 0, true);

  CHECK(res.assemblies == 1);
  CHECK(res.solutions == 1);
  CHECK(res.iterations > 0);
  REQUIRE(res.firstSolutionAssembly != nullptr);
}

TEST_CASE("Augmented Second Stellation assembly (GT_SPHERES)", "[solver][spheres]") {
  // Spheres grid supports assembly only, not disassembly
  SolveResult res = solvePuzzle("examples/AugmentedSecondStellation.xmpuzzle", 0, false);

  CHECK(res.assemblies == 2);
  CHECK(res.solutions == 0);
  CHECK(res.iterations > 0);
  REQUIRE(res.firstAssembly != nullptr);
}

TEST_CASE("Malformed XML input rejection", "[parser][malformed]") {
  SECTION("too_many_voxels.xmpuzzle must throw xmlParserException_c") {
    std::unique_ptr<std::istream> str(openGzFile("test/malformed/too_many_voxels.xmpuzzle"));
    REQUIRE(str != nullptr);
    xmlParser_c pars(*str);
    CHECK_THROWS_AS(puzzle_c(pars), xmlParserException_c);
  }

  SECTION("oversized_dimensions.xmpuzzle must throw xmlParserException_c") {
    std::unique_ptr<std::istream> str(openGzFile("test/malformed/oversized_dimensions.xmpuzzle"));
    REQUIRE(str != nullptr);
    xmlParser_c pars(*str);
    CHECK_THROWS_AS(puzzle_c(pars), xmlParserException_c);
  }

  SECTION("separation_before_assembly.xmpuzzle must throw xmlParserException_c") {
    std::unique_ptr<std::istream> str(openGzFile("test/malformed/separation_before_assembly.xmpuzzle"));
    REQUIRE(str != nullptr);
    xmlParser_c pars(*str);
    CHECK_THROWS_AS(puzzle_c(pars), xmlParserException_c);
  }
}

TEST_CASE("Puzzle metadata inspection and modern accessors", "[metadata]") {
  auto p = puzzle_c::load("examples/PelikanBurr.xmpuzzle");
  REQUIRE(p != nullptr);

  CHECK(p->getNumberOfProblems() == 1);
  CHECK(p->getComment().find("Pelikan Burr") != std::string::npos);
  CHECK(p->getProblem(0)->getNumberOfPieces() == 7);
  CHECK(p->getProblems().size() == 1);
  CHECK(p->getShapes().size() == p->getNumberOfShapes());
  CHECK(p->getShapes().size() == 8);
}

/* bt_assert and bt_assert_line do nothing in a build without assertions */
#ifndef NDEBUG
TEST_CASE("bt_assert throws assert_exception with C++20 source_location", "[assert]") {
  try {
    bt_assert(1 == 2);
    FAIL("bt_assert should have thrown assert_exception");
  } catch (const assert_exception & e) {
    CHECK(std::string(e.expr) == "1 == 2");
    CHECK(std::string(e.file).ends_with("test_solver.cpp"));
    CHECK(e.line > 0);
    CHECK(std::string(e.what()) == "1 == 2");
  }

  // Passing assertion does not throw
  CHECK_NOTHROW([&] { bt_assert(2 + 2 == 4); }());
}

TEST_CASE("assert_log correctly records lines", "[assert]") {
  REQUIRE(assert_log != nullptr);
  unsigned int initialLines = assert_log->lines();
  bt_assert_line("first assert log entry");
  bt_assert_line("second assert log entry");
  CHECK(assert_log->lines() == initialLines + 2);
  CHECK(std::string(assert_log->line(initialLines)) == "first assert log entry");
  CHECK(std::string(assert_log->line(initialLines + 1)) == "second assert log entry");
}
#endif

TEST_CASE("Symmetry calculation for non-cube grids with unaligned bounding boxes", "[symmetry]") {
  // Test GT_RHOMBIC whose voxel class (voxel_3_c) aligns bounding boxes to multiples of 5
  {
    gridType_c gt(gridType_c::GT_RHOMBIC);
    std::unique_ptr<voxel_c> v(gt.getVoxel(3, 3, 3, voxel_c::VX_EMPTY));
    REQUIRE(v != nullptr);
    for (unsigned int z = 0; z < 3; z++) {
      for (unsigned int y = 0; y < 3; y++) {
        for (unsigned int x = 0; x < 3; x++) {
          if (v->validCoordinate(x, y, z)) {
            v->set(x, y, z, voxel_c::VX_FILLED);
          }
        }
      }
    }
    CHECK_NOTHROW(v->selfSymmetries());
    CHECK(v->selfSymmetries() != 0);
  }

  // Test GT_TETRA_OCTA whose voxel class (voxel_4_c) aligns bounding boxes to multiples of 3
  {
    gridType_c gt(gridType_c::GT_TETRA_OCTA);
    std::unique_ptr<voxel_c> v(gt.getVoxel(2, 2, 2, voxel_c::VX_EMPTY));
    REQUIRE(v != nullptr);
    for (unsigned int z = 0; z < 2; z++) {
      for (unsigned int y = 0; y < 2; y++) {
        for (unsigned int x = 0; x < 2; x++) {
          if (v->validCoordinate(x, y, z)) {
            v->set(x, y, z, voxel_c::VX_FILLED);
          }
        }
      }
    }
    CHECK_NOTHROW(v->selfSymmetries());
    CHECK(v->selfSymmetries() != 0);
  }
}

TEST_CASE("Assembler early stop on false callback return and derived assemble overload visibility", "[assembler]") {
  auto p = puzzle_c::load("examples/PelikanBurr.xmpuzzle");
  REQUIRE(p != nullptr);
  auto problem = p->getProblem(0);
  REQUIRE(problem != nullptr);

  // Test assembler_0_c: calls assemble with lambda overload directly on derived assembler_0_c
  {
    assembler_0_c assm(*problem);
    REQUIRE(assm.createMatrix(false, false, false) == assembler_c::ERR_NONE);
    int count = 0;
    assm.assemble([&](std::unique_ptr<assembly_c>) -> bool {
      count++;
      return false; // Return false to stop immediately
    });
    CHECK(count == 1);
  }

  // Test assembler_1_c: calls assemble with lambda overload directly on derived assembler_1_c
  {
    assembler_1_c assm(*problem);
    REQUIRE(assm.createMatrix(false, false, false) == assembler_c::ERR_NONE);
    int count = 0;
    assm.assemble([&](std::unique_ptr<assembly_c>) -> bool {
      count++;
      return false; // Return false to stop immediately
    });
    CHECK(count == 1);
  }
}

TEST_CASE("problem_c::setAssembler takes std::unique_ptr and transfers ownership", "[problem]") {
  auto p = puzzle_c::load("examples/PelikanBurr.xmpuzzle");
  REQUIRE(p != nullptr);
  auto problem = p->getProblem(0);
  REQUIRE(problem != nullptr);
  problem->removeAllSolutions();

  auto assm = std::make_unique<assembler_0_c>(*problem);
  assembler_c * raw = assm.get();
  assembler_c::errState err = problem->setAssembler(std::move(assm));
  CHECK(err == assembler_c::ERR_NONE);
  CHECK(assm == nullptr);
  CHECK(problem->getAssembler() == raw);
}


TEST_CASE("Parallel assembler produces identical results to single-threaded", "[assembler][parallel]") {
  auto p = puzzle_c::load("examples/PelikanBurr.xmpuzzle");
  REQUIRE(p != nullptr);
  auto problem = p->getProblem(0);
  REQUIRE(problem != nullptr);

  int assemblies_1 = 0;
  int solutions_1 = 0;
  {
    assembler_0_c assm(*problem);
    assm.setNumThreads(1);
    REQUIRE(assm.createMatrix(false, false, false) == assembler_c::ERR_NONE);
    disassembler_0_c disasm(*problem);
    TestAssemblerCallback cb(&disasm);
    assm.assemble(&cb);
    assemblies_1 = cb.assemblies;
    solutions_1 = cb.solutions;
    CHECK(assemblies_1 == 12);
    CHECK(solutions_1 == 1);
  }

  {
    assembler_0_c assm(*problem);
    assm.setNumThreads(4);
    REQUIRE(assm.createMatrix(false, false, false) == assembler_c::ERR_NONE);
    disassembler_0_c disasm(*problem);
    TestAssemblerCallback cb(&disasm);
    assm.assemble(&cb);
    CHECK(cb.assemblies == assemblies_1);
    CHECK(cb.solutions == solutions_1);
    CHECK(assm.getIterations() > 0);
    CHECK(assm.getFinished() >= 1.0f);
  }
}

TEST_CASE("Parallel assembler pause and continue does not duplicate solutions", "[assembler][parallel][resume]") {
  auto p = puzzle_c::load("examples/PelikanBurr.xmpuzzle");
  REQUIRE(p != nullptr);
  auto problem = p->getProblem(0);
  REQUIRE(problem != nullptr);

  // Baseline: solve in one go
  int total_expected = 0;
  {
    assembler_0_c assm(*problem);
    assm.setNumThreads(4);
    REQUIRE(assm.createMatrix(false, false, false) == assembler_c::ERR_NONE);
    TestAssemblerCallback cb;
    assm.assemble(&cb);
    total_expected = cb.assemblies;
    REQUIRE(total_expected == 12);
  }

  // Two-phase solve: stop after 5 assemblies, then continue
  {
    assembler_0_c assm(*problem);
    assm.setNumThreads(4);
    REQUIRE(assm.createMatrix(false, false, false) == assembler_c::ERR_NONE);

    int phase1_count = 0;
    class StoppingCallback : public assembler_cb {
    public:
      int count = 0;
      assembler_0_c & a;
      StoppingCallback(assembler_0_c & assm) : a(assm) {}
      bool assembly(std::unique_ptr<assembly_c>) override {
        count++;
        if (count == 5) {
          a.stop();
          return false;
        }
        return true;
      }
    } cb1(assm);

    assm.assemble(&cb1);
    phase1_count = cb1.count;
    CHECK(phase1_count == 5);

    // Now continue searching
    TestAssemblerCallback cb2;
    assm.assemble(&cb2);

    // Total assemblies found across both phases must equal full run
    CHECK(phase1_count + cb2.assemblies == total_expected);
  }
}

TEST_CASE("Parallel assembler on small 2/3 piece problem", "[assembler][parallel][small]") {
  auto p = puzzle_c::load("examples/DemoMirrorParadox.xmpuzzle");
  REQUIRE(p != nullptr);
  auto problem = p->getProblem(0);
  REQUIRE(problem != nullptr);

  if (assembler_0_c::canHandle(*problem)) {
    assembler_0_c assm1(*problem);
    assm1.setNumThreads(1);
    REQUIRE(assm1.createMatrix(false, false, false) == assembler_c::ERR_NONE);
    TestAssemblerCallback cb1;
    assm1.assemble(&cb1);

    assembler_0_c assm4(*problem);
    assm4.setNumThreads(4);
    REQUIRE(assm4.createMatrix(false, false, false) == assembler_c::ERR_NONE);
    TestAssemblerCallback cb4;
    assm4.assemble(&cb4);

    CHECK(cb4.assemblies == cb1.assemblies);
  }
}


/* assembler_c::save() emits <assembler version="X">payload</assembler>.
 * These pull the two pieces back out so a test can feed them to setPosition()
 * exactly the way problem_c does when a puzzle is loaded.
 */
static std::string assemblerVersionOf(const std::string & xml) {
  /* start at the tag, not at the document: the <?xml ...?> header carries a
   * version attribute of its own
   */
  size_t tag = xml.find("<assembler");
  REQUIRE(tag != std::string::npos);
  size_t a = xml.find("version=\"", tag);
  REQUIRE(a != std::string::npos);
  a += 9;
  size_t b = xml.find('"', a);
  REQUIRE(b != std::string::npos);
  return xml.substr(a, b - a);
}

static std::string extractAssemblerContent(const std::string & xml) {
  size_t a = xml.find("<assembler");
  REQUIRE(a != std::string::npos);
  a = xml.find('>', a);
  REQUIRE(a != std::string::npos);
  a++;
  size_t b = xml.find("</assembler>", a);
  REQUIRE(b != std::string::npos);
  return xml.substr(a, b - a);
}

/* The leading flag added to the save payload must not break ordinary restore.
 *
 * Deliberately a not-yet-started assembler rather than a finished one: a
 * finished search is SS_SOLVED and never serialised, and feeding a completed
 * position (pos == piecenumber + 1) to setPosition() trips a pre-existing
 * out-of-bounds read, since rows/columns are sized piecenumber while the
 * integrity loop runs to pos. That is a separate bug from this change.
 */
TEST_CASE("Parallel assembler: the interrupted flag does not break normal restore",
          "[assembler][parallel][resume]") {
  auto p = puzzle_c::load("examples/PelikanBurr.xmpuzzle");
  REQUIRE(p != nullptr);
  auto problem = p->getProblem(0);
  REQUIRE(problem != nullptr);

  assembler_0_c assm(*problem);
  assm.setNumThreads(4);
  REQUIRE(assm.createMatrix(false, false, false) == assembler_c::ERR_NONE);

  std::string state;
  {
    std::ostringstream str;
    xmlWriter_c xml(str);
    assm.save(xml);
    state = str.str();
  }

  assembler_0_c restored(*problem);
  REQUIRE(restored.createMatrix(false, false, false) == assembler_c::ERR_NONE);
  std::string payload = extractAssemblerContent(state);
  CHECK(restored.setPosition(payload.c_str(), assemblerVersionOf(state).c_str())
        == assembler_c::ERR_NONE);
}

/* setNumThreads is reachable from -t and from Problem.solve(threads=...),
 * neither of which validated the value; an unclamped count went straight into
 * thread creation.
 */
TEST_CASE("Assembler clamps an absurd thread count instead of trying to spawn it",
          "[assembler][parallel][threads]") {
  auto p = puzzle_c::load("examples/PelikanBurr.xmpuzzle");
  REQUIRE(p != nullptr);
  auto problem = p->getProblem(0);
  REQUIRE(problem != nullptr);

  assembler_0_c assm(*problem);
  assm.setNumThreads(1000000);
  CHECK(assm.getNumThreads() <= 256);

  REQUIRE(assm.createMatrix(false, false, false) == assembler_c::ERR_NONE);
  int seen = 0;
  assm.assemble([&seen](std::unique_ptr<assembly_c>) -> bool { seen++; return true; });
  CHECK(seen == 12);
}

/* Parallel search on a puzzle that actually enables symmetry breaking.
 *
 * Workers call assembly_c::smallerRotationExists() outside callbackMutex, and
 * that reaches the lazily filled mutable caches (BbHsCache, symmetries) on the
 * shapes shared by every worker -- on the result shape and, via
 * normalizeTransformation() and the hotspot fixup in assembly_c::transform(),
 * on every part shape too. Those are unsynchronised check-then-write.
 *
 * The other [parallel] cases use PelikanBurr and CubeInCage, neither of which
 * has a symmetry breaker, so they never enter that branch at all and cannot
 * detect anything here. Keep this case on a symmetry-breaking puzzle; swapping
 * the puzzle silently removes the coverage.
 *
 * Under a ThreadSanitizer build this is the case that catches a missing
 * pre-warm: with only the result shape warmed it reports ~36 races on
 * assembly.cpp's getPartShape(i)->getHotspot() calls.
 */
TEST_CASE("Parallel assembler matches serial on a symmetry-breaking puzzle",
          "[assembler][parallel][tsan]") {
  auto p = puzzle_c::load("examples/Bermuda.xmpuzzle");
  REQUIRE(p != nullptr);
  auto problem = p->getProblem(0);
  REQUIRE(problem != nullptr);

  int serial = 0;
  {
    assembler_0_c assm(*problem);
    assm.setNumThreads(1);
    REQUIRE(assm.createMatrix(false, false, false) == assembler_c::ERR_NONE);
    assm.assemble([&serial](std::unique_ptr<assembly_c>) -> bool { serial++; return true; });
  }
  REQUIRE(serial > 0);

  /* Guard the premise rather than trusting the chosen puzzle: keepRotations
   * forces avoidTransformedAssemblies off, so it must yield strictly more
   * assemblies. If the two agree, symmetry breaking is not active here any
   * more and this case has stopped covering the concurrent path.
   */
  {
    int keptRotations = 0;
    assembler_0_c assm(*problem);
    assm.setNumThreads(1);
    REQUIRE(assm.createMatrix(false, true, false) == assembler_c::ERR_NONE);
    assm.assemble([&keptRotations](std::unique_ptr<assembly_c>) -> bool { keptRotations++; return true; });
    INFO("symmetry breaking must be active for this test to be meaningful");
    REQUIRE(keptRotations > serial);
  }

  /* Reload so the parallel run starts with cold caches: the lazy fills are
   * first-touch, so a parallel run after a serial run on the same puzzle
   * object races on already-warm caches and reports nothing.
   */
  auto pFresh = puzzle_c::load("examples/Bermuda.xmpuzzle");
  REQUIRE(pFresh != nullptr);
  auto problemFresh = pFresh->getProblem(0);
  REQUIRE(problemFresh != nullptr);

  int parallel = 0;
  assembler_0_c assm(*problemFresh);
  assm.setNumThreads(4);
  REQUIRE(assm.createMatrix(false, false, false) == assembler_c::ERR_NONE);
  assm.assemble([&parallel](std::unique_ptr<assembly_c>) -> bool { parallel++; return true; });

  CHECK(parallel == serial);
}

/* Records a canonical fingerprint of every assembly, so two runs can be
 * compared as multisets instead of by count alone. Equal counts are much
 * weaker than an equal set: one lost assembly plus one duplicated assembly
 * leaves the count untouched, and both are failure modes the parallel search
 * can actually produce.
 */
class RecordingAssemblerCallback : public assembler_cb {
public:
  std::multiset<std::string> fingerprints;

  bool assembly(std::unique_ptr<assembly_c> a) override {
    std::string s;
    for (unsigned int i = 0; i < a->placementCount(); i++) {
      if (a->isPlaced(i)) {
        s += std::to_string(a->getTransformation(i)) + ",";
        s += std::to_string(a->getX(i)) + ",";
        s += std::to_string(a->getY(i)) + ",";
        s += std::to_string(a->getZ(i)) + ";";
      } else {
        s += "-;";
      }
    }
    fingerprints.insert(std::move(s));
    return true;
  }
};

TEST_CASE("Parallel assembler 1 produces identical results to single-threaded", "[assembler][parallel]") {
  auto p = puzzle_c::load("examples/CubeInCage.xmpuzzle");
  REQUIRE(p != nullptr);
  auto problem = p->getProblem(0);
  REQUIRE(problem != nullptr);

  std::multiset<std::string> serial;
  {
    RecordingAssemblerCallback cb;
    assembler_1_c assm(*problem);
    assm.setNumThreads(1);
    REQUIRE(assm.createMatrix(false, false, false) == assembler_c::ERR_NONE);
    assm.assemble(&cb);
    serial = std::move(cb.fingerprints);
  }
  CHECK(serial.size() == 96);

  {
    RecordingAssemblerCallback cb;
    assembler_1_c assm(*problem);
    assm.setNumThreads(4);
    REQUIRE(assm.createMatrix(false, false, false) == assembler_c::ERR_NONE);
    assm.assemble(&cb);
    /* the set, not the size: a lost assembly paired with a duplicated one
     * would pass a count comparison
     */
    CHECK(cb.fingerprints == serial);
    CHECK(assm.getIterations() > 0);
    CHECK(assm.getFinished() >= 1.0f);
  }
}

/* Forces generateSubtreeTasks() past its first pass.
 *
 * targetTasks is max(16, workers*4), so with a large worker count depth 1
 * cannot supply enough tasks and the cutoff_depth++ escalation runs -- which
 * re-walks the prefix of the tree it already walked. That re-walk is what used
 * to re-report assemblies; CubeInCage with 4 threads clears 16 tasks on the
 * first pass and never enters the path at all.
 */
TEST_CASE("Parallel assembler 1 does not duplicate assemblies when task generation escalates",
          "[assembler][parallel][retry]") {
  auto p = puzzle_c::load("examples/CubeInCage.xmpuzzle");
  REQUIRE(p != nullptr);
  auto problem = p->getProblem(0);
  REQUIRE(problem != nullptr);

  std::multiset<std::string> serial;
  {
    RecordingAssemblerCallback cb;
    assembler_1_c assm(*problem);
    assm.setNumThreads(1);
    REQUIRE(assm.createMatrix(false, false, false) == assembler_c::ERR_NONE);
    assm.assemble(&cb);
    serial = std::move(cb.fingerprints);
  }
  REQUIRE_FALSE(serial.empty());

  RecordingAssemblerCallback cb;
  assembler_1_c assm(*problem);
  assm.setNumThreads(64);          // targetTasks = 256, unreachable at depth 1
  REQUIRE(assm.createMatrix(false, false, false) == assembler_c::ERR_NONE);
  assm.assemble(&cb);
  CHECK(cb.fingerprints == serial);
}

/* Stopping and continuing on the same assembler must not report the first
 * run's assemblies again.
 */
TEST_CASE("Parallel assembler 1 pause and continue does not duplicate assemblies",
          "[assembler][parallel][resume]") {
  auto p = puzzle_c::load("examples/CubeInCage.xmpuzzle");
  REQUIRE(p != nullptr);
  auto problem = p->getProblem(0);
  REQUIRE(problem != nullptr);

  std::multiset<std::string> serial;
  {
    RecordingAssemblerCallback cb;
    assembler_1_c assm(*problem);
    assm.setNumThreads(1);
    REQUIRE(assm.createMatrix(false, false, false) == assembler_c::ERR_NONE);
    assm.assemble(&cb);
    serial = std::move(cb.fingerprints);
  }
  REQUIRE(serial.size() > 1);

  assembler_1_c assm(*problem);
  assm.setNumThreads(4);
  REQUIRE(assm.createMatrix(false, false, false) == assembler_c::ERR_NONE);

  RecordingAssemblerCallback cb;
  int seen = 0;
  assm.assemble([&](std::unique_ptr<assembly_c> a) -> bool {
    cb.assembly(std::move(a));
    return ++seen < 1;             // stop after the first
  });
  REQUIRE(seen == 1);

  assm.assemble(&cb);              // continue on the same assembler

  /* every assembly exactly once across the two runs */
  CHECK(cb.fingerprints == serial);
}

TEST_CASE("Parallel assembler 1 stops promptly when aborted", "[assembler][parallel]") {
  auto p = puzzle_c::load("examples/CubeInCage.xmpuzzle");
  REQUIRE(p != nullptr);
  auto problem = p->getProblem(0);
  REQUIRE(problem != nullptr);

  assembler_1_c assm(*problem);
  assm.setNumThreads(4);
  REQUIRE(assm.createMatrix(false, false, false) == assembler_c::ERR_NONE);

  int count = 0;
  assm.assemble([&count](std::unique_ptr<assembly_c>) -> bool {
    count++;
    return false;                  // request immediate stop
  });

  CHECK(count == 1);
  /* not stopped(): that is just !running, which is true of any returned
   * assemble() whether or not the abort was honoured. The search really
   * stopping is what getFinished() < 1 shows.
   */
  CHECK(assm.getFinished() < 1.0f);
}

/* Parallel Huang search on a puzzle that actually enables symmetry breaking.
 *
 * Workers call assembly_c::smallerRotationExists() outside callbackMutex,
 * which reaches the lazily filled mutable caches on the shapes shared by all
 * of them -- BbHsCache and symmetries, on the result shape and, through
 * normalizeTransformation() and the hotspot fixup in assembly_c::transform(),
 * on every part shape too.
 *
 * CubeInCage, the puzzle the other assembler_1 cases use, has no symmetry
 * breaker, so its workers never enter that branch at all: instrumenting the
 * guard shows 292 worker solutions on the rest of this suite, every one of
 * them with avoidTransformedAssemblies == 0. DemoMirrorParadox does enter it
 * -- 50 worker solutions with the flag set -- which makes it the only bundled
 * example that covers this path for assembler_1. Swapping the puzzle silently
 * removes the coverage.
 */
TEST_CASE("Parallel assembler 1 matches serial on a symmetry-breaking puzzle",
          "[assembler][parallel][tsan]") {
  auto p = puzzle_c::load("examples/DemoMirrorParadox.xmpuzzle");
  REQUIRE(p != nullptr);
  REQUIRE(p->getNumberOfProblems() > 1);
  auto problem = p->getProblem(1);
  REQUIRE(problem != nullptr);

  std::multiset<std::string> serial;
  {
    RecordingAssemblerCallback cb;
    assembler_1_c assm(*problem);
    assm.setNumThreads(1);
    REQUIRE(assm.createMatrix(false, false, false) == assembler_c::ERR_NONE);
    assm.assemble(&cb);
    serial = std::move(cb.fingerprints);
  }
  REQUIRE_FALSE(serial.empty());

  /* Guard the premise rather than trusting the chosen puzzle: keepRotations
   * forces avoidTransformedAssemblies off, so it must yield strictly more
   * assemblies. If the two agree, symmetry breaking is not active here any
   * more and this case has stopped covering the concurrent path.
   */
  {
    RecordingAssemblerCallback cb;
    assembler_1_c assm(*problem);
    assm.setNumThreads(1);
    REQUIRE(assm.createMatrix(false, true, false) == assembler_c::ERR_NONE);
    assm.assemble(&cb);
    INFO("symmetry breaking must be active for this test to be meaningful");
    REQUIRE(cb.fingerprints.size() > serial.size());
  }

  /* reload so the parallel run starts with cold caches -- the lazy fills are
   * first touch, so running after a serial run on the same puzzle object
   * races on already warm caches and reports nothing
   */
  auto pFresh = puzzle_c::load("examples/DemoMirrorParadox.xmpuzzle");
  REQUIRE(pFresh != nullptr);
  auto problemFresh = pFresh->getProblem(1);
  REQUIRE(problemFresh != nullptr);

  RecordingAssemblerCallback cb;
  assembler_1_c assm(*problemFresh);
  assm.setNumThreads(4);
  REQUIRE(assm.createMatrix(false, false, false) == assembler_c::ERR_NONE);
  assm.assemble(&cb);

  CHECK(cb.fingerprints == serial);
}

/* The added flag must not break ordinary restore.
 *
 * Uses a *serial* run stopped part way, which is the state the application
 * actually saves: iterative() breaks at a restorable point, parallelInterrupted
 * stays false, and the position round-trips as it always did. (A never-started
 * assembler is not a useful case here -- its vectors are too short for
 * setPosition's own length checks, and problem_c only ever saves while
 * SS_SOLVING.)
 */
TEST_CASE("Parallel assembler 1: the interrupted flag does not break normal restore",
          "[assembler][parallel][resume]") {
  auto p = puzzle_c::load("examples/CubeInCage.xmpuzzle");
  REQUIRE(p != nullptr);
  auto problem = p->getProblem(0);
  REQUIRE(problem != nullptr);

  /* SIMD abort is not resumable (position lives in the solver, not the
   * Huang stacks). Force the serial iterative path this case is about.
   */
  ForceNoSimd forceNoSimd;
  assembler_1_c assm(*problem);
  assm.setNumThreads(1);            // serial: a resumable stop
  REQUIRE(assm.createMatrix(false, false, false) == assembler_c::ERR_NONE);

  int seen = 0;
  assm.assemble([&seen](std::unique_ptr<assembly_c>) -> bool { seen++; return false; });
  REQUIRE(seen == 1);
  REQUIRE(assm.getFinished() < 1.0f);

  std::string state;
  {
    std::ostringstream str;
    xmlWriter_c xml(str);
    assm.save(xml);
    state = str.str();
  }

  assembler_1_c restored(*problem);
  REQUIRE(restored.createMatrix(false, false, false) == assembler_c::ERR_NONE);
  std::string payload = extractAssemblerContent(state);
  CHECK(restored.setPosition(payload.c_str(), assemblerVersionOf(state).c_str())
        == assembler_c::ERR_NONE);
}

/* getFinished() must not claim a completed search just because a previous
 * parallel run left totalTasks == completedTasks behind on the object.
 */
TEST_CASE("Parallel assembler 1 does not report stale progress on a later run",
          "[assembler][parallel][progress]") {
  auto p = puzzle_c::load("examples/CubeInCage.xmpuzzle");
  REQUIRE(p != nullptr);
  auto problem = p->getProblem(0);
  REQUIRE(problem != nullptr);

  assembler_1_c assm(*problem);
  assm.setNumThreads(4);
  REQUIRE(assm.createMatrix(false, false, false) == assembler_c::ERR_NONE);
  assm.assemble([](std::unique_ptr<assembly_c>) -> bool { return true; });
  REQUIRE(assm.getFinished() >= 1.0f);

  /* a fresh assembler on the same problem must start at 0, not inherit a
   * finished-looking fraction
   */
  assembler_1_c again(*problem);
  again.setNumThreads(4);
  REQUIRE(again.createMatrix(false, false, false) == assembler_c::ERR_NONE);
  CHECK(again.getFinished() < 1.0f);
}

/* A saved assembler position with the numbers after " S <count>" sorted. */
static std::string sortedSignatures(const std::string & saved) {
  const size_t at = saved.find(" S ");
  if (at == std::string::npos)
    return saved;
  std::istringstream in(saved.substr(at + 3));
  unsigned long long count = 0;
  in >> count;
  std::vector<unsigned long long> sigs(count);
  for (unsigned long long & v : sigs)
    in >> v;
  std::sort(sigs.begin(), sigs.end());
  std::string rest;
  std::getline(in, rest, '\0');
  std::string out = saved.substr(0, at) + " S " + std::to_string(count);
  for (unsigned long long v : sigs)
    out += " " + std::to_string(v);
  return out + rest;
}

/* Stops a search after stopAt assemblies the way the application does (the
 * callback asks it to stop), then finishes it: either straight on with the
 * same assembler, or after saving the position as it would go into the
 * .xmpuzzle and restoring it into a fresh one. Every assembly must be
 * reported exactly once across both parts. Covers the parallel search
 * (threads > 1) and the single-threaded SIMD search, which used to be
 * refused on restore or to repeat itself.
 */
template <class A>
static void checkStopAndResume(const char * file, unsigned int prob, unsigned int threads,
                               unsigned int stopAt, bool viaSave) {
  std::multiset<std::string> all;
  {
    auto p = puzzle_c::load(file);
    REQUIRE(p != nullptr);
    RecordingAssemblerCallback cb;
    A assm(*p->getProblem(prob));
    assm.setNumThreads(threads);
    REQUIRE(assm.createMatrix(false, false, false) == assembler_c::ERR_NONE);
    assm.assemble(&cb);
    all = cb.fingerprints;
  }
  REQUIRE(all.size() > stopAt);

  class stopping_c : public RecordingAssemblerCallback {
  public:
    size_t stopAt = 0;
    bool assembly(std::unique_ptr<assembly_c> a) override {
      RecordingAssemblerCallback::assembly(std::move(a));
      return fingerprints.size() < stopAt;
    }
  } first;
  first.stopAt = stopAt;

  auto p = puzzle_c::load(file);
  REQUIRE(p != nullptr);
  RecordingAssemblerCallback rest;
  A assm(*p->getProblem(prob));
  assm.setNumThreads(threads);
  REQUIRE(assm.createMatrix(false, false, false) == assembler_c::ERR_NONE);
  assm.assemble(&first);
  REQUIRE(assm.getFinished() < 1.0f);

  if (viaSave) {
    std::string state;
    {
      std::ostringstream str;
      xmlWriter_c xml(str);
      assm.save(xml);
      state = str.str();
    }
    auto p2 = puzzle_c::load(file);
    REQUIRE(p2 != nullptr);
    A restored(*p2->getProblem(prob));
    restored.setNumThreads(threads);
    REQUIRE(restored.createMatrix(false, false, false) == assembler_c::ERR_NONE);
    std::string payload = extractAssemblerContent(state);
    REQUIRE(restored.setPosition(payload.c_str(), assemblerVersionOf(state).c_str())
            == assembler_c::ERR_NONE);
    /* Restoring and saving again writes the same position. The reported
     * signatures come from an unordered set, so compare those sorted. */
    {
      std::ostringstream str;
      xmlWriter_c xml(str);
      restored.save(xml);
      CHECK(sortedSignatures(str.str()) == sortedSignatures(state));
    }
    restored.assemble(&rest);
  } else {
    assm.assemble(&rest);
  }

  std::multiset<std::string> both = first.fingerprints;
  both.insert(rest.fingerprints.begin(), rest.fingerprints.end());
  CHECK(both == all);
}

TEST_CASE("Assembler 0: a stopped search resumes, saved or not, reporting each assembly once",
          "[assembler][parallel][resume]") {
  for (bool viaSave : {false, true}) {
    checkStopAndResume<assembler_0_c>("examples/PelikanBurr.xmpuzzle", 0, 4, 1, viaSave);
    checkStopAndResume<assembler_0_c>("examples/PelikanBurr.xmpuzzle", 0, 4, 5, viaSave);
    checkStopAndResume<assembler_0_c>("examples/PelikanBurr.xmpuzzle", 0, 1, 5, viaSave);
  }
}

TEST_CASE("Assembler 1: a stopped search resumes, saved or not, reporting each assembly once",
          "[assembler][parallel][resume]") {
  for (bool viaSave : {false, true}) {
    checkStopAndResume<assembler_1_c>("examples/CubeInCage.xmpuzzle", 0, 4, 1, viaSave);
    checkStopAndResume<assembler_1_c>("examples/CubeInCage.xmpuzzle", 0, 1, 1, viaSave);
  }
}

/* The vector search and the plain search must find the same assemblies:
 * on one thread, where the vector search does all of it, and on several,
 * where every thread searches its tasks with it; from the matrix as made and
 * from the reduced one (which loses rows and columns). Compared as sets of
 * assemblies, not as counts. The list holds puzzles with holes and variable
 * voxels (assembler 0) and with piece ranges (assembler 1), which the vector
 * search was kept from before.
 */
static std::multiset<std::string> assembliesOf(const char * file, unsigned int threads, bool reduce,
                                               bool noSimd, unsigned long * iterations = nullptr) {
  setNoSimd(noSimd);
  std::multiset<std::string> found;
  {
    auto p = puzzle_c::load(file);
    REQUIRE(p != nullptr);
    problem_c * problem = p->getProblem(0);
    std::unique_ptr<assembler_c> assm = problem->getPuzzle().getGridType()->findAssembler(*problem);
    REQUIRE(assm != nullptr);
    assm->setNumThreads(threads);
    REQUIRE(assm->createMatrix(false, false, false) == assembler_c::ERR_NONE);
    if (reduce)
      assm->reduce();
    RecordingAssemblerCallback cb;
    assm->assemble(&cb);
    CHECK(assm->getFinished() >= 1.0f);
    if (iterations)
      *iterations = assm->getIterations();
    found = std::move(cb.fingerprints);
  }
  setNoSimd(false);
  return found;
}

TEST_CASE("Vector and plain search find the same assemblies, on one thread and on several",
          "[solver][assembler][simd][parallel][equivalence]") {
  struct entry_s { const char * file; bool referenceReduced; };
  const entry_s puzzles[] = {
    {"examples/PelikanBurr.xmpuzzle",               false},  // holes
    {"examples/DraculasDentalDesaster.xmpuzzle",    false},
    {"examples/Bermuda.xmpuzzle",                   false},  // rotated copies left out
    {"examples/Prisgon.xmpuzzle",                   false},
    {"examples/BallRoom.xmpuzzle",                  false},
    {"examples/AugmentedSecondStellation.xmpuzzle", false},
    {"examples/CubeInCage.xmpuzzle",                false},
    {"examples/DemoMirrorParadox.xmpuzzle",         false},
    {"examples/PiecesOfEight.xmpuzzle",             false},  // range
    {"examples/DemoPieceGenerator.xmpuzzle",        false},  // range with variable voxels
    {"examples/AlPackino.xmpuzzle",                 false},
    {"examples/12PieceSeparation.xmpuzzle",         false},
    {"examples/FourPieceTetrahedron.xmpuzzle",      false},
    /* a range puzzle, and the one large enough to keep several threads
     * busy; the plain search needs 7 s for it as made, so the reference is
     * taken from the reduced matrix */
    {"examples/SolidSixPieceBurrs.xmpuzzle",        true},
  };

  unsigned int tookTheVectorSearch = 0;
  for (const entry_s & e : puzzles) {
    unsigned long plainIterations = 0, vectorIterations = 0;
    const std::multiset<std::string> plain = assembliesOf(e.file, 1, e.referenceReduced, true, &plainIterations);
    REQUIRE(!plain.empty());
    for (bool reduce : {false, true})
      for (unsigned int threads : {1u, 4u}) {
        INFO(e.file << ", " << threads << " threads" << (reduce ? ", reduced" : ""));
        const std::multiset<std::string> vec = assembliesOf(e.file, threads, reduce, false, &vectorIterations);
        CHECK(vec == plain);
        if (threads == 1 && reduce == e.referenceReduced && vectorIterations != plainIterations)
          tookTheVectorSearch++;
      }
  }

  /* if no puzzle took the vector search, this compared the plain search
   * with itself */
  CHECK(tookTheVectorSearch >= 8);
}

/* A parallel search of assembler 1 stopped and saved by an older version
 * carries signatures taken from the rows in the order the plain search
 * placed them (flag 2); today's are taken from the sorted assembly (flag 4).
 * Such a file must go on with the kind it has: loaded, it is saved again as
 * it was, and continued it reports every assembly once.
 */
TEST_CASE("Assembler 1: a parallel search saved by an older version carries on",
          "[assembler][parallel][resume]") {
  const char * file = "examples/CubeInCage.xmpuzzle";
  const std::multiset<std::string> all = assembliesOf(file, 1, false, true);

  /* what the older version wrote: the plain search, signatures by row order.
   * Loading a flag 2 position is what switches an assembler to that kind, so
   * start from a search that has not reported anything yet */
  std::string older;
  {
    auto p = puzzle_c::load(file);
    assembler_1_c assm(*p->getProblem(0));
    assm.setNumThreads(4);
    REQUIRE(assm.createMatrix(false, false, false) == assembler_c::ERR_NONE);
    class stopAtOnce_c : public assembler_cb {
    public:
      bool assembly(std::unique_ptr<assembly_c>) override { return false; }
    } stopAtOnce;
    assm.assemble(&stopAtOnce);
    std::ostringstream str;
    xmlWriter_c xml(str);
    assm.save(xml);
    older = extractAssemblerContent(str.str());
    REQUIRE(older.compare(0, 2, "4 ") == 0);
    /* no task finished and nothing reported: valid for either kind */
    const size_t c = older.find(" C ");
    const size_t sg = older.find(" S ");
    REQUIRE(c != std::string::npos);
    REQUIRE(sg != std::string::npos);
    for (size_t i = c + 3; i < sg; i++)
      if (older[i] == '1') older[i] = '0';
    older = "2 " + older.substr(2, sg - 2) + " S 0";
  }

  /* stop it part way, save, load, finish */
  class stopping_c : public RecordingAssemblerCallback {
  public:
    bool assembly(std::unique_ptr<assembly_c> a) override {
      RecordingAssemblerCallback::assembly(std::move(a));
      return fingerprints.size() < 20;
    }
  } first;
  RecordingAssemblerCallback rest;
  std::string again;
  {
    auto p = puzzle_c::load(file);
    assembler_1_c assm(*p->getProblem(0));
    assm.setNumThreads(4);
    REQUIRE(assm.createMatrix(false, false, false) == assembler_c::ERR_NONE);
    REQUIRE(assm.setPosition(older.c_str(), "2.1") == assembler_c::ERR_NONE);
    assm.assemble(&first);
    REQUIRE(assm.getFinished() < 1.0f);
    std::ostringstream str;
    xmlWriter_c xml(str);
    assm.save(xml);
    again = extractAssemblerContent(str.str());
  }
  /* still the older kind */
  CHECK(again.compare(0, 2, "2 ") == 0);
  {
    auto p = puzzle_c::load(file);
    assembler_1_c assm(*p->getProblem(0));
    assm.setNumThreads(4);
    REQUIRE(assm.createMatrix(false, false, false) == assembler_c::ERR_NONE);
    REQUIRE(assm.setPosition(again.c_str(), "2.1") == assembler_c::ERR_NONE);
    assm.assemble(&rest);
    CHECK(assm.getFinished() >= 1.0f);
  }
  std::multiset<std::string> both = first.fingerprints;
  both.insert(rest.fingerprints.begin(), rest.fingerprints.end());
  CHECK(both == all);
}

/* The memory the vector search of assembler 1 would need decides whether it
 * is used (canUseSimd): a row's mask is as wide as the solver's size class.
 */
TEST_CASE("Assembler 1: the vector search is refused past its memory budget", "[assembler][simd]") {
  /* 100000 rows of 20 nodes: small masks fit easily, 4 KB masks do not */
  CHECK(simdHuangFitsMemory(200, 100000, 2000000));
  CHECK(simdHuangFitsMemory(2048, 100000, 2000000));
  CHECK_FALSE(simdHuangFitsMemory(32768, 100000, 2000000));
  /* the mask doubles with the size class */
  CHECK(simdHuangMemoryEstimate(256, 1000, 0) < simdHuangMemoryEstimate(257, 1000, 0));
  CHECK(simdHuangMemoryEstimate(32768, 1000, 0) - simdHuangMemoryEstimate(16384, 1000, 0) == 1000u * 2048u);
  /* more nodes, more memory */
  CHECK(simdHuangMemoryEstimate(256, 1000, 20000) > simdHuangMemoryEstimate(256, 1000, 0));

  /* and through the switch: with no budget the plain search does the work,
   * with the same answer */
  const char * file = "examples/CubeInCage.xmpuzzle";
  unsigned long withBudget = 0, without = 0;
  const std::multiset<std::string> a = assembliesOf(file, 1, false, false, &withBudget);
#ifdef _WIN32
  (void)_putenv_s("BURRTOOLS_HUANG_MEM_MB", "0");
#else
  setenv("BURRTOOLS_HUANG_MEM_MB", "0", 1);
#endif
  const std::multiset<std::string> b = assembliesOf(file, 1, false, false, &without);
#ifdef _WIN32
  (void)_putenv_s("BURRTOOLS_HUANG_MEM_MB", "");
#else
  unsetenv("BURRTOOLS_HUANG_MEM_MB");
#endif
  CHECK(a == b);
  CHECK(withBudget != without);
}

/* The thread limit of the application (Settings, -t n) holds for everything
 * that picks its own number of threads, and a solve under it finds what it
 * finds without one.
 */
TEST_CASE("Solver: the thread limit is kept by every stage", "[solver][threads]") {
  struct restore_c { ~restore_c() { setSolveThreadLimit(0); } } restore;

  setSolveThreadLimit(0);
  CHECK(solveThreadLimit() == 0);
  const unsigned int all = solveThreadBudget();
  CHECK(all >= 1);

  setSolveThreadLimit(3);
  CHECK(solveThreadBudget() == 3);
  CHECK(solveThreadSplit(3).disassembly == 1);
  {
    auto p = puzzle_c::load("examples/PelikanBurr.xmpuzzle");
    assembler_0_c assm(*p->getProblem(0));
    CHECK(assm.getEffectiveThreads() == 3);   // nothing asked for: the limit
    assm.setNumThreads(2);
    CHECK(assm.getEffectiveThreads() == 2);   // asked for: that
  }

  /* searching and taking apart at once stay within the limit together */
  for (unsigned int n : {2u, 3u, 4u, 8u, 10u, 64u}) {
    const solveThreadSplit_s split = solveThreadSplit(n);
    INFO(n << " threads");
    CHECK(split.assembly >= 1);
    CHECK(split.disassembly >= 1);
    CHECK(split.assembly + split.disassembly == n);
    CHECK(split.assembly >= split.disassembly);
  }
  CHECK(solveThreadSplit(1).assembly == 1);
  CHECK(solveThreadSplit(1).disassembly == 1);

  /* a whole solve, under a limit and without */
  const char * file = "examples/SolidSixPieceBurrs.xmpuzzle";
  const int par = solveThread_c::PAR_REDUCE | solveThread_c::PAR_DISASSM;
  unsigned long assemblies[2] = {0, 0}, solutions[2] = {0, 0};
  int i = 0;
  for (unsigned int limit : {0u, 2u}) {
    setSolveThreadLimit(limit);
    auto p = puzzle_c::load(file);
    REQUIRE(p != nullptr);
    p->getProblem(0)->removeAllSolutions();
    solveThread_c solver(*p->getProblem(0), par);
    REQUIRE(solver.start());
    solver.waitUntilFinished();
    CHECK(p->getProblem(0)->getSolveState() == SS_SOLVED);
    assemblies[i] = p->getProblem(0)->getNumAssemblies();
    solutions[i] = p->getProblem(0)->getNumSolutions();
    i++;
  }
  CHECK(assemblies[0] == 588);
  CHECK(assemblies[1] == assemblies[0]);
  CHECK(solutions[1] == solutions[0]);
}

/* Pause and Continue through the solve thread, as the GUI does it: wherever
 * the pause lands -- in the assembler, with assemblies queued to be taken
 * apart, or part way through taking one apart -- nothing may be lost or
 * counted twice. Continue follows either straight on, or after saving the
 * puzzle and loading it again, as after quitting BurrTools.
 */
static std::string savedPuzzle(const puzzle_c & p) {
  std::ostringstream str;
  xmlWriter_c xml(str);
  p.save(xml);
  return str.str();
}

TEST_CASE("Solver: pausing and continuing finds every assembly and solution once",
          "[solver][resume]") {
  const char * file = "examples/SolidSixPieceBurrs.xmpuzzle";
  const int par = solveThread_c::PAR_REDUCE | solveThread_c::PAR_DISASSM;
  unsigned long assemblies = 0, solutions = 0;
  {
    auto p = puzzle_c::load(file);
    REQUIRE(p != nullptr);
    p->getProblem(0)->removeAllSolutions();
    solveThread_c solver(*p->getProblem(0), par);
    REQUIRE(solver.start());
    solver.waitUntilFinished();
    assemblies = p->getProblem(0)->getNumAssemblies();
    solutions = p->getProblem(0)->getNumSolutions();
  }
  REQUIRE(solutions > 0);

  /* The whole solve takes some 60 ms here since the vector search does this
   * puzzle; the short delays are the ones that land inside it, the long
   * ones still do on a slow machine or under a sanitizer. */
  int paused = 0;
  for (int delayMs : {2, 6, 20, 100, 300}) {
    for (bool viaSave : {false, true}) {
      INFO("pause after " << delayMs << " ms, " << (viaSave ? "saved and loaded" : "straight on"));
      auto p = puzzle_c::load(file);
      REQUIRE(p != nullptr);
      /* The example is saved solved; Solve starts from a clean slate. */
      p->getProblem(0)->removeAllSolutions();
      {
        solveThread_c solver(*p->getProblem(0), par);
        REQUIRE(solver.start());
        std::this_thread::sleep_for(std::chrono::milliseconds(delayMs));
        solver.stop();
        solver.waitUntilFinished();
      }
      if (p->getProblem(0)->getSolveState() != SS_SOLVED)
        paused++;
      if (viaSave) {
        std::istringstream in(savedPuzzle(*p));
        xmlParser_c pars(in);
        p = std::make_unique<puzzle_c>(pars);
      }
      problem_c * pr = p->getProblem(0);
      if (pr->getSolveState() != SS_SOLVED) {
        solveThread_c solver(*pr, par);
        REQUIRE(solver.start());
        solver.waitUntilFinished();
      }
      CHECK(pr->getNumAssemblies() == assemblies);
      CHECK(pr->getNumSolutions() == solutions);
      CHECK(pr->pendingCount() == 0);
    }
  }
  /* The premise: some pauses landed before the solve was over. */
  CHECK(paused >= 2);
}

/* A pause just after the assembler starts lands while the parallel search is
 * still splitting the work into subtrees. The subtrees made so far are not
 * the whole search: continuing must not finish with just those. The delays
 * sweep that window; on a slow machine (CI) the 20 ms pause of the test
 * above landed there now and then. */
TEST_CASE("Solver: a pause as assembling starts loses nothing", "[solver][resume]") {
  const char * file = "examples/SolidSixPieceBurrs.xmpuzzle";
  const int par = solveThread_c::PAR_REDUCE | solveThread_c::PAR_DISASSM;
  unsigned long assemblies = 0, solutions = 0;
  {
    auto p = puzzle_c::load(file);
    REQUIRE(p != nullptr);
    p->getProblem(0)->removeAllSolutions();
    solveThread_c solver(*p->getProblem(0), par);
    REQUIRE(solver.start());
    solver.waitUntilFinished();
    assemblies = p->getProblem(0)->getNumAssemblies();
    solutions = p->getProblem(0)->getNumSolutions();
  }

  for (int delayUs : {0, 100, 200, 400, 700, 1000, 1500, 2000, 3000, 5000}) {
    INFO("pause " << delayUs << " us into assembling");
    auto p = puzzle_c::load(file);
    REQUIRE(p != nullptr);
    problem_c * pr = p->getProblem(0);
    pr->removeAllSolutions();
    {
      solveThread_c solver(*pr, par);
      REQUIRE(solver.start());
      while (solver.currentAction() == solveThread_c::ACT_PREPARATION ||
             solver.currentAction() == solveThread_c::ACT_REDUCE)
        std::this_thread::yield();
      std::this_thread::sleep_for(std::chrono::microseconds(delayUs));
      solver.stop();
      solver.waitUntilFinished();
    }
    while (pr->getSolveState() != SS_SOLVED) {
      solveThread_c solver(*pr, par);
      REQUIRE(solver.start());
      solver.waitUntilFinished();
      REQUIRE(solver.currentAction() != solveThread_c::ACT_ERROR);
    }
    CHECK(pr->getNumAssemblies() == assemblies);
    CHECK(pr->getNumSolutions() == solutions);
  }
}

/* The autosave pause: only the assembler stops, work under way finishes, and
 * what is saved carries on to the same totals. */
TEST_CASE("Solver: an autosave pause saves a solve that carries on to the same totals",
          "[solver][resume]") {
  const char * file = "examples/SolidSixPieceBurrs.xmpuzzle";
  const int par = solveThread_c::PAR_REDUCE | solveThread_c::PAR_DISASSM;
  unsigned long assemblies = 0, solutions = 0;
  {
    auto p = puzzle_c::load(file);
    REQUIRE(p != nullptr);
    p->getProblem(0)->removeAllSolutions();
    solveThread_c solver(*p->getProblem(0), par);
    REQUIRE(solver.start());
    solver.waitUntilFinished();
    assemblies = p->getProblem(0)->getNumAssemblies();
    solutions = p->getProblem(0)->getNumSolutions();
  }

  /* An autosave pause is only taken while assembling or taking apart (one
   * that comes during the preparation is left for the next autosave), and
   * the whole solve takes some 60 ms here. So besides the fixed delays, 0
   * stands for "as soon as the assembler runs", which is the one sure to
   * land inside the solve whatever the machine. */
  int paused = 0;
  for (int delayMs : {0, 0, 30, 120}) {
    INFO("autosave pause after " << delayMs << " ms");
    auto p = puzzle_c::load(file);
    REQUIRE(p != nullptr);
    p->getProblem(0)->removeAllSolutions();
    {
      solveThread_c solver(*p->getProblem(0), par);
      REQUIRE(solver.start());
      if (delayMs == 0) {
        while (solver.currentAction() == solveThread_c::ACT_PREPARATION ||
               solver.currentAction() == solveThread_c::ACT_REDUCE)
          std::this_thread::yield();
      } else {
        std::this_thread::sleep_for(std::chrono::milliseconds(delayMs));
      }
      solver.stopSoft();
      solver.waitUntilFinished();
    }
    if (p->getProblem(0)->getSolveState() != SS_SOLVED)
      paused++;
    std::istringstream in(savedPuzzle(*p));
    xmlParser_c pars(in);
    auto q = std::make_unique<puzzle_c>(pars);
    problem_c * pr = q->getProblem(0);
    if (pr->getSolveState() != SS_SOLVED) {
      solveThread_c solver(*pr, par);
      REQUIRE(solver.start());
      solver.waitUntilFinished();
    }
    CHECK(pr->getNumAssemblies() == assemblies);
    CHECK(pr->getNumSolutions() == solutions);
  }
  CHECK(paused >= 1);
}

/* No solution limit (0) and a drop of 0, as a typed-in value can give: the
 * solve must start (it used to loop for ever in start()) and keep every
 * solution. */
TEST_CASE("Solver: no solution limit keeps every solution", "[solver]") {
  auto p = puzzle_c::load("examples/SolidSixPieceBurrs.xmpuzzle");
  REQUIRE(p != nullptr);
  problem_c * pr = p->getProblem(0);
  pr->removeAllSolutions();
  solveThread_c solver(*pr, solveThread_c::PAR_REDUCE | solveThread_c::PAR_DISASSM);
  solver.setSortMethod(solveThread_c::SRT_UNSORT);
  solver.setSolutionLimits(0, 0);
  REQUIRE(solver.start());
  solver.waitUntilFinished();
  CHECK(pr->getSolveState() == SS_SOLVED);
  CHECK(pr->getNumSolutions() > 10);
  CHECK(pr->getNumberOfSavedSolutions() == pr->getNumSolutions());
}

/* A saved position the assembler cannot use is reported once; the problem
 * then starts over cleanly. A second solve used to assert, because the
 * unusable state stayed behind on an unsolved problem. */
TEST_CASE("Solver: a damaged saved position errors once, then solves from the start", "[solver][resume]") {
  std::string text;
  {
    auto p = puzzle_c::load("examples/PelikanBurr.xmpuzzle");
    REQUIRE(p != nullptr);
    p->getProblem(0)->removeAllSolutions();
    solveThread_c solver(*p->getProblem(0), solveThread_c::PAR_REDUCE);
    REQUIRE(solver.start(true));  // prepare only: the problem is now solving
    solver.waitUntilFinished();
    REQUIRE(p->getProblem(0)->getSolveState() == SS_SOLVING);
    std::ostringstream out;
    xmlWriter_c xml(out);
    p->save(xml);
    text = out.str();
  }
  const size_t at = text.find("<assembler version=\"");
  REQUIRE(at != std::string::npos);
  const size_t body = text.find('>', at) + 1;
  text.replace(body, text.find("</assembler>", body) - body, "x");

  std::istringstream in(text);
  xmlParser_c pars(in);
  puzzle_c puz(pars);
  problem_c * pr = puz.getProblem(0);
  REQUIRE(pr->getSolveState() == SS_SOLVING);
  {
    solveThread_c solver(*pr, solveThread_c::PAR_REDUCE);
    REQUIRE(solver.start());
    solver.waitUntilFinished();
    CHECK(solver.currentAction() == solveThread_c::ACT_ERROR);
  }
  CHECK(pr->getSolveState() == SS_UNSOLVED);
  {
    solveThread_c solver(*pr, solveThread_c::PAR_REDUCE);
    REQUIRE(solver.start());
    solver.waitUntilFinished();
    CHECK(solver.currentAction() == solveThread_c::ACT_FINISHED);
  }
  CHECK(pr->getNumAssemblies() > 0);
}

namespace {

/* The twelve pentominoes in a w x h x 1 tray, built in memory. */
std::unique_ptr<puzzle_c> pentominoPuzzle(unsigned int w, unsigned int h) {
  static const std::vector<std::vector<std::string>> shapes = {
    {".##", "##.", ".#."},           // F
    {"#####"},                       // I
    {"#...", "####"},                // L
    {"##..", ".###"},                // N
    {"##", "##", "#."},              // P
    {"###", ".#.", ".#."},           // T
    {"#.#", "###"},                  // U
    {"#..", "#..", "###"},           // V
    {"#..", "##.", ".##"},           // W
    {".#.", "###", ".#."},           // X
    {"..#.", "####"},                // Y
    {"##.", ".#.", ".##"},           // Z
  };
  auto puz = std::make_unique<puzzle_c>(new gridType_c(gridType_c::GT_BRICKS));
  const unsigned int tray = puz->addShape(w, h, 1);
  for (unsigned int y = 0; y < h; y++)
    for (unsigned int x = 0; x < w; x++)
      puz->getShape(tray)->setState(x, y, 0, voxel_c::VX_FILLED);
  const unsigned int prob = puz->addProblem();
  problem_c * pr = puz->getProblem(prob);
  pr->setResultId(tray);
  for (const auto & art : shapes) {
    const unsigned int id = puz->addShape((unsigned int)art[0].size(), (unsigned int)art.size(), 1);
    for (unsigned int y = 0; y < art.size(); y++)
      for (unsigned int x = 0; x < art[y].size(); x++)
        if (art[y][x] == '#')
          puz->getShape(id)->setState(x, y, 0, voxel_c::VX_FILLED);
    pr->setShapeMaximum(id, 1);
    pr->setShapeMinimum(id, 1);
  }
  return puz;
}

/* Counts assemblies; safe to call from several threads at once. */
class CountingCallback : public assembler_cb {
public:
  std::atomic<unsigned long> assemblies{0};
  bool assembly(std::unique_ptr<assembly_c>) override {
    assemblies.fetch_add(1, std::memory_order_relaxed);
    return true;
  }
};

/* Count the assemblies of problem 0 the way burrTxt runs a solver type. */
unsigned long countAssemblies(puzzle_c & puz, solverType_e type, unsigned int threads) {
  problem_c * pr = puz.getProblem(0);
  std::unique_ptr<assembler_c> assm = puz.getGridType()->findAssembler(*pr, true, type);
  REQUIRE(assm != nullptr);
  assm->setNumThreads(threads);
  REQUIRE(assm->createMatrix(false, false, false) == assembler_c::ERR_NONE);
  assm->reduce();
  CountingCallback cb;
  if (type == SOLVER_BT2)
    bt2Assemble(assm.get(), &cb, bt2ChooseAssemblerWorkers(assm.get()));
  else
    assm->assemble(&cb);
  return cb.assemblies.load();
}

} // namespace

/* The twelve pentominoes fill a 3 x 20 tray in 2 ways and a 6 x 10 tray in
 * 2339 (turned-over and mirrored copies not counted). A search split over
 * threads must find each once: its parts must not overlap. */
TEST_CASE("Pentominoes: every solver type finds the published counts at any thread count",
          "[solver][assembler][pentomino]") {
  std::unique_ptr<puzzle_c> narrow = pentominoPuzzle(20, 3);
  for (solverType_e type : {SOLVER_CLASSIC, SOLVER_BT2})
    for (unsigned int threads : {1u, 2u, 5u}) {
      INFO(solverTypeLabel(type) << ", " << threads << " threads");
      CHECK(countAssemblies(*narrow, type, threads) == 2);
    }
  std::unique_ptr<puzzle_c> wide = pentominoPuzzle(10, 6);
  CHECK(countAssemblies(*wide, SOLVER_BT2, 1) == 2339);
  CHECK(countAssemblies(*wide, SOLVER_BT2, 4) == 2339);
}

/* Stops the search from inside after a number of assemblies. */
class StoppingCallback : public assembler_cb {
public:
  std::atomic<unsigned long> assemblies{0};
  std::atomic<unsigned long> stopAfter{0};
  bool assembly(std::unique_ptr<assembly_c>) override {
    const unsigned long n = assemblies.fetch_add(1, std::memory_order_relaxed) + 1;
    const unsigned long limit = stopAfter.load(std::memory_order_relaxed);
    return limit == 0 || n < limit;
  }
};

/* A BurrTools 2 search split over threads must stop when told to, keep the
 * branches it had not finished, and find every assembly once when it is
 * continued. */
TEST_CASE("BurrTools 2: a search split over threads stops and continues",
          "[solver][assembler][pentomino][bt2]") {
  std::unique_ptr<puzzle_c> wide = pentominoPuzzle(10, 6);
  problem_c * pr = wide->getProblem(0);

  for (unsigned int threads : {1u, 4u}) {
    INFO(threads << " threads");
    std::unique_ptr<assembler_c> assm = wide->getGridType()->findAssembler(*pr, true, SOLVER_BT2);
    REQUIRE(assm != nullptr);
    assm->setNumThreads(threads);
    REQUIRE(assm->createMatrix(false, false, false) == assembler_c::ERR_NONE);
    assm->reduce();

    StoppingCallback cb;
    float last = 0;
    unsigned int runs = 0;

    /* stop every 300 assemblies */
    do {
      cb.stopAfter.store(cb.assemblies.load() + 300);
      bt2Assemble(assm.get(), &cb, threads);
      runs++;
      const float f = assm->getFinished();
      CHECK(f >= last);
      last = f;
      REQUIRE(runs < 100);
    } while (assm->getFinished() < 1);

    CHECK(runs > 2);
    CHECK(cb.assemblies.load() == 2339);
    CHECK(assm->getIterations() > 0);
  }

  SECTION("stop() from another thread ends the run early") {
    std::unique_ptr<assembler_c> assm = wide->getGridType()->findAssembler(*pr, true, SOLVER_BT2);
    REQUIRE(assm != nullptr);
    REQUIRE(assm->createMatrix(false, false, false) == assembler_c::ERR_NONE);
    assm->reduce();

    StoppingCallback cb;
    std::thread stopper([&]() {
      while (cb.assemblies.load() < 50)
        std::this_thread::yield();
      assm->stop();
    });
    bt2Assemble(assm.get(), &cb, 4);
    stopper.join();

    CHECK(cb.assemblies.load() < 2339);
    CHECK(assm->getFinished() < 1);

    cb.stopAfter.store(0);
    bt2Assemble(assm.get(), &cb, 4);
    CHECK(cb.assemblies.load() == 2339);
    CHECK(assm->getFinished() == 1);
  }
}

/* Hidden: assembly speed of each solver type on the 6 x 10 pentomino tray.
 *   ./build/test_burrtools "[.bench][pentomino]" */
TEST_CASE("Pentominoes: benchmark", "[.bench][pentomino]") {
  std::unique_ptr<puzzle_c> wide = pentominoPuzzle(10, 6);
  wide->save(std::filesystem::path("tmp/pentomino_6x10.xmpuzzle"));
  for (solverType_e type : {SOLVER_CLASSIC, SOLVER_BT2})
    for (unsigned int threads : {1u, 4u, 8u}) {
      const auto t0 = std::chrono::steady_clock::now();
      const unsigned long n = countAssemblies(*wide, type, threads);
      const double s = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
      printf("pentomino 6x10, %s, %u threads: %lu assemblies in %.3f s\n",
             solverTypeLabel(type), threads, n, s);
    }
}
