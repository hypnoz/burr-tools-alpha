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

static void cb_TutorialClose_stub(Fl_Widget* /*o*/, void* v) { static_cast<Fl_Double_Window*>(v)->hide(); }
void cb_Tutorial_stub(Fl_Widget* /*o*/, void* v) { static_cast<mainWindow_c*>(v)->cb_Tutorial(); }
void cb_SolverTypeHelp_stub(Fl_Widget* /*o*/, void* v) { static_cast<mainWindow_c*>(v)->cb_SolverTypeHelp(); }
void cb_SolverType_stub(Fl_Widget* /*o*/, void* v) { static_cast<mainWindow_c*>(v)->cb_SolverType(); }
void cb_SortByHelp_stub(Fl_Widget* /*o*/, void* v) { static_cast<mainWindow_c*>(v)->cb_SortByHelp(); }
static void cb_SolverTypeHelpClose_stub(Fl_Widget* /*o*/, void* v) { static_cast<Fl_Double_Window*>(v)->hide(); }

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
