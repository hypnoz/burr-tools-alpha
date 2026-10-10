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
#ifndef __SYMMETRIES_2_H__
#define __SYMMETRIES_2_H__

#include "symmetries.h"
#include "bitfield.h"

class gridType_c;

/** this is the symmetries class for spheres.
 * Internally this class differs a bit from the other implementations
 * because the spheres have so many orientations. To many that they
 * don't fit into a long long, so we need to use a self made bitfield
 * class for those things
 *
 * The transformations of the sphere grid do not form a group: many
 * compositions leave the grid, so a shape's symmetry list is not a
 * subgroup and the generated list of 241 symmetry lists is only what
 * the generator happened to find. A shape with a list outside it used
 * to fail the assertion in calculateSymmetry(). Now the lists live in
 * a registry seeded from the generated tables: an unseen list is
 * appended, with its derived tables computed by the same rules as the
 * generator (see deriveTables()), and gets the next free number. The
 * registry is shared by every instance and safe to read from the solver
 * threads while another thread adds to it.
 */
class symmetries_2_c : public symmetries_c {

  public:

    /** all transformations of the sphere grid, mirrors included */
    static constexpr unsigned int TRANSFORMATIONS = 240;

    symmetries_2_c(void);

    unsigned int getNumTransformations(void) const override;
    unsigned int getNumTransformationsMirror(void) const override;
    bool symmetrieContainsTransformation(symmetries_t s, unsigned int t) const override;
    unsigned char transAdd(unsigned char t1, unsigned char t2) const override;
    unsigned char minimizeTransformation(symmetries_t s, unsigned char trans) const override;
    unsigned int countSymmetryIntersection(symmetries_t resultSym, symmetries_t s2) const override;
    bool symmetriesLeft(symmetries_t resultSym, symmetries_t s2) const override;
    symmetries_t calculateSymmetry(const voxel_c * pp) const override;
    bool symmetryContainsMirror(symmetries_t sym) const override;
    bool symmetryKnown(const voxel_c * pp) const override;
    bool isTransformationUnique(symmetries_t s, unsigned int trans) const override;

    /**
     * Compute the tables that belong to one symmetry list, the way the
     * generator in tabs_2/generator_2.cpp does it.
     *
     * \param sym the transformations that map the shape onto itself
     * \param unified the transformations that map some orientation of the shape onto itself
     * \param unique one transformation for each distinct orientation of the shape
     * \param minimizer for each transformation the lowest one that gives the same orientation,
     *        an array of TRANSFORMATIONS entries
     */
    static void deriveTables(const bitfield_c<TRANSFORMATIONS> & sym,
                             bitfield_c<TRANSFORMATIONS> & unified,
                             bitfield_c<TRANSFORMATIONS> & unique,
                             unsigned char * minimizer);

    /** the symmetry list of one shape, bit t set when transformation t maps it onto itself */
    static bitfield_c<TRANSFORMATIONS> symmetryList(const voxel_c * pp);

    /** the number of symmetry lists in the registry, generated and appended ones */
    static unsigned int numSymmetryLists(void);

  public:

    // no copying and assigning
    symmetries_2_c(const symmetries_2_c&) = delete;
    symmetries_2_c& operator=(const symmetries_2_c&) = delete;
};

#endif
