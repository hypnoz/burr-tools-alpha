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

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */
#ifndef __HELPER_POOL_H__
#define __HELPER_POOL_H__

#include <atomic>
#include <condition_variable>
#include <deque>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>

/**
 * Threads that are lent out for a short piece of work and given back.
 *
 * A search that can spread one step over several threads (one level of a
 * take-apart search) calls run() for each such step. The pool is made for a
 * number of cores; work that uses cores on its own threads (an assembly
 * search, each take-apart under way) says so with addLoad(), and the pool
 * lends only as many threads as cores are left. Several callers can use the
 * pool at once.
 */
class helperPool_c {

public:

  /** cores is the number of threads that may work at once, the callers included */
  explicit helperPool_c(unsigned int cores);
  ~helperPool_c(void);

  unsigned int cores(void) const { return total; }

  /** n cores more (or, negative, fewer) are in use outside the pool */
  void addLoad(int n) { load.fetch_add(n, std::memory_order_relaxed); }

  /**
   * Run job(0) on the calling thread and job(1), job(2), ... on lent
   * threads, at most maxThreads in all, and return when every one has
   * returned. Gives the number of threads that ran the job, at least 1. An
   * exception from any of them is thrown again here, after the others are
   * through.
   */
  unsigned int run(unsigned int maxThreads, const std::function<void(unsigned int)> & job);

private:

  unsigned int total;
  std::vector<std::thread> threads;
  std::mutex m;
  std::condition_variable cv;
  std::deque<std::function<void()>> tasks;
  bool quit = false;
  /* threads lent at the moment */
  unsigned int lent = 0;
  std::atomic<int> load{0};

  void threadMain(void);

  helperPool_c(const helperPool_c &) = delete;
  helperPool_c & operator=(const helperPool_c &) = delete;
};

/**
 * A limit on the threads a solve uses, set by the application (the Settings
 * dialog, -t n); 0 for none. It holds for everything that picks its own
 * number of threads: the assembly search, the take-apart workers and the
 * threads lent to them, the Panex and stacking searches and the packing of
 * a saved search.
 */
void setSolveThreadLimit(unsigned int n);
unsigned int solveThreadLimit(void);

/**
 * The number of threads a solve may use: the limit above when one is set,
 * else BURRTOOLS_THREADS when set, else the hardware's count; at least 1.
 */
unsigned int solveThreadBudget(void);

/**
 * With a limit of n threads, how many search for assemblies and how many take
 * them apart while both go on at once, so that together they keep to n (one
 * each when n is 1). Once the assembly search is through, the take-aparts
 * get the rest back as threads lent by the helper pool. Without a limit the
 * two are sized on their own (chooseDisasmWorkerCount in solvethread.cpp).
 */
struct solveThreadSplit_s {
  unsigned int assembly;
  unsigned int disassembly;
};
solveThreadSplit_s solveThreadSplit(unsigned int n);

#endif
