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
#include "voxeleditgroup.h"
#include "piececolor.h"
#include "guigridtype.h"

#include "../lib/voxel.h"
#include "../lib/puzzle.h"

#include <FL/Fl_Pixmap.H>
#include <FL/Fl_Multiline_Output.H>
#include <FL/fl_draw.H>

#include <math.h>

// some tool widgets, that may be swapped out later into another file

// draws an definable evenly spaced number of lines in one direction
class LineSpacer : Fl_Widget {

  int lines;
  bool vertical;
  int gap;

  public:

    LineSpacer(int x, int y, int w, int h, int borderSpace) : Fl_Widget(x, y, w, h), lines(2), vertical(true), gap(borderSpace) {}

    void draw(void) {

      fl_color(color());
      fl_rectf(x(), y(), w(), h());

      if (lines <= 1) return;

      fl_color(0);

      if (vertical) {

        /* 3px ticks read better than hairlines. Thin them when the stops
         * are close, so neighbouring ticks never run together. */
        int span = h()-2*gap-1;
        int thick = span / (lines-1) / 2;
        if (thick > 3) thick = 3;
        if (thick < 1) thick = 1;
        for (int i = 0; i < lines; i++) {
          int ypos = y()+ gap + span*i/(lines-1);
          fl_rectf(x(), ypos - thick/2, w(), thick);
        }

      } else {

        for (int i = 0; i < lines; i++) {
          int xpos = x()+ gap + (w()-2*gap-1)*i/(lines-1);
          fl_line(y(), xpos, y()+w()-1, xpos);
        }
      }

    }

    void setLines(int l, int vert) {
      lines = l;
      vertical = vert;
      redraw();
    }

};

/* "Front" / "Z Layer" / "Back", turned 90° counter-clockwise so the words
 * read upward along the Z slider. */
class ZAxisLabel_c : public Fl_Widget {
public:
  ZAxisLabel_c(int x, int y, int w, int h) : Fl_Widget(x, y, w, h) {
    color(FL_BACKGROUND_COLOR);
  }
  void draw(void) {
    fl_color(color());
    fl_rectf(x(), y(), w(), h());
    fl_font(FL_HELVETICA, FL_NORMAL_SIZE);
    fl_color(FL_FOREGROUND_COLOR);
    fl_push_clip(x(), y(), w(), h());

    int frontW = (int)fl_width("Front");
    int backW = (int)fl_width("Back");
    int midW = (int)fl_width("Z Layer");
    int pad = 3;
    int top = y() + pad;
    int bottom = y() + h() - pad;
    /* Glyphs sit to the left of the baseline after the turn. */
    int bx = x() + w() - fl_descent() - 1;

    fl_draw(90, "Front", bx, top + frontW);
    fl_draw(90, "Back", bx, bottom);

    int gapTop = top + frontW;
    int gapBot = bottom - backW;
    int mid = (gapTop + gapBot) / 2;
    fl_draw(90, "Z Layer", bx, mid + midW / 2);

    fl_pop_clip();
  }
};

static void cb_VoxelEditGroupZselect_stub(Fl_Widget* o, void* v) { static_cast<VoxelEditGroup_c*>(v)->cb_Zselect(static_cast<Fl_Slider*>(o)); }
static void cb_VoxelEditGroupSqedit_stub(Fl_Widget* /*o*/, void* v) { static_cast<VoxelEditGroup_c*>(v)->cb_Sqedit(); }

VoxelEditGroup_c::VoxelEditGroup_c(int x, int y, int w, int h, puzzle_c * puzzle, const guiGridType_c * ggt) : Fl_Group(0, 0, 300, 300), layoutable_c(x, y, w, h) {

  setShrinkMinSize(65, 80);
  shrinkPrio(128, 0);

  x = 0;
  y = 0;
  w = 300;
  h = 300;

  const int labelW = 22;
  const int axisW = 5;
  new ZAxisLabel_c(x, y, labelW, h);

  int sx = x + labelW;
  zselect = new Fl_Slider(sx, y, 15, h);
  zselect->tooltip(" Select Z Plane ");
  zselect->color((Fl_Color)237);
  zselect->selection_color(FL_WHITE);
  zselect->step(1);
  zselect->callback(cb_VoxelEditGroupZselect_stub, this);
  zselect->clear_visible_focus();
  squareZKnob();

  space = new LineSpacer(sx + 15, y, 5, h, 4);

  /* A few pixels of air between the ticks and the green Z axis. */
  const int tickGap = 3;
  int gx = sx + 20 + tickGap;
  {
    Fl_Box* o = new Fl_Box(gx, y, axisW, h - 5);
    o->box(FL_FLAT_BOX);
    o->color(fl_rgb_color(0, 192, 0));
  }
  {
    Fl_Box* o = new Fl_Box(gx, y + h - 5, w - gx, 5);
    o->box(FL_FLAT_BOX);
    o->color((Fl_Color)1);
  }

  sqedit = ggt->getGridEditor(gx + 10, y, w - (gx + 10), h - 10, puzzle);
  sqedit->tooltip(" Fill and empty voxels ");
  sqedit->box(FL_NO_BOX);
  sqedit->callback(cb_VoxelEditGroupSqedit_stub, this);
  sqedit->clear_visible_focus();

  resizable(sqedit);
}

void VoxelEditGroup_c::squareZKnob(void) {
  if (!zselect || zselect->h() < 1)
    return;
  /* Knob length is a fraction of the track. Match the slider's width
   * so the button stays square as the editor is resized. */
  double frac = (double)zselect->w() / (double)zselect->h();
  if (frac > 1)
    frac = 1;
  zselect->slider_size(frac);
}

void VoxelEditGroup_c::resize(int X, int Y, int W, int H) {
  Fl_Group::resize(X, Y, W, H);
  squareZKnob();
}

void VoxelEditGroup_c::newGridType(const guiGridType_c * ggt, puzzle_c * puzzle) {

  gridEditor_c * nsq;

  nsq = ggt->getGridEditor(sqedit->x(), sqedit->y(), sqedit->w(), sqedit->h(), puzzle);
  nsq->tooltip(" Fill and empty voxels ");
  nsq->box(FL_NO_BOX);
  nsq->callback(cb_VoxelEditGroupSqedit_stub, this);
  nsq->clear_visible_focus();

  resizable(nsq);

  remove(sqedit);
  delete sqedit;

  sqedit = nsq;
  add(sqedit);
}


void VoxelEditGroup_c::setZ(unsigned int val) {
  if (val > zselect->maximum()) val = (unsigned int)zselect->maximum();
  zselect->value(int(zselect->maximum()-val));
  sqedit->setZ(val);
}

void VoxelEditGroup_c::setPuzzle(puzzle_c * puzzle, unsigned int num) {
  sqedit->setPuzzle(puzzle, num);
  if (puzzle && (num < puzzle->getNumberOfShapes())) {
    voxel_c * v = puzzle->getShape(num);
    if (v) {
      zselect->bounds(0, v->getZ()-1);
      zselect->value(int(zselect->maximum()-sqedit->getZ()));
      space->setLines(v->getZ(), true);
    }
  }
}

void VoxelEditGroup_c::draw() {
  fl_push_clip(x(), y(), w(), h());
  Fl_Group::draw();
  fl_pop_clip();
}

void VoxelEditGroup_c::cb_Zselect(Fl_Slider* o) {
  sqedit->setZ(int(zselect->maximum() - o->value()));
}

