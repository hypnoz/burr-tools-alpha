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

#include "gridtypegui.h"

#include "guigridtype.h"
#include "Layouter.h"
#include "tutorials.h"

#include "../lib/gridtype.h"

#include <FL/Fl_Browser_.H>
#include <FL/Fl_Image.H>

#include <algorithm>
#include <cstring>

gridTypeGui_0_c::gridTypeGui_0_c(int /*x*/, int /*y*/, int /*w*/, int /*h*/, gridType_c * /*gt*/) {
  end();
}

gridTypeGui_1_c::gridTypeGui_1_c(int x, int y, int w, int h, gridType_c * /*gt*/) {
  new LFl_Box("There are no parameters for this space grid.", x, y, w, h);

  end();
}

gridTypeGui_2_c::gridTypeGui_2_c(int x, int y, int w, int h, gridType_c * /*gt*/) {

  new LFl_Box("There are no parameters for this space grid.", x, y, w, h);

  end();
}


class gridTypeInfos_c {
  public:

    std::unique_ptr<gridType_c> gt;
    std::unique_ptr<guiGridType_c> ggt;
    LFl_Radio_Button * btn{nullptr};
    gridTypeGui_c * gui{nullptr};

    gridTypeInfos_c(std::unique_ptr<gridType_c> g) : gt(std::move(g)), ggt(std::make_unique<guiGridType_c>(gt.get())) {}
};

static void cb_WindowButton_stub(Fl_Widget * /*o*/, void *v) { ((Fl_Double_Window*)(v))->hide(); }

gridTypeParameterWindow_c::gridTypeParameterWindow_c(guiGridType_c * ggt) : LFl_Double_Window(false) {
  label("Set parameters for grid type");

  ggt->getConfigurationDialog(0, 0, 1, 1);

  LFl_Button * b = new LFl_Button("Close", 0, 1);
  b->pitch(7);
  b->callback(cb_WindowButton_stub, this);
}

static void cb_gridTypeSelectorSelect_stub(Fl_Widget * /*o*/, void *v) { ((gridTypeSelectorWindow_c*)(v))->select_cb(); }
void gridTypeSelectorWindow_c::select_cb(void) {
  for (unsigned int i = 0; i < gti.size(); i++)
    if (gti[i]->btn->value()) {
      current = i;
      showInfo();
    }
}

void gridTypeSelectorWindow_c::select(unsigned int i) {
  if (i >= gti.size())
    return;
  gti[i]->btn->setonly();
  current = i;
  showInfo();
}

/* The description, warnings and example of the selected grid, as one text. */
void gridTypeSelectorWindow_c::showInfo(void) {
  const gridType_c::gridType type = gti[current]->gt->getType();
  /* The picture fits under the text, without a scroll bar: the height
   * limits most pictures, the width only the wide Stacking one. */
  const std::string html = tutorials::selectorHtml(type, info->textsize(), 560, 260);
  info->value(html.c_str());
  info->topline(0);
}

/* FLTK keeps a widget's argument in the same place as its user data, so
 * each button has a callback of its own. */
static void cb_gridTypeSelectorOk_stub(Fl_Widget * /*o*/, void *v) {
  static_cast<gridTypeSelectorWindow_c*>(v)->finish(gridTypeSelectorWindow_c::SEL_OK);
}
static void cb_gridTypeSelectorCancel_stub(Fl_Widget * /*o*/, void *v) {
  static_cast<gridTypeSelectorWindow_c*>(v)->finish(gridTypeSelectorWindow_c::SEL_CANCEL);
}
static void cb_gridTypeSelectorOpen_stub(Fl_Widget * /*o*/, void *v) {
  static_cast<gridTypeSelectorWindow_c*>(v)->finish(gridTypeSelectorWindow_c::SEL_OPEN);
}
static void cb_gridTypeSelectorTutorial_stub(Fl_Widget * /*o*/, void *v) {
  static_cast<gridTypeSelectorWindow_c*>(v)->finish(gridTypeSelectorWindow_c::SEL_TUTORIAL);
}

/* The "open file" link of the example: close the window to open it.
 * Fl_Help_View asks here for its images too, which stay as they are. */
static const char * cb_gridTypeSelectorLink_stub(Fl_Widget * w, const char * uri) {
  const size_t n = std::strlen(EXAMPLE_LINK);
  if (!uri || std::strncmp(uri, EXAMPLE_LINK, n) != 0)
    return uri;
  if (gridTypeSelectorWindow_c * win = dynamic_cast<gridTypeSelectorWindow_c *>(w->window()))
    win->openExample(uri + n);
  return nullptr;
}

void gridTypeSelectorWindow_c::openExample(const char * file) {
  example = file;
  finish(SEL_EXAMPLE);
}

gridTypeSelectorWindow_c::gridTypeSelectorWindow_c(void) : LFl_Double_Window(false), current(0), info(nullptr), res(SEL_CANCEL) {

  /* for each grid type available we need to create an instance
   * here and put it into a gridtypeinfo class and into the
   * vector. This vector will be later on the one
   * with all required information
   */
  /* Display order. Sliding stays immediately after Brick. The enum value
   * itself is unchanged, so saved grid-type numbers do not shift. */
  std::vector<int> order;
  order.push_back(gridType_c::GT_BRICKS);
  order.push_back(gridType_c::GT_SLIDING);
  order.push_back(gridType_c::GT_STACKING);
  for (int i = 0; i < gridType_c::GT_NUM_GRIDS; i++)
    if (i != gridType_c::GT_BRICKS && i != gridType_c::GT_SLIDING &&
        i != gridType_c::GT_STACKING)
      order.push_back(i);

  for (int i : order)
    gti.push_back(std::make_unique<gridTypeInfos_c>(std::make_unique<gridType_c>(gridType_c::gridType(i))));

  /* from here on the code should not need changes when new grid types are added */

  label("Select space grid");

  /* Text two points larger than the rest of the program, and room around
   * everything: a margin at the window's edge, gaps between the parts. */
  const int textSize = FL_NORMAL_SIZE + 2;
  const int MARGIN = 20;
  const int GAP = 16;
  (new LFl_Box(0, 0))->setMinimumSize(MARGIN, MARGIN);
  (new LFl_Box(4, 4))->setMinimumSize(MARGIN, MARGIN);

  /* the grid types to choose from */
  {
    LFl_Frame * fr = new LFl_Frame(1, 1, 1, 1);

    for (unsigned int i = 0; i < gti.size(); i++) {
      gti[i]->btn = new LFl_Radio_Button(tutorials::gridName(gti[i]->gt->getType()), 0, i);
      gti[i]->btn->labelsize(textSize);
      gti[i]->btn->pitch(8);
      gti[i]->btn->callback(cb_gridTypeSelectorSelect_stub, this);
      if (i == 0)
        gti[i]->btn->set();
    }

    (new LFl_Box(0, gti.size()))->weight(0, 100);

    fr->end();

    int listW = 0, listH = 0;
    fr->getMinSize(&listW, &listH);
    fr->setMinimumSize(listW + 24, 0);
  }

  (new LFl_Box(2, 1))->setMinimumSize(GAP, 0);

  /* what the selected grid is: one text, no frame of its own */
  info = new LFl_Help_View(3, 1, 1, 1);
  info->textfont(FL_HELVETICA);
  info->textsize(textSize);
  info->box(FL_FLAT_BOX);
  info->color(FL_BACKGROUND_COLOR);
  info->textcolor(FL_FOREGROUND_COLOR);
  info->setMinimumSize(600, 600);
  info->weight(1, 1);
  info->link(cb_gridTypeSelectorLink_stub);

  (new LFl_Box(1, 2))->setMinimumSize(0, GAP);

  /* now the buttons */
  {
    layouter_c * l = new layouter_c(1, 3, 3, 1);

    struct button_c {
      const char * text;
      result_e result;
      Fl_Callback * cb;
    };
    const button_c buttons[] = {
      {"OK", SEL_OK, cb_gridTypeSelectorOk_stub},
      {"Cancel", SEL_CANCEL, cb_gridTypeSelectorCancel_stub},
      {"Open File...", SEL_OPEN, cb_gridTypeSelectorOpen_stub},
      {"Tutorial", SEL_TUTORIAL, cb_gridTypeSelectorTutorial_stub},
    };
    int bw = 0, bh = 0;
    std::vector<LFl_Button *> made;
    int x = 0;
    for (const button_c & b : buttons) {
      /* a gap after Cancel: the last two do something else than choose */
      if (b.result == SEL_OPEN)
        (new LFl_Box(x++, 0))->setMinimumSize(3 * GAP, 0);
      LFl_Button * btn = new LFl_Button(b.text, x++, 0);
      btn->labelsize(textSize);
      btn->pitch(4);
      btn->stretchVCenter();
      btn->callback(b.cb, this);
      if (b.result == SEL_CANCEL)
        btn->shortcut(FL_Escape);
      int w = 0, h = 0;
      btn->getMinSize(&w, &h);
      bw = std::max(bw, w + 36);
      bh = std::max(bh, h + 8);
      made.push_back(btn);
    }
    for (LFl_Button * b : made) {
      b->setMinimumSize((unsigned)bw, (unsigned)bh);
      b->weight(0, 0);
    }

    (new LFl_Box(x, 0))->weight(1, 0);

    l->end();
  }

  current = 0;
  showInfo();
}

gridTypeSelectorWindow_c::~gridTypeSelectorWindow_c(void) = default;

void gridTypeSelectorWindow_c::finish(result_e r) {
  res = r;
  hide();
}

std::unique_ptr<gridType_c> gridTypeSelectorWindow_c::getGridType(void) {
  return std::move(gti[current]->gt);
}
