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

#include "../lib/gridtype.h"

#include <FL/Fl_Browser_.H>
#include <FL/Fl_Image.H>

gridTypeGui_0_c::gridTypeGui_0_c(int /*x*/, int /*y*/, int /*w*/, int /*h*/, gridType_c * /*gt*/) {
  end();
}

static const char * gridTypeDescription(gridType_c::gridType type) {
  switch (type) {
    case gridType_c::GT_BRICKS:
      return "Pieces are cubes.\n"
             "This is the usual interlocking burr: find how the\n"
             "pieces fill the result and how they come apart.";
    case gridType_c::GT_TRIANGULAR_PRISM:
      return "Like Brick, with a different cell shape.\n"
             "Each cell is a triangular prism. Layers are packed\n"
             "triangles, half pointing up and half pointing down.";
    case gridType_c::GT_SPHERES:
      return "Like Brick, with a different cell shape.\n"
             "Each cell is a sphere. The spheres sit in a tight\n"
             "three-dimensional packing.";
    case gridType_c::GT_RHOMBIC:
      return "Like Brick, with a different cell shape.\n"
             "Each cell is a small tetrahedron cut from a cube.\n"
             "Together they build rhombic dodecahedra.";
    case gridType_c::GT_TETRA_OCTA:
      return "Like Brick, with a different cell shape.\n"
             "The cells are tetrahedra and octahedra that fill\n"
             "space together. Pieces use both shapes.";
    case gridType_c::GT_SLIDING:
      return "Pieces are flat shapes on a shared floor.\n"
             "Mark a start cell and a goal cell for each piece.\n"
             "The solver slides them from start to goal.";
    case gridType_c::GT_STACKING:
      return "Pieces are flat discs on vertical rods.\n"
             "Move the top disc from one rod to another, as in\n"
             "Tower of Hanoi, until the stacks match the goal.";
    default:
      return "";
  }
}

gridTypeGui_1_c::gridTypeGui_1_c(int x, int y, int w, int h, gridType_c * /*gt*/) {
  new LFl_Box("There are no parameters for this space grid!\n"
      "This space grid also has no disassembler (yet)", x, y, w, h);

  end();
}

gridTypeGui_2_c::gridTypeGui_2_c(int x, int y, int w, int h, gridType_c * /*gt*/) {

  new LFl_Box("There are no parameters for this space grid!\n"
      "This space grid has no disassembler", x, y, w, h);

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
static void cb_gridTypeSelectorOk_stub(Fl_Widget * /*o*/, void *v) { ((gridTypeSelectorWindow_c*)(v))->ok_cb(); }

gridTypeParameterWindow_c::gridTypeParameterWindow_c(guiGridType_c * ggt) : LFl_Double_Window(false) {
  label("Set parameters for grid type");

  ggt->getConfigurationDialog(0, 0, 1, 1);

  LFl_Button * b = new LFl_Button("Close", 0, 1);
  b->pitch(7);
  b->callback(cb_WindowButton_stub, this);
}

static void cb_gridTypeSelectorSelect_stub(Fl_Widget * /*o*/, void *v) { ((gridTypeSelectorWindow_c*)(v))->select_cb(); }
void gridTypeSelectorWindow_c::select_cb(void) {
  for (unsigned int i = 0; i < gti.size(); i++) {
    if (gti[i]->btn->value()) {

      gti[current]->gui->hide();
      current = i;
      gti[current]->gui->show();

      if (typeDescription)
        typeDescription->copy_label(gridTypeDescription(gti[current]->gt->getType()));

      /* Brick-like types have no parameters, so the frame under the
       * description would be an empty box. */
      if (parameterFrame) {
        if (gti[current]->gui->children() > 0)
          parameterFrame->show();
        else
          parameterFrame->hide();
      }

      if (layouter_c * root = dynamic_cast<layouter_c*>(resizable())) {
        root->invalidateMinSize();
        root->resize(root->x(), root->y(), root->w(), root->h());
      }
      redraw();
    }
  }
}

gridTypeSelectorWindow_c::gridTypeSelectorWindow_c(void) : LFl_Double_Window(false), typeDescription(0), parameterFrame(0), okPressed(false) {

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

  LFl_Frame *fr;

  /* first the selector for all grid types */
  {
    fr = new LFl_Frame(0, 0, 1, 2);

    for (unsigned int i = 0; i < gti.size(); i++) {
      gti[i]->btn = new LFl_Radio_Button(gti[i]->ggt->getName(), 0, i);
      gti[i]->btn->callback(cb_gridTypeSelectorSelect_stub, this);
      if (i == 0)
        gti[i]->btn->set();
    }

    (new LFl_Box(0, gti.size()))->weight(0, 100);

    fr->end();

    int listW = 0, listH = 0;
    fr->getMinSize(&listW, &listH);
    fr->setMinimumSize(listW + 10, 0);
  }

  typeDescription = new LFl_Box(gridTypeDescription(gridType_c::GT_BRICKS), 1, 0);
  typeDescription->pitch(7);
  typeDescription->align(FL_ALIGN_INSIDE | FL_ALIGN_TOP_LEFT | FL_ALIGN_WRAP);

  /* now all the parameter boxes, only the first one is visible, the others are hidden for now */
  {
    fr = new LFl_Frame(1, 1);
    parameterFrame = fr;

    for (unsigned int i = 0; i < gti.size(); i++) {
      gti[i]->gui = gti[i]->ggt->getConfigurationDialog(0, 0, 1, 1);

      if (i != 0)
        gti[i]->gui->hide();
    }

    fr->end();
    /* Bricks is selected first and has no parameter panel. */
    parameterFrame->hide();
  }

  /* finally the 3D view that contains exactly one voxel */


  /* now the buttons */
  {
    layouter_c * l = new layouter_c(0, 2, 3, 1);

    LFl_Button * b = new LFl_Button("OK", 0, 0);
    b->pitch(7);
    b->stretchVCenter();
    b->callback(cb_gridTypeSelectorOk_stub, this);

    LFl_Button * c = new LFl_Button("Cancel", 1, 0);
    c->pitch(7);
    c->stretchVCenter();
    c->shortcut(FL_Escape);
    c->callback(cb_WindowButton_stub, this);

    int okW = 0, okH = 0, cancelW = 0, cancelH = 0;
    b->getMinSize(&okW, &okH);
    c->getMinSize(&cancelW, &cancelH);
    int bw = (okW > cancelW ? okW : cancelW) + 36;
    int bh = okH > cancelH ? okH : cancelH;
    b->setMinimumSize((unsigned)bw, (unsigned)bh);
    c->setMinimumSize((unsigned)bw, (unsigned)bh);
    b->weight(0, 0);
    c->weight(0, 0);

    (new LFl_Box(2, 0))->weight(1, 0);

    l->end();
  }

  current = 0;
}

gridTypeSelectorWindow_c::~gridTypeSelectorWindow_c(void) = default;

void gridTypeSelectorWindow_c::ok_cb(void) {
  okPressed = true;
  hide();
}

std::unique_ptr<gridType_c> gridTypeSelectorWindow_c::getGridType(void) {
  return std::move(gti[current]->gt);
}

