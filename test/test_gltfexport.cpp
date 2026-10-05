#include <catch2/catch_test_macros.hpp>
#include "lib/gltfexport.h"
#include "lib/puzzle.h"
#include "lib/problem.h"
#include "lib/solution.h"
#include "lib/disassembly.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <string>
#include <vector>

namespace {

uint32_t get32(const std::vector<unsigned char> & b, size_t at) {
  return uint32_t(b[at]) | uint32_t(b[at+1]) << 8 | uint32_t(b[at+2]) << 16 | uint32_t(b[at+3]) << 24;
}

size_t count(const std::string & s, const std::string & what) {
  size_t n = 0;
  for (size_t at = s.find(what); at != std::string::npos; at = s.find(what, at + 1))
    n++;
  return n;
}

/* Export solution 0 of problem 0 and check the GLB container. Returns the
 * JSON chunk. BT_GLB_DUMP=dir writes the files out for a viewer. */
std::string exportFirst(const char * file, const char * dumpName) {
  std::unique_ptr<puzzle_c> puzzle = puzzle_c::load(file);
  REQUIRE(puzzle != nullptr);
  const problem_c * pr = puzzle->getProblem(0);
  REQUIRE(pr->getNumberOfSavedSolutions() > 0);
  REQUIRE(pr->getSavedSolution(0)->getDisassembly() != nullptr);

  std::vector<gltfExport::color_c> colors(pr->getNumberOfPieces(), { 0.8f, 0.4f, 0.2f });
  std::vector<unsigned char> glb;
  REQUIRE(gltfExport::solutionAnimation(*pr, 0, colors, gltfExport::options_c(), glb) == "");

  REQUIRE(glb.size() > 28);
  CHECK(get32(glb, 0) == 0x46546C67);
  CHECK(get32(glb, 4) == 2);
  CHECK(get32(glb, 8) == glb.size());
  const uint32_t jsonLen = get32(glb, 12);
  CHECK(jsonLen % 4 == 0);
  CHECK(get32(glb, 16) == 0x4E4F534A);
  REQUIRE(20 + jsonLen + 8 <= glb.size());
  const uint32_t binLen = get32(glb, 20 + jsonLen);
  CHECK(binLen % 4 == 0);
  CHECK(get32(glb, 24 + jsonLen) == 0x004E4942);
  CHECK(28 + jsonLen + binLen == glb.size());

  if (const char * dir = getenv("BT_GLB_DUMP")) {
    std::string out = std::string(dir) + "/" + dumpName;
    FILE * f = fopen(out.c_str(), "wb");
    if (f) { fwrite(glb.data(), 1, glb.size(), f); fclose(f); }
  }

  return std::string(glb.begin() + 20, glb.begin() + 20 + jsonLen);
}

}

TEST_CASE("glTF export: burr disassembly", "[gltf]") {
  std::string json = exportFirst("examples/PelikanBurr.xmpuzzle", "pelikan.glb");
  CHECK(json.find("\"animations\"") != std::string::npos);
  /* cage and six sticks, each with a translation track and none turning */
  CHECK(count(json, "\"path\":\"translation\"") == 7);
  CHECK(count(json, "\"path\":\"rotation\"") == 0);
}

TEST_CASE("glTF export: rotation moves turn the pieces", "[gltf]") {
  std::string json = exportFirst("test/test_rotation_solvers.xmpuzzle", "rotation.glb");
  CHECK(count(json, "\"path\":\"rotation\"") > 0);
}

TEST_CASE("glTF export: sliding puzzle", "[gltf]") {
  std::string json = exportFirst("test/test_sliding_solver.xmpuzzle", "sliding.glb");
  CHECK(json.find("\"path\":\"translation\"") != std::string::npos);
}

TEST_CASE("glTF export: flat pieces make a much smaller file", "[gltf]") {
  std::unique_ptr<puzzle_c> puzzle = puzzle_c::load("examples/PelikanBurr.xmpuzzle");
  REQUIRE(puzzle != nullptr);
  const problem_c * pr = puzzle->getProblem(0);
  gltfExport::options_c opt;
  std::vector<unsigned char> bevelled, flat;
  REQUIRE(gltfExport::solutionAnimation(*pr, 0, {}, opt, bevelled) == "");
  opt.bevel = false;
  REQUIRE(gltfExport::solutionAnimation(*pr, 0, {}, opt, flat) == "");
  CHECK(flat.size() * 2 < bevelled.size());
}

TEST_CASE("glTF export: a solution without disassembly is refused", "[gltf]") {
  std::unique_ptr<puzzle_c> puzzle = puzzle_c::load("examples/PelikanBurr.xmpuzzle");
  REQUIRE(puzzle != nullptr);
  const problem_c * pr = puzzle->getProblem(0);
  std::vector<unsigned char> glb;
  CHECK(gltfExport::solutionAnimation(*pr, pr->getNumberOfSavedSolutions(), {}, gltfExport::options_c(), glb) != "");
  CHECK(glb.empty());
}
