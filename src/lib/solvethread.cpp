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
#include "solvethread.h"

#include "disassembly.h"
#include "problem.h"
#include "puzzle.h"
#include "assembly.h"
#include "disassembler_0.h"
#include "disassembler_factory.h"
#include "bt2_assemble.h"
#include "solution.h"
#include "voxel.h"
#include "sliding.h"
#include "stacking.h"
#include "panex.h"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <memory>
#include <string>

namespace {

enum {
  SOL_COUNT_ASM,
  SOL_SAVE_ASM,
  SOL_COUNT_DISASM,
  SOL_DISASM,
};

int solutionActionFromParameters(int parameters) {
  int action = 0;
  if (!(parameters & solveThread_c::PAR_JUST_COUNT)) action += 1;
  if (parameters & solveThread_c::PAR_DISASSM) action += 2;
  return action;
}

struct disasmDurationGuard_c {
  std::atomic<unsigned int> * count;
  std::atomic<unsigned long long> * totalMs;
  std::chrono::steady_clock::time_point t0;

  disasmDurationGuard_c(std::atomic<unsigned int> * c, std::atomic<unsigned long long> * t)
    : count(c), totalMs(t), t0(std::chrono::steady_clock::now()) {}

  ~disasmDurationGuard_c() {
    long long ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - t0).count();
    if (ms < 0)
      ms = 0;
    totalMs->fetch_add((unsigned long long)ms, std::memory_order_relaxed);
    count->fetch_add(1, std::memory_order_relaxed);
  }
};

unsigned int chooseDisasmWorkerCount(bool rotationsEnabled) {
#ifdef NO_THREADING
  return 1;
#else
  /* BURRTOOLS_DISASM_WORKERS=n: that many, whatever the puzzle */
  if (const char * env = getenv("BURRTOOLS_DISASM_WORKERS")) {
    const int n = atoi(env);
    if (n > 0)
      return (unsigned int)(n > 64 ? 64 : n);
  }

  /* Rotation puzzles get the same count as the others. One worker used to
   * be the rule for them, from when extra ones made the run slower; with
   * the faster rotation search, eight workers on ten cores take a quarter
   * of the time one does (CoverUp3 9.9 s -> 2.5 s, CornerCube 26.6 s ->
   * 6.1 s). */
  (void)rotationsEnabled;

  /* with a limit set the take-aparts get their share of it */
  if (const unsigned int limit = solveThreadLimit())
    return solveThreadSplit(limit).disassembly;

  const unsigned int budget = solveThreadBudget();
  if (budget <= 2)
    return 1;
  unsigned int n = budget - 2;
  /* Leave headroom for GUI + assembler. Cap concurrent BFS fronts so a
   * large machine does not spawn dozens of searches at once. */
  if (n > 16)
    n = 16;
  return n;
#endif
}

unsigned long long elapsedMs(std::chrono::steady_clock::time_point t0) {
  long long ms = std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::steady_clock::now() - t0).count();
  if (ms < 0)
    return 0;
  return (unsigned long long)ms;
}

} // namespace

void solveThread_c::runStacking(void) {
  setSolverNote("");
  puzzle.removeAllSolutions();
  puzzle.markSolving();

  const bool find = (parameters & PAR_DISASSM) != 0;
  const bool countOnly = (parameters & PAR_JUST_COUNT) != 0;
  if (!find && !countOnly) {
    setSolverNote("Turn on Find Solutions");
    action.store(ACT_FINISHED, std::memory_order_relaxed);
    puzzle.finishedSolving();
    return;
  }

  std::string err = stacking::setupError(puzzle);
  if (!err.empty()) {
    setSolverNote(err);
    action.store(ACT_FINISHED, std::memory_order_relaxed);
    puzzle.finishedSolving();
    return;
  }

  action.store(ACT_ASSEMBLING, std::memory_order_relaxed);
  /* The Panex Solver, and the Stacking Solver on any puzzle the same search
   * can hold: that search can pause, resume and autosave. */
  if ((parameters & PAR_PANEX_SOLVER) || panex::unsupported(puzzle, true).empty()) {
    panex::panexSearch_c search;
    search.anyRules = (parameters & PAR_PANEX_SOLVER) == 0;
    search.highMemory = (parameters & PAR_HIGH_MEMORY) != 0;
    search.stop = &stopPressed;
    search.progress = &slideProgress;
    search.depth = &searchDepth;
    search.traced = &searchTraced;
    search.resume = (parameters & PAR_PANEX_RESUME) != 0;
    search.autosaveMinutes = (parameters & PAR_PANEX_NO_AUTOSAVE) ? 0 : 20;
    /* Time used counts every run of a saved search, before this one too. */
    if (search.resume)
      puzzle.addTimeMs(panex::savedMs(puzzle));
    std::unique_ptr<separation_c> path = panex::solve(puzzle, search);
    puzzle.addTimeMs(getTimeMs());
    puzzle.incNumAssemblies();
    if (path) {
      puzzle.incNumSolutions();
      if (!countOnly) {
        std::unique_ptr<assembly_c> start = stacking::startAssembly(puzzle);
        puzzle.addSolution(start.release(), path.release(), 0, 0);
      }
    } else if (search.outcome == panex::PANEX_STOPPED && search.saved) {
      setSolverNote("Paused " + std::to_string(search.depthReached) +
                    " moves deep. The search is saved: press Continue to carry on, even after quitting BurrTools.");
    } else if (search.outcome == panex::PANEX_STOPPED) {
      setSolverNote("Stopped before the search was complete.");
    } else if (search.outcome == panex::PANEX_MEMORY) {
      setSolverNote("The search outgrew its memory after " + std::to_string(search.found) +
                    " stackings." +
                    (search.saved ? std::string(" It is saved:") + (search.highMemory ? "" : " turn on Enable High Memory and") +
                                        " press Continue to carry on."
                                  : (search.highMemory ? "" : " Try Enable High Memory.")));
    } else if (search.outcome == panex::PANEX_ERROR) {
      setSolverNote(search.error);
    } else {
      setSolverNote("Every reachable stacking was searched: this puzzle has no solution.");
    }
    action.store(ACT_FINISHED, std::memory_order_relaxed);
    puzzle.finishedSolving();
    return;
  }
  stacking::stackSearch_c search;
  search.highMemory = (parameters & PAR_HIGH_MEMORY) != 0;
  search.stop = &stopPressed;
  search.progress = &slideProgress;
  std::unique_ptr<separation_c> path = stacking::findStackPath(puzzle, search);
  puzzle.addTimeMs(getTimeMs());
  if (!path) {
    if (search.outcome == stacking::STACK_STOPPED)
      setSolverNote("Stopped before the search was complete.");
    else if (search.outcome == stacking::STACK_MEMORY)
      setSolverNote("The search reached its memory limit of " +
                    std::to_string(search.memoryStates) +
                    " stackings. There may be a solution the solver did not find." +
                    (search.highMemory ? "" : " Try Enable High Memory."));
    else
      setSolverNote("Every reachable stacking was searched: this puzzle has no solution.");
    puzzle.incNumAssemblies();
    action.store(ACT_FINISHED, std::memory_order_relaxed);
    puzzle.finishedSolving();
    return;
  }

  puzzle.incNumAssemblies();
  puzzle.incNumSolutions();
  if (!countOnly) {
    std::unique_ptr<assembly_c> start = stacking::startAssembly(puzzle);
    puzzle.addSolution(start.release(), path.release(), 0, 0);
  }
  action.store(ACT_FINISHED, std::memory_order_relaxed);
  puzzle.finishedSolving();
}

void solveThread_c::run(void){

  try {

    if (stacking::isStacking(puzzle)) {
      runStacking();
      return;
    }

    /* local pointer for this thread's own use; the shared member `assm` is
     * only written (published) here and read by the GUI thread
     */
    assembler_c * a = 0;

    /* However run ends, withdraw the assembler before anything can free it:
     * the GUI may remove the problem's solutions, and with them its
     * assembler, as soon as this thread has stopped. */
    struct withdraw_c {
      solveThread_c * t;
      ~withdraw_c() { t->publishAssembler(nullptr); }
    } withdraw{this};

    /* first check, if there is an assembler available with the
     * problem, if there is one take that
     */
    const bool strictColors = (parameters & PAR_STRICT_COLORS) != 0
                              || sliding::isSliding(puzzle);

    if (sliding::isSliding(puzzle)) {
      sliding::syncMaxHoles(puzzle);
      sliding::refreshStartLocks(puzzle);
    }

    /* A prepared assembler bakes colour matching into its placement matrix.
     * A different strictness, or a saved resume point from a run that did not
     * record this option, cannot be continued.
     */
    if (puzzle.getAssembler() &&
        puzzle.getAssembler()->usesStrictColorRestrictions() != strictColors)
      puzzle.removeAllSolutions();
    else if (!puzzle.getAssembler() && strictColors)
      puzzle.removeAllSolutions();

    if (puzzle.getAssembler()) {
      a = puzzle.getAssembler();
      publishAssembler(a);
      a->applySolutionFilterFlags(parameters & PAR_KEEP_MIRROR, parameters & PAR_KEEP_ROTATIONS, parameters & PAR_COMPLETE_ROTATIONS);
    }
    else {

      /* otherwise we have to create a new one
       */
      action.store(ACT_PREPARATION, std::memory_order_relaxed);
      beginPhase(PHASE_PREPARE);
      std::unique_ptr<assembler_c> new_assm = puzzle.getPuzzle().getGridType()->findAssembler(puzzle, false, solverType);
      {
        /* While new_assm is published but still owned here, an assertion
         * thrown by createMatrix() or reduce() unwinds through this block.
         * withdraw_c above is destroyed after new_assm and would then read
         * the freed assembler (it asks it for getFinished()), so withdraw
         * from inside the block, before new_assm goes. The explicit calls
         * below stay so that the GUI never sees the error or pause state
         * with the assembler still published. */
        struct unpublish_c {
          solveThread_c * t;
          ~unpublish_c() { t->publishAssembler(nullptr); }
        } unpublish{this};

        a = new_assm.get();
        publishAssembler(a);

        errState = a->createMatrix(parameters & PAR_KEEP_MIRROR, parameters & PAR_KEEP_ROTATIONS, parameters & PAR_COMPLETE_ROTATIONS, strictColors);
        endPhase(prepareMs);
        if (errState != assembler_c::ERR_NONE) {
          errParam = a->getErrorsParam();
          publishAssembler(nullptr);  // before new_assm goes
          action.store(ACT_ERROR, std::memory_order_relaxed);
          return;
        }

        if (!stopPressed.load(std::memory_order_relaxed) && (parameters & PAR_REDUCE)) {
          action.store(ACT_REDUCE, std::memory_order_relaxed);
          beginPhase(PHASE_REDUCE);
          a->reduce();
          endPhase(reduceMs);
        }

        if (stopPressed.load(std::memory_order_relaxed)) {
          publishAssembler(nullptr);  // before new_assm goes
          action.store(ACT_PAUSING, std::memory_order_relaxed);
          return;
        }

        publishAssembler(nullptr);
      }

      /* set the assembler to the problem as soon as it is finished
       * with initialisation, NOT EARLIER as the function
       * also restores the assembler state to a state that might
       * be saved within the problem
       */
      errState = puzzle.setAssembler(std::move(new_assm));
      if (errState != assembler_c::ERR_NONE) {
        action.store(ACT_ERROR, std::memory_order_relaxed);
        return;
      }
      a = puzzle.getAssembler();
      publishAssembler(a);
    }

    if (return_after_prep) {
      action.store(ACT_PAUSING, std::memory_order_relaxed);
      return;
    }

    /* From here on a stop asked of the assembler is kept until it has
     * run. Without this, one that came between the action being set to
     * assembling and assemble() starting was thrown away as assemble()
     * cleared the flag, and the search ran to its end with Stop pressed
     * (and an autosave pause was not taken). A stop from before this point
     * has set stopPressed, which is looked at next.
     */
    a->armStop();

    if (!stopPressed.load(std::memory_order_relaxed)) {

      for (unsigned int i = 0; i < puzzle.getPuzzle().getNumberOfShapes(); i++)
        puzzle.getPuzzle().getShape(i)->initHotspot();

      action.store(ACT_ASSEMBLING, std::memory_order_relaxed);
      beginPhase(PHASE_ASSEMBLE);
      /* What a paused run had not finished with comes first: assemblies it
       * had counted go straight to be taken apart, the rest as if just found. */
      for (problem_c::pendingAssembly_c & p : puzzle.takePending()) {
        if (stopPressed.load(std::memory_order_relaxed) || !p.counted) {
          if (stopPressed.load(std::memory_order_relaxed))
            puzzle.addPending(std::move(p.assembly), p.counted, p.assemblyNumber, p.solutionNumber);
          else
            assembly(std::move(p.assembly));
          continue;
        }
#ifdef NO_THREADING
        disasmTask_c task;
        task.assembly = std::move(p.assembly);
        task.assemblyNumber = p.assemblyNumber;
        task.solutionNumber = p.solutionNumber;
        processDisassembly(task, solutionActionFromParameters(parameters), disassemblers[0].get());
#else
        enqueueDisassembly(std::move(p.assembly), p.assemblyNumber, p.solutionNumber);
#endif
      }
      {
        /* the assembly search uses its threads' cores until it is through */
        /* (less what enqueueDisassembly() has given up for the time the
         * search stands still at a full queue: held says how much is left) */
        struct load_c {
          helperPool_c * pool;
          std::atomic<int> & held;
          load_c(helperPool_c * p, int cores, std::atomic<int> & h) : pool(p), held(h) {
            if (pool) {
              pool->addLoad(cores);
              held.store(cores, std::memory_order_release);
            }
          }
          ~load_c() { if (pool) pool->addLoad(-held.exchange(0, std::memory_order_acq_rel)); }
        };
        /* With a limit on the threads, the search shares it with the
         * take-apart workers running beside it. */
        if (const unsigned int limit = solveThreadLimit())
          a->setNumThreads((parameters & PAR_DISASSM) ? solveThreadSplit(limit).assembly : limit);

        if (solverType == SOLVER_BT2) {
          const unsigned int workers = bt2ChooseAssemblerWorkers(a);
          assemblerThreadCount.store(workers, std::memory_order_relaxed);
          load_c load(helperPool.get(), (int)workers, assemblyLoad);
          assemblerThreadCount.store(bt2Assemble(a, this, workers), std::memory_order_relaxed);
        } else {
          assemblerThreadCount.store(a->getEffectiveThreads(), std::memory_order_relaxed);
          load_c load(helperPool.get(), (int)a->getEffectiveThreads(), assemblyLoad);
          a->assemble(this);
        }
      }
      endPhase(assemblyMs);

      if (!stopPressed.load(std::memory_order_relaxed)) {
        beginPhase(PHASE_DRAIN);
        /* only when there is something left to take apart, and not over a
         * stop that the GUI thread has just asked for */
        if ((parameters & PAR_DISASSM) && disasmPending.load(std::memory_order_acquire) > 0) {
          unsigned int expected = ACT_ASSEMBLING;
          action.compare_exchange_strong(expected, ACT_DISASSEMBLING, std::memory_order_relaxed);
        }
        flushDisassemblyQueue();
        endPhase(drainMs);
      } else {
        /* An assembly queued just as Pause emptied the queue would otherwise
         * wait there for workers that are gone: keep it for the next run. */
        cancelDisassemblyWork();
      }

      puzzle.addTimeMs(getTimeMs());

      if (sliding::isSliding(puzzle) && (parameters & PAR_DISASSM) &&
          !stopPressed.load(std::memory_order_relaxed))
        setSolverNote(slidingSummary());

      if (stopPressed.load(std::memory_order_relaxed))
        action.store(ACT_PAUSING, std::memory_order_relaxed);
      else if (a->getFinished() >= 1) {
        action.store(ACT_FINISHED, std::memory_order_relaxed);
        puzzle.finishedSolving();
      } else
        action.store(ACT_PAUSING, std::memory_order_relaxed);

    } else {
      action.store(ACT_PAUSING, std::memory_order_relaxed);
      puzzle.addTimeMs(getTimeMs());
    }

  }

  catch (assert_exception & a) {

    ae = a;
    publishAssembler(nullptr);
    action.store(ACT_ASSERT, std::memory_order_relaxed);
    if (puzzle.getAssembler())
      puzzle.removeAllSolutions();
  }
}

std::string solveThread_c::slidingSummary(void) const {
  /* Say plainly whether "no solution" is proven or the search gave up. */
  if (slideStartsCut > 0) {
    std::string msg = std::to_string(slideStartsCut) +
        (slideStartsCut == 1 ? " start layout was" : " start layouts were") +
        " not searched completely";
    if (!slideDiskError.empty())
      msg += ". " + slideDiskError.substr(0, slideDiskError.size() - 1);
    else if (slideMemoryCut)
      msg += ": the search reached its memory limit of " +
             std::to_string(slideMemoryStates) + " arrangements";
    else if (parameters & PAR_DEEP_SEARCH)
      msg += ": the search reached its limit of 1,000,000 arrangements";
    else
      msg += ": the search reached its limit of 250,000 arrangements";
    msg += ". There may be solutions the solver did not find.";
    if (slideMemoryCut && !(parameters & PAR_HIGH_MEMORY))
      msg += " Try Enable High Memory.";
    if (!slideMemoryCut && slideDiskError.empty() && !(parameters & PAR_FULL_SEARCH))
      msg += (parameters & PAR_DEEP_SEARCH)
          ? " Try the Sliding Full Solver."
          : " Try the Sliding Deep or Full Solver.";
    return msg;
  }
  if (puzzle.getNumSolutions() == 0 && puzzle.getNumAssemblies() > 0) {
    std::string msg = "Every reachable arrangement was searched: this puzzle has no solution "
                      "with the current settings.";
    if (!(parameters & PAR_NESTED_SLIDES))
      msg += " If a piece can carry pieces that sit inside it, try Allow Nested Slides.";
    else if (!(parameters & PAR_PARTIAL_NESTED))
      msg += " If a piece can carry pieces that stick out of it, try Allow Partially Nested.";
    return msg;
  }
  return "";
}

solveThread_c::solveThread_c(problem_c & puz, int par) :
action(ACT_PREPARATION),
puzzle(puz),
parameters(par),
sortMethod(SRT_COMPLETE_MOVES),
solverType(SOLVER_CLASSIC),
liveSort(-1),
solutionLimit(10),
solutionDrop(1),
stopPressed(false),
return_after_prep(false),
disasmWorkerStop(false),
disasmWorkerCount(0),
disasmPending(0),
disasmCompleted(0),
disasmMsTotal(0),
disasmPeakPending(0),
disasmInseparable(0),
prepareMs(0),
reduceMs(0),
assemblyMs(0),
drainMs(0)
{

  /* Persist solutions under <solutionsWithRotations> so older BurrTools skip them */
  if (par & PAR_CHECK_ROTATIONS)
    puzzle.setSolutionsWithRotations(true);

  /* A sliding start is judged against a fixed goal map, so a rotated or
   * mirrored start is a different start, not a duplicate. Keep every one. */
  if (sliding::isSliding(puzzle)) {
    parameters |= PAR_KEEP_MIRROR | PAR_KEEP_ROTATIONS;
    parameters &= ~PAR_COMPLETE_ROTATIONS;
  }
}

solveThread_c::~solveThread_c(void) {

  /* signal the worker to stop and wait for it to actually finish before we
   * free anything it might still be using. stopInternal() rather than the
   * virtual stop(): a virtual call from a destructor does not dispatch
   * further than this class anyway, and naming it makes that explicit.
   */
  stopInternal();
  joinThread();
  stopDisasmWorker();

  disassemblers.clear();
}

void solveThread_c::waitUntilFinished(void) {
  joinThread();
}

void solveThread_c::startDisasmWorker(void) {

  if (!(parameters & PAR_DISASSM))
    return;

  const bool checkRotations = (parameters & PAR_CHECK_ROTATIONS) != 0;
  unsigned int n = chooseDisasmWorkerCount(checkRotations);
  disasmWorkerCount.store(n, std::memory_order_relaxed);

  /* A take-apart with no others waiting spreads each level of its search
   * over the cores nothing else is using. */
  helperPool = std::make_unique<helperPool_c>(solveThreadBudget());
  disasmQueueLimit = std::max<size_t>(64, 8 * (size_t)n);

  for (unsigned int i = 0; i < n; i++) {
    disassemblers.push_back(createDisassembler(puzzle, checkRotations, solverType));
    if (disassemblers.back())
      disassemblers.back()->setHelperPool(helperPool.get());
  }

#ifndef NO_THREADING
  disasmWorkerStop.store(false, std::memory_order_relaxed);
  disasmWorkers.reserve(n);
  for (unsigned int i = 0; i < n; i++) {
    disassembler_c * d = disassemblers[i].get();
    disasmWorkers.emplace_back([this, d]() { this->disasmWorkerRun(d); });
  }
#endif
}

void solveThread_c::stopDisasmWorker(void) {
  cancelDisassemblyWork();
}

void solveThread_c::cancelDisassemblyWork(void) {

  if (!(parameters & PAR_DISASSM))
    return;

  /* Pause calls this from the GUI thread while the solver thread may call it
   * too as it winds down; only one may join the workers. */
  std::lock_guard<std::mutex> cancelLock(cancelMutex);

  {
    /* Set with the queue's mutex held. A worker looks at this flag under
     * that mutex and then goes to sleep; were the flag set and the workers
     * woken in between, without the mutex, the worker would sleep on and
     * never see it, and the join below would wait for ever (it did, about
     * once in a hundred solves of a small puzzle). The same holds for
     * stopPressed, which the caller has set before coming here.
     */
    std::lock_guard<std::mutex> lock(disasmQueueMutex);
    disasmWorkerStop.store(true, std::memory_order_release);
  }

  for (unsigned int i = 0; i < disassemblers.size(); i++)
    if (disassemblers[i])
      disassemblers[i]->stop();

  disasmQueueCv.notify_all();
  disasmSpaceCv.notify_all();

#ifndef NO_THREADING
  for (unsigned int i = 0; i < disasmWorkers.size(); i++)
    if (disasmWorkers[i].joinable())
      disasmWorkers[i].join();
  disasmWorkers.clear();
#endif

  /* Queued assemblies are already counted; keep them for the next run. */
  std::lock_guard<std::mutex> lock(disasmQueueMutex);
  while (!disasmQueue.empty()) {
    disasmTask_c & task = disasmQueue.front();
    puzzle.addPending(std::move(task.assembly), true, task.assemblyNumber, task.solutionNumber);
    disasmQueue.pop();
  }
  disasmPending.store(0, std::memory_order_relaxed);
}

void solveThread_c::disasmWorkerRun(disassembler_c * workerDisassm) {

  const int solutionAction = solutionActionFromParameters(parameters);

  while (true) {
    disasmTask_c task;

    {
      std::unique_lock<std::mutex> lock(disasmQueueMutex);
      disasmQueueCv.wait(lock, [this]() {
        return !disasmQueue.empty() || disasmWorkerStop.load(std::memory_order_acquire);
      });

      if (disasmQueue.empty()) {
        if (disasmWorkerStop.load(std::memory_order_acquire))
          break;
        continue;
      }

      task = std::move(disasmQueue.front());
      disasmQueue.pop();
    }
    disasmSpaceCv.notify_one();

    if (disasmWorkerStop.load(std::memory_order_acquire)) {
      puzzle.addPending(std::move(task.assembly), true, task.assemblyNumber, task.solutionNumber);
      disassemblyDone();
      continue;
    }

    /* this thread uses a core now: fewer to lend to the others' levels */
    helperPool->addLoad(1);
    try {
      processDisassembly(task, solutionAction, workerDisassm);
    } catch (...) {
      helperPool->addLoad(-1);
      throw;
    }
    helperPool->addLoad(-1);

    disassemblyDone();
  }
}

/* One assembly less to take apart; wakes flushDisassemblyQueue() when it was
 * the last. The count is not guarded by the queue's mutex, so the mutex is
 * taken for a moment before waking: without that the waiter could look at
 * the count, be overtaken here, and then sleep through the wake-up.
 */
void solveThread_c::disassemblyDone(void) {
  if (disasmPending.fetch_sub(1, std::memory_order_acq_rel) == 1) {
    { std::lock_guard<std::mutex> lock(disasmQueueMutex); }
    disasmQueueCv.notify_all();
  }
}

void solveThread_c::enqueueDisassembly(std::unique_ptr<assembly_c> a) {
  enqueueDisassembly(std::move(a), puzzle.getNumAssemblies(), puzzle.getNumSolutions());
}

void solveThread_c::enqueueDisassembly(std::unique_ptr<assembly_c> a, unsigned long assemblyNumber,
                                       unsigned long solutionNumber) {

  disasmTask_c task;
  task.assembly = std::move(a);
  task.assemblyNumber = assemblyNumber;
  task.solutionNumber = solutionNumber;

  disasmPending.fetch_add(1, std::memory_order_relaxed);
  {
    unsigned int p = disasmPending.load(std::memory_order_relaxed);
    unsigned int peak = disasmPeakPending.load(std::memory_order_relaxed);
    while (p > peak && !disasmPeakPending.compare_exchange_weak(peak, p, std::memory_order_relaxed))
      ;
  }
  {
    /* When assemblies come faster than they are taken apart, the thread
     * that found this one waits here until there is room. That keeps the
     * queue (and the memory it takes) small, and leaves the cores to the
     * take-aparts. A stop ends the wait: what is queued is kept. */
    std::unique_lock<std::mutex> lock(disasmQueueMutex);
#ifndef NO_THREADING
    auto room = [this]() {
      return disasmQueue.size() < disasmQueueLimit ||
             stopPressed.load(std::memory_order_acquire) ||
             disasmWorkerStop.load(std::memory_order_acquire);
    };
    if (!room()) {
      /* The assembly search stands still while this waits: its other
       * threads end up waiting behind this one as soon as they find an
       * assembly too. So for as long, its cores do not count as in use,
       * and the take-aparts may borrow them for their levels. Without
       * this a search with few take-apart workers (a low thread limit)
       * left most of its cores idle whenever the queue was full.
       */
      const int given = helperPool ? assemblyLoad.exchange(0, std::memory_order_acq_rel) : 0;
      if (given)
        helperPool->addLoad(-given);
      disasmSpaceCv.wait(lock, room);
      if (given) {
        helperPool->addLoad(given);
        assemblyLoad.fetch_add(given, std::memory_order_acq_rel);
      }
    }
#endif
    disasmQueue.push(std::move(task));
  }
  disasmQueueCv.notify_one();
}

void solveThread_c::flushDisassemblyQueue(void) {

  if (!(parameters & PAR_DISASSM))
    return;

#ifdef NO_THREADING
  return;
#else
  std::unique_lock<std::mutex> lock(disasmQueueMutex);
  disasmQueueCv.wait(lock, [this]() {
    return stopPressed.load(std::memory_order_acquire) ||
           disasmWorkerStop.load(std::memory_order_acquire) ||
           (disasmQueue.empty() &&
            disasmPending.load(std::memory_order_acquire) == 0);
  });
#endif
}

unsigned int solveThread_c::findInsertIndexByMoves(unsigned int lev) const {

  unsigned int lo = 0;
  unsigned int hi = puzzle.getNumberOfSavedSolutions();

  while (lo < hi) {
    unsigned int mid = lo + (hi - lo) / 2;
    const disassembly_c * s2 = puzzle.getSavedSolution(mid)->getDisassemblyInfo();

    if (s2 && s2->sumMoves() < lev)
      hi = mid;
    else
      lo = mid + 1;
  }

  return lo;
}

unsigned int solveThread_c::findInsertIndexByRotations(unsigned int lev) const {

  unsigned int lo = 0;
  unsigned int hi = puzzle.getNumberOfSavedSolutions();

  while (lo < hi) {
    unsigned int mid = lo + (hi - lo) / 2;
    const disassembly_c * s2 = puzzle.getSavedSolution(mid)->getDisassemblyInfo();

    if (s2 && s2->sumRotations() < lev)
      hi = mid;
    else
      lo = mid + 1;
  }

  return lo;
}

void solveThread_c::processDisassembly(disasmTask_c & task, int _solutionAction, disassembler_c * workerDisassm) {

  disasmDurationGuard_c duration(&disasmCompleted, &disasmMsTotal);

  std::unique_ptr<assembly_c> a = std::move(task.assembly);

  if (a->placementCount() <= 1) {
    if (_solutionAction == SOL_DISASM)
      puzzle.addSolution(a.release(), task.assemblyNumber);
    puzzle.incNumSolutions();
    return;
  }

  std::unique_ptr<separation_c> s = workerDisassm->disassemble(a.get());

  if (!s) {
    /* Stopped part way is not the same as cannot come apart: do it again
     * on the next run. */
    if (disasmWorkerStop.load(std::memory_order_acquire) ||
        stopPressed.load(std::memory_order_acquire)) {
      puzzle.addPending(std::move(a), true, task.assemblyNumber, task.solutionNumber);
      return;
    }
    disasmInseparable.fetch_add(1, std::memory_order_relaxed);
    return;
  }

  if (_solutionAction != SOL_DISASM) {
    puzzle.incNumSolutions();
    return;
  }

  {
    problem_c::SolutionsLock solutionsLock(puzzle);

    /* Save the solution at pos (the end by default), with its take-apart or
     * only the summary of it. */
    auto add = [&](unsigned int pos = 0xFFFFFFFF) {
      if (parameters & PAR_DROP_DISASSEMBLIES)
        puzzle.addSolution(a.release(), new separationInfo_c(s.get()), task.assemblyNumber, task.solutionNumber, pos);
      else
        puzzle.addSolution(a.release(), s.release(), task.assemblyNumber, task.solutionNumber, pos);
    };
    /* The sorted modes keep the best solutionLimit: drop the last. */
    auto trim = [&]() {
      if (solutionLimit && (puzzle.getNumberOfSavedSolutions() > solutionLimit))
        puzzle.removeSolution(puzzle.getNumberOfSavedSolutions()-1);
    };

    switch(sortMethod) {
      case SRT_COMPLETE_MOVES:
        add(findInsertIndexByMoves(s->sumMoves()));
        trim();
        break;
      case SRT_ROTATIONS:
        add(findInsertIndexByRotations(s->sumRotations()));
        trim();
        break;
      case SRT_LEVEL:
        {
          unsigned int pos = 0xFFFFFFFF;
          for (unsigned int i = 0; i < puzzle.getNumberOfSavedSolutions(); i++) {
            const disassembly_c * s2 = puzzle.getSavedSolution(i)->getDisassemblyInfo();
            if (s2 && (s2->compare(s.get()) < 0)) {
              pos = i;
              break;
            }
          }
          add(pos);
          trim();
        }
        break;
      case SRT_UNSORT:
        if (task.solutionNumber % (solutionDrop * dropMultiplicator) == 0)
          add();
        break;
    }
  }

  applyLiveSort();

  puzzle.incNumSolutions();
}

void solveThread_c::applyLiveSort(void) {

  /* keep the list sorted by the method the user picked in the GUI, if any, so
   * the sort stays applied as new solutions arrive. Only the new solutions
   * are placed, so this stays cheap however long the list grows. It locks
   * the list itself.
   */
  int ls = liveSort.load(std::memory_order_relaxed);
  if (ls >= 0 && puzzle.getNumberOfSavedSolutions() >= 2)
    puzzle.keepSolutionsSorted(ls);
}

void solveThread_c::trimSavedSolutions(int _solutionAction) {

  if (!solutionLimit)
    return;

  problem_c::SolutionsLock solutionsLock(puzzle);

  if (puzzle.getNumberOfSavedSolutions() > solutionLimit) {
    unsigned int idx = (_solutionAction == SOL_SAVE_ASM) ? puzzle.getNumAssemblies()-1
                                                         : puzzle.getNumSolutions()-1;

    idx = (idx % (solutionLimit * solutionDrop * dropMultiplicator)) / (solutionDrop * dropMultiplicator);

    if (idx == solutionLimit-1)
      dropMultiplicator.store(dropMultiplicator.load(std::memory_order_relaxed) * 2, std::memory_order_relaxed);

    puzzle.removeSolution(idx+1);
  }
}

bool solveThread_c::assembly(std::unique_ptr<assembly_c> a) {

  std::lock_guard<std::mutex> lock(assemblyCallbackMutex);

  /* Reported after Pause: the assembler will not report it again, so keep
   * it for the next run rather than lose it. */
  if (stopPressed.load(std::memory_order_acquire)) {
    puzzle.addPending(std::move(a), false);
    return true;
  }

  const int _solutionAction = solutionActionFromParameters(parameters);

  /* Sliding trays: Disassemble keeps a start only when it can slide to the goal. */
  if (sliding::isSliding(puzzle)) {
    if (!(parameters & PAR_JUST_COUNT)) {
      if (parameters & PAR_DISASSM) {
        sliding::slideSearch_c search;
        search.maxStates = (parameters & PAR_FULL_SEARCH) ? sliding::FULL_SEARCH
                         : (parameters & PAR_DEEP_SEARCH) ? sliding::DEEP_SEARCH_STATES
                         : sliding::SEARCH_STATES;
        search.highMemory = (parameters & PAR_HIGH_MEMORY) != 0;
        search.nested = (parameters & PAR_NESTED_SLIDES) != 0;
        search.partialNested = (parameters & PAR_PARTIAL_NESTED) != 0;
        search.stop = &stopPressed;
        search.progress = &slideProgress;
        search.depthProgress = &searchDepth;
        std::unique_ptr<separation_c> path = sliding::findSlidePath(puzzle, *a, search);
        searchDepth.store(0, std::memory_order_relaxed);
        /* Paused in the middle of this start's search: search it again on
         * the next run instead of counting it as one with no solution. */
        if (search.outcome == sliding::SLIDE_STOPPED) {
          puzzle.addPending(std::move(a), false);
          return true;
        }
        if (search.outcome == sliding::SLIDE_LIMIT || search.outcome == sliding::SLIDE_MEMORY ||
            search.outcome == sliding::SLIDE_DISK) {
          slideStartsCut++;
          if (search.outcome == sliding::SLIDE_DISK)
            slideDiskError = search.error;
          if (search.outcome == sliding::SLIDE_MEMORY) {
            slideMemoryCut = true;
            slideMemoryStates = search.memoryStates;
          }
        }
        if (path) {
          /* Several starts can slide to the same finished picture. A search
           * never walks in a circle, but a worse start takes more moves to
           * reach that picture. Keep the shortest path to each ending. */
          const std::string key = sliding::finalPlacementKey(*path);
          const unsigned int moves = path->getMoves();
          int longer = -1;
          bool keep = true;
          for (unsigned int i = 0; i < puzzle.getNumberOfSavedSolutions(); i++) {
            const separation_c * old = puzzle.getSavedSolution(i)->getDisassembly();
            if (!old || sliding::finalPlacementKey(*old) != key)
              continue;
            if (old->getMoves() <= moves)
              keep = false;
            else
              longer = (int)i;
            break;
          }
          if (keep) {
            /* A shorter path to an ending already counted is the same solution.
             * Reuse that solution's number. getNumSolutions() is already the
             * count of distinct endings, so using it here shows Solution: N+1
             * while Solutions stays at N. */
            unsigned long solNum = puzzle.getNumSolutions();
            if (longer >= 0) {
              solNum = puzzle.getSavedSolution((unsigned int)longer)->getSolutionNumber();
              puzzle.removeSolution((unsigned int)longer);
            }
            puzzle.addSolution(a.release(), path.release(),
                               puzzle.getNumAssemblies(), solNum);
            if (longer < 0)
              puzzle.incNumSolutions();
          }
        }
      } else {
        puzzle.addSolution(a.release());
      }
    }
    puzzle.incNumAssemblies();
    trimSavedSolutions(SOL_DISASM);
    applyLiveSort();
    return true;
  }

  switch(_solutionAction) {
  case SOL_COUNT_ASM:
    break;
  case SOL_SAVE_ASM:

    if (puzzle.getNumAssemblies() % (solutionDrop*dropMultiplicator) == 0)
      puzzle.addSolution(a.release());

    break;

  case SOL_DISASM:
  case SOL_COUNT_DISASM:
    {
      if (a->placementCount() <= 1) {
        if (_solutionAction == SOL_DISASM)
          puzzle.addSolution(a.release());
        puzzle.incNumSolutions();
        break;
      }

#ifdef NO_THREADING
      disasmTask_c task;
      task.assembly = std::move(a);
      task.assemblyNumber = puzzle.getNumAssemblies();
      task.solutionNumber = puzzle.getNumSolutions();
      processDisassembly(task, _solutionAction, disassemblers[0].get());
#else
      enqueueDisassembly(std::move(a));
#endif
    }
    break;
  }

  puzzle.incNumAssemblies();
  trimSavedSolutions(_solutionAction);
  applyLiveSort();

  return true;
}

void solveThread_c::stopInternal(void) {

  unsigned int act = action.load(std::memory_order_relaxed);

  if ((act != ACT_ASSEMBLING) &&
      (act != ACT_REDUCE) &&
      (act != ACT_DISASSEMBLING) &&
      (act != ACT_PREPARATION)
     )
    return;

  stopPressed.store(true, std::memory_order_release);
  action.store(ACT_WAIT_TO_STOP, std::memory_order_relaxed);

  /* The assembler as published by the worker: the GUI thread must not read
   * the problem's, which the worker may be replacing. */
  withAssembler([](assembler_c * running) { running->stop(); });

  cancelDisassemblyWork();
}

void solveThread_c::stop(void) {
  stopInternal();
}

void solveThread_c::stopSoft(void) {
  /* Only the assembler stops: a slide search or a disassembly under way
   * finishes, the queue drains, and the run pauses where it can be saved. */
  const unsigned int act = action.load(std::memory_order_relaxed);
  if (act != ACT_ASSEMBLING && act != ACT_DISASSEMBLING)
    return;
  /* The assembler as published by the worker: the GUI thread must not read
   * the problem's, which the worker may be replacing. */
  withAssembler([](assembler_c * running) { running->stop(); });
}

bool solveThread_c::start(bool stop_after_prep) {

  stopPressed.store(false, std::memory_order_relaxed);
  return_after_prep = stop_after_prep;
  statsOrigin = std::chrono::steady_clock::now();

  dropMultiplicator = 1;

  unsigned int a;

  if ((parameters & (PAR_JUST_COUNT | PAR_DISASSM)) == 0) {

    if (!puzzle.numAssembliesKnown())
      a = 0;
    else
      a = puzzle.getNumAssemblies();
  } else {
    if (!puzzle.numSolutionsKnown())
      a = 0;
    else
      a = puzzle.getNumSolutions();
  }

  /* With no limit every solution is kept: nothing to thin out. */
  while (solutionLimit && a+solutionDrop > 2 * solutionLimit * solutionDrop) {
    dropMultiplicator.store(dropMultiplicator.load(std::memory_order_relaxed) * 2, std::memory_order_relaxed);
    a = (a+1) / 2;
  }

  /* Sliding and stacking record their own paths and do not use the brick disassembler. */
  if ((parameters & PAR_DISASSM) && !sliding::isSliding(puzzle) && !stacking::isStacking(puzzle))
    startDisasmWorker();

  return thread_c::start();
}

void solveThread_c::publishAssembler(assembler_c * a) {
  std::lock_guard<std::mutex> lock(assmMutex);
  if (!a)
    if (assembler_c * old = assm.load(std::memory_order_relaxed))
      lastFinished.store(old->getFinished(), std::memory_order_relaxed);
  assm.store(a, std::memory_order_release);
}

void solveThread_c::beginPhase(phase_e ph) {
  phaseOrigin.store(std::chrono::steady_clock::now().time_since_epoch().count(), std::memory_order_relaxed);
  statsPhase.store(ph, std::memory_order_release);
}

void solveThread_c::endPhase(std::atomic<unsigned long long> & total) {
  const std::chrono::steady_clock::time_point t0{
      std::chrono::steady_clock::duration(phaseOrigin.load(std::memory_order_relaxed))};
  total.store(elapsedMs(t0), std::memory_order_relaxed);
  statsPhase.store(PHASE_NONE, std::memory_order_release);
}

bool solveThread_c::searchReadable(void) const {
  /* While the assembler is being prepared and reduced it is building the
   * very tables its progress is read from. */
  const unsigned int act = action.load(std::memory_order_relaxed);
  return act != ACT_PREPARATION && act != ACT_REDUCE;
}

float solveThread_c::assemblerFinished(void) const {
  float f = lastFinished.load(std::memory_order_relaxed);
  if (searchReadable())
    withAssembler([&f](assembler_c * a) { f = a->getFinished(); });
  return f;
}

unsigned int solveThread_c::currentActionParameter(void) {

  const unsigned int act = action.load(std::memory_order_relaxed);
  if (act != ACT_REDUCE && act != ACT_PREPARATION)
    return 0;
  unsigned int piece = 0;
  withAssembler([&piece](assembler_c * a) { piece = a->getReducePiece(); });
  return piece;
}

std::string solveProgress_c::activity(void) const {

  char tmp[200];

  /* the longest running take-apart: the level it is on and how far that is */
  std::string row;
  if (!running.empty()) {
    const disassemblyProgress_c & d = running[0];
    snprintf(tmp, sizeof(tmp), ", level %u: %lu/%lu", d.level, d.levelDone, d.levelSize);
    row = tmp;
    if (d.pieces > 2) {
      snprintf(tmp, sizeof(tmp), ", split %u/%u", d.separations, d.pieces - 1);
      row += tmp;
    }
    if (d.threads > 1) {
      snprintf(tmp, sizeof(tmp), ", %u threads", d.threads);
      row += tmp;
    }
  }

  switch (stage) {
    case STAGE_PREPARE:
      snprintf(tmp, sizeof(tmp), "prepare piece %u of %u", piece, pieces);
      return tmp;
    case STAGE_REDUCE:
      snprintf(tmp, sizeof(tmp), "optimize piece %u of %u", piece, pieces);
      return tmp;
    case STAGE_ASSEMBLE:
      if (!disassembly)
        return "assemble";
      snprintf(tmp, sizeof(tmp), "assemble, disassemble %u done %u waiting",
               disasmCompleted, disasmPending);
      return tmp + row;
    case STAGE_DISASSEMBLE:
      snprintf(tmp, sizeof(tmp), "disassemble %u of %u",
               disasmCompleted + (disasmPending ? 1 : 0), disasmCompleted + disasmPending);
      return tmp + row;
    case STAGE_STOPPING:
      return "please wait";
    case STAGE_PAUSED:
      return "pause";
    case STAGE_DONE:
      return "finished";
    case STAGE_ERROR:
      return "error";
    default:
      return "";
  }
}

solveProgress_c solveThread_c::getProgressSnapshot(void) const {
  return progressSnapshot(0, false);
}

float solveThread_c::getProgress(float assemblyFraction) const {
  return progressSnapshot(assemblyFraction, true).overall;
}

solveProgress_c solveThread_c::progressSnapshot(float assemblyFraction, bool haveFraction) const {

  solveProgress_c p;

  p.elapsedMs = getTimeMs();
  p.disassembly = (parameters & PAR_DISASSM) != 0 && !disassemblers.empty();
  p.assemblies = puzzle.numAssembliesKnown() ? puzzle.getNumAssemblies() : 0;
  p.solutions = puzzle.numSolutionsKnown() ? puzzle.getNumSolutions() : 0;
  p.disasmCompleted = disasmCompleted.load(std::memory_order_relaxed);
  p.disasmPending = disasmPending.load(std::memory_order_relaxed);
  p.disasmWorkers = disasmWorkerCount.load(std::memory_order_relaxed);
  p.assemblerThreads = assemblerThreadCount.load(std::memory_order_relaxed);

  float af = lastFinished.load(std::memory_order_relaxed);
  unsigned int piece = 0;
  unsigned long iterations = 0;
  const bool readable = searchReadable();
  withAssembler([&](assembler_c * a) {
    if (readable) {
      af = a->getFinished();
      iterations = a->getIterations();
    }
    piece = a->getReducePiece();
  });
  if (haveFraction)
    af = assemblyFraction;
  if (af < 0) af = 0;
  if (af > 1) af = 1;
  p.assemblyFraction = af;
  p.iterations = iterations;

  const unsigned int act = action.load(std::memory_order_relaxed);
  const bool ownReport = stacking::isStacking(puzzle) ||
                         (sliding::isSliding(puzzle) && (parameters & PAR_DISASSM));

  switch (act) {
    case ACT_PREPARATION:
      p.stage = solveProgress_c::STAGE_PREPARE;
      p.pieces = puzzle.getNumberOfParts();
      p.piece = piece + 1 > p.pieces ? p.pieces : piece + 1;
      break;
    case ACT_REDUCE:
      p.stage = solveProgress_c::STAGE_REDUCE;
      p.pieces = puzzle.getNumberOfPieces();
      p.piece = piece + 1 > p.pieces ? p.pieces : piece + 1;
      break;
    case ACT_ASSEMBLING:
      p.stage = ownReport ? solveProgress_c::STAGE_OTHER : solveProgress_c::STAGE_ASSEMBLE;
      break;
    case ACT_DISASSEMBLING:
      p.stage = solveProgress_c::STAGE_DISASSEMBLE;
      break;
    case ACT_WAIT_TO_STOP:
      p.stage = solveProgress_c::STAGE_STOPPING;
      break;
    case ACT_PAUSING:
      p.stage = solveProgress_c::STAGE_PAUSED;
      break;
    case ACT_FINISHED:
      p.stage = solveProgress_c::STAGE_DONE;
      break;
    default:
      p.stage = solveProgress_c::STAGE_ERROR;
      break;
  }

  /* the take-aparts under way, the longest running first */
  for (unsigned int i = 0; i < disassemblers.size(); i++) {
    disassemblyProgress_c d;
    if (disassemblers[i] && disassemblers[i]->getProgress(d) && d.active)
      p.running.push_back(d);
  }
  std::sort(p.running.begin(), p.running.end(),
            [](const disassemblyProgress_c & a, const disassemblyProgress_c & b) {
              return a.elapsedMs > b.elapsedMs;
            });
  if (!p.running.empty() && p.running[0].levelSize > 0)
    p.levelFraction = (float)p.running[0].levelDone / (float)p.running[0].levelSize;

  /* how long the assembly search has run, for what it has left */
  const phase_e phase = statsPhase.load(std::memory_order_acquire);
  double asmSeconds = (double)assemblyMs.load(std::memory_order_relaxed) / 1000.0;
  if (phase == PHASE_ASSEMBLE)
    asmSeconds += (double)elapsedMs(std::chrono::steady_clock::time_point{
        std::chrono::steady_clock::duration(phaseOrigin.load(std::memory_order_relaxed))}) / 1000.0;

  /* the fraction covers what earlier runs of a continued solve did, too */
  if (puzzle.usedTimeKnown())
    asmSeconds += (double)puzzle.getUsedMs() / 1000.0;

  const bool searching = p.stage == solveProgress_c::STAGE_ASSEMBLE ||
                         p.stage == solveProgress_c::STAGE_DISASSEMBLE;

  double asmLeft = -1;
  if (af >= 0.999f)
    asmLeft = 0;
  else if (af > 0.001f)
    asmLeft = asmSeconds * (1.0 - af) / af;

  if (p.stage == solveProgress_c::STAGE_DONE) {

    p.overall = 1;
    p.secondsLeft = 0;

  } else if (!p.disassembly) {

    p.overall = af;
    if (searching)
      p.secondsLeft = asmLeft;

  } else {

    /* assemblies the search has still to find, at the rate so far */
    double future = 0;
    if (af > 0.0001f && af < 0.999f && p.assemblies > 0)
      future = (double)p.assemblies * (1.0 - af) / af;

    /* A take-apart under way counts by the separations it has found of
     * the ones it needs: the one thing known about how far it is. */
    double partDone = 0, runningSeconds = 0;
    for (const disassemblyProgress_c & d : p.running) {
      if (d.pieces > 1)
        partDone += (double)d.separations / (double)(d.pieces - 1);
      runningSeconds += (double)d.elapsedMs / 1000.0;
    }
    if (partDone > (double)p.disasmPending)
      partDone = (double)p.disasmPending;

    const double done = (double)p.disasmCompleted + partDone;
    const double left = (double)p.disasmPending - partDone + future;
    const double total = done + left;

    double disasmFrac;
    if (total < 1.0)
      /* none seen: with the search through, there is nothing to take apart */
      disasmFrac = af >= 0.999f ? 1.0 : 0.0;
    else
      disasmFrac = done / total;

    /* how long a take-apart takes is only known once one is through */
    const double avg = getAverageDisassemblySeconds();

    /* The two sides count by what they cost: the whole assembly search
     * at the rate so far (thread-seconds) against all the take-aparts at
     * the average of the ones done. The bar then moves with the time
     * spent of the time the solve will take, whichever side is the slow
     * one (the idea is upstream's progressModel_c, Tom Burns). Before
     * either can be measured: half and half, or with rotations, where
     * taking apart is much the slower side, one to four.
     */
    double asmWeight = (parameters & PAR_CHECK_ROTATIONS) ? 0.2 : 0.5;
    if (af > 0.02f && asmSeconds > 0 && avg > 0 && p.disasmCompleted > 0 && total >= 1.0) {
      const double threads = p.assemblerThreads > 0 ? (double)p.assemblerThreads : 1.0;
      const double asmCost = asmSeconds * threads / (double)af;
      const double disCost = avg * total;
      asmWeight = asmCost / (asmCost + disCost);
    }
    p.overall = (float)(asmWeight * af + (1.0 - asmWeight) * disasmFrac);
    double disLeft = -1;
    if (p.disasmPending == 0 && future < 0.5)
      disLeft = 0;
    else if (p.disasmCompleted > 0) {
      const double count = (double)p.disasmPending + future;
      double work = avg * count - (runningSeconds < avg * (double)p.running.size()
                                     ? runningSeconds : avg * (double)p.running.size());
      if (work < 0)
        work = 0;
      double workers = (double)p.disasmWorkers;
      if (workers > count) workers = count;
      if (workers < 1) workers = 1;
      disLeft = work / workers;
    }

    p.overallKnown = disLeft >= 0;

    if (searching && asmLeft >= 0 && disLeft >= 0)
      p.secondsLeft = asmLeft > disLeft ? asmLeft : disLeft;
  }

  if (p.stage != solveProgress_c::STAGE_DONE) {
    if (p.overall > 0.999f)
      p.overall = 0.999f;
    if (p.overall < 0)
      p.overall = 0;
    /* an estimate that is corrected downwards does not take the bar back */
    if (p.overall < progressShown)
      p.overall = progressShown;
  }
  progressShown = p.overall;

  return p;
}

solveStats_c solveThread_c::getStats(void) const {

  solveStats_c s = {};
  s.hasData = true;
  s.solverType = solverType;
  s.disassemblyEnabled = (parameters & PAR_DISASSM) != 0;
  s.rotationsEnabled = (parameters & PAR_CHECK_ROTATIONS) != 0;
  s.hardwareThreads = std::thread::hardware_concurrency();
  if (s.hardwareThreads < 1)
    s.hardwareThreads = 1;
  s.assemblerThreads = assemblerThreadCount.load(std::memory_order_relaxed);
  if (s.assemblerThreads < 1)
    s.assemblerThreads = 1;
  s.disasmWorkers = disasmWorkerCount.load(std::memory_order_relaxed);
  s.assembliesFound = puzzle.numAssembliesKnown() ? puzzle.getNumAssemblies() : 0;
  s.solutionsFound = puzzle.numSolutionsKnown() ? puzzle.getNumSolutions() : 0;
  s.inseparable = disasmInseparable.load(std::memory_order_relaxed);
  s.pending = disasmPending.load(std::memory_order_relaxed);
  s.peakPending = disasmPeakPending.load(std::memory_order_relaxed);
  s.disasmCompleted = disasmCompleted.load(std::memory_order_relaxed);
  s.disasmWorkMs = disasmMsTotal.load(std::memory_order_relaxed);
  s.avgDisasmSeconds = getAverageDisassemblySeconds();

  if (searchReadable())
    withAssembler([&s](assembler_c * a) {
      s.dlxIterations = a->getIterations();
      s.assemblyProgress = a->getFinished();
    });

  const phase_e phase = statsPhase.load(std::memory_order_acquire);
  unsigned long long extra = 0;
  if (phase != PHASE_NONE)
    extra = elapsedMs(std::chrono::steady_clock::time_point{
        std::chrono::steady_clock::duration(phaseOrigin.load(std::memory_order_relaxed))});

  s.prepareMs = prepareMs.load(std::memory_order_relaxed);
  s.reduceMs = reduceMs.load(std::memory_order_relaxed);
  s.assemblyMs = assemblyMs.load(std::memory_order_relaxed);
  s.drainMs = drainMs.load(std::memory_order_relaxed);
  if (phase == PHASE_PREPARE) s.prepareMs += extra;
  else if (phase == PHASE_REDUCE) s.reduceMs += extra;
  else if (phase == PHASE_ASSEMBLE) s.assemblyMs += extra;
  else if (phase == PHASE_DRAIN) s.drainMs += extra;

  s.elapsedMs = elapsedMs(statsOrigin);

  unsigned long long rotUs = 0;
  unsigned long long linUs = 0;
  for (unsigned int i = 0; i < disassemblers.size(); i++)
    if (disassemblers[i]) {
      rotUs += disassemblers[i]->getRotationSearchUs();
      linUs += disassemblers[i]->getLinearSearchUs();
    }
  s.rotationSearchMs = rotUs / 1000;
  s.linearSearchMs = linUs / 1000;

  for (unsigned int i = 0; i < disassemblers.size(); i++) {
    disassemblyProgress_c d;
    if (disassemblers[i] && disassemblers[i]->getProgress(d) && d.active)
      s.running.push_back(d);
  }

  unsigned int act = action.load(std::memory_order_relaxed);
  switch (act) {
    case ACT_FINISHED:
      s.status = solveStats_c::ST_FINISHED;
      break;
    case ACT_PAUSING:
      s.status = solveStats_c::ST_PAUSED;
      break;
    case ACT_ERROR:
    case ACT_ASSERT:
      s.status = solveStats_c::ST_ERROR;
      break;
    default:
      s.status = solveStats_c::ST_RUNNING;
      break;
  }

  return s;
}
