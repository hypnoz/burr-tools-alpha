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
#include "piececolor.h"
#include <FL/Fl_Button.H>
#include <FL/Fl_Box.H>
#include <FL/Fl_Double_Window.H>
#include <FL/Fl_Return_Button.H>
#include "statuswindow.h"
#include "debugstatspanel.h"
#include "tooltabs.h"
#include "WindowWidgets.h"
#include "BlockList.h"
#include "Images.h"

#include "../lib/sliding.h"
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


/* 1234567 as "1,234,567", for the Activity line. */
static std::string withCommas(unsigned long n) {
  std::string digits = std::to_string(n);
  std::string out;
  for (size_t i = 0; i < digits.size(); i++) {
    if (i > 0 && (digits.size() - i) % 3 == 0)
      out += ',';
    out += digits[i];
  }
  return out;
}

static int liveMenuIndex(const Fl_Menu_ * m, Fl_Callback * cb) {
  const Fl_Menu_Item * items = m->menu();
  if (!items) return -1;
  for (int i = 0; i < m->size(); i++)
    if (items[i].callback() == cb) return i;
  return -1;
}

static void setLiveMenuActive(Fl_Menu_ * m, int index, bool active) {
  bt_assert(index >= 0);
  int flags = m->mode(index);
  if (active)
    flags &= ~FL_MENU_INACTIVE;
  else
    flags |= FL_MENU_INACTIVE;
  m->mode(index, flags);
}

/* returns true, if file exists, this is not the
 optimal way to do this. It would be better to open
 the directory the file is supposed to be in and look there
 but this is not really portable so this
 */
bool fileExists(const char *n) {

  FILE *f = fopen(n, "r");

  if (f) {
    fclose(f);
    return true;
  } else
    return false;
}

/* A colour dialog: FLTK's colour chooser with OK, Cancel and, with
 * withReset, Reset (for a shape: the colour that goes with its number).
 * It starts at r, g, b; the bar on the left shows beforeRGB (0xRRGGBB), or
 * that same colour when it is -1. 1 for OK, with the colour in r, g, b; 2
 * for Reset; 0 for Cancel. */
static int colorDialog(const char * title, unsigned char & r, unsigned char & g, unsigned char & b,
                       bool withReset, int beforeRGB = -1) {

  Fl_Double_Window win(300, 245, title);
  Fl_Color_Chooser chooser(10, 10, 280, 160);
  chooser.rgb(r / 255.0, g / 255.0, b / 255.0);

  /* before and after, as fl_color_chooser shows them; a click on before
   * takes that colour */
  class swatch_c : public Fl_Box {
  public:
    swatch_c(int X, int Y, int W, int H) : Fl_Box(X, Y, W, H) {}
    int handle(int event) override {
      if (event == FL_PUSH) {
        do_callback();
        return 1;
      }
      return Fl_Box::handle(event);
    }
  };
  swatch_c before(10, 176, 137, 24);
  before.box(FL_DOWN_BOX);
  before.color(beforeRGB < 0 ? fl_rgb_color(r, g, b)
                             : fl_rgb_color((unsigned char)(beforeRGB >> 16), (unsigned char)(beforeRGB >> 8),
                                            (unsigned char)beforeRGB));
  before.tooltip(" Click to take this colour ");
  before.callback([](Fl_Widget * w, void * a) {
    unsigned char cr, cg, cb;
    Fl::get_color(w->color(), cr, cg, cb);
    Fl_Color_Chooser * c = static_cast<Fl_Color_Chooser *>(a);
    c->rgb(cr / 255.0, cg / 255.0, cb / 255.0);
    c->do_callback();
  }, &chooser);
  Fl_Box after(153, 176, 137, 24);
  after.box(FL_DOWN_BOX);
  after.color(fl_rgb_color(r, g, b));
  chooser.callback([](Fl_Widget * w, void * a) {
    const Fl_Color_Chooser * c = static_cast<Fl_Color_Chooser *>(w);
    Fl_Box * box = static_cast<Fl_Box *>(a);
    box->color(fl_rgb_color((unsigned char)(c->r() * 255 + 0.5), (unsigned char)(c->g() * 255 + 0.5),
                            (unsigned char)(c->b() * 255 + 0.5)));
    box->redraw();
  }, &after);

  int result = -1;
  Fl_Return_Button ok(10, 210, 88, 26, "OK");
  Fl_Button cancel(106, 210, 88, 26, "Cancel");
  Fl_Button reset(202, 210, 88, 26, "Reset");
  reset.tooltip(" Back to the colour BurrTools gives this shape ");
  if (!withReset)
    reset.hide();
  /* argument() is kept where the callback's data is, so the result goes
   * through the window's user data */
  win.user_data(&result);
  auto pick = [](Fl_Widget * w, long which) {
    *static_cast<int *>(w->window()->user_data()) = (int)which;
    w->window()->hide();
  };
  ok.callback(pick, 1);
  cancel.callback(pick, 0);
  reset.callback(pick, 2);
  win.end();
  win.set_modal();
  win.show();
  while (win.shown())
    Fl::wait();

  if (result == 1) {
    r = (unsigned char)(chooser.r() * 255 + 0.5);
    g = (unsigned char)(chooser.g() * 255 + 0.5);
    b = (unsigned char)(chooser.b() * 255 + 0.5);
  }
  return result < 0 ? 0 : result;
}

void cb_AddColor_stub(Fl_Widget* /*o*/, void* v) { static_cast<mainWindow_c*>(v)->cb_AddColor(); }
void mainWindow_c::cb_AddColor(void) {

  /* A new colour starts from white. */
  unsigned char r = 255, g = 255, b = 255;

  if (colorDialog("New colour", r, g, b, false) == 1) {
    puzzle->addColor(r, g, b);

    // add this color as a default to the matrix, so that
    // pieces of color x can be plces into cubes of color x
    int col = puzzle->colorNumber();
    for (unsigned int p = 0; p < puzzle->getNumberOfProblems(); p++)
      puzzle->getProblem(p)->allowPlacement(col, col);

    colorSelector->setSelection(puzzle->colorNumber());
    changed = true;
    View3D->getView()->showColors(puzzle, StatusLine->getColorMode());
    updateInterface();
  }
}

void cb_ShapeColor_stub(Fl_Widget* /*o*/, void* v) { static_cast<mainWindow_c*>(v)->cb_ShapeColor(); }
void mainWindow_c::cb_ShapeColor(void) {

  const unsigned int shape = PcSel->getSelection();
  if (shape >= puzzle->getNumberOfShapes())
    return;
  voxel_c * v = puzzle->getShape(shape);
  /* A start/goal shape is the tray: its cells show the pieces' colours. */
  if (sliding::isSliding(*puzzle) && sliding::isStartGoalShape(v))
    return;

  /* Starts at white, full brightness, as Add does; the bar on the left
   * shows the colour the shape has now. */
  unsigned char r = 255, g = 255, b = 255;
  const int now = (int)((pieceColorRi(shape) << 16) | (pieceColorGi(shape) << 8) | pieceColorBi(shape));

  const int choice = colorDialog("Shape colour", r, g, b, true, now);
  if (choice != 0) {
    v->setShapeColor(choice == 2 ? -1 : (r << 16) | (g << 8) | b);
    changed = true;
    syncPieceColors();
    activateShape(shape);
    updateInterface();
    redraw();
  }
}

void mainWindow_c::syncPieceColors(void) {
  std::vector<int> own(puzzle ? puzzle->getNumberOfShapes() : 0, -1);
  for (unsigned int s = 0; s < own.size(); s++)
    own[s] = puzzle->getShape(s)->getShapeColor();
  setOwnPieceColors(own);
}

void cb_RemoveColor_stub(Fl_Widget* /*o*/, void* v) { static_cast<mainWindow_c*>(v)->cb_RemoveColor(); }
void mainWindow_c::cb_RemoveColor(void) {

  if (colorSelector->getSelection() == 0)
    fl_message("Can not delete the Neutral colour, this colour has to be there");
  else {
    changeColor(colorSelector->getSelection());
    puzzle->removeColor(colorSelector->getSelection());

    unsigned int current = colorSelector->getSelection();

    while ((current > 0) && (current > puzzle->colorNumber()))
      current--;

    colorSelector->setSelection(current);

    changed = true;
    View3D->getView()->showColors(puzzle, StatusLine->getColorMode());
    activateShape(PcSel->getSelection());
    updateInterface();
  }
}

void cb_ChangeColor_stub(Fl_Widget* /*o*/, void* v) { static_cast<mainWindow_c*>(v)->cb_ChangeColor(); }
void mainWindow_c::cb_ChangeColor(void) {

  if (colorSelector->getSelection() == 0)
    fl_message("Can not edit the Neutral colour");
  else {
    unsigned char r, g, b;
    puzzle->getColor(colorSelector->getSelection()-1, &r, &g, &b);
    if (colorDialog("Change colour", r, g, b, false) == 1) {
      puzzle->changeColor(colorSelector->getSelection()-1, r, g, b);
      changed = true;
      View3D->getView()->showColors(puzzle, StatusLine->getColorMode());
      updateInterface();
    }
  }
}

void cb_NewShape_stub(Fl_Widget* /*o*/, void* v) { static_cast<mainWindow_c*>(v)->cb_NewShape(); }
void mainWindow_c::cb_NewShape(void) {

  if (puzzle && stacking::isStacking(*puzzle)) {
    unsigned int id = stacking::addDisk(*puzzle, 1);
    if (View3D)
      View3D->getView()->lookFront();
    activateShape(id);
    updateInterface();
    StatPieceInfo(id);
    recordShapeAction(shapeHistory_c::AK_STRUCTURAL);
    return;
  }

  if (PcSel->getSelection() < puzzle->getNumberOfShapes()) {
    const voxel_c * v = puzzle->getShape(PcSel->getSelection());
    unsigned int z = sliding::isSliding(*puzzle) ? 1 : v->getZ();
    PcSel->setSelection(puzzle->addShape(v->getX(), v->getY(), z));
  } else {
    unsigned int s = ggt->defaultSize();
    unsigned int z = sliding::isSliding(*puzzle) ? 1 : s;
    PcSel->setSelection(puzzle->addShape(s, s, z));
  }
  pieceEdit->setZ(0);
  updateInterface();
  StatPieceInfo(PcSel->getSelection());
  recordShapeAction(shapeHistory_c::AK_STRUCTURAL);
}

void cb_DeleteShape_stub(Fl_Widget* /*o*/, void* v) { static_cast<mainWindow_c*>(v)->cb_DeleteShape(); }
void mainWindow_c::cb_DeleteShape(void) {

  unsigned int current = PcSel->getSelection();

  if (current < puzzle->getNumberOfShapes()) {

    if (sliding::isSliding(*puzzle))
      sliding::noteShapeRemoved(*puzzle, current);

    puzzle->removeShape(current);

    if (sliding::isSliding(*puzzle))
      sliding::syncSlidingProblems(*puzzle);

    if (puzzle->getNumberOfShapes() == 0)
      current = (unsigned int)-1;
    else
      while (current >= puzzle->getNumberOfShapes())
        current--;

    activateShape(current);

    PcSel->setSelection(current);
    updateInterface();
    StatPieceInfo(PcSel->getSelection());

    recordShapeAction(shapeHistory_c::AK_STRUCTURAL);

  } else

    fl_message("No shape to delete selected!");

}

void cb_CopyShape_stub(Fl_Widget* /*o*/, void* v) { static_cast<mainWindow_c*>(v)->cb_CopyShape(); }
void mainWindow_c::cb_CopyShape(void) {

  unsigned int current = PcSel->getSelection();

  if (current < puzzle->getNumberOfShapes()) {

    unsigned int id = puzzle->addShape(puzzle->getGridType()->getVoxel(puzzle->getShape(current)));
    voxel_c * copy = puzzle->getShape(id);
    if (sliding::isStartGoalShape(copy)) {
      int next = 1;
      for (unsigned int i = 0; i < puzzle->getNumberOfShapes(); i++) {
        if (i == id) continue;
        int n = sliding::startGoalNumber(puzzle->getShape(i));
        if (n >= next) next = n + 1;
      }
      std::string user = sliding::startGoalUserName(copy);
      copy->setName(std::string("sg:") + std::to_string(next) + ":" + user);
      sliding::syncSlidingProblems(*puzzle);
    }
    PcSel->setSelection(id);
    recordShapeAction(shapeHistory_c::AK_STRUCTURAL);

    updateInterface();
    StatPieceInfo(PcSel->getSelection());

  } else

    fl_message("No shape to copy selected!");

}

void cb_NameShape_stub(Fl_Widget* /*o*/, void* v) { static_cast<mainWindow_c*>(v)->cb_NameShape(); }
void mainWindow_c::cb_NameShape(void) {

  if (PcSel->getSelection() < puzzle->getNumberOfShapes()) {

    const char * name = fl_input("Enter name for the shape",
        sliding::isStartGoalShape(puzzle->getShape(PcSel->getSelection()))
          ? sliding::startGoalUserName(puzzle->getShape(PcSel->getSelection())).c_str()
          : puzzle->getShape(PcSel->getSelection())->getName().c_str());

    if (name) {
      voxel_c * sh = puzzle->getShape(PcSel->getSelection());
      if (sliding::isStartGoalShape(sh))
        sliding::setStartGoalUserName(sh, name);
      else
        sh->setName(name);
      recordShapeAction(shapeHistory_c::AK_STRUCTURAL);
      updateInterface();
    }
  }
}

void cb_WeightInc_stub(Fl_Widget* /*o*/, void* v) { static_cast<mainWindow_c*>(v)->cb_WeightChange(1); }
void cb_WeightDec_stub(Fl_Widget* /*o*/, void* v) { static_cast<mainWindow_c*>(v)->cb_WeightChange(-1); }
void mainWindow_c::cb_WeightChange(int by) {

  if (PcSel->getSelection() < puzzle->getNumberOfShapes()) {

    voxel_c * v = puzzle->getShape(PcSel->getSelection());
    v->setWeight(v->getWeight() + by);
    recordShapeAction(shapeHistory_c::AK_STRUCTURAL);
    updateInterface();
  }
}


void cb_TaskSelectionTab_stub(Fl_Widget* o, void* v) { static_cast<mainWindow_c*>(v)->cb_TaskSelectionTab(static_cast<Fl_Tabs*>(o)); }
void mainWindow_c::cb_TaskSelectionTab(Fl_Tabs* o) {

  if (o->value() == TabPieces) {
    relayoutTab(TabPieces);
    activateShape(PcSel->getSelection());
    StatPieceInfo(PcSel->getSelection());
    if (!shapeEditorWithBig3DView)
      Small3DView();
    else
      Big3DView();
    hideDebugRightPane();
    ViewSizes[currentTab] = View3D->getZoom();
    if (ViewSizes[0] >= 0)
      View3D->setZoom(ViewSizes[0]);
    currentTab = 0;
  } else if(o->value() == TabProblems) {
    relayoutTab(TabProblems);

    // make sure the selector has a valid problem selected, when there is one
    if (puzzle->getNumberOfProblems() && (problemSelector->getSelection() >= puzzle->getNumberOfProblems()))
      problemSelector->setSelection(puzzle->getNumberOfProblems()-1);

    if (problemSelector->getSelection() < puzzle->getNumberOfProblems()) {
      activateProblem(problemSelector->getSelection());
      selectProblemRod(problemSelector->getSelection());
    }
    StatProblemInfo(problemSelector->getSelection());

    Big3DView();
    hideDebugRightPane();
    ViewSizes[currentTab] = View3D->getZoom();
    if (ViewSizes[1] >= 0)
      View3D->setZoom(ViewSizes[1]);
    currentTab = 1;
  } else if(o->value() == TabSolve) {
    relayoutTab(TabSolve);

    attachSolverPane(TabSolve);

    // make sure the selector has a valid problem selected, when there is one
    if (puzzle->getNumberOfProblems() && (solutionProblem->getSelection() >= puzzle->getNumberOfProblems()))
      solutionProblem->setSelection(puzzle->getNumberOfProblems()-1);

    /* Show the selected problem's solution, or nothing when it has none
     * yet: never whatever another tab or problem left in the view. */
    if (solutionProblem->getSelection() < puzzle->getNumberOfProblems())
      activateSolution(solutionProblem->getSelection(), int(SolutionSel->value()-1));
    else
      View3D->getView()->showNothing();
    Big3DView();
    hideDebugRightPane();
    StatusLine->setText("");
    ViewSizes[currentTab] = View3D->getZoom();
    if (ViewSizes[2] >= 0)
      View3D->setZoom(ViewSizes[2]);
    currentTab = 2;
  } else if (o->value() == TabTutorial) {

    StatusLine->setText("");
    showTutorialRightPane();
  } else if (o->value() == TabDebug) {

    attachSolverPane(TabDebug);

    if (puzzle->getNumberOfProblems() && (solutionProblem->getSelection() >= puzzle->getNumberOfProblems()))
      solutionProblem->setSelection(puzzle->getNumberOfProblems()-1);

    Big3DView();
    StatusLine->setText("");
    showDebugRightPane();
    currentTab = 2;
  }

  updateInterface();

  /* The tab's controls may have been shown, hidden or resized while it was
   * not on screen, and the 3D view beside it may have changed: lay out the
   * whole window again, then the tab's content at the size it has now,
   * as a resize of the window would. */
  relayoutViewStack();
  if (layouter_c * shown = dynamic_cast<layouter_c *>(o->value()))
    relayoutTab(shown);
  refreshSlideValid();
}

void cb_TransformPiece_stub(Fl_Widget* /*o*/, void* v) { static_cast<mainWindow_c*>(v)->cb_TransformPiece(); }
void cb_TransformPreview_stub(void* v, voxel_c* preview, unsigned int shapeNum) {
  static_cast<mainWindow_c*>(v)->cb_TransformPreview(preview, shapeNum);
}
void mainWindow_c::cb_TransformPiece(void) {

  ToolTab_0 * tab = dynamic_cast<ToolTab_0*>(pieceTools->getToolTab());
  if (tab && (tab->takeModeCallback() || tab->consumeTabChange())) {
    applySlidingGridMode();
    return;
  }

  if (pieceTools->operationToAll()) {
    for (unsigned int i = 0; i < puzzle->getNumberOfShapes(); i++)
      changeShape(i);
  } else {
    changeShape(PcSel->getSelection());
  }

  StatPieceInfo(PcSel->getSelection());
  activateShape(PcSel->getSelection());
  applySlidingGridMode();

  recordShapeAction(shapeHistory_c::AK_TRANSFORM);
}

void mainWindow_c::cb_TransformPreview(voxel_c *preview, unsigned int shapeNum) {

  if (preview) {
    if (currentTab != 0) {
      delete preview;
      return;
    }
    View3D->getView()->showOwnedVoxel(preview, shapeNum);
    return;
  }

  if (currentTab == 0)
    activateShape(PcSel->getSelection());
}

void cb_EditSym_stub(Fl_Widget* o, void* v) {
  static_cast<mainWindow_c*>(v)->cb_EditSym(static_cast<LToggleButton_c*>(o)->value(), static_cast<LToggleButton_c*>(o)->ButtonVal());
}
void mainWindow_c::cb_EditSym(int onoff, int value) {
  if (onoff) {
    editSymmetries |= value;
  } else {
    editSymmetries &= ~value;
  }

  pieceEdit->editSymmetries(editSymmetries);
}

void cb_EditChoice_stub(Fl_Widget* /*o*/, void* v) { static_cast<mainWindow_c*>(v)->cb_EditChoice(); }
void mainWindow_c::cb_EditChoice(void) {
  switch(editChoice->getSelected()) {
    case 0:
      pieceEdit->editChoice(gridEditor_c::TSK_SET);
      break;
    case 1:
      pieceEdit->editChoice(gridEditor_c::TSK_VAR);
      break;
    case 2:
      pieceEdit->editChoice(gridEditor_c::TSK_RESET);
      break;
    case 3:
      pieceEdit->editChoice(gridEditor_c::TSK_COLOR);
      break;
  }
}

void cb_EditMode_stub(Fl_Widget* /*o*/, void* v) { static_cast<mainWindow_c*>(v)->cb_EditMode(); }
void mainWindow_c::cb_EditMode(void) {
  switch(editMode->getSelected()) {
    case 0:
      pieceEdit->editType(gridEditor_c::EDT_RUBBER);
      config.useRubberband(true);
      break;
    case 1:
      pieceEdit->editType(gridEditor_c::EDT_SINGLE);
      config.useRubberband(false);
      break;
  }
}

void cb_PcSel_stub(Fl_Widget* o, void* v) { static_cast<mainWindow_c*>(v)->cb_PcSel(static_cast<LBlockListGroup_c*>(o)); }
void mainWindow_c::cb_PcSel(LBlockListGroup_c* grp) {
  int reason = grp->getReason();

  switch(reason) {
  case PieceSelector::RS_CHANGEDSELECTION:
    activateShape(PcSel->getSelection());
    updateInterface();
    StatPieceInfo(PcSel->getSelection());
    break;
  }
}

void cb_SolProbSel_stub(Fl_Widget* o, void* v) { static_cast<mainWindow_c*>(v)->cb_SolProbSel(static_cast<LBlockListGroup_c*>(o)); }
void mainWindow_c::cb_SolProbSel(LBlockListGroup_c* grp) {
  int reason = grp->getReason();

  switch(reason) {
  case ProblemSelector::RS_CHANGEDSELECTION:

    unsigned int prob = solutionProblem->getSelection();

    if (prob < puzzle->getNumberOfProblems()) {

      /* check the number of solutions on this tab and lower the slider value if necessary */
      if (SolutionSel->value() > puzzle->getProblem(prob)->getNumberOfSavedSolutions())
        SolutionSel->value(puzzle->getProblem(prob)->getNumberOfSavedSolutions());

      applySolverOptions(prob);
      updateInterface();
      activateSolution(prob, (int)SolutionSel->value()-1);
    }
    break;
  }
}

void cb_ColSel_stub(Fl_Widget* o, void* v) { static_cast<mainWindow_c*>(v)->cb_ColSel(static_cast<LBlockListGroup_c*>(o)); }
void mainWindow_c::cb_ColSel(LBlockListGroup_c* grp) {
  int reason = grp->getReason();

  switch(reason) {
  case PieceSelector::RS_CHANGEDSELECTION:
    pieceEdit->setColor(colorSelector->getSelection());
    if (puzzle && stacking::isStacking(*puzzle) &&
        PcSel->getSelection() < puzzle->getNumberOfShapes()) {
      voxel_c * disk = puzzle->getShape(PcSel->getSelection());
      unsigned int color = colorSelector->getSelection();
      for (unsigned int i = 0; i < disk->getXYZ(); i++) {
        if (disk->getState(i) != voxel_c::VX_EMPTY)
          disk->setColor(i, color);
      }
      recordShapeAction(shapeHistory_c::AK_STRUCTURAL);
    }
    updateInterface();
    activateShape(PcSel->getSelection());
    break;
  }
}

void cb_ProbSel_stub(Fl_Widget* o, void* v) { static_cast<mainWindow_c*>(v)->cb_ProbSel(static_cast<LBlockListGroup_c*>(o)); }
void mainWindow_c::cb_ProbSel(LBlockListGroup_c* grp) {
  int reason = grp->getReason();

  switch(reason) {
  case PieceSelector::RS_CHANGEDSELECTION:
    updateInterface();
    activateProblem(problemSelector->getSelection());
    selectProblemRod(problemSelector->getSelection());
    StatProblemInfo(problemSelector->getSelection());
    break;
  }
}

void cb_pieceEdit_stub(Fl_Widget* o, void* v) { static_cast<mainWindow_c*>(v)->cb_pieceEdit(static_cast<VoxelEditGroup_c*>(o)); }
void mainWindow_c::cb_pieceEdit(VoxelEditGroup_c* o) {

  switch (o->getReason()) {
  case gridEditor_c::RS_MOUSEMOVE:
    if (o->getMouse()) {
      View3D->getView()->setMarker(o->getMouseX1(), o->getMouseY1(), o->getMouseX2(), o->getMouseY2(), o->getMouseZ(), editSymmetries);
      StatPieceInfo(PcSel->getSelection(), true, o->getCursorX(), o->getCursorY(), o->getCursorZ());
    } else {
      View3D->getView()->hideMarker();
      StatPieceInfo(PcSel->getSelection());
    }
    break;
  case gridEditor_c::RS_STROKEBEGIN:
    if (shapeHistory)
      shapeHistory->beginStroke();
    break;
  case gridEditor_c::RS_SLIDING_CLICK:
    if (sliding::isSliding(*puzzle) &&
        PcSel->getSelection() < puzzle->getNumberOfShapes() &&
        sliding::isStartGoalShape(puzzle->getShape(PcSel->getSelection()))) {
      ToolTab_0 * tab = dynamic_cast<ToolTab_0*>(pieceTools->getToolTab());
      unsigned int shapeId = tab ? tab->selectedPiece() : (unsigned int)-1;
      voxel_c * tray = puzzle->getShape(PcSel->getSelection());
      const bool goal = slidingEditMode != 0;
      const int cx = o->getCursorX();
      const int cy = o->getCursorY();
      const unsigned int under = sliding::stampAt(tray, cx, cy, goal);
      /* A whole piece at a time: a left click places the chosen piece with
       * its top-left cell there, moving it if it was elsewhere, or takes it
       * away when it would land exactly where it is; a right click takes
       * away the piece that is there. */
      bool did = false;
      std::string why;
      if (o->getClickButton() != 1) {
        if (under != (unsigned int)-1)
          did = sliding::clearStamp(tray, under, goal);
      } else if (shapeId < puzzle->getNumberOfShapes() &&
                 !sliding::isStartGoalShape(puzzle->getShape(shapeId))) {
        if (sliding::stampIsAt(tray, puzzle->getShape(shapeId), shapeId, cx, cy, goal)) {
          did = sliding::clearStamp(tray, shapeId, goal);
        } else {
          why = sliding::stampPiece(tray, puzzle->getShape(shapeId), shapeId, cx, cy, goal);
          did = why.empty();
        }
      }
      if (!why.empty())
        StatusLine->setText(why.c_str());
      if (did) {
        sliding::syncSlidingProblems(*puzzle);
        changed = true;
        for (unsigned int p = 0; p < puzzle->getNumberOfProblems(); p++)
          if (puzzle->getProblem(p)->resultValid() &&
              puzzle->getProblem(p)->getResultId() == PcSel->getSelection())
            changeProblem(p);
      }
      activateShape(PcSel->getSelection());
      applySlidingGridMode();
      updateInterface();
      View3D->redraw();
    }
    break;
  case gridEditor_c::RS_CHANGESQUARE:
    if (shapeHistory)
      shapeHistory->markStrokeDirty();
    View3D->getView()->showSingleShape(puzzle, PcSel->getSelection());
    if (o->getMouse())
      StatPieceInfo(PcSel->getSelection(), true, o->getCursorX(), o->getCursorY(), o->getCursorZ());
    else
      StatPieceInfo(PcSel->getSelection());
    changeShape(PcSel->getSelection());
    if (sliding::isSliding(*puzzle) &&
        PcSel->getSelection() < puzzle->getNumberOfShapes() &&
        sliding::isStartGoalShape(puzzle->getShape(PcSel->getSelection())))
      sliding::syncSlidingProblems(*puzzle);
    changed = true;
    break;
  case gridEditor_c::RS_STROKEEND:
    if (shapeHistory && shapeHistory->endStroke(puzzle, PcSel->getSelection())) {
      changed = true;
      updateUndoRedoButtons();
    }
    break;
  }

  View3D->redraw();
}

void cb_NewProblem_stub(Fl_Widget* /*o*/, void* v) { static_cast<mainWindow_c*>(v)->cb_NewProblem(); }
void mainWindow_c::cb_NewProblem(void) {

  unsigned int prob = puzzle->addProblem();

  for (unsigned int c = 0; c < puzzle->colorNumber(); c++)
    puzzle->getProblem(prob)->allowPlacement(c+1, c+1);

  problemSelector->setSelection(prob);

  changed = true;
  updateInterface();
  activateProblem(problemSelector->getSelection());
  StatProblemInfo(problemSelector->getSelection());
}

void cb_DeleteProblem_stub(Fl_Widget* /*o*/, void* v) { static_cast<mainWindow_c*>(v)->cb_DeleteProblem(); }
void mainWindow_c::cb_DeleteProblem(void) {

  if (problemSelector->getSelection() < puzzle->getNumberOfProblems()) {

    puzzle->removeProblem(problemSelector->getSelection());

    changed = true;

    while ((problemSelector->getSelection() >= puzzle->getNumberOfProblems()) &&
           (problemSelector->getSelection() > 0))
      problemSelector->setSelection(problemSelector->getSelection()-1);

    updateInterface();
    if (problemSelector->getSelection() < puzzle->getNumberOfProblems())
      activateProblem(problemSelector->getSelection());
    else
      activateClear();
    StatProblemInfo(problemSelector->getSelection());
  }
}

void cb_CopyProblem_stub(Fl_Widget* /*o*/, void* v) { static_cast<mainWindow_c*>(v)->cb_CopyProblem(); }
void mainWindow_c::cb_CopyProblem(void) {

  if (problemSelector->getSelection() < puzzle->getNumberOfProblems()) {

    unsigned int prob = puzzle->addProblem(puzzle->getProblem(problemSelector->getSelection()));
    problemSelector->setSelection(prob);

    changed = true;
    updateInterface();
    activateProblem(problemSelector->getSelection());
    StatProblemInfo(problemSelector->getSelection());
  }
}

void cb_RenameProblem_stub(Fl_Widget* /*o*/, void* v) { static_cast<mainWindow_c*>(v)->cb_RenameProblem(); }
void mainWindow_c::cb_RenameProblem(void) {

  if (problemSelector->getSelection() < puzzle->getNumberOfProblems()) {

    const char * name = fl_input("Enter name for the problem",
        puzzle->getProblem(problemSelector->getSelection())->getName().c_str());

    if (name) {

      puzzle->getProblem(problemSelector->getSelection())->setName(name);
      changed = true;
      updateInterface();
      activateProblem(problemSelector->getSelection());
    }
  }
}

void cb_ProblemLeft_stub(Fl_Widget* /*o*/, void* v) { static_cast<mainWindow_c*>(v)->cb_ProblemExchange(-1); }
void cb_ProblemRight_stub(Fl_Widget* /*o*/, void* v) { static_cast<mainWindow_c*>(v)->cb_ProblemExchange(+1); }
void mainWindow_c::cb_ProblemExchange(int with) {

  unsigned int current = problemSelector->getSelection();
  unsigned int other = current + with;

  if ((current < puzzle->getNumberOfProblems()) && (other < puzzle->getNumberOfProblems())) {
    puzzle->exchangeProblems(current, other);
    changed = true;
    problemSelector->setSelection(other);
  }
}

void cb_ShapeLeft_stub(Fl_Widget* /*o*/, void* v) { static_cast<mainWindow_c*>(v)->cb_ShapeExchange(-1); }
void cb_ShapeRight_stub(Fl_Widget* /*o*/, void* v) { static_cast<mainWindow_c*>(v)->cb_ShapeExchange(+1); }
void mainWindow_c::cb_ShapeExchange(int with) {

  unsigned int current = PcSel->getSelection();
  unsigned int other = current + with;

  if ((current < puzzle->getNumberOfShapes()) && (other < puzzle->getNumberOfShapes())) {
    if (sliding::isSliding(*puzzle))
      sliding::noteShapeSwap(*puzzle, current, other);
    puzzle->exchangeShapes(current, other);
    PcSel->setSelection(other);
    recordShapeAction(shapeHistory_c::AK_STRUCTURAL);
    updateInterface();
  }
}

void cb_ProbShapeLeft_stub(Fl_Widget* /*o*/, void* v) { static_cast<mainWindow_c*>(v)->cb_ProbShapeExchange(-1); }
void cb_ProbShapeRight_stub(Fl_Widget* /*o*/, void* v) { static_cast<mainWindow_c*>(v)->cb_ProbShapeExchange(+1); }
void mainWindow_c::cb_ProbShapeExchange(int with) {

  unsigned int p = problemSelector->getSelection();

  if (stacking::isStacking(*puzzle) && rodStackList) {
    problem_c * pr = puzzle->getProblem(p);
    if (!stackingBoard(pr))
      return;
    const stacking::stackMap_c & map = stackingEditGoal ? pr->goalStacks() : pr->startStacks();
    unsigned int rod = selectedStackRod();
    if (rod >= map.rods.size() || map.rods[rod].empty())
      return;
    unsigned int display = rodStackList->getSelection();
    if (display >= map.rods[rod].size())
      display = 0;
    unsigned int index = (unsigned int)map.rods[rod].size() - 1 - display;
    int delta = -with;
    if (!stacking::moveDisk(*pr, stackingEditGoal, rod, index, delta))
      return;
    changed = true;
    pushRodHistory();
    refreshStackList();
    int next = (int)index + delta;
    if (next >= 0 && rodStackList)
      rodStackList->setSelection((unsigned int)map.rods[rod].size() - 1 - (unsigned int)next);
    activateProblem(p);
    updateInterface();
    return;
  }

  unsigned int s = shapeAssignmentSelector->getSelection();

  problem_c * pr = puzzle->getProblem(p);

  // find out the index in the problem table
  unsigned int current;

  for (current = 0; current < pr->getNumberOfParts(); current++)
    if (pr->getShapeIdOfPart(current) == s)
      break;

  unsigned int other = current + with;

  if ((current < pr->getNumberOfParts()) && (other < pr->getNumberOfParts())) {
    pr->exchangeParts(current, other);
    changed = true;
    updateInterface();
    activateProblem(problemSelector->getSelection());
  }
}

void cb_ColorAssSel_stub(Fl_Widget* /*o*/, void* v) { static_cast<mainWindow_c*>(v)->cb_ColorAssSel(); }
void mainWindow_c::cb_ColorAssSel(void) {
  updateInterface();
}

void cb_ColorConstrSel_stub(Fl_Widget* /*o*/, void* v) { static_cast<mainWindow_c*>(v)->cb_ColorConstrSel(); }
void mainWindow_c::cb_ColorConstrSel(void) {
  updateInterface();
}

void cb_ShapeToResult_stub(Fl_Widget* /*o*/, void* v) { static_cast<mainWindow_c*>(v)->cb_ShapeToResult(); }
void mainWindow_c::cb_ShapeToResult(void) {

  if (problemSelector->getSelection() >= puzzle->getNumberOfProblems()) {
    fl_message("First create a problem");
    return;
  }

  if (stacking::isStacking(*puzzle)) {
    if (!rodAssignSel || rodAssignSel->getSelection() >= puzzle->rodSetCount())
      return;
    unsigned int prob = problemSelector->getSelection();
    problem_c * pr = puzzle->getProblem(prob);
    pr->setRodSetId(rodAssignSel->getSelection());
    stacking::syncMaps(*pr);
    if (problemResult)
      problemResult->setPuzzle(pr);
    changed = true;
    pushRodHistory();
    activateProblem(prob);
    StatProblemInfo(prob);
    updateInterface();
    return;
  }

  if (shapeAssignmentSelector->getSelection() >= puzzle->getNumberOfShapes())
    return;

  unsigned int prob = problemSelector->getSelection();
  problem_c * pr = puzzle->getProblem(prob);

  // check if this shape is already a piece of the problem
  pr->setShapeMaximum(shapeAssignmentSelector->getSelection(), 0);

  pr->setResultId(shapeAssignmentSelector->getSelection());
  problemResult->setPuzzle(puzzle->getProblem(prob));
  activateProblem(prob);
  StatProblemInfo(prob);
  updateInterface();

  changed = true;
}

void cb_ShapeSel_stub(Fl_Widget* /*o*/, void* v) { static_cast<mainWindow_c*>(v)->cb_SelectProblemShape(); }
void mainWindow_c::cb_SelectProblemShape(void) {
  updateInterface();
  activateProblem(problemSelector->getSelection());
}

void cb_PiecesClicked_stub(Fl_Widget* /*o*/, void* v) { static_cast<mainWindow_c*>(v)->cb_PiecesClicked(); }
void mainWindow_c::cb_PiecesClicked(void) {

  problem_c * pr = puzzle->getProblem(problemSelector->getSelection());

  shapeAssignmentSelector->setSelection(pr->getShapeIdOfPart(PiecesCountList->getClicked()));

  updateInterface();
  activateProblem(problemSelector->getSelection());
}

void cb_AddShapeToProblem_stub(Fl_Widget* /*o*/, void* v) { static_cast<mainWindow_c*>(v)->cb_AddShapeToProblem(); }
void mainWindow_c::cb_AddShapeToProblem(void) {

  if (problemSelector->getSelection() >= puzzle->getNumberOfProblems()) {
    fl_message("First create a problem");
    return;
  }
  unsigned int shape = shapeAssignmentSelector->getSelection();
  if (shape >= puzzle->getNumberOfShapes())
    return;

  unsigned int prob = problemSelector->getSelection();

  problem_c * pr = puzzle->getProblem(prob);

  if (stacking::isStacking(*puzzle) && !stackingBoard(pr)) {
    StatusLine->setText("Choose a rod set with Set Start/Goal Rods");
    return;
  }

  changed = true;
  PiecesCountList->redraw();

  if (stacking::isStacking(*puzzle)) {
    std::string err = placeOneDisk(pr, shape);
    if (!err.empty()) {
      StatusLine->setText(err.c_str());
      return;
    }
    StatusLine->setText("");
    pushRodHistory();
    refreshStackList();
    if (rodStackList)
      rodStackList->setSelection(0);
    activateProblem(prob);
    updateInterface();
    StatProblemInfo(prob);
    return;
  }

  // first see, if there is already a selected shape inside
  pr->setShapeMaximum(shape, pr->getShapeMaximum(shape) + 1);
  pr->setShapeMinimum(shape, pr->getShapeMinimum(shape) + 1);

  activateProblem(problemSelector->getSelection());
  updateInterface();
  StatProblemInfo(problemSelector->getSelection());
}

void cb_AddAllShapesToProblem_stub(Fl_Widget* /*o*/, void* v) { static_cast<mainWindow_c*>(v)->cb_AddAllShapesToProblem(); }
void mainWindow_c::cb_AddAllShapesToProblem(void) {

  if (problemSelector->getSelection() >= puzzle->getNumberOfProblems()) {
    fl_message("First create a problem");
    return;
  }

  unsigned int prob = problemSelector->getSelection();

  problem_c * pr = puzzle->getProblem(prob);

  if (stacking::isStacking(*puzzle)) {
    if (!stackingBoard(pr)) {
      StatusLine->setText("Choose a rod set with Set Start/Goal Rods");
      return;
    }
    /* Largest first, so a size rule can still stack them on one rod. */
    std::vector<unsigned int> shapes;
    shapes.reserve(puzzle->getNumberOfShapes());
    for (unsigned int j = 0; j < puzzle->getNumberOfShapes(); j++)
      shapes.push_back(j);
    std::stable_sort(shapes.begin(), shapes.end(), [&](unsigned int a, unsigned int b) {
      const voxel_c * sa = puzzle->getShape(a);
      const voxel_c * sb = puzzle->getShape(b);
      unsigned int za = sa ? sa->getDiskSize() : 0;
      unsigned int zb = sb ? sb->getDiskSize() : 0;
      return za > zb;
    });
    std::string err;
    bool placed = false;
    for (unsigned int shape : shapes) {
      err = placeOneDisk(pr, shape);
      if (!err.empty())
        break;
      placed = true;
    }
    StatusLine->setText(err.c_str());
    if (placed) {
      changed = true;
      PiecesCountList->redraw();
      pushRodHistory();
      refreshStackList();
      if (rodStackList)
        rodStackList->setSelection(0);
    }
    activateProblem(prob);
    PcVis->setPuzzle(puzzle->getProblem(solutionProblem->getSelection()));
    updateInterface();
    StatProblemInfo(prob);
    return;
  }

  changed = true;
  PiecesCountList->redraw();

  for (unsigned int j = 0; j < puzzle->getNumberOfShapes(); j++) {

    // we don't add the result shape
    if (pr->resultValid() && j == pr->getResultId())
      continue;
    if (sliding::shapeIsRequired(*pr, j))
      continue;

    pr->setShapeMaximum(j, pr->getShapeMaximum(j) + 1);
    pr->setShapeMinimum(j, pr->getShapeMinimum(j) + 1);
  }

  activateProblem(problemSelector->getSelection());
  PcVis->setPuzzle(puzzle->getProblem(solutionProblem->getSelection()));
  updateInterface();
  StatProblemInfo(problemSelector->getSelection());
}

void cb_RemoveShapeFromProblem_stub(Fl_Widget* /*o*/, void* v) { static_cast<mainWindow_c*>(v)->cb_RemoveShapeFromProblem(); }
void mainWindow_c::cb_RemoveShapeFromProblem(void) {

  if (problemSelector->getSelection() >= puzzle->getNumberOfProblems()) {
    fl_message("First create a problem");
    return;
  }

  unsigned int shape = shapeAssignmentSelector->getSelection();

  if (shape >= puzzle->getNumberOfShapes())
    return;

  unsigned int prob = problemSelector->getSelection();

  problem_c * pr = puzzle->getProblem(prob);

  if (stacking::isStacking(*puzzle)) {
    if (!stackingBoard(pr))
      return;
    const stacking::stackMap_c & map = stackingEditGoal ? pr->goalStacks() : pr->startStacks();
    unsigned int rod = selectedStackRod();
    if (rod >= map.rods.size() || map.rods[rod].empty()) {
      StatusLine->setText("That rod is empty");
      return;
    }
    unsigned int display = rodStackList ? rodStackList->getSelection() : 0;
    if (display >= map.rods[rod].size())
      display = 0;
    stacking::diskRef_c disk = map.rods[rod][map.rods[rod].size() - 1 - display];
    stacking::dropInstance(*pr, disk.shapeId, disk.instance);
    if (pr->getShapeMinimum(disk.shapeId) > 0)
      pr->setShapeMinimum(disk.shapeId, pr->getShapeMinimum(disk.shapeId) - 1);
    if (pr->getShapeMaximum(disk.shapeId) > 0)
      pr->setShapeMaximum(disk.shapeId, pr->getShapeMaximum(disk.shapeId) - 1);
    StatusLine->setText("");
    changed = true;
    pushRodHistory();
    refreshStackList();
    activateProblem(prob);
    StatProblemInfo(prob);
    updateInterface();
    return;
  }

  if (pr->getShapeMinimum(shape) > 0) pr->setShapeMinimum(shape, pr->getShapeMinimum(shape)-1);
  if (pr->getShapeMaximum(shape) > 0) pr->setShapeMaximum(shape, pr->getShapeMaximum(shape)-1);

  changed = true;
  PiecesCountList->redraw();
  PcVis->setPuzzle(puzzle->getProblem(solutionProblem->getSelection()));

  activateProblem(problemSelector->getSelection());
  StatProblemInfo(problemSelector->getSelection());
}


void cb_SetShapeMinimumToZero_stub (Fl_Widget* /*o*/, void* v) { static_cast<mainWindow_c*>(v)->cb_SetShapeMinimumToZero(); }
void mainWindow_c::cb_SetShapeMinimumToZero(void) {

  if (problemSelector->getSelection() >= puzzle->getNumberOfProblems()) {
    fl_message("First create a problem");
    return;
  }

  unsigned int shape = shapeAssignmentSelector->getSelection();

  if (shape >= puzzle->getNumberOfShapes())
    return;

  unsigned int prob = problemSelector->getSelection();
  changeProblem(prob);

  problem_c * pr = puzzle->getProblem(prob);

  pr->setShapeMinimum(shape, 0);

  changed = true;
  PiecesCountList->redraw();
  PcVis->setPuzzle(puzzle->getProblem(solutionProblem->getSelection()));

  activateProblem(problemSelector->getSelection());
  StatProblemInfo(problemSelector->getSelection());
}


void cb_RemoveAllShapesFromProblem_stub(Fl_Widget* /*o*/, void* v) { static_cast<mainWindow_c*>(v)->cb_RemoveAllShapesFromProblem(); }
void mainWindow_c::cb_RemoveAllShapesFromProblem(void) {

  if (problemSelector->getSelection() >= puzzle->getNumberOfProblems()) {
    fl_message("First create a problem");
    return;
  }

  unsigned int prob = problemSelector->getSelection();
  problem_c * pr = puzzle->getProblem(prob);

  /* Start and Goal share the piece counts. Clearing counts drops discs
   * from both maps, so only the map being edited is emptied. */
  if (stacking::isStacking(*puzzle)) {
    stacking::stackMap_c & map = stackingEditGoal ? pr->editGoal() : pr->editStart();
    bool any = false;
    for (auto & rod : map.rods) {
      if (rod.empty())
        continue;
      rod.clear();
      any = true;
    }
    if (any) {
      changeProblem(prob);
      changed = true;
      pushRodHistory();
    }
    StatusLine->setText("");
    refreshStackList();
    activateProblem(prob);
    updateInterface();
    StatProblemInfo(prob);
    return;
  }

  changeProblem(prob);

  for (unsigned int i = 0; i < puzzle->getNumberOfShapes(); i++)
    if (!sliding::shapeIsRequired(*pr, i))
      pr->setShapeMaximum(i, 0);

  changed = true;
  PiecesCountList->redraw();
  PcVis->setPuzzle(puzzle->getProblem(solutionProblem->getSelection()));

  activateProblem(problemSelector->getSelection());
  StatProblemInfo(problemSelector->getSelection());
}

void cb_SetAllRange_stub(Fl_Widget* /*o*/, void* v) { static_cast<mainWindow_c*>(v)->cb_SetAllRange(); }
void mainWindow_c::cb_SetAllRange(void) {

  if (problemSelector->getSelection() >= puzzle->getNumberOfProblems()) {
    fl_message("First create a problem");
    return;
  }

  bulkRangeWindow_c win;
  win.show();
  while (win.visible())
    Fl::wait();

  if (!win.okSelected())
    return;

  unsigned int prob = problemSelector->getSelection();
  changeProblem(prob);

  problem_c * pr = puzzle->getProblem(prob);

  unsigned int newMin = win.getMin();
  unsigned int newMax = win.getMax();
  if (newMin > newMax)
    newMax = newMin;

  for (unsigned int p = 0; p < pr->getNumberOfParts(); p++) {
    unsigned int shapeId = pr->getShapeIdOfPart(p);
    if (sliding::shapeIsRequired(*pr, shapeId))
      continue;
    pr->setShapeMaximum(shapeId, newMax);
    pr->setShapeMinimum(shapeId, newMin);
  }

  changed = true;
  PiecesCountList->redraw();
  PcVis->setPuzzle(puzzle->getProblem(solutionProblem->getSelection()));

  activateProblem(problemSelector->getSelection());
  StatProblemInfo(problemSelector->getSelection());
}

void cb_ShapeGroup_stub(Fl_Widget* /*o*/, void* v) { static_cast<mainWindow_c*>(v)->cb_ShapeGroup(); }
void mainWindow_c::cb_ShapeGroup(void) {

  unsigned int prob = problemSelector->getSelection();

  groupsEditor_c groupEditWin(puzzle, prob);

  groupEditWin.show();

  while (groupEditWin.visible())
    Fl::wait();

  if (groupEditWin.changed()) {

    problem_c * pr = puzzle->getProblem(prob);

    /* if the user added the result shape to the problem, we inform him and
     * remove that shape again
     */
    if (pr->resultValid() && pr->getShapeMaximum(pr->getResultId()) > 0)
      pr->setShapeMaximum(pr->getResultId(), 0);

    /* as the user may have reset the counts of one shape to zero, go
     * through the list and remove entries of zero count */
    PiecesCountList->redraw();
    PcVis->setPuzzle(puzzle->getProblem(solutionProblem->getSelection()));
    changed = true;
    activateProblem(problemSelector->getSelection());
    StatProblemInfo(problemSelector->getSelection());
    updateInterface();
  }
}

void cb_ExportSolutionSTL_stub(Fl_Widget* /*o*/, void* v) { static_cast<mainWindow_c*>(v)->cb_ExportSolutionSTL(); }
void mainWindow_c::cb_ExportSolutionSTL(void) {

  unsigned int prob = solutionProblem->getSelection();
  if (prob >= puzzle->getNumberOfProblems())
    return;

  unsigned int sol = (unsigned int)SolutionSel->value() - 1;
  if (sol >= puzzle->getProblem(prob)->getNumberOfSavedSolutions())
    return;

  stlExportSolution_c w(puzzle, prob, sol);
  w.show();
  while (w.visible())
    Fl::wait();
}

void cb_BtnPlacementBrowser_stub(Fl_Widget* /*o*/, void* v) { static_cast<mainWindow_c*>(v)->cb_BtnPlacementBrowser(); }
void mainWindow_c::cb_BtnPlacementBrowser(void) {

  unsigned int prob = solutionProblem->getSelection();

  if (prob >= puzzle->getNumberOfProblems())
    return;

  const assembler_c * pa = puzzle->getProblem(prob)->getAssembler();
  if (!pa || assmThread)
    return;

  if (!pa->getPiecePlacementSupported()) {
    fl_message("Sorry, no placement browser for this type of puzzle");
    return;
  }

  placementBrowser_c plbr(puzzle->getProblem(prob));

  plbr.show();

  while (plbr.visible())
    Fl::wait();
}

void cb_BtnMovementBrowser_stub(Fl_Widget* /*o*/, void* v) { static_cast<mainWindow_c*>(v)->cb_BtnMovementBrowser(); }
void mainWindow_c::cb_BtnMovementBrowser(void) {

  unsigned int prob = solutionProblem->getSelection();

  if (prob >= puzzle->getNumberOfProblems())
    return;

  unsigned int sol = (int)SolutionSel->value()-1;

  if (sol >= puzzle->getProblem(prob)->getNumberOfSavedSolutions())
    return;

  movementBrowser_c mvbr(puzzle->getProblem(prob), sol);

  mvbr.show();

  while (mvbr.visible())
    Fl::wait();
}

void cb_BtnAssemblerStep_stub(Fl_Widget* /*o*/, void* v) { static_cast<mainWindow_c*>(v)->cb_BtnAssemblerStep(); }
void mainWindow_c::cb_BtnAssemblerStep(void) {

  bt_assert(!assmThread);

  assembler_c * assm = puzzle->getProblem(solutionProblem->getSelection())->getAssembler();

  bt_assert(assm);

  assm->debug_step(1);

  if (assm->getFinished() >= 1)
    puzzle->getProblem(solutionProblem->getSelection())->finishedSolving();

  updateInterface();

  std::unique_ptr<assembly_c> a = assm->getAssembly();
  View3D->getView()->showAssemblerState(puzzle->getProblem(solutionProblem->getSelection()), a.get());
}

void cb_AllowColor_stub(Fl_Widget* /*o*/, void* v) { static_cast<mainWindow_c*>(v)->cb_AllowColor(); }
void mainWindow_c::cb_AllowColor(void) {

  if (problemSelector->getSelection() >= puzzle->getNumberOfProblems()) {
    fl_message("First create a problem");
    return;
  }

  unsigned int prob = problemSelector->getSelection();
  problem_c * pr = puzzle->getProblem(prob);

  if (colconstrList->GetSortByResult())
    pr->allowPlacement(colorAssignmentSelector->getSelection()+1,
                       colconstrList->getSelection()+1);
  else
    pr->allowPlacement(colconstrList->getSelection()+1,
                       colorAssignmentSelector->getSelection()+1);
  changed = true;
  changeProblem(problemSelector->getSelection());
  updateInterface();
}

void cb_DisallowColor_stub(Fl_Widget* /*o*/, void* v) { static_cast<mainWindow_c*>(v)->cb_DisallowColor(); }
void mainWindow_c::cb_DisallowColor(void) {

  if (problemSelector->getSelection() >= puzzle->getNumberOfProblems()) {
    fl_message("First create a problem");
    return;
  }

  unsigned int prob = problemSelector->getSelection();
  problem_c * pr = puzzle->getProblem(prob);

  if (colconstrList->GetSortByResult())
    pr->disallowPlacement(colorAssignmentSelector->getSelection()+1,
                              colconstrList->getSelection()+1);
  else
    pr->disallowPlacement(colconstrList->getSelection()+1,
                          colorAssignmentSelector->getSelection()+1);

  changed = true;
  changeProblem(problemSelector->getSelection());
  updateInterface();
}

void cb_CCSortByResult_stub(Fl_Widget* /*o*/, void* v) { static_cast<mainWindow_c*>(v)->cb_CCSort(1); }
void cb_CCSortByPiece_stub(Fl_Widget* /*o*/, void* v) { static_cast<mainWindow_c*>(v)->cb_CCSort(0); }
void mainWindow_c::cb_CCSort(bool byResult) {
  colconstrList->SetSortByResult(byResult);

  if (byResult) {
    BtnColSrtPc->activate();
    BtnColSrtRes->deactivate();
  } else {
    BtnColSrtPc->deactivate();
    BtnColSrtRes->activate();
  }
}

void cb_Quit_stub(Fl_Widget* /*o*/, void* v) { static_cast<mainWindow_c*>(v)->hide(); }
void mainWindow_c::hide(void) {
  if (confirmDiscard("quit"))
    Fl_Double_Window::hide();
}

void cb_Config_stub(Fl_Widget* /*o*/, void* v) { static_cast<mainWindow_c*>(v)->cb_Config(); }
void mainWindow_c::cb_Config(void) {
  config.dialog();
  activateConfigOptions();
}

void cb_ToggleNotes_stub(Fl_Widget* /*o*/, void* v) { static_cast<mainWindow_c*>(v)->cb_ToggleNotes(); }
void mainWindow_c::cb_ToggleNotes(void) {

  if (notesPanel->visible())
    notesPanel->hide();
  else
    notesPanel->show();

  updateNotesMenuLabel();
  if (contentTile)
    contentTile->forceLayout();
  relayoutViewStack();
}

void mainWindow_c::updateNotesMenuLabel(void) {

  if (!MainMenu)
    return;

  const int idx = liveMenuIndex(MainMenu, cb_ToggleNotes_stub);
  if (idx < 0)
    return;

  MainMenu->replace(idx, notesPanel->visible() ? "Hide Notes" : "Show Notes");
  MainMenu->update();
}

void cb_NotesUpdate_stub(Fl_Widget* /*o*/, void* v) { static_cast<mainWindow_c*>(v)->cb_NotesUpdate(); }
void mainWindow_c::cb_NotesUpdate(void) {

  const char * text = notesInput->value();
  puzzle->setComment(text ? text : "");
  changed = true;
  setNotesButtonsEnabled(false);
}

void cb_NotesRevert_stub(Fl_Widget* /*o*/, void* v) { static_cast<mainWindow_c*>(v)->cb_NotesRevert(); }
void mainWindow_c::cb_NotesRevert(void) {
  notesInput->value(puzzle->getComment().c_str());
  setNotesButtonsEnabled(false);
}

void cb_NotesChanged_stub(Fl_Widget* /*o*/, void* v) { static_cast<mainWindow_c*>(v)->cb_NotesChanged(); }
void mainWindow_c::cb_NotesChanged(void) {
  const char * text = notesInput->value();
  const char * saved = puzzle->getComment().c_str();
  setNotesButtonsEnabled(strcmp(text ? text : "", saved ? saved : "") != 0);
}

void mainWindow_c::setNotesButtonsEnabled(bool enabled) {
  if (!notesUpdate || !notesRevert)
    return;
  if (enabled) {
    notesUpdate->activate();
    notesRevert->activate();
  } else {
    notesUpdate->deactivate();
    notesRevert->deactivate();
  }
}

void mainWindow_c::relayoutViewStack(void) {
  if (view3DStack)
    view3DStack->invalidateMinSize();
  if (notesPanel)
    notesPanel->invalidateMinSize();
  if (detailsPanel)
    detailsPanel->invalidateMinSize();
  if (rightPane)
    rightPane->invalidateMinSize();

  /* Resize the right column first so Details can take a strip under the 3D
   * view even when the window size has not changed. */
  if (rightPane)
    rightPane->resize(rightPane->x(), rightPane->y(), rightPane->w(), rightPane->h());
  else if (view3DStack)
    view3DStack->resize(view3DStack->x(), view3DStack->y(), view3DStack->w(), view3DStack->h());

  layouter_c * root = dynamic_cast<layouter_c*>(resizable());
  if (root) {
    root->invalidateMinSize();
    root->resize(root->x(), root->y(), root->w(), root->h());
  }
  if (rightPane)
    rightPane->resize(rightPane->x(), rightPane->y(), rightPane->w(), rightPane->h());
  redraw();
}

void cb_StatusWindow_stub(Fl_Widget* /*o*/, void* v) { static_cast<mainWindow_c*>(v)->cb_StatusWindow(); }
void mainWindow_c::cb_StatusWindow(void) {

  if (!detailsPanel)
    return;

  detailsPanel->show();
  if (rightPane)
    rightPane->forceLayout();
  relayoutViewStack();
  detailsPanel->populate(puzzle);
  relayoutViewStack();
}

void cb_DetailsClose_stub(Fl_Widget*, void* v) { static_cast<mainWindow_c*>(v)->cb_DetailsClose(); }
void mainWindow_c::cb_DetailsClose(void) {

  if (!detailsPanel)
    return;

  detailsPanel->hide();
  if (rightPane)
    rightPane->forceLayout();
  relayoutViewStack();
}

void cb_DetailsChanged_stub(Fl_Widget*, void* v) { static_cast<mainWindow_c*>(v)->cb_DetailsChanged(); }
void mainWindow_c::cb_DetailsChanged(void) {

  changed = true;

  unsigned int current = PcSel->getSelection();

  if (puzzle->getNumberOfShapes() == 0)
    current = (unsigned int)-1;
  else
    while (current >= puzzle->getNumberOfShapes())
      current--;

  activateShape(current);
  PcSel->setSelection(current);
  updateInterface();
}

void cb_Toggle3D_stub(Fl_Widget* /*o*/, void* v) { static_cast<mainWindow_c*>(v)->cb_Toggle3D(); }
void mainWindow_c::cb_Toggle3D(void) {

  if (TaskSelectionTab->value() == TabPieces) {
    shapeEditorWithBig3DView = !shapeEditorWithBig3DView;
    if (!shapeEditorWithBig3DView)
      Small3DView();
    else
      Big3DView();
  }
}

void cb_ViewMode0_stub(Fl_Widget* /*o*/, void* v) { static_cast<mainWindow_c*>(v)->cb_ViewMode(0); }
void cb_ViewMode1_stub(Fl_Widget* /*o*/, void* v) { static_cast<mainWindow_c*>(v)->cb_ViewMode(1); }
void cb_ViewMode2_stub(Fl_Widget* /*o*/, void* v) { static_cast<mainWindow_c*>(v)->cb_ViewMode(2); }
void cb_ViewMode3_stub(Fl_Widget* /*o*/, void* v) { static_cast<mainWindow_c*>(v)->cb_ViewMode(3); }
void cb_RenderStyle0_stub(Fl_Widget* /*o*/, void* v) { static_cast<mainWindow_c*>(v)->cb_RenderStyle(0); }
void cb_RenderStyle1_stub(Fl_Widget* /*o*/, void* v) { static_cast<mainWindow_c*>(v)->cb_RenderStyle(1); }
void cb_RenderStyle2_stub(Fl_Widget* /*o*/, void* v) { static_cast<mainWindow_c*>(v)->cb_RenderStyle(2); }

void mainWindow_c::cb_ViewMode(int mode) {

  static Fl_Callback * const cbs[4] = {
    cb_ViewMode0_stub, cb_ViewMode1_stub, cb_ViewMode2_stub, cb_ViewMode3_stub
  };

  if (MainMenu) {
    Fl_Menu_Item * items = const_cast<Fl_Menu_Item *>(MainMenu->menu());
    for (int i = 0; i < 4; i++) {
      int idx = liveMenuIndex(MainMenu, cbs[i]);
      if (idx < 0) continue;
      if (i == mode) items[idx].set();
      else items[idx].clear();
    }
    MainMenu->update();
  }

  StatusLine->setColorModeIndex(mode);
  cb_Status();
}

void mainWindow_c::syncRenderStyleMenu(void) {

  static Fl_Callback * const cbs[3] = {
    cb_RenderStyle0_stub, cb_RenderStyle1_stub, cb_RenderStyle2_stub
  };

  if (!MainMenu || !StatusLine)
    return;

  const int mode = StatusLine->getRenderStyleIndex();
  Fl_Menu_Item * items = const_cast<Fl_Menu_Item *>(MainMenu->menu());
  for (int i = 0; i < 3; i++) {
    int idx = liveMenuIndex(MainMenu, cbs[i]);
    if (idx < 0) continue;
    if (i == mode) items[idx].set();
    else items[idx].clear();
  }
  MainMenu->update();
}

void mainWindow_c::cb_RenderStyle(int mode) {
  StatusLine->setRenderStyleIndex(mode);
  syncRenderStyleMenu();
  cb_Status();
}

void mainWindow_c::StatPieceInfo(unsigned int pc) {
  StatPieceInfo(pc, false, 0, 0, 0);
}

void mainWindow_c::StatPieceInfo(unsigned int pc, bool withCoords, int x, int y, int z) {

  if (pc < puzzle->getNumberOfShapes()) {
    char txt[160];

    unsigned int fx = puzzle->getShape(pc)->countState(voxel_c::VX_FILLED);
    unsigned int vr = puzzle->getShape(pc)->countState(voxel_c::VX_VARIABLE);

    if (withCoords)
      snprintf(txt, sizeof(txt),
               "Shape S%u has %u voxels (%u fixed, %u variable) (Coordinates X=%i, Y=%i, Z=%i)",
               pc+1, fx+vr, fx, vr, x, y, z);
    else
      snprintf(txt, sizeof(txt),
               "Shape S%u has %u voxels (%u fixed, %u variable)",
               pc+1, fx+vr, fx, vr);
    StatusLine->setText(txt);
  }
}

void mainWindow_c::StatProblemInfo(unsigned int prob) {

  if ((prob < puzzle->getNumberOfProblems()) && (puzzle->getProblem(prob)->resultValid())) {

    problem_c * pr = puzzle->getProblem(prob);

    char txt[100];

    unsigned int cnt = 0;
    unsigned int cntMin = 0;

    for (unsigned int i = 0; i < pr->getNumberOfParts(); i++) {
      cnt += pr->getPartShape(i)->countState(voxel_c::VX_FILLED) * pr->getPartMaximum(i);
      cntMin += pr->getPartShape(i)->countState(voxel_c::VX_FILLED) * pr->getPartMinimum(i);
    }

    if (cnt == cntMin) {

      snprintf(txt, 100, "Problem P%u result can contain %u - %u voxels, pieces (n = %u) contain %u voxels", prob+1,
          getResultShape(*pr)->countState(voxel_c::VX_FILLED),
          getResultShape(*pr)->countState(voxel_c::VX_FILLED) +
          getResultShape(*pr)->countState(voxel_c::VX_VARIABLE),
          pr->getNumberOfPieces(), cnt);

    } else {

      snprintf(txt, 100, "Problem P%u result can contain %u - %u voxels, pieces (n = %u) contain %u-%u voxels", prob+1,
          getResultShape(*pr)->countState(voxel_c::VX_FILLED),
          getResultShape(*pr)->countState(voxel_c::VX_FILLED) +
          getResultShape(*pr)->countState(voxel_c::VX_VARIABLE),
          pr->getNumberOfPieces(), cntMin, cnt);
    }

    StatusLine->setText(txt);

  } else

    StatusLine->setText("");
}

void mainWindow_c::changeColor(unsigned int nr) {

  for (unsigned int i = 0; i < puzzle->getNumberOfShapes(); i++)
    for (unsigned int j = 0; j < puzzle->getShape(i)->getXYZ(); j++)
      if (puzzle->getShape(i)->getColor(j) == nr) {
        changeShape(i);
        break;
      }
}

void mainWindow_c::changeShape(unsigned int nr) {
  for (unsigned int i = 0; i < puzzle->getNumberOfProblems(); i++)
    if (puzzle->getProblem(i)->usesShape(nr))
      puzzle->getProblem(i)->removeAllSolutions();

  /* A sliding piece whose shape changed no longer fits its starts and
   * goals: they come off every start/goal shape. */
  if (sliding::isSliding(*puzzle)) {
    const std::vector<unsigned int> trays = sliding::dropChangedStamps(*puzzle, nr);
    if (!trays.empty()) {
      sliding::syncSlidingProblems(*puzzle);
      std::string where;
      for (unsigned int t : trays)
        where += (where.empty() ? "S" : ", S") + std::to_string(t + 1);
      const std::string msg = "S" + std::to_string(nr + 1) +
          " changed shape: its start and goal were removed from " + where + ".";
      StatusLine->setText(msg.c_str());
      refreshSlideValid();
      changed = true;
    }
  }
}

void mainWindow_c::updateUndoRedoButtons(void) {
  if (!shapeHistory)
    return;

  bool canU = shapeHistory->canUndo() && !assmThread;
  bool canR = shapeHistory->canRedo() && !assmThread;

  if (BtnUndo) {
    if (canU) BtnUndo->activate();
    else BtnUndo->deactivate();
  }
  if (BtnRedo) {
    if (canR) BtnRedo->activate();
    else BtnRedo->deactivate();
  }

  if (MainMenu) {
    const int ui = liveMenuIndex(MainMenu, cb_Undo_stub);
    const int ri = liveMenuIndex(MainMenu, cb_Redo_stub);
    if (ui >= 0) setLiveMenuActive(MainMenu, ui, canU);
    if (ri >= 0) setLiveMenuActive(MainMenu, ri, canR);
    MainMenu->update();
  }
}

void mainWindow_c::recordShapeAction(int kind) {
  if (shapeHistory)
    shapeHistory->record(puzzle, (shapeHistory_c::actionKind_e)kind, PcSel->getSelection());
  changed = true;
  updateUndoRedoButtons();
}

void mainWindow_c::applyHistoryRestore(unsigned int selected) {
  unsigned int n = puzzle->getNumberOfShapes();
  if (selected >= n)
    selected = n ? n - 1 : (unsigned int)-1;

  if (selected < n)
    activateShape(selected);
  else
    activateClear();

  PcSel->setSelection(selected);
  changed = shapeHistory ? shapeHistory->isModifiedFromSave() : true;
  if (detailsPanel && detailsPanel->visible())
    detailsPanel->populate(puzzle);
  updateInterface();
  StatPieceInfo(PcSel->getSelection());
  redraw();
}

void cb_Undo_stub(Fl_Widget* /*o*/, void* v) { static_cast<mainWindow_c*>(v)->cb_Undo(); }
void mainWindow_c::cb_Undo(void) {
  if (!shapeHistory || !shapeHistory->canUndo() || assmThread)
    return;
  applyHistoryRestore(shapeHistory->undo(puzzle));
}

void cb_Redo_stub(Fl_Widget* /*o*/, void* v) { static_cast<mainWindow_c*>(v)->cb_Redo(); }
void mainWindow_c::cb_Redo(void) {
  if (!shapeHistory || !shapeHistory->canRedo() || assmThread)
    return;
  applyHistoryRestore(shapeHistory->redo(puzzle));
}

void mainWindow_c::changeProblem(unsigned int nr) {
  puzzle->getProblem(nr)->removeAllSolutions();
}

bool mainWindow_c::threadStopped(void) {

  if (assmThread) {

    fl_message("Stop solving process first!");
    return false;
  }

  return true;
}

void mainWindow_c::setFileName(const std::string & f) {
  fname = f;
  copy_label(platform::windowTitle(fname.empty() ? nullptr : fname.c_str()).c_str());
}

bool mainWindow_c::tryToLoad(const char * f, bool * reportedError) {

  if (reportedError) *reportedError = false;

  // it may well be that the file doesn't exist, if it came from the command line
  if (!f) return false;
  if (!fileExists(f)) return false;

  /* A recovery copy newer than the file: a solve was autosaved after the
   * file was last saved, and BurrTools closed or crashed before saving it. */
  std::string from = f;
  bool restored = false;
  {
    const std::string recovery = autosavePathFor(f);
    std::error_code ec;
    if (!recovery.empty() && std::filesystem::exists(recovery, ec) &&
        std::filesystem::last_write_time(recovery, ec) > std::filesystem::last_write_time(f, ec)) {
      int choice = fl_choice("BurrTools autosaved a solve of this puzzle after the file was last "
                             "saved, so it has progress the file does not.\n\n"
                             "Restore the autosaved solve?",
                             "Discard It", "Restore", nullptr);
      if (choice == 1) {
        from = recovery;
        restored = true;
      } else {
        std::filesystem::remove(recovery, ec);
      }
    }
  }

  auto str = openGzFile(from.c_str());
  // openGzFile() can still return nullptr here even though fileExists()
  // just passed: TOCTOU (the file was removed/renamed between the two
  // calls) or a gzopen() allocation failure. Match the fileExists() early
  // return above rather than dereferencing a null stream.
  if (!str) return false;
  xmlParser_c pars(*str);

  puzzle_c * newPuzzle;

  try {
    newPuzzle = new puzzle_c(pars);
  }

  catch (xmlParserException_c &e)
  {
    fl_message("%s",(std::string("load error: ") + e.what()).c_str());
    if (reportedError) *reportedError = true;
    return false;
  }

  setFileName(f);

  /* Sliding pieces are one layer deep: keep the first of any more. */
  std::vector<unsigned int> flattened;
  if (sliding::isSliding(*newPuzzle))
    flattened = sliding::flattenPieces(*newPuzzle);

  ReplacePuzzle(newPuzzle);

  selectEntitiesTab(true);
  activateShape(PcSel->getSelection());
  StatPieceInfo(PcSel->getSelection());
  View3D->getView()->showColors(puzzle, StatusLine->getColorMode());

  /* A restored solve is not in the file yet. */
  changed = restored;

  if (!flattened.empty()) {
    std::string names;
    for (unsigned int s : flattened)
      names += (names.empty() ? "S" : ", S") + std::to_string(s + 1);
    fl_message("Sliding shapes are one layer deep. %s had more than one layer: only the "
               "first layer was kept. Save the puzzle to keep this change.", names.c_str());
    changed = true;
  }

  // check for a started assemblies, and warn user about it
  bool containsStarted = false;

  for (unsigned int p = 0; p < puzzle->getNumberOfProblems(); p++) {
    if (puzzle->getProblem(p)->getSolveState() == SS_SOLVING) {
      containsStarted = true;
      break;
    }
  }

  if (containsStarted)
    fl_message("This puzzle file contains started but not finished search for solutions.");

  if (puzzle->getCommentPopup())
    fl_message("%s",puzzle->getComment().c_str());

  return true;
}


void mainWindow_c::openFromSystem(const char * filename) {

  struct ReentrancyGuard {
    bool & flag;
    explicit ReentrancyGuard(bool & f) : flag(f) { flag = true; }
    ~ReentrancyGuard() { flag = false; }
  };

  if (handlingSystemOpen)
    return;

  ReentrancyGuard guard(handlingSystemOpen);

  /* Opening a puzzle answers File, New's question: close it unchosen. */
  if (gridSelector)
    gridSelector->finish(gridTypeSelectorWindow_c::SEL_CANCEL);

  if (!filename || !filename[0])
    return;

  if (!threadStopped())
    return;

  if (!confirmDiscard("open that puzzle"))
    return;

  bool reportedError = false;

  if (!tryToLoad(filename, &reportedError) && !reportedError)
    fl_message("Could not open %s", filename);
}

void mainWindow_c::ReplacePuzzle(puzzle_c * NewPuzzle) {

  // inform everybody
  colorSelector->setPuzzle(NewPuzzle);
  PcSel->setPuzzle(NewPuzzle);
  if (diskList)
    diskList->setPuzzle(NewPuzzle);
  pieceEdit->setPuzzle(NewPuzzle, 0);
  problemSelector->setPuzzle(NewPuzzle);
  colorAssignmentSelector->setPuzzle(NewPuzzle);
  colconstrList->setPuzzle(NewPuzzle, 0);
  if (NewPuzzle->getNumberOfProblems() > 0) {
    problemResult->setPuzzle(NewPuzzle->getProblem(0));
    PiecesCountList->setPuzzle(NewPuzzle->getProblem(0));
    PcVis->setPuzzle(NewPuzzle->getProblem(0));
  } else {
    problemResult->setPuzzle(0);
    PiecesCountList->setPuzzle(0);
    PcVis->setPuzzle(0);
  }
  shapeAssignmentSelector->setPuzzle(NewPuzzle);
  solutionProblem->setPuzzle(NewPuzzle);

  SolutionSel->value(1);
  SolutionAnim->value(0);

  if (NewPuzzle != puzzle) {
    delete puzzle;
    puzzle = NewPuzzle;
  }

  slidingDisasmDefaulted = false;
  stackingDisasmDefaulted = false;
  if (rodSel)
    rodSel->setPuzzle(puzzle);
  if (rodAssignSel)
    rodAssignSel->setPuzzle(puzzle);
  resetRodHistory();

  if (shapeHistory)
    shapeHistory->reset(puzzle);

  if (View3D)
    View3D->getView()->setStackingView(stacking::isStacking(*puzzle));

  /* Opening a file often leaves the same rod selected, so the list does
   * not notify. The radios and checkboxes still have to match the file. */
  loadRodFields();

  notesInput->value(puzzle->getComment().c_str());
  setNotesButtonsEnabled(false);

  if (detailsPanel && detailsPanel->visible())
    detailsPanel->populate(puzzle);

  guiGridType_c * nggt = new guiGridType_c(puzzle->getGridType());

  // now replace all gridtype dependent gui elements with
  // instances from the guigridtype
  pieceEdit->newGridType(nggt, puzzle);
  pieceTools->newGridType(nggt);

  // for the pieceEditor we need to reset all the edit mode fields
  pieceEdit->editSymmetries(editSymmetries);
  switch(editChoice->getSelected()) {
    case 0: pieceEdit->editChoice(gridEditor_c::TSK_SET); break;
    case 1: pieceEdit->editChoice(gridEditor_c::TSK_VAR); break;
    case 2: pieceEdit->editChoice(gridEditor_c::TSK_RESET); break;
    case 3: pieceEdit->editChoice(gridEditor_c::TSK_COLOR); break;
  }
  switch(editMode->getSelected()) {
    case 0: pieceEdit->editType(gridEditor_c::EDT_RUBBER); break;
    case 1: pieceEdit->editType(gridEditor_c::EDT_SINGLE); break;
  }

  delete ggt;
  ggt = nggt;

  applySolverOptions(solutionProblem->getSelection());
}

// cppcheck-suppress duplInheritedMember
void mainWindow_c::show(int argn, char ** argv) {
  LFl_Double_Window::show();

  int arg = 1;

  // try all command line switches, until either they are
  // all tried or one got loaded successfully
  while (arg < argn)
    if (tryToLoad(argv[arg]))
      break;
    else
      arg++;
}

void mainWindow_c::activateClear(void) {
  View3D->getView()->showNothing();
  pieceEdit->clearPuzzle();
  pieceTools->setVoxelSpace(0, 0);

  SolutionEmpty = true;
}

void mainWindow_c::activateShape(unsigned int number) {

  if ((number < puzzle->getNumberOfShapes())) {

    View3D->getView()->showSingleShape(puzzle, number);
    pieceEdit->setPuzzle(puzzle, number);
    pieceTools->setVoxelSpace(puzzle, number);

    PcSel->setSelection(number);
    if (diskList)
      diskList->setSelection(number);
    applySlidingGridMode();

  } else {

    View3D->getView()->showNothing();
    pieceEdit->clearPuzzle();
    pieceTools->setVoxelSpace(0, 0);
  }

  SolutionEmpty = true;
  refreshSlideValid();
}

void mainWindow_c::activateProblem(unsigned int prob) {

  if (prob < puzzle->getNumberOfProblems()) {
    if (stacking::isStacking(*puzzle)) {
      View3D->getView()->showStacking(puzzle->getProblem(prob), stackingEditGoal);
      easeRodZoom(View3D);
    } else
      View3D->getView()->showProblem(puzzle, prob, shapeAssignmentSelector->getSelection());
  } else
    View3D->getView()->showNothing();

  if (puzzle && stacking::isStacking(*puzzle))
    refreshStackList();

  SolutionEmpty = true;
}

static void setMovesRotationsMetrics(Fl_Output * movesOut, Fl_Output * rotsOut, const disassembly_c * da)
{
  if (!da) {
    movesOut->value("");
    rotsOut->value("");
    return;
  }

  char buf[32];
  snprintf(buf, sizeof(buf), "%u", da->sumMoves());
  movesOut->value(buf);
  snprintf(buf, sizeof(buf), "%u", da->sumRotations());
  rotsOut->value(buf);
}

void mainWindow_c::activateSolution(unsigned int prob, unsigned int num) {

  disassemble.reset();
  shownSolution.reset();

  if (prob < puzzle->getNumberOfProblems()) {

    problem_c * pr = puzzle->getProblem(prob);
    problem_c::SolutionsLock lock(*pr);

    if (num < pr->getNumberOfSavedSolutions()) {

    shownSolution = pr->shareSavedSolution(num);

    PcVis->setPuzzle(puzzle->getProblem(prob));
    PcVis->setAssembly(pr->getSavedSolution(num)->getAssembly());
    AssemblyNumber->value(pr->getSavedSolution(num)->getAssemblyNumber()+1);

    const disassembly_c * disassemblyMetrics = pr->getSavedSolution(num)->getDisassemblyInfo();
    setMovesRotationsMetrics(MovesMetric, RotationsMetric, disassemblyMetrics);

    if (pr->getSavedSolution(num)->getDisassembly()) {
      const separation_c * sep = pr->getSavedSolution(num)->getDisassembly();
      const bool stackingSol = stacking::isStacking(*puzzle);
      SolutionAnim->activate();
      /* A stacking transfer is stored as lift, cross and drop. The slider
       * counts transfers, and animStep() scales it back to placements. */
      SolutionAnim->range(0, stackingSol ? stacking::logicalMoves(*sep) : sep->sumSteps());

      SolutionsInfo->value(pr->getNumberOfSavedSolutions());

      if (stackingSol) {
        char levelText[32];
        snprintf(levelText, sizeof(levelText), "%u", stacking::logicalMoves(*sep));
        MovesInfo->value(levelText);
        MovesMetric->value(levelText);
      } else {
        /* "steps (moves per stage)": as long as the moves text needs. */
        const std::string levelText = std::to_string(sep->sumSteps()) + " (" + sep->movesText() + ")";
        MovesInfo->value(levelText.c_str());
      }

      unsigned int animSize = stackingSol
          ? stacking::boardSpan(*pr)
          : 2 * getResultShape(*pr)->getBiggestDimension();
      disassemble = std::make_unique<disasmToMoves_c>(sep, animSize, pr->getNumberOfPieces());
      /* A sliding move can turn corners. Follow its route, not the diagonal. */
      if (sliding::isSliding(*puzzle))
        sliding::applySlideRoutes(*pr, *sep, *disassemble);
      disassemble->setStep(animStep(), config.useBlendedRemoving(), true);

      if (prob < puzzle->getNumberOfProblems()) { View3D->getView()->showAssembly(puzzle->getProblem(prob), num); if (stacking::isStacking(*puzzle)) easeRodZoom(View3D); }
      View3D->getView()->updatePositions(disassemble.get());
      View3D->getView()->updateVisibility(PcVis);

      SolutionNumber->value(pr->getSavedSolution(num)->getSolutionNumber()+1);

    } else if (pr->getSavedSolution(num)->getDisassemblyInfo()) {

      SolutionAnim->range(0, 0);
      SolutionAnim->deactivate();

      SolutionsInfo->value(pr->getNumberOfSavedSolutions());

      const disassembly_c * info = pr->getSavedSolution(num)->getDisassemblyInfo();
      const std::string levelText = std::to_string(info->sumSteps()) + " (" + info->movesText() + ")";
      MovesInfo->value(levelText.c_str());

      if (prob < puzzle->getNumberOfProblems()) { View3D->getView()->showAssembly(puzzle->getProblem(prob), num); if (stacking::isStacking(*puzzle)) easeRodZoom(View3D); }
      View3D->getView()->updateVisibility(PcVis);

      SolutionNumber->value(pr->getSavedSolution(num)->getSolutionNumber()+1);

    } else {

      SolutionAnim->range(0, 0);
      SolutionAnim->deactivate();
      MovesInfo->value("");

      if (prob < puzzle->getNumberOfProblems()) { View3D->getView()->showAssembly(puzzle->getProblem(prob), num); if (stacking::isStacking(*puzzle)) easeRodZoom(View3D); }
      View3D->getView()->updateVisibility(PcVis);

      SolutionNumber->value(0);
    }

    SolutionEmpty = false;

    } else {

      View3D->getView()->showNothing();
      SolutionEmpty = true;

      SolutionAnim->range(0, 0);
      SolutionAnim->deactivate();
      MovesInfo->value("");

      PcVis->setPuzzle(0);

      AssemblyNumber->value(0);
      SolutionNumber->value(0);
      setMovesRotationsMetrics(MovesMetric, RotationsMetric, 0);
    }

  } else {

    View3D->getView()->showNothing();
    SolutionEmpty = true;

    SolutionAnim->range(0, 0);
    SolutionAnim->deactivate();
    MovesInfo->value("");

    PcVis->setPuzzle(0);

    AssemblyNumber->value(0);
    SolutionNumber->value(0);
    setMovesRotationsMetrics(MovesMetric, RotationsMetric, 0);
  }
}

const char * timeToString(float time) {

  static char tmp[50];

  if (time < 60)                               snprintf(tmp, 50, "%i seconds",     int(time/(1                 )));
  else if (time < 60*60)                       snprintf(tmp, 50, "%.1f minutes",   time/(60                    ));
  else if (time < 60*60*24)                    snprintf(tmp, 50, "%.1f hours",     time/(60*60                 ));
  else if (time < 60*60*24*30)                 snprintf(tmp, 50, "%.1f days",      time/(60*60*24              ));
  else if (time < 60*60*24*365.2422)           snprintf(tmp, 50, "%.1f months",    time/(60*60*24*30           ));
  else if (time < 60*60*24*365.2422*1000)      snprintf(tmp, 50, "%.1f years",     time/(60*60*24*365.2422     ));
  else if (time < 60*60*24*365.2422*1000*1000) snprintf(tmp, 50, "%.1f millennia", time/(60*60*24*365.2422*1000));
  else                                         snprintf(tmp, 50, "ages");

  return tmp;
}

/* Time used: milliseconds under a second, as timeToString beyond. */
static const char * usedTimeToString(unsigned long long ms) {
  static char tmp[50];
  if (ms < 1000) {
    snprintf(tmp, sizeof(tmp), "%llu milliseconds", ms);
    return tmp;
  }
  return timeToString(ms / 1000.0f);
}

void mainWindow_c::initViewMenuIcons(void) {

#ifdef __APPLE__
  /* Fl_Sys_Menu_Bar turns Fl_Multi_Label image+text items into blank native
   * rows (the radio check still appears). Keep the text labels so View is
   * readable on the system menu bar.
   */
  return;
#else
  static const char * names[4] = {
    "Display normally with shape color",
    "Display with colour constraint colors",
    "Display in anaglyph mode",
    "Display in anaglyph mode with glasses swapped"
  };
  static pixmapList_c pm;
  static Fl_Multi_Label ml[4];
  static const char ** xpms[4] = {
    ViewModeNormal_xpm,
    ViewModeColor_xpm,
    ViewMode3D_xpm,
    ViewMode3DL_xpm
  };
  static Fl_Callback * const cbs[4] = {
    cb_ViewMode0_stub, cb_ViewMode1_stub, cb_ViewMode2_stub, cb_ViewMode3_stub
  };

  for (int i = 0; i < 4; i++) {
    ml[i].typea = FL_IMAGE_LABEL;
    ml[i].labela = (const char *)pm.get(xpms[i]);
    ml[i].typeb = FL_NORMAL_LABEL;
    ml[i].labelb = names[i];

    if (MainMenu) {
      int idx = liveMenuIndex(MainMenu, cbs[i]);
      if (idx >= 0) {
        Fl_Menu_Item * items = const_cast<Fl_Menu_Item *>(MainMenu->menu());
        items[idx].multi_label(&ml[i]);
      }
    }
  }
#endif
}

void mainWindow_c::selectEntitiesTab(bool resetZoom) {
  TaskSelectionTab->value(TabPieces);
  cb_TaskSelectionTab(TaskSelectionTab);
  if (resetZoom) {
    View3D->resetZoomToDefault();
    ViewSizes[0] = LView3dGroup::defaultZoom;
  }
}

void mainWindow_c::updateInterface(void) {

  syncPieceColors();

  /* The selected problem may want another solver: stacking rod sets decide. */
  syncSolverTypeMenu();

  // update the menu items activate state

  const bool exportActive = puzzle->getNumberOfShapes() > 0;
  const bool stlActive    = (ggt->getGridType()->getCapabilities() & gridType_c::CAP_STLEXPORT)
                            && puzzle->getNumberOfShapes() > 0;

  if (exportActive != menuExportActive || stlActive != menuSTLActive) {
    const int imageIndex = liveMenuIndex(MainMenu, cb_ImageExport_stub);
    const int stlIndex   = liveMenuIndex(MainMenu, cb_STLExport_stub);
    const int scadIndex  = liveMenuIndex(MainMenu, cb_Export_Scad_stub);
    const int vecIndex   = liveMenuIndex(MainMenu, cb_ImageExportVector_stub);

    bt_assert(imageIndex >= 0);
    bt_assert(stlIndex >= 0);

    if (imageIndex >= 0 && stlIndex >= 0) {
      setLiveMenuActive(MainMenu, imageIndex, exportActive);
      setLiveMenuActive(MainMenu, stlIndex, stlActive);
      if (scadIndex >= 0) setLiveMenuActive(MainMenu, scadIndex, exportActive);
      if (vecIndex >= 0) setLiveMenuActive(MainMenu, vecIndex, exportActive);
      MainMenu->update();
      menuExportActive = exportActive;
      menuSTLActive    = stlActive;
    }
  }

  updateUndoRedoButtons();

  const bool slidingPuzzle = sliding::isSliding(*puzzle);
  const bool stackingPuzzle = stacking::isStacking(*puzzle);
  syncStackingChrome();
  if (colourAssignmentGroup) {
    if (slidingPuzzle || stackingPuzzle) colourAssignmentGroup->hide();
    else colourAssignmentGroup->show();
  }
  if (colourConstraintsGroup) {
    if (slidingPuzzle || stackingPuzzle) colourConstraintsGroup->hide();
    else colourConstraintsGroup->show();
  }
  if (slidingPuzzle && editChoice) {
    /* Colour paint mode is unused; keep Walls/set as default. */
    if (editChoice->getSelected() == 3)
      editChoice->select(0);
  }

  unsigned int prob = solutionProblem->getSelection();

  if (TaskSelectionTab->value() == TabPieces) {
    updateEntitiesTab(slidingPuzzle);
  } else if (TaskSelectionTab->value() == TabProblems) {

    problem_c * pr = (problemSelector->getSelection() < puzzle->getNumberOfProblems())
      ? puzzle->getProblem(problemSelector->getSelection()) : 0;

    // problem tab
    PiecesCountList->setPuzzle(pr);
    colconstrList->setPuzzle(puzzle, problemSelector->getSelection());
    problemResult->setPuzzle(pr);

    // problems can only be renames and copied, when something valid is selected
    if (slidingPuzzle) {
      BtnNewProb->deactivate();
      BtnDelProb->deactivate();
      BtnCpyProb->deactivate();
      if (problemSelector->getSelection() < puzzle->getNumberOfProblems())
        BtnRenProb->activate();
      else
        BtnRenProb->deactivate();
    } else {
      BtnNewProb->activate();
      if (problemSelector->getSelection() < puzzle->getNumberOfProblems()) {
        BtnCpyProb->activate();
        BtnRenProb->activate();
      } else {
        BtnCpyProb->deactivate();
        BtnRenProb->deactivate();
      }
    }

    // problems can only be shifted around when the corresponding neighbour is
    // available
    if ((problemSelector->getSelection() > 0) && (problemSelector->getSelection() < puzzle->getNumberOfProblems()) && !assmThread)
      BtnProbLeft->activate();
    else
      BtnProbLeft->deactivate();
    if ((problemSelector->getSelection()+1 < puzzle->getNumberOfProblems()) && !assmThread)
      BtnProbRight->activate();
    else
      BtnProbRight->deactivate();

    if (problemSelector->getSelection() < puzzle->getNumberOfProblems() && !assmThread)
    {
      unsigned int current;
      unsigned int p = problemSelector->getSelection();
      unsigned int s = shapeAssignmentSelector->getSelection();

      problem_c * pr = puzzle->getProblem(p);

      if (stackingPuzzle) {
        /* The stack list shows the top disc first. Up needs a disc above
         * the selection, down a disc below it. */
        unsigned int depth = 0;
        if (stackingBoard(pr)) {
          const stacking::stackMap_c & map = stackingEditGoal ? pr->goalStacks() : pr->startStacks();
          unsigned int rod = selectedStackRod();
          if (rod < map.rods.size())
            depth = (unsigned int)map.rods[rod].size();
        }
        unsigned int display = rodStackList ? rodStackList->getSelection() : (unsigned int)-1;
        if (display < depth && display > 0)
          BtnProbShapeLeft->activate();
        else
          BtnProbShapeLeft->deactivate();
        if (display + 1 < depth)
          BtnProbShapeRight->activate();
        else
          BtnProbShapeRight->deactivate();
      } else {

      for (current = 0; current < pr->getNumberOfParts(); current++)
        if (pr->getShapeIdOfPart(current) == s)
          break;

      if (current && (current < pr->getNumberOfParts()))
        BtnProbShapeLeft->activate();
      else
        BtnProbShapeLeft->deactivate();
      if (current+1 < pr->getNumberOfParts())
        BtnProbShapeRight->activate();
      else
        BtnProbShapeRight->deactivate();
      }

    } else {
      BtnProbShapeRight->deactivate();
      BtnProbShapeLeft->deactivate();
    }

    // problems can only be deleted, something valid is selected and the
    // assembler is not running
    if (!slidingPuzzle && (problemSelector->getSelection() < puzzle->getNumberOfProblems()) && !assmThread)
      BtnDelProb->activate();
    else
      BtnDelProb->deactivate();

    // we can only edit colour constraints when a valid problem is selected
    // the selected colour is valid
    // the assembler is not running or not busy with the selected problem
    if ((problemSelector->getSelection() < puzzle->getNumberOfProblems()) &&
        (colorAssignmentSelector->getSelection() < puzzle->colorNumber()) &&
        (!assmThread || (&(assmThread->getProblem()) != puzzle->getProblem(problemSelector->getSelection())))) {

      problem_c * pr = puzzle->getProblem(problemSelector->getSelection());

      // check, if the given colour is already added
      if (colconstrList->GetSortByResult()) {
        if (pr->placementAllowed(colorAssignmentSelector->getSelection()+1,
                                 colconstrList->getSelection()+1)) {
          BtnColAdd->deactivate();
          BtnColRem->activate();
        } else {
          BtnColAdd->activate();
          BtnColRem->deactivate();
        }
      } else {
        if (pr->placementAllowed(colconstrList->getSelection()+1,
                                 colorAssignmentSelector->getSelection()+1)) {
          BtnColAdd->deactivate();
          BtnColRem->activate();
        } else {
          BtnColAdd->activate();
          BtnColRem->deactivate();
        }
      }
    } else {
      BtnColAdd->deactivate();
      BtnColRem->deactivate();
    }

    // we can only change shapes, when a valid problem is selected
    // a valid shape is selected
    // the assembler is not running or not busy with out problem
    if ((problemSelector->getSelection() < puzzle->getNumberOfProblems()) &&
        (shapeAssignmentSelector->getSelection() < puzzle->getNumberOfShapes()) &&
        (!assmThread || (&(assmThread->getProblem()) != puzzle->getProblem(problemSelector->getSelection())))) {
      BtnSetResult->activate();

      problem_c * pr = puzzle->getProblem(problemSelector->getSelection());
      unsigned int selShape = shapeAssignmentSelector->getSelection();
      bool required = slidingPuzzle && sliding::shapeIsRequired(*pr, selShape);

      if (slidingPuzzle)
        BtnSetResult->deactivate();

      // we can only add a shape, when it's not the result of the current problem
      if ((!pr->resultValid() || pr->getResultId() != selShape) && !required)
        BtnAddShape->activate();
      else
        BtnAddShape->deactivate();

      bool found = false;

      for (unsigned int p = 0; p < pr->getNumberOfParts(); p++)
        if (pr->getShapeIdOfPart(p) == selShape) {
          found = true;
          break;
        }

      if (found && !required) {
        BtnRemShape->activate();
        BtnMinZero->activate();
      } else {
        BtnRemShape->deactivate();
        BtnMinZero->deactivate();
      }

    } else {
      BtnSetResult->deactivate();
      BtnAddShape->deactivate();
      BtnRemShape->deactivate();
      BtnMinZero->deactivate();
    }

    // we can edit the groups, when we have a problem with at least one shape and
    // the assembler is not working on the current problem
    if (!slidingPuzzle && !stackingPuzzle &&
        (problemSelector->getSelection() < puzzle->getNumberOfProblems()) &&
        (!assmThread || (&(assmThread->getProblem()) != puzzle->getProblem(problemSelector->getSelection())))) {
      BtnGroup->activate();
    } else {
      BtnGroup->deactivate();
    }

    if ((problemSelector->getSelection() < puzzle->getNumberOfProblems()) &&
        (!assmThread || (&(assmThread->getProblem()) != puzzle->getProblem(problemSelector->getSelection())))) {
      BtnAddAll->activate();
      BtnRemAll->activate();
      if (stackingPuzzle)
        BtnSetAllRange->deactivate();
      else
        BtnSetAllRange->activate();
    } else {
      BtnAddAll->deactivate();
      BtnRemAll->deactivate();
      BtnSetAllRange->deactivate();
    }

    if (stackingPuzzle) {
      bool busy = assmThread && problemSelector->getSelection() < puzzle->getNumberOfProblems() &&
                  (&(assmThread->getProblem()) == puzzle->getProblem(problemSelector->getSelection()));
      if (!busy && problemSelector->getSelection() < puzzle->getNumberOfProblems() &&
          rodAssignSel && rodAssignSel->getSelection() < puzzle->rodSetCount())
        BtnSetResult->activate();
      else
        BtnSetResult->deactivate();
      BtnMinZero->deactivate();

      bool hasBoard = !busy &&
          problemSelector->getSelection() < puzzle->getNumberOfProblems() &&
          puzzle->getProblem(problemSelector->getSelection())->rodSetValid();
      if (stackRodBar) {
        if (hasBoard) stackRodBar->activate();
        else stackRodBar->deactivate();
      }
      if (stackStartMode) {
        if (hasBoard) stackStartMode->activate();
        else stackStartMode->deactivate();
      }
      if (stackGoalMode) {
        if (hasBoard) stackGoalMode->activate();
        else stackGoalMode->deactivate();
      }
      if (!hasBoard) {
        BtnProbShapeLeft->deactivate();
        BtnProbShapeRight->deactivate();
        BtnAddShape->deactivate();
        BtnRemShape->deactivate();
        BtnAddAll->deactivate();
        BtnRemAll->deactivate();
        BtnSetAllRange->deactivate();
        BtnGroup->deactivate();
      }
    }

  } else {
    updateSolverTab(stackingPuzzle, prob);
  }
  TaskSelectionTab->redraw();
  TaskSelectionTab->resize(TaskSelectionTab->x(), TaskSelectionTab->y(),
                           TaskSelectionTab->w(), TaskSelectionTab->h());

  /* File > Export > Solution Animation: only while a disassembly is shown. */
  {
    const bool animated = disassemble && !SolutionEmpty && !stackingPuzzle;
    static int shown = -1;
    if ((int)animated != shown) {
      shown = animated;
      if (MainMenu) {
        const int idx = liveMenuIndex(MainMenu, cb_ExportGltf_stub);
        if (idx >= 0) {
          setLiveMenuActive(MainMenu, idx, animated);
          MainMenu->update();
        }
      }
    }
  }

  /* File > Export > Paused solver state: only for a paused solve. */
  {
    const bool paused = solutionProblem && problemPaused(solutionProblem->getSelection());
    static int shown = -1;
    if ((int)paused != shown) {
      shown = paused;
      if (MainMenu) {
        const int idx = liveMenuIndex(MainMenu, cb_ExportPaused_stub);
        if (idx >= 0) {
          setLiveMenuActive(MainMenu, idx, paused);
          MainMenu->update();
        }
      }
    }
  }
}

/* updateInterface for the Entities tab. */
void mainWindow_c::updateEntitiesTab(bool slidingPuzzle) {
  // shapes tab

  // we can only delete colours, when something valid is selected
  // and no assembler is running
  if ((colorSelector->getSelection() > 0) && !assmThread)
    BtnDelColor->activate();
  else
    BtnDelColor->deactivate();

  // colours can be changed for all colours except the neutral colour
  if (colorSelector->getSelection() > 0)
    BtnChnColor->activate();
  else
    BtnChnColor->deactivate();

  // a shape's own colour; not the tray of a sliding puzzle, which shows the pieces'
  if (BtnShapeColor) {
    if (PcSel->getSelection() < puzzle->getNumberOfShapes() &&
        !(slidingPuzzle && sliding::isStartGoalShape(puzzle->getShape(PcSel->getSelection()))))
      BtnShapeColor->activate();
    else
      BtnShapeColor->deactivate();
  }

  // we can only edit and copy shapes, when something valid is selected
  if (PcSel->getSelection() < puzzle->getNumberOfShapes()) {
    BtnCpyShape->activate();
    BtnRenShape->activate();
    pieceEdit->activate();
  } else {
    BtnCpyShape->deactivate();
    BtnRenShape->deactivate();
    pieceEdit->deactivate();
  }

  // shapes can only be moved, when the neighbour shape is there
  if ((PcSel->getSelection() > 0) && (PcSel->getSelection() < puzzle->getNumberOfShapes()) && !assmThread)
    BtnShapeLeft->activate();
  else
    BtnShapeLeft->deactivate();
  if ((PcSel->getSelection()+1 < puzzle->getNumberOfShapes()) && !assmThread)
    BtnShapeRight->activate();
  else
    BtnShapeRight->deactivate();

  // we can only delete shapes, when something valid is selected
  // and no assembler is running
  if ((PcSel->getSelection() < puzzle->getNumberOfShapes()) && !assmThread &&
      !(slidingPuzzle && sliding::isStartGoalShape(puzzle->getShape(PcSel->getSelection())))) {
    BtnDelShape->activate();
  } else {
    BtnDelShape->deactivate();
  }

  if (BtnNewStartGoal) {
    if (slidingPuzzle && !assmThread) BtnNewStartGoal->activate();
    else BtnNewStartGoal->deactivate();
  }
  if (BtnDelStartGoal) {
    bool sg = slidingPuzzle && PcSel->getSelection() < puzzle->getNumberOfShapes() &&
              sliding::isStartGoalShape(puzzle->getShape(PcSel->getSelection()));
    if (sg && !assmThread) BtnDelStartGoal->activate();
    else BtnDelStartGoal->deactivate();
  }
  if (startGoalRow && ((bool)startGoalRow->visible() != slidingPuzzle)) {
    if (slidingPuzzle) {
      startGoalRow->show();
      if (startGoalGap) startGoalGap->show();
    } else {
      startGoalRow->hide();
      if (startGoalGap) startGoalGap->hide();
    }
    if (layouter_c * column = dynamic_cast<layouter_c*>(startGoalRow->parent())) {
      column->invalidateMinSize();
      column->resize(column->x(), column->y(), column->w(), column->h());
    }
  }

  updateUndoRedoButtons();

  const problem_c * pr = (assmThread) ? &assmThread->getProblem() : 0;

  // we can only edit shapes, when something valid is selected and
  // either no assembler is running or the shape is not in the problem that the assembler works on
  if ((PcSel->getSelection() < puzzle->getNumberOfShapes()) &&
      (!assmThread || !pr->usesShape(PcSel->getSelection()))) {
    pieceTools->activate();
  } else {
    pieceTools->deactivate();
  }

  // when the current shape is in the assembler we lock the editor, only viewing is possible
  if (assmThread && (pr->usesShape(PcSel->getSelection())))
    pieceEdit->deactivate();
  else
    pieceEdit->activate();

  if (puzzle->colorNumber() < 63)
    BtnNewColor->activate();
  else
    BtnNewColor->deactivate();
}

/* updateInterface for the Solver tab, on problem prob. */
void mainWindow_c::updateSolverTab(bool stackingPuzzle, unsigned int prob) {

  /* While the solver runs, read its assembler through the thread: it may
   * be replacing the problem's one. */
  const bool running = assmThread &&
      (prob < puzzle->getNumberOfProblems()) &&
      (&(assmThread->getProblem()) == puzzle->getProblem(prob));
  /* One picture of the running solve for everything shown below. */
  solveProgress_c progress;
  if (assmThread)
    progress = assmThread->getProgressSnapshot();
  float asmFrac = 0;
  if (running)
    asmFrac = progress.assemblyFraction;
  else if ((prob < puzzle->getNumberOfProblems()) && puzzle->getProblem(prob)->getAssembler())
    asmFrac = puzzle->getProblem(prob)->getAssembler()->getFinished();
  float finished = asmFrac;
  if (running)
    finished = progress.overall;
  /* Stacking has no assembler, so getFinished() stays 0 after the search.
   * A finished search is the whole job. */
  else if (prob < puzzle->getNumberOfProblems() &&
           stacking::isStacking(*puzzle) &&
           puzzle->getProblem(prob)->getSolveState() == SS_SOLVED)
    finished = 1;

  if (prob < puzzle->getNumberOfProblems()) {

    problem_c * pr = puzzle->getProblem(prob);

    unsigned long numSol = 0;
    bool selectedHasDisassembly = false;

    {
      problem_c::SolutionsLock lock(*pr);
      numSol = pr->getNumberOfSavedSolutions();
      if ((SolutionSel->value() >= 1) &&
          ((int)SolutionSel->value()-1) < (int)numSol)
        selectedHasDisassembly = (pr->getSavedSolution((int)SolutionSel->value()-1)->getDisassembly() != 0);
    }

    // solution tab
    PcVis->setPuzzle(pr);

    // we have a valid problem selected, so update the information visible

    {
      static char tmp[100];
      if (running && !progress.overallKnown && !progress.running.empty()) {
        /* Take-aparts of a length nobody can know yet: show how far the
         * level at hand of the search is, which is exact. */
        SolvingProgress->value(100*progress.levelFraction);
        snprintf(tmp, 100, "level %u: %.0f%%", progress.running[0].level, 100*progress.levelFraction);
      } else {
        SolvingProgress->value(100*finished);
        snprintf(tmp, 100, "%.1f%%", 100*finished);
      }
      SolvingProgress->show();
      SolvingProgress->label(tmp);
    }

    if (numSol > 0) {

      SolutionSel->activate();
      SolutionSel->range(1, numSol);
      if (SolutionSel->value() > numSol)
        SolutionSel->value(numSol);
      SolutionsInfo->value(numSol);

      // if we are in the solve tab and have a valid solution
      // we can activate that
      if (SolutionEmpty && (numSol > 0)) {
        activateSolution(prob, 0);
        SolutionSel->value(1);
      }

    } else {

      SolutionSel->range(1, 1);
      SolutionSel->value(1);
      SolutionSel->deactivate();
      SolutionsInfo->value(0);
      SolutionAnim->range(0, 0);
      SolutionAnim->deactivate();
      MovesInfo->value("");

      AssemblyNumber->value(0);
      SolutionNumber->value(0);
      setMovesRotationsMetrics(MovesMetric, RotationsMetric, 0);
    }

    if (pr->numAssembliesKnown()) {
      OutputAssemblies->value(pr->getNumAssemblies());
    } else {
      OutputAssemblies->value(0);
    }

    if (pr->numSolutionsKnown()) {
      OutputSolutions->value(pr->getNumSolutions());
    } else {
      OutputSolutions->value(0);
    }

    // the placement and movement browsers are only available when an assembler
    // is present and the solver is not running
    if (pr->getAssembler() && !assmThread) {
      BtnPlacement->activate();

      if ((ggt->getGridType()->getCapabilities() & gridType_c::CAP_DISASSEMBLE) &&
          numSol > 0 &&
          (SolutionSel->value()-1) < numSol)
        BtnMovement->activate();
      else
        BtnMovement->deactivate();
    } else {
      BtnPlacement->deactivate();
      BtnMovement->deactivate();
    }

    if ((ggt->getGridType()->getCapabilities() & gridType_c::CAP_STLEXPORT) &&
        numSol > 0 &&
        pr->getNumberOfParts() > 0)
    {
      BtnExportSolutionSTL->activate();
    }
    else
    {
      BtnExportSolutionSTL->deactivate();
    }

    if (numSol >= 2) {
      BtnSrtFind->activate();
      BtnSrtLevel->activate();
      BtnSrtMoves->activate();
      BtnSrtPieces->activate();
    } else {
      BtnSrtFind->deactivate();
      BtnSrtLevel->deactivate();
      BtnSrtMoves->deactivate();
      BtnSrtPieces->deactivate();
    }

    if (numSol > 0) {
      BtnDelAll->activate();
      if (SolutionSel->value() > 1)
        BtnDelBefore->activate();
      else
        BtnDelBefore->deactivate();

      BtnDelAt->activate();

      if ((SolutionSel->value()-1) < (numSol-1))
        BtnDelAfter->activate();
      else
        BtnDelAfter->deactivate();

      BtnDelDisasm->activate();
    } else {
      BtnDelAll->deactivate();
      BtnDelBefore->deactivate();
      BtnDelAt->deactivate();
      BtnDelAfter->deactivate();
      BtnDelDisasm->deactivate();
    }

    if ((SolutionSel->value() >= 1) &&
        ((int)SolutionSel->value()-1) < (int)numSol &&
        selectedHasDisassembly) {
      BtnDisasmDel->activate();
    } else {
      BtnDisasmDel->deactivate();
    }

    if (numSol > 0) {
      BtnDisasmDelAll->activate();
    } else {
      BtnDisasmDelAll->deactivate();
    }

    if (ggt->getGridType()->getCapabilities() & gridType_c::CAP_DISASSEMBLE) {

      if (numSol > 0) {
        BtnDisasmAdd->activate();
        BtnDisasmAddAll->activate();
        BtnDisasmAddMissing->activate();
      } else {
        BtnDisasmAdd->deactivate();
        BtnDisasmAddAll->deactivate();
        BtnDisasmAddMissing->deactivate();
      }
    } else {
      BtnDisasmAdd->deactivate();
      BtnDisasmAddAll->deactivate();
      BtnDisasmAddMissing->deactivate();
    }

    if (stackingPuzzle) {
      BtnDisasmDel->deactivate();
      BtnDisasmDelAll->deactivate();
      BtnDisasmAdd->deactivate();
      BtnDisasmAddAll->deactivate();
      BtnDisasmAddMissing->deactivate();
      BtnDelDisasm->deactivate();
      BtnPlacement->deactivate();
      BtnMovement->deactivate();
    }

    /* While a solve adds to and trims this list, deleting from it or
     * taking a solution apart here would race with it. Sorting is safe:
     * the solver keeps the chosen order (live sort). */
    if (assmThread && &assmThread->getProblem() == pr) {
      for (Fl_Widget * b : {(Fl_Widget *)BtnDelAll, (Fl_Widget *)BtnDelBefore, (Fl_Widget *)BtnDelAt,
                            (Fl_Widget *)BtnDelAfter, (Fl_Widget *)BtnDelDisasm, (Fl_Widget *)BtnDisasmDel,
                            (Fl_Widget *)BtnDisasmDelAll, (Fl_Widget *)BtnDisasmAdd,
                            (Fl_Widget *)BtnDisasmAddAll, (Fl_Widget *)BtnDisasmAddMissing})
        b->deactivate();
    }

    updateSolverOptionCheckboxes();

    // the step button is only active when the placements browser can be active AND when the puzzle is not
    // yet completely solved
    if (pr->getAssembler() && !assmThread && (pr->getSolveState() != SS_SOLVED)) {
      if (BtnStep) BtnStep->activate();
    } else {
      if (BtnStep) BtnStep->deactivate();
    }
  } else {

    // no valid problem available, keep value fields in place so labels stay aligned

    SolutionSel->range(1, 1);
    SolutionSel->value(1);
    SolutionSel->deactivate();
    SolutionsInfo->value(0);
    OutputSolutions->value(0);
    SolutionAnim->range(0, 0);
    SolutionAnim->deactivate();
    MovesInfo->value("");

    AssemblyNumber->value(0);
    SolutionNumber->value(0);
    setMovesRotationsMetrics(MovesMetric, RotationsMetric, 0);

    SolvingProgress->hide();
    OutputAssemblies->value(0);

    BtnPlacement->deactivate();
    BtnMovement->deactivate();
    if (BtnStep) BtnStep->deactivate();
    if (BtnPrepare) BtnPrepare->deactivate();

    BtnSrtFind->deactivate();
    BtnSrtLevel->deactivate();
    BtnSrtMoves->deactivate();
    BtnSrtPieces->deactivate();
    BtnDelAll->deactivate();
    BtnDelBefore->deactivate();
    BtnDelAt->deactivate();
    BtnDelAfter->deactivate();
    BtnDelDisasm->deactivate();
    BtnDisasmDel->deactivate();
    BtnDisasmDelAll->deactivate();
    BtnDisasmAdd->deactivate();
    BtnDisasmAddAll->deactivate();
    BtnDisasmAddMissing->deactivate();
    BtnExportSolutionSTL->deactivate();

    PcVis->setPuzzle(0);
  }


  if (assmThread && (&(assmThread->getProblem()) == puzzle->getProblem(prob))) {

    problem_c * pr = puzzle->getProblem(prob);

    // a thread is currently running

    unsigned long long usedMs = assmThread->getTimeMs();
    if (pr->usedTimeKnown())
      usedMs += pr->getUsedMs();

    TimeUsed->value(usedTimeToString(usedMs));
    if (progress.secondsLeft >= 1)
      TimeEst->value(timeToString((float)progress.secondsLeft));
    else
      TimeEst->value("unknown");

  } else {

    if ((prob < puzzle->getNumberOfProblems()) && puzzle->getProblem(prob)->usedTimeKnown()) {
      problem_c * pr = puzzle->getProblem(prob);
      TimeUsed->value(usedTimeToString(pr->getUsedMs()));
    } else {
      TimeUsed->value("");
    }

    TimeEst->value("");
  }

  if (assmThread) {

    switch(assmThread->currentAction()) {
    case solveThread_c::ACT_PREPARATION:
    case solveThread_c::ACT_REDUCE:
      OutputActivity->value(progress.activity().c_str());
      break;
    case solveThread_c::ACT_ASSEMBLING:
      if (sliding::isSliding(*puzzle) && assmThread->disassemblyEnabled()) {
        /* A long sliding search should not look frozen. */
        char tmp[96];
        if (assmThread->getSearchDepth() > 0)
          snprintf(tmp, 96, "slide search: %s moves deep\n%s arrangements",
                   withCommas(assmThread->getSearchDepth()).c_str(),
                   withCommas(assmThread->getSlideProgress()).c_str());
        else
          snprintf(tmp, 96, "slide search: %s arrangements",
                   withCommas(assmThread->getSlideProgress()).c_str());
        OutputActivity->value(tmp);
      } else if (stacking::isStacking(*puzzle) && assmThread->panexSearch()) {
        /* Short enough for the Activity line: 3.7M, 12.4B. */
        const double n = (double)assmThread->getSlideProgress();
        char count[32];
        if (n >= 1e9)
          snprintf(count, sizeof(count), "%.1fB", n / 1e9);
        else if (n >= 1e6)
          snprintf(count, sizeof(count), "%.1fM", n / 1e6);
        else
          snprintf(count, sizeof(count), "%.0f", n);
        char tmp[96];
        if (assmThread->getSearchTraced() > 0)
          snprintf(tmp, 96, "panex: tracing the path\n%s of %s moves",
                   withCommas(assmThread->getSearchTraced()).c_str(),
                   withCommas(assmThread->getSearchDepth()).c_str());
        else
          snprintf(tmp, 96, "panex: %s moves deep\n%s stackings found",
                   withCommas(assmThread->getSearchDepth()).c_str(), count);
        OutputActivity->value(tmp);
      } else if (stacking::isStacking(*puzzle)) {
        char tmp[64];
        snprintf(tmp, 64, "stack search\n%s stackings",
                 withCommas(assmThread->getSlideProgress()).c_str());
        OutputActivity->value(tmp);
      } else {
        /* with what each take-apart under way is doing */
        OutputActivity->value(progress.activity().c_str());
      }
      break;
    case solveThread_c::ACT_DISASSEMBLING:
      OutputActivity->value(progress.activity().c_str());
      break;
    case solveThread_c::ACT_PAUSING:
      OutputActivity->value("pause");
      break;
    case solveThread_c::ACT_FINISHED:
      OutputActivity->value("finished");
      break;
    case solveThread_c::ACT_WAIT_TO_STOP:
      OutputActivity->value("please wait");
      break;
    case solveThread_c::ACT_ERROR:
      OutputActivity->value("error");
      break;
    }

    if (&(assmThread->getProblem()) == puzzle->getProblem(prob)) {

      // for the actually solved problem we enable the stop button
      BtnStart->deactivate();
      if (BtnPrepare) BtnPrepare->deactivate();
      BtnCont->deactivate();
      BtnStop->activate();
      if (BtnAbort) BtnAbort->activate();

      // we can not edit solutions for a currently solved problem
      BtnSrtFind->deactivate();
      BtnSrtLevel->deactivate();
      BtnSrtMoves->deactivate();
      BtnSrtPieces->deactivate();
      BtnDelAll->deactivate();
      BtnDelBefore->deactivate();
      BtnDelAt->deactivate();
      BtnDelAfter->deactivate();
      BtnDelDisasm->deactivate();
      BtnDisasmDel->deactivate();
      BtnDisasmDelAll->deactivate();
      BtnDisasmAdd->deactivate();
      BtnDisasmAddAll->deactivate();
      BtnDisasmAddMissing->deactivate();

      sortMethod->deactivate();

    } else {

      // all other problems can do nothing
      BtnStart->deactivate();
      if (BtnPrepare) BtnPrepare->deactivate();
      BtnCont->deactivate();
      BtnStop->deactivate();
      if (BtnPrepare) BtnPrepare->deactivate();
      if (BtnAbort) BtnAbort->deactivate();

      sortMethod->deactivate();

    }

  } else {

    pieceEdit->activate();

    // no thread currently calculating

    // so we can not stop the thread
    BtnStop->deactivate();

    // the stop button might be pressed when the thread finished, this might happen
    // relatively often, so we clear the state of that button
    BtnStop->clear();

    if (BtnAbort) {
      if (abortable(prob)) BtnAbort->activate();
      else BtnAbort->deactivate();
    }

    if (prob < puzzle->getNumberOfProblems()) {

      problem_c * pr = puzzle->getProblem(prob);
      // a valid problem is selected

      switch(pr->getSolveState()) {
      case SS_UNSOLVED:
        OutputActivity->value("nothing");
        BtnCont->deactivate();
        break;
      case SS_SOLVED:
        OutputActivity->value("finished");
        BtnCont->deactivate();
        break;
      case SS_SOLVING:
        OutputActivity->value("pause");
        BtnCont->activate();
        break;
      case SS_UNKNOWN:
        OutputActivity->value("partial");
        BtnCont->deactivate();
        break;

      }

      // if we have a result and at least one piece, we can give it a try
      bool hasResult = stacking::isStacking(*puzzle) ? pr->rodSetValid() : pr->resultValid();
      /* A stacking the Puzzle tab's bar marks Invalid cannot be solved. */
      bool stackOk = true;
      /* Nor can a sliding puzzle whose Start/Goal tab says Invalid. */
      if (sliding::isSliding(*puzzle) && pr->resultValid() &&
          sliding::isStartGoalShape(getResultShape(*pr))) {
        const std::string why = sliding::startGoalError(*puzzle, pr->getResultId());
        stackOk = why.empty();
        BtnStart->copy_tooltip(stackOk ? " Start new solving process, removing old result "
                                       : (" Fix the start and goal on the Entities tab first: " + why + " ").c_str());
      }
      if (stacking::isStacking(*puzzle)) {
        std::string why = stacking::setupError(*pr);
        stackOk = why.empty();
        if (stackOk) {
          BtnStart->copy_tooltip(" Start new solving process, removing old result ");
          /* A saved Panex search can be carried on, whatever the problem's state. */
          BtnCont->copy_tooltip(" Continue started process ");
          if (stacking::isStacking(*puzzle)) {
            const std::string saved = panex::savedSearch(*pr);
            if (!saved.empty()) {
              BtnCont->activate();
              BtnCont->copy_tooltip((" Carry on " + saved + " ").c_str());
              OutputActivity->value("paused (saved)");
            }
          }
        } else {
          BtnStart->copy_tooltip((" Fix the puzzle on the Puzzle tab first: " + why + " ").c_str());
          BtnCont->deactivate();
        }
      }
      if ((pr->getNumberOfPieces() > 0) && hasResult && stackOk) {
        BtnStart->activate();
        if (BtnPrepare) BtnPrepare->activate();
      } else {
        BtnStart->deactivate();
        if (BtnPrepare) BtnPrepare->deactivate();
      }

      sortMethod->activate();

    } else {

      // no start possible, when no valid problem selected
      BtnStart->deactivate();
      if (BtnPrepare) BtnPrepare->deactivate();
      BtnCont->deactivate();
      if (BtnPrepare) BtnPrepare->deactivate();

      sortMethod->deactivate();
    }
  }
}


void mainWindow_c::update(void) {

  if (assmThread)
    lastSolveStats = assmThread->getStats();
  if (debugPanel && debugPanel->visible())
    debugPanel->showStats(lastSolveStats);

  if (assmThread) {

    // check if the thread has thrown an exception. if so, re-throw it
    if (assmThread->currentAction() == solveThread_c::ACT_ASSERT) {

      assertWindow_c * aw = new assertWindow_c("Because of an internal error the current puzzle\n"
                                               "can not be solved\n",
                                               &(assmThread->getAssertException()));

      aw->show();

      while (aw->visible())
        Fl::wait();

      delete aw;
      assmThread.reset();
      updateInterface();
      return;
    }

    /* Autosave a brick or sliding solve: pause it where it can be saved,
     * save, carry on. Stacking solves save their own search. Only a puzzle
     * with a file has a place for the recovery copy. */
    if (!autosaving && Autosave && Autosave->value() && !fname.empty() && !stacking::isStacking(*puzzle)) {
      long interval = 20 * 60;
      if (const char * e = getenv("BURRTOOLS_AUTOSAVE_SECONDS"))
        interval = std::max(1L, atol(e));
      if (std::chrono::steady_clock::now() - autosaveFrom >= std::chrono::seconds(interval)) {
        const unsigned int act = assmThread->currentAction();
        if (act == solveThread_c::ACT_ASSEMBLING || act == solveThread_c::ACT_DISASSEMBLING) {
          autosaving = true;
          assmThread->stopSoft();
        }
      }
    }

    // check, if the thread has stopped, if so then delete the object
    if ((assmThread->currentAction() == solveThread_c::ACT_PAUSING) ||
        (assmThread->currentAction() == solveThread_c::ACT_FINISHED)) {

      const bool finished = assmThread->currentAction() == solveThread_c::ACT_FINISHED;
      std::string solverNote = assmThread->getSolverNote();
      assmThread.reset();
      if (autosaving) {
        autosaving = false;
        if (!finished) {
          /* Paused only to save: save, and carry straight on. */
          if (!writeAutosave())
            fl_message("Could not autosave the solve to %s.", autosavePath().c_str());
          cb_BtnCont(false, (int)autosaveProblem);
          updateInterface();
          return;
        }
      }
      if (finished)
        removeAutosave();
      if (!solverNote.empty())
        fl_message("%s", solverNote.c_str());

    } else if (assmThread->currentAction() == solveThread_c::ACT_ERROR) {

      unsigned int selectShape = 0xFFFFFFFF;

      switch(assmThread->getErrorState()) {
      case assembler_c::ERR_TOO_MANY_UNITS:
        fl_message("Pieces contain %i units too many", assmThread->getErrorParam());
        break;
      case assembler_c::ERR_TOO_FEW_UNITS:
        fl_message("Pieces contain %i units less than required\n"
                   "See user guide sections\n"
                   "1.3.2.1 'Voxel States'\n"
                   "3.4.2 'Basic Drawing Tools' and\n"
                   "3.8 'Miscellaneous Editing Tools'", assmThread->getErrorParam());
        break;
      case assembler_c::ERR_CAN_NOT_PLACE:
        fl_message("Piece %i can be placed nowhere within the result", assmThread->getErrorParam()+1);
        selectShape = assmThread->getErrorParam();
        break;
      case assembler_c::ERR_CAN_NOT_RESTORE_VERSION:
        fl_message("Impossible to restore the saved state because the internal format changed.\n"
                   "You either have to start from the beginning or finish with the old version of BurrTools, sorry");
        break;
      case assembler_c::ERR_CAN_NOT_RESTORE_SYNTAX:
        fl_message("Impossible to restore the saved state because something with the data is wrong.\n"
                   "You have to start from the beginning, sorry");
        break;
      case assembler_c::ERR_PUZZLE_UNHANDABLE:
        fl_message("Something went wrong the program can not solve your puzzle definitions.\n"
                   "You should send the puzzle file to the programmer!");
        break;
      default:
        break;
      }

      if (selectShape < puzzle->getNumberOfShapes()) {
        TaskSelectionTab->value(TabPieces);
        PcSel->setSelection(selectShape);
        activateShape(PcSel->getSelection());
        updateInterface();
        StatPieceInfo(PcSel->getSelection());
      }

      assmThread.reset();
    }

    // update the window, either when the thread stopped and so the buttons need to
    // be updated, or then the thread works for the currently selected problem
    if (!assmThread || &(assmThread->getProblem()) == puzzle->getProblem(solutionProblem->getSelection()))
      updateInterface();
  }
  platform::setDocumentEdited(this, changed);
}

void mainWindow_c::Toggle3DView(void)
{
  // select the pieces tab, as exchanging widgets while they are invisible
  // didn't work. Save the current tab before that
  // this is required when changing the tab and we need to get the 3D view back
  // in the large window
  TaskSelectionTab->when(0);
  Fl_Widget *v = TaskSelectionTab->value();
  if (v != TabPieces) TaskSelectionTab->value(TabPieces);

  // exchange widget positions
  Fl_Group * tmp = pieceEdit->parent();
  View3D->parent()->add(pieceEdit);
  tmp->add(View3D);

  // exchange sizes
  {
    int x = pieceEdit->x();
    int y = pieceEdit->y();
    int w = pieceEdit->w();
    int h = pieceEdit->h();
    pieceEdit->resize(View3D->x(), View3D->y(), View3D->w(), View3D->h());
    View3D->resize(x, y, w, h);
  }

  // exchange grid positions
  {
    unsigned int x1, y1, w1, h1, x2, y2, w2, h2;
    pieceEdit->getGridValues(&x1, &y1, &w1, &h1);
    View3D->getGridValues   (&x2, &y2, &w2, &h2);

    pieceEdit->setGridValues( x2,  y2,  w2,  h2);
    View3D->setGridValues   ( x1,  y1,  w1,  h1);
  }

  is3DViewBig = !is3DViewBig;

  if (is3DViewBig)
    pieceEdit->parent()->resizable(pieceEdit);
  else
    View3D->parent()->resizable(View3D);

  // restore the old selected tab
  if (v != TabPieces) TaskSelectionTab->value(v);
  TaskSelectionTab->when(FL_WHEN_CHANGED);
}

void mainWindow_c::Big3DView(void) {
  if (!is3DViewBig) Toggle3DView();
  View3D->show();
  redraw();
}

void mainWindow_c::Small3DView(void) {
  if (is3DViewBig) Toggle3DView();
  pieceEdit->show();
  redraw();
}

int mainWindow_c::handle(int event) {

  if (event == FL_SHORTCUT && notesInput && Fl::focus() == notesInput) {
    unsigned mods = Fl::event_state();
    int k = Fl::event_key();
    if ((mods & FL_COMMAND) && (k == 'z' || k == 'Z' || k == 'y' || k == 'Y'))
      return notesInput->handle(event);
  }

  if (Fl_Double_Window::handle(event))
    return 1;

  if (event == FL_SHORTCUT) {
    unsigned mods = Fl::event_state();
    if ((mods & FL_COMMAND) && !(mods & FL_ALT) && !(mods & FL_SHIFT)) {
      int k = Fl::event_key();
      if (k == 'y' || k == 'Y') {
        cb_Redo();
        return 1;
      }
    }
  }

  switch(event) {
  case FL_SHORTCUT:
    if (Fl::event_length()) {
      switch (Fl::event_text()[0]) {
      case '+':
        if (TaskSelectionTab->value() == TabPieces) {
          pieceEdit->setZ(pieceEdit->getZ()+1);
          return 1;
        }
        break;
      case '-':
        if (TaskSelectionTab->value() == TabPieces) {
          if (pieceEdit->getZ() > 0)
            pieceEdit->setZ(pieceEdit->getZ()-1);
          return 1;
        }
        break;
      }
    }
    switch(Fl::event_key()) {
      case FL_F + 2:
        if (!platform::usesSystemMenuBar()) break;
        cb_Save_stub(this, this);
        return 1;
      case FL_F + 3:
        if (!platform::usesSystemMenuBar()) break;
        cb_Load_stub(this, this);
        return 1;
      case FL_F + 4:
        if (!platform::usesSystemMenuBar()) break;
        cb_Toggle3D_stub(this, this);
        return 1;
      case FL_F + 5:
        if (TaskSelectionTab->value() == TabPieces) {
          editChoice->select(0);
          return 1;
        }
        break;
      case FL_F + 6:
        if (TaskSelectionTab->value() == TabPieces) {
          editChoice->select(1);
          return 1;
        }
        break;
      case FL_F + 7:
        if (TaskSelectionTab->value() == TabPieces) {
          editChoice->select(2);
          return 1;
        }
        break;
      case FL_F + 8:
        if (TaskSelectionTab->value() == TabPieces) {
          editChoice->select(3);
          return 1;
        }
        break;
    }
  }

  return 0;
}