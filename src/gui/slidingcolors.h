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
 * Colours of a sliding puzzle's start/goal tray in the GUI, kept in one
 * place so they can be changed or reverted together.
 *
 * History:
 *   2026-10-04  Marked cells in the piece's colour, no S# labels.
 *   2026-10-02  Black tray with white S# labels.
 *               Before: a white tray (list swatch 255,255,255; 2D editor
 *               chequer 255/235; 3D view 1.0) with orange labels
 *               (230,110,0). To revert, put those values back below.
 */
#ifndef __SLIDINGCOLORS_H__
#define __SLIDINGCOLORS_H__

namespace slidingColors {

/* The tray's swatch in the Entities shape list. List text picks black or
 * white by itself to suit the swatch. */
const unsigned char LIST_R = 0, LIST_G = 0, LIST_B = 0;

/* 2D voxel editor: the tray's chequerboard tiles. Not pure black, so the
 * black grid lines between tiles still show. */
const unsigned char EDIT_LIGHT = 50;
const unsigned char EDIT_DARK = 30;

/* 3D view: the tray's grey level, 0 black to 1 white. Slightly above 0 so
 * the lit faces still show the tray's shape. */
const float VIEW_GREY = 0.12f;

/* Text labels on tray voxels. Unused since 2026-10-04: the 2D editor and
 * the 3D view show a marked cell in its piece's colour instead. */
const unsigned char LABEL_R = 255, LABEL_G = 255, LABEL_B = 255;

} // namespace slidingColors

#endif
