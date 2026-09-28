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
#ifndef __TOOL_TABS__
#define __TOOL_TABS__

#include "Images.h"
#include "Layouter.h"

class ChangeSize;
class puzzle_c;
class guiGridType_c;
class voxel_c;
class Fl_Hold_Browser;

// the class that contains the tool tab
class ToolTab : public LFl_Tabs {

public:

  ToolTab(int x, int y, int w, int h) : LFl_Tabs(x, y, w, h), puzzle(0), shape(0) {}

  virtual void setVoxelSpace(puzzle_c * puz, unsigned int sh) = 0;
  bool operationToAll(void) { return toAll->value() != 0; }
  void previewTransform(long task, bool on);

protected:

  LFl_Check_Button * toAll;
  puzzle_c * puzzle;
  unsigned int shape;
  virtual void applyTask(voxel_c * space, long task) = 0;
};

// the class that contains the tool tab
class ToolTab_0 : public ToolTab {

  ChangeSize * changeSize;
  pixmapList_c pm;
  layouter_c * startGoalTab;
  Fl_Hold_Browser * pieceList;
  LFl_Radio_Button * modeStart;
  LFl_Radio_Button * modeGoal;
  unsigned int selectedSgPiece;
  bool modeCallbackPending;
  Fl_Widget * shownTab;

public:

  ToolTab_0(int x, int y, int w, int h);

  void setVoxelSpace(puzzle_c * puz, unsigned int sh);

  void cb_size(void);
  void cb_transform(long task);
  void cb_startGoalMode(void);
  void selectPiece(unsigned int shapeId);
  void applyTask(voxel_c * space, long task);

  /** 0 = Start, 1 = Goal. Only meaningful for a start/goal shape. */
  int startGoalMode(void) const;
  /** True while the Start/Goal tab itself is the selected tool tab. */
  bool labelEditActive(void);
  /** True once when the user has just switched tool tabs. Updates the remembered tab. */
  bool consumeTabChange(void);
  /** Shape index of the piece chosen in the Start/Goal list, or ~0u. */
  unsigned int selectedPiece(void) const;
  /** True once after a Start/Goal radio or piece-list callback. */
  bool takeModeCallback(void);
  void refreshPieceList(void);
};

// the class that contains the tool tab
class ToolTab_1 : public ToolTab {

  ChangeSize * changeSize;
  pixmapList_c pm;

public:

  ToolTab_1(int x, int y, int w, int h);

  void setVoxelSpace(puzzle_c * puz, unsigned int sh);

  void cb_size(void);
  void cb_transform(long task);
  void applyTask(voxel_c * space, long task);
};

// the class that contains the tool tab
class ToolTab_2 : public ToolTab {

  ChangeSize * changeSize;
  pixmapList_c pm;

public:

  ToolTab_2(int x, int y, int w, int h);

  void setVoxelSpace(puzzle_c * puz, unsigned int sh);

  void cb_size(void);
  void cb_transform(long task);
  void applyTask(voxel_c * space, long task);
};


class ToolTab_3 : public ToolTab {

  ChangeSize * changeSize;
  pixmapList_c pm;

public:

  ToolTab_3(int x, int y, int w, int h);

  void setVoxelSpace(puzzle_c * puz, unsigned int sh);

  void cb_size(void);
  void cb_transform(long task);
  void applyTask(voxel_c * space, long task);
};

class ToolTab_4 : public ToolTab {

  ChangeSize * changeSize;
  pixmapList_c pm;

public:

  ToolTab_4(int x, int y, int w, int h);

  void setVoxelSpace(puzzle_c * puz, unsigned int sh);

  void cb_size(void);
  void cb_transform(long task);
  void applyTask(voxel_c * space, long task);
};

/**
 * Tools for the Sliding tray grid: 2D size, and Walls / Start / Goal modes.
 * Mode changes fire do_callback with MODE_* as the long parameter.
 */
class ToolTab_Sliding : public ToolTab {

  ChangeSize * changeSize;
  LFl_Radio_Button * modeWalls;
  LFl_Radio_Button * modeStart;
  LFl_Radio_Button * modeGoal;

public:

  enum {
    MODE_WALLS = 0,
    MODE_START = 1,
    MODE_GOAL = 2
  };

  ToolTab_Sliding(int x, int y, int w, int h);

  void setVoxelSpace(puzzle_c * puz, unsigned int sh);

  void cb_size(void);
  void cb_mode(void);
  void applyTask(voxel_c * space, long task);

  int getMode(void) const;
};

class ToolTabContainer : public layouter_c {

  ToolTab * tt;
  void (*previewHandler)(void * user, voxel_c * preview, unsigned int shapeNum);
  void * previewUser;
  unsigned int delayedClearShape;
  static void previewClearTimeout(void * v);

  public:

  ToolTabContainer(int x, int y, int w, int h, const guiGridType_c * ggt);

  void setVoxelSpace(puzzle_c * puz, unsigned int sh) { if (tt) tt->setVoxelSpace(puz, sh); }
  bool operationToAll(void) { if (tt) return tt->operationToAll(); else return false; }
  ToolTab * getToolTab(void) { return tt; }
  void setPreviewHandler(void (*cb)(void * user, voxel_c * preview, unsigned int shapeNum), void * user) {
    previewHandler = cb;
    previewUser = user;
  }
  void emitPreview(voxel_c * preview, unsigned int shapeNum);

  void newGridType(const guiGridType_c * ggt);
};

#endif
