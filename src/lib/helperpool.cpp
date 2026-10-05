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
#include "helperpool.h"

#include <cstdlib>
#include <exception>

unsigned int solveThreadBudget(void) {

#ifdef NO_THREADING
  return 1;
#else
  const char * env = getenv("BURRTOOLS_THREADS");
  if (env) {
    int val = atoi(env);
    if (val > 0)
      return (unsigned int)(val > 256 ? 256 : val);
  }
  unsigned int hw = std::thread::hardware_concurrency();
  return hw > 0 ? hw : 1;
#endif
}

helperPool_c::helperPool_c(unsigned int cores) : total(cores < 1 ? 1 : cores) {

#ifndef NO_THREADING
  threads.reserve(total - 1);
  try {
    for (unsigned int i = 1; i < total; i++)
      threads.emplace_back(&helperPool_c::threadMain, this);
  } catch (...) {
    /* fewer threads than asked for: go on with the ones there are */
  }
#endif
}

helperPool_c::~helperPool_c(void) {

  {
    std::lock_guard<std::mutex> lock(m);
    quit = true;
  }
  cv.notify_all();
  for (std::thread & t : threads)
    t.join();
}

void helperPool_c::threadMain(void) {

  for (;;) {
    std::function<void()> task;
    {
      std::unique_lock<std::mutex> lock(m);
      cv.wait(lock, [this]() { return quit || !tasks.empty(); });
      if (tasks.empty())
        return;
      task = std::move(tasks.front());
      tasks.pop_front();
    }
    task();
  }
}

unsigned int helperPool_c::run(unsigned int maxThreads, const std::function<void(unsigned int)> & job) {

  /* the lent threads count down as they finish */
  struct latch_c {
    std::mutex m;
    std::condition_variable cv;
    unsigned int left = 0;
    std::exception_ptr error;
  } latch;

  unsigned int k = 0;

  if (maxThreads > 1 && !threads.empty()) {
    std::lock_guard<std::mutex> lock(m);

    /* the caller is one of the load, or uses a core all the same */
    int used = load.load(std::memory_order_relaxed);
    if (used < 1)
      used = 1;
    int freeCores = (int)total - used - (int)lent;
    int freeThreads = (int)threads.size() - (int)lent;
    int want = (int)maxThreads - 1;
    if (want > freeCores) want = freeCores;
    if (want > freeThreads) want = freeThreads;

    if (want > 0) {
      k = (unsigned int)want;
      lent += k;
      latch.left = k;
      for (unsigned int j = 1; j <= k; j++)
        tasks.push_back([&latch, &job, j]() {
          std::exception_ptr e;
          try {
            job(j);
          } catch (...) {
            e = std::current_exception();
          }
          std::lock_guard<std::mutex> l(latch.m);
          if (e && !latch.error)
            latch.error = e;
          latch.left--;
          latch.cv.notify_all();
        });
    }
  }

  if (k > 0)
    cv.notify_all();

  std::exception_ptr own;
  try {
    job(0);
  } catch (...) {
    own = std::current_exception();
  }

  if (k > 0) {
    {
      std::unique_lock<std::mutex> l(latch.m);
      latch.cv.wait(l, [&latch]() { return latch.left == 0; });
    }
    std::lock_guard<std::mutex> lock(m);
    lent -= k;
  }

  if (own)
    std::rethrow_exception(own);
  if (latch.error)
    std::rethrow_exception(latch.error);

  return k + 1;
}
