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

/* mainWindow_c, part: the Solver tab: solving, pausing, the solution list and its disassemblies. */
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


void cb_BtnPrepare_stub(Fl_Widget* /*o*/, void* v) { static_cast<mainWindow_c*>(v)->cb_BtnPrepare(); }
void mainWindow_c::cb_BtnPrepare(void) {
  cb_BtnStart(true);
}

void cb_BtnStart_stub(Fl_Widget* /*o*/, void* v) { static_cast<mainWindow_c*>(v)->cb_BtnStart(false); }
/* The Solver tab will run the Panex Solver on a stacking puzzle. */
bool mainWindow_c::panexSelected(void) const {
  return puzzle && stacking::isStacking(*puzzle) && solverMenuMode == 3;
}

void mainWindow_c::cb_BtnStart(bool prep_only) {

  if (assmThread)
    return;
  if (solutionProblem->getSelection() >= puzzle->getNumberOfProblems()) {
    fl_message("First create a problem");
    return;
  }

  /* Solve starts afresh; a saved stacking search would be lost, so ask. */
  if (stacking::isStacking(*puzzle) && !prep_only) {
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

void cb_BtnCont_stub(Fl_Widget* /*o*/, void* v) { static_cast<mainWindow_c*>(v)->cb_BtnCont(false); }
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

  bt_assert(!assmThread);

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

  assmThread = std::make_unique<solveThread_c>(*puzzle->getProblem(prob), par);

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
    assmThread.reset();

  } else {

    updateInterface();
    TimeEst->value("unknown");
    changed = true;
  }
}

void cb_BtnStop_stub(Fl_Widget* /*o*/, void* v) { static_cast<mainWindow_c*>(v)->cb_BtnStop(); }
void cb_BtnAbort_stub(Fl_Widget* /*o*/, void* v) { static_cast<mainWindow_c*>(v)->cb_BtnAbort(); }

bool mainWindow_c::abortable(unsigned int prob) const {
  if (!puzzle || prob >= puzzle->getNumberOfProblems())
    return false;
  if (solvingProblem(prob))
    return true;
  const problem_c * pr = puzzle->getProblem(prob);
  if (pr->getSolveState() != SS_UNSOLVED || pr->getNumberOfSavedSolutions() > 0 ||
      pr->pendingCount() > 0)
    return true;
  if (stacking::isStacking(*puzzle) && !panex::savedSearch(*pr).empty())
    return true;
  std::error_code ec;
  const std::string recovery = autosavePath();
  return !recovery.empty() && std::filesystem::exists(recovery, ec);
}

void mainWindow_c::cb_BtnAbort(void) {

  const unsigned int prob = solutionProblem->getSelection();
  if (!abortable(prob))
    return;
  /* Another problem's solve keeps running; only this one is thrown away. */
  if (assmThread && !solvingProblem(prob))
    return;

  if (fl_choice("Abort the solve of this problem and throw away all of it?\n\n"
                "Its solutions, the paused state, any saved search and the autosave copy "
                "are deleted. This cannot be undone.",
                "Cancel", "Abort", 0) != 1)
    return;

  problem_c * pr = puzzle->getProblem(prob);

  /* A hard stop: the solver thread's destructor stops the assembler, the
   * take-apart workers and any slide or stacking search, and waits for
   * them. A stacking search may save itself as it stops; that is deleted
   * below. */
  if (assmThread) {
    fl_cursor(FL_CURSOR_WAIT);
    Fl::check();
    assmThread.reset();
    fl_cursor(FL_CURSOR_DEFAULT);
  }
  autosaving = false;

  pr->removeAllSolutions();
  if (stacking::isStacking(*puzzle))
    panex::discardSaved(*pr);
  removeAutosave();

  SolutionEmpty = true;
  changed = true;
  activateSolution(prob, 0);
  updateInterface();
}
void mainWindow_c::cb_BtnStop(void) {

  /* The solve may have ended since the button was last updated. */
  if (assmThread)
    assmThread->stop();
}

void cb_SolutionSel_stub(Fl_Widget* o, void* v) { static_cast<mainWindow_c*>(v)->cb_SolutionSel(static_cast<Fl_Value_Slider*>(o)); }
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

void cb_SolutionAnim_stub(Fl_Widget* o, void* v) { static_cast<mainWindow_c*>(v)->cb_SolutionAnim(static_cast<Fl_Value_Slider*>(o)); }
void mainWindow_c::cb_SolutionAnim(Fl_Value_Slider* o) {
  o->take_focus();
  if (disassemble) {
    disassemble->setStep(animStep(), config.useBlendedRemoving(), true);
    View3D->getView()->updatePositions(disassemble.get());
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

bool mainWindow_c::solvingProblem(unsigned int prob) const {
  return assmThread && prob < puzzle->getNumberOfProblems() &&
         &assmThread->getProblem() == puzzle->getProblem(prob);
}

void cb_SolverOptions_stub(Fl_Widget* o, void* v) {
  static_cast<mainWindow_c*>(v)->cb_SolverOptions(o);
}
void mainWindow_c::cb_SolverOptions(Fl_Widget* o) {
  /* When Just Levels is newly checked, force Check Rotations off (but keep it enabled). */
  if (o == DropDisassemblies && DropDisassemblies->value() != 0)
    CheckRotations->value(0);

  updateSolverOptionCheckboxes();
  storeSolverOptions();
}

/* The Solver tab checkboxes saved with each problem, by attribute name.
 * Display Limit and Keep Each are not saved. */
namespace {
const char * const optionNames[] = {
  "disassemble", "checkRotations", "justCount", "justLevels",
  "deepSymmetry", "keepMirrors", "keepRotations", "strictColors",
  "nestedSlides", "highMemory", "autosave",
};
const char * const sortNames[] = { "unsorted", "moves", "level", "rotations" };
const char * const slidingSolverNames[] = { "fast", "deep", "full" };
const char * const gridSolverNames[] = { "classic", "crowell", "bt2" };

int nameIndex(const char * const * names, int count, const std::string & v) {
  for (int i = 0; i < count; i++)
    if (v == names[i])
      return i;
  return -1;
}
}

Fl_Check_Button * mainWindow_c::solverOptionBox(const char * name) {
  const std::string n(name);
  if (n == "disassemble") return SolveDisasm;
  if (n == "checkRotations") return CheckRotations;
  if (n == "justCount") return JustCount;
  if (n == "justLevels") return DropDisassemblies;
  if (n == "deepSymmetry") return CompleteRotations;
  if (n == "keepMirrors") return KeepMirrors;
  if (n == "keepRotations") return KeepRotations;
  if (n == "strictColors") return StrictColors;
  if (n == "nestedSlides") return NestedSlides;
  if (n == "highMemory") return HighMemory;
  if (n == "autosave") return Autosave;
  return nullptr;
}

void mainWindow_c::storeSolverOptions(void) {
  const unsigned int prob = solutionProblem->getSelection();
  if (!puzzle || prob >= puzzle->getNumberOfProblems())
    return;

  std::map<std::string, std::string> o;
  for (const char * name : optionNames)
    if (Fl_Check_Button * b = solverOptionBox(name))
      o[name] = b->value() ? "1" : "0";

  /* Stacking offers a single solver, so there is nothing to choose. */
  const int st = solverTypeChoice->value();
  if (solverMenuMode == 0 && st >= 0 && st < 3)
    o["solverType"] = gridSolverNames[st];
  else if (solverMenuMode == 2 && st >= 0 && st < 3)
    o["solverType"] = slidingSolverNames[st];

  const int sm = sortMethod->value();
  if (sm >= 0 && sm < 4)
    o["sortBy"] = sortNames[sm];

  problem_c * pr = puzzle->getProblem(prob);
  if (o != pr->getSolverOptions()) {
    pr->setSolverOptions(o);
    changed = true;
  }
}

void mainWindow_c::applySolverOptions(unsigned int prob) {
  if (!puzzle || prob >= puzzle->getNumberOfProblems())
    return;
  const std::map<std::string, std::string> & o = puzzle->getProblem(prob)->getSolverOptions();
  if (o.empty())
    return;

  /* The menu must list this puzzle's solvers before one is picked. */
  syncSolverTypeMenu();

  for (const auto & kv : o)
    if (Fl_Check_Button * b = solverOptionBox(kv.first.c_str()))
      b->value(kv.second == "1" ? 1 : 0);

  auto it = o.find("solverType");
  if (it != o.end()) {
    int st = -1;
    if (solverMenuMode == 0) st = nameIndex(gridSolverNames, 3, it->second);
    else if (solverMenuMode == 2) st = nameIndex(slidingSolverNames, 3, it->second);
    if (st >= 0)
      solverTypeChoice->value(st);
  }

  it = o.find("sortBy");
  if (it != o.end()) {
    const int sm = nameIndex(sortNames, 4, it->second);
    if (sm >= 0)
      sortMethod->value(sm);
  }

  /* The saved Disassemble wins over the first-time sliding/stacking default. */
  if (o.count("disassemble")) {
    slidingDisasmDefaulted = true;
    stackingDisasmDefaulted = true;
  }

  updateSolverOptionCheckboxes();
}

void cb_SrtFind_stub(Fl_Widget* /*o*/, void* v) { static_cast<mainWindow_c*>(v)->cb_SortSolutions(0); }
void cb_SrtLevel_stub(Fl_Widget* /*o*/, void* v) { static_cast<mainWindow_c*>(v)->cb_SortSolutions(1); }
void cb_SrtMoves_stub(Fl_Widget* /*o*/, void* v) { static_cast<mainWindow_c*>(v)->cb_SortSolutions(2); }
void cb_SrtPieces_stub(Fl_Widget* /*o*/, void* v) { static_cast<mainWindow_c*>(v)->cb_SortSolutions(3); }
void cb_SortMethod_stub(Fl_Widget* /*o*/, void* v) { static_cast<mainWindow_c*>(v)->cb_SortMethod(); }
void mainWindow_c::cb_SortMethod(void) {

  storeSolverOptions();

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

void cb_DelAll_stub(Fl_Widget* /*o*/, void* v) { static_cast<mainWindow_c*>(v)->cb_DeleteSolutions(0); }
void cb_DelBefore_stub(Fl_Widget* /*o*/, void* v) { static_cast<mainWindow_c*>(v)->cb_DeleteSolutions(1); }
void cb_DelAt_stub(Fl_Widget* /*o*/, void* v) { static_cast<mainWindow_c*>(v)->cb_DeleteSolutions(2); }
void cb_DelAfter_stub(Fl_Widget* /*o*/, void* v) { static_cast<mainWindow_c*>(v)->cb_DeleteSolutions(3); }
void cb_DelDisasmless_stub(Fl_Widget* /*o*/, void* v) { static_cast<mainWindow_c*>(v)->cb_DeleteSolutions(4); }
void mainWindow_c::cb_DeleteSolutions(unsigned int which) {

  unsigned int prob = solutionProblem->getSelection();

  if (prob >= puzzle->getNumberOfProblems() || solvingProblem(prob))
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

void cb_DelDisasm_stub(Fl_Widget* /*o*/, void* v) { static_cast<mainWindow_c*>(v)->cb_DeleteDisasm(); }
void mainWindow_c::cb_DeleteDisasm(void) {
  unsigned int prob = solutionProblem->getSelection();

  if (prob >= puzzle->getNumberOfProblems() || solvingProblem(prob))
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

void cb_DelAllDisasm_stub(Fl_Widget* /*o*/, void* v) { static_cast<mainWindow_c*>(v)->cb_DeleteAllDisasm(); }
void mainWindow_c::cb_DeleteAllDisasm(void) {
  unsigned int prob = solutionProblem->getSelection();

  if (prob >= puzzle->getNumberOfProblems() || solvingProblem(prob))
    return;

  problem_c * pr = puzzle->getProblem(prob);

  for (unsigned int i = 0; i < pr->getNumberOfSavedSolutions(); i++)
    pr->getSavedSolution(i)->removeDisassembly();

  changed = true;

  activateSolution(prob, (int)SolutionSel->value()-1);
  updateInterface();
}

void cb_AddDisasm_stub(Fl_Widget* /*o*/, void* v) { static_cast<mainWindow_c*>(v)->cb_AddDisasm(); }
void mainWindow_c::cb_AddDisasm(void) {
  unsigned int prob = solutionProblem->getSelection();

  if (prob >= puzzle->getNumberOfProblems() || solvingProblem(prob))
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

void cb_AddAllDisasm_stub(Fl_Widget* /*o*/, void* v) { static_cast<mainWindow_c*>(v)->cb_AddAllDisasm(true); }
void cb_AddMissingDisasm_stub(Fl_Widget* /*o*/, void* v) { static_cast<mainWindow_c*>(v)->cb_AddAllDisasm(false); }
void mainWindow_c::cb_AddAllDisasm(bool all) {
  unsigned int prob = solutionProblem->getSelection();

  if (prob >= puzzle->getNumberOfProblems() || solvingProblem(prob))
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

  std::unique_ptr<Fl_Double_Window> w = std::make_unique<Fl_Double_Window>(20, 20, 300, 30);
  Fl_Box * b = new Fl_Box(0, 0, 300, 30);
  w->end();
  w->label("Disassembling...");
  w->set_modal();
  char txt[100];
  w->show();

  for (unsigned int sol = 0; sol < pr->getNumberOfSavedSolutions(); sol++) {

    snprintf(txt, 100, "solved %u of %u disassemblies\n", sol, pr->getNumberOfSavedSolutions());
    b->label(txt);

    Fl::wait(0);

    if (all || !pr->getSavedSolution(sol)->getDisassembly()) {

      std::unique_ptr<separation_c> d = dis->disassemble(pr->getSavedSolution(sol)->getAssembly());

      if (d)
        pr->getSavedSolution(sol)->setDisassembly(std::move(d));
    }
  }

  w.reset();

  activateSolution(prob, (int)SolutionSel->value()-1);
  updateInterface();
}


void cb_PcVis_stub(Fl_Widget* /*o*/, void* v) { static_cast<mainWindow_c*>(v)->cb_PcVis(); }
void mainWindow_c::cb_PcVis(void) {
  View3D->getView()->updateVisibility(PcVis);
}

void mainWindow_c::cb_Status(void) {
  View3D->getView()->showColors(puzzle, StatusLine->getColorMode());
  View3D->getView()->setRenderStyle(StatusLine->getRenderStyle());
  config.renderStyle(StatusLine->getRenderStyle());
}

void cb_3dClick_stub(Fl_Widget* /*o*/, void* v) { static_cast<mainWindow_c*>(v)->cb_3dClick(); }
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
