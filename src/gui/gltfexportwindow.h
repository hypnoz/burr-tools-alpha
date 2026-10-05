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

/* The options dialog of File > Export > Solution Animation (GLB) */
#ifndef __GLTF_EXPORT_WINDOW_H__
#define __GLTF_EXPORT_WINDOW_H__

#include "Layouter.h"
#include "../lib/gltfexport.h"

class gltfExportWindow_c : public LFl_Double_Window {

  private:

    LFl_Float_Input  * Speed;
    LFl_Float_Input  * CellSize;
    LFl_Check_Button * Reassemble;
    LFl_Check_Button * Bevel;

    bool accepted;

  public:

    gltfExportWindow_c(const gltfExport::options_c & opt, unsigned int steps);

    void cb_Export(void);
    void cb_Cancel(void);

    /* true, when the dialog was closed with Export */
    bool wasAccepted(void) const { return accepted; }

    /* the options as entered */
    gltfExport::options_c options(void) const;
};

#endif
