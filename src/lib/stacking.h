/* BurrTools
 *
 * Stacking boards: discs on vertical rods, moved one top disc at a time.
 */
#ifndef __STACKING_H__
#define __STACKING_H__

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
};

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
  std::vector<int> rodX;
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

/**
 * Shortest sequence of rod transfers from the start stacking to the goal.
 * Each transfer is three placements (lift, cross, drop) so the move slider
 * can scrub that motion. nullptr when no path exists within maxStates.
 */
std::unique_ptr<separation_c> findStackPath(const problem_c & prob,
                                            unsigned int maxStates = 250000);

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
