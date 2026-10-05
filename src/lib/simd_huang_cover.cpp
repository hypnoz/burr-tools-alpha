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
#include "simd_huang_cover.h"
#include "bt_assert.h"

#include <cstdlib>
#include <cstdio>
#include <algorithm>
#include <thread>
#include <mutex>

template <typename BitsetType>
SimdHuangCover<BitsetType>::SimdHuangCover(unsigned int num_cols, unsigned int num_s)
  : num_columns(num_cols), num_shapes(num_s) {
  bt_assert(num_cols <= BitsetType::NUM_WORDS * 64);
  columns.resize(num_columns + 1);

#if (defined(__x86_64__) || defined(_M_X64)) && (defined(__GNUC__) || defined(__clang__))
  /* BURRTOOLS_NO_SIMD and BURRTOOLS_NO_AVX2 clear *both* flags, as in
   * SimdExactCover. filterRows() tries the AVX-512 kernel first, so gating
   * only use_avx2 would make BURRTOOLS_NO_AVX2=1 run wider SIMD instead of
   * narrower, and leave the scalar fallback unreachable from tier 512 up.
   */
  if (!std::getenv("BURRTOOLS_NO_SIMD") && !std::getenv("BURRTOOLS_NO_AVX2")) {
    use_avx2 = __builtin_cpu_supports("avx2");
    use_avx512 = __builtin_cpu_supports("avx512f");
  }
  if (std::getenv("BURRTOOLS_NO_AVX512")) {
    use_avx512 = false;
  }
#elif defined(__aarch64__) || defined(__ARM_NEON)
  use_neon = !(std::getenv("BURRTOOLS_NO_SIMD") || std::getenv("BURRTOOLS_NO_NEON"));
#endif
}

template <typename BitsetType>
void SimdHuangCover<BitsetType>::setColumnBounds(
  unsigned int col,
  unsigned int min_w,
  unsigned int max_w,
  bool is_voxel,
  bool is_shape,
  bool is_range,
  bool is_hole
) {
  bt_assert(col <= num_columns);
  bt_assert(col <= BitsetType::NUM_WORDS * 64);

  /* search() checks voxel columns with placed_voxels.containsAll(required_voxels),
   * which is a presence test: it cannot distinguish weight 1 from weight n. A
   * voxel column demanding more than one unit would be reported satisfied at
   * one, so the contract for this class is min_weight <= 1 on voxel columns.
   * assembler_1_c always sets 1 for real voxels and 0 for hole columns.
   */
  bt_assert(!is_voxel || min_w <= 1);

  if (col >= columns.size()) {
    columns.resize(col + 1);
  }

  /* Contribution this column already made to total_min_pieces, so that calling
   * setColumnBounds() twice for the same shape column replaces it rather than
   * double-counting. A double count would push the goal-check gate in search()
   * out of reach and silently suppress every solution.
   */
  const unsigned int prev_min_pieces = columns[col].is_shape ? columns[col].min_weight : 0u;

  columns[col].min_weight = min_w;
  columns[col].max_weight = max_w;
  columns[col].is_voxel = is_voxel;
  columns[col].is_shape = is_shape;
  columns[col].is_range = is_range;
  columns[col].is_hole = is_hole;

  if (is_shape) {
    total_min_pieces += min_w;
  }
  total_min_pieces -= prev_min_pieces;

  if (is_range) {
    has_range = true;
    range_column = col;
  }
  if (is_hole && std::find(hole_columns.begin(), hole_columns.end(), col) == hole_columns.end()) {
    /* guarded for the same reason as total_min_pieces above: a repeated call
     * must not list the same hole column twice and inflate the empty-hole
     * count that search() prunes on
     */
    hole_columns.push_back(col);
  }
  if (is_voxel && min_w > 0 && col > 0 && (col - 1) < BitsetType::NUM_WORDS * 64) {
    required_voxels.set(col - 1);
  }
  active_column_list.push_back(col);
}

template <typename BitsetType>
uint32_t SimdHuangCover<BitsetType>::addRow(
  unsigned int node_id,
  unsigned int shape_id,
  unsigned int shape_col,
  unsigned int shape_row_idx,
  unsigned int range_weight,
  const std::vector<unsigned int> &cols,
  const std::vector<unsigned int> &weights
) {
  uint32_t idx = static_cast<uint32_t>(rows.size());
  Row r;
  r.node_id = node_id;
  r.shape_id = shape_id;
  r.shape_col = shape_col;
  r.shape_row_idx = shape_row_idx;
  r.range_weight = range_weight;
  r.columns = cols;
  r.weights = weights;

  for (size_t i = 0; i < cols.size(); i++) {
    unsigned int c = cols[i];
    bt_assert(c <= num_columns);
    bt_assert(c <= BitsetType::NUM_WORDS * 64);
    if (c > 0 && c <= num_columns && (c - 1) < BitsetType::NUM_WORDS * 64 && columns[c].is_voxel) {
      r.voxel_mask.set(c - 1);
    }
  }

  node_to_row_idx[node_id] = idx;
  rows.push_back(std::move(r));
  return idx;
}

template <typename BitsetType>
void SimdHuangCover<BitsetType>::registerNodeAlias(unsigned int node_id, uint32_t row_idx) {
  node_to_row_idx[node_id] = row_idx;
}

#if (defined(__x86_64__) || defined(_M_X64)) && (defined(__GNUC__) || defined(__clang__))
template <typename BitsetType>
__attribute__((target("avx512f")))
void SimdHuangCover<BitsetType>::filterRowsAvx512(
  const std::vector<uint32_t> &src,
  uint32_t chosen_idx,
  const BitsetType &chosen_voxel_mask,
  unsigned int chosen_shape,
  bool shape_is_full,
  bool filter_monotonic,
  unsigned int chosen_shape_row_idx,
  bool check_range,
  unsigned int max_allowed_range_weight,
  std::vector<uint32_t> &dst
) const {
  if constexpr (BitsetType::NUM_WORDS >= 8) {
    constexpr size_t N_VEC = BitsetType::NUM_WORDS / 8;
    __m512i va[N_VEC];
    for (size_t i = 0; i < N_VEC; ++i) {
      va[i] = _mm512_load_si512(reinterpret_cast<const void*>(&chosen_voxel_mask.words[i * 8]));
    }

    for (uint32_t idx : src) {
      if (idx == chosen_idx)
        continue;
      const auto &cand = rows[idx];
      if (cand.shape_id == chosen_shape) {
        if (shape_is_full || (filter_monotonic && cand.shape_row_idx <= chosen_shape_row_idx))
          continue;
      }
      if (check_range && cand.range_weight > max_allowed_range_weight)
        continue;

      const uint64_t *rw = cand.voxel_mask.words;
      bool disjoint = true;
      for (size_t i = 0; i < N_VEC; ++i) {
        __m512i vb = _mm512_load_si512(reinterpret_cast<const void*>(&rw[i * 8]));
        if (_mm512_test_epi64_mask(va[i], vb) != 0) {
          disjoint = false;
          break;
        }
      }
      if (disjoint) {
        dst.push_back(idx);
      }
    }
  } else {
    filterRowsAvx2(src, chosen_idx, chosen_voxel_mask, chosen_shape, shape_is_full,
                   filter_monotonic, chosen_shape_row_idx, check_range, max_allowed_range_weight, dst);
  }
}
template <typename BitsetType>
__attribute__((target("avx2")))
void SimdHuangCover<BitsetType>::filterRowsAvx2(
  const std::vector<uint32_t> &src,
  uint32_t chosen_idx,
  const BitsetType &chosen_voxel_mask,
  unsigned int chosen_shape,
  bool shape_is_full,
  bool filter_monotonic,
  unsigned int chosen_shape_row_idx,
  bool check_range,
  unsigned int max_allowed_range_weight,
  std::vector<uint32_t> &dst
) const {
  constexpr size_t N_VEC = BitsetType::NUM_WORDS / 4;
  __m256i va[N_VEC];
  for (size_t i = 0; i < N_VEC; ++i) {
    va[i] = _mm256_load_si256(reinterpret_cast<const __m256i*>(&chosen_voxel_mask.words[i * 4]));
  }

  for (uint32_t idx : src) {
    if (idx == chosen_idx)
      continue;
    const auto &cand = rows[idx];
    if (cand.shape_id == chosen_shape) {
      if (shape_is_full || (filter_monotonic && cand.shape_row_idx <= chosen_shape_row_idx))
        continue;
    }
    if (check_range && cand.range_weight > max_allowed_range_weight)
      continue;

    const uint64_t *rw = cand.voxel_mask.words;
    bool disjoint = true;
    for (size_t i = 0; i < N_VEC; ++i) {
      __m256i vb = _mm256_load_si256(reinterpret_cast<const __m256i*>(&rw[i * 4]));
      if (!_mm256_testz_si256(va[i], vb)) {
        disjoint = false;
        break;
      }
    }
    if (disjoint) {
      dst.push_back(idx);
    }
  }
}
#elif defined(__aarch64__) || defined(__ARM_NEON)
template <typename BitsetType>
void SimdHuangCover<BitsetType>::filterRowsNeon(
  const std::vector<uint32_t> &src,
  uint32_t chosen_idx,
  const BitsetType &chosen_voxel_mask,
  unsigned int chosen_shape,
  bool shape_is_full,
  bool filter_monotonic,
  unsigned int chosen_shape_row_idx,
  bool check_range,
  unsigned int max_allowed_range_weight,
  std::vector<uint32_t> &dst
) const {
  constexpr size_t N_VEC = BitsetType::NUM_WORDS / 2;
  uint64x2_t va[N_VEC];
  for (size_t i = 0; i < N_VEC; ++i) {
    va[i] = vld1q_u64(&chosen_voxel_mask.words[i * 2]);
  }

  for (uint32_t idx : src) {
    if (idx == chosen_idx)
      continue;
    const auto &cand = rows[idx];
    if (cand.shape_id == chosen_shape) {
      if (shape_is_full || (filter_monotonic && cand.shape_row_idx <= chosen_shape_row_idx))
        continue;
    }
    if (check_range && cand.range_weight > max_allowed_range_weight)
      continue;

    const uint64_t *rw = cand.voxel_mask.words;
    bool disjoint = true;
    for (size_t i = 0; i < N_VEC; ++i) {
      uint64x2_t vb = vld1q_u64(&rw[i * 2]);
      uint64x2_t c = vandq_u64(va[i], vb);
      if ((vgetq_lane_u64(c, 0) | vgetq_lane_u64(c, 1)) != 0) {
        disjoint = false;
        break;
      }
    }
    if (disjoint) {
      dst.push_back(idx);
    }
  }
}
#endif

template <typename BitsetType>
void SimdHuangCover<BitsetType>::filterRows(
  const std::vector<uint32_t> &src,
  uint32_t chosen_idx,
  const BitsetType &chosen_voxel_mask,
  unsigned int chosen_shape,
  bool shape_is_full,
  bool filter_monotonic,
  unsigned int chosen_shape_row_idx,
  bool check_range,
  unsigned int max_allowed_range_weight,
  std::vector<uint32_t> &dst
) const {
  if (dst.capacity() < src.size()) {
    dst.reserve(src.size());
  }

#if (defined(__x86_64__) || defined(_M_X64)) && (defined(__GNUC__) || defined(__clang__))
  if (use_avx512 && BitsetType::NUM_WORDS >= 8) {
    filterRowsAvx512(src, chosen_idx, chosen_voxel_mask, chosen_shape, shape_is_full,
                     filter_monotonic, chosen_shape_row_idx, check_range, max_allowed_range_weight, dst);
    return;
  }
  if (use_avx2) {
    filterRowsAvx2(src, chosen_idx, chosen_voxel_mask, chosen_shape, shape_is_full,
                   filter_monotonic, chosen_shape_row_idx, check_range, max_allowed_range_weight, dst);
    return;
  }
#elif defined(__aarch64__) || defined(__ARM_NEON)
  if (use_neon) {
    filterRowsNeon(src, chosen_idx, chosen_voxel_mask, chosen_shape, shape_is_full,
                   filter_monotonic, chosen_shape_row_idx, check_range, max_allowed_range_weight, dst);
    return;
  }
#endif

  for (uint32_t idx : src) {
    if (idx == chosen_idx)
      continue;
    const auto &cand = rows[idx];
    if (cand.shape_id == chosen_shape) {
      if (shape_is_full || (filter_monotonic && cand.shape_row_idx <= chosen_shape_row_idx))
        continue;
    }
    if (check_range && cand.range_weight > max_allowed_range_weight)
      continue;

    if (is_disjoint_scalar(chosen_voxel_mask, cand.voxel_mask)) {
      dst.push_back(idx);
    }
  }
}

template <typename BitsetType>
void SimdHuangCover<BitsetType>::solve(
  SolutionCallback callback,
  const std::atomic<bool> &abort_flag,
  std::atomic<uint64_t> &iterations,
  ProgressCallback progress
) const {
  if (rows.empty() || active_column_list.empty())
    return;

  SearchContext ctx;
  initContext(ctx);
  ctx.progress = progress ? &progress : nullptr;

  ctx.scratch_active_rows[0].resize(rows.size());
  for (size_t i = 0; i < rows.size(); i++) {
    ctx.scratch_active_rows[0][i] = static_cast<uint32_t>(i);
  }

  search(0, ctx, callback, abort_flag, iterations);

  flushIterations(ctx, iterations);
}

template <typename BitsetType>
void SimdHuangCover<BitsetType>::solveSubtree(
  const std::vector<unsigned int> &prefix_node_ids,
  const std::vector<unsigned int> &hidden_node_ids,
  SolutionCallback callback,
  const std::atomic<bool> &abort_flag,
  std::atomic<uint64_t> &iterations,
  ProgressCallback progress
) const {
  if (rows.empty() || active_column_list.empty())
    return;

  SearchContext ctx;
  initContext(ctx);
  ctx.progress = progress ? &progress : nullptr;
  ctx.base_depth = static_cast<unsigned int>(prefix_node_ids.size());
  /* the prefix comes from the plain search, which may have placed more rows
   * than this search would ever have on its own */
  if (ctx.scratch_active_rows.size() < prefix_node_ids.size() + 16)
    ctx.scratch_active_rows.resize(prefix_node_ids.size() + 16);

  std::vector<bool> is_hidden(rows.size(), false);
  for (unsigned int h : hidden_node_ids) {
    if (h == 0) continue;
    auto it = node_to_row_idx.find(h);
    if (it != node_to_row_idx.end()) {
      is_hidden[it->second] = true;
    }
  }

  ctx.scratch_active_rows[0].reserve(rows.size());
  for (size_t i = 0; i < rows.size(); i++) {
    if (!is_hidden[i]) {
      ctx.scratch_active_rows[0].push_back(static_cast<uint32_t>(i));
    }
  }

  bool conflict = false;

  for (unsigned int d = 0; d < prefix_node_ids.size(); d++) {
    auto it = node_to_row_idx.find(prefix_node_ids[d]);
    if (it == node_to_row_idx.end()) {
      conflict = true;
      break;
    }
    uint32_t r_idx = it->second;
    const auto &cand = rows[r_idx];

    // Check voxel conflict
    if (!is_disjoint_scalar(ctx.placed_voxels, cand.voxel_mask)) {
      conflict = true;
      break;
    }
    // Check shape bound
    if (ctx.col_weights[cand.shape_col] + 1 > columns[cand.shape_col].max_weight) {
      conflict = true;
      break;
    }
    // Check range bound
    if (has_range && ctx.col_weights[range_column] + cand.range_weight > columns[range_column].max_weight) {
      conflict = true;
      break;
    }

    // Place candidate
    ctx.placed_voxels = ctx.placed_voxels | cand.voxel_mask;
    for (size_t i = 0; i < cand.columns.size(); i++) {
      ctx.col_weights[cand.columns[i]] += cand.weights[i];
    }
    ctx.current_solution.push_back(cand.node_id);

    bool shape_full = (ctx.col_weights[cand.shape_col] >= columns[cand.shape_col].max_weight);
    unsigned int max_allowed_range = has_range ? (columns[range_column].max_weight - ctx.col_weights[range_column]) : 0;

    auto &next_active = ctx.scratch_active_rows[d + 1];
    next_active.clear();
    filterRows(ctx.scratch_active_rows[d], r_idx, cand.voxel_mask, cand.shape_id, shape_full,
               false, cand.shape_row_idx, has_range, max_allowed_range, next_active);
  }

  if (!conflict) {
    search(prefix_node_ids.size(), ctx, callback, abort_flag, iterations);
  }

  flushIterations(ctx, iterations);
}

template <typename BitsetType>
void SimdHuangCover<BitsetType>::search(
  unsigned int depth,
  SearchContext &ctx,
  SolutionCallback &callback,
  const std::atomic<bool> &abort_flag,
  std::atomic<uint64_t> &iterations
) const {
  if (abort_flag.load(std::memory_order_relaxed))
    return;

  ctx.local_iterations++;
  if (ctx.local_iterations - ctx.flushed_iterations >= 256) {
    iterations.fetch_add(256, std::memory_order_relaxed);
    ctx.flushed_iterations += 256;
  }
  /* on a count of its own: where solutions come thick, each of them flushes
   * and the batch above is never full */
  if (ctx.progress && (ctx.local_iterations & 4095) == 0)
    (*ctx.progress)(ctx.fraction());

  // Goal check: are all conditions fulfilled?
  if (ctx.current_solution.size() >= total_min_pieces) {
    if (ctx.placed_voxels.containsAll(required_voxels)) {
      bool all_fulfilled = true;
      for (unsigned int c = 1; c <= num_shapes; c++) {
        if (ctx.col_weights[c] < columns[c].min_weight || ctx.col_weights[c] > columns[c].max_weight) {
          all_fulfilled = false;
          break;
        }
      }
      if (all_fulfilled && has_range) {
        if (ctx.col_weights[range_column] < columns[range_column].min_weight ||
            ctx.col_weights[range_column] > columns[range_column].max_weight) {
          all_fulfilled = false;
        }
      }
      if (all_fulfilled) {
        flushIterations(ctx, iterations);
        /* told here as well, see SimdExactCover::search */
        if (ctx.progress)
          (*ctx.progress)(ctx.fraction());
        if (!callback(ctx.current_solution))
          return;
        return;
      }
    }
  }

  const auto &curr_active = ctx.scratch_active_rows[depth];
  if (curr_active.empty())
    return;

  // Calculate col_counts for active rows
  std::fill(ctx.col_counts.begin(), ctx.col_counts.end(), 0);
  for (uint32_t r_idx : curr_active) {
    const auto &r = rows[r_idx];
    const unsigned int *cols = r.columns.data();
    const unsigned int *wgts = r.weights.data();
    size_t sz = r.columns.size();
    for (size_t i = 0; i < sz; i++) {
      ctx.col_counts[cols[i]] += wgts[i];
    }
  }

  // Hole pruning
  if (holes < hole_columns.size()) {
    unsigned int empty_holes = 0;
    for (unsigned int hc : hole_columns) {
      if (ctx.col_counts[hc] == 0 && ctx.col_weights[hc] == 0) {
        empty_holes++;
        if (empty_holes > holes)
          return;
      }
    }
  }

  // Combined dead-end pruning and MRV pivot column selection
  unsigned int min_metric = UINT32_MAX;
  unsigned int best_col = UINT32_MAX;

  // 1. Check shape columns (1..num_shapes)
  for (unsigned int c = 1; c <= num_shapes; c++) {
    if (ctx.col_weights[c] >= columns[c].min_weight)
      continue;
    unsigned int count = ctx.col_counts[c];
    if (ctx.col_weights[c] + count < columns[c].min_weight)
      return; // Dead end: cannot satisfy shape piece requirement
    unsigned int remaining_need = columns[c].min_weight - ctx.col_weights[c];
    unsigned int metric = count * remaining_need;
    if (metric < min_metric) {
      min_metric = metric;
      best_col = c;
    }
  }

  // 2. Check range column if present: to prune, and without variable voxels
  // never to branch on. When every voxel is covered exactly once the range
  // weight of a complete cover is forced, so the column only has to be
  // checked (rows over the maximum are filtered out, under the minimum is
  // pruned here, and the goal test looks at both). Branching on it would
  // report two range rows that go together once for each order, as nothing
  // keeps its rows in order the way a shape column's are. With variable
  // voxels it stays a pivot: filling holes may be the only way to the minimum.
  if (has_range && ctx.col_weights[range_column] < columns[range_column].min_weight) {
    unsigned int count = ctx.col_counts[range_column];
    if (ctx.col_weights[range_column] + count < columns[range_column].min_weight)
      return; // Dead end: cannot satisfy range minimum
    if (!hole_columns.empty()) {
      unsigned int remaining_need = columns[range_column].min_weight - ctx.col_weights[range_column];
      unsigned int metric = count * remaining_need;
      if (metric < min_metric) {
        min_metric = metric;
        best_col = range_column;
      }
    }
  }

  // 3. Check unplaced voxels using bitset scanning (skips placed voxels entirely)
  for (size_t w = 0; w < BitsetType::NUM_WORDS; ++w) {
    uint64_t unplaced = required_voxels.words[w] & ~ctx.placed_voxels.words[w];
    while (unplaced != 0) {
      int bit = std::countr_zero(unplaced);
      unsigned int c = static_cast<unsigned int>(w * 64 + bit + 1);
      unplaced &= (unplaced - 1);

      unsigned int count = ctx.col_counts[c];
      if (count == 0)
        return; // Dead end: required voxel has 0 remaining placements!

      if (count < min_metric) {
        min_metric = count;
        best_col = c;
      }
    }
  }

  if (best_col == UINT32_MAX)
    return;

  /* Branch on candidate rows covering best_col.
   *
   * DELIBERATELY no resize of ctx.scratch_active_rows here: curr_active is a
   * reference into that vector, and a resize would reallocate the outer buffer
   * and leave it dangling before the loop below walks it. The depth stays
   * within the list: see searchDepthBound() and initContext().
   */
  bt_assert(depth + 1 < ctx.scratch_active_rows.size());

  auto isCandidate = [&](const Row &cand) -> bool {
    bool covers_best = columns[best_col].is_shape ? (cand.shape_col == best_col)
                     : columns[best_col].is_voxel ? cand.voxel_mask.test(best_col - 1)
                     : (cand.range_weight > 0);
    if (!covers_best)
      return false;
    if (ctx.col_weights[cand.shape_col] + 1 > columns[cand.shape_col].max_weight)
      return false;
    if (has_range && ctx.col_weights[range_column] + cand.range_weight > columns[range_column].max_weight)
      return false;
    return true;
  };

  // which choice of how many, for the progress report (near the top only,
  // where counting the choices first costs nothing that matters)
  const unsigned int level = depth - ctx.base_depth;
  const bool track = ctx.progress && level < PROGRESS_LEVELS;
  if (track) {
    uint32_t n = 0;
    for (uint32_t r_idx : curr_active)
      if (isCandidate(rows[r_idx]))
        n++;
    ctx.branch_cnt[level] = n;
    ctx.branch_idx[level] = 0;
    if (level + 1 < PROGRESS_LEVELS)
      ctx.branch_cnt[level + 1] = 0;
  }
  bool first = true;

  for (uint32_t r_idx : curr_active) {
    const auto &cand = rows[r_idx];

    if (!isCandidate(cand))
      continue;

    if (track) {
      if (!first) {
        ctx.branch_idx[level]++;
        if (level + 1 < PROGRESS_LEVELS)
          ctx.branch_cnt[level + 1] = 0;
      }
      first = false;
    }

    // Place candidate row
    ctx.placed_voxels = ctx.placed_voxels | cand.voxel_mask;
    for (size_t i = 0; i < cand.columns.size(); i++) {
      ctx.col_weights[cand.columns[i]] += cand.weights[i];
    }
    ctx.current_solution.push_back(cand.node_id);

    bool shape_full = (ctx.col_weights[cand.shape_col] >= columns[cand.shape_col].max_weight);
    bool filter_monotonic = (!shape_full && columns[best_col].is_shape);
    unsigned int max_allowed_range = has_range ? (columns[range_column].max_weight - ctx.col_weights[range_column]) : 0;

    auto &next_active = ctx.scratch_active_rows[depth + 1];
    next_active.clear();

    filterRows(curr_active, r_idx, cand.voxel_mask, cand.shape_id, shape_full,
               filter_monotonic, cand.shape_row_idx, has_range, max_allowed_range, next_active);

    search(depth + 1, ctx, callback, abort_flag, iterations);

    // Backtrack (zero heap allocation, zero pointer re-linking)
    ctx.current_solution.pop_back();
    for (size_t i = 0; i < cand.columns.size(); i++) {
      ctx.col_weights[cand.columns[i]] -= cand.weights[i];
    }
    ctx.placed_voxels = ctx.placed_voxels ^ cand.voxel_mask;

    if (abort_flag.load(std::memory_order_relaxed))
      return;
  }
}

template class SimdHuangCover<SimdBitset256>;
template class SimdHuangCover<SimdBitset512>;
template class SimdHuangCover<SimdBitset1024>;
template class SimdHuangCover<SimdBitset2048>;
template class SimdHuangCover<SimdBitset4096>;
template class SimdHuangCover<SimdBitset8192>;
template class SimdHuangCover<SimdBitset16384>;
template class SimdHuangCover<SimdBitset32768>;
