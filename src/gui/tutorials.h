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
/*
 * The tutorial for each space grid, and what the grid selector says about
 * each one.
 */
#ifndef __TUTORIALS_H__
#define __TUTORIALS_H__

#include "../lib/gridtype.h"

#include <string>

namespace tutorials {

/* The name of a grid type as the selector and the tutorials give it. */
const char * gridName(gridType_c::gridType type);

/* The link of the selector's "open file" text: this, then the file name. */
#define EXAMPLE_LINK "burrtools-example:"

/* The file of a grid type's example puzzle in the examples folder, such
 * as "CubeInCage.xmpuzzle"; nullptr when there is none. */
const char * exampleFile(gridType_c::gridType type);

/* The name of its example picture in exampleimages.h. */
const char * pictureName(gridType_c::gridType type);

/* The example picture of a grid type, ready for an <img src> in
 * Fl_Help_View: its name, or empty when there is none, and its size. */
std::string examplePicture(gridType_c::gridType type, int * w = nullptr, int * h = nullptr);

/* The tutorial for a grid type, as HTML for Fl_Help_View. */
std::string tutorialHtml(gridType_c::gridType type);

/* What the grid selector shows for a grid type, as HTML for Fl_Help_View:
 * a description, any warnings, and an example: the grid's example picture,
 * scaled to fit within maxW x maxH. */
std::string selectorHtml(gridType_c::gridType type, int fontSize, int maxW, int maxH);

} // namespace tutorials

#endif
