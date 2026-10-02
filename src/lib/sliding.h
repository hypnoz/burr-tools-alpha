/* BurrTools
 *
 * Sliding-tray helpers: start/goal colour maps and one-cell slide search.
 */
#ifndef __SLIDING_H__
#define __SLIDING_H__

#include <memory>
#include <string>
#include <utility>
#include <vector>

class puzzle_c;
class problem_c;
class assembly_c;
class separation_c;
class voxel_c;
class gridType_c;
class disasmToMoves_c;

namespace sliding {

/** True when the puzzle uses the Sliding grid type. */
bool isSliding(const puzzle_c & puz);
bool isSliding(const problem_c & prob);

/** Name prefix for a start/goal tray shape: "sg:<n>:" plus the user label. */
bool isStartGoalShape(const voxel_c * v);
int startGoalNumber(const voxel_c * v);
std::string startGoalUserName(const voxel_c * v);
void setStartGoalUserName(voxel_c * v, const std::string & user);
/** User-visible name. Start/goal shapes hide the stored "sg:N:" prefix. */
std::string displayName(const voxel_c * v);
bool isHiddenSlidingShape(const voxel_c * v);

/** Create a filled one-layer start/goal shape. Returns its shape index. */
unsigned int addStartGoalShape(puzzle_c & puz, unsigned int sx, unsigned int sy);

/** One problem per start/goal shape, result locked to that shape. */
void syncSlidingProblems(puzzle_c & puz);

/** True when this piece is stamped onto the start or goal of the problem result. */
bool shapeIsRequired(const problem_c & prob, unsigned int shapeId);

/** Toggle the S# mark of shapeId on one cell. goal selects the goal map. */
bool toggleCellMark(voxel_c * tray, int x, int y, unsigned int shapeId, bool goal);

/** Keep piece-voxel colours in step with start stamps so the assembler can lock them. */
void refreshStartLocks(problem_c & prob);

/** Renumber start/goal colour ids before a shape is removed or two shapes swap. */
void noteShapeRemoved(puzzle_c & puz, unsigned int shapeId);
void noteShapeSwap(puzzle_c & puz, unsigned int a, unsigned int b);

/**
 * Ensure a Sliding puzzle has a filled 2D tray (result), a matching goal
 * map shape, and a problem that references both. Safe to call repeatedly.
 */
void ensureSetup(puzzle_c & puz, unsigned int width = 6, unsigned int height = 6);

/** Colour index (1-based) reserved for a piece shape; 0 if unset / free. */
unsigned int pieceColor(const problem_c & prob, unsigned int shapeId);

/** True when the piece has a coloured start lock on the result tray. */
bool hasStart(const problem_c & prob, unsigned int shapeId);

/** True when the piece has a coloured goal on the goal map. */
bool hasGoal(const problem_c & prob, unsigned int shapeId);

/**
 * Place a piece on the start tray with its drawn orientation, hotspot at
 * (ax, ay, 0). Assigns a palette colour and paints piece + result. Returns
 * false if the footprint would leave the floor or overlap another start.
 */
bool placeStart(problem_c & prob, unsigned int shapeId, int ax, int ay);

/** Clear a start lock for the piece (piece voxels become Neutral). */
void clearStart(problem_c & prob, unsigned int shapeId);

/**
 * Place a goal footprint for the piece at (ax, ay, 0) on the goal map.
 * Does not recolour the piece (so free pieces stay Neutral for assembly).
 */
bool placeGoal(problem_c & prob, unsigned int shapeId, int ax, int ay);

/** Clear the goal footprint for the piece. */
void clearGoal(problem_c & prob, unsigned int shapeId);

/**
 * If the click cell belongs to an existing start of some piece, clear that
 * start and return true. Used by the Start-mode toggle-off click.
 */
bool clearStartAt(problem_c & prob, int x, int y);

/**
 * Toggle goal for the selected piece at (ax, ay). If that cell is already
 * the piece's goal anchor, clear it.
 */
bool toggleGoal(problem_c & prob, unsigned int shapeId, int ax, int ay);

/** Recompute maxHoles = floor cells − total piece volume. */
void syncMaxHoles(problem_c & prob);

/** Arrangements the slide search may visit, normally and with a deeper search. */
const unsigned int SEARCH_STATES = 250000;
const unsigned int DEEP_SEARCH_STATES = 1000000;

/**
 * Find a path with the fewest moves from the given start assembly to the
 * goal map. One move is one piece going anywhere it can reach while the
 * others stay put, turns included; slideRoute gives the way it goes. With
 * nested true a piece may also carry every piece nested inside it, that is
 * lying wholly in its outline (its cells plus the empty cells between them
 * along a row or column, such as a pocket). Touching alone is not nesting. Returns a single-branch separation_c suitable for replay, or
 * nullptr if no path is found within the state budget.
 */
std::unique_ptr<separation_c> findSlidePath(const problem_c & prob,
                                            const assembly_c & start,
                                            unsigned int maxStates = SEARCH_STATES,
                                            bool nested = false);

/**
 * Identity of the finished picture: each piece's placement in the last
 * state. Paths that share this key end with the pieces in the same places.
 */
/**
 * Corner points, as shifts from the start, of the route the moving pieces
 * take on step `step` of path, start and end included, with the fewest
 * straight runs. *movers gets the pieces that move: one, or an outer piece
 * and what is nested in it. Empty when the step is not such a slide.
 */
std::vector<std::pair<int, int>> slideRoute(const problem_c & prob, const separation_c & path,
                                            unsigned int step, std::vector<unsigned int> * movers);

/** Make anim follow each move's route, so a move that turns does not cut the corner. */
void applySlideRoutes(const problem_c & prob, const separation_c & path, disasmToMoves_c & anim);

std::string finalPlacementKey(const separation_c & path);

/**
 * Empty when every start and goal stamp set fits inside its piece.
 * Otherwise a message naming each piece that is marked on more cells
 * than it has voxels.
 */
std::string stampOverflowMessage(const problem_c & prob);

/** Count floor (non-empty) cells on the result tray. */
unsigned int floorCells(const voxel_c & tray);

} // namespace sliding

#endif
