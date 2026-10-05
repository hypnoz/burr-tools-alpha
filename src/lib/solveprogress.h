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
#ifndef __SOLVE_PROGRESS_H__
#define __SOLVE_PROGRESS_H__

#include "disassembler.h"

#include <string>
#include <vector>

/**
 * Where a solve stands, from start to end: one picture for the GUI, the
 * command line tools and anything else that wants to show progress. Made by
 * solveThread_c::getProgressSnapshot().
 */
struct solveProgress_c {

  enum Stage {
    STAGE_PREPARE,      ///< finding the placements of each piece
    STAGE_REDUCE,       ///< removing placements that can not be part of an assembly
    STAGE_ASSEMBLE,     ///< searching for assemblies (taking them apart alongside)
    STAGE_DISASSEMBLE,  ///< all assemblies found, the rest are being taken apart
    STAGE_STOPPING,     ///< asked to stop, winding down
    STAGE_PAUSED,
    STAGE_DONE,
    STAGE_ERROR,
    STAGE_OTHER         ///< a search that reports in its own way (sliding, stacking)
  };

  Stage stage = STAGE_OTHER;
  unsigned long long elapsedMs = 0;

  /** preparing and reducing: the piece at work (from 1) of how many */
  unsigned int piece = 0, pieces = 0;

  /** the assembly search: how far (0..1), search steps, threads */
  float assemblyFraction = 0;
  unsigned long iterations = 0;
  unsigned int assemblerThreads = 1;

  unsigned long assemblies = 0, solutions = 0;

  /** taking apart: is it wanted, how many done, how many found and not done */
  bool disassembly = false;
  unsigned int disasmCompleted = 0, disasmPending = 0, disasmWorkers = 0;
  /** the take-aparts under way, the longest running first */
  std::vector<disassemblyProgress_c> running;

  /**
   * The whole solve, 0..1; it does not go back during a run. An estimate
   * while the assembly search runs. When all that is left is take-aparts
   * of unknown length, overallKnown is false and the value only says how
   * many of them are through.
   */
  float overall = 0;
  bool overallKnown = true;
  /** the level at hand of the longest running take-apart, 0..1 */
  float levelFraction = 0;

  /** seconds still to go; negative when that can not be told */
  double secondsLeft = -1;

  /** One line saying what is going on, for a status line. */
  std::string activity(void) const;
};

#endif
