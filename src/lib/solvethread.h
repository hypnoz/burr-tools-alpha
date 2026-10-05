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
#ifndef __SOLVETHREAD_H__
#define __SOLVETHREAD_H__

#include "assembler.h"
#include "disassembler.h"
#include "helperpool.h"
#include "solveprogress.h"
#include "bt_assert.h"
#include "thread.h"
#include "solvertype.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <vector>

class problem_c;
class assembly_c;
class separation_c;

struct disasmTask_c {
  std::unique_ptr<assembly_c> assembly;
  unsigned long assemblyNumber = 0;
  unsigned long solutionNumber = 0;
};

/** Snapshot of solver timing and counts for the Debug statistics pane. */
struct solveStats_c {

  enum Status {
    ST_IDLE,
    ST_RUNNING,
    ST_PAUSED,
    ST_FINISHED,
    ST_ERROR
  };

  bool hasData = false;
  Status status = ST_IDLE;
  solverType_e solverType = SOLVER_CLASSIC;
  bool disassemblyEnabled = false;
  bool rotationsEnabled = false;
  unsigned int hardwareThreads = 0;
  unsigned int assemblerThreads = 0;
  unsigned int disasmWorkers = 0;
  unsigned long assembliesFound = 0;
  unsigned long solutionsFound = 0;
  unsigned long inseparable = 0;
  unsigned long dlxIterations = 0;
  float assemblyProgress = 0;
  unsigned int pending = 0;
  unsigned int peakPending = 0;
  unsigned int disasmCompleted = 0;
  unsigned long long elapsedMs = 0;
  unsigned long long prepareMs = 0;
  unsigned long long reduceMs = 0;
  unsigned long long assemblyMs = 0;
  unsigned long long disasmWorkMs = 0;
  unsigned long long linearSearchMs = 0;
  unsigned long long rotationSearchMs = 0;
  unsigned long long drainMs = 0;
  float avgDisasmSeconds = 0;
  /** what each take-apart under way is doing, see disassemblyProgress_c */
  std::vector<disassemblyProgress_c> running;
};

class solveThread_c : public assembler_cb, public thread_c {

  public:

    enum {
      ACT_PREPARATION,
      ACT_REDUCE,
      ACT_ASSEMBLING,
      ACT_DISASSEMBLING,
      ACT_PAUSING,
      ACT_FINISHED,
      ACT_ERROR,
      ACT_ASSERT,
      ACT_WAIT_TO_STOP
    };

  private:
    /* what is currently happening in the assembler thread */
    std::atomic<unsigned int> action;

  public:
    /* return the current activity */
    unsigned int currentAction(void) { return action.load(std::memory_order_relaxed); }

    /* some activities might have a parameter, return that */
    unsigned int currentActionParameter(void);

    /**
     * How far the running assembler is, 0..1; 0 while there is none. Use
     * this from the GUI instead of the problem's assembler, which this
     * thread may be replacing.
     */
    float assemblerFinished(void) const;

    /** block until the solver thread has finished (success, pause, or error) */
    void waitUntilFinished(void);

  private:

    assembler_c::errState errState = assembler_c::ERR_NONE;
    int errParam = 0;

  public:

    assembler_c::errState getErrorState(void) {
      bt_assert(action.load(std::memory_order_relaxed) == ACT_ERROR);
      return errState;
    }
    int getErrorParam(void) {
      bt_assert(action.load(std::memory_order_relaxed) == ACT_ERROR);
      return errParam;
    }

  public:

    /** Milliseconds since this run started. */
    unsigned long long getTimeMs(void) const {
      return (unsigned long long)std::chrono::duration_cast<std::chrono::milliseconds>(
          std::chrono::steady_clock::now() - statsOrigin).count();
    }

    bool disassemblyEnabled(void) const { return (parameters & PAR_DISASSM) != 0; }
    unsigned int getDisassemblyPending(void) const { return disasmPending.load(std::memory_order_relaxed); }
    unsigned int getDisassemblyCompleted(void) const { return disasmCompleted.load(std::memory_order_relaxed); }
    unsigned int getDisassemblyWorkerCount(void) const { return disasmWorkerCount.load(std::memory_order_relaxed); }
    float getAverageDisassemblySeconds(void) const {
      unsigned int n = disasmCompleted.load(std::memory_order_relaxed);
      if (n == 0)
        return 0;
      return (float)disasmMsTotal.load(std::memory_order_relaxed) / 1000.0f / (float)n;
    }

    /**
     * Overall solve progress in [0, 1].
     * assemblyFraction is the covering-search (DLX) fraction from the assembler.
     * When disassembly is enabled this also includes take-apart work so the
     * value does not jump to 100% while the disassembly queue is still draining.
     * It does not go back during a run. The same value as
     * getProgressSnapshot().overall; to be called from one thread only.
     */
    float getProgress(float assemblyFraction) const;

    /**
     * Where the whole solve stands: the stage, how far each stage is, what
     * every take-apart under way is doing, and an estimate of the time
     * left. To be called from one thread only (the one showing progress).
     */
    solveProgress_c getProgressSnapshot(void) const;

    solveStats_c getStats(void) const;

  private:

    problem_c & puzzle;
    int parameters;

  public:

    static const int PAR_REDUCE =             0x01;  // do a reduction after preparation
    static const int PAR_KEEP_MIRROR =        0x02;  // keep mirror solutions
    static const int PAR_KEEP_ROTATIONS =     0x04;  // keep rotated solutions
    static const int PAR_DROP_DISASSEMBLIES = 0x08;  // remove disassembly instructions after analysis
    static const int PAR_DISASSM =            0x10;  // do the disassembly analysis
    static const int PAR_JUST_COUNT =         0x20;  // just count the solutions, don't save them
    static const int PAR_COMPLETE_ROTATIONS = 0x40;  // do a thorough rotation check
    static const int PAR_CHECK_ROTATIONS =    0x80;  // try 90° piece rotations during disassembly
    static const int PAR_STRICT_COLORS =     0x100;  // piece colour must equal result colour
    static const int PAR_NESTED_SLIDES =     0x200;  // sliding: a piece may carry pieces nested inside it
    static const int PAR_DEEP_SEARCH =       0x400;  // sliding: search 1,000,000 arrangements, not 250,000
    static const int PAR_FULL_SEARCH =       0x800;  // sliding: no limit on arrangements searched
    static const int PAR_HIGH_MEMORY =      0x1000;  // sliding: hold up to half the physical memory in arrangements
    static const int PAR_PANEX_SOLVER =     0x2000;  // stacking: the Panex Solver, for Panex columns
    static const int PAR_PANEX_RESUME =     0x4000;  // Panex Solver: carry on the problem's saved search
    static const int PAR_PANEX_NO_AUTOSAVE = 0x8000; // Panex Solver: save only when paused, not every 20 minutes

    // create all the necessary data structures to start the thread later on
    solveThread_c(problem_c & puz, int par);
    const problem_c & getProblem(void) const { return puzzle; }

  private:

    int sortMethod;
    solverType_e solverType;

  public:

    enum {
      SRT_UNSORT,
      SRT_COMPLETE_MOVES,
      SRT_LEVEL,
      SRT_ROTATIONS
    };

    void setSortMethod(int sort) { sortMethod = sort; }

    void setSolverType(solverType_e type) { solverType = type; }
    solverType_e getSolverType(void) const { return solverType; }

    /* If >= 0, the worker keeps the (limit-bounded) solution list sorted by
     * this problem_c::sortSolutions method after every solution it adds, so a
     * sort chosen in the GUI stays applied as new solutions arrive. -1 = off.
     * Atomic: set from the GUI thread, read by the worker.
     */
    void setLiveSort(int method) { liveSort.store(method, std::memory_order_relaxed); }

  private:

    std::atomic<int> liveSort;

    /* don't save more than this number of solutions 0 means no limit */
    unsigned int solutionLimit;

    /* save only every x-th solution, the others are dropped */
    unsigned int solutionDrop;

    /* this is used to increase the drop with time, when the limit is reached
     * and only every 2nd valid solution is taken
     */
    std::atomic<unsigned int> dropMultiplicator{1};

  public:

    /** Keep at most limit solutions (0: no limit), and only every drop-th. */
    void setSolutionLimits(unsigned int limit, unsigned int drop = 1) {
      solutionLimit = limit;
      solutionDrop = drop ? drop : 1;
    }

  private:

    assert_exception ae;

  public:

    const assert_exception & getAssertException(void) {
      return ae;
    }

  private:

    std::atomic<bool> stopPressed{false};  // set by the GUI thread, read by the worker
    bool return_after_prep = false;  // sometimes it is useful to only prepare and return,
                             // if this flag is set, the program will return

    /* Threads the take-aparts spread a level of their search over while
     * cores are free. Declared before the disassemblers, which use it, so
     * that it goes after them. */
    std::unique_ptr<helperPool_c> helperPool;
    std::vector<std::unique_ptr<disassembler_c>> disassemblers;

    /* The worker publishes the assembler here once it is fully constructed so
     * that currentActionParameter(), assemblerFinished(), getStats() and stop(),
     * called from the GUI thread, can use it. Every use from another thread
     * holds assmMutex, and the worker takes it to publish or withdraw the
     * assembler, always before the assembler can be freed: so the GUI never
     * sees a half-built or a freed one. Use it through withAssembler.
     */
    std::atomic<assembler_c *> assm{nullptr};
    mutable std::mutex assmMutex;
    /* How far the assembler had got when it was withdrawn. */
    std::atomic<float> lastFinished{0};
    void publishAssembler(assembler_c * a);
    template <class F> void withAssembler(F f) const {
      std::lock_guard<std::mutex> lock(assmMutex);
      if (assembler_c * a = assm.load(std::memory_order_acquire))
        f(a);
    }
    void disassemblyDone(void);
    std::atomic<unsigned int> assemblerThreadCount{1};
    /* the cores the running assembly search counts for in helperPool's load */
    std::atomic<int> assemblyLoad{0};

    std::mutex assemblyCallbackMutex;

    /* asynchronous disassembly pipeline (when PAR_DISASSM is set) */
    std::vector<std::thread> disasmWorkers;
    std::mutex disasmQueueMutex;
    std::condition_variable disasmQueueCv;
    /* signalled when the queue has room again, see enqueueDisassembly */
    std::condition_variable disasmSpaceCv;
    size_t disasmQueueLimit = 64;
    std::queue<disasmTask_c> disasmQueue;
    std::atomic<bool> disasmWorkerStop;
    std::atomic<unsigned int> disasmWorkerCount;
    std::atomic<unsigned int> disasmPending;
    std::atomic<unsigned int> disasmCompleted;
    std::atomic<unsigned long long> disasmMsTotal;
    std::atomic<unsigned int> disasmPeakPending;
    std::atomic<unsigned long> disasmInseparable;
    std::atomic<unsigned long long> prepareMs;
    std::atomic<unsigned long long> reduceMs;
    std::atomic<unsigned long long> assemblyMs;
    std::atomic<unsigned long long> drainMs;
    std::chrono::steady_clock::time_point statsOrigin;
    enum phase_e { PHASE_NONE, PHASE_PREPARE, PHASE_REDUCE, PHASE_ASSEMBLE, PHASE_DRAIN };
    /* The phase under way and when it began (steady_clock ticks): written by
     * the worker, read by getStats on the GUI thread. */
    std::atomic<phase_e> statsPhase{PHASE_NONE};
    std::atomic<long long> phaseOrigin{0};
    void beginPhase(phase_e ph);
    /* End the phase under way, storing how long it took in total. */
    void endPhase(std::atomic<unsigned long long> & total);

    /* The furthest overall progress shown so far, so that it never goes
     * back; used by the one thread that asks for progress. */
    mutable float progressShown = 0;
    solveProgress_c progressSnapshot(float assemblyFraction, bool haveFraction) const;
    /** may the assembler's progress be read: not while it is being built */
    bool searchReadable(void) const;

    void startDisasmWorker(void);
    void stopDisasmWorker(void);
    void cancelDisassemblyWork(void);
    void disasmWorkerRun(disassembler_c * workerDisassm);
    void enqueueDisassembly(std::unique_ptr<assembly_c> a);
    /** Serialises cancelDisassemblyWork between the GUI and solver threads. */
    std::mutex cancelMutex;
    void enqueueDisassembly(std::unique_ptr<assembly_c> a, unsigned long assemblyNumber,
                            unsigned long solutionNumber);
    void flushDisassemblyQueue(void);
    void processDisassembly(disasmTask_c & task, int solutionAction, disassembler_c * workerDisassm);
    unsigned int findInsertIndexByMoves(unsigned int lev) const;
    unsigned int findInsertIndexByRotations(unsigned int lev) const;
    void trimSavedSolutions(int solutionAction);
    void applyLiveSort(void);

public:

  // stop and exit
  virtual ~solveThread_c(void);

private:

  // helper to stop without virtual dispatch in destructor
  void stopInternal(void);

  // the call-back
  bool assembly(std::unique_ptr<assembly_c> a) override;

public:

  // let the thread start
  // returns true, if everything went well, false otherwise
  bool start(bool stop_after_prep = false);

  // try to stop the thread at the next possible position
  void stop(void) override;

  /** Pause at the next point where the run can be saved, without abandoning a
   * slide search or a disassembly under way; for autosave. */
  void stopSoft(void);

  /* true once the worker has left run() for good. ACT_ASSERT belongs here:
   * an assert in the worker ends the thread just as surely as the other three,
   * and a caller polling for the thread to finish would otherwise wait forever.
   */
  bool stopped(void) const {
    unsigned int act = action.load(std::memory_order_relaxed);
    return ((act == ACT_PAUSING) ||
            (act == ACT_FINISHED) ||
            (act == ACT_ERROR) ||
            (act == ACT_ASSERT)
           );
  }

  void run(void) override;

  /** Empty unless the stacking or sliding search has something to report,
   * such as no path, or a search that hit its limit. Read it once the
   * solve has finished or paused. */
  std::string getSolverNote(void) const {
    std::lock_guard<std::mutex> lock(noteMutex);
    return solverNote;
  }

  /** Moves the Panex Solver has searched from both ends together. */
  unsigned long getSearchDepth(void) const {
    return searchDepth.load(std::memory_order_relaxed);
  }
  bool panexSearch(void) const { return (parameters & PAR_PANEX_SOLVER) != 0; }
  /** Moves of the path the Panex Solver has traced back, once the ends meet. */
  unsigned long getSearchTraced(void) const {
    return searchTraced.load(std::memory_order_relaxed);
  }
  /** Arrangements (sliding) or stackings (stacking) searched so far. */
  unsigned long getSlideProgress(void) const {
    return slideProgress.load(std::memory_order_relaxed);
  }

private:

  void runStacking(void);
  /* After a sliding solve: whether a missing solution is proven or not. */
  std::string slidingSummary(void) const;
  void setSolverNote(const std::string & note) {
    std::lock_guard<std::mutex> lock(noteMutex);
    solverNote = note;
  }
  /* Written by the worker, read by the GUI thread after the solve. */
  mutable std::mutex noteMutex;
  std::string solverNote;

  /* Sliding search results across all start layouts of one solve. */
  std::atomic<unsigned long> slideProgress{0};
  std::atomic<unsigned long> searchDepth{0};
  std::atomic<unsigned long> searchTraced{0};
  unsigned int slideStartsCut = 0;    // starts whose search hit a limit
  bool slideMemoryCut = false;        // one of those hit the memory ceiling
  unsigned long slideMemoryStates = 0; // that ceiling, in arrangements

private:

  solveThread_c(const solveThread_c&) = delete;
  void operator=(const solveThread_c&) = delete;
};

#endif
