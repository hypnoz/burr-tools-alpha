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
#include "tutorials.h"

#include "exampleimages.h"

#include <FL/Fl_PNG_Image.H>
#include <FL/Fl_Shared_Image.H>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace tutorials {

namespace {

/* The tutorials. A line starting "-- " is a heading; every other line is a
 * paragraph, wrapped to the width of the view. */

const char brickTutorial[] = R"TUTORIAL(This tutorial is for the Brick space grid, where every piece is built from cubes. It covers how to make and solve a puzzle. Each of the other space grids has a tutorial of its own: choose the grid in File, New and press Tutorial, or open Help, Tutorial while a puzzle of that grid is open.

-- Creating shapes:
The first step on the Entities tab is to create a new shape. This is done by clicking the "New" button. Below the shapes list, you will see options to set the X/Y/Z size and a grid to draw the piece. The "Colors" section at the bottom is more advanced, and allows restrictions for where pieces can go in the final solution. Once the correct X/Y/Z size is set, make sure the solid red square is selected. This is the normal "static" voxel, and pieces can only use this. The green square next to it is the "variable" voxel, which can only be used in the "solution" shape to allow either a piece or empty space to occupy that position. There are other buttons here to play with; the next most useful are the 3 at the right, which fill an entire row of X/Y/Z at a time. There are also tabs at the top to let you rotate or move shapes in the grid.
The grid below has a slider on the left to change the Z layer. You can think of this grid and slider like you have a puzzle directly in front of you: the grid is the voxels you see and the slider is the layers going back away from you. When the slider is at the bottom, it is the "back" layer of the puzzle, the furthest away. The top of the slider is the "front" layer, the closest to you. As you draw voxels in the grid they are always relative to this view.
If the puzzle has a box, tray, or frame, this object needs to also be made as a shape. BurrTools treats it as a piece of the puzzle while solving.
The final shape to create is the "solution" shape. This is the shape that matches the final assembled shape of the puzzle. This shape can use both static and variable voxels. Static requires that a piece be present in that spot. Variable can be occupied by either a piece or empty space.
After all shapes, frame/box/tray, and the solution shape are created, we will move on to building the puzzle.

-- Creating the puzzle:
The Puzzle tab is where we combine the shapes and solution to analyze as a puzzle. There may be many more shapes than you want to use in a single puzzle, so we will select a subset of pieces here. Press the "New" button to create a new puzzle, and you will be given the full list of shapes to choose from. First, select the final solution shape and then press the "Set Result" button. After that, we need to add all the shapes that will be used to make the puzzle. For each shape you want you can click it, then press the +1 button to add, or -1 to remove it. If you press +1 multiple times it will add multiple copies of the same shape. The "min=0" button will set the shape to be optional, so that either 0 or 1 or more copies can be used. You can create multiple puzzles with different sets of shapes. It's helpful to "Label" each puzzle with a name so you can easily tell them apart.

-- Solving the puzzle:
The Solver tab is where we solve the puzzle. You will see a list of puzzles you have created on the top left. There are many check boxes to change how the puzzle is solved. Each one has a hover tool tip to explain what it does. The primary one is "Disassemble", which will try to show only assemblies which can be disassembled. There are options for checking for rotations, and limiting to just a count or level without the solving animations. Level is the number of moves to remove the first piece from an assembled puzzle. Other checks will reduce or keep symmetric, mirrored or rotated solutions.
After choosing the solve options, press the "Solve" button. If everything was set up correctly, you will see a progress bar at the bottom of the screen. When the solving is complete, you will see a list of solutions. If something wasn't set up correctly, there will be an error about voxels missing from the final shape or too many, etc. Once the solve is completed, the number of Assemblies and Solutions will be displayed. The "Assemblies" count is the number of unique assemblies that can be made from the puzzle. The "Solutions" count is the number of those assemblies which can actually be disassembled.
Only the top 100 solutions are kept, and there is a "Solution" slider to scroll through them. For each solution, you will see a "Move" slider. There is a list of numbers like 12 (5.3.2.1.1) which represents the total moves to disassemble the puzzle, and then inside the parentheses are the number of moves to remove each piece from the puzzle. If you drag the slider, you will see an animation of the puzzle being disassembled step by step, along with the step number on the left of the slider.
For now ignore the "Advanced Filters" buttons, but the list of pieces at the bottom is useful. Each one can be selected to turn that piece either into a wire frame, or totally invisible. It's very helpful to see other pieces that are obstructed from view.

-- More Advanced Topics:
The main things to learn from here are color constraints and groups. Color constraints are a way to restrict where pieces can go in the final solution. Groups are a way to group pieces together so they can be treated as a single piece. Color constraints are set in the Entities tab, by creating a new color at the bottom. When you add a voxel to a piece, it will have a small color indicator on it showing that color constraint is part of the voxel. Add the color to all the voxels in the piece, and then in the solution shape, add that same color constraint to where the piece must go.
Groups are set in the Puzzle tab, using the "Set Groups" button. If you want 2 pieces to be treated as a single piece, choose the "Add Group" button, then next to the two pieces put a number like 1 or 2 that is the same for both pieces. Back in the list of pieces, you will see a label like "G1(2)" which is the main group number and sub group number within that group.
For even more advanced topics or learning, visit the BurrTools documentation website at https://burrtools.sourceforge.net/gui-doc/toc.html. The documentation was written for an older version of BurrTools, but the concepts are still valid.
)TUTORIAL";

const char slidingTutorial[] = R"TUTORIAL(This tutorial is for the Sliding space grid. A sliding puzzle is a flat tray with pieces in it. The pieces start in one place and must be slid, one at a time, to their goal places without lifting them out of the tray. BurrTools finds the fewest moves, where one move is one piece going anywhere it can reach while the others stay put, around corners included.

-- Creating the pieces:
On the Entities tab, press "New" to add a shape for each piece. A sliding piece is flat: set its X and Y size, keep Z at 1, and draw its cells in the grid with the solid red square. Pieces that are copies of one another, such as four identical small squares, may be the same shape used several times; the solver treats copies as one, which makes the search much faster. Use "Label" to give each piece a name, as the names appear on the Start/Goal tab.

-- Creating the tray:
Press "New Start/Goal Positions" to add the tray. This is the board the pieces slide on, and it holds both where each piece starts and where it must end. Set its size on the Size tab. Then use the "Start/Goal" tab:
Walls: click cells to switch them between floor and wall. Pieces only move over floor. A variable cell (the green square) is a corridor: a piece may slide across it but may not stop on it.
Start: select a piece in the shape list, then click the tray where it should start. Click a placed start again to remove it. Every piece in the puzzle needs a start.
Goal: select a piece, then click where it must end. A piece without a goal may end anywhere, which is common: often only one piece has to reach a goal.
You can make several start/goal shapes for different challenges with the same pieces; each one becomes its own puzzle.

-- The puzzle:
The Puzzle tab lists one puzzle for each start/goal shape. BurrTools keeps it up to date as you place starts and goals: the pieces with a start are in it, and the tray is its result. You will rarely need to change anything here.

-- Solving the puzzle:
On the Solver tab, keep "Find Solutions" ticked and press "Solve". The Sliding Full Solver, the default, searches until it finds the fewest moves or proves there is no solution. It keeps older moves on disk, so a large search is limited by free disk space rather than memory. The Fast and Deep solvers stop after 250,000 or 1,000,000 arrangements; they can finish sooner on a puzzle with many start layouts, at the risk of missing a solution.
"Allow Nested Slides" lets a piece carry the pieces that sit inside it, such as a small piece in another piece's pocket. Pieces that only touch never move together. "Enable High Memory" gives the search more memory for its widest step.
While it runs, the Activity line shows how many moves deep the search is and how many arrangements it has seen. When it finishes, move the "Move" slider to watch the pieces slide from start to goal. If there is no solution, the note under the solver says whether that is proven or the search stopped at a limit.
)TUTORIAL";

const char stackingTutorial[] = R"TUTORIAL(This tutorial is for the Stacking space grid. A stacking puzzle is a set of discs on vertical rods, like the Tower of Hanoi or Panex. A move takes the top disc of one rod and puts it on top of another. BurrTools finds the fewest moves that take the start stacks to the goal stacks.

-- Creating the discs:
On the Entities tab, the disc list holds every disc. Press "New" to add a disc, then select it and set its "Size". The size decides how large the disc is drawn, and, when the rod set says size matters, which discs may sit on which: a disc may not go on a smaller one. Discs of the same size are copies of one another.

-- Creating the rods:
The "Rods" section holds rod sets. Press "New" to add one, and set:
How many rods: the pegs on the board.
Grow to the height of all pieces, or Defined height: whether a rod can hold every disc or is full at a given count.
Disc size matters: no large disc on a smaller one, as in the Tower of Hanoi.
Disc can only move over 1 rod: a disc may only go to a neighbouring rod.
Panex Style Columns: the rules of the Panex puzzle, where a disc of size n can sit at most n places below the top of its column. "Add pocket column" adds the extra column some Panex variants have.

-- The puzzle:
On the Puzzle tab, press "New" to make a puzzle, choose its rod set, and add the discs it uses. Under "Stack Order", choose "Start", pick a rod, and order the discs that start on it from bottom to top; then do the same with "Goal" for where they must end. The 3D view shows the rods with the stacks.

-- Solving the puzzle:
On the Solver tab, keep "Find Solutions" ticked and press "Solve". The Stacking Solver finds the fewest moves. For rod sets with Panex Style Columns, the Panex Solver is used: it is built for large towers, searches from both ends at once, uses every core, and saves its search so that it can be continued after quitting. "Enable High Memory" lets either solver hold more in memory. When it finishes, move the "Move" slider to watch the discs move.
)TUTORIAL";

/* The cube-like grids share one tutorial, with a few words of their own. */
const char cellGridTutorial[] = R"TUTORIAL(-- Creating shapes:
On the Entities tab, press "New" to add a shape for each piece. Set its size on the Size tab and draw its cells in the grid editor with the solid red square, the normal "static" cell. The green square is the "variable" cell, which only the solution shape uses: a place that a piece may fill or leave empty. The grid editor shows one layer at a time; the slider beside it moves between layers, and the 3D view shows the whole shape.
If the puzzle has a box, tray or frame, make it a shape too: BurrTools treats it as one of the pieces.
Last, make the "solution" shape, the shape of the assembled puzzle.

-- Creating the puzzle:
On the Puzzle tab, press "New" to make a puzzle. Select the solution shape and press "Set Result". Then select each piece and press "+1" for every copy the puzzle has. "min=0" makes a piece optional. Use "Label" to name the puzzle.

-- Solving the puzzle:
On the Solver tab, choose the puzzle and press "Solve". The assembler finds every way the pieces fill the solution shape. The "Assemblies" count shows how many it found, and the Solution slider shows each one in the 3D view. Colour constraints, groups and the other options work as they do for the Brick grid; see the Brick tutorial for them.
)TUTORIAL";

struct gridInfo_c {
  gridType_c::gridType type;
  const char * name;
  /* The selector's description, and a warning or nullptr. */
  const char * description;
  const char * warning;
  /* The picture in exampleimages.h, the puzzle it shows, and that
   * puzzle's file in the examples folder. */
  const char * picture;
  const char * pictureOf;
  const char * file;
  /* For the grids that share cellGridTutorial: what is particular to it. */
  const char * intro;
};

const char noDisassembler[] =
    "This space grid has no disassembler. The solver finds every assembly, every way the pieces fill "
    "the result, but cannot check whether an assembly can be taken apart, or show how. Untick "
    "Disassemble on the Solver tab before solving; the Solutions count then stays empty and the "
    "assemblies are what you get.";

const gridInfo_c grids[] = {
  {gridType_c::GT_BRICKS, "Brick",
   "Pieces are made of cubes. This is the usual interlocking burr or packing puzzle: BurrTools finds "
   "every way the pieces fill the result shape, then works out which of those can really be taken "
   "apart, and how.",
   nullptr, "brick", "Cube in Cage by Mineyuki Uyematsu", "CubeInCage.xmpuzzle", nullptr},
  {gridType_c::GT_SLIDING, "Sliding",
   "Pieces are flat shapes in a tray. Mark where each piece starts and where it must end, and the "
   "solver finds the fewest moves that slide them from start to goal, one piece at a time. Pieces "
   "can carry others nested inside them when that is allowed.",
   nullptr, "sliding", "Sliding puzzle by GiiKER Super Slide", "SliderByGiiker.xmpuzzle", nullptr},
  {gridType_c::GT_STACKING, "Stacking",
   "Pieces are discs on vertical rods. A move takes the top disc of one rod and puts it on another, "
   "as in the Tower of Hanoi or Panex. Rules such as no large disc on a smaller one are set for each "
   "rod set, and the solver finds the fewest moves from the start stacks to the goal stacks.",
   nullptr, "stacking", "Panex Level 8 by Toshio Akanuma", "PanexLevel3to10.xmpuzzle", nullptr},
  {gridType_c::GT_TRIANGULAR_PRISM, "Triangular Prism",
   "Like Brick, with a different cell. Each cell is a prism with a triangular base, and the cells of "
   "a layer are triangles, half pointing up and half pointing down. Layers stack on top of one "
   "another. Pieces are found, and taken apart, as for Brick.",
   nullptr, "prism", "Prisgon by Markus G\xC3\xB6tz", "Prisgon.xmpuzzle",
   "This tutorial is for the Triangular Prism space grid. It works like the Brick grid, with a "
   "different cell: each cell is a prism with a triangular base. In the grid editor a layer is a "
   "field of triangles, half pointing up and half pointing down; click a triangle to fill it. The "
   "layers stack straight on top of one another. The solver takes pieces apart as it does for Brick, "
   "so Disassemble works."},
  {gridType_c::GT_SPHERES, "Spheres",
   "Pieces are made of balls. The balls sit in the tightest packing there is, each layer resting in "
   "the hollows of the one below, as oranges are stacked at a market. Good for pyramid and ball "
   "packing puzzles.",
   noDisassembler, "spheres", "Ball Room by Stewart Coffin", "BallRoom.xmpuzzle",
   "This tutorial is for the Spheres space grid, where pieces are made of balls. The balls sit in "
   "the tightest packing: each layer rests in the hollows of the one below. The grid editor "
   "shows one layer at a time and the slider moves between layers. Layers are offset from one "
   "another, so check the 3D view as you draw. This grid has no disassembler: untick Disassemble on the "
   "Solver tab before solving. The solver then finds every assembly, but cannot tell whether one "
   "can be taken apart."},
  {gridType_c::GT_RHOMBIC, "Rhombic",
   "Pieces are made of small tetrahedra cut from cubes, so that they build rhombic dodecahedra, "
   "stellations and other shapes with faces that are not square.",
   noDisassembler, "rhombic", "Diagonal Cube by Stewart Coffin", "DiagonalCube.xmpuzzle",
   "This tutorial is for the Rhombic space grid. Each cube of the grid is cut into small "
   "tetrahedra, and pieces are made of these, so that they can build rhombic dodecahedra, "
   "stellations and other shapes whose faces are not square. In the grid editor each cube shows "
   "its cells as triangles; it takes some practice, so build pieces up slowly and check them in "
   "the 3D view. This grid has no disassembler: untick Disassemble on the Solver tab before "
   "solving. The solver then finds every assembly, but cannot tell whether one can be taken apart."},
  {gridType_c::GT_TETRA_OCTA, "Tetrahedra-Octahedra",
   "Pieces are made of tetrahedra and octahedra, which together fill space. Good for pyramid, "
   "tetrahedron and octahedron shaped puzzles. A piece may use both kinds of cell.",
   noDisassembler, "tetraocta", "Four Piece Tetrahedron by Wayne Daniel", "FourPieceTetrahedron.xmpuzzle",
   "This tutorial is for the Tetrahedra-Octahedra space grid. Its cells are tetrahedra and "
   "octahedra, which fill space together, and a piece may use both. It is the grid for pyramids, "
   "tetrahedra and octahedra built from such pieces. In the grid editor each cube of the grid "
   "shows the cells cut from it; check the 3D view as you draw. This grid has no disassembler: "
   "untick Disassemble on the Solver tab before solving. The solver then finds every assembly, "
   "but cannot tell whether one can be taken apart."},
};

const gridInfo_c * infoFor(gridType_c::gridType type) {
  for (const gridInfo_c & g : grids)
    if (g.type == type)
      return &g;
  return &grids[0];
}

void appendEscaped(std::string & out, const char * s, size_t n) {
  for (size_t i = 0; i < n; i++) {
    switch (s[i]) {
      case '&': out += "&amp;"; break;
      case '<': out += "&lt;"; break;
      case '>': out += "&gt;"; break;
      default:  out += s[i]; break;
    }
  }
}

void appendEscaped(std::string & out, const char * s) {
  appendEscaped(out, s, std::strlen(s));
}

/* The markup of a tutorial: "-- " headings and paragraphs. */
void appendTutorialText(std::string & html, const char * text) {
  const char * p = text;
  while (*p) {
    const char * eol = std::strchr(p, '\n');
    const size_t len = eol ? (size_t)(eol - p) : std::strlen(p);
    if (len >= 3 && p[0] == '-' && p[1] == '-' && p[2] == ' ') {
      html += "<p><font size=\"5\"><b>";
      appendEscaped(html, p + 3, len - 3);
      html += "</b></font></p>";
    } else if (len > 0) {
      html += "<p>";
      appendEscaped(html, p, len);
      html += "</p>";
    }
    if (!eol)
      break;
    p = eol + 1;
  }
}

/* Fl_Help_View sizes a font as the text size times 1.2 to the power of
 * (size - 3), and takes a fraction: the size that adds `points`. */
std::string sizeFor(int fontSize, int points) {
  const double s = 3.0 + std::log((fontSize + points + 0.3) / (double)fontSize) / std::log(1.2);
  char buf[16];
  snprintf(buf, sizeof(buf), "%.3f", s);
  return buf;
}

} // namespace

const char * gridName(gridType_c::gridType type) {
  return infoFor(type)->name;
}

std::string tutorialHtml(gridType_c::gridType type) {
  const gridInfo_c * g = infoFor(type);
  std::string html = "<html><body>";
  html += "<p><font size=\"6\"><b><u>BurrTools ";
  appendEscaped(html, g->name);
  html += " Tutorial</u></b></font></p>";
  switch (type) {
    case gridType_c::GT_BRICKS:
      appendTutorialText(html, brickTutorial);
      break;
    case gridType_c::GT_SLIDING:
      appendTutorialText(html, slidingTutorial);
      break;
    case gridType_c::GT_STACKING:
      appendTutorialText(html, stackingTutorial);
      break;
    default:
      html += "<p>";
      appendEscaped(html, g->intro ? g->intro : "");
      html += "</p>";
      appendTutorialText(html, cellGridTutorial);
      break;
  }
  html += "</body></html>";
  return html;
}

std::string selectorHtml(gridType_c::gridType type, int fontSize, int maxW, int maxH) {
  const gridInfo_c * g = infoFor(type);
  const std::string label = sizeFor(fontSize, 2);
  std::string html = "<html><body>";

  html += "<p><font size=\"" + label + "\" color=\"#2E8B2E\"><b>Description:</b></font><br>";
  appendEscaped(html, g->description);
  html += "</p>";

  if (g->warning) {
    html += "<p><font size=\"" + label + "\" color=\"#D9731A\"><b>Warning:</b></font><br>";
    appendEscaped(html, g->warning);
    html += "</p>";
  }

  int w = 0, h = 0;
  const std::string imageFile = examplePicture(type, &w, &h);
  if (!imageFile.empty() && w > 0 && h > 0) {
    /* As large as fits, never larger than drawn. */
    const double scale = std::min(1.0, std::min((double)maxW / w, (double)maxH / h));
    char size[64];
    snprintf(size, sizeof(size), "width=\"%d\" height=\"%d\"", (int)(w * scale), (int)(h * scale));
    html += "<p><font size=\"" + label + "\" color=\"#2F6FD0\"><b>Example:</b></font><br>";
    appendEscaped(html, g->pictureOf);
    if (g->file) {
      html += " (<a href=\"" + std::string(EXAMPLE_LINK) + g->file + "\">open file</a>)";
    }
    html += "</p>";
    /* the wide, low Panex picture looks crowded against the text without a
     * gap; Fl_Help_View gives an empty line no height, so the gap holds spaces */
    if (type == gridType_c::GT_STACKING)
      html += "<p>&nbsp;<br>&nbsp;</p>";
    html += "<p><center><img src=\"" + imageFile + "\" " + size + "></center></p>";
  }

  html += "</body></html>";
  return html;
}

const char * exampleFile(gridType_c::gridType type) {
  return infoFor(type)->file;
}

const char * pictureName(gridType_c::gridType type) {
  return infoFor(type)->picture;
}

std::string examplePicture(gridType_c::gridType type, int * w, int * h) {
  const char * name = infoFor(type)->picture;
  if (!name)
    return "";
  /* Fl_Help_View finds an image by the path in <img src>, and looks in
   * the cache of shared images first: a name that looks like a path keeps
   * it from adding the current folder. */
  const std::string path = std::string("/burrtools-examples/") + name + ".png";
  if (Fl_Shared_Image * known = Fl_Shared_Image::find(path.c_str())) {
    if (w) *w = known->w();
    if (h) *h = known->h();
    known->release();
    return path;
  }
  for (unsigned int i = 0; i < exampleImages::count; i++) {
    const exampleImages::file_c & f = exampleImages::files[i];
    if (std::strcmp(f.name, name) != 0)
      continue;
    Fl_PNG_Image * png = new Fl_PNG_Image(path.c_str(), f.data, (int)f.size);
    if (png->fail()) {
      delete png;
      return "";
    }
    if (w) *w = png->w();
    if (h) *h = png->h();
    /* The cache owns it from here, for as long as the program runs. */
    Fl_Shared_Image::get(png, 1);
    return path;
  }
  return "";
}

} // namespace tutorials
