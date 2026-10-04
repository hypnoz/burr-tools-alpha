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

/* mainWindow_c, part: sliding start/goal states, and the stacking rod sets, rods and discs (Entities and Problems tabs). */
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

void cb_NewStartGoal_stub(Fl_Widget* /*o*/, void* v) { static_cast<mainWindow_c*>(v)->cb_NewStartGoal(); }
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

void cb_DelStartGoal_stub(Fl_Widget* /*o*/, void* v) { static_cast<mainWindow_c*>(v)->cb_DelStartGoal(); }
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

void cb_NewRod_stub(Fl_Widget* /*o*/, void* v) { static_cast<mainWindow_c*>(v)->cb_NewRod(); }
void cb_DeleteRod_stub(Fl_Widget* /*o*/, void* v) { static_cast<mainWindow_c*>(v)->cb_DeleteRod(); }
void cb_CopyRod_stub(Fl_Widget* /*o*/, void* v) { static_cast<mainWindow_c*>(v)->cb_CopyRod(); }
void cb_NameRod_stub(Fl_Widget* /*o*/, void* v) { static_cast<mainWindow_c*>(v)->cb_NameRod(); }
void cb_RodLeft_stub(Fl_Widget* /*o*/, void* v) { static_cast<mainWindow_c*>(v)->cb_RodExchange(-1); }
void cb_RodRight_stub(Fl_Widget* /*o*/, void* v) { static_cast<mainWindow_c*>(v)->cb_RodExchange(1); }
void cb_RodUndo_stub(Fl_Widget* /*o*/, void* v) { static_cast<mainWindow_c*>(v)->cb_RodUndo(); }
void cb_RodRedo_stub(Fl_Widget* /*o*/, void* v) { static_cast<mainWindow_c*>(v)->cb_RodRedo(); }
void cb_RodSel_stub(Fl_Widget* /*o*/, void* v) { static_cast<mainWindow_c*>(v)->cb_RodSel(); }
void cb_RodField_stub(Fl_Widget* /*o*/, void* v) { static_cast<mainWindow_c*>(v)->cb_RodField(); }
void cb_DiskList_stub(Fl_Widget* /*o*/, void* v) { static_cast<mainWindow_c*>(v)->cb_DiskList(); }
void cb_StackMode_stub(Fl_Widget* o, void* v) { static_cast<mainWindow_c*>(v)->cb_StackMode(o); }
void cb_StackRod_stub(Fl_Widget* /*o*/, void* v) { static_cast<mainWindow_c*>(v)->cb_StackRod(); }
void cb_StackValidRelayout_stub(Fl_Widget* /*o*/, void* v) { static_cast<mainWindow_c*>(v)->relayoutProblemTab(); }
void cb_StackListSel_stub(Fl_Widget* /*o*/, void* v) { static_cast<mainWindow_c*>(v)->cb_StackListSel(); }
void cb_ProblemRod_stub(Fl_Widget* /*o*/, void* v) { static_cast<mainWindow_c*>(v)->cb_ProblemRod(); }

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

void easeRodZoom(LView3dGroup * view) {
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
  /* An edit drops the solutions of every problem on this rod set, and one
   * of them may be solving: put the fields back. */
  if (assmThread) {
    loadRodFields();
    return;
  }
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

void relayoutTab(layouter_c * tab) {
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

    /* The rule fields too: an edit drops solutions, see cb_RodField. */
    Fl_Widget * fields[] = {rodCountInput, rodGrow, rodFixed, rodDistance, rodPanex};
    for (Fl_Widget * w : fields)
      if (assmThread) w->deactivate();
      else w->activate();
    if (assmThread) {
      rodHeightInput->deactivate();
      rodSizeMatters->deactivate();
      rodPocket->deactivate();
      rodPocketHeight->deactivate();
    } else {
      if (rodGrow->value()) rodHeightInput->deactivate();
      else rodHeightInput->activate();
      syncPanexFields();
    }
  }

  syncSolverTypeMenu();
}
