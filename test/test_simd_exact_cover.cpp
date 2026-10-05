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
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_template_test_macros.hpp>
#include "lib/simd_exact_cover.h"

#include <vector>
#include <algorithm>
#include <set>
#include <thread>
#include <mutex>
#include <string>
#include <cstdlib>

TEST_CASE("SimdBitset256 operations", "[simd][bitset]") {
  SimdBitset256 b;
  REQUIRE(b.empty());

  b.set(0);
  b.set(63);
  b.set(64);
  b.set(127);
  b.set(128);
  b.set(191);
  b.set(192);
  b.set(255);

  REQUIRE(b.test(0));
  REQUIRE(b.test(63));
  REQUIRE(b.test(64));
  REQUIRE(b.test(127));
  REQUIRE(b.test(128));
  REQUIRE(b.test(191));
  REQUIRE(b.test(192));
  REQUIRE(b.test(255));

  REQUIRE_FALSE(b.test(1));
  REQUIRE_FALSE(b.test(62));
  REQUIRE_FALSE(b.test(100));
  REQUIRE_FALSE(b.test(200));

  SimdBitset256 target;
  target.set(0);
  target.set(128);
  REQUIRE(b.containsAll(target));

  target.set(1);
  REQUIRE_FALSE(b.containsAll(target));

  SimdBitset256 disjoint_set;
  disjoint_set.set(1);
  disjoint_set.set(2);
  REQUIRE(is_disjoint_scalar(b, disjoint_set));

#if (defined(__x86_64__) || defined(_M_X64)) && (defined(__GNUC__) || defined(__clang__))
  if (__builtin_cpu_supports("avx2")) {
    REQUIRE(is_disjoint_avx2(b, disjoint_set));
    REQUIRE_FALSE(is_disjoint_avx2(b, target));
  }
#endif
}

TEST_CASE("SimdBitset512 operations", "[simd][bitset]") {
  SimdBitset512 b;
  REQUIRE(b.empty());

  b.set(0);
  b.set(63);
  b.set(64);
  b.set(127);
  b.set(128);
  b.set(191);
  b.set(192);
  b.set(255);
  b.set(256);
  b.set(383);
  b.set(384);
  b.set(511);

  REQUIRE(b.test(0));
  REQUIRE(b.test(63));
  REQUIRE(b.test(64));
  REQUIRE(b.test(127));
  REQUIRE(b.test(128));
  REQUIRE(b.test(191));
  REQUIRE(b.test(192));
  REQUIRE(b.test(255));
  REQUIRE(b.test(256));
  REQUIRE(b.test(383));
  REQUIRE(b.test(384));
  REQUIRE(b.test(511));

  REQUIRE_FALSE(b.test(1));
  REQUIRE_FALSE(b.test(62));
  REQUIRE_FALSE(b.test(257));
  REQUIRE_FALSE(b.test(500));

  SimdBitset512 target;
  target.set(0);
  target.set(384);
  REQUIRE(b.containsAll(target));

  target.set(1);
  REQUIRE_FALSE(b.containsAll(target));

  SimdBitset512 disjoint_set;
  disjoint_set.set(1);
  disjoint_set.set(300);
  REQUIRE(is_disjoint_scalar(b, disjoint_set));

#if (defined(__x86_64__) || defined(_M_X64)) && (defined(__GNUC__) || defined(__clang__))
  if (__builtin_cpu_supports("avx2")) {
    REQUIRE(is_disjoint_avx2(b, disjoint_set));
    REQUIRE_FALSE(is_disjoint_avx2(b, target));
  }
#endif
}

TEST_CASE("SimdBitset1024 operations", "[simd][bitset]") {
  SimdBitset1024 b;
  REQUIRE(b.empty());

  b.set(0);
  b.set(511);
  b.set(512);
  b.set(1023);

  REQUIRE(b.test(0));
  REQUIRE(b.test(511));
  REQUIRE(b.test(512));
  REQUIRE(b.test(1023));

  REQUIRE_FALSE(b.test(1));
  REQUIRE_FALSE(b.test(500));
  REQUIRE_FALSE(b.test(1000));

  SimdBitset1024 target;
  target.set(0);
  target.set(1023);
  REQUIRE(b.containsAll(target));

  target.set(1);
  REQUIRE_FALSE(b.containsAll(target));

  SimdBitset1024 disjoint_set;
  disjoint_set.set(1);
  disjoint_set.set(700);
  REQUIRE(is_disjoint_scalar(b, disjoint_set));

#if (defined(__x86_64__) || defined(_M_X64)) && (defined(__GNUC__) || defined(__clang__))
  if (__builtin_cpu_supports("avx2")) {
    REQUIRE(is_disjoint_avx2(b, disjoint_set));
    REQUIRE_FALSE(is_disjoint_avx2(b, target));
  }
  if (__builtin_cpu_supports("avx512f")) {
    REQUIRE(is_disjoint_avx512(b, disjoint_set));
    REQUIRE_FALSE(is_disjoint_avx512(b, target));
  }
#endif
}

TEST_CASE("SimdBitset2048 operations", "[simd][bitset]") {
  SimdBitset2048 b;
  REQUIRE(b.empty());

  b.set(0);
  b.set(1023);
  b.set(1024);
  b.set(2047);

  REQUIRE(b.test(0));
  REQUIRE(b.test(1023));
  REQUIRE(b.test(1024));
  REQUIRE(b.test(2047));

  REQUIRE_FALSE(b.test(1));
  REQUIRE_FALSE(b.test(1000));
  REQUIRE_FALSE(b.test(2000));

  SimdBitset2048 target;
  target.set(0);
  target.set(2047);
  REQUIRE(b.containsAll(target));

  target.set(1);
  REQUIRE_FALSE(b.containsAll(target));

  SimdBitset2048 disjoint_set;
  disjoint_set.set(1);
  disjoint_set.set(1500);
  REQUIRE(is_disjoint_scalar(b, disjoint_set));

#if (defined(__x86_64__) || defined(_M_X64)) && (defined(__GNUC__) || defined(__clang__))
  if (__builtin_cpu_supports("avx2")) {
    REQUIRE(is_disjoint_avx2(b, disjoint_set));
    REQUIRE_FALSE(is_disjoint_avx2(b, target));
  }
  if (__builtin_cpu_supports("avx512f")) {
    REQUIRE(is_disjoint_avx512(b, disjoint_set));
    REQUIRE_FALSE(is_disjoint_avx512(b, target));
  }
#endif
}

TEST_CASE("SimdBitset4096 and 8192 operations", "[simd][bitset]") {
  SimdBitset8192 b;
  REQUIRE(b.empty());

  b.set(0);
  b.set(4095);
  b.set(4096);
  b.set(8191);

  REQUIRE(b.test(0));
  REQUIRE(b.test(4095));
  REQUIRE(b.test(4096));
  REQUIRE(b.test(8191));

  REQUIRE_FALSE(b.test(1));
  REQUIRE_FALSE(b.test(2000));
  REQUIRE_FALSE(b.test(5832)); // 18^3 voxel index

  SimdBitset8192 target;
  target.set(0);
  target.set(8191);
  REQUIRE(b.containsAll(target));

  target.set(1);
  REQUIRE_FALSE(b.containsAll(target));

  SimdBitset8192 disjoint_set;
  disjoint_set.set(1);
  disjoint_set.set(5832);
  REQUIRE(is_disjoint_scalar(b, disjoint_set));

#if (defined(__x86_64__) || defined(_M_X64)) && (defined(__GNUC__) || defined(__clang__))
  if (__builtin_cpu_supports("avx2")) {
    REQUIRE(is_disjoint_avx2(b, disjoint_set));
    REQUIRE_FALSE(is_disjoint_avx2(b, target));
  }
  if (__builtin_cpu_supports("avx512f")) {
    REQUIRE(is_disjoint_avx512(b, disjoint_set));
    REQUIRE_FALSE(is_disjoint_avx512(b, target));
  }
#endif
}

TEST_CASE("SimdBitset16384 and 32768 operations", "[simd][bitset]") {
  SimdBitset32768 b;
  REQUIRE(b.empty());

  b.set(0);
  b.set(16383);
  b.set(16384);
  b.set(32767);

  REQUIRE(b.test(0));
  REQUIRE(b.test(16383));
  REQUIRE(b.test(16384));
  REQUIRE(b.test(32767));

  REQUIRE_FALSE(b.test(1));
  REQUIRE_FALSE(b.test(10000));

  SimdBitset32768 target;
  target.set(0);
  target.set(32767);
  REQUIRE(b.containsAll(target));

  target.set(1);
  REQUIRE_FALSE(b.containsAll(target));

  SimdBitset32768 disjoint_set;
  disjoint_set.set(1);
  disjoint_set.set(10000);
  REQUIRE(is_disjoint_scalar(b, disjoint_set));

#if (defined(__x86_64__) || defined(_M_X64)) && (defined(__GNUC__) || defined(__clang__))
  if (__builtin_cpu_supports("avx2")) {
    REQUIRE(is_disjoint_avx2(b, disjoint_set));
    REQUIRE_FALSE(is_disjoint_avx2(b, target));
  }
  if (__builtin_cpu_supports("avx512f")) {
    REQUIRE(is_disjoint_avx512(b, disjoint_set));
    REQUIRE_FALSE(is_disjoint_avx512(b, target));
  }
#endif
}

TEST_CASE("SimdExactCover extended solve up to 32768", "[simd][exact_cover]") {
  struct TierSpec {
    unsigned int capacity;
    std::function<std::unique_ptr<ISimdExactCover>(unsigned int, unsigned int)> make;
  };

  const std::vector<TierSpec> tiers = {
    {256, [](unsigned int c, unsigned int p) { return std::make_unique<SimdExactCover256>(c, p); }},
    {512, [](unsigned int c, unsigned int p) { return std::make_unique<SimdExactCover512>(c, p); }},
    {1024, [](unsigned int c, unsigned int p) { return std::make_unique<SimdExactCover1024>(c, p); }},
    {2048, [](unsigned int c, unsigned int p) { return std::make_unique<SimdExactCover2048>(c, p); }},
    {4096, [](unsigned int c, unsigned int p) { return std::make_unique<SimdExactCover4096>(c, p); }},
    {8192, [](unsigned int c, unsigned int p) { return std::make_unique<SimdExactCover8192>(c, p); }},
    {16384, [](unsigned int c, unsigned int p) { return std::make_unique<SimdExactCover16384>(c, p); }},
    {32768, [](unsigned int c, unsigned int p) { return std::make_unique<SimdExactCover32768>(c, p); }},
  };

  for (const auto & tier : tiers) {
    unsigned int capacity = tier.capacity;
    unsigned int offset = capacity - 7;
    auto solver = tier.make(capacity, 3);

#ifndef NDEBUG
    // Boundary check: col == capacity is out of bounds and must throw assert_exception
    // (bt_assert only throws in a build with assertions)
    CHECK_THROWS_AS(solver->setRequiredColumn(capacity), assert_exception);
    CHECK_THROWS_AS(solver->addRow(99, 0, {capacity}), assert_exception);
#endif

    for (unsigned int i = 0; i < 7; i++) {
      solver->setRequiredColumn(offset + i);
    }

    solver->addRow(1, 0, {offset + 2, offset + 4});
    solver->addRow(2, 1, {offset + 0, offset + 3, offset + 6});
    solver->addRow(3, 2, {offset + 1, offset + 2, offset + 5});
    solver->addRow(4, 1, {offset + 0, offset + 3, offset + 5});
    solver->addRow(5, 2, {offset + 1, offset + 6});
    solver->addRow(6, 1, {offset + 3, offset + 4, offset + 6});

    std::vector<std::vector<unsigned int>> solutions;
    std::atomic<bool> abort_flag{false};
    std::atomic<uint64_t> iterations{0};

    solver->solve([&](const std::vector<unsigned int> &sol) {
      std::vector<unsigned int> sorted = sol;
      std::sort(sorted.begin(), sorted.end());
      solutions.push_back(sorted);
      return true;
    }, abort_flag, iterations);

    REQUIRE(solutions.size() == 1);
    REQUIRE(solutions[0] == std::vector<unsigned int>{1, 4, 5});
    REQUIRE(iterations.load() > 0);
  }
}

TEST_CASE("SimdExactCover256 Knuth textbook example", "[simd][exact_cover]") {
  // Knuth's exact cover problem from TAOCP:
  // Items 0..6: {A, B, C, D, E, F, G}
  // Row 1: {C, E}       -> {2, 4}
  // Row 2: {A, D, G}    -> {0, 3, 6}
  // Row 3: {B, C, F}    -> {1, 2, 5}
  // Row 4: {A, D}       -> {0, 3}
  // Row 5: {B, G}       -> {1, 6}
  // Row 6: {D, E, G}    -> {3, 4, 6}
  //
  // Unique solution: Row 1, 4, 5 ({C, E}, {A, D}, {B, G})

  SimdExactCover256 solver(7, 3);

  SimdBitset256 req;
  for (unsigned int i = 0; i < 7; i++) req.set(i);
  solver.setRequiredColumns(req);

  solver.addRow(1, 0, {2, 4});
  solver.addRow(2, 1, {0, 3, 6});
  solver.addRow(3, 2, {1, 2, 5});
  solver.addRow(4, 1, {0, 3, 5});
  solver.addRow(5, 2, {1, 6});
  solver.addRow(6, 1, {3, 4, 6});

  std::vector<std::vector<unsigned int>> solutions;
  std::atomic<bool> abort_flag{false};
  std::atomic<uint64_t> iterations{0};

  solver.solve([&](const std::vector<unsigned int> &sol) {
    std::vector<unsigned int> sorted = sol;
    std::sort(sorted.begin(), sorted.end());
    solutions.push_back(sorted);
    return true;
  }, abort_flag, iterations);

  REQUIRE(solutions.size() == 1);
  REQUIRE(solutions[0] == std::vector<unsigned int>{1, 4, 5});
  REQUIRE(iterations.load() > 0);
}

TEST_CASE("SimdExactCover256 unsolvable problem", "[simd][exact_cover]") {
  SimdExactCover256 solver(4, 2);

  SimdBitset256 req;
  for (unsigned int i = 0; i < 4; i++) req.set(i);
  solver.setRequiredColumns(req);

  // Column 3 is never covered
  solver.addRow(1, 0, {0, 1});
  solver.addRow(2, 1, {1, 2});

  std::vector<std::vector<unsigned int>> solutions;
  std::atomic<bool> abort_flag{false};
  std::atomic<uint64_t> iterations{0};

  solver.solve([&](const std::vector<unsigned int> &sol) {
    solutions.push_back(sol);
    return true;
  }, abort_flag, iterations);

  REQUIRE(solutions.empty());
}

TEST_CASE("SimdExactCover256 abort flag", "[simd][exact_cover]") {
  SimdExactCover256 solver(3, 3);

  SimdBitset256 req;
  for (unsigned int i = 0; i < 3; i++) req.set(i);
  solver.setRequiredColumns(req);

  solver.addRow(1, 0, {0});
  solver.addRow(2, 1, {1});
  solver.addRow(3, 2, {2});

  std::atomic<bool> abort_flag{true};
  std::atomic<uint64_t> iterations{0};
  std::vector<std::vector<unsigned int>> solutions;

  solver.solve([&](const std::vector<unsigned int> &sol) {
    solutions.push_back(sol);
    return true;
  }, abort_flag, iterations);

  REQUIRE(solutions.empty());
}

TEST_CASE("SimdExactCover512 large column cover", "[simd][exact_cover]") {
  // Problem with columns up to 400 (spanning across >256 bit boundary):
  // 3 pieces, required columns: {50, 150, 270, 350, 400}
  // Row 1: piece 0 -> {50, 270}
  // Row 2: piece 1 -> {150}
  // Row 3: piece 2 -> {350, 400}
  // Row 4: piece 0 -> {50, 150} (conflict with row 2)

  SimdExactCover512 solver(401, 3);
  SimdBitset512 req;
  req.set(50);
  req.set(150);
  req.set(270);
  req.set(350);
  req.set(400);
  solver.setRequiredColumns(req);

  solver.addRow(1, 0, {50, 270});
  solver.addRow(2, 1, {150});
  solver.addRow(3, 2, {350, 400});
  solver.addRow(4, 0, {50, 150});

  std::vector<std::vector<unsigned int>> solutions;
  std::atomic<bool> abort_flag{false};
  std::atomic<uint64_t> iterations{0};

  solver.solve([&](const std::vector<unsigned int> &sol) {
    std::vector<unsigned int> sorted = sol;
    std::sort(sorted.begin(), sorted.end());
    solutions.push_back(sorted);
    return true;
  }, abort_flag, iterations);

  REQUIRE(solutions.size() == 1);
  REQUIRE(solutions[0] == std::vector<unsigned int>{1, 2, 3});
  REQUIRE(iterations.load() > 0);
}

TEST_CASE("SimdExactCover256 solveSubtree", "[simd][exact_cover]") {
  SimdExactCover256 solver(7, 3);

  SimdBitset256 req;
  for (unsigned int i = 0; i < 7; i++) req.set(i);
  solver.setRequiredColumns(req);

  solver.addRow(1, 0, {2, 4});
  solver.addRow(2, 1, {0, 3, 6});
  solver.addRow(3, 2, {1, 2, 5});
  solver.addRow(4, 1, {0, 3, 5});
  solver.addRow(5, 2, {1, 6});
  solver.addRow(6, 1, {3, 4, 6});

  std::vector<std::vector<unsigned int>> solutions;
  std::atomic<bool> abort_flag{false};
  std::atomic<uint64_t> iterations{0};

  // Subtree with correct prefix {1} should find the unique solution
  solver.solveSubtree({1}, [&](const std::vector<unsigned int> &sol) {
    std::vector<unsigned int> sorted = sol;
    std::sort(sorted.begin(), sorted.end());
    solutions.push_back(sorted);
    return true;
  }, abort_flag, iterations);

  REQUIRE(solutions.size() == 1);
  REQUIRE(solutions[0] == std::vector<unsigned int>{1, 4, 5});

  // Subtree with wrong prefix {2} should find no solutions
  solutions.clear();
  solver.solveSubtree({2}, [&](const std::vector<unsigned int> &sol) {
    solutions.push_back(sol);
    return true;
  }, abort_flag, iterations);

  REQUIRE(solutions.empty());
}

TEST_CASE("SimdExactCover256 concurrent solveSubtree", "[simd][exact_cover][threads]") {
  SimdExactCover256 solver(7, 3);

  SimdBitset256 req;
  for (unsigned int i = 0; i < 7; i++) req.set(i);
  solver.setRequiredColumns(req);

  solver.addRow(1, 0, {2, 4});
  solver.addRow(2, 1, {0, 3, 6});
  solver.addRow(3, 2, {1, 2, 5});
  solver.addRow(4, 1, {0, 3, 5});
  solver.addRow(5, 2, {1, 6});
  solver.addRow(6, 1, {3, 4, 6});

  std::vector<std::vector<unsigned int>> all_solutions;
  std::mutex sol_mutex;
  std::atomic<bool> abort_flag{false};
  std::atomic<uint64_t> iterations{0};

  // Branch on candidate rows covering column 0: row 2 and row 4
  std::vector<unsigned int> prefixes = {2, 4};
  std::vector<std::thread> threads;

  for (unsigned int p : prefixes) {
    threads.emplace_back([&, p]() {
      std::atomic<uint64_t> thread_iter{0};
      solver.solveSubtree({p}, [&](const std::vector<unsigned int> &sol) {
        std::vector<unsigned int> sorted = sol;
        std::sort(sorted.begin(), sorted.end());
        std::lock_guard<std::mutex> lock(sol_mutex);
        all_solutions.push_back(sorted);
        return true;
      }, abort_flag, thread_iter);
      iterations.fetch_add(thread_iter.load());
    });
  }

  for (auto &t : threads) {
    t.join();
  }

  REQUIRE(all_solutions.size() == 1);
  REQUIRE(all_solutions[0] == std::vector<unsigned int>{1, 4, 5});
  REQUIRE(iterations.load() > 0);
}

/* bt_assert only throws in a build with assertions */
#ifndef NDEBUG
TEST_CASE("SimdExactCover256 bounds check assertions", "[simd][exact_cover]") {
  CHECK_THROWS_AS(SimdExactCover256(257, 1), assert_exception);

  SimdExactCover256 solver(10, 2);
  CHECK_THROWS_AS(solver.addRow(1, 0, {10}), assert_exception);
  CHECK_THROWS_AS(solver.addRow(2, 0, {256}), assert_exception);

  SimdBitset256 bitset;
  CHECK_THROWS_AS(bitset.set(256), assert_exception);
  CHECK_THROWS_AS(bitset.reset(256), assert_exception);
  CHECK_THROWS_AS(bitset.test(256), assert_exception);
}
#endif

#include "lib/simd_huang_cover.h"

TEST_CASE("SimdHuangCover256 duplicate pieces exact cover", "[simd][huang]") {
  // Shape 1 (col 1): 2 pieces required
  // Shape 2 (col 2): 1 piece required
  // Voxels (cols 3..7): 5 voxels, 1 required each
  SimdHuangCover256 solver(7, 2);

  // Column bounds
  solver.setColumnBounds(1, 2, 2, false, true, false, false); // Shape 1
  solver.setColumnBounds(2, 1, 1, false, true, false, false); // Shape 2
  for (unsigned int v = 3; v <= 7; v++) {
    solver.setColumnBounds(v, 1, 1, true, false, false, false); // Voxels 3..7
  }

  // Rows for Shape 1:
  // Row 1: Shape 1, voxels 3, 4
  solver.addRow(1, 0, 1, 0, 0, {1, 3, 4}, {1, 1, 1});
  // Row 2: Shape 1, voxels 5, 6
  solver.addRow(2, 0, 1, 1, 0, {1, 5, 6}, {1, 1, 1});
  // Row 3: Shape 1, voxels 6, 7
  solver.addRow(3, 0, 1, 2, 0, {1, 6, 7}, {1, 1, 1});

  // Rows for Shape 2:
  // Row 4: Shape 2, voxel 7
  solver.addRow(4, 1, 2, 0, 0, {2, 7}, {1, 1});
  // Row 5: Shape 2, voxel 5
  solver.addRow(5, 1, 2, 1, 0, {2, 5}, {1, 1});

  std::vector<std::vector<unsigned int>> solutions;
  std::atomic<bool> abort_flag{false};
  std::atomic<uint64_t> iterations{0};

  solver.solve([&](const std::vector<unsigned int> &sol) {
    std::vector<unsigned int> sorted = sol;
    std::sort(sorted.begin(), sorted.end());
    solutions.push_back(sorted);
    return true;
  }, abort_flag, iterations);

  // Two distinct valid solutions: {1, 2, 4} and {1, 3, 5}
  REQUIRE(solutions.size() == 2);
  std::set<std::vector<unsigned int>> sol_set(solutions.begin(), solutions.end());
  REQUIRE(sol_set.count(std::vector<unsigned int>{1, 2, 4}) == 1);
  REQUIRE(sol_set.count(std::vector<unsigned int>{1, 3, 5}) == 1);
  REQUIRE(iterations.load() > 0);
}

TEMPLATE_TEST_CASE("SimdHuangCover extended sizes duplicate pieces exact cover", "[simd][huang]",
                   SimdHuangCover512, SimdHuangCover1024, SimdHuangCover4096, SimdHuangCover32768) {
  unsigned int base_v = 280;
  TestType solver(base_v + 5, 2);

  solver.setColumnBounds(1, 2, 2, false, true, false, false); // Shape 1
  solver.setColumnBounds(2, 1, 1, false, true, false, false); // Shape 2
  for (unsigned int v = base_v; v < base_v + 5; v++) {
    solver.setColumnBounds(v, 1, 1, true, false, false, false);
  }

  // Rows for Shape 1:
  // Row 1: Shape 1, voxels base_v, base_v + 1
  solver.addRow(1, 0, 1, 0, 0, {1, base_v, base_v + 1}, {1, 1, 1});
  // Row 2: Shape 1, voxels base_v + 2, base_v + 3
  solver.addRow(2, 0, 1, 1, 0, {1, base_v + 2, base_v + 3}, {1, 1, 1});
  // Row 3: Shape 1, voxels base_v + 3, base_v + 4
  solver.addRow(3, 0, 1, 2, 0, {1, base_v + 3, base_v + 4}, {1, 1, 1});

  // Rows for Shape 2:
  // Row 4: Shape 2, voxel base_v + 4
  solver.addRow(4, 1, 2, 0, 0, {2, base_v + 4}, {1, 1});
  // Row 5: Shape 2, voxel base_v + 2
  solver.addRow(5, 1, 2, 1, 0, {2, base_v + 2}, {1, 1});

  std::vector<std::vector<unsigned int>> solutions;
  std::atomic<bool> abort_flag{false};
  std::atomic<uint64_t> iterations{0};

  solver.solve([&](const std::vector<unsigned int> &sol) {
    std::vector<unsigned int> sorted = sol;
    std::sort(sorted.begin(), sorted.end());
    solutions.push_back(sorted);
    return true;
  }, abort_flag, iterations);

  REQUIRE(solutions.size() == 2);
  REQUIRE(solutions[0] == std::vector<unsigned int>{1, 2, 4});
  REQUIRE(solutions[1] == std::vector<unsigned int>{1, 3, 5});
  REQUIRE(iterations.load() > 0);

  /* solveSubtree: what the threads of a parallel search call for a task of
   * the plain search, with the rows the task has placed and the rows it has
   * hidden. It must find the covers that hold the first and none of the
   * second, each once.
   */
  auto subtree = [&](const std::vector<unsigned int> & placed, const std::vector<unsigned int> & hidden) {
    std::vector<std::vector<unsigned int>> found;
    std::atomic<uint64_t> it{0};
    solver.solveSubtree(placed, hidden, [&](const std::vector<unsigned int> & sol) {
      std::vector<unsigned int> sorted = sol;
      std::sort(sorted.begin(), sorted.end());
      found.push_back(sorted);
      return true;
    }, abort_flag, it);
    std::sort(found.begin(), found.end());
    return found;
  };
  typedef std::vector<std::vector<unsigned int>> covers_t;

  /* nothing placed, nothing hidden: the whole search */
  CHECK(subtree({}, {}) == covers_t{{1, 2, 4}, {1, 3, 5}});
  /* a row placed (in any order with the rows the search adds) */
  CHECK(subtree({2}, {}) == covers_t{{1, 2, 4}});
  CHECK(subtree({5}, {}) == covers_t{{1, 3, 5}});
  CHECK(subtree({1}, {}) == covers_t{{1, 2, 4}, {1, 3, 5}});
  CHECK(subtree({4, 1}, {}) == covers_t{{1, 2, 4}});
  /* a row hidden; 0 is the mark between groups of hidden rows, not a row */
  CHECK(subtree({}, {0, 2, 0}) == covers_t{{1, 3, 5}});
  CHECK(subtree({1}, {3}) == covers_t{{1, 2, 4}});
  /* rows that do not go together: no cover, and no crash */
  CHECK(subtree({2, 3}, {}).empty());
  CHECK(subtree({1}, {2, 3}).empty());
}


namespace {

#ifdef _WIN32
void hc_set_env_var(const char * name, const char * value) {
  if (value) _putenv_s(name, value);
  else _putenv_s(name, "");
}
#else
void hc_set_env_var(const char * name, const char * value) {
  if (value) setenv(name, value, 1);
  else unsetenv(name);
}
#endif

struct HcScopedEnv {
  std::string name;
  bool hadValue;
  std::string oldValue;

  HcScopedEnv(const char * var, const char * val) : name(var) {
    const char * existing = std::getenv(var);
    hadValue = existing != nullptr;
    if (hadValue) oldValue = existing;
    hc_set_env_var(var, val);
  }

  ~HcScopedEnv() {
    hc_set_env_var(name.c_str(), hadValue ? oldValue.c_str() : nullptr);
  }
};

}  // namespace

/* The SIMD kill switches must actually reach the kernel that runs.
 *
 * filterRows() tries AVX-512 before AVX2, so gating only use_avx2 on
 * BURRTOOLS_NO_AVX2 made that variable select *wider* SIMD on an AVX-512
 * host instead of falling back, leaving the scalar loop unreachable from
 * tier 512 up. Every kernel returns the same solutions, so this cannot be
 * caught by comparing results -- it needs the dispatch decision itself.
 */
TEMPLATE_TEST_CASE("SimdHuangCover: the SIMD kill switches disable every kernel",
                   "[simd][huang][dispatch]",
                   SimdHuangCover256, SimdHuangCover512, SimdHuangCover1024,
                   SimdHuangCover4096, SimdHuangCover32768) {
  SECTION("BURRTOOLS_NO_SIMD falls all the way back to scalar") {
    HcScopedEnv env("BURRTOOLS_NO_SIMD", "1");
    TestType solver(7, 2);
    CHECK(std::string(solver.activeKernel()) == "scalar");
  }

  SECTION("BURRTOOLS_NO_AVX2 does not leave AVX-512 enabled") {
    HcScopedEnv env("BURRTOOLS_NO_AVX2", "1");
    TestType solver(7, 2);
    CHECK(std::string(solver.activeKernel()) != "avx512");
    CHECK(std::string(solver.activeKernel()) != "avx2");
  }

  SECTION("BURRTOOLS_NO_AVX512 leaves the narrower kernels alone") {
    HcScopedEnv env("BURRTOOLS_NO_AVX512", "1");
    TestType solver(7, 2);
    CHECK(std::string(solver.activeKernel()) != "avx512");
  }
}

/* A range column must not be branched on when there are no variable voxels.
 * Nothing keeps the rows of a range column in order the way a shape column's
 * are, so two range rows that go together were reported once for each order
 * ({1,2} and {2,1}). The columns here are laid out so that the range column
 * is the one with the fewest choices and would be picked. From upstream
 * burr-tools (Arne Köhn, #114).
 */
TEMPLATE_TEST_CASE("SimdHuangCover: a range column reports each assembly once", "[simd][huang][range]",
                   SimdHuangCover256, SimdHuangCover512) {
  // column 1: the shape, exactly 2 pieces. columns 2-5: voxels. column 6: range.
  TestType solver(6, 1);
  solver.setColumnBounds(1, 2, 2, false, true, false, false);
  for (unsigned int v = 2; v <= 5; v++)
    solver.setColumnBounds(v, 1, 1, true, false, false, false);
  solver.setColumnBounds(6, 1, 100, false, false, true, false);

  // rows 1 and 2 each cover half the voxels and carry range weight; rows 3-6
  // carry none and are laid out so that every voxel has 3 rows while the
  // range column has 2. The only assembly is {1, 2}.
  solver.addRow(1, 0, 1, 0, 1, {1, 2, 3, 6}, {1, 1, 1, 1});
  solver.addRow(2, 0, 1, 1, 1, {1, 4, 5, 6}, {1, 1, 1, 1});
  solver.addRow(3, 0, 1, 2, 0, {1, 2, 4}, {1, 1, 1});
  solver.addRow(4, 0, 1, 3, 0, {1, 3, 5}, {1, 1, 1});
  solver.addRow(5, 0, 1, 4, 0, {1, 2, 5}, {1, 1, 1});
  solver.addRow(6, 0, 1, 5, 0, {1, 3, 4}, {1, 1, 1});

  std::vector<std::vector<unsigned int>> sols;
  std::atomic<bool> abort_flag{false};
  std::atomic<uint64_t> iterations{0};
  solver.solve([&](const std::vector<unsigned int> & s) {
    std::vector<unsigned int> sorted = s;
    std::sort(sorted.begin(), sorted.end());
    sols.push_back(sorted);
    return true;
  }, abort_flag, iterations);

  REQUIRE(sols.size() == 1);
  CHECK(sols[0] == std::vector<unsigned int>{1, 2});
}

/* Optional columns (assembler 0's variable voxels) and the hole budget.
 * From upstream burr-tools (Arne Köhn, #115).
 */
TEMPLATE_TEST_CASE("SimdExactCover: optional columns and the hole budget", "[simd][exact_cover][holes]",
                   SimdExactCover256, SimdExactCover512) {

  // column 0: the piece. columns 1, 2: voxels that must be filled.
  // column 3: a variable voxel, which may stay empty if the budget allows.
  auto build = [](unsigned int budget, bool fillable) {
    auto solver = std::make_unique<TestType>(4, 1);
    solver->setHoleBudget(budget);
    solver->setRequiredColumn(0);
    solver->setRequiredColumn(1);
    solver->setRequiredColumn(2);
    solver->setOptionalColumn(3);
    solver->addRow(1, 0, {0, 1, 2});         // leaves the variable voxel empty
    if (fillable)
      solver->addRow(2, 0, {0, 1, 2, 3});    // fills it
    return solver;
  };

  auto run = [](TestType & solver) {
    std::vector<std::vector<unsigned int>> sols;
    std::atomic<bool> abort_flag{false};
    std::atomic<uint64_t> iterations{0};
    solver.solve([&](const std::vector<unsigned int> & s) {
      std::vector<unsigned int> sorted = s;
      std::sort(sorted.begin(), sorted.end());
      sols.push_back(sorted);
      return true;
    }, abort_flag, iterations);
    std::sort(sols.begin(), sols.end());
    return sols;
  };

  SECTION("an optional column is never a dead end and is not asked for at the goal") {
    auto sols = run(*build(1, true));
    REQUIRE(sols.size() == 2);
    CHECK(sols[0] == std::vector<unsigned int>{1});
    CHECK(sols[1] == std::vector<unsigned int>{2});
  }

  SECTION("the budget is looked at when a column is chosen, not at the goal") {
    // as in the plain search: with no hole allowed row 1 is still reported,
    // because the variable voxel could have been filled where the choice was made
    CHECK(run(*build(0, true)).size() == 2);
  }

  SECTION("an optional column no row can fill uses up the budget") {
    CHECK(run(*build(0, false)).empty());
    auto sols = run(*build(1, false));
    REQUIRE(sols.size() == 1);
    CHECK(sols[0] == std::vector<unsigned int>{1});
  }

  SECTION("two rows may not share an optional column") {
    // two pieces, one voxel each must fill, and both want the variable voxel
    TestType solver(5, 2);
    solver.setHoleBudget(1);
    for (unsigned int c = 0; c < 4; c++)
      solver.setRequiredColumn(c);
    solver.setOptionalColumn(4);
    solver.addRow(1, 0, {0, 2, 4});
    solver.addRow(2, 1, {1, 3, 4});
    solver.addRow(3, 1, {1, 3});
    auto sols = run(solver);
    REQUIRE(sols.size() == 1);
    CHECK(sols[0] == std::vector<unsigned int>{1, 3});
  }
}

/* A search tells how far it is and counts its nodes while it runs, not only
 * when it is through.
 */
TEST_CASE("SimdExactCover: progress and iterations are reported during the search", "[simd][exact_cover][progress]") {
  // n pieces, each of which can go to any of n places: n! covers
  const unsigned int n = 8;
  SimdExactCover256 solver(2 * n, n);
  for (unsigned int c = 0; c < 2 * n; c++)
    solver.setRequiredColumn(c);
  unsigned int node = 1;
  for (unsigned int p = 0; p < n; p++)
    for (unsigned int place = 0; place < n; place++)
      solver.addRow(node++, p, {p, n + place});

  std::atomic<bool> abort_flag{false};
  std::atomic<uint64_t> iterations{0};
  uint64_t solutions = 0, seenAtFirstSolution = 0;
  std::vector<float> fractions;
  solver.solve([&](const std::vector<unsigned int> &) {
    if (solutions++ == 0)
      seenAtFirstSolution = iterations.load();
    return true;
  }, abort_flag, iterations, [&](float f) { fractions.push_back(f); });

  CHECK(solutions == 40320);
  CHECK(seenAtFirstSolution > 0);
  REQUIRE(fractions.size() > 4);
  CHECK(std::is_sorted(fractions.begin(), fractions.end()));
  CHECK(fractions.front() >= 0.0f);
  CHECK(fractions.back() <= 1.0f);
  CHECK(fractions.back() > 0.5f);
}
