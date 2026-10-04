/* BurrTools
 *
 * Stacking boards: discs on vertical rods, moved one top disc at a time.
 */
#ifndef __STACKING_H__
#define __STACKING_H__

#include <atomic>
#include <memory>
#include <string>
#include <vector>

class puzzle_c;
class problem_c;
class separation_c;
class assembly_c;
class voxel_c;
class xmlWriter_c;
class xmlParser_c;

namespace stacking {

/** True when the puzzle uses the Stacking grid type. */
bool isStacking(const puzzle_c & puz);
bool isStacking(const problem_c & prob);

/** One board: a row of rods plus the rules that govern moves. */
struct rodSet_c {
  std::string name;
  unsigned int rodCount = 3;
  /** When true, a rod can hold every disc in the problem. */
  bool growHeight = true;
  /** Discs a rod can hold when growHeight is false. */
  unsigned int definedHeight = 8;
  /** A larger size number may not sit on a smaller one. Equal sizes may stack. */
  bool sizeMatters = true;
  /** When true, a disc may move only to a neighboring rod. */
  bool distanceMatters = false;
  /** Kept so older files still load. The editor no longer uses it. */
  bool canMoveOver = false;
  /**
   * Panex columns: a disc of size s may sit at most s places below the top
   * of its rod, and discs stack in any order (size matters is ignored). One
   * disc more than the height may wait at the top, raised into the bridge
   * that joins the rods; a raised disc blocks every move that passes over it.
   */
  bool panexColumns = false;
  /** With Panex columns: an extra rod beside rod 1 that holds any disc. */
  bool pocketColumn = false;
  /** Discs the pocket column holds. */
  unsigned int pocketHeight = 1;
};

/** Rods on the board: the rod count, plus the pocket column when it is on. */
unsigned int totalRods(const rodSet_c & board);

/**
 * True for the pocket column. It is the last rod in a stack map, so turning
 * it on or off never renumbers the other rods, but it stands beside rod 1.
 */
bool isPocket(const rodSet_c & board, unsigned int rod);

/** Where a rod stands along the board: rod r at r, the pocket at -1. */
int rodPosition(const rodSet_c & board, unsigned int rod);

/** "rod 2" or "the pocket column", for messages. */
std::string rodName(const rodSet_c & board, unsigned int rod);

/** One disc occurrence: shape id plus which copy (0 .. count-1). */
struct diskRef_c {
  unsigned int shapeId = 0;
  unsigned int instance = 0;

  bool operator==(const diskRef_c & o) const {
    return shapeId == o.shapeId && instance == o.instance;
  }
};

/** Bottom-to-top disc lists, one vector per rod. */
struct stackMap_c {
  std::vector<std::vector<diskRef_c>> rods;
};

/** Hotspot position of one disc. x == -1 means the disc is not on a rod. */
struct hotspot_c {
  int x = 0;
  int y = 0;
  int z = 0;
  bool placed = false;
};

/** Where the rods and the discs of one stacking sit, for the 3D view and replay. */
struct boardLayout_c {
  int spacing = 4;
  unsigned int rodCount = 0;
  unsigned int rodHeight = 1;
  /** x of each rod, the pocket column included. */
  std::vector<int> rodX;
  /** Drawn height of each rod; the pocket column is shorter. */
  std::vector<unsigned int> rodHeights;
  /** Rods are Panex columns: drawn with a V that narrows to the bottom. */
  bool panex = false;
  /** The last rod is the pocket column, drawn as a plain peg. */
  bool pocket = false;
  /** Largest disc size, so the V stays inside the rod spacing. */
  unsigned int maxSize = 1;
  /** Parallel to the problem's piece list (parts, then instances). */
  std::vector<hotspot_c> disks;
};

/**
 * Rebuild a flat disc. Size 1 is a 3×3 square, one voxel thick, with the
 * centre cell empty. Each larger size adds one ring, so size n is
 * (2n+1)×(2n+1) with only the centre empty. Keeps the current colour.
 */
void generateDisk(voxel_c * v, unsigned int size);

/** Put the hotspot at the disc centre so a rod position is the disc's middle. */
void ensureHotspot(voxel_c * v);

/** Add a size-1 disc shape. Returns its shape index. */
unsigned int addDisk(puzzle_c & puz, unsigned int size = 1);

void saveRodSets(const puzzle_c & puz, xmlWriter_c & xml);
void loadRodSets(puzzle_c & puz, xmlParser_c & pars);

void saveProblem(const problem_c & prob, xmlWriter_c & xml);
void loadProblem(problem_c & prob, xmlParser_c & pars);

/** Drop disc refs whose shape was removed, then renumber higher shape ids. */
void noteShapeRemoved(problem_c & prob, unsigned int shapeId);
void noteShapeSwap(problem_c & prob, unsigned int a, unsigned int b);
/** Drop instances that are no longer in the problem's piece counts. */
void trimStacks(problem_c & prob);

/**
 * Give the start and goal maps at least the rod set's rod count. Rods past
 * the count are kept while they still hold discs, so no disc is lost.
 */
void syncMaps(problem_c & prob);

/**
 * Empty when every disc is on the start and the goal exactly once and both
 * stackings obey the rod set's rod count, height and size rules. Otherwise
 * one sentence naming the first problem, rule breaks before missing discs.
 */
std::string setupError(const problem_c & prob);

/**
 * Place the next unused copy of shapeId on top of rod. Empty string on
 * success. With enforceRules false the height and size rules are not
 * checked, so the editor can build a stacking setupError then reports.
 */
std::string placeDisk(problem_c & prob, bool goal, unsigned int shapeId, unsigned int rod,
                      bool enforceRules = true);
/** Take the top disc off rod. Empty string on success. */
std::string liftTop(problem_c & prob, bool goal, unsigned int rod);

/** Remove one disc instance from both stackings and renumber later copies. */
void dropInstance(problem_c & prob, unsigned int shapeId, unsigned int instance);

/** Swap a disc with the one delta steps away. +1 moves it up the rod. */
bool moveDisk(problem_c & prob, bool goal, unsigned int rod, unsigned int index, int delta);

boardLayout_c layoutBoard(const problem_c & prob, bool goal);

/** How a stacking search ended. */
enum stackOutcome_e {
  STACK_FOUND,    ///< a path to the goal
  STACK_NO_PATH,  ///< every reachable stacking was searched: there is no path
  STACK_MEMORY,   ///< stopped at its memory limit; a path may still exist
  STACK_STOPPED   ///< *stop was set
};

/** Settings for one stacking search, and how it went. */
struct stackSearch_c {
  /** Stackings to hold before giving up; 0 for as many as fit the memory budget. */
  unsigned long maxStates = 0;
  /** With maxStates 0, budget half the physical memory, not 2 GB. */
  bool highMemory = false;
  /** Checked as the search runs; when it reads true the search stops. */
  const std::atomic<bool> * stop = nullptr;
  /** When set, kept up to date with the stackings visited so far. */
  std::atomic<unsigned long> * progress = nullptr;

  /** Out: how the search ended, stackings visited, and the limit it used. */
  stackOutcome_e outcome = STACK_NO_PATH;
  unsigned long visited = 0;
  unsigned long memoryStates = 0;
};

/**
 * Shortest sequence of rod transfers from the start stacking to the goal.
 * Each transfer is three placements (lift, cross, drop) so the move slider
 * can scrub that motion. nullptr when no path exists within maxStates.
 */
std::unique_ptr<separation_c> findStackPath(const problem_c & prob,
                                            unsigned int maxStates = 0);

/** findStackPath with a stop flag and progress, reporting how it ended in search. */
std::unique_ptr<separation_c> findStackPath(const problem_c & prob, stackSearch_c & search);

/**
 * A stacking as a search sees it: for each rod, its discs bottom to top,
 * numbered as the problem lists its pieces (parts, then copies).
 */
typedef std::vector<std::vector<unsigned int>> stacking_t;

/** Start, goal and each disc's size, for a search. False when setupError is not empty. */
bool searchInput(const problem_c & prob, stacking_t & start, stacking_t & goal,
                 std::vector<unsigned int> & sizes);

/** Height of the rods: the defined height, or the disc count when they grow. */
unsigned int rodHeight(const rodSet_c & board, unsigned int discCount);

/** Discs one rod holds: a Panex column one more than its height, the pocket its own height. */
unsigned int rodCapacity(const rodSet_c & board, unsigned int rod, unsigned int discCount);

/** The move slider's frames for a path of stackings, start to goal. */
std::unique_ptr<separation_c> pathSeparation(const problem_c & prob,
                                             const std::vector<stacking_t> & path);

/** Placements per transfer in a findStackPath result: lift, cross, drop. */
const unsigned int STEPS_PER_MOVE = 3;

/** Transfers, not the three visual placements of each transfer. */
unsigned int logicalMoves(const separation_c & path);

/** Assembly whose placements are the start stacking. */
std::unique_ptr<assembly_c> startAssembly(const problem_c & prob);

/** Span used as the disassembly animation unit. */
unsigned int boardSpan(const problem_c & prob);

} // namespace stacking

#endif
