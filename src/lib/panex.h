/* BurrTools
 *
 * The Panex Solver: fewest rod transfers for stacking puzzles with Panex
 * columns, searched from both ends so that large towers can be solved.
 */
#ifndef __PANEX_H__
#define __PANEX_H__

#include <atomic>
#include <memory>
#include <string>

class problem_c;
class separation_c;

namespace panex {

/**
 * Empty when the Panex Solver can take this problem, otherwise why it
 * cannot. With anyRules, rod sets without Panex columns are taken too: the
 * same search then serves the Stacking Solver.
 */
std::string unsupported(const problem_c & prob, bool anyRules = false);

/**
 * A saved search of this problem that Continue can carry on, described for
 * the user ("8,990 + 8,990 moves deep, 351.5B stackings"); empty when there
 * is none. workDir as in panexSearch_c.
 */
std::string savedSearch(const problem_c & prob, const std::string & workDir = "");

/** Seconds of solving the saved search of this problem holds; 0 when there is none. */
unsigned long long savedSeconds(const problem_c & prob, const std::string & workDir = "");

/**
 * The folder where this problem's saved search lives, whether there is one
 * or not; empty when it has no place. For export and import.
 */
std::string searchFolder(const problem_c & prob, const std::string & workDir = "");

/** Delete the saved search of this problem, if there is one. */
void discardSaved(const problem_c & prob, const std::string & workDir = "");

/** How a Panex search ended. */
enum outcome_e {
  PANEX_FOUND,    ///< a shortest path to the goal
  PANEX_NO_PATH,  ///< every reachable stacking was searched: there is no path
  PANEX_MEMORY,   ///< the frontiers outgrew the memory budget
  PANEX_STOPPED,  ///< *stop was set
  PANEX_ERROR     ///< see error
};

/** Settings for one Panex search, and how it went. */
struct panexSearch_c {
  /** Budget half the physical memory, not 2 GB. */
  bool highMemory = false;
  /** Checked as the search runs; when it reads true the search stops. */
  const std::atomic<bool> * stop = nullptr;
  /** When set, kept up to date with the stackings found so far. */
  std::atomic<unsigned long> * progress = nullptr;
  /** When set, kept up to date with how many moves both ends have searched together. */
  std::atomic<unsigned long> * depth = nullptr;
  /** When set, kept up to date with how many moves of the path are traced back, once the ends meet. */
  std::atomic<unsigned long> * traced = nullptr;
  /** Carry on this problem's saved search, if there is one; otherwise start afresh. */
  bool resume = false;
  /**
   * Where searches are saved: a folder per puzzle in here. Empty for
   * BURRTOOLS_PANEX_DIR, or else the user's cache folder (userCacheDirectory).
   */
  std::string workDir;
  /**
   * Disk the saved levels may fill, in bytes. 0 for BURRTOOLS_PANEX_DISK_GB,
   * or else half the free space, at most 200 GB. The fuller it gets, the
   * fewer levels are kept, and the longer tracing the path back takes.
   */
  unsigned long long diskBudget = 0;
  /**
   * Save the search this often, the first time this long into the run, so a
   * crash loses no more. 0 saves only when the search stops.
   */
  unsigned int autosaveMinutes = 20;
  /** For the tests: stop once both ends together are this many moves deep. */
  unsigned long stopAtDepth = 0;
  /** Worker threads; 0 for BURRTOOLS_THREADS, or else every core. */
  unsigned int threads = 0;
  /** Take rod sets without Panex columns too (unsupported with anyRules). */
  bool anyRules = false;
  /**
   * Stackings a search may keep for tracing its path back before it solves
   * each half instead; 0 for a quarter of the memory budget. The tests set it
   * low to try the halving on small puzzles.
   */
  unsigned long long keepLimit = 0;

  /** Out: how the search ended, and what it took. */
  outcome_e outcome = PANEX_NO_PATH;
  std::string error;
  unsigned long long found = 0;
  unsigned long long peakMemory = 0;
  /** The search was saved when it stopped: Continue can carry it on. */
  bool saved = false;
  /** Disk the saved levels take, and how many levels apart they are. */
  unsigned long long diskBytes = 0;
  unsigned int levelInterval = 1;
  /** Moves both ends had searched when the search ended. */
  unsigned long depthReached = 0;
};

/**
 * A shortest sequence of rod transfers from the start stacking to the goal,
 * as findStackPath would give but for far larger Panex puzzles: it searches
 * level by level from both ends (from one end only when the goal is the
 * start's mirror image) and holds just the newest levels in memory. Levels
 * that do not fit in memory go to disk every few levels apart, and the path
 * is traced back between them once the ends meet. A search that stops, by
 * *stop or for want of memory, is saved for resume. nullptr when there is
 * no path or the search stopped.
 */
std::unique_ptr<separation_c> solve(const problem_c & prob, panexSearch_c & search);

} // namespace panex

#endif
