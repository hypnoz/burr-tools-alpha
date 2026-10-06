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

#include "tooltabs.h"
#include "btmessage.h"

#include "../lib/puzzle.h"
#include "../lib/voxel.h"
#include "../lib/gridtype.h"
#include "../lib/sliding.h"

#include <FL/Fl_Hold_Browser.H>
#include <FL/fl_draw.H>
#include "rodbars.h"
#include "piececolor.h"
#include <algorithm>
#include <cstdint>

/* Scrolling piece list that participates in the tool-tab grid. */
class LFl_Hold_Browser : public Fl_Hold_Browser, public layoutable_c {
public:
  LFl_Hold_Browser(int x, int y, int w, int h)
    : Fl_Hold_Browser(0, 0, 10, 10), layoutable_c(x, y, w, h) {}

  void getMinSize(int *width, int *height) const override {
    *width = 110;
    *height = 80;
  }
};

#include "../lib/puzzle.h"
#include "../lib/voxel.h"
#include "../lib/gridtype.h"
#include "WindowWidgets.h"
#include "guigridtype.h"
#include <stdlib.h>

#include "FL/fl_ask.H"
#include "FL/Fl.H"

// the transform group
class TransformButtons : public layouter_c {

  pixmapList_c pm;

public:

  /* type 0 = bricks
   * type 1 = triangles
   * type 2 = spheres
   */
  TransformButtons(int x, int y, int w, int h, int type);

  void cb_Press(long button) { do_callback(this, button); }
  void cb_Preview(long button, bool on);
};

class ToolsButtons : public layouter_c {

  pixmapList_c pm;

public:

  ToolsButtons(int x, int y, int w, int h);

  void cb_Press(long button) { do_callback(this, button); }
};

class SizeButtons : public layouter_c {

  pixmapList_c pm;

public:

  SizeButtons(int x, int y, int w, int h, bool addScale);

  void cb_Press(long button) { do_callback(this, button); }
};

/* Beside the change size group's link buttons: a line along the right edge
 * of their check boxes, and "Locked" beside it, reading downwards. It lies
 * under the buttons, in their column and the one after it. */
class LockBracket : public Fl_Widget, public layoutable_c {

  const LFl_Check_Button * first{nullptr};
  const LFl_Check_Button * last{nullptr};

  static const int GAP = 4;

  /* where the check boxes of the buttons are, as FLTK draws them */
  static int boxSize(const Fl_Check_Button * b) { return std::min((int)b->labelsize(), 25); }
  static int boxX(const Fl_Check_Button * b) { return b->x() + Fl::box_dx(b->box()) + 2; }
  static int boxY(const Fl_Check_Button * b) { return b->y() + (b->h() - boxSize(b)) / 2; }

public:

  LockBracket(int x, int y, int w, int h) : Fl_Widget(0, 0, 0, 0), layoutable_c(x, y, w, h) {
    labelcolor(fl_rgb_color(85, 26, 139));
    labelfont(FL_HELVETICA);
  }

  void buttons(const LFl_Check_Button * f, const LFl_Check_Button * l) { first = f; last = l; }

  void getMinSize(int *width, int *height) const override {
    int bw = 0, bh = 0;
    if (first)
      first->getMinSize(&bw, &bh);
    fl_font(labelfont(), labelsize());
    *width = bw + GAP + fl_height();
    *height = (int)fl_width("Locked");
  }

  void draw(void) override {
    if (!first || !last)
      return;
    const int lineX = boxX(first) + boxSize(first);
    const int top = boxY(first);
    const int bottom = boxY(last) + boxSize(last);

    fl_color(labelcolor());
    fl_line_style(FL_SOLID, 2);
    fl_line(lineX, top, lineX, bottom - 1);
    fl_line_style(0);

    fl_font(labelfont(), labelsize());
    const int tw = (int)fl_width("Locked");
    /* turned a quarter clockwise: the baseline runs down, the letters stand to its right */
    fl_draw(-90, "Locked", lineX + 1 + GAP + fl_descent(), (top + bottom - tw) / 2);
  }
};

// the change size group
class ChangeSize : public layouter_c {

  LFl_Roller* SizeX;
  LFl_Roller* SizeY;
  LFl_Roller* SizeZ;

  Fl_Int_Input* SizeOutX;
  Fl_Int_Input* SizeOutY;
  Fl_Int_Input* SizeOutZ;

  LFl_Check_Button * ConnectX;
  LFl_Check_Button * ConnectY;
  LFl_Check_Button * ConnectZ;

  void calcNewSizes(int ox, int oy, int oz, int *nx, int *ny, int *nz);

public:

  ChangeSize(int x, int y, int w, int h);

  void cb_roll(void);
  void cb_input(void);

  int getX(void) const { return (int)SizeX->value(); }
  int getY(void) const { return (int)SizeY->value(); }
  int getZ(void) const { return (int)SizeZ->value(); }

  void setXYZ(long x, long y, long z);

  /* Keep the Z size as it is: no box, roller or link to change it. */
  void lockZ(bool lock) {
    for (Fl_Widget * w : {(Fl_Widget *)SizeZ, (Fl_Widget *)SizeOutZ, (Fl_Widget *)ConnectZ}) {
      if (lock) w->deactivate();
      else w->activate();
    }
  }
};


#define SZ_BUTTON_Y 20
#define SZ_BUTTON2_Y 25
#define LABEL_FONT_SIZE 14

static void cb_TransformButtons_stub(Fl_Widget* o, long v) { static_cast<TransformButtons*>(o->parent()->parent())->cb_Press(v); }

void TransformButtons::cb_Preview(long button, bool on) {
  Fl_Widget *p = parent();
  while (p) {
    ToolTab *tab = dynamic_cast<ToolTab*>(p);
    if (tab) {
      tab->previewTransform(button, on);
      return;
    }
    p = p->parent();
  }
}

class TransformPreviewButton : public LFlatButton_c {

  TransformButtons *owner;
  long task;

public:

  TransformPreviewButton(int x, int y, int w, int h, Fl_Image *img, Fl_Image *inact, const char *tt, TransformButtons *o, long t)
    : LFlatButton_c(x, y, w, h, img, inact, tt, cb_TransformButtons_stub, t), owner(o), task(t) {}

  TransformPreviewButton(int x, int y, int w, int h, const char *txt, const char *tt, TransformButtons *o, long t)
    : LFlatButton_c(x, y, w, h, txt, tt, cb_TransformButtons_stub, t), owner(o), task(t) {}

  int handle(int event) {
    if (active()) {
      if (event == FL_ENTER)
        owner->cb_Preview(task, true);
      else if (event == FL_LEAVE)
        owner->cb_Preview(task, false);
    }
    return LFlatButton_c::handle(event);
  }
};

TransformButtons::TransformButtons(int x, int y, int w, int h, int type) : layouter_c(x, y, w, h) {

  label("Transform");

  (new LFl_Box(0, 0, 1, 1))->weight(1, 1);
  (new LFl_Box(0, 8, 1, 1))->weight(1, 1);
  (new LFl_Box(8, 0, 1, 1))->weight(1, 1);
  (new LFl_Box(8, 8, 1, 1))->weight(1, 1);

  (new LFl_Box("Flip",   1, 1, 1, 1))->labelsize(LABEL_FONT_SIZE);
  (new LFl_Box("Nudge",  3, 1, 2, 1))->labelsize(LABEL_FONT_SIZE);
  (new LFl_Box("Rotate", 6, 1, 2, 1))->labelsize(LABEL_FONT_SIZE);

  layouter_c * o = new layouter_c(1, 3, 1, 1);

  new TransformPreviewButton(0, 0, 1, 1, pm.get(Transform_Color_Flip_X_xpm, TOOL_ICON_SCALE)        , pm.get(Transform_Disabled_Flip_X_xpm, TOOL_ICON_SCALE)        ,
      " Flip along Y-Z Plane ",              this, 12);
  new TransformPreviewButton(0, 1, 1, 1, pm.get(Transform_Color_Flip_Y_xpm, TOOL_ICON_SCALE)        , pm.get(Transform_Disabled_Flip_Y_xpm, TOOL_ICON_SCALE)        ,
      " Flip along X-Z Plane ",              this, 13);
  new TransformPreviewButton(0, 2, 1, 1, pm.get(Transform_Color_Flip_Z_xpm, TOOL_ICON_SCALE)        , pm.get(Transform_Disabled_Flip_Z_xpm, TOOL_ICON_SCALE)        ,
      " Flip along X-Y Plane ",              this, 14);

  o->end();

  o = new layouter_c(3, 3, 1, 1);

  if (type == 0) {
    new TransformPreviewButton(0, 0, 1, 1, pm.get(Transform_Color_Nudge_X_Left_xpm, TOOL_ICON_SCALE)  , pm.get(Transform_Disabled_Nudge_X_Left_xpm, TOOL_ICON_SCALE)  ,
        " Shift down along X ",                this,  1);
    new TransformPreviewButton(1, 0, 1, 1, pm.get(Transform_Color_Nudge_X_Right_xpm, TOOL_ICON_SCALE) , pm.get(Transform_Disabled_Nudge_X_Right_xpm, TOOL_ICON_SCALE) ,
        " Shift up along X ",                  this,  0);
    new TransformPreviewButton(0, 1, 1, 1, pm.get(Transform_Color_Nudge_Y_Left_xpm, TOOL_ICON_SCALE)  , pm.get(Transform_Disabled_Nudge_Y_Left_xpm, TOOL_ICON_SCALE)  ,
        " Shift down along Y ",                this,  3);
    new TransformPreviewButton(1, 1, 1, 1, pm.get(Transform_Color_Nudge_Y_Right_xpm, TOOL_ICON_SCALE) , pm.get(Transform_Disabled_Nudge_Y_Right_xpm, TOOL_ICON_SCALE) ,
        " Shift up along Y ",                  this,  2);
    new TransformPreviewButton(0, 2, 1, 1, pm.get(Transform_Color_Nudge_Z_Left_xpm, TOOL_ICON_SCALE)  , pm.get(Transform_Disabled_Nudge_Z_Left_xpm, TOOL_ICON_SCALE)  ,
        " Shift down along Z ",                this,  5);
    new TransformPreviewButton(1, 2, 1, 1, pm.get(Transform_Color_Nudge_Z_Right_xpm, TOOL_ICON_SCALE) , pm.get(Transform_Disabled_Nudge_Z_Right_xpm, TOOL_ICON_SCALE) ,
        " Shift up along Z ",                  this,  4);

  } else if (type == 1) {

    new TransformPreviewButton(0, 0, 1, 1, "@7->",
        " Shift up left along XY plane ",  this,  1);
    new TransformPreviewButton(1, 0, 1, 1, "@9->",
        " Shift up right along XY plane ", this,  0);
    new TransformPreviewButton(0, 1, 1, 1, "@4->",
        " Shift left along X ",            this,  3);
    new TransformPreviewButton(1, 1, 1, 1, "@6->",
        " Shift right along X ",           this,  2);
    new TransformPreviewButton(0, 2, 1, 1, "@1->",
        " Shift down left XY plane ",      this,  28);
    new TransformPreviewButton(1, 2, 1, 1, "@3->",
        " Shift down right XY plane ",     this,  27);

    new TransformPreviewButton(0, 3, 1, 1, pm.get(Transform_Color_Nudge_Z_Left_xpm, TOOL_ICON_SCALE)  , pm.get(Transform_Disabled_Nudge_Z_Left_xpm, TOOL_ICON_SCALE)  ,
        " Shift down along Z ",            this,  5);
    new TransformPreviewButton(1, 3, 1, 1, pm.get(Transform_Color_Nudge_Z_Right_xpm, TOOL_ICON_SCALE) , pm.get(Transform_Disabled_Nudge_Z_Right_xpm, TOOL_ICON_SCALE) ,
        " Shift up along Z ",              this,  4);

  } else if (type == 2) {

    new TransformPreviewButton(0, 0, 1, 1, "u @6->",
        " Shift up along Z and right along X ",this,  0);
    new TransformPreviewButton(1, 0, 1, 1, "u @8->",
        " Shift up along Z and up along Y ",   this,  1);
    new TransformPreviewButton(2, 0, 1, 1, "u @4->",
        " Shift up along Z and left along X ", this,  2);
    new TransformPreviewButton(3, 0, 1, 1, "u @2->",
        " Shift up along Z and down along Y ", this,  3);

    new TransformPreviewButton(0, 1, 1, 1, "@9->",
        " Shift up right along XY plane ",     this,  4);
    new TransformPreviewButton(1, 1, 1, 1, "@7->",
        " Shift up left along XY plane ",      this,  5);
    new TransformPreviewButton(2, 1, 1, 1, "@1->",
        " Shift down left along XY plane ",    this,  27);
    new TransformPreviewButton(3, 1, 1, 1, "@3->",
        " Shift down right along XY plane ",   this,  28);


    new TransformPreviewButton(0, 2, 1, 1, "d @6->",
        " Shift down along Z and right along X ", this, 29);
    new TransformPreviewButton(1, 2, 1, 1, "d @8->",
        " Shift down along Z and up along Y ",    this, 30);
    new TransformPreviewButton(2, 2, 1, 1, "d @4->",
        " Shift down along Z and left along X ",  this, 31);
    new TransformPreviewButton(3, 2, 1, 1, "d @2->",
        " Shift down along Z and down along Y ",  this, 32);

  }

  o->end();

  o = new layouter_c(6, 3, 1, 1);

  if (type == 1) {

    new TransformPreviewButton(6, 0, 2, 1, pm.get(Transform_Color_Rotate_X_Left_xpm, TOOL_ICON_SCALE) , pm.get(Transform_Disabled_Rotate_X_Left_xpm, TOOL_ICON_SCALE) ,
        " Rotate 180° along X-Axis ",     this,  6);
    new TransformPreviewButton(6, 1, 2, 1, pm.get(Transform_Color_Rotate_Y_Left_xpm, TOOL_ICON_SCALE) , pm.get(Transform_Disabled_Rotate_Y_Left_xpm, TOOL_ICON_SCALE) ,
        " Rotate 180° along Y-Axis ",     this,  9);
    new TransformPreviewButton(6, 2, 1, 1, pm.get(Transform_Color_Rotate_Z_Left_xpm, TOOL_ICON_SCALE) , pm.get(Transform_Disabled_Rotate_Z_Left_xpm, TOOL_ICON_SCALE) ,
        " Rotate 60° clockwise along Z-Axis ",     this, 10);
    new TransformPreviewButton(7, 2, 1, 1, pm.get(Transform_Color_Rotate_Z_Right_xpm, TOOL_ICON_SCALE), pm.get(Transform_Disabled_Rotate_Z_Right_xpm, TOOL_ICON_SCALE),
        " Rotate 60° anticlockwise along Z-Axis ", this, 11);

  } else {

    new TransformPreviewButton(6, 0, 1, 1, pm.get(Transform_Color_Rotate_X_Left_xpm, TOOL_ICON_SCALE) , pm.get(Transform_Disabled_Rotate_X_Left_xpm, TOOL_ICON_SCALE) ,
        " Rotate 90° clockwise along X-Axis ",     this,  6);
    new TransformPreviewButton(7, 0, 1, 1, pm.get(Transform_Color_Rotate_X_Right_xpm, TOOL_ICON_SCALE), pm.get(Transform_Disabled_Rotate_X_Right_xpm, TOOL_ICON_SCALE),
        " Rotate 90° anticlockwise along X-Axis ", this,  7);
    new TransformPreviewButton(6, 1, 1, 1, pm.get(Transform_Color_Rotate_Y_Left_xpm, TOOL_ICON_SCALE) , pm.get(Transform_Disabled_Rotate_Y_Left_xpm, TOOL_ICON_SCALE) ,
        " Rotate 90° clockwise along Y-Axis ",     this,  9);
    new TransformPreviewButton(7, 1, 1, 1, pm.get(Transform_Color_Rotate_Y_Right_xpm, TOOL_ICON_SCALE), pm.get(Transform_Disabled_Rotate_Y_Right_xpm, TOOL_ICON_SCALE),
        " Rotate 90° anticlockwise along Y-Axis ", this,  8);
    new TransformPreviewButton(6, 2, 1, 1, pm.get(Transform_Color_Rotate_Z_Left_xpm, TOOL_ICON_SCALE) , pm.get(Transform_Disabled_Rotate_Z_Left_xpm, TOOL_ICON_SCALE) ,
        " Rotate 90° clockwise along Z-Axis ",     this, 10);
    new TransformPreviewButton(7, 2, 1, 1, pm.get(Transform_Color_Rotate_Z_Right_xpm, TOOL_ICON_SCALE), pm.get(Transform_Disabled_Rotate_Z_Right_xpm, TOOL_ICON_SCALE),
        " Rotate 90° anticlockwise along Z-Axis ", this, 11);
  }

  o->end();

  (new LFl_Box(0, 2, 1, 1))->setMinimumSize(0, 5);

  (new LFl_Box(2, 0, 1, 1))->setMinimumSize(5, 0);
  (new LFl_Box(5, 0, 1, 1))->setMinimumSize(5, 0);

  end();
}


static void cb_ToolsButtons_stub(Fl_Widget* o, long v) { static_cast<ToolsButtons*>(o->parent())->cb_Press(v); }

ToolsButtons::ToolsButtons(int x, int y, int w, int h) : layouter_c(x, y, w, h) {

  label("Tools");

  (new LFl_Box(0, 0, 1, 1))->weight(1, 1);
  (new LFl_Box(0, 8, 1, 1))->weight(1, 1);
  (new LFl_Box(9, 0, 1, 1))->weight(1, 1);
  (new LFl_Box(9, 8, 1, 1))->weight(1, 1);

  (new LFl_Box("Constrain", 3, 1, 2, 1))->labelsize(LABEL_FONT_SIZE);

  new LFlatButton_c(3, 3, 1, 1, pm.get(InOut_Color_Fixed_In_xpm, TOOL_ICON_SCALE), pm.get(InOut_Disabled_Fixed_In_xpm, TOOL_ICON_SCALE),
      " Make inside fixed ", cb_ToolsButtons_stub, 16);
  new LFlatButton_c(3, 5, 1, 1, pm.get(InOut_Color_Variable_In_xpm, TOOL_ICON_SCALE), pm.get(InOut_Disabled_Variable_In_xpm, TOOL_ICON_SCALE),
      " Make inside variable ", cb_ToolsButtons_stub, 18);
  new LFlatButton_c(3, 7, 1, 1, pm.get(InOut_Color_RemoveColor_In_xpm, TOOL_ICON_SCALE), pm.get(InOut_Disabled_RemoveColor_In_xpm, TOOL_ICON_SCALE),
      " Remove Colours from inside voxels ", cb_ToolsButtons_stub, 20);

  new LFlatButton_c(4, 3, 1, 1, pm.get(InOut_Color_Fixed_Out_xpm, TOOL_ICON_SCALE), pm.get(InOut_Disabled_Fixed_Out_xpm, TOOL_ICON_SCALE),
      " Make outside fixed ", cb_ToolsButtons_stub, 17);
  new LFlatButton_c(4, 5, 1, 1, pm.get(InOut_Color_Variable_Out_xpm, TOOL_ICON_SCALE), pm.get(InOut_Disabled_Variable_Out_xpm, TOOL_ICON_SCALE),
      " Make outside variable ", cb_ToolsButtons_stub, 19);
  new LFlatButton_c(4, 7, 1, 1, pm.get(InOut_Color_RemoveColor_Out_xpm, TOOL_ICON_SCALE), pm.get(InOut_Disabled_RemoveColor_Out_xpm, TOOL_ICON_SCALE),
      " Remove Colours from outside voxels ", cb_ToolsButtons_stub, 21);

  (new LFl_Box(0, 2, 1, 1))->setMinimumSize(0, 5);
  (new LFl_Box(0, 4, 1, 1))->setMinimumSize(0, 5);
  (new LFl_Box(0, 6, 1, 1))->setMinimumSize(0, 5);

  (new LFl_Box(7, 3, 1, 1))->setMinimumSize(5, 0);
  new LFlatButton_c(8, 3, 1, 1, "Fill Holes",
      " Fill every empty voxel that is closed in by the shape, so that it cannot be reached from outside ",
      cb_ToolsButtons_stub, 40);
  new LFlatButton_c(8, 5, 1, 1, "Grid Scale",
      " Scale the shape up, keeping only its surface and the edges of each original voxel, so that the voxels show as a grid ",
      cb_ToolsButtons_stub, 41);

  end();
}


static void cb_SizeButtons_stub(Fl_Widget* o, long v) { static_cast<SizeButtons*>(o->parent())->cb_Press(v); }

SizeButtons::SizeButtons(int x, int y, int w, int h, bool addScale) : layouter_c(x, y, w, h) {

  (new LFl_Box(0, 0, 1, 1))->weight(1, 1);
  (new LFl_Box(0, 8, 1, 1))->weight(1, 1);

  (new LFl_Box("Grid", 0, 1, 1, 1))->labelsize(LABEL_FONT_SIZE);

  new LFlatButton_c(0, 3, 1, 1, pm.get(Grid_Color_Minimize_xpm, TOOL_ICON_SCALE), pm.get(Grid_Disabled_Minimize_xpm, TOOL_ICON_SCALE),
      " Minimize size of grid ", cb_SizeButtons_stub, 15);
  new LFlatButton_c(0, 5, 1, 1, pm.get(Grid_Color_Center_xpm, TOOL_ICON_SCALE), pm.get(Grid_Disabled_Center_xpm, TOOL_ICON_SCALE),
      " Centre shape inside the grid ", cb_SizeButtons_stub, 25);
  new LFlatButton_c(0, 7, 1, 1, pm.get(Grid_Color_Origin_xpm, TOOL_ICON_SCALE), pm.get(Grid_Disabled_Origin_xpm, TOOL_ICON_SCALE),
      " Move shape to origin of grid ", cb_SizeButtons_stub, 24);


  if (addScale) {
    (new LFl_Box("Shape", 2, 1, 1, 1))->labelsize(LABEL_FONT_SIZE);

    new LFlatButton_c(2, 3, 1, 1, pm.get(Rescale_Color_X1_xpm), pm.get(Rescale_Disabled_X1_xpm),
        " Try to minimize size of shape ", cb_SizeButtons_stub, 26);
    new LFlatButton_c(2, 5, 1, 1, pm.get(Rescale_Color_X2_xpm), pm.get(Rescale_Disabled_X2_xpm),
        " Double size of shape ", cb_SizeButtons_stub, 22);
    new LFlatButton_c(2, 7, 1, 1, pm.get(Rescale_Color_X3_xpm), pm.get(Rescale_Disabled_X3_xpm),
        " Triple size of shape ", cb_SizeButtons_stub, 23);
  }

  (new LFl_Box(0, 4, 1, 1))->setMinimumSize(0, 5);
  (new LFl_Box(0, 6, 1, 1))->setMinimumSize(0, 5);

  end();
}

static void cb_ChangeSize_stub(Fl_Widget* o, long /*v*/) { static_cast<ChangeSize*>(o->parent())->cb_roll(); }
static void cb_InputSize_stub(Fl_Widget* o, long /*v*/) { static_cast<ChangeSize*>(o->parent())->cb_input(); }

void ChangeSize::cb_roll(void) {

  int ox, oy, oz, nx, ny, nz;

  ox = atoi(SizeOutX->value());
  nx = (int)SizeX->value();
  oy = atoi(SizeOutY->value());
  ny = (int)SizeY->value();
  oz = atoi(SizeOutZ->value());
  nz = (int)SizeZ->value();

  calcNewSizes(ox, oy, oz, &nx, &ny, &nz);

  char num[20];

  snprintf(num, 20, "%i", nx); SizeOutX->value(num);
  snprintf(num, 20, "%i", ny); SizeOutY->value(num);
  snprintf(num, 20, "%i", nz); SizeOutZ->value(num);
  SizeX->value(nx);
  SizeY->value(ny);
  SizeZ->value(nz);

  do_callback();
}

void ChangeSize::cb_input(void) {
  int ox, oy, oz, nx, ny, nz;

  ox = (int)SizeX->value();
  nx = atoi(SizeOutX->value());
  oy = (int)SizeY->value();
  ny = atoi(SizeOutY->value());
  oz = (int)SizeZ->value();
  nz = atoi(SizeOutZ->value());

  calcNewSizes(ox, oy, oz, &nx, &ny, &nz);

  char num[20];

  snprintf(num, 20, "%i", nx); SizeOutX->value(num);
  snprintf(num, 20, "%i", ny); SizeOutY->value(num);
  snprintf(num, 20, "%i", nz); SizeOutZ->value(num);
  SizeX->value(nx);
  SizeY->value(ny);
  SizeZ->value(nz);

  // seems like the Rollers don't callback, when value is set
  do_callback();
}

void ChangeSize::calcNewSizes(int ox, int oy, int oz, int *nx, int *ny, int *nz) {
  int dx = *nx - ox;
  int dy = *ny - oy;
  int dz = *nz - oz;

  if (dx != 0 && ConnectX->value()) {
    if (ConnectY->value()) dy = dx;
    if (ConnectZ->value()) dz = dx;
  }

  if (dy != 0 && ConnectY->value()) {
    if (ConnectX->value()) dx = dy;
    if (ConnectZ->value()) dz = dy;
  }

  if (dz != 0 && ConnectZ->value()) {
    if (ConnectX->value()) dx = dz;
    if (ConnectY->value()) dy = dz;
  }

  *nx = ox + dx;
  *ny = oy + dy;
  *nz = oz + dz;

  if (*nx < 1) *nx = 1;
  if (*ny < 1) *ny = 1;
  if (*nz < 1) *nz = 1;

  if (*nx > 1000) *nx = 1000;
  if (*ny > 1000) *ny = 1000;
  if (*nz > 1000) *nz = 1000;
}

ChangeSize::ChangeSize(int x, int y, int w, int h) : layouter_c(x, y, w, h) {

  tooltip(" Change size of space ");

  (new LFl_Box(0, 2, 1, 1))->setMinimumSize(0, 5);
  (new LFl_Box(0, 4, 1, 1))->setMinimumSize(0, 5);
  (new LFl_Box(4, 1, 1, 1))->setMinimumSize(5, 0);
  (new LFl_Box(2, 1, 1, 1))->setMinimumSize(5, 0);

  (new LFl_Box(2, 0, 1, 1))->weight(0, 1);
  (new LFl_Box(2, 6, 1, 1))->weight(0, 1);

  SizeX = new LFl_Roller(5, 1, 1, 1);
  SizeX->type(1);
  SizeX->minimum(1);
  SizeX->maximum(1000);
  SizeX->step(0.25);
  SizeX->callback(cb_ChangeSize_stub, 0l);
  SizeX->clear_visible_focus();
  /* the rollers take four fifths of the width to spare, the space after the bracket the rest */
  SizeX->weight(4, 0);
  (new LFl_Box(8, 1, 1, 1))->weight(1, 0);

  SizeY = new LFl_Roller(5, 3, 1, 1);
  SizeY->type(1);
  SizeY->minimum(1);
  SizeY->maximum(1000);
  SizeY->step(0.25);
  SizeY->callback(cb_ChangeSize_stub, 1l);
  SizeY->clear_visible_focus();

  SizeZ = new LFl_Roller(5, 5, 1, 1);
  SizeZ->type(1);
  SizeZ->minimum(1);
  SizeZ->maximum(1000);
  SizeZ->step(0.25);
  SizeZ->callback(cb_ChangeSize_stub, 2l);
  SizeZ->clear_visible_focus();

  SizeOutX = new LFl_Int_Input(3, 1, 1, 1);
  SizeOutX->callback(cb_InputSize_stub, 0l);
  SizeOutX->when(FL_WHEN_RELEASE | FL_WHEN_ENTER_KEY);

  SizeOutY = new LFl_Int_Input(3, 3, 1, 1);
  SizeOutY->callback(cb_InputSize_stub, 1l);
  SizeOutY->when(FL_WHEN_RELEASE | FL_WHEN_ENTER_KEY);

  SizeOutZ = new LFl_Int_Input(3, 5, 1, 1);
  SizeOutZ->callback(cb_InputSize_stub, 2l);
  SizeOutZ->when(FL_WHEN_RELEASE | FL_WHEN_ENTER_KEY);

  (new LFl_Box("X", 1, 1, 1, 1))->labelcolor(fl_rgb_color(255, 0, 0));
  (new LFl_Box("Y", 1, 3, 1, 1))->labelcolor(fl_rgb_color(0, 128, 0));
  (new LFl_Box("Z", 1, 5, 1, 1))->labelcolor(fl_rgb_color(0, 0, 255));

  /* before the buttons, so that they draw over it */
  LockBracket * bracket = new LockBracket(6, 1, 2, 5);
  bracket->tooltip(" Sizes that are ticked are locked together: changing one changes the others ");

  ConnectX = new LFl_Check_Button(" ", 6, 1, 1, 1);
  ConnectY = new LFl_Check_Button(" ", 6, 3, 1, 1);
  ConnectZ = new LFl_Check_Button(" ", 6, 5, 1, 1);

  ConnectX->tooltip(" Link the sizes together so that a change will be done to the other sizes as well ");
  ConnectY->tooltip(" Link the sizes together so that a change will be done to the other sizes as well ");
  ConnectZ->tooltip(" Link the sizes together so that a change will be done to the other sizes as well ");

  ConnectX->clear_visible_focus();
  ConnectY->clear_visible_focus();
  ConnectZ->clear_visible_focus();

  bracket->buttons(ConnectX, ConnectZ);

  end();
}

void ChangeSize::setXYZ(long x, long y, long z) {
  SizeX->value(x);
  SizeY->value(y);
  SizeZ->value(z);

  char num[20];

  snprintf(num, 20, "%li", x); SizeOutX->value(num);
  snprintf(num, 20, "%li", y); SizeOutY->value(num);
  snprintf(num, 20, "%li", z); SizeOutZ->value(num);
}


static void resizeSpace(bool toAll, const ChangeSize *changeSize, puzzle_c * puzzle, unsigned int shape, unsigned int factor = 1) {

  if (toAll) {

    int dx, dy, dz;

    dx = factor*changeSize->getX() - puzzle->getShape(shape)->getX();
    dy = factor*changeSize->getY() - puzzle->getShape(shape)->getY();
    dz = factor*changeSize->getZ() - puzzle->getShape(shape)->getZ();

    if (dx < 0) dx = 0;
    if (dy < 0) dy = 0;
    if (dz < 0) dz = 0;

    for (unsigned int s = 0; s < puzzle->getNumberOfShapes(); s++) {
      int nx = puzzle->getShape(s)->getX()+dx;
      int ny = puzzle->getShape(s)->getY()+dy;
      int nz = puzzle->getShape(s)->getZ()+dz;

      if (nx < 1) nx = 1;
      if (ny < 1) ny = 1;
      if (nz < 1) nz = 1;

      /* make sure the new size is a multiple of factor */
      nx = factor * ((nx+factor-1)/factor);
      ny = factor * ((ny+factor-1)/factor);
      nz = factor * ((nz+factor-1)/factor);

      puzzle->getShape(s)->resize(nx, ny, nz, 0);
    }

  }

  /* Always, as the comment upstream said (the code had an `else`): the loop
   * above only grows shapes, so shrinking the selected one, with "apply to
   * all" on too, happens here. */
  puzzle->getShape(shape)->resize(
        factor*changeSize->getX(), factor*changeSize->getY(), factor*changeSize->getZ(), 0);
}






void ToolTab_0::setVoxelSpace(puzzle_c * puz, unsigned int sh) {
  puzzle = puz;
  shape = sh;

  bt_assert(!puzzle ||
            puzzle->getGridType()->getType() == gridType_c::GT_BRICKS ||
            puzzle->getGridType()->getType() == gridType_c::GT_SLIDING ||
            puzzle->getGridType()->getType() == gridType_c::GT_STACKING);

  if (puzzle && shape < puzzle->getNumberOfShapes())
    changeSize->setXYZ(puzzle->getShape(shape)->getX(),
        puzzle->getShape(shape)->getY(),
        puzzle->getShape(shape)->getZ());
  else
    changeSize->setXYZ(0, 0, 0);

  bool sliding = puzzle && puzzle->getGridType()->getType() == gridType_c::GT_SLIDING;
  /* Sliding shapes lie flat, pieces and start/goal shapes alike: one
   * layer, and nothing that adds more. */
  lockZ(sliding);

  /* Fl_Tabs draws a header for every child, shown or not: the Start/Goal
   * tab is only a child while the puzzle is a sliding one. */
  if (startGoalTab) {
    const bool present = startGoalTab->parent() == this;
    if (sliding && !present) {
      Fl_Widget * cur = value();
      add(startGoalTab);
      startGoalTab->hide();
      if (cur)
        value(cur);
    } else if (!sliding && present) {
      if (value() == startGoalTab && children() > 0)
        value(child(0));
      remove(startGoalTab);
      startGoalTab->hide();
    }
    redraw();
  }
  refreshPieceList();
}

static void cb_ToolTab0Size_stub(Fl_Widget* o, long /*v*/) { static_cast<ToolTab_0*>(o->parent()->parent()->parent())->cb_size(); }
static void cb_ToolTab0Transform_stub(Fl_Widget* o, long v) { static_cast<ToolTab_0*>(o->parent())->cb_transform(v); }
static void cb_ToolTab0Transform2_stub(Fl_Widget* o, long v) { static_cast<ToolTab_0*>(o->parent()->parent())->cb_transform(v); }
static void cb_ToolTab0StartGoal_stub(Fl_Widget* o, long) {
  Fl_Widget * p = o;
  while (p && !dynamic_cast<ToolTab_0*>(p))
    p = p->parent();
  if (p)
    static_cast<ToolTab_0*>(p)->cb_startGoalMode();
}
static void cb_ToolTab0SgPiece_stub(Fl_Widget* o, long) {
  Fl_Hold_Browser * b = dynamic_cast<Fl_Hold_Browser*>(o);
  Fl_Widget * p = o;
  while (p && !dynamic_cast<ToolTab_0*>(p))
    p = p->parent();
  if (p && b && b->value() > 0)
    static_cast<ToolTab_0*>(p)->selectPiece((unsigned int)(uintptr_t)b->data(b->value()));
}

slidePiecePreview_c::slidePiecePreview_c(int x, int y, int w, int h)
  : Fl_Widget(0, 0, 0, 0), layoutable_c(x, y, w, h) {
  box(FL_DOWN_BOX);
  color(FL_BACKGROUND2_COLOR);
}

void slidePiecePreview_c::setPiece(const puzzle_c * puz, unsigned int shapeId) {
  puzzle = puz;
  shape = shapeId;
  redraw();
}

void slidePiecePreview_c::draw(void) {
  draw_box();
  if (!puzzle || shape >= puzzle->getNumberOfShapes())
    return;
  const voxel_c * v = puzzle->getShape(shape);

  /* The cells in use, all layers seen from above. */
  int x0 = INT32_MAX, y0 = INT32_MAX, x1 = -1, y1 = -1;
  for (unsigned int z = 0; z < v->getZ(); z++)
    for (unsigned int y = 0; y < v->getY(); y++)
      for (unsigned int x = 0; x < v->getX(); x++)
        if (!v->isEmpty(x, y, z)) {
          x0 = std::min(x0, (int)x);
          y0 = std::min(y0, (int)y);
          x1 = std::max(x1, (int)x);
          y1 = std::max(y1, (int)y);
        }
  if (x1 < 0)
    return;

  /* As large as fits, with a margin, but not a single cell filling it all. */
  const int margin = 8;
  const int cw = x1 - x0 + 1;
  const int ch = y1 - y0 + 1;
  int cell = std::min((w() - 2 * margin) / cw, (h() - 2 * margin) / ch);
  cell = std::min(cell, 48);
  if (cell < 2)
    return;
  const int left = x() + (w() - cw * cell) / 2;
  const int bottom = y() + (h() + ch * cell) / 2;

  fl_push_clip(x() + Fl::box_dx(box()), y() + Fl::box_dy(box()),
               w() - Fl::box_dw(box()), h() - Fl::box_dh(box()));
  const Fl_Color fill = fl_rgb_color(pieceColorRi(shape), pieceColorGi(shape), pieceColorBi(shape));
  /* cell edges that show on dark pieces as well as on light ones */
  const unsigned int light = pieceColorRi(shape) * 3 + pieceColorGi(shape) * 6 + pieceColorBi(shape);
  const Fl_Color edge = light < 800 ? fl_lighter(fl_lighter(fill)) : fl_darker(fl_darker(fill));
  /* y rises upwards, as in the grid editor */
  for (int y = y0; y <= y1; y++)
    for (int x = x0; x <= x1; x++) {
      bool filled = false;
      for (unsigned int z = 0; z < v->getZ() && !filled; z++)
        filled = !v->isEmpty(x, y, z);
      if (!filled)
        continue;
      const int px = left + (x - x0) * cell;
      const int py = bottom - (y - y0 + 1) * cell;
      fl_color(fill);
      fl_rectf(px, py, cell, cell);
      fl_color(edge);
      fl_rect(px, py, cell, cell);
    }
  fl_pop_clip();
}

/* Lay out again the scroll the widget sits in: its size may have changed. */
static void relayoutAround(Fl_Widget * w) {
  for (Fl_Widget * p = w; p; p = p->parent())
    if (LFl_Scroll * s = dynamic_cast<LFl_Scroll *>(p)) {
      s->relayout();
      return;
    }
}

static void cb_ToolTab0Relayout_stub(Fl_Widget * o, void *) { relayoutAround(o); }

ToolTab_0::ToolTab_0(int x, int y, int w, int h) : ToolTab(x, y, w, h), pieceList(0), piecePreview(0), sgControls(0), sgInfo(0), sgTabColor(FL_BACKGROUND_COLOR), plainTabColor(FL_BACKGROUND_COLOR), plainGroupColor(FL_BACKGROUND_COLOR), selectedSgPiece((unsigned int)-1), modeCallbackPending(false), shownTab(0) {

  {
    layouter_c * o = new layouter_c(0, 1, 1, 1);
    o->label("Size");
    o->pitch(5);

    layouter_c *o2 = new layouter_c(0, 0, 1, 1);

    changeSize = new ChangeSize(0, 1, 1, 1);
    changeSize->callback(cb_ToolTab0Size_stub);

    toAll = new LFl_Check_Button("Apply to All Shapes", 0, 0, 1, 1);
    toAll->tooltip(" If this is active, all operations (including transformations and constrains are done to all shapes ");
    toAll->clear_visible_focus();
    toAll->stretchHCenter();

    o2->end();
    o2->weight(1, 0);

    (new LFl_Box(1, 0, 1, 1))->setMinimumSize(5, 0);

    o2 = new SizeButtons(2, 0, 1, 1, true);
    o2->callback(cb_ToolTab0Transform2_stub);

    o->end();
  }
  {
    Fl_Group* o = new TransformButtons(0, 1, 1, 1, 0);
    o->callback(cb_ToolTab0Transform_stub);
    o->hide();
  }
  {
    Fl_Group* o = new ToolsButtons(0, 1, 1, 1);
    o->callback(cb_ToolTab0Transform_stub);
    o->hide();
  }

  {
    startGoalTab = new layouter_c(0, 1, 1, 1);
    startGoalTab->label("Start/Goal");
    startGoalTab->pitch(5);
    startGoalTab->hide();

    sgControls = new layouter_c(0, 0, 1, 1);
    sgControls->weight(1, 1);

    modeStart = new LFl_Radio_Button("Start", 0, 0, 1, 1);
    modeStart->tooltip(" Paint start labels onto the selected start/goal shape ");
    modeStart->value(1);
    modeStart->callback(cb_ToolTab0StartGoal_stub);
    modeGoal = new LFl_Radio_Button("Goal", 0, 1, 1, 1);
    modeGoal->tooltip(" Paint goal labels onto the selected start/goal shape ");
    modeGoal->callback(cb_ToolTab0StartGoal_stub);

    (new LFl_Box(1, 0, 1, 2))->setMinimumSize(20, 0);

    LFl_Hold_Browser * list = new LFl_Hold_Browser(2, 0, 1, 2);
    list->weight(3, 1);
    list->callback(cb_ToolTab0SgPiece_stub);
    pieceList = list;

    (new LFl_Box(3, 0, 1, 2))->setMinimumSize(8, 0);

    /* About 40% of the width: the piece chosen in the list. */
    piecePreview = new slidePiecePreview_c(4, 0, 1, 2);
    piecePreview->weight(5, 1);
    piecePreview->tooltip(" The piece chosen in the list ");

    sgControls->end();

    /* in the controls' place while another shape is chosen */
    sgInfo = new StackValidBar_c(0, 2, 1, 1);
    sgInfo->weight(1, 1);
    sgInfo->align(FL_ALIGN_INSIDE | FL_ALIGN_WRAP | FL_ALIGN_LEFT | FL_ALIGN_TOP);
    sgInfo->setMessage("This tab is only used to define the Start and Goal position of pieces "
                       "for a Start/Goal specific piece.", FL_BACKGROUND_COLOR, FL_FOREGROUND_COLOR);
    sgInfo->labelfont(FL_HELVETICA);
    sgInfo->callback(cb_ToolTab0Relayout_stub);
    sgInfo->hide();

    plainTabColor = startGoalTab->selection_color();
    sgTabColor = plainTabColor;
    plainGroupColor = selection_color();

    startGoalTab->end();
  }

  end();
  /* Taken out until a sliding puzzle needs it (setVoxelSpace). */
  remove(startGoalTab);
  shownTab = value();
}

ToolTab_0::~ToolTab_0(void) {
  if (startGoalTab && startGoalTab->parent() != this)
    delete startGoalTab;
}

void ToolTab_0::cb_startGoalMode(void) {
  modeCallbackPending = true;
  do_callback();
}

void ToolTab_0::selectPiece(unsigned int shapeId) {
  selectedSgPiece = shapeId;
  if (piecePreview)
    piecePreview->setPiece(puzzle, shapeId);
  modeCallbackPending = true;
  do_callback();
}

int ToolTab_0::startGoalMode(void) const {
  return (modeGoal && modeGoal->value()) ? 1 : 0;
}

bool ToolTab_0::labelEditActive(void) {
  return startGoalTab && value() == startGoalTab;
}

bool ToolTab_0::consumeTabChange(void) {
  Fl_Widget * now = value();
  if (now == shownTab)
    return false;
  shownTab = now;
  return true;
}

unsigned int ToolTab_0::selectedPiece(void) const {
  return selectedSgPiece;
}

bool ToolTab_0::takeModeCallback(void) {
  bool pending = modeCallbackPending;
  modeCallbackPending = false;
  return pending;
}

void ToolTab_0::refreshPieceList(void) {
  if (!pieceList)
    return;

  bool sliding = puzzle && puzzle->getGridType()->getType() == gridType_c::GT_SLIDING;
  bool editing = sliding && puzzle && shape < puzzle->getNumberOfShapes() &&
                 sliding::isStartGoalShape(puzzle->getShape(shape));

  if (modeStart) {
    if (editing) { modeStart->activate(); modeGoal->activate(); }
    else { modeStart->deactivate(); modeGoal->deactivate(); }
  }
  if (editing) pieceList->activate();
  else pieceList->deactivate();

  unsigned int keep = selectedSgPiece;
  pieceList->callback((Fl_Callback1*)0);
  pieceList->clear();

  int line = 0;
  int selectLine = 1;
  if (puzzle) {
    for (unsigned int i = 0; i < puzzle->getNumberOfShapes(); i++) {
      const voxel_c * v = puzzle->getShape(i);
      if (sliding::isStartGoalShape(v) || sliding::isHiddenSlidingShape(v))
        continue;
      char txt[120];
      if (v->getName().length())
        snprintf(txt, sizeof(txt), "S%u - %s", i + 1, v->getName().c_str());
      else
        snprintf(txt, sizeof(txt), "S%u", i + 1);
      pieceList->add(txt, (void*)(uintptr_t)i);
      line++;
      if (i == keep)
        selectLine = line;
    }
  }

  if (pieceList->size() > 0) {
    pieceList->select(selectLine);
    selectedSgPiece = (unsigned int)(uintptr_t)pieceList->data(pieceList->value());
  } else {
    selectedSgPiece = (unsigned int)-1;
  }
  pieceList->callback(cb_ToolTab0SgPiece_stub);
  pieceList->redraw();
  if (piecePreview) {
    piecePreview->setPiece(puzzle, selectedSgPiece);
    if (editing) piecePreview->activate();
    else piecePreview->deactivate();
  }
  refreshStartGoalState();
}

void ToolTab_0::refreshStartGoalState(void) {
  if (!sgControls)
    return;

  const bool sliding = puzzle && puzzle->getGridType()->getType() == gridType_c::GT_SLIDING;
  const bool editing = sliding && shape < puzzle->getNumberOfShapes() &&
                       sliding::isStartGoalShape(puzzle->getShape(shape));

  bool relayout = false;
  auto setVis = [&relayout](Fl_Widget * w, bool on) {
    if ((w->visible() != 0) == on)
      return;
    if (on) w->show(); else w->hide();
    relayout = true;
  };
  setVis(sgControls, editing);
  setVis(sgInfo, !editing);

  /* Whether it can be solved is said under the 3D view; the tab's colour
   * says it here. */
  if (editing) {
    const std::string err = sliding::startGoalError(*puzzle, shape);
    sgTabColor = err.empty() ? fl_rgb_color(190, 235, 190) : fl_rgb_color(245, 190, 190);
  } else {
    sgTabColor = plainTabColor;
  }
  startGoalTab->selection_color(sgTabColor);

  if (relayout)
    relayoutAround(this);
  redraw();
}

/* The selected tab takes the colour of the group, the others their own:
 * the Start/Goal tab shows whether it is valid either way. */
void ToolTab_0::draw(void) {
  selection_color(startGoalTab && value() == startGoalTab ? sgTabColor : plainGroupColor);
  ToolTab::draw();
}

/* The buttons that would give a piece more layers or move it along Z:
 * nudge along Z (4, 5), rotate about X and Y (6 to 9), double and triple
 * the shape (22, 23), and the grid scale (41). */
static void lockZButtons(Fl_Widget * w, bool lock) {
  if (LFlatButton_c * b = dynamic_cast<LFlatButton_c *>(w)) {
    const long task = b->argument();
    if ((task >= 4 && task <= 9) || task == 22 || task == 23 || task == 41) {
      if (lock) b->deactivate();
      else b->activate();
    }
    return;
  }
  if (Fl_Group * g = w->as_group())
    for (int i = 0; i < g->children(); i++)
      lockZButtons(g->child(i), lock);
}

void ToolTab_0::lockZ(bool lock) {
  changeSize->lockZ(lock);
  lockZButtons(this, lock);
}

void ToolTab_0::cb_size(void) {
  if (puzzle && shape < puzzle->getNumberOfShapes()) {

    resizeSpace(toAll->value(), changeSize, puzzle, shape);
    do_callback();
  }
}

void ToolTab_0::applyTask(voxel_c * space, long task) {
  switch(task) {
        case  0: space->translate( 1, 0, 0, 0); break;
        case  1: space->translate(-1, 0, 0, 0); break;
        case  2: space->translate( 0, 1, 0, 0); break;
        case  3: space->translate( 0,-1, 0, 0); break;
        case  4: space->translate( 0, 0, 1, 0); break;
        case  5: space->translate( 0, 0,-1, 0); break;
        case  7: space->transform(3); break;
        case  6: space->transform(1); break;
        case  9: space->transform(12); break;
        case  8: space->transform(4); break;
        case 11: space->transform(20); break;
        case 10: space->transform(16); break;
        case 12: space->transform(24); break;
        case 13: space->transform(34); break;
        case 14: space->transform(32); break;
        case 15: space->minimizePiece(); break;
        case 16: space->actionOnSpace(voxel_c::ACT_FIXED, true); break;
        case 17: space->actionOnSpace(voxel_c::ACT_FIXED, false); break;
        case 18: space->actionOnSpace(voxel_c::ACT_VARIABLE, true); break;
        case 19: space->actionOnSpace(voxel_c::ACT_VARIABLE, false); break;
        case 20: space->actionOnSpace(voxel_c::ACT_DECOLOR, true); break;
        case 21: space->actionOnSpace(voxel_c::ACT_DECOLOR, false); break;
        case 22: space->scale(2); break;
        case 23: space->scale(3); break;
        case 24: space->translate(- space->boundX1(), - space->boundY1(), - space->boundZ1(), 0); break;
        case 25:
                 {
                   // if the space is empty, don't do anything
                   if (space->boundX2() < space->boundX1())
                     break;

                   int fx = space->getX() - (space->boundX2()-space->boundX1()+1);
                   int fy = space->getY() - (space->boundY2()-space->boundY1()+1);
                   int fz = space->getZ() - (space->boundZ2()-space->boundZ1()+1);

                   if ((fx & 1) || (fy & 1) || (fz & 1)) {
                     space->resize(space->getX()+(fx&1), space->getY()+(fy&1), space->getZ()+(fz&1), 0);
                     fx += fx&1;
                     fy += fy&1;
                     fz += fz&1;
                   }
                   space->translate(fx/2 - space->boundX1(), fy/2 - space->boundY1(), fz/2 - space->boundZ1(), 0);
                 }
                 break;
        case 40: space->fillHoles(0); break;
        case 41: space->scale(5, true); break;
      }
}

void ToolTab_0::cb_transform(long task) {
  if (puzzle && shape < puzzle->getNumberOfShapes()) {

    int ss, se;

    if (toAll->value() && ((task == 15) || ((task >= 22) && (task <= 26)))) {
      ss = 0;
      se = puzzle->getNumberOfShapes();
    } else {
      ss = shape;
      se = shape+1;
    }

    if (task == 26) {

      unsigned char primes[] = {2, 3, 5, 7, 11, 13, 17, 19, 0};

      // special case for minimisation

      int prime = 0;

      while (primes[prime]) {

        bool canScale = true;

        for (int s = ss; s < se; s++)
          if (!puzzle->getShape(s)->scaleDown(primes[prime], false)) {
            canScale = false;
            break;
          }

        if (canScale) {
          for (int s = ss; s < se; s++)
            puzzle->getShape(s)->scaleDown(primes[prime], true);
        } else
          prime++;
      }

      for (int s = ss; s < se; s++)
        puzzle->getShape(s)->initHotspot();

      do_callback(this, user_data());

      return;
    }

    for (int s = ss; s < se; s++) {
      applyTask(puzzle->getShape(s), task);
      puzzle->getShape(s)->initHotspot();
    }

    do_callback(this, user_data());
  }
}





void ToolTab_1::setVoxelSpace(puzzle_c * puz, unsigned int sh) {
  puzzle = puz;
  shape = sh;

  bt_assert(!puzzle || (puzzle->getGridType()->getType() == gridType_c::GT_TRIANGULAR_PRISM));

  if (puzzle && shape < puzzle->getNumberOfShapes())
    changeSize->setXYZ(puzzle->getShape(shape)->getX(),
        puzzle->getShape(shape)->getY(),
        puzzle->getShape(shape)->getZ());
  else
    changeSize->setXYZ(0, 0, 0);
}

static void cb_ToolTab1Size_stub(Fl_Widget* o, long /*v*/) { static_cast<ToolTab_1*>(o->parent()->parent()->parent())->cb_size(); }
static void cb_ToolTab1Transform_stub(Fl_Widget* o, long v) { static_cast<ToolTab_1*>(o->parent())->cb_transform(v); }
static void cb_ToolTab1Transform2_stub(Fl_Widget* o, long v) { static_cast<ToolTab_1*>(o->parent()->parent())->cb_transform(v); }

ToolTab_1::ToolTab_1(int x, int y, int w, int h) : ToolTab(x, y, w, h) {

  {
    layouter_c * o = new layouter_c(0, 1, 1, 1);
    o->label("Size");
    o->pitch(5);

    layouter_c *o2 = new layouter_c(0, 0, 1, 1);

    changeSize = new ChangeSize(0, 1, 1, 1);
    changeSize->callback(cb_ToolTab1Size_stub);

    toAll = new LFl_Check_Button("Apply to All Shapes", 0, 0, 1, 1);
    toAll->tooltip(" If this is active, all operations (including transformations and constrains are done to all shapes ");
    toAll->clear_visible_focus();
    toAll->stretchHCenter();

    o2->end();
    o2->weight(1, 0);

    (new LFl_Box(1, 0, 1, 1))->setMinimumSize(5, 0);

    o2 = new SizeButtons(2, 0, 1, 1, true);
    o2->callback(cb_ToolTab1Transform2_stub);

    o->end();
  }
  {
    Fl_Group* o = new TransformButtons(0, 1, 1, 1, 1);
    o->callback(cb_ToolTab1Transform_stub);
    o->hide();
  }
  {
    Fl_Group* o = new ToolsButtons(0, 1, 1, 1);
    o->callback(cb_ToolTab1Transform_stub);
    o->hide();
  }

  end();
}

void ToolTab_1::cb_size(void) {
  if (puzzle && shape < puzzle->getNumberOfShapes()) {

    resizeSpace(toAll->value(), changeSize, puzzle, shape);
    do_callback();
  }
}

void ToolTab_1::applyTask(voxel_c * space, long task) {
  switch(task) {
        case  0: space->translate( 1, 1, 0, 0); break;
        case  1: space->translate(-1, 1, 0, 0); break;
        case  2: space->translate( 2, 0, 0, 0); break;
        case  3: space->translate(-2, 0, 0, 0); break;
        case  4: space->translate( 0, 0, 1, 0); break;
        case  5: space->translate( 0, 0,-1, 0); break;
        case 28: space->translate(-1,-1, 0, 0); break;
        case 27: space->translate( 1,-1, 0, 0); break;
        case  6: space->transform(9); break;
        case  9: space->transform(6); break;
        case 11: space->transform(1); break;
        case 10: space->transform(5); break;
        case 12: space->transform(12); break;
        case 13: space->transform(15); break;
        case 14: space->transform(18); break;
        case 15: space->minimizePiece(); break;
        case 16: space->actionOnSpace(voxel_c::ACT_FIXED, true); break;
        case 17: space->actionOnSpace(voxel_c::ACT_FIXED, false); break;
        case 18: space->actionOnSpace(voxel_c::ACT_VARIABLE, true); break;
        case 19: space->actionOnSpace(voxel_c::ACT_VARIABLE, false); break;
        case 20: space->actionOnSpace(voxel_c::ACT_DECOLOR, true); break;
        case 21: space->actionOnSpace(voxel_c::ACT_DECOLOR, false); break;
        case 22: space->scale(2); break;
        case 23: space->scale(3); break;
        case 24: {
                   int dx = -space->boundX1();
                   int dy = -space->boundY1();

                   // we need to make sure that we move by an even amount, otherwise
                   // the state toggles
                   if ((dx+dy) & 1) {
                     if (dx < 0)
                       dx++;
                     else
                       dy++;
                   }

                   space->translate(dx, dy, - space->boundZ1(), 0);
                 }
                 break;
        case 25:
                 {
                   // if the space is empty, don't do anything
                   if (space->boundX2() < space->boundX1())
                     break;

                   int fx = space->getX() - (space->boundX2()-space->boundX1()+1);
                   int fy = space->getY() - (space->boundY2()-space->boundY1()+1);
                   int fz = space->getZ() - (space->boundZ2()-space->boundZ1()+1);

                   if ((fx & 1) || (fy & 1) || (fz & 1)) {
                     space->resize(space->getX()+(fx&1), space->getY()+(fy&1), space->getZ()+(fz&1), 0);
                     fx += fx&1;
                     fy += fy&1;
                     fz += fz&1;
                   }
                   if (((fx/2 - space->boundX1()) + (fy/2 - space->boundY1())) & 1) {
                     space->resize(space->getX()+2, space->getY(), space->getZ(), 0);
                     fx+=2;
                   }
                   space->translate(fx/2 - space->boundX1(), fy/2 - space->boundY1(), fz/2 - space->boundZ1(), 0);
                 }
                 break;
        case 40: space->fillHoles(0); break;
        case 41: space->scale(5, true); break;
      }
}

void ToolTab_1::cb_transform(long task) {

  if (puzzle && shape < puzzle->getNumberOfShapes()) {

    int ss, se;

    if (toAll->value() && ((task == 15) || ((task >= 22) && (task <= 26)))) {
      ss = 0;
      se = puzzle->getNumberOfShapes();
    } else {
      ss = shape;
      se = shape+1;
    }

    if (task == 26) {

      bt_message("Sorry this is not yet implemented!");
      return;

    }

    for (int s = ss; s < se; s++) {
      applyTask(puzzle->getShape(s), task);
      puzzle->getShape(s)->initHotspot();
    }

    do_callback(this, user_data());
  }
}


void ToolTab_2::setVoxelSpace(puzzle_c * puz, unsigned int sh) {
  puzzle = puz;
  shape = sh;

  bt_assert(!puzzle || (puzzle->getGridType()->getType() == gridType_c::GT_SPHERES));

  if (puzzle && shape < puzzle->getNumberOfShapes())
    changeSize->setXYZ(puzzle->getShape(shape)->getX(),
        puzzle->getShape(shape)->getY(),
        puzzle->getShape(shape)->getZ());
  else
    changeSize->setXYZ(0, 0, 0);
}

static void cb_ToolTab2Size_stub(Fl_Widget* o, long /*v*/) { static_cast<ToolTab_2*>(o->parent()->parent()->parent())->cb_size(); }
static void cb_ToolTab2Transform_stub(Fl_Widget* o, long v) { static_cast<ToolTab_2*>(o->parent())->cb_transform(v); }
static void cb_ToolTab2Transform2_stub(Fl_Widget* o, long v) { static_cast<ToolTab_2*>(o->parent()->parent())->cb_transform(v); }

ToolTab_2::ToolTab_2(int x, int y, int w, int h) : ToolTab(x, y, w, h) {

  {
    layouter_c * o = new layouter_c(0, 1, 1, 1);
    o->label("Size");
    o->pitch(5);

    layouter_c *o2 = new layouter_c(0, 0, 1, 1);

    changeSize = new ChangeSize(0, 1, 1, 1);
    changeSize->callback(cb_ToolTab2Size_stub);

    toAll = new LFl_Check_Button("Apply to All Shapes", 0, 0, 1, 1);
    toAll->tooltip(" If this is active, all operations (including transformations and constrains are done to all shapes ");
    toAll->clear_visible_focus();
    toAll->stretchHCenter();

    o2->end();
    o2->weight(1, 0);

    (new LFl_Box(1, 0, 1, 1))->setMinimumSize(5, 0);

    o2 = new SizeButtons(2, 0, 1, 1, false);
    o2->callback(cb_ToolTab2Transform2_stub);

    o->end();
  }
  {
    Fl_Group* o = new TransformButtons(0, 1, 1, 1, 2);
    o->callback(cb_ToolTab2Transform_stub);
    o->hide();
  }
  {
    Fl_Group* o = new ToolsButtons(0, 1, 1, 1);
    o->callback(cb_ToolTab2Transform_stub);
    o->hide();
  }

  end();
}

void ToolTab_2::cb_size(void) {
  if (puzzle && shape < puzzle->getNumberOfShapes()) {

    resizeSpace(toAll->value(), changeSize, puzzle, shape);
    do_callback();
  }
}

void ToolTab_2::applyTask(voxel_c * space, long task) {
  switch(task) {
        case  0: space->translate( 1, 0, 1, 0); break;
        case  1: space->translate( 0, 1, 1, 0); break;
        case  2: space->translate(-1, 0, 1, 0); break;
        case  3: space->translate( 0,-1, 1, 0); break;

        case  4: space->translate( 1, 1, 0, 0); break;
        case  5: space->translate(-1, 1, 0, 0); break;
        case 27: space->translate(-1,-1, 0, 0); break;
        case 28: space->translate( 1,-1, 0, 0); break;

        case 29: space->translate( 1, 0,-1, 0); break;
        case 30: space->translate( 0, 1,-1, 0); break;
        case 31: space->translate(-1, 0,-1, 0); break;
        case 32: space->translate( 0,-1,-1, 0); break;

        case  7: space->transform(9); break;
        case  6: space->transform(14); break;
        case  9: space->transform(5); break;
        case  8: space->transform(16); break;
        case 11: space->transform(2); break;
        case 10: space->transform(6); break;
        case 12: space->transform(120); break;
        case 13: space->transform(124); break;
        case 14: space->transform(141); break;
        case 15: space->minimizePiece(); break;
        case 16: space->actionOnSpace(voxel_c::ACT_FIXED, true); break;
        case 17: space->actionOnSpace(voxel_c::ACT_FIXED, false); break;
        case 18: space->actionOnSpace(voxel_c::ACT_VARIABLE, true); break;
        case 19: space->actionOnSpace(voxel_c::ACT_VARIABLE, false); break;
        case 20: space->actionOnSpace(voxel_c::ACT_DECOLOR, true); break;
        case 21: space->actionOnSpace(voxel_c::ACT_DECOLOR, false); break;
        case 24: {
                   int dx = -space->boundX1();
                   int dy = -space->boundY1();
                   int dz = -space->boundZ1();

                   // we need to make sure that we move by an even amount, otherwise
                   // the state toggles
                   if ((dx+dy+dz) & 1) {
                     if (dx < 0)
                       dx++;
                     else if (dy < 0)
                       dy++;
                     else
                       dz++;
                   }

                   space->translate(dx, dy, dz, 0);
                 }
                 break;
        case 25:
                 {
                   // if the space is empty, don't do anything
                   if (space->boundX2() < space->boundX1())
                     break;

                   int fx = space->getX() - (space->boundX2()-space->boundX1()+1);
                   int fy = space->getY() - (space->boundY2()-space->boundY1()+1);
                   int fz = space->getZ() - (space->boundZ2()-space->boundZ1()+1);

                   if ((fx & 1) || (fy & 1) || (fz & 1)) {
                     space->resize(space->getX()+(fx&1), space->getY()+(fy&1), space->getZ()+(fz&1), 0);
                     fx += fx&1;
                     fy += fy&1;
                     fz += fz&1;
                   }
                   if (((fx/2 - space->boundX1()) + (fy/2 - space->boundY1()) + (fz/2 - space->boundZ1())) & 1) {
                     space->resize(space->getX()+2, space->getY(), space->getZ(), 0);
                     fx+=2;
                   }
                   space->translate(fx/2 - space->boundX1(), fy/2 - space->boundY1(), fz/2 - space->boundZ1(), 0);
                 }
                 break;
        case 40: space->fillHoles(0); break;
      }
}

void ToolTab_2::cb_transform(long task) {
  if (puzzle && shape < puzzle->getNumberOfShapes()) {

    int ss, se;

    if (toAll->value() && ((task == 15) || (task == 24) || (task == 25))) {
      ss = 0;
      se = puzzle->getNumberOfShapes();
    } else {
      ss = shape;
      se = shape+1;
    }

    for (int s = ss; s < se; s++) {
      applyTask(puzzle->getShape(s), task);
      puzzle->getShape(s)->initHotspot();
    }

    do_callback(this, user_data());
  }
}




void ToolTab_3::setVoxelSpace(puzzle_c * puz, unsigned int sh) {
  puzzle = puz;
  shape = sh;

  bt_assert(!puzzle || (puzzle->getGridType()->getType() == gridType_c::GT_RHOMBIC));

  if (puzzle && shape < puzzle->getNumberOfShapes())
    changeSize->setXYZ(
        (puzzle->getShape(shape)->getX()+4)/5,
        (puzzle->getShape(shape)->getY()+4)/5,
        (puzzle->getShape(shape)->getZ()+4)/5);
  else
    changeSize->setXYZ(0, 0, 0);
}

static void cb_ToolTab3Size_stub(Fl_Widget* o, long /*v*/) { static_cast<ToolTab_3*>(o->parent()->parent()->parent())->cb_size(); }
static void cb_ToolTab3Transform_stub(Fl_Widget* o, long v) { static_cast<ToolTab_3*>(o->parent())->cb_transform(v); }
static void cb_ToolTab3Transform2_stub(Fl_Widget* o, long v) { static_cast<ToolTab_3*>(o->parent()->parent())->cb_transform(v); }

ToolTab_3::ToolTab_3(int x, int y, int w, int h) : ToolTab(x, y, w, h) {

  {
    layouter_c * o = new layouter_c(0, 1, 1, 1);
    o->label("Size");
    o->pitch(5);

    layouter_c *o2 = new layouter_c(0, 0, 1, 1);

    changeSize = new ChangeSize(0, 1, 1, 1);
    changeSize->callback(cb_ToolTab3Size_stub);

    toAll = new LFl_Check_Button("Apply to All Shapes", 0, 0, 1, 1);
    toAll->tooltip(" If this is active, all operations (including transformations and constrains are done to all shapes ");
    toAll->clear_visible_focus();
    toAll->stretchHCenter();

    o2->end();
    o2->weight(1, 0);

    (new LFl_Box(1, 0, 1, 1))->setMinimumSize(5, 0);

    o2 = new SizeButtons(2, 0, 1, 1, true);
    o2->callback(cb_ToolTab3Transform2_stub);

    o->end();
  }
  {
    Fl_Group* o = new TransformButtons(0, 1, 1, 1, 0);
    o->callback(cb_ToolTab3Transform_stub);
    o->hide();
  }
  {
    Fl_Group* o = new ToolsButtons(0, 1, 1, 1);
    o->callback(cb_ToolTab3Transform_stub);
    o->hide();
  }

  end();
}

void ToolTab_3::cb_size(void) {
  if (puzzle && shape < puzzle->getNumberOfShapes()) {

    resizeSpace(toAll->value(), changeSize, puzzle, shape, 5);
    do_callback();
  }
}

void ToolTab_3::applyTask(voxel_c * space, long task) {
  switch(task) {
        case  0: space->translate( 5, 0, 0, 0); break;
        case  1: space->translate(-5, 0, 0, 0); break;
        case  2: space->translate( 0, 5, 0, 0); break;
        case  3: space->translate( 0,-5, 0, 0); break;
        case  4: space->translate( 0, 0, 5, 0); break;
        case  5: space->translate( 0, 0,-5, 0); break;
        case  7: space->transform(3); break;
        case  6: space->transform(1); break;
        case  9: space->transform(12); break;
        case  8: space->transform(4); break;
        case 11: space->transform(20); break;
        case 10: space->transform(16); break;
        case 12: space->transform(24); break;
        case 13: space->transform(34); break;
        case 14: space->transform(32); break;
        case 15: space->minimizePiece(); break;
        case 16: space->actionOnSpace(voxel_c::ACT_FIXED, true); break;
        case 17: space->actionOnSpace(voxel_c::ACT_FIXED, false); break;
        case 18: space->actionOnSpace(voxel_c::ACT_VARIABLE, true); break;
        case 19: space->actionOnSpace(voxel_c::ACT_VARIABLE, false); break;
        case 20: space->actionOnSpace(voxel_c::ACT_DECOLOR, true); break;
        case 21: space->actionOnSpace(voxel_c::ACT_DECOLOR, false); break;
        case 22: space->scale(2); break;
        case 23: space->scale(3); break;
        case 24: space->translate(- (space->boundX1()/5)*5, - (space->boundY1()/5)*5, - (space->boundZ1()/5)*5, 0); break;
        case 25:
                 {
                   // if the space is empty, don't do anything
                   if (space->boundX2() < space->boundX1())
                     break;

                   int fx = (space->getX() - (space->boundX2()-space->boundX1()+1))/2 - space->boundX1();
                   int fy = (space->getY() - (space->boundY2()-space->boundY1()+1))/2 - space->boundY1();
                   int fz = (space->getZ() - (space->boundZ2()-space->boundZ1()+1))/2 - space->boundZ1();

                   if (fx%5 <= 2) fx -= fx%5; else if (space->boundX2()+(5-fx%5) < space->getX()) fx += (5-fx%5);
                   if (fy%5 <= 2) fy -= fy%5; else if (space->boundY2()+(5-fy%5) < space->getY()) fy += (5-fy%5);
                   if (fz%5 <= 2) fz -= fz%5; else if (space->boundZ2()+(5-fz%5) < space->getZ()) fz += (5-fz%5);

                   space->translate(fx, fy, fz, 0);
                 }
                 break;
        case 26: bt_message("Sorry minimizing is not (yet) implemented for the rhombic grid!"); return;
        case 40: space->fillHoles(0); break;
        case 41: space->scale(7, true); break;
      }
}

void ToolTab_3::cb_transform(long task) {
  if (puzzle && shape < puzzle->getNumberOfShapes()) {

    int ss, se;

    if (toAll->value() && ((task == 15) || ((task >= 22) && (task <= 26)))) {
      ss = 0;
      se = puzzle->getNumberOfShapes();
    } else {
      ss = shape;
      se = shape+1;
    }

    for (int s = ss; s < se; s++) {
      applyTask(puzzle->getShape(s), task);
      puzzle->getShape(s)->initHotspot();
    }

    do_callback(this, user_data());
  }
}





void ToolTab_4::setVoxelSpace(puzzle_c * puz, unsigned int sh) {
  puzzle = puz;
  shape = sh;

  bt_assert(!puzzle || (puzzle->getGridType()->getType() == gridType_c::GT_TETRA_OCTA));

  if (puzzle && shape < puzzle->getNumberOfShapes())
    changeSize->setXYZ(
        (puzzle->getShape(shape)->getX()+2)/3,
        (puzzle->getShape(shape)->getY()+2)/3,
        (puzzle->getShape(shape)->getZ()+2)/3);
  else
    changeSize->setXYZ(0, 0, 0);
}

static void cb_ToolTab4Size_stub(Fl_Widget* o, long /*v*/) { static_cast<ToolTab_4*>(o->parent()->parent()->parent())->cb_size(); }
static void cb_ToolTab4Transform_stub(Fl_Widget* o, long v) { static_cast<ToolTab_4*>(o->parent())->cb_transform(v); }
static void cb_ToolTab4Transform2_stub(Fl_Widget* o, long v) { static_cast<ToolTab_4*>(o->parent()->parent())->cb_transform(v); }

ToolTab_4::ToolTab_4(int x, int y, int w, int h) : ToolTab(x, y, w, h) {

  {
    layouter_c * o = new layouter_c(0, 1, 1, 1);
    o->label("Size");
    o->pitch(3);

    layouter_c *o2 = new layouter_c(0, 0, 1, 1);

    changeSize = new ChangeSize(0, 1, 1, 1);
    changeSize->callback(cb_ToolTab4Size_stub);

    toAll = new LFl_Check_Button("Apply to All Shapes", 0, 0, 1, 1);
    toAll->tooltip(" If this is active, all operations (including transformations and constrains are done to all shapes ");
    toAll->clear_visible_focus();
    toAll->stretchHCenter();

    o2->end();
    o2->weight(1, 0);

    (new LFl_Box(1, 0, 1, 1))->setMinimumSize(5, 0);

    o2 = new SizeButtons(2, 0, 1, 1, true);
    o2->callback(cb_ToolTab4Transform2_stub);

    o->end();
  }
  {
    Fl_Group* o = new TransformButtons(0, 1, 1, 1, 0);
    o->callback(cb_ToolTab4Transform_stub);
    o->hide();
  }
  {
    Fl_Group* o = new ToolsButtons(0, 1, 1, 1);
    o->callback(cb_ToolTab4Transform_stub);
    o->hide();
  }

  end();
}

void ToolTab_4::cb_size(void) {
  if (puzzle && shape < puzzle->getNumberOfShapes()) {

    resizeSpace(toAll->value(), changeSize, puzzle, shape, 3);
    do_callback();
  }
}

void ToolTab_4::applyTask(voxel_c * space, long task) {
  switch(task) {
        case  0: space->translate( 6, 0, 0, 0); break;
        case  1: space->translate(-6, 0, 0, 0); break;
        case  2: space->translate( 0, 6, 0, 0); break;
        case  3: space->translate( 0,-6, 0, 0); break;
        case  4: space->translate( 0, 0, 6, 0); break;
        case  5: space->translate( 0, 0,-6, 0); break;
        case  7: space->transform(3); break;
        case  6: space->transform(1); break;
        case  9: space->transform(12); break;
        case  8: space->transform(4); break;
        case 11: space->transform(20); break;
        case 10: space->transform(16); break;
        case 12: space->transform(24); break;
        case 13: space->transform(34); break;
        case 14: space->transform(32); break;
        case 15: space->minimizePiece(); break;
        case 16: space->actionOnSpace(voxel_c::ACT_FIXED, true); break;
        case 17: space->actionOnSpace(voxel_c::ACT_FIXED, false); break;
        case 18: space->actionOnSpace(voxel_c::ACT_VARIABLE, true); break;
        case 19: space->actionOnSpace(voxel_c::ACT_VARIABLE, false); break;
        case 20: space->actionOnSpace(voxel_c::ACT_DECOLOR, true); break;
        case 21: space->actionOnSpace(voxel_c::ACT_DECOLOR, false); break;
        case 22: space->scale(2); break;
        case 23: space->scale(3); break;
        case 24: space->translate(- (space->boundX1()/6)*6, - (space->boundY1()/6)*6, - (space->boundZ1()/6)*6, 0); break;
        case 25:
                 {
                   // if the space is empty, don't do anything
                   if (space->boundX2() < space->boundX1())
                     break;

                   int fx = (space->getX() - (space->boundX2()-space->boundX1()+1))/2 - space->boundX1();
                   int fy = (space->getY() - (space->boundY2()-space->boundY1()+1))/2 - space->boundY1();
                   int fz = (space->getZ() - (space->boundZ2()-space->boundZ1()+1))/2 - space->boundZ1();

                   if (fx%6 <= 3) fx -= fx%6; else if (space->boundX2()+(6-fx%6) < space->getX()) fx += (6-fx%6);
                   if (fy%6 <= 3) fy -= fy%6; else if (space->boundY2()+(6-fy%6) < space->getY()) fy += (6-fy%6);
                   if (fz%6 <= 3) fz -= fz%6; else if (space->boundZ2()+(6-fz%6) < space->getZ()) fz += (6-fz%6);

                   space->translate(fx, fy, fz, 0);
                 }
                 break;
        case 26: bt_message("Sorry minimizing is not (yet) implemented for the rhombic grid!"); return;
        case 40: space->fillHoles(0); break;
      }
}

void ToolTab_4::cb_transform(long task) {
  if (puzzle && shape < puzzle->getNumberOfShapes()) {

    int ss, se;

    if (toAll->value() && ((task == 15) || ((task >= 22) && (task <= 26)))) {
      ss = 0;
      se = puzzle->getNumberOfShapes();
    } else {
      ss = shape;
      se = shape+1;
    }

    for (int s = ss; s < se; s++) {
      applyTask(puzzle->getShape(s), task);
      puzzle->getShape(s)->initHotspot();
    }

    do_callback(this, user_data());
  }
}






void ToolTab::previewTransform(long task, bool on) {
  ToolTabContainer *c = dynamic_cast<ToolTabContainer*>(parent());
  if (!c)
    return;
  if (!on || !puzzle || shape >= puzzle->getNumberOfShapes()) {
    c->emitPreview(0, shape);
    return;
  }
  voxel_c *v = puzzle->getGridType()->getVoxel(puzzle->getShape(shape));
  applyTask(v, task);
  v->initHotspot();
  c->emitPreview(v, shape);
}

static void cb_ToolTabContainer_stub(Fl_Widget* /*o*/, void*v) {
  ToolTabContainer *vv = (ToolTabContainer*)v;
  vv->do_callback(vv, vv->user_data());
}

ToolTabContainer::ToolTabContainer(int x, int y, int w, int h, const guiGridType_c * ggt)
  : layouter_c(x, y, w, h), previewHandler(0), previewUser(0), delayedClearShape(0) {
  tt = ggt->getToolTab(0, 0, 1, 1);
  tt->callback(cb_ToolTabContainer_stub, this);
  end();
}

void ToolTabContainer::previewClearTimeout(void *v) {
  ToolTabContainer *c = (ToolTabContainer*)v;
  if (c->previewHandler)
    c->previewHandler(c->previewUser, 0, c->delayedClearShape);
}

void ToolTabContainer::emitPreview(voxel_c *preview, unsigned int shapeNum) {
  Fl::remove_timeout(previewClearTimeout, this);
  if (!preview) {
    delayedClearShape = shapeNum;
    Fl::add_timeout(0.06, previewClearTimeout, this);
    return;
  }
  if (previewHandler)
    previewHandler(previewUser, preview, shapeNum);
  else
    delete preview;
}

void ToolTabContainer::newGridType(const guiGridType_c * ggt) {

  remove(tt);
  delete tt;
  tt = ggt->getToolTab(0, 0, 1, 1);
  tt->callback(cb_ToolTabContainer_stub, this);
  add(tt);
  resize(x(), y(), w(), h());
}


void ToolTab_Sliding::setVoxelSpace(puzzle_c * puz, unsigned int sh) {
  puzzle = puz;
  shape = sh;

  bt_assert(!puzzle || (puzzle->getGridType()->getType() == gridType_c::GT_SLIDING));

  if (puzzle && shape < puzzle->getNumberOfShapes())
    changeSize->setXYZ(puzzle->getShape(shape)->getX(),
        puzzle->getShape(shape)->getY(),
        1);
  else
    changeSize->setXYZ(0, 0, 1);
}

static void cb_ToolTabSlidingSize_stub(Fl_Widget* o, long /*v*/) {
  static_cast<ToolTab_Sliding*>(o->parent()->parent()->parent())->cb_size();
}
static void cb_ToolTabSlidingMode_stub(Fl_Widget* o, long /*v*/) {
  Fl_Widget *p = o->parent();
  while (p && !dynamic_cast<ToolTab_Sliding*>(p))
    p = p->parent();
  if (p)
    static_cast<ToolTab_Sliding*>(p)->cb_mode();
}

ToolTab_Sliding::ToolTab_Sliding(int x, int y, int w, int h) : ToolTab(x, y, w, h) {

  {
    layouter_c * o = new layouter_c(0, 1, 1, 1);
    o->label("Size");
    o->pitch(5);

    layouter_c *o2 = new layouter_c(0, 0, 1, 1);

    changeSize = new ChangeSize(0, 1, 1, 1);
    changeSize->callback(cb_ToolTabSlidingSize_stub);

    toAll = new LFl_Check_Button("Apply to All Shapes", 0, 0, 1, 1);
    toAll->tooltip(" If this is active, size changes apply to all shapes ");
    toAll->clear_visible_focus();
    toAll->stretchHCenter();

    o2->end();
    o->end();
  }

  {
    layouter_c * o = new layouter_c(0, 2, 1, 1);
    o->label("Tray edit");
    o->pitch(5);

    modeWalls = new LFl_Radio_Button("Walls", 0, 0, 1, 1);
    modeWalls->tooltip(" Toggle floor and wall cells on the tray ");
    modeWalls->value(1);
    modeWalls->callback(cb_ToolTabSlidingMode_stub);

    modeStart = new LFl_Radio_Button("Start", 0, 1, 1, 1);
    modeStart->tooltip(" Place or remove a piece's starting position on the tray ");
    modeStart->callback(cb_ToolTabSlidingMode_stub);

    modeGoal = new LFl_Radio_Button("Goal", 0, 2, 1, 1);
    modeGoal->tooltip(" Place or clear a piece's goal position on the tray ");
    modeGoal->callback(cb_ToolTabSlidingMode_stub);

    o->end();
  }

  end();
}

void ToolTab_Sliding::cb_size(void) {
  if (!puzzle || shape >= puzzle->getNumberOfShapes())
    return;

  /* Sliding is always one layer deep. */
  int nx = changeSize->getX();
  int ny = changeSize->getY();
  if (nx < 1) nx = 1;
  if (ny < 1) ny = 1;

  if (toAll->value()) {
    int dx = nx - (int)puzzle->getShape(shape)->getX();
    int dy = ny - (int)puzzle->getShape(shape)->getY();
    if (dx < 0) dx = 0;
    if (dy < 0) dy = 0;
    for (unsigned int s = 0; s < puzzle->getNumberOfShapes(); s++) {
      int sx = (int)puzzle->getShape(s)->getX() + dx;
      int sy = (int)puzzle->getShape(s)->getY() + dy;
      if (sx < 1) sx = 1;
      if (sy < 1) sy = 1;
      puzzle->getShape(s)->resize(sx, sy, 1, 0);
    }
  } else {
    puzzle->getShape(shape)->resize(nx, ny, 1, 0);
  }
  changeSize->setXYZ(nx, ny, 1);
  do_callback();
}

void ToolTab_Sliding::cb_mode(void) {
  /* Do not pass getMode() as the long argument: Fl_Widget::do_callback(widget, long)
   * feeds that value to the container stub as void* user_data, which crashes when
   * the stub casts it back to ToolTabContainer*. The mode is read via getMode()
   * from the main-window callback instead. */
  do_callback();
}

int ToolTab_Sliding::getMode(void) const {
  if (modeStart->value())
    return MODE_START;
  if (modeGoal->value())
    return MODE_GOAL;
  return MODE_WALLS;
}

void ToolTab_Sliding::applyTask(voxel_c * /*space*/, long /*task*/) {
}


