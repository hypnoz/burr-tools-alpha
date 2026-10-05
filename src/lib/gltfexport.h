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

/* Export the disassembly animation of a solution as a binary glTF (.glb)
 * file: one node per piece, keyframed so that any glTF viewer plays the
 * same moves and rotations as the solution slider in the 3D view.
 */
#ifndef __GLTF_EXPORT_H__
#define __GLTF_EXPORT_H__

#include <string>
#include <vector>

class problem_c;

namespace gltfExport {

  struct options_c {
    /* playback time of one move or one 90 degree rotation */
    float secondsPerStep = 0.5f;
    /* edge length of one cell in metres; AR viewers show the puzzle this size */
    float cellSize = 0.01f;
    /* keyframes per step; rotations and bent slides need more than 1 */
    unsigned int samplesPerStep = 8;
    /* play the moves backwards after the disassembly, so a looping viewer
     * goes from assembled to apart and back */
    bool reassemble = true;
    /* bevelled cell edges as in the 3D view; without them the pieces are
     * plain blocks and the file is a fraction of the size */
    bool bevel = true;
  };

  /* sRGB colour in 0..1, as the 3D view uses it */
  struct color_c { float r, g, b; };

  /* Build the .glb for saved solution sol of pr. pieceColors holds one
   * colour per piece (problem piece index). Returns an empty string on
   * success, otherwise why nothing was written. */
  std::string solutionAnimation(const problem_c & pr, unsigned int sol,
                                const std::vector<color_c> & pieceColors,
                                const options_c & opt,
                                std::vector<unsigned char> & glb);

  /* Same, written to fname. */
  std::string writeSolutionAnimation(const char * fname,
                                     const problem_c & pr, unsigned int sol,
                                     const std::vector<color_c> & pieceColors,
                                     const options_c & opt);
}

#endif
