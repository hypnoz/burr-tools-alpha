/* BurrTools
 *
 * BurrTools 2 assembly driver: dancing-cells search with MCC-style split
 * and work stealing. Feature status: bt2_solver.h
 */
#include "bt2_assemble.h"

#include "assembler.h"

#include <memory>
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

static assembler_c * busiestWorker(assembler_c * primary,
                                   const cloneList_t & clones) {

  assembler_c * best = 0;
  unsigned int bestWork = 0;

  if (primary && !primary->searchFinished()) {
    best = primary;
    bestWork = primary->remainingSearchWork();
  }

  for (unsigned int i = 0; i < clones.size(); i++) {
    assembler_c * c = clones[i].get();
    if (!c || c->searchFinished())
      continue;
    unsigned int w = c->remainingSearchWork();
    if (!best || w > bestWork) {
      best = c;
      bestWork = w;
    }
  }
  return best;
}

static void runSlice(assembler_c * a, assembler_cb * callback, unsigned int budget) {
  if (a && !a->searchFinished())
    a->assembleLimited(callback, budget);
}

unsigned int bt2Assemble(assembler_c * assm, assembler_cb * callback,
                         unsigned int workerCount) {

  if (!assm)
    return 0;

  if (workerCount <= 1) {
    assm->assembleLimited(callback, 0);
    return 1;
  }

  if (!assm->clonePrepared()) {
    assm->assemble(callback);
    return 1;
  }

  /* Peel root branches until the pool is full (Andreas split at depth). A
   * place left empty -- the search cannot be split while its first steps
   * are forced -- is filled later by the stealing below. */
  cloneList_t clones(workerCount - 1);
  for (unsigned int i = 0; i < clones.size(); i++) {
    clones[i] = assm->splitSearch();
    if (!clones[i])
      break;
  }

  /* Iterations of workers that finished and were replaced. */
  unsigned long extra = 0;

  auto setPeers = [&]() {
    assm->clearProgressPeers();
    for (unsigned int i = 0; i < clones.size(); i++)
      if (clones[i])
        assm->addProgressPeer(clones[i].get());
  };
  /* An idle place takes work from the busiest search. True when it got some. */
  auto steal = [&](unsigned int i) {
    if (clones[i] && !clones[i]->searchFinished())
      return false;
    assembler_c * src = busiestWorker(assm, clones);
    if (!src)
      return false;
    std::unique_ptr<assembler_c> n = src->splitSearch();
    if (!n)
      return false;
    if (clones[i])
      extra += clones[i]->getIterations();
    clones[i] = std::move(n);
    return true;
  };

  setPeers();

  const unsigned int slice = 8000;

#ifdef NO_THREADING
  bool any = true;
  while (any) {
    any = false;
    runSlice(assm, callback, slice);
    if (!assm->searchFinished())
      any = true;
    for (unsigned int i = 0; i < clones.size(); i++) {
      runSlice(clones[i].get(), callback, slice);
      if (clones[i] && !clones[i]->searchFinished())
        any = true;
    }
    for (unsigned int i = 0; i < clones.size(); i++)
      if (steal(i))
        any = true;
    setPeers();
  }
#else
  bool any = true;
  while (any) {
    std::vector<std::thread> threads;
    threads.reserve(clones.size());
    try {
      for (unsigned int i = 0; i < clones.size(); i++) {
        assembler_c * c = clones[i].get();
        if (c && !c->searchFinished()) {
          threads.push_back(std::thread([c, callback]() {
            runSlice(c, callback, 8000);
          }));
        }
      }
      runSlice(assm, callback, slice);
    } catch (...) {
      assm->stop();
      for (unsigned int i = 0; i < threads.size(); i++)
        if (threads[i].joinable())
          threads[i].join();
      assm->clearProgressPeers();
      throw;
    }
    for (unsigned int i = 0; i < threads.size(); i++)
      if (threads[i].joinable())
        threads[i].join();

    any = !assm->searchFinished();
    for (unsigned int i = 0; i < clones.size(); i++)
      if (clones[i] && !clones[i]->searchFinished())
        any = true;

    /* Finished workers steal from the largest leftover (Andreas tail). */
    for (unsigned int i = 0; i < clones.size(); i++)
      if (steal(i))
        any = true;
    if (assm->searchFinished()) {
      assembler_c * src = busiestWorker(0, clones);
      if (src) {
        std::unique_ptr<assembler_c> n = src->splitSearch();
        if (n) {
          /* Primary finished: keep it as a shell and move stolen work onto a clone. */
          clones.push_back(std::move(n));
          any = true;
        }
      }
    }

    setPeers();
  }
#endif

  for (unsigned int i = 0; i < clones.size(); i++)
    if (clones[i])
      extra += clones[i]->getIterations();
  assm->clearProgressPeers();
  assm->addIterations(extra);
  return workerCount;
}
