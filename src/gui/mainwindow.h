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

#include <vector>

class VoxelEditGroup_c;
class RodIndexBar_c;
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
  char * fname;
  disasmToMoves_c * disassemble;
  solveThread_c *assmThread;
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
  Fl_Group *solverPane;
  debugStatsPanel_c *debugPanel;
  solveStats_c lastSolveStats;
  Fl_Check_Button *SolveDisasm, *CheckRotations, *JustCount, *DropDisassemblies, *KeepMirrors, *KeepRotations, *StrictColors, *CompleteRotations;

  FlatButton *BtnPrepare, *BtnStart, *BtnCont, *BtnStop, *BtnPlacement, *BtnStep, *BtnMovement;
  FlatButton *BtnNewShape, *BtnDelShape, *BtnCpyShape, *BtnRenShape, *BtnUndo, *BtnRedo, *BtnShapeLeft, *BtnShapeRight, *BtnWeightInc, *BtnWeightDec, *BtnDetails;
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
  static Fl_Menu_Item menu_MainMenu[];

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
  bool solverMenuStacking;

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
  void hideDebugRightPane(void);


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
  void selectEntitiesTab(bool resetZoom = false);
  void updateUndoRedoButtons(void);
  void recordShapeAction(int kind);
  void applyHistoryRestore(unsigned int selected);

public:

  mainWindow_c(gridType_c * gt);
  virtual ~mainWindow_c();

  int handle(int event);

  using LFl_Double_Window::show;
  void show(int argn, char ** argv);

  void openFromSystem(const char * filename);

  // overwrite hide to check for changes in all possible exit situations
  void hide(void);

  /* this is used on assert to save the current puzzle */
  const puzzle_c * getPuzzle(void) const { return puzzle; }

  /* update the interface to represent the latest state of
   * the solving progress, that works in background
   */
  void update(void);

  /* return an index into the main menu array with the given text */
  static int findMenuEntry(const char * txt);
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
  void cb_BtnCont(bool prep_only);
  void cb_BtnStop(void);
  void cb_BtnPlacementBrowser(void);
  void cb_BtnMovementBrowser(void);
  void cb_BtnAssemblerStep(void);

  void cb_SolutionSel(Fl_Value_Slider*);
  void cb_SolutionAnim(Fl_Value_Slider*);
  void cb_SolverOptions(Fl_Widget* o);
  void updateSolverOptionCheckboxes(void);

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
  void cb_SolverTypeHelp(void);
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
  void cb_Export_Scad(void);
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
