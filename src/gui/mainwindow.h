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
#ifndef __MAINWINDOW_H__
#define __MAINWINDOW_H__

#include "Images.h"

#include "Layouter.h"
#include "../lib/solvethread.h"
#include "../lib/stacking.h"

#include <chrono>
#include <memory>
#include <string>
#include <vector>

class VoxelEditGroup_c;
class RodIndexBar_c;
class solution_c;
class StackValidBar_c;
class ChangeSize;
class ToolTab;
class puzzle_c;
class solveThread_c;
class disasmToMoves_c;
class gridType_c;
class guiGridType_c;
class layouter_c;

class PieceSelector;
class ProblemSelector;
class RodSelector;
class ColorSelector;
class ResultViewer_c;
class PiecesList;
class RodStackList;
class DiskSelector;
class PieceVisibility;
class ColorConstraintsEdit;
class ToolTabContainer;
class ButtonGroup_c;
class FlatButton;
class LStatusLine;
class LBlockListGroup_c;
class LView3dGroup;
class LFlatButton_c;
class LFl_Text_Editor;
class LFl_Button;
class statusWindow_c;
class debugStatsPanel_c;
class gridTypeSelectorWindow_c;
class LFl_Tile;
class voxel_c;

class shapeHistory_c;
class Fl_Tabs;
class Fl_Group;
class Fl_Check_Button;
class Fl_Value_Output;
class Fl_Output;
class Fl_Value_Slider;
class Fl_Value_Input;
class Fl_Menu_Bar;
class Fl_Choice;
class Fl_Progress;

class mainWindow_c : public LFl_Double_Window {

  puzzle_c * puzzle;
  shapeHistory_c * shapeHistory;
  guiGridType_c * ggt;  // this is the guigridtype for the puzzle, is must always be in sync
  /** The puzzle's file; empty for one never saved. */
  std::string fname;
  /** Remember the puzzle's file and name the window after it. */
  void setFileName(const std::string & f);
  std::unique_ptr<disasmToMoves_c> disassemble;
  /* The solution the Solver tab shows. Held so that a running solve that
   * drops it from the list cannot free what the views still draw. */
  std::shared_ptr<const solution_c> shownSolution;
  std::unique_ptr<solveThread_c> assmThread;
  bool SolutionEmpty;
  bool changed;
  int editSymmetries;

  bool handlingSystemOpen;
  bool menuExportActive;
  bool menuSTLActive;

  bool expertMode;

  pixmapList_c pm;

  Fl_Tabs *TaskSelectionTab;
  layouter_c *TabPieces;
  layouter_c *TabRods;
  Fl_Group *MinSizeSelector;

  PieceSelector * PcSel;
  ProblemSelector * problemSelector;
  ProblemSelector * solutionProblem;
  ColorSelector * colorAssignmentSelector;
  PieceSelector * shapeAssignmentSelector;
  ResultViewer_c * problemResult;
  PiecesList * PiecesCountList;
  PieceVisibility * PcVis;
  ColorConstraintsEdit * colconstrList;

  layouter_c *TabProblems;

  LBlockListGroup_c * pieceSelGroup;
  DiskSelector * diskList;
  layouter_c * rodsPanel;

  ToolTabContainer * pieceTools;
  ButtonGroup_c *editChoice;
  ButtonGroup_c *editMode;

  layouter_c *TabSolve;
  layouter_c *TabDebug;
  /* The Tutorial tab, after Solver while a tutorial is open, and the
   * tutorial text that takes the 3D view's place on the right. */
  layouter_c *TabTutorial;
  LFl_Help_View *tutorialPanel;
  LFl_Help_View *tutorialNote;
  /* The tab to go back to when the tutorial is closed. */
  Fl_Widget *tabBeforeTutorial;
  /* File, New's grid selector while it is open. */
  gridTypeSelectorWindow_c *gridSelector;
  Fl_Group *solverPane;
  debugStatsPanel_c *debugPanel;
  solveStats_c lastSolveStats;
  Fl_Check_Button *SolveDisasm, *CheckRotations, *JustCount, *DropDisassemblies, *KeepMirrors, *KeepRotations, *StrictColors, *CompleteRotations;
  /* Sliding only. It takes the place of Just Levels. */
  Fl_Check_Button *NestedSlides;
  Fl_Check_Button *PartialNested;
  Fl_Check_Button *HighMemory;
  Fl_Check_Button *Autosave;
  /* Autosave of brick and sliding solves: when the solve last started or
   * saved, and whether a pause for saving is under way. */
  std::chrono::steady_clock::time_point autosaveFrom;
  bool autosaving = false;
  unsigned int autosaveProblem = 0;
  std::string autosavePath(void) const;
  std::string autosavePathFor(const std::string & file) const;
  bool writeAutosave(void);
  /* Save the puzzle to path through a temporary file, so a failed save
   * never leaves a half-written file in its place. */
  bool writePuzzleFile(const std::string & path);
  /* A solve is running on this problem: its solution list is changing. */
  bool solvingProblem(unsigned int prob) const;
  void removeAutosave(void);

  FlatButton *BtnPrepare, *BtnStart, *BtnCont, *BtnStop, *BtnPlacement, *BtnStep, *BtnMovement;
  FlatButton *BtnAbort = nullptr;
  FlatButton *BtnNewShape, *BtnDelShape, *BtnCpyShape, *BtnRenShape, *BtnUndo, *BtnRedo, *BtnShapeLeft, *BtnShapeRight, *BtnWeightInc, *BtnWeightDec, *BtnDetails, *BtnShapeColor;
  FlatButton *BtnNewStartGoal, *BtnDelStartGoal;
  Fl_Widget *startGoalRow, *startGoalGap;
  FlatButton *BtnNewColor, *BtnDelColor, *BtnChnColor;
  FlatButton *BtnNewProb, *BtnDelProb, *BtnCpyProb, *BtnRenProb, *BtnProbLeft, *BtnProbRight;
  FlatButton *BtnColSrtPc, *BtnColSrtRes, *BtnColAdd, *BtnColRem;
  FlatButton *BtnSetResult, *BtnAddShape, *BtnRemShape, *BtnMinZero, *BtnAddAll, *BtnRemAll, *BtnSetAllRange, *BtnGroup, *BtnProbShapeLeft, *BtnProbShapeRight;

  Fl_Progress *SolvingProgress;
  Fl_Value_Output *OutputAssemblies;
  Fl_Value_Output *OutputSolutions;
  Fl_Output *OutputActivity;
  Fl_Check_Button *ReducePositions;
  Fl_Value_Slider *SolutionSel;
  Fl_Value_Slider *SolutionAnim;
  Fl_Value_Output *SolutionsInfo;
  Fl_Output *MovesInfo;

  Fl_Output *TimeUsed, *TimeEst;

  LView3dGroup * View3D;
  layouter_c * view3DStack;
  LFl_Tile * rightPane;
  statusWindow_c * detailsPanel;

  Fl_Group *MinSizeTools;
  Fl_Menu_Bar *MainMenu;
  layouter_c *notesPanel;
  LFl_Tile *contentTile;
  LFl_Text_Editor *notesInput;
  LFl_Button *notesUpdate;
  LFl_Button *notesRevert;
  LStatusLine *StatusLine;

  ColorSelector * colorSelector;

  VoxelEditGroup_c *pieceEdit;

  /** Entities "Colors" and Puzzle "Colour Assignment" groups; hidden for Sliding. */
  layouter_c * colorsGroup;
  layouter_c * colourAssignmentGroup;
  layouter_c * colourConstraintsGroup;

  /** Sliding tray edit mode: 0 = Start labels, 1 = Goal labels. */
  int slidingEditMode;

  /** Sliding starts with Disassemble checked. Later clicks are kept. */
  bool slidingDisasmDefaulted;
  /** Stacking starts with Find Solutions checked. Later clicks are kept. */
  bool stackingDisasmDefaulted;
  /** Solver Type menu is showing only Stacking Solver. */
  /* Which Solver Type list is loaded: 0 the grid solvers, 1 stacking, 2 sliding. */
  int solverMenuMode;

  void applySlidingGridMode(void);

  layouter_c * shapeEditColumn;
  layouter_c * voxelPenRow;
  LFl_Box * voxelToolGap;
  LFl_Box * voxelEditGap;
  LFl_Tabs * discTabs;
  LFl_Box * discInfo;
  LFl_Value_Input * diskSizeInput;
  bool diskSizeGuard;

  RodSelector * rodSel;
  RodSelector * rodAssignSel;
  layouter_c * rodAssignGroup;
  LFl_Value_Input * rodCountInput;
  LFl_Radio_Button * rodGrow;
  LFl_Radio_Button * rodFixed;
  LFl_Value_Input * rodHeightInput;
  LFl_Check_Button * rodSizeMatters;
  LFl_Check_Button * rodDistance;
  LFl_Check_Button * rodPanex;
  LFl_Check_Button * rodPocket;
  LFl_Value_Input * rodPocketHeight;
  LFl_Check_Button * rodChannel;
  bool rodFieldGuard;

  FlatButton *BtnNewRod, *BtnDelRod, *BtnCpyRod, *BtnRenRod, *BtnRodLeft, *BtnRodRight, *BtnRodUndo, *BtnRodRedo;

  static const int STACK_ROD_BUTTONS = 16;
  LFl_Radio_Button * stackStartMode;
  LFl_Radio_Button * stackGoalMode;
  RodIndexBar_c * stackRodBar;
  layouter_c * stackSliderRow;
  layouter_c * stackModeRow;
  RodStackList * rodStackList;
  LBlockListGroup_c * piecesCountGroup;
  Fl_Widget * stackOrderSep;
  Fl_Widget * problemButtonRule;
  /* Green or red bar under min=0: whether the start and goal obey the rod set. */
  layouter_c * stackValidRow;
  StackValidBar_c * stackValidBar;
  /* Under the 3D view, while a sliding puzzle's start/goal shape is chosen
   * on the Entities tab: whether its starts and goals can be solved. */
  StackValidBar_c * slideValidBar;
public:
  void refreshSlideValid(void);
private:
  /* Gaps beside the shape order arrows in their stacking (left) and
   * classic (right end) places. Only one pair is shown at a time. */
  Fl_Widget * probArrowGapL[2];
  Fl_Widget * probArrowGapR[2];
  int probArrowRightX;
  bool probArrowsStacking;
  /* Place the Puzzle tab's shape order arrows for the mode. True when moved. */
  bool syncProbArrows(bool stackingMode);
  bool stackingEditGoal;

  struct rodSnap_c {
    std::vector<stacking::rodSet_c> sets;
    struct pm_c {
      unsigned int id = 0xFFFFFFFFu;
      stacking::stackMap_c start;
      stacking::stackMap_c goal;
    };
    std::vector<pm_c> problems;
    unsigned int sel = 0;
  };
  std::vector<rodSnap_c> rodPast;
  std::vector<rodSnap_c> rodFuture;

  void syncStackingChrome(void);
  void syncSolverTypeMenu(void);
  void loadRodFields(void);
  void showSelectedRods(void);
  void selectProblemRod(unsigned int prob);
  rodSnap_c captureRodSnap(void) const;
  void restoreRodSnap(const rodSnap_c & snap);
  void resetRodHistory(void);
  void pushRodHistory(void);

  Fl_Choice * solverTypeChoice;
  Fl_Choice * sortMethod;
  Fl_Value_Input *solDrop, *solLimit;

  Fl_Value_Output *SolutionNumber, *AssemblyNumber;
  Fl_Output *MovesMetric, *RotationsMetric;

  FlatButton *BtnSrtFind, *BtnSrtLevel, *BtnSrtMoves, *BtnSrtPieces;
  FlatButton *BtnDelAll, *BtnDelBefore, *BtnDelAt, *BtnDelAfter, *BtnDelDisasm;
  FlatButton *BtnDisasmDel, *BtnDisasmDelAll, *BtnDisasmAdd, *BtnDisasmAddAll, *BtnDisasmAddMissing;
  FlatButton *BtnExportSolutionSTL;

  // zoom for Entities, Puzzle, Solver, and Rods
  double ViewSizes[4];
  int currentTab;

  bool tryToLoad(const char *fname, bool * reportedError = 0);
  bool confirmDiscard(const char * action);

  void CreateShapeTab(void);
  void CreateProblemTab(void);
  void refreshDiskList(void);
  void refreshStackSlider(void);
  void refreshStackList(void);
  void refreshStackValid(void);
  /* Disassembly step for the move slider. A stacking slider counts
   * transfers, each of which is several animation steps. */
  float animStep(void) const;
  unsigned int selectedStackRod(void) const;
  bool stackingBoard(problem_c * pr);
  /* Reserve one unused copy and place it on the rod selected on the Puzzle tab. */
  std::string placeOneDisk(problem_c * pr, unsigned int shape);
  void CreateSolveTab(void);
  void CreateDebugTab(void);
  void attachSolverPane(Fl_Group *tab);
  void applyDebugTabVisibility(void);
  void updateDebugStats(void);
  void showDebugRightPane(void);
  /* Back to the 3D view on the right, from the debug panel or the tutorial. */
  void hideDebugRightPane(void);
  void CreateTutorialTab(void);
  void showTutorialRightPane(void);


  bool is3DViewBig;
  bool shapeEditorWithBig3DView;

  void Toggle3DView(void);
  void Big3DView(void);
  void Small3DView(void);

  void StatPieceInfo(unsigned int pc);
  void StatPieceInfo(unsigned int pc, bool withCoords, int x, int y, int z);
  void StatProblemInfo(unsigned int pr);

  void changeShape(unsigned int nr);
  void changeProblem(unsigned int nr);
  void changeColor(unsigned int nr);

  void ReplacePuzzle(puzzle_c * newPuzzle);

  void activateShape(unsigned int number);
  void activateProblem(unsigned int prob);
  void activateSolution(unsigned int prob, unsigned int num);
  void activateClear(void);

  bool threadStopped(void);

  void updateInterface(void);
  void updateEntitiesTab(bool slidingPuzzle);
  void updateSolverTab(bool stackingPuzzle, unsigned int prob);
  void selectEntitiesTab(bool resetZoom = false);
  double startZoom(void) const;
  void recordSearchStats(void);
  std::string finishedActivity(const problem_c & pr) const;
  void updateUndoRedoButtons(void);
  void recordShapeAction(int kind);
  void applyHistoryRestore(unsigned int selected);

public:

  mainWindow_c(gridType_c * gt);
  virtual ~mainWindow_c();

  int handle(int event);

  using LFl_Double_Window::show;
  // cppcheck-suppress duplInheritedMember
  void show(int argn, char ** argv);

  void openFromSystem(const char * filename);

  /* The tutorial of a grid type in the Tutorial tab, which opens after
   * Solver if it is not open, and is selected. */
  void openTutorial(int gridType);
  void closeTutorial(void);

  /* Shortly after start: File, New's grid selector, unless a puzzle was
   * opened from the command line or the system. */
  void startupChooseGrid(void);

  // overwrite hide to check for changes in all possible exit situations
  void hide(void);

  /* this is used on assert to save the current puzzle */
  const puzzle_c * getPuzzle(void) const { return puzzle; }

  /* update the interface to represent the latest state of
   * the solving progress, that works in background
   */
  void update(void);

  /* return an index into the main menu array with the given text */
  void initViewMenuIcons(void);

  /* the callback functions, as they are called from normal functions we need
   * to make them public, even though they should not be used from the outside
   */
  void cb_AddColor(void);
  void cb_RemoveColor(void);
  void cb_ChangeColor(void);

  void cb_NewShape(void);
  void cb_DeleteShape(void);
  void cb_CopyShape(void);
  void cb_NameShape(void);
  void cb_ShapeExchange(int with);
  void cb_WeightChange(int by);
  void cb_NewStartGoal(void);
  void cb_DelStartGoal(void);
  void cb_NewRod(void);
  void cb_DeleteRod(void);
  void cb_CopyRod(void);
  void cb_NameRod(void);
  void cb_RodExchange(int with);
  void cb_RodUndo(void);
  void cb_RodRedo(void);
  void cb_RodSel(void);
  void cb_RodField(void);
  void syncPanexFields(void);
  bool panexSelected(void) const;
  void cb_DiskList(void);
  void cb_StackMode(Fl_Widget * o);
  void cb_StackRod(void);
  void cb_ProblemRod(void);
  void relayoutProblemTab(void);
  void cb_StackListSel(void);
  void cb_Undo(void);
  void cb_Redo(void);

  void cb_NewProblem(void);
  void cb_DeleteProblem(void);
  void cb_CopyProblem(void);
  void cb_RenameProblem(void);
  void cb_ProblemExchange(int with);

  void cb_ColorAssSel(void);
  void cb_ColorConstrSel(void);

  void cb_ShapeToResult(void);

  void cb_TaskSelectionTab(Fl_Tabs*);

  void cb_SelectProblemShape(void);
  void cb_AddShapeToProblem(void);
  void cb_SetShapeMinimumToZero(void);
  void cb_AddAllShapesToProblem(void);
  void cb_RemoveShapeFromProblem(void);
  void cb_RemoveAllShapesFromProblem(void);
  void cb_SetAllRange(void);
  void cb_ProbShapeExchange(int with);

  void cb_PcSel(LBlockListGroup_c* reason);
  void cb_ColSel(LBlockListGroup_c* reason);
  void cb_ProbSel(LBlockListGroup_c* reason);

  void cb_PiecesClicked(void);

  void cb_TransformPiece(void);
  void cb_TransformPreview(voxel_c * preview, unsigned int shapeNum);
  void cb_pieceEdit(VoxelEditGroup_c* o);
  void cb_EditChoice(void);
  void cb_EditSym(int onoff, int value);
  void cb_EditMode(void);

  void cb_TransformResult(void);

  void cb_AllowColor(void);
  void cb_DisallowColor(void);
  void cb_CCSort(bool byResult);

  void cb_BtnPrepare(void);
  void cb_BtnStart(bool prep_only);
  void cb_BtnCont(bool prep_only, int forProblem = -1);
  void cb_BtnStop(void);
  /* Stop the selected problem's solve at once and throw away everything
   * kept of it: results, the paused state, a saved stacking search, the
   * autosave copy. */
  void cb_BtnAbort(void);
  /* Abort has something to throw away for this problem. */
  bool abortable(unsigned int prob) const;
  /* Whether the Abort button resets a problem whose solve is over (finished,
   * or its result unknown) instead of stopping one that is running or paused. */
  bool resettable(unsigned int prob) const;
  void setAbortLabel(unsigned int prob);
  void cb_BtnPlacementBrowser(void);
  void cb_BtnMovementBrowser(void);
  void cb_BtnAssemblerStep(void);

  void cb_SolutionSel(Fl_Value_Slider*);
  void cb_SolutionAnim(Fl_Value_Slider*);
  void cb_SolverOptions(Fl_Widget* o);
  void updateSolverOptionCheckboxes(void);
  /** Copy the Solver tab settings into the problem selected there. */
  void storeSolverOptions(void);
  /** Set the Solver tab to the settings saved with a problem, if it has any. */
  void applySolverOptions(unsigned int prob);
  /** The Solver tab checkbox saved under this name, or null. */
  Fl_Check_Button * solverOptionBox(const char * name);

  void cb_PcVis(void);

  void cb_Status(void);
  void cb_3dClick(void);

  void cb_New(void);
  void cb_Load(void);
  void cb_Load_Ps3d(void);
  void cb_Load_Scad(void);
  void cb_Save(void);
  void cb_SaveAs(void);
  void cb_Convert(void);
  void cb_AssembliesToShapes(void);
  void cb_Quit(void);
  void cb_About(void);
  void cb_Tutorial(void);
  void cb_ShapeColor(void);
  /* Give the piece colours the shapes' own colours, where they have one. */
  void syncPieceColors(void);
  void cb_SolverTypeHelp(void);
  void cb_SolverType(void) {
    updateSolverOptionCheckboxes();
    storeSolverOptions();
    updateInterface();
  }
  void cb_SortByHelp(void);
  void cb_Help(void);
  void cb_Config(void);
  void cb_ToggleNotes(void);
  void updateNotesMenuLabel(void);
  void cb_NotesUpdate(void);
  void cb_NotesRevert(void);
  void cb_NotesChanged(void);
  void setNotesButtonsEnabled(bool enabled);
  void relayoutViewStack(void);
  void cb_Toggle3D(void);
  void cb_ViewMode(int mode);
  void cb_RenderStyle(int mode);
  void syncRenderStyleMenu(void);
  void cb_SolProbSel(LBlockListGroup_c* reason);

  void cb_ShapeGroup(void);
  void cb_ImageExport(void);
  void cb_ImageExportVector(void);
  void cb_STLExport(void);
  void cb_ExportGltf(void);
  void cb_Export_Scad(void);
  void cb_ExportPaused(void);
  void cb_ImportPaused(void);
  /** The problem has a paused solve, and no solve is running. */
  bool problemPaused(unsigned int prob) const;
  void cb_StatusWindow(void);
  void cb_DetailsClose(void);
  void cb_DetailsChanged(void);

  void cb_SortSolutions(unsigned int by);
  void cb_SortMethod(void);
  void cb_DeleteSolutions(unsigned int which);

  void cb_DeleteDisasm(void);
  void cb_DeleteAllDisasm(void);
  void cb_AddDisasm(void);
  void cb_AddAllDisasm(bool all);
  void cb_ExportSolutionSTL(void);

  void activateConfigOptions(void);
};

#endif
