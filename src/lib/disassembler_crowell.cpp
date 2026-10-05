/* BurrTools
 *
 * Andrew Crowell take-apart BFS. Same structure as disassembler_0_c with a
 * per-node successor cap matching CheckAvailMoves Nkeep.
 * Feature map (implemented vs not, and how to extend): crowell_solver.h
 */
#include "disassembler_crowell.h"

#include "bt_assert.h"

#include "disassemblernode.h"

#include <vector>

/* The search itself is disassembler_a_c::searchLevels(); here a position
 * gives at most MAX_SUCCESSORS new ones (Fortran Nkeep).
 */
separation_c * disassembler_crowell_c::disassemble_rec(const std::vector<unsigned int> &pieces, disassemblerNode_c * start) {
  return searchLevels(pieces, start, MAX_SUCCESSORS);
}
