/* BurrTools
 *
 * Colours of a sliding puzzle's start/goal tray in the GUI, kept in one
 * place so they can be changed or reverted together.
 *
 * History:
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

/* The S# piece labels drawn on tray voxels, in the 2D editor and 3D view. */
const unsigned char LABEL_R = 255, LABEL_G = 255, LABEL_B = 255;

} // namespace slidingColors

#endif
