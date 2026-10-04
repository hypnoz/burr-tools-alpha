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

#include <chrono>
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
  /* 90° rotation search is memory-bandwidth heavy. Extra workers contend
   * with the assembler and with each other, and on rotation puzzles that
   * made wall-clock time worse than a single worker. */
  if (rotationsEnabled)
    return 1;

  unsigned int hw = std::thread::hardware_concurrency();
  if (hw < 1)
    hw = 1;
  if (hw <= 2)
    return 1;
  unsigned int n = hw - 2;
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

      /* set the assembler to the problem as soon as it is finished
       * with initialisation, NOT EARLIER as the function
       * also restores the assembler state to a state that might
       * be saved within the problem
       */
      publishAssembler(nullptr);
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
      if (solverType == SOLVER_BT2) {
        const unsigned int workers = bt2ChooseAssemblerWorkers(a);
        assemblerThreadCount.store(workers, std::memory_order_relaxed);
        assemblerThreadCount.store(bt2Assemble(a, this, workers), std::memory_order_relaxed);
      } else {
        assemblerThreadCount.store(a->getEffectiveThreads(), std::memory_order_relaxed);
        a->assemble(this);
      }
      endPhase(assemblyMs);

      if (!stopPressed.load(std::memory_order_relaxed)) {
        beginPhase(PHASE_DRAIN);
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
    if (slideMemoryCut)
      msg += ": the search reached its memory limit of " +
             std::to_string(slideMemoryStates) + " arrangements";
    else if (parameters & PAR_DEEP_SEARCH)
      msg += ": the search reached its limit of 1,000,000 arrangements";
    else
      msg += ": the search reached its limit of 250,000 arrangements";
    msg += ". There may be solutions the solver did not find.";
    if (slideMemoryCut && !(parameters & PAR_HIGH_MEMORY))
      msg += " Try Enable High Memory.";
    if (!slideMemoryCut && !(parameters & PAR_FULL_SEARCH))
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
drainMs(0),
disasmCreepActive(false),
disasmCreepShown(0)
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

  for (unsigned int i = 0; i < n; i++)
    disassemblers.push_back(createDisassembler(puzzle, checkRotations, solverType));

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

  disasmWorkerStop.store(true, std::memory_order_release);

  for (unsigned int i = 0; i < disassemblers.size(); i++)
    if (disassemblers[i])
      disassemblers[i]->stop();

  disasmQueueCv.notify_all();

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

    if (disasmWorkerStop.load(std::memory_order_acquire)) {
      puzzle.addPending(std::move(task.assembly), true, task.assemblyNumber, task.solutionNumber);
      if (disasmPending.fetch_sub(1, std::memory_order_acq_rel) == 1)
        disasmQueueCv.notify_all();
      continue;
    }

    processDisassembly(task, solutionAction, workerDisassm);

    if (disasmPending.fetch_sub(1, std::memory_order_acq_rel) == 1)
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
    std::lock_guard<std::mutex> lock(disasmQueueMutex);
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
      dropMultiplicator *= 2;

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
        search.stop = &stopPressed;
        search.progress = &slideProgress;
        std::unique_ptr<separation_c> path = sliding::findSlidePath(puzzle, *a, search);
        /* Paused in the middle of this start's search: search it again on
         * the next run instead of counting it as one with no solution. */
        if (search.outcome == sliding::SLIDE_STOPPED) {
          puzzle.addPending(std::move(a), false);
          return true;
        }
        if (search.outcome == sliding::SLIDE_LIMIT || search.outcome == sliding::SLIDE_MEMORY) {
          slideStartsCut++;
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
    dropMultiplicator *= 2;
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

float solveThread_c::assemblerFinished(void) const {
  float f = lastFinished.load(std::memory_order_relaxed);
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

namespace {

/* Seconds between creep steps while take-apart is running.
 * Classic / BurrTools 2: 1% steps on the one-left vs many-left schedule.
 * Andrew Crowell: 5% per second (speed is unknown). Halt at 95% for all types. */
double disasmCreepInterval(int pct, bool oneLeft, solverType_e type, int *step) {
  if (pct >= 95) {
    *step = 0;
    return 1e9;
  }
  if (type == SOLVER_CROWELL) {
    *step = 5;
    return 1.0;
  }
  *step = 1;
  if (oneLeft) {
    if (pct < 80) return 1.0;
    if (pct < 90) return 2.0;
    if (pct < 95) return 5.0;
  } else {
    if (pct < 80) return 3.0;
    if (pct < 90) return 5.0;
    if (pct < 95) return 15.0;
  }
  *step = 0;
  return 1e9;
}

} // namespace

float solveThread_c::getProgress(float assemblyFraction) const {

  if (assemblyFraction < 0)
    assemblyFraction = 0;
  else if (assemblyFraction > 1)
    assemblyFraction = 1;

  if (!(parameters & PAR_DISASSM))
    return assemblyFraction;

  const unsigned int pending = disasmPending.load(std::memory_order_relaxed);
  const unsigned int completed = disasmCompleted.load(std::memory_order_relaxed);
  const unsigned long assemblies = puzzle.numAssembliesKnown() ? puzzle.getNumAssemblies() : 0;

  float futureAsm = 0;
  if (assemblyFraction > 0.0001f && assemblyFraction < 0.999f && assemblies > 0)
    futureAsm = (float)assemblies * (1.0f - assemblyFraction) / assemblyFraction;

  const float disasmDone = (float)completed;
  const float disasmLeft = (float)pending + futureAsm;
  const float disasmTotal = disasmDone + disasmLeft;

  float disasmFrac;
  if (disasmTotal < 1.0f) {
    /* No take-apart work seen yet. Covering complete with zero assemblies
     * means there is nothing to disassemble. */
    disasmFrac = (assemblyFraction >= 0.999f) ? 1.0f : 0.0f;
  } else {
    disasmFrac = disasmDone / disasmTotal;
    if (disasmFrac > 1.0f)
      disasmFrac = 1.0f;
  }

  /* When workers keep up, disasmFrac tracks assemblyFraction and any mix
   * still equals covering progress. When covering finishes first, the bar
   * continues with the queue. Rotations make take-apart much slower, so
   * weight that side more. */
  const bool rotations = (parameters & PAR_CHECK_ROTATIONS) != 0;
  const float asmWeight = rotations ? 0.2f : 0.5f;
  float progress = asmWeight * assemblyFraction + (1.0f - asmWeight) * disasmFrac;

  if ((pending > 0 || assemblyFraction < 0.999f) && progress > 0.999f)
    progress = 0.999f;

  if (progress < 0)
    progress = 0;

  /* While at least one assembly is still being taken apart, creep the bar
   * forward so it does not sit frozen. Real progress always wins if it
   * jumps ahead. Cap at 95% until the solve actually finishes. */
  if (pending == 0) {
    disasmCreepActive = false;
    return progress;
  }

  const bool oneLeft = (disasmLeft <= 1.001f);
  const auto now = std::chrono::steady_clock::now();

  if (!disasmCreepActive) {
    disasmCreepActive = true;
    disasmCreepShown = progress;
    disasmCreepTick = now;
    return progress;
  }

  if (progress > disasmCreepShown) {
    disasmCreepShown = progress;
    disasmCreepTick = now;
  }

  int pct = (int)(disasmCreepShown * 100.0f + 1e-4f);
  if (pct < 0) pct = 0;
  if (pct > 95) pct = 95;

  double elapsed = std::chrono::duration<double>(now - disasmCreepTick).count();
  while (pct < 95) {
    int step = 1;
    const double iv = disasmCreepInterval(pct, oneLeft, solverType, &step);
    if (step <= 0 || elapsed + 1e-9 < iv)
      break;
    elapsed -= iv;
    pct += step;
    if (pct > 95)
      pct = 95;
  }

  disasmCreepTick = now - std::chrono::duration_cast<std::chrono::steady_clock::duration>(
      std::chrono::duration<double>(elapsed));
  disasmCreepShown = (float)pct / 100.0f;

  if (disasmCreepShown < progress)
    disasmCreepShown = progress;

  return disasmCreepShown;
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
