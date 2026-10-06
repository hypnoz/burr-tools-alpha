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
/*
 * Helpers and menu/button callbacks that the mainwindow_*.cpp files share.
 * mainWindow_c is split over several files by area (see each file's top
 * comment); this header is for those files only.
 */
#ifndef __MAINWINDOW_INTERNAL_H__
#define __MAINWINDOW_INTERNAL_H__

#include "mainwindow.h"
#include "rodbars.h"

#include <FL/Fl_Menu_.H>
#include <FL/Fl_Output.H>

class layouter_c;
class LView3dGroup;
class solveThread_c;
class separation_c;
class disassembly_c;
class voxel_c;

bool fileExists(const char *n);

void cb_AddColor_stub(Fl_Widget* /*o*/, void* v);
void cb_RemoveColor_stub(Fl_Widget* /*o*/, void* v);
void cb_ChangeColor_stub(Fl_Widget* /*o*/, void* v);
void cb_NewShape_stub(Fl_Widget* /*o*/, void* v);
void cb_DeleteShape_stub(Fl_Widget* /*o*/, void* v);
void cb_CopyShape_stub(Fl_Widget* /*o*/, void* v);
void cb_NameShape_stub(Fl_Widget* /*o*/, void* v);
void cb_WeightInc_stub(Fl_Widget* /*o*/, void* v);
void cb_WeightDec_stub(Fl_Widget* /*o*/, void* v);
void cb_NewStartGoal_stub(Fl_Widget* /*o*/, void* v);
void cb_DelStartGoal_stub(Fl_Widget* /*o*/, void* v);
void cb_NewRod_stub(Fl_Widget* /*o*/, void* v);
void cb_DeleteRod_stub(Fl_Widget* /*o*/, void* v);
void cb_CopyRod_stub(Fl_Widget* /*o*/, void* v);
void cb_NameRod_stub(Fl_Widget* /*o*/, void* v);
void cb_RodLeft_stub(Fl_Widget* /*o*/, void* v);
void cb_RodRight_stub(Fl_Widget* /*o*/, void* v);
void cb_RodUndo_stub(Fl_Widget* /*o*/, void* v);
void cb_RodRedo_stub(Fl_Widget* /*o*/, void* v);
void cb_RodSel_stub(Fl_Widget* /*o*/, void* v);
void cb_RodField_stub(Fl_Widget* /*o*/, void* v);
void cb_DiskList_stub(Fl_Widget* /*o*/, void* v);
void cb_StackMode_stub(Fl_Widget* o, void* v);
void cb_StackRod_stub(Fl_Widget* /*o*/, void* v);
void cb_StackValidRelayout_stub(Fl_Widget* /*o*/, void* v);
void cb_ShapeColor_stub(Fl_Widget* /*o*/, void* v);
void cb_SlideValidRelayout_stub(Fl_Widget* /*o*/, void* v);
void cb_StackListSel_stub(Fl_Widget* /*o*/, void* v);
void cb_ProblemRod_stub(Fl_Widget* /*o*/, void* v);
void easeRodZoom(LView3dGroup * view);
void relayoutTab(layouter_c * tab);
void cb_TaskSelectionTab_stub(Fl_Widget* o, void* v);
void cb_TransformPiece_stub(Fl_Widget* /*o*/, void* v);
void cb_TransformPreview_stub(void* v, voxel_c* preview, unsigned int shapeNum);
void cb_EditSym_stub(Fl_Widget* o, void* v);
void cb_EditChoice_stub(Fl_Widget* /*o*/, void* v);
void cb_EditMode_stub(Fl_Widget* /*o*/, void* v);
void cb_PcSel_stub(Fl_Widget* o, void* v);
void cb_SolProbSel_stub(Fl_Widget* o, void* v);
void cb_ColSel_stub(Fl_Widget* o, void* v);
void cb_ProbSel_stub(Fl_Widget* o, void* v);
void cb_pieceEdit_stub(Fl_Widget* o, void* v);
void cb_NewProblem_stub(Fl_Widget* /*o*/, void* v);
void cb_DeleteProblem_stub(Fl_Widget* /*o*/, void* v);
void cb_CopyProblem_stub(Fl_Widget* /*o*/, void* v);
void cb_RenameProblem_stub(Fl_Widget* /*o*/, void* v);
void cb_ProblemLeft_stub(Fl_Widget* /*o*/, void* v);
void cb_ProblemRight_stub(Fl_Widget* /*o*/, void* v);
void cb_ShapeLeft_stub(Fl_Widget* /*o*/, void* v);
void cb_ShapeRight_stub(Fl_Widget* /*o*/, void* v);
void cb_ProbShapeLeft_stub(Fl_Widget* /*o*/, void* v);
void cb_ProbShapeRight_stub(Fl_Widget* /*o*/, void* v);
void cb_ColorAssSel_stub(Fl_Widget* /*o*/, void* v);
void cb_ColorConstrSel_stub(Fl_Widget* /*o*/, void* v);
void cb_ShapeToResult_stub(Fl_Widget* /*o*/, void* v);
void cb_ShapeSel_stub(Fl_Widget* /*o*/, void* v);
void cb_PiecesClicked_stub(Fl_Widget* /*o*/, void* v);
void cb_AddShapeToProblem_stub(Fl_Widget* /*o*/, void* v);
void cb_AddAllShapesToProblem_stub(Fl_Widget* /*o*/, void* v);
void cb_RemoveShapeFromProblem_stub(Fl_Widget* /*o*/, void* v);
void cb_SetShapeMinimumToZero_stub (Fl_Widget* /*o*/, void* v);
void cb_RemoveAllShapesFromProblem_stub(Fl_Widget* /*o*/, void* v);
void cb_SetAllRange_stub(Fl_Widget* /*o*/, void* v);
void cb_ShapeGroup_stub(Fl_Widget* /*o*/, void* v);
void cb_ExportSolutionSTL_stub(Fl_Widget* /*o*/, void* v);
void cb_BtnPlacementBrowser_stub(Fl_Widget* /*o*/, void* v);
void cb_BtnMovementBrowser_stub(Fl_Widget* /*o*/, void* v);
void cb_BtnAssemblerStep_stub(Fl_Widget* /*o*/, void* v);
void cb_AllowColor_stub(Fl_Widget* /*o*/, void* v);
void cb_DisallowColor_stub(Fl_Widget* /*o*/, void* v);
void cb_CCSortByResult_stub(Fl_Widget* /*o*/, void* v);
void cb_CCSortByPiece_stub(Fl_Widget* /*o*/, void* v);
void cb_BtnPrepare_stub(Fl_Widget* /*o*/, void* v);
void cb_BtnStart_stub(Fl_Widget* /*o*/, void* v);
void cb_BtnCont_stub(Fl_Widget* /*o*/, void* v);
void cb_BtnStop_stub(Fl_Widget* /*o*/, void* v);
void cb_BtnAbort_stub(Fl_Widget* /*o*/, void* v);
void cb_SolutionSel_stub(Fl_Widget* o, void* v);
void cb_SolutionAnim_stub(Fl_Widget* o, void* v);
void cb_SolverOptions_stub(Fl_Widget* o, void* v);
void cb_SrtFind_stub(Fl_Widget* /*o*/, void* v);
void cb_SrtLevel_stub(Fl_Widget* /*o*/, void* v);
void cb_SrtMoves_stub(Fl_Widget* /*o*/, void* v);
void cb_SrtPieces_stub(Fl_Widget* /*o*/, void* v);
void cb_SortMethod_stub(Fl_Widget* /*o*/, void* v);
void cb_DelAll_stub(Fl_Widget* /*o*/, void* v);
void cb_DelBefore_stub(Fl_Widget* /*o*/, void* v);
void cb_DelAt_stub(Fl_Widget* /*o*/, void* v);
void cb_DelAfter_stub(Fl_Widget* /*o*/, void* v);
void cb_DelDisasmless_stub(Fl_Widget* /*o*/, void* v);
void cb_DelDisasm_stub(Fl_Widget* /*o*/, void* v);
void cb_DelAllDisasm_stub(Fl_Widget* /*o*/, void* v);
void cb_AddDisasm_stub(Fl_Widget* /*o*/, void* v);
void cb_AddAllDisasm_stub(Fl_Widget* /*o*/, void* v);
void cb_AddMissingDisasm_stub(Fl_Widget* /*o*/, void* v);
void cb_PcVis_stub(Fl_Widget* /*o*/, void* v);
void cb_3dClick_stub(Fl_Widget* /*o*/, void* v);
void cb_NotesUpdate_stub(Fl_Widget* /*o*/, void* v);
void cb_NotesRevert_stub(Fl_Widget* /*o*/, void* v);
void cb_NotesChanged_stub(Fl_Widget* /*o*/, void* v);
void cb_DetailsClose_stub(Fl_Widget*, void* v);
void cb_DetailsChanged_stub(Fl_Widget*, void* v);
void cb_SolverTypeHelp_stub(Fl_Widget* /*o*/, void* v);
void cb_SolverType_stub(Fl_Widget* /*o*/, void* v);
void cb_SortByHelp_stub(Fl_Widget* /*o*/, void* v);

#endif
