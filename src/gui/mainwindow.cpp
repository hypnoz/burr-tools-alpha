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

static void cb_AddColor_stub(Fl_Widget* /*o*/, void* v) { ((mainWindow_c*)v)->cb_AddColor(); }
void mainWindow_c::cb_AddColor(void) {

  unsigned char r, g, b;

  if (colorSelector->getSelection() == 0)
    r = g = b = 128;
  else
    puzzle->getColor(colorSelector->getSelection()-1, &r, &g, &b);

  if (fl_color_chooser("New colour", r, g, b)) {
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

static void cb_RemoveColor_stub(Fl_Widget* /*o*/, void* v) { ((mainWindow_c*)v)->cb_RemoveColor(); }
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

static void cb_ChangeColor_stub(Fl_Widget* /*o*/, void* v) { ((mainWindow_c*)v)->cb_ChangeColor(); }
void mainWindow_c::cb_ChangeColor(void) {

  if (colorSelector->getSelection() == 0)
    fl_message("Can not edit the Neutral colour");
  else {
    unsigned char r, g, b;
    puzzle->getColor(colorSelector->getSelection()-1, &r, &g, &b);
    if (fl_color_chooser("Change colour", r, g, b)) {
      puzzle->changeColor(colorSelector->getSelection()-1, r, g, b);
      changed = true;
      View3D->getView()->showColors(puzzle, StatusLine->getColorMode());
      updateInterface();
    }
  }
}

static void cb_NewShape_stub(Fl_Widget* /*o*/, void* v) { ((mainWindow_c*)v)->cb_NewShape(); }
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

static void cb_DeleteShape_stub(Fl_Widget* /*o*/, void* v) { ((mainWindow_c*)v)->cb_DeleteShape(); }
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

static void cb_CopyShape_stub(Fl_Widget* /*o*/, void* v) { ((mainWindow_c*)v)->cb_CopyShape(); }
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

static void cb_NameShape_stub(Fl_Widget* /*o*/, void* v) { ((mainWindow_c*)v)->cb_NameShape(); }
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

static void cb_WeightInc_stub(Fl_Widget* /*o*/, void* v) { ((mainWindow_c*)v)->cb_WeightChange(1); }
static void cb_WeightDec_stub(Fl_Widget* /*o*/, void* v) { ((mainWindow_c*)v)->cb_WeightChange(-1); }
void mainWindow_c::cb_WeightChange(int by) {

  if (PcSel->getSelection() < puzzle->getNumberOfShapes()) {

    voxel_c * v = puzzle->getShape(PcSel->getSelection());
    v->setWeight(v->getWeight() + by);
    recordShapeAction(shapeHistory_c::AK_STRUCTURAL);
    updateInterface();
  }
}

void mainWindow_c::applySlidingGridMode(void) {
  if (!pieceEdit)
    return;

  /* The voxel pen stays whatever the user picked. Start/goal only adds an
   * S# stamp on top of that pen; it must not replace left-click with erase. */
  if (editChoice) {
    switch (editChoice->getSelected()) {
      case 1: pieceEdit->editChoice(gridEditor_c::TSK_VAR); break;
      case 2: pieceEdit->editChoice(gridEditor_c::TSK_RESET); break;
      case 3: pieceEdit->editChoice(gridEditor_c::TSK_COLOR); break;
      default: pieceEdit->editChoice(gridEditor_c::TSK_SET); break;
    }
  }

  bool stamp = false;
  if (puzzle && sliding::isSliding(*puzzle) &&
      PcSel && PcSel->getSelection() < puzzle->getNumberOfShapes() &&
      sliding::isStartGoalShape(puzzle->getShape(PcSel->getSelection()))) {
    ToolTab_0 * tab = dynamic_cast<ToolTab_0*>(pieceTools->getToolTab());
    if (tab && tab->labelEditActive()) {
      slidingEditMode = tab->startGoalMode();
      pieceEdit->setSlidingLabels(slidingEditMode == 0 ? 1 : 2);
      unsigned int piece = tab->selectedPiece();
      pieceEdit->setSlidingPiece(piece == (unsigned int)-1 ? 0 : piece + 1);
      stamp = true;
    }
  }
  if (!stamp) {
    pieceEdit->setSlidingLabels(0);
    pieceEdit->setSlidingPiece(0);
  }
  if (View3D && View3D->getView())
    View3D->getView()->setMarkerShape(stamp ? slidingEditMode : -1);
}

static void cb_NewStartGoal_stub(Fl_Widget* /*o*/, void* v) { ((mainWindow_c*)v)->cb_NewStartGoal(); }
void mainWindow_c::cb_NewStartGoal(void) {
  if (!puzzle || !sliding::isSliding(*puzzle))
    return;
  unsigned int sx = 6, sy = 6;
  if (PcSel->getSelection() < puzzle->getNumberOfShapes()) {
    const voxel_c * cur = puzzle->getShape(PcSel->getSelection());
    sx = cur->getX();
    sy = cur->getY();
  }
  unsigned int id = sliding::addStartGoalShape(*puzzle, sx, sy);
  sliding::syncSlidingProblems(*puzzle);
  changed = true;
  activateShape(id);
  applySlidingGridMode();
  updateInterface();
  StatPieceInfo(id);
  recordShapeAction(shapeHistory_c::AK_STRUCTURAL);
}

static void cb_DelStartGoal_stub(Fl_Widget* /*o*/, void* v) { ((mainWindow_c*)v)->cb_DelStartGoal(); }
void mainWindow_c::cb_DelStartGoal(void) {
  unsigned int current = PcSel->getSelection();
  if (current >= puzzle->getNumberOfShapes())
    return;
  if (!sliding::isStartGoalShape(puzzle->getShape(current)))
    return;
  sliding::noteShapeRemoved(*puzzle, current);
  puzzle->removeShape(current);
  sliding::syncSlidingProblems(*puzzle);
  if (puzzle->getNumberOfShapes() == 0)
    current = (unsigned int)-1;
  else
    while (current >= puzzle->getNumberOfShapes())
      current--;
  changed = true;
  activateShape(current);
  applySlidingGridMode();
  updateInterface();
  StatPieceInfo(current);
  recordShapeAction(shapeHistory_c::AK_STRUCTURAL);
}

static void cb_NewRod_stub(Fl_Widget* /*o*/, void* v) { ((mainWindow_c*)v)->cb_NewRod(); }
static void cb_DeleteRod_stub(Fl_Widget* /*o*/, void* v) { ((mainWindow_c*)v)->cb_DeleteRod(); }
static void cb_CopyRod_stub(Fl_Widget* /*o*/, void* v) { ((mainWindow_c*)v)->cb_CopyRod(); }
static void cb_NameRod_stub(Fl_Widget* /*o*/, void* v) { ((mainWindow_c*)v)->cb_NameRod(); }
static void cb_RodLeft_stub(Fl_Widget* /*o*/, void* v) { ((mainWindow_c*)v)->cb_RodExchange(-1); }
static void cb_RodRight_stub(Fl_Widget* /*o*/, void* v) { ((mainWindow_c*)v)->cb_RodExchange(1); }
static void cb_RodUndo_stub(Fl_Widget* /*o*/, void* v) { ((mainWindow_c*)v)->cb_RodUndo(); }
static void cb_RodRedo_stub(Fl_Widget* /*o*/, void* v) { ((mainWindow_c*)v)->cb_RodRedo(); }
static void cb_RodSel_stub(Fl_Widget* /*o*/, void* v) { ((mainWindow_c*)v)->cb_RodSel(); }
static void cb_RodField_stub(Fl_Widget* /*o*/, void* v) { ((mainWindow_c*)v)->cb_RodField(); }
static void cb_DiskList_stub(Fl_Widget* /*o*/, void* v) { ((mainWindow_c*)v)->cb_DiskList(); }
static void cb_StackMode_stub(Fl_Widget* o, void* v) { ((mainWindow_c*)v)->cb_StackMode(o); }
static void cb_StackRod_stub(Fl_Widget* /*o*/, void* v) { ((mainWindow_c*)v)->cb_StackRod(); }
static void cb_StackValidRelayout_stub(Fl_Widget* /*o*/, void* v) { ((mainWindow_c*)v)->relayoutProblemTab(); }
static void cb_StackListSel_stub(Fl_Widget* /*o*/, void* v) { ((mainWindow_c*)v)->cb_StackListSel(); }
static void cb_ProblemRod_stub(Fl_Widget* /*o*/, void* v) { ((mainWindow_c*)v)->cb_ProblemRod(); }

mainWindow_c::rodSnap_c mainWindow_c::captureRodSnap(void) const {
  rodSnap_c snap;
  if (!puzzle)
    return snap;
  snap.sets.reserve(puzzle->rodSetCount());
  for (unsigned int i = 0; i < puzzle->rodSetCount(); i++)
    snap.sets.push_back(puzzle->getRodSet(i));
  snap.problems.resize(puzzle->getNumberOfProblems());
  for (unsigned int i = 0; i < puzzle->getNumberOfProblems(); i++) {
    const problem_c * pr = puzzle->getProblem(i);
    snap.problems[i].id = pr->getRodSetId();
    snap.problems[i].start = pr->startStacks();
    snap.problems[i].goal = pr->goalStacks();
  }
  snap.sel = rodSel ? rodSel->getSelection() : 0;
  return snap;
}

void mainWindow_c::resetRodHistory(void) {
  rodPast.clear();
  rodFuture.clear();
  if (puzzle && stacking::isStacking(*puzzle))
    rodPast.push_back(captureRodSnap());
}

void mainWindow_c::pushRodHistory(void) {
  if (!puzzle || !stacking::isStacking(*puzzle))
    return;
  rodFuture.clear();
  rodPast.push_back(captureRodSnap());
  if (rodPast.size() > 40)
    rodPast.erase(rodPast.begin());
}

void mainWindow_c::restoreRodSnap(const rodSnap_c & snap) {
  if (!puzzle)
    return;
  rodFieldGuard = true;
  while (puzzle->rodSetCount() > snap.sets.size())
    puzzle->removeRodSet(puzzle->rodSetCount() - 1);
  while (puzzle->rodSetCount() < snap.sets.size())
    puzzle->addRodSet();
  for (unsigned int i = 0; i < snap.sets.size(); i++)
    puzzle->getRodSet(i) = snap.sets[i];
  unsigned int n = puzzle->getNumberOfProblems();
  if (n > snap.problems.size())
    n = (unsigned int)snap.problems.size();
  for (unsigned int i = 0; i < n; i++) {
    problem_c * pr = puzzle->getProblem(i);
    pr->setRodSetId(snap.problems[i].id);
    pr->editStart() = snap.problems[i].start;
    pr->editGoal() = snap.problems[i].goal;
  }
  rodFieldGuard = false;
  if (rodSel && puzzle->rodSetCount()) {
    unsigned int sel = snap.sel;
    if (sel >= puzzle->rodSetCount())
      sel = puzzle->rodSetCount() - 1;
    if (rodSel->getSelection() != sel)
      rodSel->setSelection(sel);
  }
  loadRodFields();
  changed = true;
  if (rodSel) rodSel->redraw();
  if (rodAssignSel) rodAssignSel->redraw();
  updateInterface();
  if (TaskSelectionTab && TaskSelectionTab->value() == TabPieces)
    showSelectedRods();
  else if (TabProblems && TaskSelectionTab && TaskSelectionTab->value() == TabProblems &&
           problemSelector && problemSelector->getSelection() < puzzle->getNumberOfProblems())
    activateProblem(problemSelector->getSelection());
}

void mainWindow_c::cb_RodUndo(void) {
  if (rodPast.size() < 2)
    return;
  rodFuture.push_back(rodPast.back());
  rodPast.pop_back();
  restoreRodSnap(rodPast.back());
}

void mainWindow_c::cb_RodRedo(void) {
  if (rodFuture.empty())
    return;
  rodPast.push_back(rodFuture.back());
  rodFuture.pop_back();
  restoreRodSnap(rodPast.back());
}

void mainWindow_c::loadRodFields(void) {
  if (!rodCountInput || !puzzle)
    return;
  rodFieldGuard = true;
  unsigned int sel = rodSel ? rodSel->getSelection() : (unsigned int)-1;
  bool ok = sel < puzzle->rodSetCount();
  if (ok) {
    const stacking::rodSet_c & r = puzzle->getRodSet(sel);
    rodCountInput->value(r.rodCount);
    rodGrow->value(r.growHeight ? 1 : 0);
    rodFixed->value(r.growHeight ? 0 : 1);
    rodHeightInput->value(r.definedHeight);
    if (r.growHeight) rodHeightInput->deactivate();
    else rodHeightInput->activate();
    rodSizeMatters->value(r.sizeMatters ? 1 : 0);
    /* Older files used "can move over" to cancel a distance limit. */
    rodDistance->value((r.distanceMatters && !r.canMoveOver) ? 1 : 0);
    rodPanex->value(r.panexColumns ? 1 : 0);
    rodPocket->value(r.pocketColumn ? 1 : 0);
    rodPocketHeight->value(r.pocketHeight);
    syncPanexFields();
    if (r.growHeight) rodGrow->setonly();
    else rodFixed->setonly();
  }
  rodFieldGuard = false;
}

static void easeRodZoom(LView3dGroup * view) {
  if (!view)
    return;
  /* The piece editor's default zoom frames one disc. A board of rods
   * needs to sit further back so all of it is in view, however large the
   * discs. Refit while the zoom is the default or our last fit, and leave
   * a zoom the user has moved. */
  static double lastFit = -1;
  const double zoom = view->getZoom();
  if (std::fabs(zoom - LView3dGroup::defaultZoom) < 0.15 || std::fabs(zoom - lastFit) < 0.005) {
    lastFit = view->getView()->fitZoom();
    view->setZoom(lastFit);
  }
}

void mainWindow_c::showSelectedRods(void) {
  if (!View3D || !puzzle || !rodSel || rodSel->getSelection() >= puzzle->rodSetCount()) {
    if (View3D) View3D->getView()->showNothing();
    return;
  }
  View3D->getView()->showRodSet(puzzle, rodSel->getSelection());
  easeRodZoom(View3D);
}

void mainWindow_c::selectProblemRod(unsigned int prob) {
  if (!rodAssignSel || !puzzle || !stacking::isStacking(*puzzle))
    return;
  if (prob >= puzzle->getNumberOfProblems())
    return;
  const problem_c * pr = puzzle->getProblem(prob);
  if (!pr->rodSetValid())
    return;
  if (rodAssignSel->getSelection() != pr->getRodSetId())
    rodAssignSel->setSelection(pr->getRodSetId());
}

void mainWindow_c::cb_NewRod(void) {
  if (!puzzle || !stacking::isStacking(*puzzle))
    return;
  unsigned int id = puzzle->addRodSet();
  changed = true;
  if (rodSel) rodSel->setSelection(id);
  pushRodHistory();
  loadRodFields();
  updateInterface();
  showSelectedRods();
}

void mainWindow_c::cb_DeleteRod(void) {
  if (!puzzle || !rodSel)
    return;
  unsigned int sel = rodSel->getSelection();
  if (sel >= puzzle->rodSetCount())
    return;
  if (puzzle->rodSetCount() < 2) {
    fl_message("Keep at least one rod set");
    return;
  }
  puzzle->removeRodSet(sel);
  if (sel >= puzzle->rodSetCount())
    sel = puzzle->rodSetCount() - 1;
  changed = true;
  rodSel->setSelection(sel);
  pushRodHistory();
  loadRodFields();
  updateInterface();
  showSelectedRods();
}

void mainWindow_c::cb_CopyRod(void) {
  if (!puzzle || !rodSel)
    return;
  unsigned int sel = rodSel->getSelection();
  if (sel >= puzzle->rodSetCount())
    return;
  unsigned int id = puzzle->addRodSet(puzzle->getRodSet(sel));
  changed = true;
  rodSel->setSelection(id);
  pushRodHistory();
  loadRodFields();
  updateInterface();
  showSelectedRods();
}

void mainWindow_c::cb_NameRod(void) {
  if (!puzzle || !rodSel)
    return;
  unsigned int sel = rodSel->getSelection();
  if (sel >= puzzle->rodSetCount())
    return;
  const char * name = fl_input("Enter name for the rod set", puzzle->getRodSet(sel).name.c_str());
  if (!name)
    return;
  puzzle->getRodSet(sel).name = name;
  changed = true;
  pushRodHistory();
  rodSel->redraw();
  if (rodAssignSel) rodAssignSel->redraw();
  updateInterface();
}

void mainWindow_c::cb_RodExchange(int with) {
  if (!puzzle || !rodSel)
    return;
  unsigned int sel = rodSel->getSelection();
  unsigned int other = sel + with;
  if (sel >= puzzle->rodSetCount() || other >= puzzle->rodSetCount())
    return;
  puzzle->exchangeRodSets(sel, other);
  changed = true;
  rodSel->setSelection(other);
  pushRodHistory();
  updateInterface();
  showSelectedRods();
}

void mainWindow_c::cb_RodSel(void) {
  loadRodFields();
  if (TaskSelectionTab && TaskSelectionTab->value() == TabPieces)
    showSelectedRods();
  updateInterface();
}

/* Panex columns replace the size rule, and the pocket needs them. */
void mainWindow_c::syncPanexFields(void) {
  if (!rodPanex)
    return;
  const bool panex = rodPanex->value() != 0;
  if (panex) {
    rodSizeMatters->deactivate();
    rodPocket->activate();
  } else {
    rodSizeMatters->activate();
    rodPocket->value(0);
    rodPocket->deactivate();
  }
  if (panex && rodPocket->value() != 0)
    rodPocketHeight->activate();
  else
    rodPocketHeight->deactivate();
}

void mainWindow_c::cb_RodField(void) {
  if (rodFieldGuard || !puzzle || !rodSel || !rodCountInput)
    return;
  unsigned int sel = rodSel->getSelection();
  if (sel >= puzzle->rodSetCount())
    return;
  stacking::rodSet_c & r = puzzle->getRodSet(sel);
  unsigned int count = (unsigned int)rodCountInput->value();
  if (count < 1) count = 1;
  if (count > (unsigned int)STACK_ROD_BUTTONS) count = (unsigned int)STACK_ROD_BUTTONS;
  r.rodCount = count;
  r.growHeight = rodGrow->value() != 0;
  unsigned int h = (unsigned int)rodHeightInput->value();
  if (h < 1) h = 1;
  if (h > 64) h = 64;
  r.definedHeight = h;
  r.sizeMatters = rodSizeMatters->value() != 0;
  r.distanceMatters = rodDistance->value() != 0;
  r.canMoveOver = false;
  r.panexColumns = rodPanex->value() != 0;
  r.pocketColumn = r.panexColumns && rodPocket->value() != 0;
  unsigned int ph = (unsigned int)rodPocketHeight->value();
  if (ph < 1) ph = 1;
  if (ph > 64) ph = 64;
  r.pocketHeight = ph;
  if (r.growHeight) rodHeightInput->deactivate();
  else rodHeightInput->activate();
  syncPanexFields();
  /* Stacks saved against the old rules can be illegal after these edits.
   * They are kept: the Puzzle tab's validity bar reports what is wrong. */
  for (unsigned int p = 0; p < puzzle->getNumberOfProblems(); p++) {
    problem_c * pr = puzzle->getProblem(p);
    if (pr->rodSetValid() && pr->getRodSetId() == sel) {
      stacking::syncMaps(*pr);
      /* A solution found under the old rules may not hold under these. */
      pr->removeAllSolutions();
    }
  }
  changed = true;
  pushRodHistory();
  if (rodSel) rodSel->redraw();
  updateInterface();
  refreshStackSlider();
  if (TaskSelectionTab && TaskSelectionTab->value() == TabPieces)
    showSelectedRods();
  else if (TaskSelectionTab && TaskSelectionTab->value() == TabProblems)
    refreshStackList();
}

void mainWindow_c::cb_DiskList(void) {
  if (!diskList || !puzzle)
    return;
  unsigned int sel = diskList->getSelection();
  if (sel >= puzzle->getNumberOfShapes())
    return;
  if (diskList->getReason() == DiskSelector::RS_SIZE) {
    recordShapeAction(shapeHistory_c::AK_STRUCTURAL);
    changed = true;
  }
  activateShape(sel);
  StatPieceInfo(sel);
  updateInterface();
}

/* Full-width coloured strip that states whether the stacking on the Puzzle
 * tab obeys its rod set. The reason wraps, so the strip asks for as many
 * lines as the text needs at its current width. When a resize changes that
 * line count it fires its callback, so the owner can lay the tab out again. */
class StackValidBar_c : public Fl_Box, public layoutable_c {
  std::string text;
  int askedHeight;
  static void cb_relayout(void * v) {
    ((StackValidBar_c *)v)->do_callback();
  }
  int heightFor(int W) const {
    fl_font(labelfont(), labelsize());
    int tw = W - 12;
    if (tw < 40)
      tw = 40;
    int th = 0;
    fl_measure(text.c_str(), tw, th, 0);
    return th + 8;
  }
public:
  StackValidBar_c(int gx, int gy, int gw, int gh)
    : Fl_Box(0, 0, 100, 20), layoutable_c(gx, gy, gw, gh), askedHeight(0) {
    box(FL_FLAT_BOX);
    labelcolor(FL_WHITE);
    labelfont(FL_HELVETICA_BOLD);
    align(FL_ALIGN_INSIDE | FL_ALIGN_WRAP | FL_ALIGN_LEFT);
    setState(true, "");
  }
  ~StackValidBar_c(void) { Fl::remove_timeout(cb_relayout, this); }
  /* True when the text changed. */
  bool setState(bool valid, const std::string & reason) {
    std::string t = valid ? "Valid" : "Invalid";
    if (!reason.empty())
      t += " - " + reason;
    Fl_Color c = valid ? fl_rgb_color(46, 139, 87) : fl_rgb_color(192, 40, 40);
    if (t == text && c == color())
      return false;
    text = t;
    label(text.c_str());
    color(c);
    redraw();
    return true;
  }
  virtual void getMinSize(int *width, int *height) const {
    *width = 40;
    *height = heightFor(w() > 40 ? w() : 200);
  }
  virtual void draw(void) {
    draw_box();
    draw_label(x() + 6, y(), w() - 12, h(), align());
  }
  virtual void resize(int X, int Y, int W, int H) {
    Fl_Box::resize(X, Y, W, H);
    int need = heightFor(W);
    if (need != H && need != askedHeight) {
      askedHeight = need;
      Fl::remove_timeout(cb_relayout, this);
      Fl::add_timeout(0, cb_relayout, this);
    } else if (need == H) {
      askedHeight = need;
    }
  }
};

/* Tick marks beside the brick editor's Z slider, turned to run under a
 * horizontal slider. LineSpacer's horizontal branch draws with its axes
 * swapped, so this is a corrected copy of the vertical case. */
class RodTickBar_c : public Fl_Widget {
  int lines;
public:
  RodTickBar_c(int x, int y, int w, int h) : Fl_Widget(x, y, w, h), lines(3) {
    color(FL_BACKGROUND_COLOR);
  }
  void setLines(int n) {
    if (n < 1)
      n = 1;
    lines = n;
    redraw();
  }
  void draw(void) {
    fl_color(color());
    fl_rectf(x(), y(), w(), h());
    if (lines <= 1)
      return;
    fl_color(FL_BLACK);
    int gap = 4;
    int span = w() - 2 * gap - 1;
    if (span < 1)
      span = 1;
    /* Same tick width as the Entities Z slider. */
    int thick = span / (lines - 1) / 2;
    if (thick > 3)
      thick = 3;
    if (thick < 1)
      thick = 1;
    for (int i = 0; i < lines; i++) {
      int xpos = x() + gap + span * i / (lines - 1);
      fl_rectf(xpos - thick / 2, y(), thick, h());
    }
  }
};

/* The Entities Z control is an Fl_Slider, 15px on its short side, trough
 * color 237, with a 5px tick strip beside it. This is that slider with
 * FL_HOR_SLIDER and the ticks underneath, shortened for a "Rod: N" label. */
class RodIndexBar_c : public Fl_Group, public layoutable_c {
  Fl_Slider * slider;
  RodTickBar_c * ticks;
  Fl_Box * caption;
  int rods;
  int labelW;
public:
  /* With a pocket column the first place on the slider is the pocket,
   * which stands left of rod 1 on the board. */
  bool pocketFirst = false;
private:
  void syncCaption(void) {
    char buf[16];
    if (pocketFirst && value() == 0)
      snprintf(buf, sizeof(buf), "Rod: P");
    else
      snprintf(buf, sizeof(buf), "Rod: %d", value() + (pocketFirst ? 0 : 1));
    caption->copy_label(buf);
  }
  static void cb_slider(Fl_Widget *, void * v) {
    ((RodIndexBar_c *)v)->syncCaption();
    ((RodIndexBar_c *)v)->do_callback();
  }
public:
  RodIndexBar_c(int gx, int gy, int gw, int gh)
    : Fl_Group(0, 0, 120, 20), layoutable_c(gx, gy, gw, gh), rods(3) {
    box(FL_NO_BOX);
    fl_font(FL_HELVETICA, FL_NORMAL_SIZE);
    int tw = 0, th = 0;
    fl_measure("Rod: 16", tw, th, 0);
    labelW = tw + 8;
    caption = new Fl_Box(0, 0, labelW, 15);
    caption->box(FL_NO_BOX);
    caption->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
    caption->copy_label("Rod: 1");
    slider = new Fl_Slider(labelW, 0, 40, 15);
    slider->type(FL_HOR_SLIDER);
    slider->color((Fl_Color)237);
    slider->selection_color(FL_WHITE);
    slider->step(1);
    slider->bounds(0, 2);
    slider->value(0);
    slider->clear_visible_focus();
    slider->callback(cb_slider, this);
    squareKnob();
    ticks = new RodTickBar_c(labelW, 15, 40, 5);
    end();
    resizable(nullptr);
    stretchVCenter();
    setMinimumSize(labelW + 40, 20);
  }
  /* Knob length is a fraction of the track. Match the slider's height so
   * the button stays square, as on the Entities Z slider. */
  void squareKnob(void) {
    if (slider->w() < 1)
      return;
    double frac = (double)slider->h() / (double)slider->w();
    if (frac > 1)
      frac = 1;
    slider->slider_size(frac);
  }
  int value(void) const { return (int)(slider->value() + 0.5); }
  void value(int v) {
    int n = rods < 1 ? 1 : rods;
    if (v < 0)
      v = 0;
    if (v >= n)
      v = n - 1;
    slider->value(v);
    syncCaption();
  }
  void setCount(int n) {
    if (n < 1)
      n = 1;
    int keep = value();
    rods = n;
    slider->bounds(0, n - 1);
    if (keep >= n)
      keep = n - 1;
    if (keep < 0)
      keep = 0;
    slider->value(keep);
    ticks->setLines(n);
    syncCaption();
  }
  virtual void getMinSize(int *width, int *height) const {
    *width = labelW + 40;
    *height = 20;
  }
  virtual void resize(int X, int Y, int W, int H) {
    Fl_Widget::resize(X, Y, W, H);
    int sh = 15;
    if (sh > H)
      sh = H;
    int lw = labelW;
    int gap = 4;
    if (lw + gap > W - 20)
      lw = W - 20 - gap;
    if (lw < 1)
      lw = 1;
    caption->resize(X, Y, lw, sh);
    int sx = X + lw + gap;
    int sw = W - (lw + gap);
    if (sw < 1)
      sw = 1;
    slider->resize(sx, Y, sw, sh);
    squareKnob();
    int th = H - sh;
    if (th < 0)
      th = 0;
    ticks->resize(sx, Y + sh, sw, th);
  }
};

unsigned int mainWindow_c::selectedStackRod(void) const {
  if (!stackRodBar)
    return 0;
  int v = stackRodBar->value();
  if (v < 0)
    v = 0;
  if (!stackRodBar->pocketFirst)
    return (unsigned int)v;
  /* Slider place 0 is the pocket, the last rod in a stack map. */
  unsigned int id = rodAssignSel ? rodAssignSel->getSelection() : 0;
  if (!puzzle || id >= puzzle->rodSetCount())
    return 0;
  return v == 0 ? puzzle->getRodSet(id).rodCount : (unsigned int)(v - 1);
}

bool mainWindow_c::stackingBoard(problem_c * pr) {
  return pr && pr->rodSetValid();
}

std::string mainWindow_c::placeOneDisk(problem_c * pr, unsigned int shape) {
  unsigned int before = pr->getShapeMaximum(shape);
  bool added = true;
  const stacking::stackMap_c & map = stackingEditGoal ? pr->goalStacks() : pr->startStacks();
  for (unsigned int i = 0; i < before; i++) {
    bool used = false;
    for (const auto & rod : map.rods)
      for (const stacking::diskRef_c & d : rod)
        if (d.shapeId == shape && d.instance == i)
          used = true;
    if (!used) {
      added = false;
      break;
    }
  }
  if (added) {
    pr->setShapeMaximum(shape, before + 1);
    if (pr->getShapeMinimum(shape) == before)
      pr->setShapeMinimum(shape, before + 1);
  }
  /* The rules are not enforced here: the validity bar reports a stacking
   * that breaks them, so the user can build one step by step. */
  std::string err = stacking::placeDisk(*pr, stackingEditGoal, shape, selectedStackRod(), false);
  if (!err.empty() && added) {
    pr->setShapeMaximum(shape, before);
    if (pr->getShapeMinimum(shape) > before)
      pr->setShapeMinimum(shape, before);
  }
  return err;
}

void mainWindow_c::refreshStackSlider(void) {
  if (!stackRodBar || !puzzle)
    return;
  unsigned int n = 1;
  bool pocket = false;
  unsigned int id = rodAssignSel ? rodAssignSel->getSelection() : 0;
  if (id < puzzle->rodSetCount()) {
    n = stacking::totalRods(puzzle->getRodSet(id));
    pocket = n > puzzle->getRodSet(id).rodCount;
  }
  stackRodBar->pocketFirst = pocket;
  if (n < 1)
    n = 1;
  int keep = stackRodBar->value();
  stackRodBar->setCount((int)n);
  if (keep >= (int)n)
    keep = (int)n - 1;
  if (keep < 0)
    keep = 0;
  stackRodBar->value(keep);
}

void mainWindow_c::refreshStackList(void) {
  if (!rodStackList)
    return;
  std::vector<std::string> labels;
  std::vector<unsigned int> shapes;
  if (puzzle && stacking::isStacking(*puzzle) && problemSelector &&
      problemSelector->getSelection() < puzzle->getNumberOfProblems()) {
    problem_c * pr = puzzle->getProblem(problemSelector->getSelection());
    const stacking::stackMap_c & map = stackingEditGoal ? pr->goalStacks() : pr->startStacks();
    unsigned int rod = selectedStackRod();
    if (pr->rodSetValid() && rod < map.rods.size()) {
      const std::vector<stacking::diskRef_c> & stack = map.rods[rod];
      for (int i = (int)stack.size() - 1; i >= 0; --i) {
        const stacking::diskRef_c & d = stack[(unsigned int)i];
        char txt[120];
        const voxel_c * sh = d.shapeId < puzzle->getNumberOfShapes() ? puzzle->getShape(d.shapeId) : 0;
        if (sh && sh->getName().length())
          snprintf(txt, sizeof(txt), "S%u - %s", d.shapeId + 1, sh->getName().c_str());
        else
          snprintf(txt, sizeof(txt), "S%u", d.shapeId + 1);
        labels.push_back(txt);
        shapes.push_back(d.shapeId);
      }
    }
  }
  rodStackList->setStack(labels, shapes);
  refreshStackValid();
}

static void relayoutTab(layouter_c * tab);

void mainWindow_c::refreshStackValid(void) {
  if (!stackValidBar || !puzzle || !stacking::isStacking(*puzzle))
    return;
  bool valid = false;
  std::string reason;
  if (!problemSelector || problemSelector->getSelection() >= puzzle->getNumberOfProblems()) {
    reason = "First create a problem.";
  } else {
    reason = stacking::setupError(*puzzle->getProblem(problemSelector->getSelection()));
    valid = reason.empty();
  }
  if (stackValidBar->setState(valid, reason))
    relayoutProblemTab();
}

void mainWindow_c::relayoutProblemTab(void) {
  relayoutTab(TabProblems);
}

void mainWindow_c::refreshDiskList(void) {
  if (diskList)
    diskList->sync();
}

void mainWindow_c::cb_StackRod(void) {
  refreshStackList();
  if (problemSelector && puzzle &&
      problemSelector->getSelection() < puzzle->getNumberOfProblems())
    activateProblem(problemSelector->getSelection());
  updateInterface();
}

void mainWindow_c::cb_StackListSel(void) {
  updateInterface();
}

void mainWindow_c::cb_ProblemRod(void) {
  refreshStackSlider();
  refreshStackList();
  if (!puzzle || !problemSelector || !rodAssignSel)
    return;
  unsigned int prob = problemSelector->getSelection();
  if (prob >= puzzle->getNumberOfProblems())
    return;
  problem_c * pr = puzzle->getProblem(prob);
  unsigned int id = rodAssignSel->getSelection();
  if (pr->rodSetValid() && pr->getRodSetId() == id)
    activateProblem(prob);
  else if (View3D && id < puzzle->rodSetCount()) {
    View3D->getView()->showRodSet(puzzle, id);
    easeRodZoom(View3D);
  }
}

void mainWindow_c::cb_StackMode(Fl_Widget * o) {
  if (!puzzle)
    return;
  stackingEditGoal = (o == stackGoalMode);
  if (stackingEditGoal) {
    if (stackGoalMode && !stackGoalMode->value())
      stackGoalMode->setonly();
  } else if (stackStartMode && !stackStartMode->value()) {
    stackStartMode->setonly();
  }
  refreshStackList();
  if (TaskSelectionTab && TaskSelectionTab->value() == TabProblems &&
      problemSelector && problemSelector->getSelection() < puzzle->getNumberOfProblems())
    activateProblem(problemSelector->getSelection());
  updateInterface();
}

void mainWindow_c::syncSolverTypeMenu(void) {
  if (!solverTypeChoice || !puzzle)
    return;
  int mode = stacking::isStacking(*puzzle) ? 1 : sliding::isSliding(*puzzle) ? 2 : 0;
  /* Stacking offers one solver, chosen by the selected problem's rod set:
   * the Panex Solver for Panex columns (mode 3), else the Stacking Solver. */
  if (mode == 1 && solutionProblem && solutionProblem->getSelection() < puzzle->getNumberOfProblems()) {
    const problem_c * pr = puzzle->getProblem(solutionProblem->getSelection());
    if (pr->rodSetValid() && pr->getRodSetId() < puzzle->rodSetCount() &&
        puzzle->getRodSet(pr->getRodSetId()).panexColumns)
      mode = 3;
  }
  if (mode == solverMenuMode && solverTypeChoice->size() > 0)
    return;
  const int was = solverMenuMode;
  solverMenuMode = mode;
  int keep = solverTypeChoice->value();
  solverTypeChoice->clear();
  if (mode == 1) {
    solverTypeChoice->add("Stacking Solver");
    solverTypeChoice->value(0);
    solverTypeChoice->tooltip(" Finds the fewest rod transfers. Rod sets with Panex Style Columns use the Panex Solver instead. Click ? for more. ");
  } else if (mode == 3) {
    solverTypeChoice->add("Panex Solver");
    solverTypeChoice->value(0);
    solverTypeChoice->tooltip(" Finds the fewest rod transfers for Panex Style Columns, built for large towers: it searches from both ends and uses every core. Click ? for more. ");
  } else if (mode == 2) {
    /* The grid solvers only differ in how they take pieces apart, which a
     * sliding puzzle never does. What matters here is how far to search.
     * Entry 1 is the deep search, 2 the full one; cb_BtnStart reads the index. */
    solverTypeChoice->add("Sliding Fast Solver (250k depth)");
    solverTypeChoice->add("Sliding Deep Solver (1mil depth)");
    solverTypeChoice->add("Sliding Full Solver (full depth)");
    /* Deep by default: larger puzzles such as Panex Jr need it, and a
     * puzzle the fast search can solve takes no longer on the deep one. */
    solverTypeChoice->value(1);
    solverTypeChoice->tooltip(" How far the slide search may go before it gives up. Click ? for more. ");
  } else {
    solverTypeChoice->tooltip(solverTypeTooltip());
    for (unsigned int i = 0; i < solverTypeCount(); i++)
      solverTypeChoice->add(solverTypeLabel((solverType_e)i));
    if (was != 0 || keep < 0 || (unsigned int)keep >= solverTypeCount())
      keep = (int)SOLVER_CLASSIC;
    solverTypeChoice->value(keep);
  }
}

static void relayoutTab(layouter_c * tab) {
  if (!tab)
    return;
  tab->invalidateMinSize();
  if (tab->children() > 0) {
    LFl_Scroll * s = dynamic_cast<LFl_Scroll *>(tab->child(0));
    if (s)
      s->relayout();
  }
  tab->redraw();
}

bool mainWindow_c::syncProbArrows(bool stackingMode) {
  if (!BtnProbShapeLeft || !BtnProbShapeRight || stackingMode == probArrowsStacking)
    return false;
  probArrowsStacking = stackingMode;
  LFlatButton_c * left = static_cast<LFlatButton_c*>(BtnProbShapeLeft);
  LFlatButton_c * right = static_cast<LFlatButton_c*>(BtnProbShapeRight);
  if (stackingMode) {
    /* Up and down at the left, beside the disc list they reorder. */
    left->setGridValues(0, 0, 1, 1);
    right->setGridValues(2, 0, 1, 1);
    left->label("@-18->");
    right->label("@-12->");
    left->tooltip(" Move the selected disc up the rod ");
    right->tooltip(" Move the selected disc down the rod ");
  } else {
    left->setGridValues(probArrowRightX + 1, 0, 1, 1);
    right->setGridValues(probArrowRightX + 3, 0, 1, 1);
    left->label("@-14->");
    right->label("@-16->");
    left->tooltip(" Exchange current shape with previous shape ");
    right->tooltip(" Exchange current shape with next shape ");
  }
  for (Fl_Widget * g : probArrowGapL) {
    if (stackingMode) g->show();
    else g->hide();
  }
  for (Fl_Widget * g : probArrowGapR) {
    if (stackingMode) g->hide();
    else g->show();
  }
  left->redraw();
  right->redraw();
  return true;
}

void mainWindow_c::syncStackingChrome(void) {
  if (!puzzle || !TaskSelectionTab)
    return;
  const bool on = stacking::isStacking(*puzzle);

  auto setVis = [](Fl_Widget * w, bool show) {
    if (!w)
      return false;
    if ((bool)w->visible() == show)
      return false;
    if (show) w->show();
    else w->hide();
    return true;
  };

  bool relayoutEdit = false;
  if (setVis(shapeEditColumn, !on)) relayoutEdit = true;
  if (setVis(pieceSelGroup, !on)) relayoutEdit = true;
  if (setVis(diskList, on)) relayoutEdit = true;
  if (setVis(rodsPanel, on)) relayoutEdit = true;
  if (setVis(colorsGroup, !on)) relayoutEdit = true;
  if (relayoutEdit)
    relayoutTab(TabPieces);
  if (on)
    refreshDiskList();

  bool relayoutProb = false;
  if (setVis(rodAssignGroup, on)) relayoutProb = true;
  if (setVis(stackSliderRow, on)) relayoutProb = true;
  if (setVis(stackModeRow, on)) relayoutProb = true;
  if (setVis(piecesCountGroup, !on)) relayoutProb = true;
  if (rodStackList && setVis(rodStackList->parent(), on)) relayoutProb = true;
  if (setVis(stackOrderSep, on)) relayoutProb = true;
  if (setVis(stackValidRow, on)) relayoutProb = true;
  if (setVis(problemButtonRule, !on)) relayoutProb = true;
  if (BtnSetResult) {
    const char * lab = on ? "Set Start/Goal Rods" : "Set Result";
    if (!BtnSetResult->label() || strcmp(BtnSetResult->label(), lab) != 0) {
      BtnSetResult->label(lab);
      int tw = 0, th = 0;
      BtnSetResult->measure_label(tw, th);
      static_cast<LFlatButton_c*>(BtnSetResult)->setMinimumSize(tw + 20, th + 8);
      BtnSetResult->redraw();
      relayoutProb = true;
    }
  }
  if (relayoutProb)
    relayoutTab(TabProblems);
  if (on) {
    refreshStackSlider();
    refreshStackList();
  }

  if (BtnSetResult) {
    BtnSetResult->tooltip(on
        ? " Set the selected rod set as this problem's board "
        : " Set selected shape as result ");
  }
  if (syncProbArrows(on))
    relayoutTab(TabProblems);

  if (on && BtnNewRod && rodSel) {
    unsigned int sel = rodSel->getSelection();
    unsigned int n = puzzle->rodSetCount();
    bool ok = sel < n && !assmThread;
    if (!assmThread) BtnNewRod->activate();
    else BtnNewRod->deactivate();
    if (ok && n > 1) BtnDelRod->activate();
    else BtnDelRod->deactivate();
    if (ok) {
      BtnCpyRod->activate();
      BtnRenRod->activate();
    } else {
      BtnCpyRod->deactivate();
      BtnRenRod->deactivate();
    }
    if (ok && sel > 0) BtnRodLeft->activate();
    else BtnRodLeft->deactivate();
    if (ok && sel + 1 < n) BtnRodRight->activate();
    else BtnRodRight->deactivate();
    if (!assmThread && rodPast.size() >= 2) BtnRodUndo->activate();
    else BtnRodUndo->deactivate();
    if (!assmThread && !rodFuture.empty()) BtnRodRedo->activate();
    else BtnRodRedo->deactivate();
  }

  syncSolverTypeMenu();
}


static void cb_TaskSelectionTab_stub(Fl_Widget* o, void* v) { ((mainWindow_c*)v)->cb_TaskSelectionTab((Fl_Tabs*)o); }
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
}

static void cb_TransformPiece_stub(Fl_Widget* /*o*/, void* v) { ((mainWindow_c*)v)->cb_TransformPiece(); }
static void cb_TransformPreview_stub(void* v, voxel_c* preview, unsigned int shapeNum) {
  ((mainWindow_c*)v)->cb_TransformPreview(preview, shapeNum);
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

static void cb_EditSym_stub(Fl_Widget* o, void* v) {
  ((mainWindow_c*)v)->cb_EditSym(((LToggleButton_c*)o)->value(), ((LToggleButton_c*)o)->ButtonVal());
}
void mainWindow_c::cb_EditSym(int onoff, int value) {
  if (onoff) {
    editSymmetries |= value;
  } else {
    editSymmetries &= ~value;
  }

  pieceEdit->editSymmetries(editSymmetries);
}

static void cb_EditChoice_stub(Fl_Widget* /*o*/, void* v) { ((mainWindow_c*)v)->cb_EditChoice(); }
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

static void cb_EditMode_stub(Fl_Widget* /*o*/, void* v) { ((mainWindow_c*)v)->cb_EditMode(); }
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

static void cb_PcSel_stub(Fl_Widget* o, void* v) { ((mainWindow_c*)v)->cb_PcSel((LBlockListGroup_c*)o); }
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

static void cb_SolProbSel_stub(Fl_Widget* o, void* v) { ((mainWindow_c*)v)->cb_SolProbSel((LBlockListGroup_c*)o); }
void mainWindow_c::cb_SolProbSel(LBlockListGroup_c* grp) {
  int reason = grp->getReason();

  switch(reason) {
  case ProblemSelector::RS_CHANGEDSELECTION:

    unsigned int prob = solutionProblem->getSelection();

    if (prob < puzzle->getNumberOfProblems()) {

      /* check the number of solutions on this tab and lower the slider value if necessary */
      if (SolutionSel->value() > puzzle->getProblem(prob)->getNumberOfSavedSolutions())
        SolutionSel->value(puzzle->getProblem(prob)->getNumberOfSavedSolutions());

      updateInterface();
      activateSolution(prob, (int)SolutionSel->value()-1);
    }
    break;
  }
}

static void cb_ColSel_stub(Fl_Widget* o, void* v) { ((mainWindow_c*)v)->cb_ColSel((LBlockListGroup_c*)o); }
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

static void cb_ProbSel_stub(Fl_Widget* o, void* v) { ((mainWindow_c*)v)->cb_ProbSel((LBlockListGroup_c*)o); }
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

static void cb_pieceEdit_stub(Fl_Widget* o, void* v) { ((mainWindow_c*)v)->cb_pieceEdit((VoxelEditGroup_c*)o); }
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
      if (shapeId < puzzle->getNumberOfShapes() &&
          !sliding::isStartGoalShape(puzzle->getShape(shapeId))) {
        if (sliding::toggleCellMark(tray, o->getCursorX(), o->getCursorY(), shapeId, slidingEditMode != 0)) {
          sliding::syncSlidingProblems(*puzzle);
          changed = true;
          for (unsigned int p = 0; p < puzzle->getNumberOfProblems(); p++)
            if (puzzle->getProblem(p)->resultValid() &&
                puzzle->getProblem(p)->getResultId() == PcSel->getSelection())
              changeProblem(p);
        }
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

static void cb_NewProblem_stub(Fl_Widget* /*o*/, void* v) { ((mainWindow_c*)v)->cb_NewProblem(); }
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

static void cb_DeleteProblem_stub(Fl_Widget* /*o*/, void* v) { ((mainWindow_c*)v)->cb_DeleteProblem(); }
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

static void cb_CopyProblem_stub(Fl_Widget* /*o*/, void* v) { ((mainWindow_c*)v)->cb_CopyProblem(); }
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

static void cb_RenameProblem_stub(Fl_Widget* /*o*/, void* v) { ((mainWindow_c*)v)->cb_RenameProblem(); }
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

static void cb_ProblemLeft_stub(Fl_Widget* /*o*/, void* v) { ((mainWindow_c*)v)->cb_ProblemExchange(-1); }
static void cb_ProblemRight_stub(Fl_Widget* /*o*/, void* v) { ((mainWindow_c*)v)->cb_ProblemExchange(+1); }
void mainWindow_c::cb_ProblemExchange(int with) {

  unsigned int current = problemSelector->getSelection();
  unsigned int other = current + with;

  if ((current < puzzle->getNumberOfProblems()) && (other < puzzle->getNumberOfProblems())) {
    puzzle->exchangeProblems(current, other);
    changed = true;
    problemSelector->setSelection(other);
  }
}

static void cb_ShapeLeft_stub(Fl_Widget* /*o*/, void* v) { ((mainWindow_c*)v)->cb_ShapeExchange(-1); }
static void cb_ShapeRight_stub(Fl_Widget* /*o*/, void* v) { ((mainWindow_c*)v)->cb_ShapeExchange(+1); }
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

static void cb_ProbShapeLeft_stub(Fl_Widget* /*o*/, void* v) { ((mainWindow_c*)v)->cb_ProbShapeExchange(-1); }
static void cb_ProbShapeRight_stub(Fl_Widget* /*o*/, void* v) { ((mainWindow_c*)v)->cb_ProbShapeExchange(+1); }
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

static void cb_ColorAssSel_stub(Fl_Widget* /*o*/, void* v) { ((mainWindow_c*)v)->cb_ColorAssSel(); }
void mainWindow_c::cb_ColorAssSel(void) {
  updateInterface();
}

static void cb_ColorConstrSel_stub(Fl_Widget* /*o*/, void* v) { ((mainWindow_c*)v)->cb_ColorConstrSel(); }
void mainWindow_c::cb_ColorConstrSel(void) {
  updateInterface();
}

static void cb_ShapeToResult_stub(Fl_Widget* /*o*/, void* v) { ((mainWindow_c*)v)->cb_ShapeToResult(); }
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

static void cb_ShapeSel_stub(Fl_Widget* /*o*/, void* v) { ((mainWindow_c*)v)->cb_SelectProblemShape(); }
void mainWindow_c::cb_SelectProblemShape(void) {
  updateInterface();
  activateProblem(problemSelector->getSelection());
}

static void cb_PiecesClicked_stub(Fl_Widget* /*o*/, void* v) { ((mainWindow_c*)v)->cb_PiecesClicked(); }
void mainWindow_c::cb_PiecesClicked(void) {

  problem_c * pr = puzzle->getProblem(problemSelector->getSelection());

  shapeAssignmentSelector->setSelection(pr->getShapeIdOfPart(PiecesCountList->getClicked()));

  updateInterface();
  activateProblem(problemSelector->getSelection());
}

static void cb_AddShapeToProblem_stub(Fl_Widget* /*o*/, void* v) { ((mainWindow_c*)v)->cb_AddShapeToProblem(); }
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

static void cb_AddAllShapesToProblem_stub(Fl_Widget* /*o*/, void* v) { ((mainWindow_c*)v)->cb_AddAllShapesToProblem(); }
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

static void cb_RemoveShapeFromProblem_stub(Fl_Widget* /*o*/, void* v) { ((mainWindow_c*)v)->cb_RemoveShapeFromProblem(); }
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


static void cb_SetShapeMinimumToZero_stub (Fl_Widget* /*o*/, void* v) { ((mainWindow_c*)v)->cb_SetShapeMinimumToZero(); }
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


static void cb_RemoveAllShapesFromProblem_stub(Fl_Widget* /*o*/, void* v) { ((mainWindow_c*)v)->cb_RemoveAllShapesFromProblem(); }
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

static void cb_SetAllRange_stub(Fl_Widget* /*o*/, void* v) { ((mainWindow_c*)v)->cb_SetAllRange(); }
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

static void cb_ShapeGroup_stub(Fl_Widget* /*o*/, void* v) { ((mainWindow_c*)v)->cb_ShapeGroup(); }
void mainWindow_c::cb_ShapeGroup(void) {

  unsigned int prob = problemSelector->getSelection();

  groupsEditor_c * groupEditWin = new groupsEditor_c(puzzle, prob);

  groupEditWin->show();

  while (groupEditWin->visible())
    Fl::wait();

  if (groupEditWin->changed()) {

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

  delete groupEditWin;
}

static void cb_ExportSolutionSTL_stub(Fl_Widget* /*o*/, void* v) { ((mainWindow_c*)v)->cb_ExportSolutionSTL(); }
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

static void cb_BtnPlacementBrowser_stub(Fl_Widget* /*o*/, void* v) { ((mainWindow_c*)v)->cb_BtnPlacementBrowser(); }
void mainWindow_c::cb_BtnPlacementBrowser(void) {

  unsigned int prob = solutionProblem->getSelection();

  if (prob >= puzzle->getNumberOfProblems())
    return;

  if (!puzzle->getProblem(prob)->getAssembler()->getPiecePlacementSupported()) {
    fl_message("Sorry, no placement browser for this type of puzzle");
    return;
  }

  placementBrowser_c * plbr = new placementBrowser_c(puzzle->getProblem(prob));

  plbr->show();

  while (plbr->visible())
    Fl::wait();

  delete plbr;
}

static void cb_BtnMovementBrowser_stub(Fl_Widget* /*o*/, void* v) { ((mainWindow_c*)v)->cb_BtnMovementBrowser(); }
void mainWindow_c::cb_BtnMovementBrowser(void) {

  unsigned int prob = solutionProblem->getSelection();

  if (prob >= puzzle->getNumberOfProblems())
    return;

  unsigned int sol = (int)SolutionSel->value()-1;

  if (sol >= puzzle->getProblem(prob)->getNumberOfSavedSolutions())
    return;

  movementBrowser_c * mvbr = new movementBrowser_c(puzzle->getProblem(prob), sol);

  mvbr->show();

  while (mvbr->visible())
    Fl::wait();

  delete mvbr;
}

static void cb_BtnAssemblerStep_stub(Fl_Widget* /*o*/, void* v) { ((mainWindow_c*)v)->cb_BtnAssemblerStep(); }
void mainWindow_c::cb_BtnAssemblerStep(void) {

  bt_assert(assmThread == 0);

  assembler_c * assm = (assembler_c*)puzzle->getProblem(solutionProblem->getSelection())->getAssembler();

  bt_assert(assm);

  assm->debug_step(1);

  if (assm->getFinished() >= 1)
    puzzle->getProblem(solutionProblem->getSelection())->finishedSolving();

  updateInterface();

  std::unique_ptr<assembly_c> a = assm->getAssembly();
  View3D->getView()->showAssemblerState(puzzle->getProblem(solutionProblem->getSelection()), a.get());
}

static void cb_AllowColor_stub(Fl_Widget* /*o*/, void* v) { ((mainWindow_c*)v)->cb_AllowColor(); }
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

static void cb_DisallowColor_stub(Fl_Widget* /*o*/, void* v) { ((mainWindow_c*)v)->cb_DisallowColor(); }
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

static void cb_CCSortByResult_stub(Fl_Widget* /*o*/, void* v) { ((mainWindow_c*)v)->cb_CCSort(1); }
static void cb_CCSortByPiece_stub(Fl_Widget* /*o*/, void* v) { ((mainWindow_c*)v)->cb_CCSort(0); }
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

static void cb_BtnPrepare_stub(Fl_Widget* /*o*/, void* v) { ((mainWindow_c*)v)->cb_BtnPrepare(); }
void mainWindow_c::cb_BtnPrepare(void) {
  cb_BtnStart(true);
}

static void cb_BtnStart_stub(Fl_Widget* /*o*/, void* v) { ((mainWindow_c*)v)->cb_BtnStart(false); }
/* The Solver tab will run the Panex Solver on a stacking puzzle. */
bool mainWindow_c::panexSelected(void) const {
  return puzzle && stacking::isStacking(*puzzle) && solverMenuMode == 3;
}

void mainWindow_c::cb_BtnStart(bool prep_only) {

  /* Solve starts afresh; a saved stacking search would be lost, so ask. */
  if (stacking::isStacking(*puzzle) && !prep_only &&
      solutionProblem->getSelection() < puzzle->getNumberOfProblems()) {
    const problem_c * pr = puzzle->getProblem(solutionProblem->getSelection());
    const std::string saved = panex::savedSearch(*pr);
    if (!saved.empty()) {
      int choice = fl_choice("This problem has %s.\n\nContinue it, or start over and discard it?",
                             "Cancel", "Continue", "Start Over", saved.c_str());
      if (choice == 0)
        return;
      if (choice == 1) {
        cb_BtnCont(false);
        updateInterface();
        return;
      }
      panex::discardSaved(*pr);
    }
  }

  puzzle->getProblem(solutionProblem->getSelection())->removeAllSolutions();
  SolutionEmpty = true;

  for (unsigned int i = 0; i < puzzle->getNumberOfShapes(); i++)
    puzzle->getShape(i)->initHotspot();

  cb_BtnCont(prep_only);

  updateInterface();
  changed = true;
}

static void cb_BtnCont_stub(Fl_Widget* /*o*/, void* v) { ((mainWindow_c*)v)->cb_BtnCont(false); }
void mainWindow_c::cb_BtnCont(bool prep_only, int forProblem) {

  unsigned int prob = forProblem >= 0 ? (unsigned int)forProblem : solutionProblem->getSelection();
  const bool stackingPuzzle = puzzle && stacking::isStacking(*puzzle);

  if (!stackingPuzzle &&
      !(ggt->getGridType()->getCapabilities() & gridType_c::CAP_ASSEMBLE)) {
    fl_message("Sorry, this space grid doesn't have an assembler (yet)!");
    return;
  }

  if (SolveDisasm->value() &&
      !(ggt->getGridType()->getCapabilities() & gridType_c::CAP_DISASSEMBLE) &&
      !(puzzle && sliding::isSliding(*puzzle)) &&
      !stackingPuzzle) {
    fl_message("Sorry, this space grid doesn't have a disassembler (yet)!\n"
               "You must disable the disassembler first\n");
    return;
  }

  if (prob >= puzzle->getNumberOfProblems()) {
    fl_message("First create a problem");
    return;
  }

  if (stackingPuzzle) {
    problem_c * pr = puzzle->getProblem(prob);
    if (!pr->rodSetValid()) {
      fl_message("Choose a rod set with Set Start/Goal Rods");
      return;
    }
    if (SolveDisasm->value() == 0 && JustCount->value() == 0) {
      fl_message("Turn on Find Solutions");
      return;
    }
    std::string err = stacking::setupError(*pr);
    if (!err.empty()) {
      fl_message("%s", err.c_str());
      return;
    }
    for (unsigned int i = 0; i < puzzle->getNumberOfShapes(); i++)
      stacking::ensureHotspot(puzzle->getShape(i));
  } else if (!puzzle->getProblem(prob)->resultValid()) {
    fl_message("A result shape must be defined");
    return;
  }

  if (puzzle && sliding::isSliding(*puzzle)) {
    std::string overflow = sliding::stampOverflowMessage(*puzzle->getProblem(prob));
    if (!overflow.empty()) {
      fl_message("%s", overflow.c_str());
      return;
    }
  }

  bt_assert(assmThread == 0);

  int par = solveThread_c::PAR_REDUCE;
  if (KeepMirrors->value() != 0) par |= solveThread_c::PAR_KEEP_MIRROR;
  if (KeepRotations->value() != 0) par |= solveThread_c::PAR_KEEP_ROTATIONS;
  if (StrictColors->value() != 0) par |= solveThread_c::PAR_STRICT_COLORS;
  if (DropDisassemblies->value() != 0) par |= solveThread_c::PAR_DROP_DISASSEMBLIES;
  if (SolveDisasm->value() != 0) par |= solveThread_c::PAR_DISASSM;
  if (CheckRotations->value() != 0) par |= solveThread_c::PAR_CHECK_ROTATIONS;
  if (JustCount->value() != 0) par |= solveThread_c::PAR_JUST_COUNT;
  if (CompleteRotations->value() != 0) par |= solveThread_c::PAR_COMPLETE_ROTATIONS;
  const bool slidingSolve = sliding::isSliding(*puzzle);
  if (slidingSolve) {
    if (NestedSlides->value() != 0) par |= solveThread_c::PAR_NESTED_SLIDES;
    if (solverTypeChoice && solverTypeChoice->value() == 1) par |= solveThread_c::PAR_DEEP_SEARCH;
    if (solverTypeChoice && solverTypeChoice->value() == 2) par |= solveThread_c::PAR_FULL_SEARCH;
  }

  if ((slidingSolve || stacking::isStacking(*puzzle)) && HighMemory->value() != 0)
    par |= solveThread_c::PAR_HIGH_MEMORY;
  if (panexSelected()) {
    std::string why = panex::unsupported(*puzzle->getProblem(prob));
    if (!why.empty()) {
      fl_message("%s", why.c_str());
      return;
    }
    par |= solveThread_c::PAR_PANEX_SOLVER;
  }
  if (stacking::isStacking(*puzzle)) {
    /* Both stacking solvers save their search: Solve has already discarded
     * a saved search it should not carry on. */
    par |= solveThread_c::PAR_PANEX_RESUME;
    if (Autosave && Autosave->value() == 0)
      par |= solveThread_c::PAR_PANEX_NO_AUTOSAVE;
    puzzle->getProblem(prob)->removeAllSolutions();
  }

  assmThread = new solveThread_c(*puzzle->getProblem(prob), par);

  /* Sliding and stacking menus list their own solvers, not solverType_e. */
  solverType_e st = SOLVER_CLASSIC;
  if (solverTypeChoice && solverMenuMode == 0)
    st = solverTypeFromIndex(solverTypeChoice->value());
  assmThread->setSolverType(st);
  assmThread->setSortMethod(sortMethod->value());
  assmThread->setSolutionLimits((int)solLimit->value(), (int)solDrop->value());

  autosaveFrom = std::chrono::steady_clock::now();
  autosaveProblem = prob;

  if (!assmThread->start(prep_only)) {
    fl_message("Could not start the solving process, the thread creation failed, sorry.");
    delete assmThread;
    assmThread = 0;

  } else {

    updateInterface();
    TimeEst->value("unknown");
    changed = true;
  }
}

static void cb_BtnStop_stub(Fl_Widget* /*o*/, void* v) { ((mainWindow_c*)v)->cb_BtnStop(); }
void mainWindow_c::cb_BtnStop(void) {

  bt_assert(assmThread);

  assmThread->stop();
}

static void cb_SolutionSel_stub(Fl_Widget* o, void* v) { ((mainWindow_c*)v)->cb_SolutionSel((Fl_Value_Slider*)o); }
void mainWindow_c::cb_SolutionSel(Fl_Value_Slider* o) {
  o->take_focus();
  activateSolution(solutionProblem->getSelection(), int(o->value()-1));
  updateInterface();
}

float mainWindow_c::animStep(void) const {
  float v = (float)SolutionAnim->value();
  if (puzzle && stacking::isStacking(*puzzle))
    v *= (float)stacking::STEPS_PER_MOVE;
  return v;
}

static void cb_SolutionAnim_stub(Fl_Widget* o, void* v) { ((mainWindow_c*)v)->cb_SolutionAnim((Fl_Value_Slider*)o); }
void mainWindow_c::cb_SolutionAnim(Fl_Value_Slider* o) {
  o->take_focus();
  if (disassemble) {
    disassemble->setStep(animStep(), config.useBlendedRemoving(), true);
    View3D->getView()->updatePositions(disassemble);
  }
}

void mainWindow_c::updateSolverOptionCheckboxes(void) {

  const bool justCount = JustCount->value() != 0;
  const bool justLevels = DropDisassemblies->value() != 0;
  const bool gridDisasm =
      (ggt->getGridType()->getCapabilities() & gridType_c::CAP_DISASSEMBLE) != 0;
  /* Sliding has no brick take-apart. The checkbox still selects whether a
   * start must slide to the goal before it is kept as a solution. */
  const bool slidingPuzzle = puzzle && sliding::isSliding(*puzzle);
  const bool stackingPuzzle = puzzle && stacking::isStacking(*puzzle);
  const bool canDisassemble = gridDisasm || slidingPuzzle || stackingPuzzle;
  const char * disasmLabel = (slidingPuzzle || stackingPuzzle) ? "Find Solutions" : "Disassemble";
  if (!SolveDisasm->label() || std::strcmp(SolveDisasm->label(), disasmLabel) != 0)
    SolveDisasm->copy_label(disasmLabel);
  SolveDisasm->tooltip(stackingPuzzle
      ? " Search for a shortest sequence of rod transfers from the start stacking to the goal. "
      : slidingPuzzle
      ? " Keep a placement only when each marked piece can slide from its start cell to its goal cell. "
      : " Do also try to disassemble the assembled puzzles. Only puzzles that can be disassembled will be added to solutions ");

  if (justCount) {
    DropDisassemblies->value(0);
    DropDisassemblies->deactivate();
    SolveDisasm->value(0);
    SolveDisasm->deactivate();
    CheckRotations->value(0);
    CheckRotations->deactivate();
    JustCount->activate();
  } else if (justLevels) {
    JustCount->value(0);
    JustCount->deactivate();
    DropDisassemblies->activate();
    if (canDisassemble) {
      SolveDisasm->value(1);
      SolveDisasm->deactivate();
    } else {
      SolveDisasm->value(0);
      SolveDisasm->deactivate();
    }
    /* Just Levels: Check Rotations stays available for grids that disassemble.
     * Sliding has no piece-rotation take-apart. */
    if (gridDisasm)
      CheckRotations->activate();
    else {
      CheckRotations->value(0);
      CheckRotations->deactivate();
    }
  } else {
    JustCount->activate();
    DropDisassemblies->activate();

    if (canDisassemble)
      SolveDisasm->activate();
    else {
      SolveDisasm->value(0);
      SolveDisasm->deactivate();
    }

    if (slidingPuzzle && !slidingDisasmDefaulted) {
      SolveDisasm->value(1);
      slidingDisasmDefaulted = true;
    }
    if (stackingPuzzle && !stackingDisasmDefaulted) {
      SolveDisasm->value(1);
      stackingDisasmDefaulted = true;
    }

    /* Piece rotations belong to the brick disassembler. */
    if (SolveDisasm->value() == 0 || !gridDisasm) {
      CheckRotations->value(0);
      CheckRotations->deactivate();
    } else {
      CheckRotations->activate();
    }
  }

  /* Symmetry filter options:
   * Keep Rotated turns the filter off entirely, so Deep and Keep Mirror are moot.
   * Deep only runs when the filter is active, so it conflicts with Keep Rotated.
   */
  const bool keepRotations = KeepRotations->value() != 0;
  const bool deepSymmetry = CompleteRotations->value() != 0;

  if (keepRotations) {
    CompleteRotations->value(0);
    CompleteRotations->deactivate();
    KeepMirrors->value(0);
    KeepMirrors->deactivate();
    KeepRotations->activate();
  } else if (deepSymmetry) {
    KeepRotations->value(0);
    KeepRotations->deactivate();
    CompleteRotations->activate();
    KeepMirrors->activate();
  } else {
    CompleteRotations->activate();
    KeepMirrors->activate();
    KeepRotations->activate();
  }

  /* Just Levels and Strict Colors do nothing for sliding. Nested slides
   * takes Just Levels' place and High Memory (sliding and stacking) takes
   * Deep Symmetry Check's;
   * search depth is the Solver Type choice.
   * The symmetry options go for sliding and stacking alike. */
  {
    bool moved = false;
    auto setVis = [&moved](Fl_Widget * w, bool show) {
      if (!w || (bool)w->visible() == show)
        return;
      if (show) w->show();
      else w->hide();
      moved = true;
    };
    if (slidingPuzzle) {
      DropDisassemblies->value(0);
      StrictColors->value(0);
    }
    setVis(DropDisassemblies, !slidingPuzzle);
    setVis(StrictColors, !slidingPuzzle);
    setVis(NestedSlides, slidingPuzzle);
    setVis(HighMemory, slidingPuzzle || stackingPuzzle);
    /* The symmetry filter drops starts that are rotations or mirrors of
     * another. Stacking never assembles, and sliding must keep every start,
     * so these three have no use there. */
    const bool symmetryOptions = !slidingPuzzle && !stackingPuzzle;
    if (!symmetryOptions) {
      CompleteRotations->value(0);
      KeepMirrors->value(0);
      KeepRotations->value(0);
    }
    setVis(CompleteRotations, symmetryOptions);
    setVis(KeepMirrors, symmetryOptions);
    setVis(KeepRotations, symmetryOptions);
    if (moved && TabSolve)
      relayoutTab(TabSolve);
  }

  if (stackingPuzzle) {
    CheckRotations->value(0);
    CheckRotations->deactivate();
    DropDisassemblies->value(0);
    DropDisassemblies->deactivate();
    CompleteRotations->value(0);
    CompleteRotations->deactivate();
    KeepMirrors->value(0);
    KeepMirrors->deactivate();
    KeepRotations->value(0);
    KeepRotations->deactivate();
    StrictColors->value(0);
    StrictColors->deactivate();
  }
}

static void cb_SolverOptions_stub(Fl_Widget* o, void* v) {
  ((mainWindow_c*)v)->cb_SolverOptions(o);
}
void mainWindow_c::cb_SolverOptions(Fl_Widget* o) {
  /* When Just Levels is newly checked, force Check Rotations off (but keep it enabled). */
  if (o == DropDisassemblies && DropDisassemblies->value() != 0)
    CheckRotations->value(0);

  updateSolverOptionCheckboxes();
}

static void cb_SrtFind_stub(Fl_Widget* /*o*/, void* v) { ((mainWindow_c*)v)->cb_SortSolutions(0); }
static void cb_SrtLevel_stub(Fl_Widget* /*o*/, void* v) { ((mainWindow_c*)v)->cb_SortSolutions(1); }
static void cb_SrtMoves_stub(Fl_Widget* /*o*/, void* v) { ((mainWindow_c*)v)->cb_SortSolutions(2); }
static void cb_SrtPieces_stub(Fl_Widget* /*o*/, void* v) { ((mainWindow_c*)v)->cb_SortSolutions(3); }
static void cb_SortMethod_stub(Fl_Widget* /*o*/, void* v) { ((mainWindow_c*)v)->cb_SortMethod(); }
void mainWindow_c::cb_SortMethod(void) {

  if (assmThread)
    return;

  unsigned int prob = solutionProblem->getSelection();

  if (prob >= puzzle->getNumberOfProblems())
    return;

  problem_c * pr = puzzle->getProblem(prob);

  if (pr->getNumberOfSavedSolutions() < 2)
    return;

  pr->sortSolutionsBySolverMethod(sortMethod->value());
  SolutionSel->value(1);
  activateSolution(prob, 0);
  updateInterface();
}
void mainWindow_c::cb_SortSolutions(unsigned int by) {
  unsigned int prob = solutionProblem->getSelection();

  if (prob >= puzzle->getNumberOfProblems())
    return;

  problem_c * pr = puzzle->getProblem(prob);

  unsigned int sol = pr->getNumberOfSavedSolutions();

  if (sol < 2)
    return;

  pr->sortSolutions(by);
  activateSolution(prob, (int)SolutionSel->value()-1);
  updateInterface();
}

static void cb_DelAll_stub(Fl_Widget* /*o*/, void* v) { ((mainWindow_c*)v)->cb_DeleteSolutions(0); }
static void cb_DelBefore_stub(Fl_Widget* /*o*/, void* v) { ((mainWindow_c*)v)->cb_DeleteSolutions(1); }
static void cb_DelAt_stub(Fl_Widget* /*o*/, void* v) { ((mainWindow_c*)v)->cb_DeleteSolutions(2); }
static void cb_DelAfter_stub(Fl_Widget* /*o*/, void* v) { ((mainWindow_c*)v)->cb_DeleteSolutions(3); }
static void cb_DelDisasmless_stub(Fl_Widget* /*o*/, void* v) { ((mainWindow_c*)v)->cb_DeleteSolutions(4); }
void mainWindow_c::cb_DeleteSolutions(unsigned int which) {

  unsigned int prob = solutionProblem->getSelection();

  if (prob >= puzzle->getNumberOfProblems())
    return;

  unsigned int sol = (int)SolutionSel->value()-1;

  problem_c * pr = puzzle->getProblem(prob);

  if (sol >= pr->getNumberOfSavedSolutions())
    return;

  unsigned int cnt;

  changed = true;

  switch (which) {
  case 0:
    cnt = pr->getNumberOfSavedSolutions();
    for (unsigned int i = 0; i < cnt; i++)
      pr->removeSolution(0);
    break;
  case 1:
    for (unsigned int i = 0; i < sol; i++)
      pr->removeSolution(0);
    SolutionSel->value(1);
    break;
  case 2:
    pr->removeSolution(sol);
    break;
  case 3:
    cnt = pr->getNumberOfSavedSolutions() - sol - 1;
    for (unsigned int i = 0; i < cnt; i++)
      pr->removeSolution(sol+1);
    break;
  case 4:
    cnt = pr->getNumberOfSavedSolutions();
    {
      unsigned int i = 0;
      while (i < cnt) {
        if (pr->getSavedSolution(i)->getDisassembly() || pr->getSavedSolution(i)->getDisassemblyInfo())
          i++;
        else {
          pr->removeSolution(i);
          cnt--;
          if (SolutionSel->value() > i)
            SolutionSel->value(SolutionSel->value()-1);
        }
      }
    }
    break;
  }

  if (SolutionSel->value() > pr->getNumberOfSavedSolutions())
    SolutionSel->value(pr->getNumberOfSavedSolutions());

  activateSolution(prob, (int)SolutionSel->value()-1);
  updateInterface();
}

static void cb_DelDisasm_stub(Fl_Widget* /*o*/, void* v) { ((mainWindow_c*)v)->cb_DeleteDisasm(); }
void mainWindow_c::cb_DeleteDisasm(void) {
  unsigned int prob = solutionProblem->getSelection();

  if (prob >= puzzle->getNumberOfProblems())
    return;

  problem_c * pr = puzzle->getProblem(prob);

  unsigned int sol = (int)SolutionSel->value()-1;

  if (sol >= pr->getNumberOfSavedSolutions())
    return;

  pr->getSavedSolution(sol)->removeDisassembly();

  changed = true;

  activateSolution(prob, (int)SolutionSel->value()-1);
  updateInterface();
}

static void cb_DelAllDisasm_stub(Fl_Widget* /*o*/, void* v) { ((mainWindow_c*)v)->cb_DeleteAllDisasm(); }
void mainWindow_c::cb_DeleteAllDisasm(void) {
  unsigned int prob = solutionProblem->getSelection();

  if (prob >= puzzle->getNumberOfProblems())
    return;

  problem_c * pr = puzzle->getProblem(prob);

  for (unsigned int i = 0; i < pr->getNumberOfSavedSolutions(); i++)
    pr->getSavedSolution(i)->removeDisassembly();

  changed = true;

  activateSolution(prob, (int)SolutionSel->value()-1);
  updateInterface();
}

static void cb_AddDisasm_stub(Fl_Widget* /*o*/, void* v) { ((mainWindow_c*)v)->cb_AddDisasm(); }
void mainWindow_c::cb_AddDisasm(void) {
  unsigned int prob = solutionProblem->getSelection();

  if (prob >= puzzle->getNumberOfProblems())
    return;

  problem_c * pr = puzzle->getProblem(prob);

  unsigned int sol = (int)SolutionSel->value()-1;

  if (sol >= pr->getNumberOfSavedSolutions())
    return;

  if (!(ggt->getGridType()->getCapabilities() & gridType_c::CAP_DISASSEMBLE)) {
    fl_message("Sorry, this space grid doesn't have a disassembler (yet)!");
    return;
  }

  std::unique_ptr<disassembler_c> dis = createDisassembler(*pr, CheckRotations->value() != 0,
      solverTypeChoice ? solverTypeFromIndex(solverTypeChoice->value())
                       : SOLVER_CLASSIC);

  std::unique_ptr<separation_c> d = dis->disassemble(pr->getSavedSolution(sol)->getAssembly());

  changed = true;

  if (d)
    pr->getSavedSolution(sol)->setDisassembly(std::move(d));

  activateSolution(prob, (int)SolutionSel->value()-1);
  updateInterface();
}

static void cb_AddAllDisasm_stub(Fl_Widget* /*o*/, void* v) { ((mainWindow_c*)v)->cb_AddAllDisasm(true); }
static void cb_AddMissingDisasm_stub(Fl_Widget* /*o*/, void* v) { ((mainWindow_c*)v)->cb_AddAllDisasm(false); }
void mainWindow_c::cb_AddAllDisasm(bool all) {
  unsigned int prob = solutionProblem->getSelection();

  if (prob >= puzzle->getNumberOfProblems())
    return;

  if (!(ggt->getGridType()->getCapabilities() & gridType_c::CAP_DISASSEMBLE)) {
    fl_message("Sorry, this space grid doesn't have a disassembler (yet)!");
    return;
  }

  problem_c * pr = puzzle->getProblem(prob);

  changed = true;

  std::unique_ptr<disassembler_c> dis = createDisassembler(*pr, CheckRotations->value() != 0,
      solverTypeChoice ? solverTypeFromIndex(solverTypeChoice->value())
                       : SOLVER_CLASSIC);

  Fl_Double_Window * w = new Fl_Double_Window(20, 20, 300, 30);
  Fl_Box * b = new Fl_Box(0, 0, 300, 30);
  w->end();
  w->label("Disassembling...");
  w->set_modal();
  char txt[100];
  w->show();

  for (unsigned int sol = 0; sol < pr->getNumberOfSavedSolutions(); sol++) {

    snprintf(txt, 100, "solved %i of %i disassemblies\n", sol, pr->getNumberOfSavedSolutions());
    b->label(txt);

    Fl::wait(0);

    if (all || !pr->getSavedSolution(sol)->getDisassembly()) {

      std::unique_ptr<separation_c> d = dis->disassemble(pr->getSavedSolution(sol)->getAssembly());

      if (d)
        pr->getSavedSolution(sol)->setDisassembly(std::move(d));
    }
  }

  delete w;

  activateSolution(prob, (int)SolutionSel->value()-1);
  updateInterface();
}


static void cb_PcVis_stub(Fl_Widget* /*o*/, void* v) { ((mainWindow_c*)v)->cb_PcVis(); }
void mainWindow_c::cb_PcVis(void) {
  View3D->getView()->updateVisibility(PcVis);
}

void mainWindow_c::cb_Status(void) {
  View3D->getView()->showColors(puzzle, StatusLine->getColorMode());
  View3D->getView()->setRenderStyle(StatusLine->getRenderStyle());
  config.renderStyle(StatusLine->getRenderStyle());
}

static void cb_3dClick_stub(Fl_Widget* /*o*/, void* v) { ((mainWindow_c*)v)->cb_3dClick(); }
void mainWindow_c::cb_3dClick(void) {


  if (TaskSelectionTab->value() == TabPieces) {

    if (Fl::event_ctrl()) {
      unsigned int shape, face;
      unsigned long voxel;

      voxel_c * sh = puzzle->getShape(PcSel->getSelection());

      if (View3D->getView()->pickShape(Fl::event_x(),
            View3D->getView()->h()-Fl::event_y(),
            &shape, &voxel, &face)) {
        sh->setState(voxel, voxel_c::VX_EMPTY);

        View3D->getView()->showSingleShape(puzzle, PcSel->getSelection());
        StatPieceInfo(PcSel->getSelection());
        changeShape(PcSel->getSelection());
        redraw();
        recordShapeAction(shapeHistory_c::AK_CLICK_3D);
      }

    } else if (Fl::event_shift() || Fl::event_alt()) {

      unsigned int shape, face;
      unsigned long voxel;

      voxel_c * sh = puzzle->getShape(PcSel->getSelection());

      if (View3D->getView()->pickShape(Fl::event_x(),
            View3D->getView()->h()-Fl::event_y(),
            &shape, &voxel, &face)) {

        unsigned int x, y, z;
        if (sh->indexToXYZ(voxel, &x, &y, &z)) {

          int nx, ny, nz;

          if (sh->getNeighbor(face, 0, x, y, z, &nx, &ny, &nz)) {

            sh->resizeInclude(nx, ny, nz);

            if (Fl::event_alt())
              sh->setState(nx, ny, nz, voxel_c::VX_VARIABLE);
            else
              sh->setState(nx, ny, nz, voxel_c::VX_FILLED);

            sh->setColor(nx, ny, nz, colorSelector->getSelection());

            View3D->getView()->showSingleShape(puzzle, PcSel->getSelection());
            StatPieceInfo(PcSel->getSelection());
            changeShape(PcSel->getSelection());
            activateShape(PcSel->getSelection());
            redraw();
            recordShapeAction(shapeHistory_c::AK_CLICK_3D);
          }
        }
      }
    }
  } else if (TaskSelectionTab->value() == TabProblems) {

    unsigned int shape;

    if (View3D->getView()->pickShape(Fl::event_x(),
        View3D->getView()->h()-Fl::event_y(),
        &shape, 0, 0)) {

      /* Brick views reserve the first two spaces. A stacking view's
       * spaces are discs and pegs, and shape-2 is not a part index. */
      if (puzzle && !stacking::isStacking(*puzzle) && shape >= 2)
        shapeAssignmentSelector->setSelection(puzzle->getProblem(problemSelector->getSelection())->getShapeIdOfPart(shape-2));
    }
  } else if (TaskSelectionTab->value() == TabSolve) {
    if (Fl::event_shift()) {
      unsigned int shape;

      if (View3D->getView()->pickShape(Fl::event_x(),
            View3D->getView()->h()-Fl::event_y(),
            &shape, 0, 0)) {

        PcVis->hidePiece(shape);
        View3D->getView()->updateVisibility(PcVis);
        redraw();
      }
    }
  }
}


bool mainWindow_c::confirmDiscard(const char * action) {

  if (!changed)
    return true;

  char msg[256];
  snprintf(msg, sizeof(msg),
           "The puzzle has unsaved changes.\nSave before you %s?", action);

  switch (fl_choice("%s", "Cancel", "Save", "Don't Save", msg)) {

    case 1:
      cb_Save();
      return !changed;

    case 2:
      return true;

    default:
      return false;
  }
}

void cb_New_stub(Fl_Widget* /*o*/, void* v) { ((mainWindow_c*)v)->cb_New(); }
void mainWindow_c::cb_New(void) {

  if (threadStopped()) {

    if (!confirmDiscard("create a new puzzle"))
      return;

    gridTypeSelectorWindow_c w;
    w.show();

    while (w.visible())
      Fl::wait();

    if (!w.accepted())
      return;

    ReplacePuzzle(new puzzle_c(w.getGridType()));

    if (fname) {
      delete [] fname;
      fname = 0;
      copy_label(platform::windowTitle(0).c_str());
    }

    changed = false;

    StatusLine->setText("");
    selectEntitiesTab(true);
    activateShape(0);
  }
}

void cb_Load_stub(Fl_Widget* /*o*/, void* v) { ((mainWindow_c*)v)->cb_Load(); }
void mainWindow_c::cb_Load(void) {

  if (threadStopped()) {

    if (!confirmDiscard("open another puzzle"))
      return;

    const char * f = bt_file_chooser_open("Open Puzzle", "Puzzle Files\t*.xmpuzzle", "");

    tryToLoad(f);
  }
}

static bool hasFileExtension(const char * path, const char * ext)
{
  if (!path || !ext)
    return false;

  size_t n = strlen(path);
  size_t e = strlen(ext);
  if (n < e)
    return false;

  const char * p = path + n - e;
  for (size_t i = 0; i < e; i++) {
    char a = p[i];
    char b = ext[i];
    if (a >= 'A' && a <= 'Z') a = (char)(a - 'A' + 'a');
    if (b >= 'A' && b <= 'Z') b = (char)(b - 'A' + 'a');
    if (a != b)
      return false;
  }
  return true;
}

void cb_Load_Ps3d_stub(Fl_Widget* /*o*/, void* v) { ((mainWindow_c*)v)->cb_Load_Ps3d(); }
void mainWindow_c::cb_Load_Ps3d(void) {

  if (threadStopped()) {

    if (!confirmDiscard("import another puzzle"))
      return;

    const char * f = bt_file_chooser_open("Import PuzzleSolver3D File",
                                          "PuzzleSolver3D Files\t*.puz",
                                          "");

    if (f) {

      std::ifstream in(f);
      puzzle_c * newPuzzle = loadPuzzlerSolver3D(&in).release();

      if (!newPuzzle) {
        fl_alert("Could not load puzzle, sorry!");
        return;
      }

      if (fname) delete [] fname;
      fname = new char[strlen(f)+1];
      strcpy(fname, f);

      copy_label(platform::windowTitle(fname).c_str());

      ReplacePuzzle(newPuzzle);

      selectEntitiesTab(true);
      activateShape(PcSel->getSelection());
      StatPieceInfo(PcSel->getSelection());

      changed = false;
    }
  }
}

void cb_Load_Scad_stub(Fl_Widget* /*o*/, void* v) { ((mainWindow_c*)v)->cb_Load_Scad(); }
void mainWindow_c::cb_Load_Scad(void) {

  if (threadStopped()) {

    if (!confirmDiscard("import another puzzle"))
      return;

    const char * f = bt_file_chooser_open("Import Puzzlecad File",
                                          "OpenSCAD Files\t*.scad",
                                          "");

    if (f) {

      std::ifstream in(f);
      puzzle_c * newPuzzle = loadOpenScadPuzzle(&in);

      if (!newPuzzle) {
        fl_alert("Could not load puzzle, sorry!");
        return;
      }

      if (fname) delete [] fname;
      fname = new char[strlen(f)+1];
      strcpy(fname, f);

      copy_label(platform::windowTitle(fname).c_str());

      ReplacePuzzle(newPuzzle);

      selectEntitiesTab(true);
      activateShape(PcSel->getSelection());
      StatPieceInfo(PcSel->getSelection());

      changed = false;
    }
  }
}

void cb_Save_stub(Fl_Widget* /*o*/, void* v) { ((mainWindow_c*)v)->cb_Save(); }
/* The recovery copy of this puzzle's autosaved solve: one per puzzle file,
 * in the user's cache folder. Empty for a puzzle with no file. */
std::string mainWindow_c::autosavePath(void) const {
  if (!fname || !fname[0])
    return "";
  const std::string cache = userCacheDirectory();
  if (cache.empty())
    return "";
  std::error_code ec;
  const std::filesystem::path file = std::filesystem::absolute(fname, ec);
  uint64_t h = 1469598103934665603ull;
  for (unsigned char c : file.string()) {
    h ^= c;
    h *= 1099511628211ull;
  }
  char tag[17];
  snprintf(tag, sizeof(tag), "%016llx", (unsigned long long)h);
  return (std::filesystem::path(cache) / "autosave" /
          (file.stem().string() + "-" + tag + ".xmpuzzle")).string();
}

bool mainWindow_c::writeAutosave(void) {
  const std::string path = autosavePath();
  if (path.empty())
    return false;
  std::error_code ec;
  std::filesystem::create_directories(std::filesystem::path(path).parent_path(), ec);
  const std::string tmp = path + ".tmp";
  {
    ogzstream ostr(tmp.c_str());
    if (!ostr)
      return false;
    xmlWriter_c xml(ostr);
    puzzle->save(xml);
    if (!ostr)
      return false;
  }
  /* Replace the old copy only once the new one is whole. */
  std::filesystem::rename(tmp, path, ec);
  return !ec;
}

void mainWindow_c::removeAutosave(void) {
  const std::string path = autosavePath();
  std::error_code ec;
  if (!path.empty())
    std::filesystem::remove(path, ec);
}

void mainWindow_c::cb_Save(void) {

  if (threadStopped()) {

    if (!fname)
      cb_SaveAs();

    else {
      ogzstream ostr(fname);

      if (ostr) {
        xmlWriter_c xml(ostr);
        puzzle->save(xml);
      }

      if (!ostr)
        fl_alert("puzzle NOT saved!!");
      else {
        changed = false;
        if (shapeHistory)
          shapeHistory->markSaved();
        /* The file now holds the solve: the recovery copy is not needed. */
        removeAutosave();
      }
    }
  }
}

void cb_Convert_stub(Fl_Widget* /*o*/, void* v) { ((mainWindow_c*)v)->cb_Convert(); }
void mainWindow_c::cb_Convert(void) {

  convertWindow_c win(puzzle->getGridType()->getType());

  win.show();

  while (win.visible())
    Fl::wait();

  if (win.okSelected())
  {
    puzzle_c * p = doConvert(puzzle, win.getTargetType());

    if (p)
    {
      ReplacePuzzle(p);
      selectEntitiesTab(true);
      activateShape(0);
      changed = true;
    }
  }
}

class voxelTableVector_c : public voxelTable_c
{
  private:

    const std::vector<voxel_c *> *shapes;

  public:

    voxelTableVector_c(const std::vector<voxel_c *> *s) : shapes(s) {}

  protected:

    const voxel_c * findSpace(unsigned int index) const { return (*shapes)[index]; }
};

void cb_AssembliesToShapes_stub(Fl_Widget* /*o*/, void* v) { ((mainWindow_c*)v)->cb_AssembliesToShapes(); }
void mainWindow_c::cb_AssembliesToShapes(void) {

  assmImportWindow_c win(puzzle);

  win.show();

  while (win.visible())
    Fl::wait();

  if (win.okSelected())
  {
    problem_c * pr = puzzle->getProblem(win.getSrcProblem());

    std::vector<voxel_c *> sh;

    unsigned int filter = win.getFilter();

    voxelTableVector_c voxelTab(&sh);

    problem_c::SolutionsLock solutionsLock(*pr);

    for (unsigned int s = 0; s < pr->getNumberOfSavedSolutions(); s++)
    {
      voxel_c * shape = pr->getSavedSolution(s)->getAssembly()->createSpace(*pr).release();

      if ((filter & assmImportWindow_c::dropDisconnected) && !shape->connected(0, true, voxel_c::VX_EMPTY))
      {
        delete shape;
        continue;
      }

      symmetries_t sym = shape->selfSymmetries();

      if ((filter & assmImportWindow_c::dropMirror) && shape->getGridType()->getSymmetries()->symmetryContainsMirror(sym))
      {
        delete shape;
        continue;
      }

      if ((filter & assmImportWindow_c::dropSymmetric) && !unSymmetric(sym))
      {
        delete shape;
        continue;
      }

      if ((filter & assmImportWindow_c::dropNonMillable) && !isMillable(shape))
      {
        delete shape;
        continue;
      }

      if ((filter & assmImportWindow_c::dropNonNotchable) && !isNotchable(shape))
      {
        delete shape;
        continue;
      }

      unsigned int voxels = shape->countState(voxel_c::VX_FILLED);
      if (voxels < win.getShapeMin() || voxels > win.getShapeMax())
      {
        delete shape;
        continue;
      }

      // if the user wants no identical shapes, we look up the current
      // shape in the known shapes table and drop it if we find it
      if (filter & assmImportWindow_c::dropIdentical)
      {
        if (voxelTab.getSpace(shape))
        {
          delete shape;
          continue;
        }
      }

      sh.push_back(shape);

      // we only need to add the current shape to the shape table
      // if the user wants to drop identical shapes and we use the table
      if (filter & assmImportWindow_c::dropIdentical)
      {
        voxelTab.addSpace(sh.size()-1);
      }
    }

    if (win.getAction() == assmImportWindow_c::A_ADD_NEW)
      pr = puzzle->getProblem(puzzle->addProblem());
    else if (win.getAction() == assmImportWindow_c::A_ADD_DST)
      pr = puzzle->getProblem(win.getDstProblem());

    // add the shapes to the problem of the problem tab
    for (unsigned int s = 0; s < sh.size(); s++)
    {
      int i = puzzle->addShape(sh[s]);

      if (win.getAction() == assmImportWindow_c::A_ADD_DST || win.getAction() == assmImportWindow_c::A_ADD_NEW)
      {
        pr->setShapeMaximum(i, win.getMax());
        pr->setShapeMinimum(i, win.getMin());
      }
    }

    changed = true;
    PiecesCountList->redraw();

    updateInterface();
  }
}

void cb_SaveAs_stub(Fl_Widget* /*o*/, void* v) { ((mainWindow_c*)v)->cb_SaveAs(); }
void mainWindow_c::cb_SaveAs(void) {

  if (threadStopped()) {
    const char * f = bt_file_chooser_save("Save Puzzle As", "Puzzle Files\t*.xmpuzzle", "");

    if (f) {

      if (!fileExists(f) || fl_choice("File exists; overwrite?", "Cancel", "Overwrite", 0)) {

        std::string f2;

        size_t flen = strlen(f);
        const char ext[] = ".xmpuzzle";
        size_t extlen = sizeof(ext) - 1;

        // check if the filename ends with ".xmpuzzle"
        if (flen < extlen || strcmp(f + flen - extlen, ext) != 0) {
          f2 = std::string(f) + ext;
        } else {
          f2 = f;
        }

        ogzstream ostr(f2.c_str());

        if (ostr)
        {
          xmlWriter_c xml(ostr);
          puzzle->save(xml);
        }

        if (!ostr)
          fl_alert("puzzle NOT saved!!!");
        else {
          changed = false;
          if (shapeHistory)
            shapeHistory->markSaved();
        }

        if (fname) delete [] fname;
        fname = new char[f2.length()+1];
        strcpy(fname, f2.c_str());

        copy_label(platform::windowTitle(fname).c_str());
        if (ostr)
          removeAutosave();

      } else {

        fl_message("File not saved!\n");
      }
    }
  }
}

void cb_Quit_stub(Fl_Widget* /*o*/, void* v) { ((mainWindow_c*)v)->hide(); }
void mainWindow_c::hide(void) {
  if (confirmDiscard("quit"))
    Fl_Double_Window::hide();
}

void cb_Config_stub(Fl_Widget* /*o*/, void* v) { ((mainWindow_c*)v)->cb_Config(); }
void mainWindow_c::cb_Config(void) {
  config.dialog();
  activateConfigOptions();
}

void cb_ToggleNotes_stub(Fl_Widget* /*o*/, void* v) { ((mainWindow_c*)v)->cb_ToggleNotes(); }
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

static void cb_NotesUpdate_stub(Fl_Widget* /*o*/, void* v) { ((mainWindow_c*)v)->cb_NotesUpdate(); }
void mainWindow_c::cb_NotesUpdate(void) {

  const char * text = notesInput->value();
  puzzle->setComment(text ? text : "");
  changed = true;
  setNotesButtonsEnabled(false);
}

static void cb_NotesRevert_stub(Fl_Widget* /*o*/, void* v) { ((mainWindow_c*)v)->cb_NotesRevert(); }
void mainWindow_c::cb_NotesRevert(void) {
  notesInput->value(puzzle->getComment().c_str());
  setNotesButtonsEnabled(false);
}

static void cb_NotesChanged_stub(Fl_Widget* /*o*/, void* v) { ((mainWindow_c*)v)->cb_NotesChanged(); }
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

void cb_ImageExportVector_stub(Fl_Widget* /*o*/, void* v) { ((mainWindow_c*)v)->cb_ImageExportVector(); }
void mainWindow_c::cb_ImageExportVector(void) {

  vectorExportWindow_c w;

  w.show();
  while (w.visible())
    Fl::wait();

  if (!w.cancelled)
    View3D->getView()->exportToVector(w.getFileName(), w.getVectorType());
}

void cb_ImageExport_stub(Fl_Widget* /*o*/, void* v) { ((mainWindow_c*)v)->cb_ImageExport(); }
void mainWindow_c::cb_ImageExport(void) {
  imageExport_c w(puzzle, fname);
  w.show();

  while (w.visible()) {
    w.update();
    if (w.isWorking())
      Fl::wait(0);
    else
      Fl::wait(1);
  }
}

void cb_STLExport_stub(Fl_Widget* /*o*/, void* v) { ((mainWindow_c*)v)->cb_STLExport(); }
void mainWindow_c::cb_STLExport(void) {
  stlExport_c w(puzzle, fname);
  w.show();

  while (w.visible()) {
    Fl::wait();
  }
}

void cb_Export_Scad_stub(Fl_Widget* /*o*/, void* v) { ((mainWindow_c*)v)->cb_Export_Scad(); }
void cb_ExportPaused_stub(Fl_Widget* /*o*/, void* v) { ((mainWindow_c*)v)->cb_ExportPaused(); }
void cb_ImportPaused_stub(Fl_Widget* /*o*/, void* v) { ((mainWindow_c*)v)->cb_ImportPaused(); }

/* A paused solve as one file, *.btsolve, to carry on later or elsewhere:
 *   "BTSOLVE1", then sections, each a 32-bit name length, the name, a
 *   64-bit size and the bytes:
 *   "puzzle"         the puzzle, gzip-compressed as BurrTools saves it. A
 *                    brick or sliding problem keeps its paused solve in it.
 *   "problem"        the paused problem's number, as text
 *   "search/<name>"  a stacking solve's saved search, file by file; the
 *                    saved levels may be left out
 */
namespace {

const char SOLVE_MAGIC[8] = {'B', 'T', 'S', 'O', 'L', 'V', 'E', '1'};

bool writeSection(std::ofstream & out, const std::string & name, std::istream & data, uint64_t size) {
  const uint32_t len = (uint32_t)name.size();
  out.write(reinterpret_cast<const char *>(&len), sizeof(len));
  out.write(name.data(), len);
  out.write(reinterpret_cast<const char *>(&size), sizeof(size));
  std::vector<char> buf(1 << 20);
  uint64_t left = size;
  while (left && out) {
    const std::streamsize n = (std::streamsize)std::min<uint64_t>(left, buf.size());
    data.read(buf.data(), n);
    if (data.gcount() != n)
      return false;
    out.write(buf.data(), n);
    left -= (uint64_t)n;
  }
  return (bool)out;
}

bool writeFileSection(std::ofstream & out, const std::string & name, const std::filesystem::path & file) {
  std::error_code ec;
  const uint64_t size = std::filesystem::file_size(file, ec);
  std::ifstream in(file, std::ios::binary);
  return !ec && in && writeSection(out, name, in, size);
}

bool writeTextSection(std::ofstream & out, const std::string & name, const std::string & text) {
  std::istringstream in(text);
  return writeSection(out, name, in, text.size());
}

/* The next section's name and size; false at the end or on a bad file. */
bool readSectionHeader(std::ifstream & in, std::string & name, uint64_t & size) {
  uint32_t len = 0;
  if (!in.read(reinterpret_cast<char *>(&len), sizeof(len)) || len > 4096)
    return false;
  name.assign(len, '\0');
  if (!in.read(&name[0], len) || !in.read(reinterpret_cast<char *>(&size), sizeof(size)))
    return false;
  return true;
}

bool copySection(std::ifstream & in, uint64_t size, std::ostream & out) {
  std::vector<char> buf(1 << 20);
  while (size) {
    const std::streamsize n = (std::streamsize)std::min<uint64_t>(size, buf.size());
    if (!in.read(buf.data(), n))
      return false;
    out.write(buf.data(), n);
    size -= (uint64_t)n;
  }
  return (bool)out;
}

std::string sizeText(uint64_t bytes) {
  char buf[32];
  if (bytes >= 1000000000ull)
    snprintf(buf, sizeof(buf), "%.1f GB", bytes / 1e9);
  else
    snprintf(buf, sizeof(buf), "%.0f MB", bytes / 1e6);
  return buf;
}

} // namespace

bool mainWindow_c::problemPaused(unsigned int prob) const {
  if (assmThread || !puzzle || prob >= puzzle->getNumberOfProblems())
    return false;
  const problem_c * pr = puzzle->getProblem(prob);
  if (stacking::isStacking(*puzzle))
    return !panex::savedSearch(*pr).empty();
  return pr->getSolveState() == SS_SOLVING;
}

void mainWindow_c::cb_ExportPaused(void) {
  const unsigned int prob = solutionProblem->getSelection();
  if (!problemPaused(prob)) {
    fl_message("Pause a solve first: the problem selected on the Solver tab has no paused solve.");
    return;
  }
  const problem_c * pr = puzzle->getProblem(prob);

  /* A stacking solve's saved search: its state, and the levels if wanted. */
  std::vector<std::filesystem::path> files;
  if (stacking::isStacking(*puzzle)) {
    const std::filesystem::path dir = panex::searchFolder(*pr);
    std::error_code ec;
    uint64_t levelBytes = 0;
    std::vector<std::filesystem::path> levels;
    for (const auto & e : std::filesystem::directory_iterator(dir, ec)) {
      if (e.path().filename() == "state.bin")
        files.push_back(e.path());
      else if (e.path().extension() == ".lvl") {
        levels.push_back(e.path());
        levelBytes += e.file_size(ec);
      }
    }
    bool withLevels = true;
    if (levelBytes > 1000000000ull) {
      const int choice = fl_choice(
          "The saved search keeps %s of levels for finding the path once the search is done.\n\n"
          "Include them? Left out, the file is much smaller, but finding the path at the end "
          "takes longer.",
          "Cancel", "Include", "Leave Out", sizeText(levelBytes).c_str());
      if (choice == 0)
        return;
      withLevels = choice == 1;
    }
    if (withLevels)
      files.insert(files.end(), levels.begin(), levels.end());
  }

  std::string preset = "puzzle";
  if (fname && fname[0])
    preset = std::filesystem::path(fname).stem().string();
  preset += "-P" + std::to_string(prob + 1) + ".btsolve";
  const char * f = bt_file_chooser_save("Export Paused Solver State", "Paused Solver State\t*.btsolve",
                                        preset.c_str());
  if (!f)
    return;
  std::string target = f;
  if (!hasFileExtension(target.c_str(), ".btsolve"))
    target += ".btsolve";

  /* The puzzle as BurrTools saves it, then read back as bytes. */
  std::error_code ec;
  const std::filesystem::path tmp = std::filesystem::temp_directory_path(ec) /
                                    ("burrtools-export-" + std::to_string(getpid()) + ".xmpuzzle");
  {
    ogzstream ostr(tmp.string().c_str());
    xmlWriter_c xml(ostr);
    puzzle->save(xml);
  }
  bool ok = false;
  {
    std::ofstream out(target, std::ios::binary | std::ios::trunc);
    out.write(SOLVE_MAGIC, sizeof(SOLVE_MAGIC));
    ok = (bool)out && writeFileSection(out, "puzzle", tmp) &&
         writeTextSection(out, "problem", std::to_string(prob));
    for (const auto & file : files)
      ok = ok && writeFileSection(out, "search/" + file.filename().string(), file);
  }
  std::filesystem::remove(tmp, ec);
  if (!ok) {
    std::filesystem::remove(target, ec);
    fl_alert("Could not export the paused solve to %s.", target.c_str());
    return;
  }
  fl_message("Exported the paused solve of problem %u to %s.", prob + 1, target.c_str());
}

void mainWindow_c::cb_ImportPaused(void) {
  if (!threadStopped())
    return;
  if (!confirmDiscard("import a paused solve"))
    return;
  const char * f = bt_file_chooser_open("Import Paused Solver State", "Paused Solver State\t*.btsolve", "");
  if (!f)
    return;

  std::ifstream in(f, std::ios::binary);
  char magic[sizeof(SOLVE_MAGIC)];
  if (!in.read(magic, sizeof(magic)) || !std::equal(magic, magic + sizeof(magic), SOLVE_MAGIC)) {
    fl_alert("%s is not a paused solver state exported by BurrTools.", f);
    return;
  }

  std::error_code ec;
  const std::filesystem::path tmp = std::filesystem::temp_directory_path(ec) /
                                    ("burrtools-import-" + std::to_string(getpid()) + ".xmpuzzle");
  std::unique_ptr<puzzle_c> loaded;
  unsigned int prob = 0;
  std::filesystem::path searchDir;
  bool ok = true;
  std::string name;
  uint64_t size = 0;
  while (ok && readSectionHeader(in, name, size)) {
    if (name == "puzzle") {
      {
        std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
        ok = copySection(in, size, out);
      }
      auto str = ok ? openGzFile(tmp.string().c_str()) : nullptr;
      if (!str) {
        ok = false;
        break;
      }
      try {
        xmlParser_c pars(*str);
        loaded = std::make_unique<puzzle_c>(pars);
      } catch (xmlParserException_c & e) {
        fl_alert("The puzzle in %s could not be read: %s", f, e.what());
        std::filesystem::remove(tmp, ec);
        return;
      }
    } else if (name == "problem") {
      std::ostringstream text;
      ok = copySection(in, size, text);
      prob = (unsigned int)strtoul(text.str().c_str(), nullptr, 10);
      if (loaded && stacking::isStacking(*loaded) && prob < loaded->getNumberOfProblems()) {
        /* Where Continue looks for this problem's saved search. */
        searchDir = panex::searchFolder(*loaded->getProblem(prob));
        if (searchDir.empty()) {
          ok = false;
          break;
        }
        std::filesystem::remove_all(searchDir, ec);
        std::filesystem::create_directories(searchDir, ec);
      }
    } else if (name.rfind("search/", 0) == 0 && !searchDir.empty() &&
               name.find("..") == std::string::npos) {
      std::ofstream out(searchDir / name.substr(7), std::ios::binary | std::ios::trunc);
      ok = copySection(in, size, out);
    } else {
      in.seekg((std::streamoff)size, std::ios::cur);
    }
  }
  std::filesystem::remove(tmp, ec);
  if (!ok || !loaded || prob >= loaded->getNumberOfProblems()) {
    fl_alert("%s could not be read completely.", f);
    return;
  }

  /* A new, untitled puzzle: the user saves it where they like. */
  if (fname) delete [] fname;
  fname = 0;
  copy_label(platform::windowTitle(0).c_str());
  ReplacePuzzle(loaded.release());
  changed = true;
  View3D->getView()->showColors(puzzle, StatusLine->getColorMode());

  TaskSelectionTab->value(TabSolve);
  solutionProblem->setSelection(prob);
  cb_TaskSelectionTab(TaskSelectionTab);
  updateInterface();
  fl_message("Imported the paused solve of problem %u. Press Continue on the Solver tab to carry it on, "
             "and save the puzzle to keep it.", prob + 1);
}
void mainWindow_c::cb_Export_Scad(void) {

  if (puzzle->getGridType()->getType() != gridType_c::GT_BRICKS) {
    fl_alert("Puzzlecad export is only available for cube-grid puzzles.");
    return;
  }

  if (puzzle->getNumberOfShapes() == 0) {
    fl_alert("Nothing to export.");
    return;
  }

  const char * preset = "";
  if (fname && fname[0])
    preset = fname;

  const char * f = bt_file_chooser_save("Export Puzzlecad File", "OpenSCAD Files\t*.scad", preset);

  if (!f)
    return;

  if (fileExists(f) && !fl_choice("File exists; overwrite?", "Cancel", "Overwrite", 0))
    return;

  char f2[1000];
  if (hasFileExtension(f, ".scad"))
    snprintf(f2, 1000, "%s", f);
  else
    snprintf(f2, 1000, "%s.scad", f);

  std::ofstream out(f2);
  unsigned int prob = 0;
  if (puzzle->getNumberOfProblems() &&
      problemSelector->getSelection() < puzzle->getNumberOfProblems())
    prob = problemSelector->getSelection();

  if (!out || !saveOpenScadPuzzle(out, puzzle, fname ? fname : f2, prob)) {
    fl_alert("Could not export puzzlecad file.");
    return;
  }
}

void cb_StatusWindow_stub(Fl_Widget* /*o*/, void* v) { ((mainWindow_c*)v)->cb_StatusWindow(); }
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

static void cb_DetailsClose_stub(Fl_Widget*, void* v) { ((mainWindow_c*)v)->cb_DetailsClose(); }
void mainWindow_c::cb_DetailsClose(void) {

  if (!detailsPanel)
    return;

  detailsPanel->hide();
  if (rightPane)
    rightPane->forceLayout();
  relayoutViewStack();
}

static void cb_DetailsChanged_stub(Fl_Widget*, void* v) { ((mainWindow_c*)v)->cb_DetailsChanged(); }
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

void cb_Toggle3D_stub(Fl_Widget* /*o*/, void* v) { ((mainWindow_c*)v)->cb_Toggle3D(); }
void mainWindow_c::cb_Toggle3D(void) {

  if (TaskSelectionTab->value() == TabPieces) {
    shapeEditorWithBig3DView = !shapeEditorWithBig3DView;
    if (!shapeEditorWithBig3DView)
      Small3DView();
    else
      Big3DView();
  }
}

void cb_ViewMode0_stub(Fl_Widget* /*o*/, void* v) { ((mainWindow_c*)v)->cb_ViewMode(0); }
void cb_ViewMode1_stub(Fl_Widget* /*o*/, void* v) { ((mainWindow_c*)v)->cb_ViewMode(1); }
void cb_ViewMode2_stub(Fl_Widget* /*o*/, void* v) { ((mainWindow_c*)v)->cb_ViewMode(2); }
void cb_ViewMode3_stub(Fl_Widget* /*o*/, void* v) { ((mainWindow_c*)v)->cb_ViewMode(3); }
void cb_RenderStyle0_stub(Fl_Widget* /*o*/, void* v) { ((mainWindow_c*)v)->cb_RenderStyle(0); }
void cb_RenderStyle1_stub(Fl_Widget* /*o*/, void* v) { ((mainWindow_c*)v)->cb_RenderStyle(1); }
void cb_RenderStyle2_stub(Fl_Widget* /*o*/, void* v) { ((mainWindow_c*)v)->cb_RenderStyle(2); }

static int viewModeMenuIdx[4];

void mainWindow_c::cb_ViewMode(int mode) {

  static Fl_Callback * const cbs[4] = {
    cb_ViewMode0_stub, cb_ViewMode1_stub, cb_ViewMode2_stub, cb_ViewMode3_stub
  };

  for (int i = 0; i < 4; i++)
    menu_MainMenu[viewModeMenuIdx[i]].clear();
  if (mode >= 0 && mode < 4)
    menu_MainMenu[viewModeMenuIdx[mode]].set();

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

void cb_About_stub(Fl_Widget* /*o*/, void* v) { ((mainWindow_c*)v)->cb_About(); }
void mainWindow_c::cb_About(void) {

  fl_message("This is the GUI for BurrTools\n"
             "\n"
             "This version is a fork of the original BurrTools 0.7.1, hosted at\n"
             "https://github.com/hypnoz/burr-tools\n"
             "\n"
             "A high level summary of important changes that were made:\n"
             "- Stability improvements\n"
             "- Support for rotations instead of just linear moves\n"
             "- More options for the CLI tool burrTxt\n"
             "- Import and export from Puzzlecad (OpenSCAD)\n"
             "- Many subtle UI changes which I hope are improvements\n"
             "  - Better button layouts and labels\n"
             "  - Helper cube in the 3D view of the shape builder\n"
             "  - Cleaner settings menu with additional options\n"
             "  - Better layout of the Notes and Details panels\n"
             "  - More clear names for the Solver checkboxes and additional options\n"
             "  - Support for sorting by rotations and seeing rotation information\n"
             "- Can save extra rotation data in the save file while still being\n"
             "  compatible with previous versions\n"
             "\n"
             "Original README:\n"
             "\n"
             "BurrTools (c) 2003-2025 by Andreas Röver\n"
             "with patches from Arne Köhn, Bryan Turner, Derek Bosch, Michael Brown\n"
             "The latest version is available at github.com/burr-tools/burr-tools\n"
             "\n"
             "This software is distributed under the GPL\n"
             "You should have received a copy of the GNU General Public License\n"
             "along with this program (COPYING); if not, write to the Free Software\n"
             "Foundation, 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA\n"
             "or see www.fsf.org\n"
             "\n"
             "The program uses\n"
             "- Fltk, libZ, libpng, gzstream, gl2ps\n"
             "- FI_Table (http://3dsite.com/people/erco/FI_Table/)\n"
             "- tr by Brian Paul (http://www.mesa3d.org/brianp/TR.html)\n"
            );
}

static const char tutorialText[] = R"TUTORIAL(This tutorial will be a brief overview of how to make and solve a puzzle with BurrTools. It will only cover the "Brick" type since that is the most basic, and the others should be clear once you know that one.

-- Creating shapes:
The first step on the Entities tab is to create a new shape. This is done by clicking the "New" button. Below the shapes list, you will see options to set the X/Y/Z size and a grid to draw the piece. The "Colors" section at the bottom is more advanced, and allows restrictions for where pieces can go in the final solution. Once the correct X/Y/Z size is set, make sure the solid red square is selected. This is the normal "static" voxel, shapes can only use this. The green square next to it is the "variable" voxel, which can only be used in the "solution" shape to allow either a shape or empty space to occupy that position. There are other buttons here to play with, the next most useful are the 3 at the right which allow filling an entire row of X/Y/Z at a time. There are also tabs at the top to let you rotate or move shapes in the grid.

The grid below has a slider on the left to change the Z layer. You can think of this grid and slider like you have a puzzle directly in front of you, the grid is the voxels you see and the slider is the layers going back away from you. When the slider is at the bottom, it is the "back" layer of the puzzle, the furthest away. The top of the slider is the "front" layer, the closest to you. As you draw voxels in the grid they are always relative to this view.

If the puzzle has a box, tray, or frame, this object needs to also be mapped as a shape. BurrTools considers this as a piece of the puzzle that needs to be considered during the solving process.

The final shape to create is the "solution" shape. This is the shape that matches the final assembled shape of the puzzle. This shape can use both static and variable voxels. Static requires that a piece be present in that spot. Variable can be occupied by either a shape or empty space.

After all shapes, frame/box/tray, and the solution shape are created, we will move on to building the puzzle.

-- Creating the puzzle:
The Puzzle tab is where we combine the shapes and solution to analyze as a puzzle. There may be many more shapes than you want to use in a single puzzle, so we will select a subset of pieces here. Press the "New" button to create a new puzzle, and you will be given the full list of shapes to choose from. First, select the final solution shape and then press the "Set Result" button. After that, we need to add all the shapes that will be used to make the puzzle. For each shape you want you can click it, then press the +1 button to add, or -1 to remove it. If you press +1 multiple times it will add multiple copies of the same shape. The "min=0" button will set the shape to be optional, that either 0 or 1+ copies can be used. You can create multiple puzzles with different sets of shapes. It's helpful to "Label" each puzzle with a name so you can easily tell them apart.

-- Solving the puzzle:
The Solver tab is where we solve the puzzle. You will see a list of puzzles you have created on the top left. There are many check boxes to change the puzzle is solved. Each one has a hover tool tip to explain what it does. The primary one is "Disassemble" which will try to show only assemblies which can be disassembled. There are options for checking for rotations, and limiting to just a count or level without the solving animations. Level is the number of moves to remove the first piece from an assembled puzzle. Other checks will reduce or keep symmetric/mirrored/rotated solutions.

After choosing the solve options, press the "Solve" button. If everything was set up correctly, you will see a progress bar at the bottom of the screen. When the solving is complete, you will see a list of solutions. If something wasn't set up correctly, there will be an error about voxels missing from the final shape or too many, etc. Once the solve is completed, the number of Assemblies and Solutions will be displayed. The "Assemblies" count is the number of unique assemblies that can be made from the puzzle. The "Solutions" count is the number of those assemblies which can actually be disassembled.

Only the top 100 solutions are kept, and there is a "Solution" slider to scroll through them. For each solution, you will see a "Move" slider. There is a list of numbers like 12 (5.3.2.1.1) which represents the total moves to disassemble the puzzle, and then inside the parentheses are the number of moves to remove each piece from the puzzle. If you drag the slider, you will see an animation of the puzzle being disassembled step by step, along with the step number on the left of the slider.

For now ignore the "Advanced Filters" buttons, but the list of pieces at the bottom is useful. Each one can be selected to turn that piece either into a wire frame, or totally invisble. It's very helpful to see other pieces that are obstructed from view.

-- More Advanced Topics:
The main things to learn from here are color constraints and groups. Color constraints are a way to restrict where pieces can go in the final solution. Groups are a way to group pieces together so they can be treated as a single piece. Color constraints are set in the Entities tab, by creating a new color at the bottom. When you add a voxel to a piece, it will have a small color indicator on it showing that color constraint is part of the voxel. Add the color to all the voxels in the piece, and then in the solution shape, add that same color constraint to where the piece must go.
Groups are set in the Puzzle tab, using the "Set Groups" button. If you want 2 pieces to be treated as a single piece, choose the "Add Group" button, then next to the two pieces put a number like 1 or 2 that is the same for both pieces. Back in the list of pieces, you will see a label like "G1(2)" which is the main group number and sub group number within that group.

For even more advanced topics or learning, visit the BurrTools documentation website at https://burrtools.sourceforge.net/gui-doc/toc.html
The documentation was written for an older version of BurrTools, but the concepts are still valid.
)TUTORIAL";

static void cb_TutorialClose_stub(Fl_Widget* /*o*/, void* v) { ((Fl_Double_Window*)v)->hide(); }
void cb_Tutorial_stub(Fl_Widget* /*o*/, void* v) { ((mainWindow_c*)v)->cb_Tutorial(); }
static void cb_SolverTypeHelp_stub(Fl_Widget* /*o*/, void* v) { ((mainWindow_c*)v)->cb_SolverTypeHelp(); }
static void cb_SolverType_stub(Fl_Widget* /*o*/, void* v) { ((mainWindow_c*)v)->cb_SolverType(); }
static void cb_SortByHelp_stub(Fl_Widget* /*o*/, void* v) { ((mainWindow_c*)v)->cb_SortByHelp(); }
static void cb_SolverTypeHelpClose_stub(Fl_Widget* /*o*/, void* v) { ((Fl_Double_Window*)v)->hide(); }

class LFl_Help_View : public Fl_Help_View, public layoutable_c {
  public:
  LFl_Help_View(int x = 0, int y = 0, int w = 1, int h = 1)
    : Fl_Help_View(0, 0, 0, 0), layoutable_c(x, y, w, h) {}
  virtual void getMinSize(int *width, int *height) const {
    *width = 30;
    *height = 20;
  }
};

static void htmlAppendEscaped(std::string &out, const char *s, size_t n) {
  for (size_t i = 0; i < n; i++) {
    switch (s[i]) {
      case '&': out += "&amp;"; break;
      case '<': out += "&lt;"; break;
      case '>': out += "&gt;"; break;
      default:  out += s[i]; break;
    }
  }
}

static std::string tutorialToHtml() {
  std::string html;
  html += "<html><body>";
  html += "<p><font size=\"6\"><b><u>BurrTools Basics Tutorial</u></b></font></p>";

  const char *p = tutorialText;
  while (*p) {
    const char *eol = strchr(p, '\n');
    size_t len = eol ? (size_t)(eol - p) : strlen(p);

    if (len >= 3 && p[0] == '-' && p[1] == '-' && p[2] == ' ') {
      html += "<p><font size=\"5\"><b>";
      htmlAppendEscaped(html, p + 3, len - 3);
      html += "</b></font></p>";
    } else if (len > 0) {
      html += "<p>";
      htmlAppendEscaped(html, p, len);
      html += "</p>";
    }

    if (!eol)
      break;
    p = eol + 1;
  }

  html += "</body></html>";
  return html;
}

void mainWindow_c::cb_Tutorial(void) {

  LFl_Double_Window win(true);
  win.label("Tutorial");

  layouter_c *body = new layouter_c(0, 0, 1, 1);
  body->pitch(16);
  body->weight(1, 1);

  LFl_Help_View *txt = new LFl_Help_View(0, 0, 1, 1);
  txt->textfont(FL_HELVETICA);
  txt->textsize(18);
  txt->box(FL_FLAT_BOX);
  txt->color(FL_BACKGROUND_COLOR);
  txt->textcolor(FL_FOREGROUND_COLOR);
  std::string html = tutorialToHtml();
  txt->value(html.c_str());
  txt->weight(1, 1);
  txt->setMinimumSize(720, 520);
  body->end();

  layouter_c *btns = new layouter_c(0, 1, 1, 1);
  btns->pitch(8);
  (new LFl_Box(0, 0))->weight(1, 0);

  int tw = 0, th = 0;
  fl_font(FL_HELVETICA, FL_NORMAL_SIZE);
  fl_measure("Close", tw, th);

  LFl_Button * closeBtn = new LFl_Button("Close", 1, 0, 1, 1);
  closeBtn->callback(cb_TutorialClose_stub, &win);
  closeBtn->setMinimumSize(3 * (tw + 4), th + 16);

  (new LFl_Box(2, 0))->weight(1, 0);
  btns->end();

  win.show();
  txt->topline(0);
  while (win.visible())
    Fl::wait();
}

static void addSolverHelpHeading(int row, const char *name) {
  class Heading : public LFl_Box {
  public:
    Heading(const char *txt, int r) : LFl_Box(txt, 0, r, 1, 1) {
      labelfont(FL_HELVETICA_BOLD);
      labelsize(20);
      align(FL_ALIGN_TOP | FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
      fl_font(FL_HELVETICA_BOLD, 20);
      setMinimumSize(0, fl_height() + fl_descent() + 8);
    }
    void draw() {
      LFl_Box::draw();
      const char *t = label();
      if (!t || !*t)
        return;
      fl_font(labelfont(), labelsize());
      fl_color(labelcolor());
      int tw = 0, th = 0;
      fl_measure(t, tw, th);
      int x0 = x() + Fl::box_dx(box());
      // Below the full glyph box (including descenders), not on the baseline.
      int y0 = y() + Fl::box_dy(box()) + fl_height() + 2;
      fl_line(x0, y0, x0 + tw, y0);
    }
  };
  new Heading(name, row);
}

static void addSolverHelpBody(int row, const char *text) {
  class Body : public LFl_Box {
  public:
    Body(const char *txt, int r) : LFl_Box(txt, 0, r, 1, 1) {
      labelfont(FL_HELVETICA);
      labelsize(18);
      align(FL_ALIGN_TOP | FL_ALIGN_LEFT | FL_ALIGN_INSIDE | FL_ALIGN_WRAP);
    }
    void getMinSize(int *width, int *height) const {
      const int wrap = 620;
      *width = wrap;
      int tw = wrap;
      int th = 0;
      fl_font(labelfont(), labelsize());
      fl_measure(label() ? label() : "", tw, th);
      *height = th + 8;
    }
  };
  new Body(text, row);
}

void mainWindow_c::cb_SolverTypeHelp(void) {

  LFl_Double_Window win(true);
  win.label("Explanation of Solver Types");

  layouter_c *body = new layouter_c(0, 0, 1, 1);
  body->pitch(16);
  body->weight(1, 1);

  LFl_Box *title = new LFl_Box("Explanation of Solver Types", 0, 0, 1, 1);
  title->labelfont(FL_HELVETICA_BOLD);
  title->labelsize(22);
  title->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
  title->setMinimumSize(0, 32);

  layouter_c *list = new layouter_c(0, 1, 1, 1);
  list->pitch(4);
  list->weight(1, 1);

  int row = 0;
  if (puzzle && sliding::isSliding(*puzzle)) {
    addSolverHelpHeading(row++, "Sliding Fast Solver (250k depth)");
    addSolverHelpBody(row++,
        "•  Searches for the fewest moves from the start to the goal. One move is one piece going anywhere it can reach while the others stay put, corners included.\n"
        "•  Looks at up to 250,000 arrangements of the pieces for each start.\n"
        "•  Gives up sooner on a puzzle with no solution, or on one with many starts to try.");
    (new LFl_Box(0, row++))->setMinimumSize(0, 16);

    addSolverHelpHeading(row++, "Sliding Deep Solver (1mil depth)");
    addSolverHelpBody(row++,
        "•  The default. The same search, allowed up to 1,000,000 arrangements for each start.\n"
        "•  Solves larger puzzles, such as Panex Jr, that the fast solver runs out on. A puzzle the fast solver can solve takes no longer here.\n"
        "•  A search that runs out of arrangements cannot tell you the puzzle is impossible; this one runs out later.");
    (new LFl_Box(0, row++))->setMinimumSize(0, 16);

    addSolverHelpHeading(row++, "Sliding Full Solver (full depth)");
    addSolverHelpBody(row++,
        "•  The same search with no limit on arrangements. It either finds the fewest moves or proves there is no solution.\n"
        "•  Can take a long time and a lot of memory. It stops when the arrangements it holds fill about 2 GB, roughly 30 million of them, rather than run the computer out of memory.\n"
        "•  Enable High Memory raises that limit to half of this computer's memory, so the search can go much further before it stops.\n"
        "•  The Solver tab shows how many arrangements it has searched, and Stop ends the search at any time.\n"
        "•  Best for puzzles with one start layout. Each start that can reach the goal is searched in full.");
  } else if (puzzle && stacking::isStacking(*puzzle)) {
    addSolverHelpHeading(row++, "Stacking Solver");
    addSolverHelpBody(row++,
        "•  Finds the fewest rod transfers from the start stacking to the goal. One move is one disc going from the top of one rod to the top of another.\n"
        "•  Follows the rod set's rules: size, distance and Panex columns.\n"
        "•  Stops when the stackings it holds fill about 2 GB, or half of this computer's memory with Enable High Memory.");
    (new LFl_Box(0, row++))->setMinimumSize(0, 16);

    addSolverHelpHeading(row++, "Panex Solver");
    addSolverHelpBody(row++,
        "•  The same fewest transfers, built for large Panex towers. Needs Panex Style Columns.\n"
        "•  Searches from the start and the goal at once until they meet. For a tower swap, where the goal is the start's mirror image, one search does for both, and each mirror-image twin is stored once.\n"
        "•  Holds only the newest levels of the search in memory and uses every core. When it cannot keep every level, it finds the halfway point first and then solves each half.\n"
        "•  Solves an 8-piece tower swap (5,359 moves) in minutes. Larger towers can take many hours: the Solver tab shows how deep the search is.\n"
        "•  Pause saves the search, and Continue carries it on, even after quitting BurrTools. With Autosave every 20 min checked, it also saves 20 minutes into the run and every 20 minutes after, so a crash loses little.");
  } else {
  addSolverHelpHeading(row++, "BurrTools Classic");
  addSolverHelpBody(row++,
      "•  The original BurrTools solver, and the complete-search baseline.\n"
      "•  Assembly uses a single-thread dancing-links (DLX) covering search.\n"
      "•  Take-apart tries every linear slide and every 90° rotation subset (when Check Rotations is on).\n"
      "•  Will find every assembly and disassembly the other types can find.\n"
      "•  Usually the slowest choice on rotation-heavy puzzles, because the 90° search is complete.");
  (new LFl_Box(0, row++))->setMinimumSize(0, 16);

  addSolverHelpHeading(row++, "Andrew Crowell");
  addSolverHelpBody(row++,
      "•  Uses 90° take-apart heuristics from Andrew Crowell's Sliding-Cube solver.\n"
      "•  Assembly is the same single-thread DLX search as BurrTools Classic.\n"
      "•  Faster on many rotation puzzles: it does not rotate the largest remaining piece, prunes blocked rotation axes, and caps how many 90° moves are kept.\n"
      "•  Incomplete: it can miss a disassembly that BurrTools Classic would find.\n"
      "•  Best when you want a quicker rotation search and can accept a possible miss.");
  (new LFl_Box(0, row++))->setMinimumSize(0, 16);

  addSolverHelpHeading(row++, "BurrTools 2");
  addSolverHelpBody(row++,
      "•  Same complete take-apart search as BurrTools Classic.\n"
      "•  Assembly uses its own files (not Classic DLX): Knuth dancing cells, then splits leftover branches across CPU threads.\n"
      "•  Helps when finding assemblies, not taking them apart, is what takes the time.\n"
      "•  On rotation puzzles with only a few assemblies, time will be close to BurrTools Classic.\n"
      "•  Puzzles that use ranges or extra copies of a shape still assemble on one thread.");
  }

  list->end();
  body->end();

  layouter_c *btns = new layouter_c(0, 1, 1, 1);
  btns->pitch(8);
  (new LFl_Box(0, 0))->weight(1, 0);

  int tw = 0, th = 0;
  fl_font(FL_HELVETICA, FL_NORMAL_SIZE);
  fl_measure("Close", tw, th);

  LFl_Button * closeBtn = new LFl_Button("Close", 1, 0, 1, 1);
  closeBtn->callback(cb_SolverTypeHelpClose_stub, &win);
  closeBtn->setMinimumSize(3 * (tw + 4), th + 16);

  (new LFl_Box(2, 0))->weight(1, 0);
  btns->end();

  win.show();
  while (win.visible())
    Fl::wait();
}

void mainWindow_c::cb_SortByHelp(void) {

  LFl_Double_Window win(true);
  win.label("Explanation of Sort by");

  layouter_c *body = new layouter_c(0, 0, 1, 1);
  body->pitch(16);
  body->weight(1, 1);

  LFl_Box *title = new LFl_Box("Explanation of Sort by", 0, 0, 1, 1);
  title->labelfont(FL_HELVETICA_BOLD);
  title->labelsize(22);
  title->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
  title->setMinimumSize(0, 32);

  layouter_c *list = new layouter_c(0, 1, 1, 1);
  list->pitch(4);
  list->weight(1, 1);

  int row = 0;
  addSolverHelpBody(row++,
      "Set this before solving. New solutions are inserted in this order. "
      "If Display Limit is set, the lowest-ranked solutions are dropped so the list keeps the highest-ranked ones. "
      "Changing it after a solve re-sorts the saved list the same way. Highest first.");
  (new LFl_Box(0, row++))->setMinimumSize(0, 16);

  addSolverHelpHeading(row++, "Unsorted");
  addSolverHelpBody(row++,
      "•  Keep solutions in the order the solver finds them.\n"
      "•  No ranking by how hard they are to take apart.\n"
      "•  Display Limit then keeps the first solutions found, not the ones with the most moves.\n"
      "•  Keep Each still applies: only every Nth solution is saved.");
  (new LFl_Box(0, row++))->setMinimumSize(0, 16);

  addSolverHelpHeading(row++, "Moves for Complete Disassembly");
  addSolverHelpBody(row++,
      "•  Rank by the total number of sliding moves needed to take the whole puzzle apart.\n"
      "•  That is the Moves count shown for a solution, not rotations.\n"
      "•  Highest move count first: the longest take-aparts stay at the top of the list.\n"
      "•  With Display Limit, short take-aparts are the ones dropped first.");
  (new LFl_Box(0, row++))->setMinimumSize(0, 16);

  addSolverHelpHeading(row++, "Level");
  addSolverHelpBody(row++,
      "•  Rank by BurrTools disassembly level: how many moves are needed before each piece can be removed.\n"
      "•  Shown as the dotted numbers next to Move (for example 5.4.3).\n"
      "•  The first number is moves until the first piece comes out; later numbers are the rest of the sequence.\n"
      "•  Highest level first. A 5.4.3 solution ranks above 4.9.9 because 5 is greater than 4.");
  (new LFl_Box(0, row++))->setMinimumSize(0, 16);

  addSolverHelpHeading(row++, "Rotations");
  addSolverHelpBody(row++,
      "•  Rank by how many 90° rotation moves are in the take-apart sequence.\n"
      "•  That is the Rotations count shown for a solution.\n"
      "•  Highest rotation count first.\n"
      "•  Only meaningful when Check Rotations is on; otherwise every solution has zero rotations.");

  list->end();
  body->end();

  layouter_c *btns = new layouter_c(0, 1, 1, 1);
  btns->pitch(8);
  (new LFl_Box(0, 0))->weight(1, 0);

  int tw = 0, th = 0;
  fl_font(FL_HELVETICA, FL_NORMAL_SIZE);
  fl_measure("Close", tw, th);

  LFl_Button * closeBtn = new LFl_Button("Close", 1, 0, 1, 1);
  closeBtn->callback(cb_SolverTypeHelpClose_stub, &win);
  closeBtn->setMinimumSize(3 * (tw + 4), th + 16);

  (new LFl_Box(2, 0))->weight(1, 0);
  btns->end();

  win.show();
  while (win.visible())
    Fl::wait();
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
               "Shape S%i has %i voxels (%i fixed, %i variable) (Coordinates X=%i, Y=%i, Z=%i)",
               pc+1, fx+vr, fx, vr, x, y, z);
    else
      snprintf(txt, sizeof(txt),
               "Shape S%i has %i voxels (%i fixed, %i variable)",
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

      snprintf(txt, 100, "Problem P%i result can contain %i - %i voxels, pieces (n = %i) contain %i voxels", prob+1,
          getResultShape(*pr)->countState(voxel_c::VX_FILLED),
          getResultShape(*pr)->countState(voxel_c::VX_FILLED) +
          getResultShape(*pr)->countState(voxel_c::VX_VARIABLE),
          pr->getNumberOfPieces(), cnt);

    } else {

      snprintf(txt, 100, "Problem P%i result can contain %i - %i voxels, pieces (n = %i) contain %i-%i voxels", prob+1,
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

  if (canU) menu_MainMenu[findMenuEntry("Undo")].activate();
  else menu_MainMenu[findMenuEntry("Undo")].deactivate();
  if (canR) menu_MainMenu[findMenuEntry("Redo")].activate();
  else menu_MainMenu[findMenuEntry("Redo")].deactivate();

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

void cb_Undo_stub(Fl_Widget* /*o*/, void* v) { ((mainWindow_c*)v)->cb_Undo(); }
void mainWindow_c::cb_Undo(void) {
  if (!shapeHistory || !shapeHistory->canUndo() || assmThread)
    return;
  applyHistoryRestore(shapeHistory->undo(puzzle));
}

void cb_Redo_stub(Fl_Widget* /*o*/, void* v) { ((mainWindow_c*)v)->cb_Redo(); }
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
    char * keep = fname;
    fname = const_cast<char *>(f);
    const std::string recovery = autosavePath();
    fname = keep;
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

  if (fname) delete [] fname;
  fname = new char[strlen(f)+1];
  strcpy(fname, f);

  copy_label(platform::windowTitle(fname).c_str());

  ReplacePuzzle(newPuzzle);

  selectEntitiesTab(true);
  activateShape(PcSel->getSelection());
  StatPieceInfo(PcSel->getSelection());
  View3D->getView()->showColors(puzzle, StatusLine->getColorMode());

  /* A restored solve is not in the file yet. */
  changed = restored;

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
}

Fl_Menu_Item mainWindow_c::menu_MainMenu[] = {
  { "&File",           0, 0, 0, FL_SUBMENU, 0, 0, 0, 0 },
    {"New",            0, cb_New_stub,         0, 0, 0, 0, 16, 56},
    {"Open",    FL_F + 3, cb_Load_stub,        0, 0, 0, 0, 16, 56},
    {"Save",    FL_F + 2, cb_Save_stub,        0, 0, 0, 0, 16, 56},
    {"Save As",        0, cb_SaveAs_stub,      0, FL_MENU_DIVIDER, 0, 0, 16, 56},
    {"Import",         0, 0,                   0, FL_SUBMENU, 0, 0, 16, 56},
      {"PuzzleSolver3D",       0, cb_Load_Ps3d_stub,   0, 0, 0, 0, 16, 56},
      {"Puzzlecad (OpenSCAD)", 0, cb_Load_Scad_stub,   0, 0, 0, 0, 16, 56},
      {"Paused solver state",  0, cb_ImportPaused_stub, 0, 0, 0, 0, 16, 56},
      { },
    {"Export",         0, 0,                   0, FL_SUBMENU, 0, 0, 16, 56},
      {"Puzzlecad (OpenSCAD)", 0, cb_Export_Scad_stub, 0, 0, 0, 0, 16, 56},
      {"Images",             0, cb_ImageExport_stub, 0, 0, 0, 0, 16, 56},
      {"Vector Image",       0, cb_ImageExportVector_stub, 0, 0, 0, 0, 16, 56},
      {"STL",             0, cb_STLExport_stub, 0, 0, 0, 0, 16, 56},
      {"Paused solver state", 0, cb_ExportPaused_stub, 0, FL_MENU_INACTIVE, 0, 0, 16, 56},
      { },
    {"Quit",           0, cb_Quit_stub,        0, 0, 3, 0, 16, 56},
    { },
  {"&Edit",            0, 0, 0, FL_SUBMENU, 0, 0, 16, 56},
    {"Undo",    FL_COMMAND+'z', cb_Undo_stub,  0, FL_MENU_INACTIVE, 0, 0, 16, 56},
    {"Redo",    FL_COMMAND+FL_SHIFT+'z', cb_Redo_stub, 0, FL_MENU_INACTIVE, 0, 0, 16, 56},
    {"Show Notes",     0, cb_ToggleNotes_stub, 0, 0, 0, 0, 16, 56},
    {"Toggle 3D", FL_F + 4, cb_Toggle3D_stub,  0, 0, 0, 0, 16, 56},
    {"Convert brick grid type to other", 0, cb_Convert_stub, 0, 0, 0, 0, 16, 56},
    {"Convert assemblies to pieces", 0, cb_AssembliesToShapes_stub, 0, FL_MENU_DIVIDER, 0, 0, 16, 56},
    {"Display normally with shape color", 0, cb_ViewMode0_stub, 0, FL_MENU_RADIO | FL_MENU_VALUE, 0, 0, 16, 56},
    {"Display with colour constraint colors", 0, cb_ViewMode1_stub, 0, FL_MENU_RADIO, 0, 0, 16, 56},
    {"Display in anaglyph mode", 0, cb_ViewMode2_stub, 0, FL_MENU_RADIO, 0, 0, 16, 56},
    {"Display in anaglyph mode with glasses swapped", 0, cb_ViewMode3_stub, 0, FL_MENU_RADIO | FL_MENU_DIVIDER, 0, 0, 16, 56},
    {"Draw each voxel separately", 0, cb_RenderStyle0_stub, 0, FL_MENU_RADIO | FL_MENU_VALUE, 0, 0, 16, 56},
    {"Draw flat faces with edges", 0, cb_RenderStyle1_stub, 0, FL_MENU_RADIO, 0, 0, 16, 56},
    {"Draw pieces like STL export", 0, cb_RenderStyle2_stub, 0, FL_MENU_RADIO, 0, 0, 16, 56},
    { },
  {"Settings",         0, cb_Config_stub,      0, 0, 0, 0, 16, 56},
  {"Tutorial",         0, cb_Tutorial_stub,    0, 0, 0, 0, 16, 56},
  {"About",            0, cb_About_stub,       0, 0, 3, 0, 16, 56},
  { }
};

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

  if (disassemble) {
    delete disassemble;
    disassemble = 0;
  }

  if (prob < puzzle->getNumberOfProblems()) {

    problem_c * pr = puzzle->getProblem(prob);
    problem_c::SolutionsLock lock(*pr);

    if (num < pr->getNumberOfSavedSolutions()) {

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
      disassemble = new disasmToMoves_c(sep, animSize, pr->getNumberOfPieces());
      /* A sliding move can turn corners. Follow its route, not the diagonal. */
      if (sliding::isSliding(*puzzle))
        sliding::applySlideRoutes(*pr, *sep, *disassemble);
      disassemble->setStep(animStep(), config.useBlendedRemoving(), true);

      if (prob < puzzle->getNumberOfProblems()) { View3D->getView()->showAssembly(puzzle->getProblem(prob), num); if (stacking::isStacking(*puzzle)) easeRodZoom(View3D); }
      View3D->getView()->updatePositions(disassemble);
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

static bool computeTimeLeftEstimate(float finished, unsigned long ut, const solveThread_c * thread,
                                    unsigned long assembliesFound, float & remaining)
{
  const bool haveAsmFrac = (finished > 0.0001f && finished < 0.999f);
  float asmRemain = -1.0f;
  if (haveAsmFrac)
    asmRemain = ut / finished - ut;
  else if (finished >= 0.999f)
    asmRemain = 0.0f;

  const bool disasmOn = thread && thread->disassemblyEnabled();
  unsigned int pending = disasmOn ? thread->getDisassemblyPending() : 0;
  float avgDisasm = disasmOn ? thread->getAverageDisassemblySeconds() : 0.0f;
  unsigned int workers = disasmOn ? thread->getDisassemblyWorkerCount() : 1;
  if (workers < 1)
    workers = 1;

  float futureAsm = -1.0f;
  if (haveAsmFrac && assembliesFound > 0)
    futureAsm = (float)assembliesFound * (1.0f - finished) / finished;
  else if (finished >= 0.999f)
    futureAsm = 0.0f;

  float disasmRemain = -1.0f;
  if (avgDisasm > 0.0f && futureAsm >= 0.0f)
    disasmRemain = avgDisasm * ((float)pending + futureAsm) / (float)workers;

  if (!disasmOn || (pending == 0 && futureAsm <= 0.0f && haveAsmFrac)) {
    /* Assembly-only: original extrapolation from getFinished(). */
    if (asmRemain < 1.0f)
      return false;
    remaining = asmRemain;
    return true;
  }

  /* Assembly and disassembly run in parallel. Remaining wall time is the
   * slower of: finishing the assembly search, and draining current plus
   * expected future disassemblies at the measured average duration. */
  float combined = -1.0f;
  if (asmRemain >= 0.0f && disasmRemain >= 0.0f)
    combined = (asmRemain > disasmRemain) ? asmRemain : disasmRemain;
  else if (disasmRemain >= 0.0f)
    combined = disasmRemain;
  else if (asmRemain >= 1.0f)
    combined = asmRemain;
  else
    return false;

  if (combined < 1.0f)
    return false;

  remaining = combined;
  return true;
}

int mainWindow_c::findMenuEntry(const char * txt) {

  int found = -1;

  for (unsigned int i = 0; i < (sizeof(menu_MainMenu) / sizeof(menu_MainMenu[0])); i++)
    if (menu_MainMenu[i].text && (strcmp(menu_MainMenu[i].label(), txt) == 0)) {
      bt_assert(found == -1);
      found = i;
    }

  bt_assert(found >= 0);
  return found;
}

void mainWindow_c::initViewMenuIcons(void) {

  static const char * names[4] = {
    "Display normally with shape color",
    "Display with colour constraint colors",
    "Display in anaglyph mode",
    "Display in anaglyph mode with glasses swapped"
  };

  for (int i = 0; i < 4; i++)
    viewModeMenuIdx[i] = mainWindow_c::findMenuEntry(names[i]);

#ifdef __APPLE__
  /* Fl_Sys_Menu_Bar turns Fl_Multi_Label image+text items into blank native
   * rows (the radio check still appears). Keep the text labels so View is
   * readable on the system menu bar.
   */
  return;
#else
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
    menu_MainMenu[viewModeMenuIdx[i]].multi_label(&ml[i]);

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

  // Keep the static table in sync for findMenuEntry-based callers
  if (exportActive)
    menu_MainMenu[findMenuEntry("Images")].activate();
  else
    menu_MainMenu[findMenuEntry("Images")].deactivate();
  if (stlActive)
    menu_MainMenu[findMenuEntry("STL")].activate();
  else
    menu_MainMenu[findMenuEntry("STL")].deactivate();

  updateUndoRedoButtons();

  const bool slidingPuzzle = sliding::isSliding(*puzzle);
  const bool stackingPuzzle = stacking::isStacking(*puzzle);
  syncStackingChrome();
  if (colorsGroup) {
    if (slidingPuzzle || stackingPuzzle) colorsGroup->hide();
    else colorsGroup->show();
  }
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

    float asmFrac = ((prob < puzzle->getNumberOfProblems()) &&
        puzzle->getProblem(prob)->getAssembler())
          ? puzzle->getProblem(prob)->getAssembler()->getFinished()
          : 0;
    float finished = asmFrac;
    if (assmThread &&
        (prob < puzzle->getNumberOfProblems()) &&
        (&(assmThread->getProblem()) == puzzle->getProblem(prob)))
      finished = assmThread->getProgress(asmFrac);
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

      SolvingProgress->value(100*finished);
      SolvingProgress->show();

      {
        static char tmp[100];
        snprintf(tmp, 100, "%.4f%%", 100*finished);
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
      const unsigned int ut = (unsigned int)(usedMs / 1000);

      TimeUsed->value(usedTimeToString(usedMs));
      {
        float remaining;
        unsigned long nAsm = 0;
        if (pr->numAssembliesKnown())
          nAsm = pr->getNumAssemblies();
        if (computeTimeLeftEstimate(asmFrac, ut, assmThread, nAsm, remaining))
          TimeEst->value(timeToString(remaining));
        else
          TimeEst->value("unknown");
      }

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

      problem_c * pr = puzzle->getProblem(prob);

      switch(assmThread->currentAction()) {
      case solveThread_c::ACT_PREPARATION:
        {
          char tmp[20];
          snprintf(tmp, 20, "prepare piece %i", assmThread->currentActionParameter()+1);
          OutputActivity->value(tmp);
        }
        break;
      case solveThread_c::ACT_REDUCE:
        if (pr->getAssembler()) {
          char tmp[20];
          snprintf(tmp, 20, "optimize piece %i", pr->getAssembler()->getReducePiece()+1);
          OutputActivity->value(tmp);
        } else {
          char tmp[20];
          snprintf(tmp, 20, "optimize piece %i", assmThread->currentActionParameter()+1);
          OutputActivity->value(tmp);
        }
        break;
      case solveThread_c::ACT_ASSEMBLING:
        if (sliding::isSliding(*puzzle) && assmThread->disassemblyEnabled()) {
          /* A long sliding search should not look frozen. */
          char tmp[64];
          snprintf(tmp, 64, "slide search: %lu arrangements", assmThread->getSlideProgress());
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
            snprintf(tmp, 96, "panex: tracing the path, %lu of %lu moves",
                     assmThread->getSearchTraced(), assmThread->getSearchDepth());
          else
            snprintf(tmp, 96, "panex: %lu moves deep, %s found",
                     assmThread->getSearchDepth(), count);
          OutputActivity->value(tmp);
        } else if (stacking::isStacking(*puzzle)) {
          char tmp[64];
          snprintf(tmp, 64, "stack search: %lu stackings", assmThread->getSlideProgress());
          OutputActivity->value(tmp);
        } else if (assmThread->disassemblyEnabled()) {
          char tmp[64];
          snprintf(tmp, 64, "assemble (%u×disasm, %u pending)",
                   assmThread->getDisassemblyWorkerCount(),
                   assmThread->getDisassemblyPending());
          OutputActivity->value(tmp);
        } else {
          OutputActivity->value("assemble");
        }
        break;
      case solveThread_c::ACT_DISASSEMBLING:
        OutputActivity->value("disassemble");
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

  TaskSelectionTab->redraw();
  TaskSelectionTab->resize(TaskSelectionTab->x(), TaskSelectionTab->y(),
                           TaskSelectionTab->w(), TaskSelectionTab->h());

  /* File > Export > Paused solver state: only for a paused solve. */
  {
    const bool paused = solutionProblem && problemPaused(solutionProblem->getSelection());
    static int shown = -1;
    if ((int)paused != shown) {
      shown = paused;
      for (unsigned int i = 0; i < sizeof(menu_MainMenu) / sizeof(menu_MainMenu[0]); i++)
        if (menu_MainMenu[i].callback() == cb_ExportPaused_stub) {
          if (paused) menu_MainMenu[i].activate();
          else menu_MainMenu[i].deactivate();
        }
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
      delete assmThread;
      assmThread = 0;
      updateInterface();
      return;
    }

    /* Autosave a brick or sliding solve: pause it where it can be saved,
     * save, carry on. Stacking solves save their own search. Only a puzzle
     * with a file has a place for the recovery copy. */
    if (!autosaving && Autosave && Autosave->value() && fname && !stacking::isStacking(*puzzle)) {
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
      delete assmThread;
      assmThread = 0;
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

      delete assmThread;
      assmThread = 0;
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
    ((LFlatButton_c*)BtnNewShape)->weight(1, 0);
    (new LFl_Box(1, 0))->setMinimumSize(SZ_GAP, 0);
    BtnDelShape =   new LFlatButton_c(2, 0, 1, 1, "Delete", " Delete selected piece ", cb_DeleteShape_stub, this);
    ((LFlatButton_c*)BtnDelShape)->weight(1, 0);
    (new LFl_Box(3, 0))->setMinimumSize(SZ_GAP, 0);
    BtnCpyShape =   new LFlatButton_c(4, 0, 1, 1, "Copy", " Copy selected piece ", cb_CopyShape_stub, this);
    ((LFlatButton_c*)BtnCpyShape)->weight(1, 0);
    (new LFl_Box(5, 0))->setMinimumSize(SZ_GAP, 0);
    BtnRenShape =   new LFlatButton_c(6, 0, 1, 1, "Label", " Give the selected shape a name ", cb_NameShape_stub, this);
    ((LFlatButton_c*)BtnRenShape)->weight(1, 0);
    (new LFl_Box(7, 0))->setMinimumSize(SZ_GAP, 0);
    BtnUndo =       new LFlatButton_c(8, 0, 1, 1, "Undo",
#ifdef __APPLE__
                                      " Undo the last shape change Cmd+Z ",
#else
                                      " Undo the last shape change Ctrl+Z ",
#endif
                                      cb_Undo_stub, this);
    ((LFlatButton_c*)BtnUndo)->weight(1, 0);
    BtnUndo->deactivate();

    o->end();

    (new LFl_Box(0, 2))->setMinimumSize(0, SZ_GAP);

    o = new layouter_c(0, 3);

    BtnWeightInc =  new LFlatButton_c(0, 0, 1, 1, "W+", " Increase Weight of the selected shape ",cb_WeightInc_stub, this);
    ((LFlatButton_c*)BtnWeightInc)->weight(1, 0);
    (new LFl_Box(1, 0))->setMinimumSize(SZ_GAP, 0);
    BtnWeightDec =  new LFlatButton_c(2, 0, 1, 1, "W-", " Decrease Weight of the selected shape ",cb_WeightDec_stub, this);
    ((LFlatButton_c*)BtnWeightDec)->weight(1, 0);
    (new LFl_Box(3, 0))->setMinimumSize(SZ_GAP, 0);
    BtnShapeLeft =  new LFlatButton_c(4, 0, 1, 1, "@-14->", " Exchange current shape with previous shape ", cb_ShapeLeft_stub, this);
    ((LFlatButton_c*)BtnShapeLeft)->weight(1, 0);
    (new LFl_Box(5, 0))->setMinimumSize(SZ_GAP, 0);
    BtnShapeRight = new LFlatButton_c(6, 0, 1, 1, "@-16->", " Exchange current shape with next shape ", cb_ShapeRight_stub, this);
    ((LFlatButton_c*)BtnShapeRight)->weight(1, 0);
    (new LFl_Box(7, 0))->setMinimumSize(SZ_GAP, 0);
    BtnDetails = new LFlatButton_c(8, 0, 1, 1, "Details", " Show shape details ", cb_StatusWindow_stub, this);
    ((LFlatButton_c*)BtnDetails)->weight(1, 0);
    (new LFl_Box(9, 0))->setMinimumSize(SZ_GAP, 0);
    BtnRedo =    new LFlatButton_c(10, 0, 1, 1, "Redo",
#ifdef __APPLE__
                                   " Redo the last undone shape change Cmd+Shift+Z ",
#else
                                   " Redo the last undone shape change Ctrl+Shift+Z ",
#endif
                                   cb_Redo_stub, this);
    ((LFlatButton_c*)BtnRedo)->weight(1, 0);
    BtnRedo->deactivate();

    o->end();

    LFl_Box * gap = new LFl_Box(0, 4);
    gap->setMinimumSize(0, SZ_GAP);
    startGoalGap = gap;

    o = new layouter_c(0, 5);
    startGoalRow = o;
    BtnNewStartGoal = new LFlatButton_c(0, 0, 1, 1, "New Start/Goal Positions",
        " Add a shape that holds start and goal positions ", cb_NewStartGoal_stub, this);
    ((LFlatButton_c*)BtnNewStartGoal)->weight(1, 0);
    (new LFl_Box(1, 0))->setMinimumSize(SZ_GAP, 0);
    BtnDelStartGoal = new LFlatButton_c(2, 0, 1, 1, "Delete Start/Goal Positions",
        " Delete the selected start/goal shape ", cb_DelStartGoal_stub, this);
    ((LFlatButton_c*)BtnDelStartGoal)->weight(1, 0);
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
      ((LFlatButton_c*)BtnNewRod)->weight(1, 0);
      (new LFl_Box(1, 0))->setMinimumSize(SZ_GAP, 0);
      BtnDelRod = new LFlatButton_c(2, 0, 1, 1, "Delete", " Delete the selected rod set ", cb_DeleteRod_stub, this);
      ((LFlatButton_c*)BtnDelRod)->weight(1, 0);
      (new LFl_Box(3, 0))->setMinimumSize(SZ_GAP, 0);
      BtnCpyRod = new LFlatButton_c(4, 0, 1, 1, "Copy", " Copy the selected rod set ", cb_CopyRod_stub, this);
      ((LFlatButton_c*)BtnCpyRod)->weight(1, 0);
      (new LFl_Box(5, 0))->setMinimumSize(SZ_GAP, 0);
      BtnRenRod = new LFlatButton_c(6, 0, 1, 1, "Label", " Name the selected rod set ", cb_NameRod_stub, this);
      ((LFlatButton_c*)BtnRenRod)->weight(1, 0);
      (new LFl_Box(7, 0))->setMinimumSize(SZ_GAP, 0);
      BtnRodUndo = new LFlatButton_c(8, 0, 1, 1, "Undo", " Undo the last rod change ", cb_RodUndo_stub, this);
      ((LFlatButton_c*)BtnRodUndo)->weight(1, 0);
      o->end();

      (new LFl_Box(0, 2))->setMinimumSize(0, SZ_GAP);

      o = new layouter_c(0, 3);
      BtnRodLeft = new LFlatButton_c(0, 0, 1, 1, "@-14->", " Exchange with the previous rod set ", cb_RodLeft_stub, this);
      ((LFlatButton_c*)BtnRodLeft)->weight(1, 0);
      (new LFl_Box(1, 0))->setMinimumSize(SZ_GAP, 0);
      BtnRodRight = new LFlatButton_c(2, 0, 1, 1, "@-16->", " Exchange with the next rod set ", cb_RodRight_stub, this);
      ((LFlatButton_c*)BtnRodRight)->weight(1, 0);
      (new LFl_Box(3, 0))->setMinimumSize(SZ_GAP, 0);
      BtnRodRedo = new LFlatButton_c(4, 0, 1, 1, "Redo", " Redo the last undone rod change ", cb_RodRedo_stub, this);
      ((LFlatButton_c*)BtnRodRedo)->weight(1, 0);
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
    ((LFlatButton_c*)BtnNewColor)->weight(1, 0);
    (new LFl_Box(1, 0))->setMinimumSize(SZ_GAP, 0);
    BtnDelColor = new LFlatButton_c(2, 0, 1, 1, "Remove", " Remove selected colour ", cb_RemoveColor_stub, this);
    ((LFlatButton_c*)BtnDelColor)->weight(1, 0);
    (new LFl_Box(3, 0))->setMinimumSize(SZ_GAP, 0);
    BtnChnColor = new LFlatButton_c(4, 0, 1, 1, "Edit", " Change selected colour ", cb_ChangeColor_stub, this);
    ((LFlatButton_c*)BtnChnColor)->weight(1, 0);

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
    ((LFlatButton_c*)BtnNewProb)->weight(1, 0);
    (new LFl_Box(1, 0))->setMinimumSize(SZ_GAP, 0);
    BtnDelProb = new LFlatButton_c(2, 0, 1, 1, "Delete", " Delete selected problem ", cb_DeleteProblem_stub, this);
    ((LFlatButton_c*)BtnDelProb)->weight(1, 0);
    (new LFl_Box(3, 0))->setMinimumSize(SZ_GAP, 0);
    BtnCpyProb = new LFlatButton_c(4, 0, 1, 1, "Copy", " Copy selected problem ", cb_CopyProblem_stub, this);
    ((LFlatButton_c*)BtnCpyProb)->weight(1, 0);
    (new LFl_Box(5, 0))->setMinimumSize(SZ_GAP, 0);
    BtnRenProb = new LFlatButton_c(6, 0, 1, 1, "Label", " Rename selected problem ", cb_RenameProblem_stub, this);
    ((LFlatButton_c*)BtnRenProb)->weight(1, 0);
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
      ((LFlatButton_c*)BtnSetResult)->setMinimumSize(tw + 20, th + 8);
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
      ((LFl_Box*)probArrowGapL[0])->setMinimumSize(SZ_GAP, 0);
      BtnProbShapeRight = new LFlatButton_c(xp++, 0, 1, 1, "@-16->", " Exchange current shape with next shape ", cb_ProbShapeRight_stub, this);
      probArrowGapL[1] = new LFl_Box(xp++, 0);
      ((LFl_Box*)probArrowGapL[1])->setMinimumSize(SZ_GAP, 0);
      BtnAddShape = new LFlatButton_c(xp++, 0, 1, 1, "+1", " Add another one of the selected shape ", cb_AddShapeToProblem_stub, this);
      ((LFlatButton_c*)BtnAddShape)->weight(1, 0);
      (new LFl_Box(xp++, 0))->setMinimumSize(SZ_GAP, 0);
      BtnRemShape = new LFlatButton_c(xp++, 0, 1, 1, "-1", " Remove one of the selected shapes ", cb_RemoveShapeFromProblem_stub, this);
      ((LFlatButton_c*)BtnRemShape)->weight(1, 0);
      (new LFl_Box(xp++, 0))->setMinimumSize(SZ_GAP, 0);
      BtnAddAll = new LFlatButton_c(xp++, 0, 1, 1, "All +1", " Add one of all shapes except result ", cb_AddAllShapesToProblem_stub, this);
      ((LFlatButton_c*)BtnAddAll)->weight(1, 0);
      (new LFl_Box(xp++, 0))->setMinimumSize(SZ_GAP, 0);
      BtnRemAll = new LFlatButton_c(xp++, 0, 1, 1, "Clear All", " Remove all pieces ", cb_RemoveAllShapesFromProblem_stub, this);
      ((LFlatButton_c*)BtnRemAll)->weight(1, 0);
      probArrowRightX = xp;
      probArrowGapR[0] = new LFl_Box(xp++, 0);
      ((LFl_Box*)probArrowGapR[0])->setMinimumSize(SZ_GAP, 0);
      xp++;
      probArrowGapR[1] = new LFl_Box(xp++, 0);
      ((LFl_Box*)probArrowGapR[1])->setMinimumSize(SZ_GAP, 0);

      o->end();
      probArrowsStacking = true;
      syncProbArrows(false);
    }

    (new LFl_Box(0, 2))->setMinimumSize(0, SZ_GAP);

    {
      layouter_c * o = new layouter_c(0, 3);
      int xp = 0;

      BtnMinZero = new LFlatButton_c(xp++, 0, 1, 1, "min=0", " Set minimum number of pieces to 0 ", cb_SetShapeMinimumToZero_stub, this);
      ((LFlatButton_c*)BtnMinZero)->weight(1, 0);
      (new LFl_Box(xp++, 0))->setMinimumSize(SZ_GAP, 0);
      BtnSetAllRange = new LFlatButton_c(xp++, 0, 1, 1, "Set All Ranges", " Set min/max piece count for all shapes in the current problem ", cb_SetAllRange_stub, this);
      ((LFlatButton_c*)BtnSetAllRange)->weight(1, 0);
      (new LFl_Box(xp++, 0))->setMinimumSize(SZ_GAP, 0);
      BtnGroup =    new LFlatButton_c(xp++, 0, 1, 1, "Set Groups", " Edit groups of the problem ", cb_ShapeGroup_stub, this);
      ((LFlatButton_c*)BtnGroup)->weight(1, 0);

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
    ((LFlatButton_c*)BtnColSrtPc)->weight(1, 0);
    (new LFl_Box(1, 0))->setMinimumSize(SZ_GAP, 0);
    BtnColAdd = new LFlatButton_c(2, 0, 1, 1, "@-12->", " Add colour to constraint ", cb_AllowColor_stub, this);
    BtnColRem = new LFlatButton_c(3, 0, 1, 1, "@-18->", " Add colour to constraint ", cb_DisallowColor_stub, this);
    (new LFl_Box(4, 0))->setMinimumSize(SZ_GAP, 0);
    BtnColSrtRes = new LFlatButton_c(5, 0, 1, 1, "Sort by Result", " Sort Colour Constraints by Result ", cb_CCSortByResult_stub, this);
    ((LFlatButton_c*)BtnColSrtRes)->weight(1, 0);

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
    ((LFl_Value_Input*)solLimit)->setMinimumSize(48, 0);

    (new LFl_Box(2, 0))->setMinimumSize(SZ_GAP, 0);

    LFl_Box * keepEachLabel = new LFl_Box("Keep Each: ", 3, 0, 1, 1);
    keepEachLabel->tooltip(" If some solutions should be dropped. 1 to keep all, higher numbers to skip that number before the next solution is kept. Used to keep a smaller sample of a large number of solutions. ");
    solDrop = new LFl_Value_Input(4, 0, 1, 1);
    solDrop->bounds(1, 100000000);
    solDrop->step(1, 1);
    solDrop->value(1);
    solDrop->tooltip(" If some solutions should be dropped. 1 to keep all, higher numbers to skip that number before the next solution is kept. Used to keep a smaller sample of a large number of solutions. ");
    ((LFl_Value_Input*)solDrop)->setMinimumSize(48, 0);

    o->end();

    (new LFl_Box(0, 5))->setMinimumSize(0, SZ_GAP);

    o = new layouter_c(0, 6);

    new LFl_Box("Solve: ", 0, 0, 1, 1);
    BtnStart = new LFlatButton_c(1, 0, 1, 1, "Solve", " Start new solving process, removing old result ", cb_BtnStart_stub, this);
    ((LFlatButton_c*)BtnStart)->weight(1, 0);
    BtnStop = new LFlatButton_c(2, 0, 1, 1, "Pause", " Pause a currently running solution process ", cb_BtnStop_stub, this);
    ((LFlatButton_c*)BtnStop)->weight(1, 0);
    BtnCont = new LFlatButton_c(3, 0, 1, 1, "Continue", " Continue started process ", cb_BtnCont_stub, this);
    ((LFlatButton_c*)BtnCont)->weight(1, 0);

    o->end();

    (new LFl_Box(0, 7))->setMinimumSize(0, SZ_GAP);

    o = new layouter_c(0, 8);

    new LFl_Box("Analyze: ", 0, 0, 1, 1);
    BtnPlacement = new LFlatButton_c(1, 0, 1, 1, "Placements", " Browse the calculated placement of pieces ", cb_BtnPlacementBrowser_stub, this);
    ((LFlatButton_c*)BtnPlacement)->weight(1, 0);
    BtnMovement = new LFlatButton_c(2, 0, 1, 1, "Movements", " Browse the possible movements for an assembly ", cb_BtnMovementBrowser_stub, this);
    ((LFlatButton_c*)BtnMovement)->weight(1, 0);

    o->end();

    if (expertMode) {

      (new LFl_Box(0, 9))->setMinimumSize(0, SZ_GAP);

      o = new layouter_c(0, 10);

      new LFl_Box("Debug: ", 0, 0, 1, 1);
      BtnPrepare = new LFlatButton_c(1, 0, 1, 1, "Prepare", " Do the preparation phase and then stop, this removes old results ", cb_BtnPrepare_stub, this);
      ((LFlatButton_c*)BtnPrepare)->weight(1, 0);
      BtnStep = new LFlatButton_c(2, 0, 1, 1, "Step", " Make one step in the assembler ", cb_BtnAssemblerStep_stub, this);
      ((LFlatButton_c*)BtnStep)->weight(1, 0);

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
    ((LFl_Value_Output*)OutputAssemblies)->weight(2, 0);
    ((LFl_Value_Output*)OutputAssemblies)->setMinimumSize(48, 0);

    (new LFl_Box("Solutions: ", 0, 2, 1, 1))->stretchRight();
    OutputSolutions = new LFl_Value_Output(1, 2, 1, 1);
    OutputSolutions->box(FL_FLAT_BOX);
    OutputSolutions->step(1);    // make output NOT use scientific presentation for big numbers
    OutputSolutions->tooltip(" Number of solutions (assemblies that can be disassembled) found so far ");
    ((LFl_Value_Output*)OutputSolutions)->weight(2, 0);
    ((LFl_Value_Output*)OutputSolutions)->setMinimumSize(48, 0);

    (new LFl_Box("Time used: ", 2, 1, 1, 1))->stretchRight();
    TimeUsed = new LFl_Output(3, 1, 1, 1);
    TimeUsed->box(FL_FLAT_BOX);
    TimeUsed->color(FL_BACKGROUND_COLOR);
    TimeUsed->clear_visible_focus();
    ((LFl_Output*)TimeUsed)->weight(4, 0);
    ((LFl_Output*)TimeUsed)->setMinimumSize(90, 0);

    (new LFl_Box("Time left: ", 2, 2, 1, 1))->stretchRight();
    TimeEst = new LFl_Output(3, 2, 1, 1);
    TimeEst->box(FL_FLAT_BOX);
    TimeEst->color(FL_BACKGROUND_COLOR);
    TimeEst->clear_visible_focus();
    TimeEst->tooltip(" Approximate remaining time. With Check Rotations this also uses the average time per disassembly. Can still be far off. ");
    ((LFl_Output*)TimeEst)->setMinimumSize(90, 0);

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
    ((LFl_Value_Output*)SolutionsInfo)->weight(1, 0);
    ((LFl_Value_Output*)SolutionsInfo)->setMinimumSize(48, 0);

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
    ((LFl_Output*)MovesInfo)->weight(1, 0);
    ((LFl_Output*)MovesInfo)->setMinimumSize(48, 0);

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
    ((LFl_Value_Output*)AssemblyNumber)->weight(1, 0);
    ((LFl_Value_Output*)SolutionNumber)->weight(1, 0);
    ((LFl_Value_Output*)AssemblyNumber)->setMinimumSize(48, 0);
    ((LFl_Value_Output*)SolutionNumber)->setMinimumSize(48, 0);

    new LFl_Box("Moves:", 0, 1, 1, 1);
    new LFl_Box("Rotations:", 2, 1, 1, 1);

    MovesMetric = new LFl_Output(1, 1, 1, 1);
    RotationsMetric = new LFl_Output(3, 1, 1, 1);
    MovesMetric->box(FL_FLAT_BOX);
    RotationsMetric->box(FL_FLAT_BOX);
    MovesMetric->color(FL_BACKGROUND_COLOR);
    RotationsMetric->color(FL_BACKGROUND_COLOR);
    ((LFl_Output*)MovesMetric)->weight(1, 0);
    ((LFl_Output*)RotationsMetric)->weight(1, 0);
    ((LFl_Output*)MovesMetric)->setMinimumSize(48, 0);
    ((LFl_Output*)RotationsMetric)->setMinimumSize(48, 0);

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
    ((LFlatButton_c*)BtnSrtFind)->weight(1, 0);
    (new LFl_Box(2, 0))->setMinimumSize(SZ_GAP, 0);
    BtnSrtLevel = new LFlatButton_c(3, 0, 1, 1, "Level",
        " Reorder the saved solutions by disassembly level, from lowest to highest. "
        "Level is BurrTools' classic difficulty: how many moves are needed before each piece can be removed, "
        "shown as the dotted numbers next to Move (for example 5.4.3). "
        "Only solutions that already have a disassembly are compared. Needs at least two saved solutions. ",
        cb_SrtLevel_stub, this);
    ((LFlatButton_c*)BtnSrtLevel)->weight(1, 0);
    (new LFl_Box(4, 0))->setMinimumSize(SZ_GAP, 0);
    BtnSrtMoves = new LFlatButton_c(5, 0, 1, 1, "Disarm",
        " Reorder the saved solutions by how many sliding moves it takes to completely take the puzzle apart, "
        "from fewest moves to most. This uses the Moves count from the disassembly, not rotations. "
        "Only solutions that already have a disassembly are compared. Needs at least two saved solutions. ",
        cb_SrtMoves_stub, this);
    ((LFlatButton_c*)BtnSrtMoves)->weight(1, 0);
    (new LFl_Box(6, 0))->setMinimumSize(SZ_GAP, 0);
    BtnSrtPieces = new LFlatButton_c(7, 0, 1, 1, "Pieces",
        " Reorder the saved solutions by which piece shapes are used in each assembly. "
        "This groups solutions that use the same set of pieces, which is useful when a problem allows "
        "a range of piece counts or interchangeable shapes. Needs at least two saved solutions. ",
        cb_SrtPieces_stub, this);
    ((LFlatButton_c*)BtnSrtPieces)->weight(1, 0);

    o->end();

    (new LFl_Box(0, 9))->setMinimumSize(0, SZ_GAP);

    o = new filterTooltipGroup_c(0, 10);

    new LFl_Box("Delete: ", 0, 0);

    BtnDelAll =    new LFlatButton_c(1, 0, 1, 1, "All",
        " Permanently delete every saved solution for this problem, including their assemblies and take-apart sequences. "
        "This cannot be undone. The Solve/Continue search state is not reset; only the stored solution list is cleared. ",
        cb_DelAll_stub, this);
    ((LFlatButton_c*)BtnDelAll)->weight(1, 0);
    (new LFl_Box(2, 0))->setMinimumSize(SZ_GAP, 0);
    BtnDelBefore = new LFlatButton_c(3, 0, 1, 1, "Before",
        " Permanently delete every saved solution that appears before the one currently selected on the Solution slider. "
        "The selected solution and everything after it are kept. This cannot be undone. ",
        cb_DelBefore_stub, this);
    ((LFlatButton_c*)BtnDelBefore)->weight(1, 0);
    (new LFl_Box(4, 0))->setMinimumSize(SZ_GAP, 0);
    BtnDelAt =     new LFlatButton_c(5, 0, 1, 1, "At",
        " Permanently delete only the solution currently selected on the Solution slider. "
        "The next remaining solution becomes selected. This cannot be undone. ",
        cb_DelAt_stub, this);
    ((LFlatButton_c*)BtnDelAt)->weight(1, 0);
    (new LFl_Box(6, 0))->setMinimumSize(SZ_GAP, 0);
    BtnDelAfter =  new LFlatButton_c(7, 0, 1, 1, "After",
        " Permanently delete every saved solution that appears after the one currently selected on the Solution slider. "
        "The selected solution and everything before it are kept. This cannot be undone. ",
        cb_DelAfter_stub, this);
    ((LFlatButton_c*)BtnDelAfter)->weight(1, 0);
    (new LFl_Box(8, 0))->setMinimumSize(SZ_GAP, 0);
    BtnDelDisasm = new LFlatButton_c(9, 0, 1, 1, "w/o DA",
        " Without disassembly: permanently delete every saved solution that has no take-apart sequence. "
        "That includes assemblies the disassembler could not take apart, and assemblies saved without running it. "
        "Solutions that already have a disassembly are kept. This cannot be undone. ",
        cb_DelDisasmless_stub, this);
    ((LFlatButton_c*)BtnDelDisasm)->weight(1, 0);

    o->end();

    (new LFl_Box(0, 11))->setMinimumSize(0, SZ_GAP);

    o = new filterTooltipGroup_c(0, 12);

    BtnDisasmDel    = new LFlatButton_c(0, 0, 1, 1, "D DA",
        " Delete disassembly: remove the take-apart sequence from the currently selected solution only. "
        "The assembly stays in the list, so you can still see how the pieces fit together, "
        "but Move playback and the Moves/Rotations/level numbers for this solution will be empty until you recalculate (A DA). ",
        cb_DelDisasm_stub, this);
    ((LFlatButton_c*)BtnDisasmDel)->weight(1, 0);
    (new LFl_Box(1, 0))->setMinimumSize(SZ_GAP, 0);
    BtnDisasmDelAll = new LFlatButton_c(2, 0, 1, 1, "D A DA",
        " Delete all disassemblies: remove the take-apart sequence from every saved solution. "
        "The assemblies stay in the list. Use this to drop bulky disassembly data, "
        "or before recalculating everything with different rotation settings (A A DA). ",
        cb_DelAllDisasm_stub, this);
    ((LFlatButton_c*)BtnDisasmDelAll)->weight(1, 0);
    (new LFl_Box(3, 0))->setMinimumSize(SZ_GAP, 0);
    BtnDisasmAdd    = new LFlatButton_c(4, 0, 1, 1, "A DA",
        " Add disassembly: run the disassembler on the currently selected assembly and store the take-apart sequence "
        "so you can animate it and see Moves, Rotations, and level. "
        "If Check Rotations is on, rotational moves are allowed. Replaces any existing disassembly for this solution. "
        "This can take a while on hard puzzles. ",
        cb_AddDisasm_stub, this);
    ((LFlatButton_c*)BtnDisasmAdd)->weight(1, 0);
    (new LFl_Box(5, 0))->setMinimumSize(SZ_GAP, 0);
    BtnDisasmAddAll = new LFlatButton_c(6, 0, 1, 1, "A A DA",
        " Add all disassemblies: recalculate a take-apart sequence for every saved solution, replacing any that already exist. "
        "A progress window is shown. If Check Rotations is on, rotational moves are allowed. "
        "This can take a long time when there are many solutions. ",
        cb_AddAllDisasm_stub, this);
    ((LFlatButton_c*)BtnDisasmAddAll)->weight(1, 0);
    (new LFl_Box(7, 0))->setMinimumSize(SZ_GAP, 0);
    BtnDisasmAddMissing=new LFlatButton_c(8, 0, 1, 1, "A M DA",
        " Add missing disassemblies: run the disassembler only on saved solutions that do not already have a take-apart sequence. "
        "Existing disassemblies are left unchanged. If Check Rotations is on, rotational moves are allowed. "
        "Use this after a solve that skipped disassembly, or after deleting disassemblies (D DA / D A DA). ",
        cb_AddMissingDisasm_stub, this);
    ((LFlatButton_c*)BtnDisasmAddMissing)->weight(1, 0);

    o->end();

    (new LFl_Box(0, 13))->setMinimumSize(0, SZ_GAP);

    o = new filterTooltipGroup_c(0, 14);

    BtnExportSolutionSTL = new LFlatButton_c(0, 0, 1, 1, "Export Solution Pieces to STL",
        " Open a dialog to export each piece type used in the currently selected solution as its own STL file, "
        "using that solution's placement and orientation. Useful for 3D printing the pieces of one particular solution. ",
        cb_ExportSolutionSTL_stub, this);
    ((LFlatButton_c*)BtnExportSolutionSTL)->weight(1, 0);

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
    View3D->getView()->updatePositions(disassemble);
  }
}

mainWindow_c::mainWindow_c(gridType_c * gt) : LFl_Double_Window(true) {

  assmThread = 0;
  fname = 0;
  disassemble = 0;
  editSymmetries = 0;
  expertMode = true;
  view3DStack = 0;
  rightPane = 0;
  detailsPanel = 0;
  debugPanel = 0;
  TabDebug = 0;
  solverPane = 0;
  lastSolveStats = solveStats_c();
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
  contentTile = (LFl_Tile*)contentRow;
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

  if (assmThread) {
    delete assmThread;
    assmThread = 0;
  }

  delete puzzle;
  delete shapeHistory;

  if (fname) {
    delete [] fname;
    fname = 0;
  }

  if (disassemble) {
    delete disassemble;
    disassemble = 0;
  }

  if (ggt)
    delete ggt;

  if (TabDebug && TabDebug->parent() != TaskSelectionTab) {
    delete TabDebug;
    TabDebug = 0;
  }
}
