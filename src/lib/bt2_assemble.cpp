/* BurrTools
 *
 * BurrTools 2 assembly driver: dancing-cells search with MCC-style split
 * and work stealing. Feature status: bt2_solver.h
 */
#include "bt2_assemble.h"

#include "assembler.h"

#include <atomic>
#include <condition_variable>
#include <deque>
#include <exception>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

unsigned int bt2ChooseAssemblerWorkers(const assembler_c * assm) {

#ifdef NO_THREADING
  (void)assm;
  return 1;
#else
  if (assm && assm->getNumThreads() > 0)
    return assm->getEffectiveThreads();

  unsigned int hw = std::thread::hardware_concurrency();
  if (hw < 1)
    hw = 1;
  if (hw <= 2)
    return 1;
  unsigned int n = hw - 2;
  if (n > 16)
    n = 16;
  return n;
#endif
}

typedef std::vector<std::unique_ptr<assembler_c>> cloneList_t;

namespace {

/* Split branches waiting for a thread, and the threads waiting for one. */
struct bt2Pool_c {
  std::mutex m;
  std::condition_variable cv;
  std::deque<std::unique_ptr<assembler_c>> ready;
  unsigned int waiting = 0;
  bool done = false;
  /* threads with nothing to search; read by the busy ones without the lock */
  std::atomic<unsigned int> hungry{0};
  std::exception_ptr error;
};

}

unsigned int bt2Assemble(assembler_c * assm, assembler_cb * callback,
                         unsigned int workerCount) {

  if (!assm)
    return 0;

  if (workerCount < 1)
    workerCount = 1;
#ifdef NO_THREADING
  workerCount = 1;
#endif

  /* branches a pause left unfinished come first */
  cloneList_t parked = assm->takeParkedSearches();

  if (workerCount == 1 && parked.empty()) {
    assm->assemble(callback);
    return 1;
  }

  assm->clearStop();
  assm->prepareForWorkers();

  bt2Pool_c pool;
  for (unsigned int i = 0; i < parked.size(); i++)
    pool.ready.push_back(std::move(parked[i]));

  /* Peel root branches so that every thread starts with one (Andreas split
   * at depth). When the first steps of the search are forced nothing can
   * be peeled yet; the threads left without then get theirs below. */
  while (pool.ready.size() + 1 < workerCount) {
    std::unique_ptr<assembler_c> n = assm->splitSearch();
    if (!n)
      break;
    assm->addProgressPeer(n.get());
    pool.ready.push_back(std::move(n));
  }

  const unsigned int slice = 1000;

  /* One thread: search what it has a slice at a time. Between slices, give
   * a waiting thread a branch; when its own search ends, wait for one. The
   * run is over when every thread waits. first is the caller's assembler,
   * which the thread that gets it does not own. */
  auto worker = [&](assembler_c * first) {

    std::unique_ptr<assembler_c> owned;
    assembler_c * cur = first;

    try {
      for (;;) {

        if (!cur) {
          std::unique_lock<std::mutex> lock(pool.m);
          pool.waiting++;
          pool.hungry.fetch_add(1, std::memory_order_relaxed);
          while (pool.ready.empty() && !pool.done && !assm->stopRequested()) {
            if (pool.waiting == workerCount) {
              pool.done = true;
              pool.cv.notify_all();
              break;
            }
            pool.cv.wait(lock);
          }
          pool.waiting--;
          pool.hungry.fetch_sub(1, std::memory_order_relaxed);
          if (pool.ready.empty() || assm->stopRequested())
            return;
          owned = std::move(pool.ready.front());
          pool.ready.pop_front();
          cur = owned.get();
        }

        if (assm->stopRequested())
          break;

        cur->assembleLimited(callback, slice);

        if (cur->searchFinished()) {
          if (owned) {
            assm->removeProgressPeer(owned.get());
            owned.reset();
          }
          cur = 0;
          continue;
        }

        if (assm->stopRequested())
          break;

        if (pool.hungry.load(std::memory_order_relaxed) > 0) {
          bool wanted;
          {
            std::lock_guard<std::mutex> lock(pool.m);
            wanted = pool.ready.size() < pool.waiting;
          }
          if (wanted) {
            std::unique_ptr<assembler_c> n = cur->splitSearch();
            if (n) {
              assm->addProgressPeer(n.get());
              std::lock_guard<std::mutex> lock(pool.m);
              pool.ready.push_back(std::move(n));
              pool.cv.notify_one();
            }
          }
        }
      }
    } catch (...) {
      assm->stop();
      std::lock_guard<std::mutex> lock(pool.m);
      if (!pool.error)
        pool.error = std::current_exception();
    }

    /* stopped: what is left of this branch waits for the next run */
    if (owned)
      assm->parkSearch(std::move(owned));
    std::lock_guard<std::mutex> lock(pool.m);
    pool.cv.notify_all();
  };

  std::vector<std::thread> threads;
  threads.reserve(workerCount - 1);
  try {
    for (unsigned int i = 1; i < workerCount; i++)
      threads.push_back(std::thread(worker, (assembler_c *)0));
  } catch (...) {
    /* fewer threads than asked for: the count the others wait for */
    std::lock_guard<std::mutex> lock(pool.m);
    workerCount = (unsigned int)threads.size() + 1;
  }

  worker(assm);

  for (unsigned int i = 0; i < threads.size(); i++)
    threads[i].join();

  /* a stop can leave branches nobody had taken yet */
  while (!pool.ready.empty()) {
    assm->parkSearch(std::move(pool.ready.front()));
    pool.ready.pop_front();
  }

  if (pool.error)
    std::rethrow_exception(pool.error);

  return workerCount;
}
