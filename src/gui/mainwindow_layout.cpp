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

/* mainWindow_c, part: building the window: the tabs, the debug pane, the constructor and destructor. */
#include "mainwindow.h"
#include "mainwindow_internal.h"

#include "configuration.h"
#include "groupseditor.h"
#include "placementbrowser.h"
#include "movementbrowser.h"
#include "imageexport.h"
#include "stlexport.h"
#include "guigridtype.h"
#include "grideditor.h"
#include "gridtypegui.h"
#include "statuswindow.h"
#include "debugstatspanel.h"
#include "tooltabs.h"
#include "WindowWidgets.h"
#include "BlockList.h"
#include "Images.h"

#include "../lib/sliding.h"
#include "../lib/helperpool.h"
#include "../lib/stacking.h"
#include "../lib/panex.h"
#include "../lib/sysmemory.h"

#include "assertwindow.h"
#include "togglebutton.h"
#include "voxeleditgroup.h"
#include "view3dgroup.h"
#include "statusline.h"
#include "resultviewer.h"
#include "separator.h"
#include "buttongroup.h"
#include "constraintsgroup.h"
#include "blocklistgroup.h"
#include "shapehistory.h"
#include "vectorexportwindow.h"
#include "convertwindow.h"
#include "assmimportwindow.h"
#include "bulkrangewindow.h"
#include "stlexportsolution.h"

#include "LFl_Tile.h"

#include "../lib/ps3dloader.h"
#include "../lib/scadloader.h"
#include "../lib/voxel.h"
#include "../lib/puzzle.h"
#include "../lib/problem.h"
#include "../lib/assembler.h"
#include "../lib/solvethread.h"
#include "../lib/disassembly.h"
#include "../lib/disassembler_factory.h"
#include "../lib/solvertype.h"
#include "../lib/gridtype.h"
#include "../lib/disasmtomoves.h"
#include "../lib/assembly.h"
#include "../lib/converter.h"
#include "../lib/millable.h"
#include "../lib/voxeltable.h"
#include "../lib/solution.h"

#include "../tools/gzstream.h"
#include "../tools/xml.h"

#include "filechooser.h"
#include "platform.h"
#include "mainmenu.h"
#include "../lib/bt_assert.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#define GL_SILENCE_DEPRECATION 1
#include <FL/Fl_Color_Chooser.H>
#include <FL/Fl_Tooltip.H>
#include <FL/Fl_Choice.H>
#include <FL/Fl_Pixmap.H>
#include <FL/Fl.H>
#include <FL/Fl_Group.H>
#include <FL/Fl_Tile.H>
#include <FL/Fl_Tabs.H>
#include <FL/Fl_Value_Output.H>
#include <FL/Fl_Check_Button.H>
#include <FL/Fl_Progress.H>
#include <FL/Fl_Output.H>
#include <FL/Fl_Value_Slider.H>
#include <FL/Fl_Menu_Bar.H>
#include <FL/Fl_Multi_Label.H>
#include <FL/Fl_Choice.H>
#include <FL/Fl_Value_Input.H>
#include <FL/fl_ask.H>
#include <FL/fl_draw.H>
#include <FL/Fl_Help_View.H>
#pragma GCC diagnostic pop

#include <fstream>
#include <algorithm>
#include <filesystem>
#ifdef _WIN32
#include <process.h>
#define getpid _getpid
#else
#include <unistd.h>
#endif
#include <sstream>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>


#define SZ_GAP 5                               // gap between elements
#define MAIN_TAB_LABELSIZE 18                  // slightly larger than the raised default

/* Width of the four tab headers. Fl_Tabs only adds a few pixels of chrome
 * around each label — not 2× the font size, which overshot by ~60px. */
static int leftBarWidth(void) {
  static const char *const labels[] = {
    "  Entities  ", "  Puzzle  ", "  Solver  ", "  Debug  "
  };
  fl_font(FL_HELVETICA, MAIN_TAB_LABELSIZE);
  int total = 0;
  for (const char *lab : labels)
    total += (int)fl_width(lab) + 8;
  if (total < 360)
    total = 360;
  return total + 25;
}

/* Vertical scroll around a tab's left-bar tile. The content keeps its
 * preferred height; when the window is shorter, a scrollbar appears
 * instead of squashing the controls. */
static LFl_Scroll * makeTabScroll(void) {
  const int barW = leftBarWidth();
  LFl_Scroll * s = new LFl_Scroll(0, 0, 1, 1);
  s->type(Fl_Scroll::VERTICAL);
  s->box(FL_FLAT_BOX);
  s->color(FL_BACKGROUND_COLOR);
  s->weight(1, 1);
  s->setMinimumSize(barW, 140);
  s->setShrinkMinSize(barW, 140);
  return s;
}

void mainWindow_c::CreateShapeTab(void) {

  TabPieces = new layouter_c();
  TabPieces->label("  Entities  ");
  TabPieces->labelsize(MAIN_TAB_LABELSIZE);
  TabPieces->tooltip("Edit shapes");
  TabPieces->clear_visible_focus();

  LFl_Scroll * scroll = makeTabScroll();

  LFl_Tile * tile = new LFl_Tile(0, 0, 1, 1);
  tile->pitch(SZ_GAP);
  tile->weight(1, 1);

  {
    layouter_c * group = new layouter_c(0, 0);
    group->box(FL_FLAT_BOX);

    new LSeparator_c(0, 0, 1, 1, "Shapes", false);

    layouter_c * o = new layouter_c(0, 1);

    BtnNewShape =   new LFlatButton_c(0, 0, 1, 1, "New", " Add another piece ", cb_NewShape_stub, this);
    static_cast<LFlatButton_c*>(BtnNewShape)->weight(1, 0);
    (new LFl_Box(1, 0))->setMinimumSize(SZ_GAP, 0);
    BtnDelShape =   new LFlatButton_c(2, 0, 1, 1, "Delete", " Delete selected piece ", cb_DeleteShape_stub, this);
    static_cast<LFlatButton_c*>(BtnDelShape)->weight(1, 0);
    (new LFl_Box(3, 0))->setMinimumSize(SZ_GAP, 0);
    BtnCpyShape =   new LFlatButton_c(4, 0, 1, 1, "Copy", " Copy selected piece ", cb_CopyShape_stub, this);
    static_cast<LFlatButton_c*>(BtnCpyShape)->weight(1, 0);
    (new LFl_Box(5, 0))->setMinimumSize(SZ_GAP, 0);
    BtnRenShape =   new LFlatButton_c(6, 0, 1, 1, "Label", " Give the selected shape a name ", cb_NameShape_stub, this);
    static_cast<LFlatButton_c*>(BtnRenShape)->weight(1, 0);
    (new LFl_Box(7, 0))->setMinimumSize(SZ_GAP, 0);
    BtnUndo =       new LFlatButton_c(8, 0, 1, 1, "Undo",
#ifdef __APPLE__
                                      " Undo the last shape change Cmd+Z ",
#else
                                      " Undo the last shape change Ctrl+Z ",
#endif
                                      cb_Undo_stub, this);
    static_cast<LFlatButton_c*>(BtnUndo)->weight(1, 0);
    BtnUndo->deactivate();

    o->end();

    (new LFl_Box(0, 2))->setMinimumSize(0, SZ_GAP);

    o = new layouter_c(0, 3);

    BtnWeightInc =  new LFlatButton_c(0, 0, 1, 1, "W+", " Increase Weight of the selected shape ",cb_WeightInc_stub, this);
    static_cast<LFlatButton_c*>(BtnWeightInc)->weight(1, 0);
    (new LFl_Box(1, 0))->setMinimumSize(SZ_GAP, 0);
    BtnWeightDec =  new LFlatButton_c(2, 0, 1, 1, "W-", " Decrease Weight of the selected shape ",cb_WeightDec_stub, this);
    static_cast<LFlatButton_c*>(BtnWeightDec)->weight(1, 0);
    (new LFl_Box(3, 0))->setMinimumSize(SZ_GAP, 0);
    BtnShapeLeft =  new LFlatButton_c(4, 0, 1, 1, "@-14->", " Exchange current shape with previous shape ", cb_ShapeLeft_stub, this);
    static_cast<LFlatButton_c*>(BtnShapeLeft)->weight(1, 0);
    (new LFl_Box(5, 0))->setMinimumSize(SZ_GAP, 0);
    BtnShapeRight = new LFlatButton_c(6, 0, 1, 1, "@-16->", " Exchange current shape with next shape ", cb_ShapeRight_stub, this);
    static_cast<LFlatButton_c*>(BtnShapeRight)->weight(1, 0);
    (new LFl_Box(7, 0))->setMinimumSize(SZ_GAP, 0);
    BtnDetails = new LFlatButton_c(8, 0, 1, 1, "Details", " Show shape details ", cb_StatusWindow_stub, this);
    static_cast<LFlatButton_c*>(BtnDetails)->weight(1, 0);
    (new LFl_Box(9, 0))->setMinimumSize(SZ_GAP, 0);
    BtnRedo =    new LFlatButton_c(10, 0, 1, 1, "Redo",
#ifdef __APPLE__
                                   " Redo the last undone shape change Cmd+Shift+Z ",
#else
                                   " Redo the last undone shape change Ctrl+Shift+Z ",
#endif
                                   cb_Redo_stub, this);
    static_cast<LFlatButton_c*>(BtnRedo)->weight(1, 0);
    BtnRedo->deactivate();

    o->end();

    LFl_Box * gap = new LFl_Box(0, 4);
    gap->setMinimumSize(0, SZ_GAP);
    startGoalGap = gap;

    o = new layouter_c(0, 5);
    startGoalRow = o;
    BtnNewStartGoal = new LFlatButton_c(0, 0, 1, 1, "New Start/Goal Positions",
        " Add a shape that holds start and goal positions ", cb_NewStartGoal_stub, this);
    static_cast<LFlatButton_c*>(BtnNewStartGoal)->weight(1, 0);
    (new LFl_Box(1, 0))->setMinimumSize(SZ_GAP, 0);
    BtnDelStartGoal = new LFlatButton_c(2, 0, 1, 1, "Delete Start/Goal Positions",
        " Delete the selected start/goal shape ", cb_DelStartGoal_stub, this);
    static_cast<LFlatButton_c*>(BtnDelStartGoal)->weight(1, 0);
    o->end();

    (new LFl_Box(0, 6))->setMinimumSize(0, SZ_GAP);

    PcSel = new PieceSelector(0, 0, 200, 200, puzzle);
    pieceSelGroup = new LBlockListGroup_c(0, 7, 1, 1, PcSel);
    pieceSelGroup->callback(cb_PcSel_stub, this);
    pieceSelGroup->tooltip(" Select the shape that you want to edit ");
    pieceSelGroup->weight(1, 1);

    diskList = new DiskSelector(0, 7, 1, 1, puzzle);
    diskList->callback(cb_DiskList_stub, this);
    diskList->tooltip(" Select a disc and set its size ");
    diskList->weight(1, 1);
    /* The disc list scrolls, so on a short window it gives up height
     * first, down to two rows, and the rod controls below stay in view. */
    diskList->setShrinkMinSize(220, 56);
    diskList->shrinkPrio(128, 10);
    diskList->hide();

    (new LFl_Box(0, 8))->setMinimumSize(0, SZ_GAP);

    rodsPanel = new layouter_c(0, 9);
    rodsPanel->weight(1, 0);
    /* Shrinks before the disc list, through its rod-set list (below). */
    rodsPanel->shrinkPrio(128, 5);
    rodsPanel->hide();
    {
      new LSeparator_c(0, 0, 1, 1, "Rods", false);

      layouter_c * o = new layouter_c(0, 1);
      BtnNewRod = new LFlatButton_c(0, 0, 1, 1, "New", " Add another rod set ", cb_NewRod_stub, this);
      static_cast<LFlatButton_c*>(BtnNewRod)->weight(1, 0);
      (new LFl_Box(1, 0))->setMinimumSize(SZ_GAP, 0);
      BtnDelRod = new LFlatButton_c(2, 0, 1, 1, "Delete", " Delete the selected rod set ", cb_DeleteRod_stub, this);
      static_cast<LFlatButton_c*>(BtnDelRod)->weight(1, 0);
      (new LFl_Box(3, 0))->setMinimumSize(SZ_GAP, 0);
      BtnCpyRod = new LFlatButton_c(4, 0, 1, 1, "Copy", " Copy the selected rod set ", cb_CopyRod_stub, this);
      static_cast<LFlatButton_c*>(BtnCpyRod)->weight(1, 0);
      (new LFl_Box(5, 0))->setMinimumSize(SZ_GAP, 0);
      BtnRenRod = new LFlatButton_c(6, 0, 1, 1, "Label", " Name the selected rod set ", cb_NameRod_stub, this);
      static_cast<LFlatButton_c*>(BtnRenRod)->weight(1, 0);
      (new LFl_Box(7, 0))->setMinimumSize(SZ_GAP, 0);
      BtnRodUndo = new LFlatButton_c(8, 0, 1, 1, "Undo", " Undo the last rod change ", cb_RodUndo_stub, this);
      static_cast<LFlatButton_c*>(BtnRodUndo)->weight(1, 0);
      o->end();

      (new LFl_Box(0, 2))->setMinimumSize(0, SZ_GAP);

      o = new layouter_c(0, 3);
      BtnRodLeft = new LFlatButton_c(0, 0, 1, 1, "@-14->", " Exchange with the previous rod set ", cb_RodLeft_stub, this);
      static_cast<LFlatButton_c*>(BtnRodLeft)->weight(1, 0);
      (new LFl_Box(1, 0))->setMinimumSize(SZ_GAP, 0);
      BtnRodRight = new LFlatButton_c(2, 0, 1, 1, "@-16->", " Exchange with the next rod set ", cb_RodRight_stub, this);
      static_cast<LFlatButton_c*>(BtnRodRight)->weight(1, 0);
      (new LFl_Box(3, 0))->setMinimumSize(SZ_GAP, 0);
      BtnRodRedo = new LFlatButton_c(4, 0, 1, 1, "Redo", " Redo the last undone rod change ", cb_RodRedo_stub, this);
      static_cast<LFlatButton_c*>(BtnRodRedo)->weight(1, 0);
      o->end();

      (new LFl_Box(0, 4))->setMinimumSize(0, SZ_GAP);

      rodSel = new RodSelector(0, 0, 200, 200, puzzle);
      LBlockListGroup_c * rodGroup = new LBlockListGroup_c(0, 5, 1, 1, rodSel);
      rodGroup->callback(cb_RodSel_stub, this);
      rodGroup->tooltip(" Select the rod set to edit ");
      rodGroup->weight(1, 0);
      rodGroup->setMinimumSize(220, 168);
      /* A puzzle seldom has more than a couple of rod sets: on a short
       * window this list gives up its empty space before the disc list. */
      rodGroup->setShrinkMinSize(220, 56);
      rodGroup->shrinkPrio(128, 5);

      layouter_c * rules = new layouter_c(0, 6);
      rules->weight(0, 0);
      int y = 0;
      LFl_Box * rodsLabel = new LFl_Box("How many rods", 0, y, 1, 1);
      rodsLabel->stretchVCenter();
      rodCountInput = new LFl_Value_Input(1, y, 1, 1);
      rodCountInput->bounds(1, STACK_ROD_BUTTONS);
      rodCountInput->step(1);
      rodCountInput->value(3);
      rodCountInput->when(FL_WHEN_RELEASE | FL_WHEN_ENTER_KEY);
      rodCountInput->tooltip(" Pegs on this board ");
      rodCountInput->stretchVCenter();
      rodCountInput->callback(cb_RodField_stub, this);

      y += 2;
      /* Consecutive radio buttons, so only one of these can be on. */
      rodGrow = new LFl_Radio_Button("Grow to the height of all pieces", 0, y, 2, 1);
      rodGrow->tooltip(" A rod can hold every disc in the problem ");
      rodGrow->callback(cb_RodField_stub, this);
      y++;
      rodFixed = new LFl_Radio_Button("Defined height", 0, y, 1, 1);
      rodFixed->tooltip(" A rod is full at this disc count ");
      rodFixed->callback(cb_RodField_stub, this);
      rodHeightInput = new LFl_Value_Input(1, y, 1, 1);
      rodHeightInput->bounds(1, 64);
      rodHeightInput->step(1);
      rodHeightInput->value(8);
      rodHeightInput->when(FL_WHEN_RELEASE | FL_WHEN_ENTER_KEY);
      rodHeightInput->stretchVCenter();
      rodHeightInput->callback(cb_RodField_stub, this);
      rodGrow->setonly();

      y += 2;
      rodSizeMatters = new LFl_Check_Button("Disc size matters - no large on small", 0, y, 2, 1);
      rodSizeMatters->tooltip(" A disc may not be placed on a disc with a smaller size number. Equal numbers may stack. ");
      rodSizeMatters->callback(cb_RodField_stub, this);

      y++;
      rodDistance = new LFl_Check_Button("Disc can only move over 1 rod", 0, y, 2, 1);
      rodDistance->tooltip(" When checked, a disc may move only to a neighboring rod. When unchecked, a disc may move to any rod. ");
      rodDistance->callback(cb_RodField_stub, this);

      y++;
      rodPanex = new LFl_Check_Button("Panex Style Columns", 0, y, 2, 1);
      rodPanex->tooltip(" Rods are Panex columns: a disc of size n can sit at most n places below the top, and discs stack in any order. One more disc may wait at the top, raised into the bridge between the columns, where it blocks every move that passes over it. ");
      rodPanex->callback(cb_RodField_stub, this);

      y++;
      rodPocket = new LFl_Check_Button("Add pocket column", 0, y, 1, 1);
      rodPocket->tooltip(" An extra column beside rod 1 that holds any disc, in any order, up to the height given here. Needs Panex Style Columns. ");
      rodPocket->callback(cb_RodField_stub, this);
      rodPocketHeight = new LFl_Value_Input(1, y, 1, 1);
      rodPocketHeight->tooltip(" How many discs the pocket column holds ");
      rodPocketHeight->bounds(1, 64);
      rodPocketHeight->step(1);
      rodPocketHeight->value(1);
      rodPocketHeight->when(FL_WHEN_RELEASE | FL_WHEN_ENTER_KEY);
      rodPocketHeight->stretchVCenter();
      rodPocketHeight->callback(cb_RodField_stub, this);
      rules->end();
    }
    rodsPanel->end();

    group->weight(0, 4);
    group->end();
  }

  {
    layouter_c * group = new layouter_c(0, 1);
    shapeEditColumn = group;
    group->box(FL_FLAT_BOX);

    new LSeparator_c(0, 0, 1, 1, "Edit", true);

    pieceTools = new ToolTabContainer(0, 1, 1, 1, ggt);
    pieceTools->callback(cb_TransformPiece_stub, this);
    pieceTools->setPreviewHandler(cb_TransformPreview_stub, this);

    voxelToolGap = new LFl_Box(0, 2, 1, 1);
    voxelToolGap->setMinimumSize(0, 5);

    layouter_c * o2 = new layouter_c(0, 3, 1, 1);
    voxelPenRow = o2;

    editChoice = new ButtonGroup_c(0, 0, 1, 1);

    Fl_Button * b;
    b = editChoice->addButton();
    b->image(pm.get(TB_Color_Pen_Fixed_xpm));
    b->tooltip(" Add normal voxels to the shape F5 ");

    b = editChoice->addButton();
    b->image(pm.get(TB_Color_Pen_Variable_xpm));
    b->tooltip(" Add variable voxels to the shape F6 ");

    b = editChoice->addButton();
    b->image(pm.get(TB_Color_Eraser_xpm));
    b->tooltip(" Remove voxels from the shape F7 ");

    b = editChoice->addButton();
    b->image(pm.get(TB_Color_Brush_xpm));
    b->tooltip(" Change the constrain colour of voxels in the shape F8 ");

    editChoice->callback(cb_EditChoice_stub, this);

    (new LFl_Box(1, 0, 1, 1))->setMinimumSize(5, 0);

    editMode = new ButtonGroup_c(2, 0, 1, 1);

    b = editMode->addButton();
    b->image(pm.get(TB_Color_Mouse_Rubber_Band_xpm));
    b->tooltip(" Make changes by dragging rectangular areas in the grid editor ");

    b = editMode->addButton();
    b->image(pm.get(TB_Color_Mouse_Drag_xpm));
    b->tooltip(" Make changes by painting in the grid editor ");

    editMode->callback(cb_EditMode_stub, this);

    (new LFl_Box(3, 0, 1, 1))->setMinimumSize(5, 0);

    LToggleButton_c * btn;

    btn = new LToggleButton_c(4, 0, 1, 1, cb_EditSym_stub, this, gridEditor_c::TOOL_MIRROR_X);
    btn->image(pm.get(TB_Color_Symmetrical_X_xpm));
    btn->tooltip(" Toggle mirroring along the y-z-plane ");

    btn = new LToggleButton_c(5, 0, 1, 1, cb_EditSym_stub, this, gridEditor_c::TOOL_MIRROR_Y);
    btn->image(pm.get(TB_Color_Symmetrical_Y_xpm));
    btn->tooltip(" Toggle mirroring along the x-z-plane ");

    btn = new LToggleButton_c(6, 0, 1, 1, cb_EditSym_stub, this, gridEditor_c::TOOL_MIRROR_Z);
    btn->image(pm.get(TB_Color_Symmetrical_Z_xpm));
    btn->tooltip(" Toggle mirroring along the x-y-plane ");

    (new LFl_Box(7, 0, 1, 1))->setMinimumSize(5, 0);

    btn = new LToggleButton_c(8, 0, 1, 1, cb_EditSym_stub, this, gridEditor_c::TOOL_STACK_X);
    btn->image(pm.get(TB_Color_Columns_X_xpm));
    btn->tooltip(" Toggle drawing in all x layers ");

    btn = new LToggleButton_c(9, 0, 1, 1, cb_EditSym_stub, this, gridEditor_c::TOOL_STACK_Y);
    btn->image(pm.get(TB_Color_Columns_Y_xpm));
    btn->tooltip(" Toggle drawing in all y layers ");

    btn = new LToggleButton_c(10, 0, 1, 1, cb_EditSym_stub, this, gridEditor_c::TOOL_STACK_Z);
    btn->image(pm.get(TB_Color_Columns_Z_xpm));
    btn->tooltip(" Toggle drawing in all z layers ");

    (new LFl_Box(11, 0, 1, 1))->weight(1, 0);

    o2->end();

    voxelEditGap = new LFl_Box(0, 4, 1, 1);
    voxelEditGap->setMinimumSize(0, 5);

    pieceEdit = new VoxelEditGroup_c(0, 5, 1, 1, puzzle, ggt);
    pieceEdit->callback(cb_pieceEdit_stub, this);
    pieceEdit->end();
    pieceEdit->editType(gridEditor_c::EDT_RUBBER);
    pieceEdit->weight(0, 1);

    discTabs = new LFl_Tabs(0, 6, 1, 1);
    discTabs->weight(0, 1);
    discTabs->hide();
    {
      layouter_c * disc = new layouter_c();
      disc->label("Disc");
      discInfo = new LFl_Box("The 3D view shows this disc. It is one voxel thick.", 0, 0, 1, 1);
      discInfo->align(FL_ALIGN_INSIDE | FL_ALIGN_TOP_LEFT | FL_ALIGN_WRAP);
      discInfo->weight(1, 1);
      disc->end();

      layouter_c * sz = new layouter_c();
      sz->label("Size");
      (new LFl_Box("Size", 0, 0, 1, 1))->tooltip(" Rank compared by the size rule, and the radius of the disc ");
      diskSizeInput = new LFl_Value_Input(1, 0, 1, 1);
      diskSizeInput->bounds(1, 64);
      diskSizeInput->step(1);
      diskSizeInput->value(1);
      diskSizeInput->when(FL_WHEN_RELEASE | FL_WHEN_ENTER_KEY);
      diskSizeInput->tooltip(" The same number always builds the same disc ");
      sz->end();
    }
    discTabs->end();

    group->weight(0, 4);
    group->end();
  }

  {
    colorsGroup = new layouter_c(0, 2);
    colorsGroup->box(FL_FLAT_BOX);

    new LSeparator_c(0, 0, 1, 1, "Colors", true);

    layouter_c * o = new layouter_c(0, 1);

    BtnNewColor = new LFlatButton_c(0, 0, 1, 1, "Add", " Add another colour ", cb_AddColor_stub, this);
    static_cast<LFlatButton_c*>(BtnNewColor)->weight(1, 0);
    (new LFl_Box(1, 0))->setMinimumSize(SZ_GAP, 0);
    BtnDelColor = new LFlatButton_c(2, 0, 1, 1, "Remove", " Remove selected colour ", cb_RemoveColor_stub, this);
    static_cast<LFlatButton_c*>(BtnDelColor)->weight(1, 0);
    (new LFl_Box(3, 0))->setMinimumSize(SZ_GAP, 0);
    BtnChnColor = new LFlatButton_c(4, 0, 1, 1, "Edit", " Change selected colour ", cb_ChangeColor_stub, this);
    static_cast<LFlatButton_c*>(BtnChnColor)->weight(1, 0);

    o->end();

    (new LFl_Box(0, 2))->setMinimumSize(0, SZ_GAP);

    colorSelector = new ColorSelector(0, 0, 200, 200, puzzle, true);
    LBlockListGroup_c * colGroup = new LBlockListGroup_c(0, 3, 1, 1, colorSelector);
    colGroup->callback(cb_ColSel_stub, this);
    colGroup->tooltip(" Select colour to use for all editing operations ");
    colGroup->weight(1, 1);
    colGroup->setMinimumSize(30, 72);

    colorsGroup->weight(0, 1);
    colorsGroup->end();
  }

  tile->end();

  TabPieces->resizable(scroll);
  TabPieces->end();

  Fl_Group::current()->resizable(TabPieces);
}

void mainWindow_c::CreateProblemTab(void) {

  TabProblems = new layouter_c();
  TabProblems->label("  Puzzle  ");
  TabProblems->labelsize(MAIN_TAB_LABELSIZE);
  TabProblems->tooltip("Edit problems");
  TabProblems->hide();
  TabProblems->clear_visible_focus();

  LFl_Scroll * scroll = makeTabScroll();

  LFl_Tile * tile = new LFl_Tile(0, 0, 1, 1);
  tile->pitch(SZ_GAP);
  tile->weight(1, 1);

  {
    layouter_c * group = new layouter_c(0, 0);
    group->box(FL_FLAT_BOX);

    new LSeparator_c(0, 0, 1, 1, "Problems", false);

    layouter_c * o = new layouter_c(0, 1);

    BtnNewProb = new LFlatButton_c(0, 0, 1, 1, "New", " Add another problem ", cb_NewProblem_stub, this);
    static_cast<LFlatButton_c*>(BtnNewProb)->weight(1, 0);
    (new LFl_Box(1, 0))->setMinimumSize(SZ_GAP, 0);
    BtnDelProb = new LFlatButton_c(2, 0, 1, 1, "Delete", " Delete selected problem ", cb_DeleteProblem_stub, this);
    static_cast<LFlatButton_c*>(BtnDelProb)->weight(1, 0);
    (new LFl_Box(3, 0))->setMinimumSize(SZ_GAP, 0);
    BtnCpyProb = new LFlatButton_c(4, 0, 1, 1, "Copy", " Copy selected problem ", cb_CopyProblem_stub, this);
    static_cast<LFlatButton_c*>(BtnCpyProb)->weight(1, 0);
    (new LFl_Box(5, 0))->setMinimumSize(SZ_GAP, 0);
    BtnRenProb = new LFlatButton_c(6, 0, 1, 1, "Label", " Rename selected problem ", cb_RenameProblem_stub, this);
    static_cast<LFlatButton_c*>(BtnRenProb)->weight(1, 0);
    (new LFl_Box(7, 0))->setMinimumSize(SZ_GAP, 0);

    BtnProbLeft = new LFlatButton_c(8, 0, 1, 1, "@-14->", " Exchange current problem with previous problem ", cb_ProblemLeft_stub, this);
    (new LFl_Box(9, 0))->setMinimumSize(SZ_GAP, 0);
    BtnProbRight = new LFlatButton_c(10, 0, 1, 1, "@-16->", " Exchange current problem with next problem ", cb_ProblemRight_stub, this);

    o->end();

    (new LFl_Box(0, 2))->setMinimumSize(0, SZ_GAP);

    problemSelector = new ProblemSelector(0, 0, 100, 100, puzzle);
    LBlockListGroup_c * probGroup = new LBlockListGroup_c(0, 3, 1, 1, problemSelector);
    probGroup->callback(cb_ProbSel_stub, this);
    probGroup->tooltip(" Select problem to edit ");
    probGroup->weight(1, 1);

    group->end();
  }

  {
    layouter_c * group = new layouter_c(0, 1);
    group->box(FL_FLAT_BOX);

    layouter_c * o = new layouter_c(0, 0);

    problemResult = new ResultViewer_c(0, 0, 1, 1);
    problemResult->tooltip(" The result shape for the current problem ");
    problemResult->weight(1, 0);

    (new LFl_Box(1, 0))->setMinimumSize(SZ_GAP, 0);

    BtnSetResult = new LFlatButton_c(2, 0, 1, 1, "Set Result", " Set selected shape as result ", cb_ShapeToResult_stub, this);
    {
      int tw = 0, th = 0;
      BtnSetResult->measure_label(tw, th);
      static_cast<LFlatButton_c*>(BtnSetResult)->setMinimumSize(tw + 20, th + 8);
    }

    o->end();

    (new LFl_Box(0, 1))->setMinimumSize(0, SZ_GAP);

    rodAssignGroup = new layouter_c(0, 2);
    rodAssignGroup->weight(1, 1);
    rodAssignGroup->hide();
    new LSeparator_c(0, 0, 1, 1, "Rods", false);
    rodAssignSel = new RodSelector(0, 0, 100, 100, puzzle);
    {
      LBlockListGroup_c * rodGroup = new LBlockListGroup_c(0, 1, 1, 1, rodAssignSel);
      rodGroup->callback(cb_ProblemRod_stub, this);
      rodGroup->tooltip(" Rod sets that can be this problem's board ");
      rodGroup->weight(1, 1);
    }
    rodAssignGroup->end();

    new LSeparator_c(0, 3, 1, 1, "Pieces", true);

    shapeAssignmentSelector = new PieceSelector(0, 0, 100, 100, puzzle);
    LBlockListGroup_c * shapeGroup = new LBlockListGroup_c(0, 4, 1, 1, shapeAssignmentSelector);
    shapeGroup->callback(cb_ShapeSel_stub, this);
    shapeGroup->tooltip(" Select a shape to set as result or to add or remove from problem ");
    shapeGroup->weight(1, 1);

    (new LFl_Box(0, 5))->setMinimumSize(0, SZ_GAP);

    stackSliderRow = new layouter_c(0, 6);
    stackSliderRow->hide();
    {
      stackRodBar = new RodIndexBar_c(0, 0, 1, 1);
      stackRodBar->weight(1, 0);
      stackRodBar->tooltip(" Select which rod in the chosen rod set to stack discs on ");
      stackRodBar->callback(cb_StackRod_stub, this);
    }
    stackSliderRow->end();

    stackModeRow = new layouter_c(0, 7);
    stackModeRow->hide();
    stackStartMode = new LFl_Radio_Button("Start", 0, 0, 1, 1);
    stackStartMode->tooltip(" Order the discs that start on the selected rod, bottom to top ");
    stackStartMode->callback(cb_StackMode_stub, this);
    stackGoalMode = new LFl_Radio_Button("Goal", 1, 0, 1, 1);
    stackGoalMode->tooltip(" Order the discs the solver must reach on the selected rod ");
    stackGoalMode->callback(cb_StackMode_stub, this);
    stackStartMode->setonly();
    stackModeRow->end();

    group->end();
  }

  {
    layouter_c * group = new layouter_c(0, 2);
    group->box(FL_FLAT_BOX);

    stackOrderSep = new LSeparator_c(0, 0, 1, 1, "Stack Order", true);
    stackOrderSep->hide();
    problemButtonRule = new LSeparator_c(0, 0, 1, 1, 0, true);

    {
      layouter_c * o = new layouter_c(0, 1);
      int xp = 0;

      /* The shape order arrows start in the classic place, at the right end
       * of the row. syncProbArrows() moves them to the left, as up and
       * down, for stacking's disc order. */
      BtnProbShapeLeft = new LFlatButton_c(xp++, 0, 1, 1, "@-14->", " Exchange current shape with previous shape ", cb_ProbShapeLeft_stub, this);
      probArrowGapL[0] = new LFl_Box(xp++, 0);
      static_cast<LFl_Box*>(probArrowGapL[0])->setMinimumSize(SZ_GAP, 0);
      BtnProbShapeRight = new LFlatButton_c(xp++, 0, 1, 1, "@-16->", " Exchange current shape with next shape ", cb_ProbShapeRight_stub, this);
      probArrowGapL[1] = new LFl_Box(xp++, 0);
      static_cast<LFl_Box*>(probArrowGapL[1])->setMinimumSize(SZ_GAP, 0);
      BtnAddShape = new LFlatButton_c(xp++, 0, 1, 1, "+1", " Add another one of the selected shape ", cb_AddShapeToProblem_stub, this);
      static_cast<LFlatButton_c*>(BtnAddShape)->weight(1, 0);
      (new LFl_Box(xp++, 0))->setMinimumSize(SZ_GAP, 0);
      BtnRemShape = new LFlatButton_c(xp++, 0, 1, 1, "-1", " Remove one of the selected shapes ", cb_RemoveShapeFromProblem_stub, this);
      static_cast<LFlatButton_c*>(BtnRemShape)->weight(1, 0);
      (new LFl_Box(xp++, 0))->setMinimumSize(SZ_GAP, 0);
      BtnAddAll = new LFlatButton_c(xp++, 0, 1, 1, "All +1", " Add one of all shapes except result ", cb_AddAllShapesToProblem_stub, this);
      static_cast<LFlatButton_c*>(BtnAddAll)->weight(1, 0);
      (new LFl_Box(xp++, 0))->setMinimumSize(SZ_GAP, 0);
      BtnRemAll = new LFlatButton_c(xp++, 0, 1, 1, "Clear All", " Remove all pieces ", cb_RemoveAllShapesFromProblem_stub, this);
      static_cast<LFlatButton_c*>(BtnRemAll)->weight(1, 0);
      probArrowRightX = xp;
      probArrowGapR[0] = new LFl_Box(xp++, 0);
      static_cast<LFl_Box*>(probArrowGapR[0])->setMinimumSize(SZ_GAP, 0);
      xp++;
      probArrowGapR[1] = new LFl_Box(xp++, 0);
      static_cast<LFl_Box*>(probArrowGapR[1])->setMinimumSize(SZ_GAP, 0);

      o->end();
      probArrowsStacking = true;
      syncProbArrows(false);
    }

    (new LFl_Box(0, 2))->setMinimumSize(0, SZ_GAP);

    {
      layouter_c * o = new layouter_c(0, 3);
      int xp = 0;

      BtnMinZero = new LFlatButton_c(xp++, 0, 1, 1, "min=0", " Set minimum number of pieces to 0 ", cb_SetShapeMinimumToZero_stub, this);
      static_cast<LFlatButton_c*>(BtnMinZero)->weight(1, 0);
      (new LFl_Box(xp++, 0))->setMinimumSize(SZ_GAP, 0);
      BtnSetAllRange = new LFlatButton_c(xp++, 0, 1, 1, "Set All Ranges", " Set min/max piece count for all shapes in the current problem ", cb_SetAllRange_stub, this);
      static_cast<LFlatButton_c*>(BtnSetAllRange)->weight(1, 0);
      (new LFl_Box(xp++, 0))->setMinimumSize(SZ_GAP, 0);
      BtnGroup =    new LFlatButton_c(xp++, 0, 1, 1, "Set Groups", " Edit groups of the problem ", cb_ShapeGroup_stub, this);
      static_cast<LFlatButton_c*>(BtnGroup)->weight(1, 0);

      o->end();
    }

    {
      layouter_c * o = new layouter_c(0, 4);
      (new LFl_Box(0, 0))->setMinimumSize(0, SZ_GAP);
      stackValidRow = new layouter_c(0, 1);
      stackValidRow->hide();
      stackValidBar = new StackValidBar_c(0, 0, 1, 1);
      stackValidBar->weight(1, 0);
      stackValidBar->tooltip(" Whether the start and goal stackings obey the rod set's rules ");
      stackValidBar->callback(cb_StackValidRelayout_stub, this);
      (new LFl_Box(0, 1))->setMinimumSize(0, SZ_GAP);
      stackValidRow->end();
      o->end();
    }

    PiecesCountList = new PiecesList(0, 0, 100, 100);
    piecesCountGroup = new LBlockListGroup_c(0, 5, 1, 1, PiecesCountList);
    piecesCountGroup->callback(cb_PiecesClicked_stub, this);
    piecesCountGroup->tooltip(" Show which shapes are used in the current problem and how often they are used, can be used to select shapes ");
    piecesCountGroup->weight(1, 1);

    rodStackList = new RodStackList(0, 0, 100, 100);
    LBlockListGroup_c * stackGroup = new LBlockListGroup_c(0, 6, 1, 1, rodStackList);
    stackGroup->tooltip(" Discs on the selected rod, top of the list is the top of the stack ");
    stackGroup->callback(cb_StackListSel_stub, this);
    stackGroup->weight(1, 1);
    stackGroup->hide();

    group->end();
  }

  {
    colourAssignmentGroup = new layouter_c(0, 3);
    colourAssignmentGroup->box(FL_FLAT_BOX);

    new LSeparator_c(0, 0, 1, 1, "Colour Assignment", true);

    colorAssignmentSelector = new ColorSelector(0, 0, 100, 100, puzzle, false);
    LBlockListGroup_c * colGroup = new LBlockListGroup_c(0, 1, 1, 1, colorAssignmentSelector);
    colGroup->callback(cb_ColorAssSel_stub, this);
    colGroup->tooltip(" Select colour to add or remove from constraints ");
    colGroup->weight(1, 1);

    colourAssignmentGroup->end();
  }

  {
    colourConstraintsGroup = new layouter_c(0, 4);
    colourConstraintsGroup->box(FL_FLAT_BOX);

    new LSeparator_c(0, 0, 1, 1, 0, true);

    layouter_c * o = new layouter_c(0, 1);

    BtnColSrtPc = new LFlatButton_c(0, 0, 1, 1, "Sort by Piece", " Sort colour constraints by piece ", cb_CCSortByPiece_stub, this);
    static_cast<LFlatButton_c*>(BtnColSrtPc)->weight(1, 0);
    (new LFl_Box(1, 0))->setMinimumSize(SZ_GAP, 0);
    BtnColAdd = new LFlatButton_c(2, 0, 1, 1, "@-12->", " Add colour to constraint ", cb_AllowColor_stub, this);
    BtnColRem = new LFlatButton_c(3, 0, 1, 1, "@-18->", " Add colour to constraint ", cb_DisallowColor_stub, this);
    (new LFl_Box(4, 0))->setMinimumSize(SZ_GAP, 0);
    BtnColSrtRes = new LFlatButton_c(5, 0, 1, 1, "Sort by Result", " Sort Colour Constraints by Result ", cb_CCSortByResult_stub, this);
    static_cast<LFlatButton_c*>(BtnColSrtRes)->weight(1, 0);

    o->end();

    (new LFl_Box(0, 2))->setMinimumSize(0, SZ_GAP);

    colconstrList = new ColorConstraintsEdit(0, 0, 100, 100, puzzle);
    LConstraintsGroup_c * colGroup = new LConstraintsGroup_c(0, 3, 1, 1, colconstrList);
    colGroup->callback(cb_ColorConstrSel_stub, this);
    colGroup->tooltip(" Colour constraints for the current problem ");
    colGroup->weight(1, 1);

    colourConstraintsGroup->end();
  }

  tile->end();

  TabProblems->resizable(scroll);
  TabProblems->end();
}

/* Inactive FLTK buttons do not get mouse events, so their tooltips never appear.
 * These groups still show a child's tooltip when the pointer is over a greyed-out button.
 */
class filterTooltipGroup_c : public layouter_c {

public:

  filterTooltipGroup_c(int x, int y) : layouter_c(x, y) {}

  int handle(int event) {
    if (event == FL_ENTER || event == FL_MOVE) {
      Fl_Widget *const *kids = array();
      for (int i = children() - 1; i >= 0; i--) {
        Fl_Widget *c = kids[i];
        if (!c->visible() || c->active() || !Fl::event_inside(c))
          continue;
        const char *tt = c->tooltip();
        if (tt && tt[0]) {
          Fl_Tooltip::enter_area(c, 0, 0, c->w(), c->h(), tt);
          return 1;
        }
      }
    }
    return Fl_Group::handle(event);
  }
};

void mainWindow_c::CreateSolveTab(void) {

  TabSolve = new layouter_c();
  TabSolve->label("  Solver  ");
  TabSolve->labelsize(MAIN_TAB_LABELSIZE);
  TabSolve->tooltip("Solve problems");
  TabSolve->hide();
  TabSolve->clear_visible_focus();

  LFl_Scroll * scroll = makeTabScroll();

  LFl_Tile * tile = new LFl_Tile(0, 0, 1, 1);
  tile->pitch(SZ_GAP);
  tile->weight(1, 1);
  solverPane = scroll;

  {
    layouter_c * group = new layouter_c(0, 0);
    group->box(FL_FLAT_BOX);

    solutionProblem = new ProblemSelector(0, 0, 100, 100, puzzle);
    LBlockListGroup_c * shapeGroup = new LBlockListGroup_c(0, 0, 1, 1, solutionProblem);
    shapeGroup->callback(cb_SolProbSel_stub, this);
    shapeGroup->tooltip(" Select problem to solve ");
    shapeGroup->weight(1, 1);

    layouter_c * o = new layouter_c(0, 1);

    SolveDisasm = new LFl_Check_Button("Disassemble", 0, 0, 1, 1);
    SolveDisasm->tooltip(" Do also try to disassemble the assembled puzzles. Only puzzles that can be disassembled will be added to solutions ");
    SolveDisasm->clear_visible_focus();
    SolveDisasm->callback(cb_SolverOptions_stub, this);

    CheckRotations = new LFl_Check_Button("Check Rotations", 0, 1, 1, 1);
    CheckRotations->tooltip(" Also try 90 degree piece rotations (brick grids) during disassembly. Requires Disassemble. ");
    CheckRotations->clear_visible_focus();
    CheckRotations->callback(cb_SolverOptions_stub, this);

    JustCount = new LFl_Check_Button("Just Count", 0, 2, 1, 1);
    JustCount->tooltip(" Don\'t save the solutions, just count the number of them ");
    JustCount->clear_visible_focus();
    JustCount->callback(cb_SolverOptions_stub, this);

    DropDisassemblies = new LFl_Check_Button("Just Levels", 0, 3, 1, 1);
    DropDisassemblies->tooltip(" Don\'t save the Disassemblies, just the information about them ");
    DropDisassemblies->clear_visible_focus();
    DropDisassemblies->callback(cb_SolverOptions_stub, this);

    CompleteRotations = new LFl_Check_Button("Deep Symmetry Check", 1, 0, 1, 1);
    CompleteRotations->tooltip(" Do expensive and thorough rotation check, eliminating translations and rotations not in symmetry of the result shape ");
    CompleteRotations->clear_visible_focus();
    CompleteRotations->callback(cb_SolverOptions_stub, this);

    KeepMirrors = new LFl_Check_Button("Keep Mirror Solutions", 1, 1, 1, 1);
    KeepMirrors->tooltip(" Don't remove solutions that are mirrors of another solution ");
    KeepMirrors->clear_visible_focus();
    KeepMirrors->callback(cb_SolverOptions_stub, this);

    KeepRotations = new LFl_Check_Button("Keep Rotated Solutions", 1, 2, 1, 1);
    KeepRotations->tooltip(" Don't remove solutions that are rotations of other solutions ");
    KeepRotations->clear_visible_focus();
    KeepRotations->callback(cb_SolverOptions_stub, this);

    StrictColors = new LFl_Check_Button("Strict Color Restrictions", 1, 3, 1, 1);
    StrictColors->tooltip(" A voxel with a colour fits only a result voxel of that same colour, not a neutral one. A voxel with no colour fits only a result voxel that also has no colour. ");
    StrictColors->clear_visible_focus();
    StrictColors->callback(cb_SolverOptions_stub, this);

    NestedSlides = new LFl_Check_Button("Allow Nested Slides", 0, 3, 1, 1);
    NestedSlides->tooltip(" A piece may slide together with the pieces nested inside its outline, such as a piece in another piece's pocket. Pieces that only touch never slide together. ");
    NestedSlides->clear_visible_focus();
    NestedSlides->callback(cb_SolverOptions_stub, this);
    NestedSlides->hide();

    HighMemory = new LFl_Check_Button("Enable High Memory", 1, 0, 1, 1);
    HighMemory->tooltip(" Let a sliding or stacking search hold up to half of this computer's memory in arrangements, instead of stopping at about 2 GB. Matters mostly for the Sliding Full Solver and for large stacking puzzles. ");
    HighMemory->clear_visible_focus();
    HighMemory->callback(cb_SolverOptions_stub, this);
    HighMemory->hide();

    Autosave = new LFl_Check_Button("Autosave every 20 min", 0, 4, 1, 1);
    Autosave->tooltip(" Save the solve 20 minutes into the run and every 20 minutes after, so a crash loses little. Brick and sliding solves pause briefly and save a recovery copy of the puzzle, which BurrTools offers when you open the puzzle again; stacking solves save their search, which Continue carries on. Unchecked, a solve is kept only when you press Pause (and, for brick and sliding, save the puzzle). ");
    Autosave->clear_visible_focus();
    Autosave->callback(cb_SolverOptions_stub, this);
    Autosave->value(1);


    updateSolverOptionCheckboxes();

    o->end();

    o = new layouter_c(0, 2);

    LFl_Box * solverTypeCaption = new LFl_Box("Solver Type: ", 0, 0, 1, 1);
    solverTypeCaption->tooltip(solverTypeTooltip());

    LFl_Choice * solverType = new LFl_Choice(1, 0, 1, 1);
    solverTypeChoice = solverType;
    solverType->weight(1, 0);
    solverType->setMinimumSize(0, 25);
    solverTypeChoice->tooltip(solverTypeTooltip());
    for (unsigned int i = 0; i < solverTypeCount(); i++)
      solverTypeChoice->add(solverTypeLabel((solverType_e)i));
    solverTypeChoice->value((int)SOLVER_CLASSIC);
    /* Continue depends on the solver: the Panex Solver can carry on a saved search. */
    solverTypeChoice->callback(cb_SolverType_stub, this);

    LFl_Button * solverTypeHelp = new LFl_Button("?", 2, 0, 1, 1);
    solverTypeHelp->tooltip(" Explanation of solver types ");
    solverTypeHelp->callback(cb_SolverTypeHelp_stub, this);
    solverTypeHelp->stretchVCenter();
    solverTypeHelp->setPadding(10, 4);
    solverTypeHelp->setMinimumSize(0, 25);

    LFl_Box * sortByLabel = new LFl_Box("Sort by: ", 0, 1, 1, 1);
    sortByLabel->tooltip(" Set before solving to order saved solutions. Click ? for an explanation of each option. ");

    LFl_Choice * sortBy = new LFl_Choice(1, 1, 1, 1);
    sortMethod = sortBy;
    sortBy->weight(1, 0);
    sortBy->setMinimumSize(0, 25);
    sortMethod->tooltip(" Set before solving to order saved solutions. Click ? for an explanation of each option. ");

    // be careful the order in here must correspond with the enumeration in assembler thread
    sortMethod->add("Unsorted");
    sortMethod->add("Moves for Complete Disassembly");
    sortMethod->add("Level");
    sortMethod->add("Rotations");

    sortMethod->value(1);
    sortMethod->callback(cb_SortMethod_stub, this);

    LFl_Button * sortByHelp = new LFl_Button("?", 2, 1, 1, 1);
    sortByHelp->tooltip(" Explanation of Sort by options ");
    sortByHelp->callback(cb_SortByHelp_stub, this);
    sortByHelp->stretchVCenter();
    sortByHelp->setPadding(10, 4);
    sortByHelp->setMinimumSize(0, 25);

    o->end();

    (new LFl_Box(0, 3))->setMinimumSize(0, SZ_GAP);

    o = new layouter_c(0, 4);

    LFl_Box * dispLimitLabel = new LFl_Box("Display Limit:", 0, 0, 1, 1);
    dispLimitLabel->tooltip(" Limit the number of solutions to keep the disassembly animation for ");
    solLimit = new LFl_Value_Input(1, 0, 1, 1);
    solLimit->bounds(1, 100000000);
    solLimit->step(1, 1);
    solLimit->value(100);
    solLimit->tooltip(" Limit the number of solutions to keep the disassembly animation for ");
    static_cast<LFl_Value_Input*>(solLimit)->setMinimumSize(48, 0);

    (new LFl_Box(2, 0))->setMinimumSize(SZ_GAP, 0);

    LFl_Box * keepEachLabel = new LFl_Box("Keep Each: ", 3, 0, 1, 1);
    keepEachLabel->tooltip(" If some solutions should be dropped. 1 to keep all, higher numbers to skip that number before the next solution is kept. Used to keep a smaller sample of a large number of solutions. ");
    solDrop = new LFl_Value_Input(4, 0, 1, 1);
    solDrop->bounds(1, 100000000);
    solDrop->step(1, 1);
    solDrop->value(1);
    solDrop->tooltip(" If some solutions should be dropped. 1 to keep all, higher numbers to skip that number before the next solution is kept. Used to keep a smaller sample of a large number of solutions. ");
    static_cast<LFl_Value_Input*>(solDrop)->setMinimumSize(48, 0);

    o->end();

    (new LFl_Box(0, 5))->setMinimumSize(0, SZ_GAP);

    o = new layouter_c(0, 6);

    new LFl_Box("Solve: ", 0, 0, 1, 1);
    BtnStart = new LFlatButton_c(1, 0, 1, 1, "Solve", " Start new solving process, removing old result ", cb_BtnStart_stub, this);
    static_cast<LFlatButton_c*>(BtnStart)->weight(1, 0);
    BtnStop = new LFlatButton_c(2, 0, 1, 1, "Pause", " Pause a currently running solution process ", cb_BtnStop_stub, this);
    static_cast<LFlatButton_c*>(BtnStop)->weight(1, 0);
    BtnCont = new LFlatButton_c(3, 0, 1, 1, "Continue", " Continue started process ", cb_BtnCont_stub, this);
    static_cast<LFlatButton_c*>(BtnCont)->weight(1, 0);
    BtnAbort = new LFlatButton_c(4, 0, 1, 1, "Abort",
        " Stop at once and throw away everything kept of this problem's solve: its results, "
        "the paused state, a saved search, and the autosave copy ", cb_BtnAbort_stub, this);
    static_cast<LFlatButton_c*>(BtnAbort)->weight(1, 0);

    o->end();

    (new LFl_Box(0, 7))->setMinimumSize(0, SZ_GAP);

    o = new layouter_c(0, 8);

    new LFl_Box("Analyze: ", 0, 0, 1, 1);
    BtnPlacement = new LFlatButton_c(1, 0, 1, 1, "Placements", " Browse the calculated placement of pieces ", cb_BtnPlacementBrowser_stub, this);
    static_cast<LFlatButton_c*>(BtnPlacement)->weight(1, 0);
    BtnMovement = new LFlatButton_c(2, 0, 1, 1, "Movements", " Browse the possible movements for an assembly ", cb_BtnMovementBrowser_stub, this);
    static_cast<LFlatButton_c*>(BtnMovement)->weight(1, 0);

    o->end();

    if (expertMode) {

      (new LFl_Box(0, 9))->setMinimumSize(0, SZ_GAP);

      o = new layouter_c(0, 10);

      new LFl_Box("Debug: ", 0, 0, 1, 1);
      BtnPrepare = new LFlatButton_c(1, 0, 1, 1, "Prepare", " Do the preparation phase and then stop, this removes old results ", cb_BtnPrepare_stub, this);
      static_cast<LFlatButton_c*>(BtnPrepare)->weight(1, 0);
      BtnStep = new LFlatButton_c(2, 0, 1, 1, "Step", " Make one step in the assembler ", cb_BtnAssemblerStep_stub, this);
      static_cast<LFlatButton_c*>(BtnStep)->weight(1, 0);

      o->end();

    } else {
      BtnPrepare = 0;
      BtnStep = 0;
    }

    (new LFl_Box(0, 11))->setMinimumSize(0, SZ_GAP);

    SolvingProgress = new LFl_Progress(0, 12, 1, 1);
    SolvingProgress->tooltip(" Estimated solve progress. With Disassemble this includes take-apart work. While a take-apart is running the bar keeps moving on a time estimate and holds at 95% until the solve finishes. ");
    SolvingProgress->box(FL_ENGRAVED_BOX);
    SolvingProgress->selection_color((Fl_Color)4);
    SolvingProgress->align(FL_ALIGN_CENTER | FL_ALIGN_INSIDE);

    o = new layouter_c(0, 13);

    (new LFl_Box("Activity: ", 0, 0, 1, 1))->stretchRight();
    OutputActivity = new LFl_Output(1, 0, 3, 1);
    OutputActivity->box(FL_FLAT_BOX);
    OutputActivity->color(FL_BACKGROUND_COLOR);
    OutputActivity->tooltip(" What is currently done ");
    OutputActivity->clear_visible_focus();

    (new LFl_Box("Assemblies: ", 0, 1, 1, 1))->stretchRight();
    OutputAssemblies = new LFl_Value_Output(1, 1, 1, 1);
    OutputAssemblies->box(FL_FLAT_BOX);
    OutputAssemblies->step(1);   // make output NOT use scientific presentation for big numbers
    OutputAssemblies->tooltip(" Number of assemblies found so far ");
    static_cast<LFl_Value_Output*>(OutputAssemblies)->weight(2, 0);
    static_cast<LFl_Value_Output*>(OutputAssemblies)->setMinimumSize(48, 0);

    (new LFl_Box("Solutions: ", 0, 2, 1, 1))->stretchRight();
    OutputSolutions = new LFl_Value_Output(1, 2, 1, 1);
    OutputSolutions->box(FL_FLAT_BOX);
    OutputSolutions->step(1);    // make output NOT use scientific presentation for big numbers
    OutputSolutions->tooltip(" Number of solutions (assemblies that can be disassembled) found so far ");
    static_cast<LFl_Value_Output*>(OutputSolutions)->weight(2, 0);
    static_cast<LFl_Value_Output*>(OutputSolutions)->setMinimumSize(48, 0);

    (new LFl_Box("Time used: ", 2, 1, 1, 1))->stretchRight();
    TimeUsed = new LFl_Output(3, 1, 1, 1);
    TimeUsed->box(FL_FLAT_BOX);
    TimeUsed->color(FL_BACKGROUND_COLOR);
    TimeUsed->clear_visible_focus();
    static_cast<LFl_Output*>(TimeUsed)->weight(4, 0);
    static_cast<LFl_Output*>(TimeUsed)->setMinimumSize(90, 0);

    (new LFl_Box("Time left: ", 2, 2, 1, 1))->stretchRight();
    TimeEst = new LFl_Output(3, 2, 1, 1);
    TimeEst->box(FL_FLAT_BOX);
    TimeEst->color(FL_BACKGROUND_COLOR);
    TimeEst->clear_visible_focus();
    TimeEst->tooltip(" Approximate remaining time. With Check Rotations this also uses the average time per disassembly. Can still be far off. ");
    static_cast<LFl_Output*>(TimeEst)->setMinimumSize(90, 0);

    o->end();

    group->end();
  }

  {
    layouter_c * group = new layouter_c(0, 1);
    group->box(FL_FLAT_BOX);

    (new LFl_Box(0, 0))->setMinimumSize(0, SZ_GAP);

    layouter_c * o = new layouter_c(0, 1);

    new LFl_Box("Solution", 0, 0, 1, 1);

    // Gap between Solution and value
    (new LFl_Box(1, 0))->setMinimumSize(SZ_GAP, 0);

    SolutionsInfo = new LFl_Value_Output(2, 0, 1, 1);
    SolutionsInfo->tooltip(" Number of solutions ");
    SolutionsInfo->box(FL_FLAT_BOX);
    static_cast<LFl_Value_Output*>(SolutionsInfo)->weight(1, 0);
    static_cast<LFl_Value_Output*>(SolutionsInfo)->setMinimumSize(48, 0);

    SolutionSel = new LFl_Value_Slider(0, 1, 3, 1);
    SolutionSel->tooltip(" Select one Solution ");
    SolutionSel->value(1);
    SolutionSel->type(1);
    SolutionSel->step(1);
    SolutionSel->callback(cb_SolutionSel_stub, this);
    SolutionSel->align(FL_ALIGN_TOP_LEFT);

    (new LFl_Box(0, 3))->setMinimumSize(0, SZ_GAP);

    new LFl_Box("Move", 0, 4, 1, 1);

    MovesInfo = new LFl_Output(2, 4, 1, 1);
    MovesInfo->tooltip(" Steps for complete disassembly ");
    MovesInfo->box(FL_FLAT_BOX);
    MovesInfo->color(FL_BACKGROUND_COLOR);
    static_cast<LFl_Output*>(MovesInfo)->weight(1, 0);
    static_cast<LFl_Output*>(MovesInfo)->setMinimumSize(48, 0);

    SolutionAnim = new LFl_Value_Slider(0, 5, 3, 1);
    SolutionAnim->tooltip(" Animate the disassembly ");
    SolutionAnim->type(1);
    SolutionAnim->step(0.02);
    SolutionAnim->callback(cb_SolutionAnim_stub, this);
    SolutionAnim->align(FL_ALIGN_TOP_LEFT);

    o->end();

    (new LFl_Box(0, 2))->setMinimumSize(0, SZ_GAP);

    o = new layouter_c(0, 3);

    new LFl_Box("Assembly:", 0, 0, 1, 1);
    new LFl_Box("Solution:", 2, 0, 1, 1);

    AssemblyNumber = new LFl_Value_Output(1, 0, 1, 1);
    SolutionNumber = new LFl_Value_Output(3, 0, 1, 1);
    AssemblyNumber->box(FL_FLAT_BOX);
    SolutionNumber->box(FL_FLAT_BOX);
    AssemblyNumber->step(1);    // make output NOT use scientific presentation for big numbers
    SolutionNumber->step(1);    // make output NOT use scientific presentation for big numbers
    static_cast<LFl_Value_Output*>(AssemblyNumber)->weight(1, 0);
    static_cast<LFl_Value_Output*>(SolutionNumber)->weight(1, 0);
    static_cast<LFl_Value_Output*>(AssemblyNumber)->setMinimumSize(48, 0);
    static_cast<LFl_Value_Output*>(SolutionNumber)->setMinimumSize(48, 0);

    new LFl_Box("Moves:", 0, 1, 1, 1);
    new LFl_Box("Rotations:", 2, 1, 1, 1);

    MovesMetric = new LFl_Output(1, 1, 1, 1);
    RotationsMetric = new LFl_Output(3, 1, 1, 1);
    MovesMetric->box(FL_FLAT_BOX);
    RotationsMetric->box(FL_FLAT_BOX);
    MovesMetric->color(FL_BACKGROUND_COLOR);
    RotationsMetric->color(FL_BACKGROUND_COLOR);
    static_cast<LFl_Output*>(MovesMetric)->weight(1, 0);
    static_cast<LFl_Output*>(RotationsMetric)->weight(1, 0);
    static_cast<LFl_Output*>(MovesMetric)->setMinimumSize(48, 0);
    static_cast<LFl_Output*>(RotationsMetric)->setMinimumSize(48, 0);

    o->end();

    (new LFl_Box(0, 4))->setMinimumSize(0, SZ_GAP);

    (new LFl_Box(0, 5))->setMinimumSize(0, 22);

    new LSeparator_c(0, 6, 1, 1, "Advanced Filters", false);

    (new LFl_Box(0, 7))->setMinimumSize(0, SZ_GAP);

    o = new filterTooltipGroup_c(0, 8);

    new LFl_Box("Sort by: ", 0, 0);

    BtnSrtFind =  new LFlatButton_c(1, 0, 1, 1, "Number",
        " Reorder the saved solutions by assembly number, which is the order the covering search found them. "
        "The Solution slider stays on the same solution after sorting. Needs at least two saved solutions. ",
        cb_SrtFind_stub, this);
    static_cast<LFlatButton_c*>(BtnSrtFind)->weight(1, 0);
    (new LFl_Box(2, 0))->setMinimumSize(SZ_GAP, 0);
    BtnSrtLevel = new LFlatButton_c(3, 0, 1, 1, "Level",
        " Reorder the saved solutions by disassembly level, from lowest to highest. "
        "Level is BurrTools' classic difficulty: how many moves are needed before each piece can be removed, "
        "shown as the dotted numbers next to Move (for example 5.4.3). "
        "Only solutions that already have a disassembly are compared. Needs at least two saved solutions. ",
        cb_SrtLevel_stub, this);
    static_cast<LFlatButton_c*>(BtnSrtLevel)->weight(1, 0);
    (new LFl_Box(4, 0))->setMinimumSize(SZ_GAP, 0);
    BtnSrtMoves = new LFlatButton_c(5, 0, 1, 1, "Disarm",
        " Reorder the saved solutions by how many sliding moves it takes to completely take the puzzle apart, "
        "from fewest moves to most. This uses the Moves count from the disassembly, not rotations. "
        "Only solutions that already have a disassembly are compared. Needs at least two saved solutions. ",
        cb_SrtMoves_stub, this);
    static_cast<LFlatButton_c*>(BtnSrtMoves)->weight(1, 0);
    (new LFl_Box(6, 0))->setMinimumSize(SZ_GAP, 0);
    BtnSrtPieces = new LFlatButton_c(7, 0, 1, 1, "Pieces",
        " Reorder the saved solutions by which piece shapes are used in each assembly. "
        "This groups solutions that use the same set of pieces, which is useful when a problem allows "
        "a range of piece counts or interchangeable shapes. Needs at least two saved solutions. ",
        cb_SrtPieces_stub, this);
    static_cast<LFlatButton_c*>(BtnSrtPieces)->weight(1, 0);

    o->end();

    (new LFl_Box(0, 9))->setMinimumSize(0, SZ_GAP);

    o = new filterTooltipGroup_c(0, 10);

    new LFl_Box("Delete: ", 0, 0);

    BtnDelAll =    new LFlatButton_c(1, 0, 1, 1, "All",
        " Permanently delete every saved solution for this problem, including their assemblies and take-apart sequences. "
        "This cannot be undone. The Solve/Continue search state is not reset; only the stored solution list is cleared. ",
        cb_DelAll_stub, this);
    static_cast<LFlatButton_c*>(BtnDelAll)->weight(1, 0);
    (new LFl_Box(2, 0))->setMinimumSize(SZ_GAP, 0);
    BtnDelBefore = new LFlatButton_c(3, 0, 1, 1, "Before",
        " Permanently delete every saved solution that appears before the one currently selected on the Solution slider. "
        "The selected solution and everything after it are kept. This cannot be undone. ",
        cb_DelBefore_stub, this);
    static_cast<LFlatButton_c*>(BtnDelBefore)->weight(1, 0);
    (new LFl_Box(4, 0))->setMinimumSize(SZ_GAP, 0);
    BtnDelAt =     new LFlatButton_c(5, 0, 1, 1, "At",
        " Permanently delete only the solution currently selected on the Solution slider. "
        "The next remaining solution becomes selected. This cannot be undone. ",
        cb_DelAt_stub, this);
    static_cast<LFlatButton_c*>(BtnDelAt)->weight(1, 0);
    (new LFl_Box(6, 0))->setMinimumSize(SZ_GAP, 0);
    BtnDelAfter =  new LFlatButton_c(7, 0, 1, 1, "After",
        " Permanently delete every saved solution that appears after the one currently selected on the Solution slider. "
        "The selected solution and everything before it are kept. This cannot be undone. ",
        cb_DelAfter_stub, this);
    static_cast<LFlatButton_c*>(BtnDelAfter)->weight(1, 0);
    (new LFl_Box(8, 0))->setMinimumSize(SZ_GAP, 0);
    BtnDelDisasm = new LFlatButton_c(9, 0, 1, 1, "w/o DA",
        " Without disassembly: permanently delete every saved solution that has no take-apart sequence. "
        "That includes assemblies the disassembler could not take apart, and assemblies saved without running it. "
        "Solutions that already have a disassembly are kept. This cannot be undone. ",
        cb_DelDisasmless_stub, this);
    static_cast<LFlatButton_c*>(BtnDelDisasm)->weight(1, 0);

    o->end();

    (new LFl_Box(0, 11))->setMinimumSize(0, SZ_GAP);

    o = new filterTooltipGroup_c(0, 12);

    BtnDisasmDel    = new LFlatButton_c(0, 0, 1, 1, "D DA",
        " Delete disassembly: remove the take-apart sequence from the currently selected solution only. "
        "The assembly stays in the list, so you can still see how the pieces fit together, "
        "but Move playback and the Moves/Rotations/level numbers for this solution will be empty until you recalculate (A DA). ",
        cb_DelDisasm_stub, this);
    static_cast<LFlatButton_c*>(BtnDisasmDel)->weight(1, 0);
    (new LFl_Box(1, 0))->setMinimumSize(SZ_GAP, 0);
    BtnDisasmDelAll = new LFlatButton_c(2, 0, 1, 1, "D A DA",
        " Delete all disassemblies: remove the take-apart sequence from every saved solution. "
        "The assemblies stay in the list. Use this to drop bulky disassembly data, "
        "or before recalculating everything with different rotation settings (A A DA). ",
        cb_DelAllDisasm_stub, this);
    static_cast<LFlatButton_c*>(BtnDisasmDelAll)->weight(1, 0);
    (new LFl_Box(3, 0))->setMinimumSize(SZ_GAP, 0);
    BtnDisasmAdd    = new LFlatButton_c(4, 0, 1, 1, "A DA",
        " Add disassembly: run the disassembler on the currently selected assembly and store the take-apart sequence "
        "so you can animate it and see Moves, Rotations, and level. "
        "If Check Rotations is on, rotational moves are allowed. Replaces any existing disassembly for this solution. "
        "This can take a while on hard puzzles. ",
        cb_AddDisasm_stub, this);
    static_cast<LFlatButton_c*>(BtnDisasmAdd)->weight(1, 0);
    (new LFl_Box(5, 0))->setMinimumSize(SZ_GAP, 0);
    BtnDisasmAddAll = new LFlatButton_c(6, 0, 1, 1, "A A DA",
        " Add all disassemblies: recalculate a take-apart sequence for every saved solution, replacing any that already exist. "
        "A progress window is shown. If Check Rotations is on, rotational moves are allowed. "
        "This can take a long time when there are many solutions. ",
        cb_AddAllDisasm_stub, this);
    static_cast<LFlatButton_c*>(BtnDisasmAddAll)->weight(1, 0);
    (new LFl_Box(7, 0))->setMinimumSize(SZ_GAP, 0);
    BtnDisasmAddMissing=new LFlatButton_c(8, 0, 1, 1, "A M DA",
        " Add missing disassemblies: run the disassembler only on saved solutions that do not already have a take-apart sequence. "
        "Existing disassemblies are left unchanged. If Check Rotations is on, rotational moves are allowed. "
        "Use this after a solve that skipped disassembly, or after deleting disassemblies (D DA / D A DA). ",
        cb_AddMissingDisasm_stub, this);
    static_cast<LFlatButton_c*>(BtnDisasmAddMissing)->weight(1, 0);

    o->end();

    (new LFl_Box(0, 13))->setMinimumSize(0, SZ_GAP);

    o = new filterTooltipGroup_c(0, 14);

    BtnExportSolutionSTL = new LFlatButton_c(0, 0, 1, 1, "Export Solution Pieces to STL",
        " Open a dialog to export each piece type used in the currently selected solution as its own STL file, "
        "using that solution's placement and orientation. Useful for 3D printing the pieces of one particular solution. ",
        cb_ExportSolutionSTL_stub, this);
    static_cast<LFlatButton_c*>(BtnExportSolutionSTL)->weight(1, 0);

    o->end();

    (new LFl_Box(0, 15))->setMinimumSize(0, SZ_GAP);

    PcVis = new PieceVisibility(0, 0, 100, 100);
    LBlockListGroup_c * shapeGroup = new LBlockListGroup_c(0, 16, 1, 1, PcVis);
    shapeGroup->callback(cb_PcVis_stub, this);
    shapeGroup->tooltip(" Change appearance of the pieces between normal, grid and invisible ");
    shapeGroup->weight(1, 1);

    group->end();
  }
  tile->end();

  TabSolve->resizable(scroll);
  TabSolve->end();
}

void mainWindow_c::CreateDebugTab(void) {

  TabDebug = new layouter_c();
  TabDebug->label("  Debug  ");
  TabDebug->labelsize(MAIN_TAB_LABELSIZE);
  TabDebug->tooltip("Solver debug statistics");
  TabDebug->hide();
  TabDebug->clear_visible_focus();
  TabDebug->end();
}

void mainWindow_c::attachSolverPane(Fl_Group *tab) {

  if (!solverPane || !tab)
    return;

  if (solverPane->parent() != tab) {
    Fl_Group *old = solverPane->parent();
    if (old)
      old->remove(solverPane);
    tab->add(solverPane);
    tab->resizable(solverPane);
  }

  /* Position below the tab strip. Using the full tab rectangle puts
   * the solver controls under the tab labels until the next resize. */
  tab->resize(tab->x(), tab->y(), tab->w(), tab->h());
}

void mainWindow_c::showDebugRightPane(void) {

  if (View3D)
    View3D->hide();
  if (debugPanel) {
    debugPanel->show();
    updateDebugStats();
    debugPanel->takeFocus();
  }
  relayoutViewStack();
}

void mainWindow_c::hideDebugRightPane(void) {

  if (debugPanel)
    debugPanel->hide();
  if (View3D)
    View3D->show();
  relayoutViewStack();
}

void mainWindow_c::applyDebugTabVisibility(void) {

  if (!TabDebug || !TaskSelectionTab)
    return;

  const bool want = config.debugStatistics();
  const bool present = (TabDebug->parent() == TaskSelectionTab);

  if (want && !present) {
    Fl_Widget *cur = TaskSelectionTab->value();
    TaskSelectionTab->add(TabDebug);
    /* Inactive tab panels stay hidden; Fl_Tabs shows the label. */
    TabDebug->hide();
    if (cur)
      TaskSelectionTab->value(cur);
  } else if (!want && present) {
    if (TaskSelectionTab->value() == TabDebug) {
      attachSolverPane(TabSolve);
      hideDebugRightPane();
      TaskSelectionTab->value(TabSolve);
    }
    TaskSelectionTab->remove(TabDebug);
    TabDebug->hide();
  }

  TaskSelectionTab->redraw();
  TaskSelectionTab->resize(TaskSelectionTab->x(), TaskSelectionTab->y(),
                           TaskSelectionTab->w(), TaskSelectionTab->h());
}

void mainWindow_c::updateDebugStats(void) {

  if (!debugPanel)
    return;
  if (assmThread)
    lastSolveStats = assmThread->getStats();
  debugPanel->showStats(lastSolveStats);
}

void mainWindow_c::activateConfigOptions(void) {

  /* for every solve started from now on; one that is running keeps the
   * threads it has */
  setSolveThreadLimit((unsigned int)config.solverThreads());

  if (config.useTooltips())
    Fl_Tooltip::enable();
  else
    Fl_Tooltip::disable();

  View3D->getView()->useLightning(config.useLightning());
  View3D->getView()->setRotaterMethod(config.rotationMethod());
  View3D->getView()->setDebugRotations(config.debugRotations());
  applyDebugTabVisibility();
  if (disassemble && SolutionAnim) {
    disassemble->setStep(animStep(), config.useBlendedRemoving(), true);
    View3D->getView()->updatePositions(disassemble.get());
  }
}

mainWindow_c::mainWindow_c(gridType_c * gt) : LFl_Double_Window(true) {

  editSymmetries = 0;
  expertMode = true;
  view3DStack = 0;
  rightPane = 0;
  detailsPanel = 0;
  debugPanel = 0;
  TabDebug = 0;
  solverPane = 0;
  notesUpdate = 0;
  notesRevert = 0;
  contentTile = 0;

  puzzle = new puzzle_c(gt);
  shapeHistory = new shapeHistory_c();
  shapeHistory->reset(puzzle);
  ggt = new guiGridType_c(puzzle->getGridType());
  changed = false;
  handlingSystemOpen = false;
  menuExportActive = true;
  menuSTLActive = true;
  BtnUndo = 0;
  BtnRedo = 0;
  BtnNewStartGoal = 0;
  BtnDelStartGoal = 0;
  startGoalRow = 0;
  startGoalGap = 0;
  colorsGroup = 0;
  colourAssignmentGroup = 0;
  colourConstraintsGroup = 0;
  slidingEditMode = 0;
  slidingDisasmDefaulted = false;
  stackingDisasmDefaulted = false;
  solverMenuMode = -1;
  TabRods = 0;
  shapeEditColumn = 0;
  voxelPenRow = 0;
  voxelToolGap = 0;
  voxelEditGap = 0;
  discTabs = 0;
  discInfo = 0;
  diskSizeInput = 0;
  diskSizeGuard = false;
  rodSel = 0;
  rodAssignSel = 0;
  rodAssignGroup = 0;
  rodCountInput = 0;
  rodGrow = 0;
  rodFixed = 0;
  rodHeightInput = 0;
  rodSizeMatters = 0;
  rodDistance = 0;
  rodPanex = 0;
  rodPocket = 0;
  rodPocketHeight = 0;
  rodFieldGuard = false;
  BtnNewRod = BtnDelRod = BtnCpyRod = BtnRenRod = 0;
  BtnRodLeft = BtnRodRight = BtnRodUndo = BtnRodRedo = 0;
  pieceSelGroup = 0;
  diskList = 0;
  rodsPanel = 0;
  stackStartMode = 0;
  stackGoalMode = 0;
  stackRodBar = 0;
  stackSliderRow = 0;
  stackModeRow = 0;
  rodStackList = 0;
  piecesCountGroup = 0;
  stackOrderSep = 0;
  problemButtonRule = 0;
  stackValidRow = 0;
  stackValidBar = 0;
  NestedSlides = 0;
  HighMemory = 0;
  Autosave = 0;
  probArrowGapL[0] = probArrowGapL[1] = 0;
  probArrowGapR[0] = probArrowGapR[1] = 0;
  probArrowRightX = 0;
  probArrowsStacking = false;
  stackingEditGoal = false;

  copy_label(platform::windowTitle(0).c_str());
  user_data((void*)(this));

  /* original comment dialog is 400x200; its text box is the window
   * minus gaps and the button row. The notes panel uses that width
   * as a starting size; the text area shrinks first so Update/Revert
   * stay on screen when the window is short.
   */
  static const int NOTES_WIDTH = 400;
  static const int NOTES_SHRINK_W = NOTES_WIDTH / 5;
  static const int NOTES_TEXT_MIN_H = 48;

  int notesBtnTw = 0, notesBtnTh = 0;
  fl_font(FL_HELVETICA, FL_NORMAL_SIZE);
  fl_measure("Update", notesBtnTw, notesBtnTh);
  const int notesBtnH = notesBtnTh + 10;
  const int notesMinH = NOTES_TEXT_MIN_H + 5 + notesBtnH + 8;
  const int notesButtonsFloorH = notesBtnH + 8;

#ifdef __APPLE__
  /* The system menu bar is not a layout child, so do not reserve a row
   * for it. Content starts at the top of the window.
   */
  LFl_Sys_Menu_Bar * menuBar = new LFl_Sys_Menu_Bar(0, 0, 1, 1);
  MainMenu = menuBar;
  menuBar->copy(mainmenu::table(), this);
  initViewMenuIcons();
  menuBar->update();
  mainmenu::installApplicationMenu(this);

  StatusLine = new LStatusLine(0, 1, 1, 1);
  StatusLine->weight(1, 0);
  syncRenderStyleMenu();

  layouter_c * contentRow = new LFl_Tile(0, 0, 1, 1);
#else
  LFl_Menu_Bar * menuBar = new LFl_Menu_Bar(0, 0, 1, 1);
  MainMenu = menuBar;
  menuBar->copy(mainmenu::table(), this);
  initViewMenuIcons();
  menuBar->update();
  mainmenu::installApplicationMenu(this);
  menuBar->weight(1, 0);

  StatusLine = new LStatusLine(0, 2, 1, 1);
  StatusLine->weight(1, 0);
  syncRenderStyleMenu();

  layouter_c * contentRow = new LFl_Tile(0, 1, 1, 1);
#endif
  contentTile = static_cast<LFl_Tile*>(contentRow);
  contentRow->weight(1, 1);
  /* Let this row shrink below the tabs/3D preferred height so the notes
   * panel Update/Revert row can stay in the visible window. */
  contentRow->setShrinkMinSize(0, notesButtonsFloorH);

  /* Regular layouter, not a tile: the left bar stays at the tab-header
   * width and cannot be dragged horizontally. Extra width goes to the 3D view. */
  layouter_c * mainTile = new layouter_c(0, 0, 1, 1);
  mainTile->weight(1, 1);

  static const int VIEW3D_MIN = 400;
  static const int VIEW3D_SHRINK_MIN = VIEW3D_MIN * 3 / 10;

  rightPane = new LFl_Tile(1, 0, 1, 1);
  rightPane->weight(1, 1);
  rightPane->setMinimumSize(VIEW3D_MIN, VIEW3D_MIN);
  rightPane->setShrinkMinSize(VIEW3D_SHRINK_MIN, 0);
  rightPane->shrinkPrio(0, 128);

  view3DStack = new layouter_c(0, 0, 1, 1);
  view3DStack->weight(1, 1);
  view3DStack->setMinimumSize(VIEW3D_MIN, 80);
  view3DStack->setShrinkMinSize(VIEW3D_SHRINK_MIN, 40);
  view3DStack->shrinkPrio(0, 0);
  view3DStack->clip_children(1);

  View3D = new LView3dGroup(0, 0, 1, 1);
  View3D->weight(1, 1);
  View3D->callback(cb_3dClick_stub, this);

  debugPanel = new debugStatsPanel_c(0, 0, 1, 1);
  debugPanel->weight(1, 1);
  debugPanel->hide();

  view3DStack->end();

  detailsPanel = new statusWindow_c(0, 1, 1, 1);
  detailsPanel->weight(1, 0);
  detailsPanel->setMinimumSize(200, 250);
  detailsPanel->setShrinkMinSize(VIEW3D_SHRINK_MIN, 180);
  detailsPanel->shrinkPrio(0, 200);
  detailsPanel->setCallbacks(cb_DetailsClose_stub, cb_DetailsChanged_stub, this);
  detailsPanel->hide();

  rightPane->end();

  // this box paints the background behind the tab, because the tabs are partly transparent
  (new LFl_Box(0, 0, 1, 1))->color(FL_BACKGROUND_COLOR);

  // the tab for the tool bar
  const int barW = leftBarWidth();
  LFl_Tabs * tabs = new LFl_Tabs(0, 0, 1, 1);
  TaskSelectionTab = tabs;
  tabs->callback(cb_TaskSelectionTab_stub, this);
  tabs->labelsize(MAIN_TAB_LABELSIZE);
  tabs->clear_visible_focus();
  tabs->weight(0, 1);
  tabs->setMinimumSize(barW, 140);
  tabs->setShrinkMinSize(barW, 140);

  // the three tabs
  CreateShapeTab();
  CreateProblemTab();
  CreateSolveTab();
  CreateDebugTab();

  TaskSelectionTab->end();
  applyDebugTabVisibility();
  mainTile->end();

  notesPanel = new layouter_c(1, 0, 1, 1);
  notesPanel->setMinimumSize(NOTES_WIDTH, notesMinH);
  notesPanel->setShrinkMinSize(NOTES_SHRINK_W, notesButtonsFloorH);
  notesPanel->shrinkPrio(0, 128);
  notesPanel->pitch(4);
  notesPanel->weight(0, 1);
  notesPanel->clip_children(1);

  notesInput = new LFl_Text_Editor(0, 0, 1, 1);
  notesInput->weight(1, 1);
  notesInput->setMinimumSize(NOTES_SHRINK_W, NOTES_TEXT_MIN_H);
  notesInput->setShrinkMinSize(NOTES_SHRINK_W, 1);
  notesInput->shrinkPrio(0, 0);
  notesInput->value(puzzle->getComment().c_str());
  notesInput->when(FL_WHEN_CHANGED);
  notesInput->callback(cb_NotesChanged_stub, this);

  LFl_Box * notesGap = new LFl_Box(0, 1, 1, 1);
  notesGap->setMinimumSize(0, 5);
  notesGap->setShrinkMinSize(0, 1);
  notesGap->shrinkPrio(0, 64);

  layouter_c * notesButtons = new layouter_c(0, 2, 1, 1);
  notesButtons->weight(1, 0);
  notesButtons->setMinimumSize(NOTES_SHRINK_W, notesBtnH);
  notesButtons->setShrinkMinSize(NOTES_SHRINK_W, notesBtnH);
  notesButtons->shrinkPrio(255, 255);

  {
    int bw = 2 * (notesBtnTw + 4);

    LFl_Box * leftPad = new LFl_Box(0, 0);
    leftPad->weight(1, 0);

    notesUpdate = new LFl_Button("Update", 1, 0);
    notesUpdate->callback(cb_NotesUpdate_stub, this);
    notesUpdate->tooltip(" Save the current notes ");
    notesUpdate->setMinimumSize(bw, notesBtnH);

    (new LFl_Box(2, 0))->setMinimumSize(12, 0);

    notesRevert = new LFl_Button("Revert", 3, 0);
    notesRevert->callback(cb_NotesRevert_stub, this);
    notesRevert->tooltip(" Revert notes to the last saved version ");
    notesRevert->setMinimumSize(bw, notesBtnH);

    LFl_Box * rightPad = new LFl_Box(4, 0);
    rightPad->weight(1, 0);
  }

  notesButtons->end();
  setNotesButtonsEnabled(false);

  notesPanel->end();
  notesPanel->hide();

  contentRow->end();

  setResizeMin(120, 80);

  currentTab = 0;
  ViewSizes[0] = -1;
  ViewSizes[1] = -1;
  ViewSizes[2] = -1;
  ViewSizes[3] = -1;
  resetRodHistory();
  loadRodFields();

  /* Saved size is restored, but a previous session may have left a tiny
   * window. Floor at the default so the app does not open cramped. */
  static const int DEFAULT_WINDOW_W = 1200;
  static const int DEFAULT_WINDOW_H = 800;
  int ww = config.windowPosW();
  int wh = config.windowPosH();
  if (ww < DEFAULT_WINDOW_W)
    ww = DEFAULT_WINDOW_W;
  if (wh < DEFAULT_WINDOW_H)
    wh = DEFAULT_WINDOW_H;
  resize(config.windowPosX(), config.windowPosY(), ww, wh);

  if (!config.useRubberband())
    editMode->select(1);
  else
    editMode->select(0);

  is3DViewBig = true;
  shapeEditorWithBig3DView = true;

  updateInterface();
  activateClear();

  // set the edit mode from the selected mode
  cb_EditChoice();

  activateConfigOptions();
}

mainWindow_c::~mainWindow_c() {

  config.windowPos(x(), y(), w(), h());

  /* The solver works on a problem of the puzzle: stop it first. */
  assmThread.reset();

  delete puzzle;
  delete shapeHistory;



  disassemble.reset();
  shownSolution.reset();

  if (ggt)
    delete ggt;

  if (TabDebug && TabDebug->parent() != TaskSelectionTab) {
    delete TabDebug;
    TabDebug = 0;
  }
}
