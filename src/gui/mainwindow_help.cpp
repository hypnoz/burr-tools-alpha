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

/* mainWindow_c, part: About, the tutorial and the solver help texts. */
#include "mainwindow.h"
#include "tutorials.h"
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


void cb_About_stub(Fl_Widget* /*o*/, void* v) { static_cast<mainWindow_c*>(v)->cb_About(); }
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


void cb_Tutorial_stub(Fl_Widget* /*o*/, void* v) { static_cast<mainWindow_c*>(v)->cb_Tutorial(); }
void cb_SolverTypeHelp_stub(Fl_Widget* /*o*/, void* v) { static_cast<mainWindow_c*>(v)->cb_SolverTypeHelp(); }
void cb_SolverType_stub(Fl_Widget* /*o*/, void* v) { static_cast<mainWindow_c*>(v)->cb_SolverType(); }
void cb_SortByHelp_stub(Fl_Widget* /*o*/, void* v) { static_cast<mainWindow_c*>(v)->cb_SortByHelp(); }
static void cb_SolverTypeHelpClose_stub(Fl_Widget* /*o*/, void* v) { static_cast<Fl_Double_Window*>(v)->hide(); }

void mainWindow_c::cb_Tutorial(void) {

  /* The tutorial for the kind of puzzle that is open, in the Tutorial tab. */
  openTutorial(puzzle->getGridType()->getType());
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
        "•  Gives up sooner on a puzzle with no solution, or on one with many starts to try. Most starts that cannot reach the goal are dropped quickly, at the risk of missing a solution that needs a long search.");
    (new LFl_Box(0, row++))->setMinimumSize(0, 16);

    addSolverHelpHeading(row++, "Sliding Deep Solver (1mil depth)");
    addSolverHelpBody(row++,
        "•  The same search, allowed up to 1,000,000 arrangements for each start.\n"
        "•  Solves larger puzzles, such as Panex Jr, that the fast solver runs out on. A puzzle the fast solver can solve takes no longer here.\n"
        "•  A search that runs out of arrangements cannot tell you the puzzle is impossible; this one runs out later.");
    (new LFl_Box(0, row++))->setMinimumSize(0, 16);

    addSolverHelpHeading(row++, "Sliding Full Solver (full depth)");
    addSolverHelpBody(row++,
        "•  The default. No limit on arrangements. It either finds the fewest moves or proves there is no solution. When a solution exists it takes no longer than the Fast or Deep Solver.\n"
        "•  Searches one move at a time, on every core. Only the newest moves stay in memory; everything older is written to a folder in this computer's cache, compressed, and deleted when the search ends. So it is limited by free disk space, not by memory: billions of arrangements are possible.\n"
        "•  It keeps 2 GB of the disk free, or a twentieth of it if that is more. Memory is needed only for the widest move: it stops if that would not fit in about 2 GB, or in half of this computer's memory with Enable High Memory.\n"
        "•  The Solver tab shows how many moves deep it is and how many arrangements it has searched, and Stop ends the search at any time.\n"
        "•  With many start layouts, every start that cannot reach the goal is searched to the end to prove it. The Fast or Deep Solver can finish such a puzzle sooner.");
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
