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
#include "gltfexportwindow.h"

#include <stdio.h>
#include <stdlib.h>

static void cb_Export_stub(Fl_Widget*, void* v) { static_cast<gltfExportWindow_c*>(v)->cb_Export(); }
static void cb_Cancel_stub(Fl_Widget*, void* v) { static_cast<gltfExportWindow_c*>(v)->cb_Cancel(); }

void gltfExportWindow_c::cb_Export(void) { accepted = true; hide(); }
void gltfExportWindow_c::cb_Cancel(void) { accepted = false; hide(); }

gltfExport::options_c gltfExportWindow_c::options(void) const {
  gltfExport::options_c opt;
  float s = (float)atof(Speed->value());
  if (s > 0) opt.secondsPerStep = s;
  float c = (float)atof(CellSize->value());
  if (c > 0) opt.cellSize = c / 1000.0f;
  opt.reassemble = Reassemble->value() != 0;
  opt.bevel = Bevel->value() != 0;
  return opt;
}

gltfExportWindow_c::gltfExportWindow_c(const gltfExport::options_c & opt, unsigned int steps)
  : LFl_Double_Window(false), accepted(false)
{
  label("Export Solution Animation");

  char val[32];

  {
    LFl_Frame * fr = new LFl_Frame(0, 0, 1, 1);

    (new LFl_Box("Seconds per move", 0, 0))->stretchRight();
    (new LFl_Box(1, 0))->setMinimumSize(5, 0);
    Speed = new LFl_Float_Input(2, 0, 1, 1);
    snprintf(val, sizeof(val), "%.2f", opt.secondsPerStep);
    Speed->value(val);
    Speed->weight(1, 0);
    Speed->setMinimumSize(60, 0);
    Speed->tooltip("Time for one move or one 90 degree rotation");

    (new LFl_Box("Cell size (mm)", 0, 1))->stretchRight();
    (new LFl_Box(1, 1))->setMinimumSize(5, 0);
    CellSize = new LFl_Float_Input(2, 1, 1, 1);
    snprintf(val, sizeof(val), "%.1f", opt.cellSize * 1000.0f);
    CellSize->value(val);
    CellSize->weight(1, 0);
    CellSize->tooltip("Edge length of one cell. AR viewers show the puzzle at this size");

    Reassemble = new LFl_Check_Button("Reassemble after the disassembly", 0, 2, 3, 1);
    Reassemble->value(opt.reassemble ? 1 : 0);
    Reassemble->tooltip("Play the moves backwards at the end, so the looping animation goes apart and back together");

    Bevel = new LFl_Check_Button("Bevelled edges, as in the 3D view", 0, 3, 3, 1);
    Bevel->value(opt.bevel ? 1 : 0);
    Bevel->tooltip("Off gives plain blocks and a much smaller file");

    fr->end();
  }

  {
    char info[100];
    snprintf(info, sizeof(info), "%u steps. Pieces keep their solution colours.", steps);
    LFl_Box * b = new LFl_Box("", 0, 1, 1, 1);
    b->copy_label(info);
    b->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
    b->pitch(5);
  }

  {
    layouter_c * o = new layouter_c(0, 2);
    o->pitch(5);

    LFl_Button * btn = new LFl_Button("Export...", 0, 0, 1, 1);
    btn->callback(cb_Export_stub, this);
    btn->weight(1, 0);

    (new LFl_Box(1, 0))->setMinimumSize(5, 0);

    btn = new LFl_Button("Cancel", 2, 0, 1, 1);
    btn->callback(cb_Cancel_stub, this);
    btn->weight(1, 0);

    o->end();
  }

  set_modal();
}
