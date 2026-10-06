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
 * burrtools --render-example puzzle.xmpuzzle out.png [corner]
 *
 * Draws a puzzle the way the 3D view shows it and saves the picture: the
 * example pictures of the grid selector (src/gui/images/examples) are made
 * with it. The first solution when there is one -- for a sliding or
 * stacking puzzle its start -- else the result shape. The view is from a
 * corner of the view cube, 0 to 7 (default 0: Front, Top and Right).
 */
#define GL_SILENCE_DEPRECATION 1
#include "examplerender.h"

#include "image.h"
#include "view3dgroup.h"
#include "voxelframe.h"

#include "../lib/problem.h"
#include "../lib/puzzle.h"
#include "../lib/sliding.h"
#include "../lib/stacking.h"

#include <FL/Fl.H>
#include <FL/gl.h>

#include <cstdio>
#include <memory>

namespace {

/* Takes the picture tile by tile as the view draws, as Image Export does. */
class grabber_c : public VoxelViewCallbacks {
public:
  grabber_c(voxelFrame_c * v, image_c * i) : view(v), img(i) {}

  bool PreDraw(void) override {
    if (done)
      return false;
    img->prepareOpenGlImagePart(view);
    glClearColor(1, 1, 1, 0);
    return true;
  }

  void PostDraw(void) override {
    if (!done && !img->getOpenGlImagePart())
      done = true;
  }

  bool finished(void) const { return done; }

private:
  voxelFrame_c * view;
  image_c * img;
  bool done = false;
};

} // namespace

int renderExample(const char * puzzleFile, const char * pngFile, int corner) {

  std::unique_ptr<puzzle_c> puzzle = puzzle_c::load(puzzleFile);
  if (!puzzle || puzzle->getNumberOfProblems() == 0) {
    fprintf(stderr, "could not load a puzzle with a problem from %s\n", puzzleFile);
    return 1;
  }
  const problem_c * prob = puzzle->getProblem(0);

  const int W = 720;
  const int H = 540;
  const int AA = 3;
  Fl_Double_Window win(W, H, "render example");
  LView3dGroup * group = new LView3dGroup(0, 0, 1, 1);
  group->resize(0, 0, W, H);
  win.end();
  win.show();
  voxelFrame_c * view = group->getView();
  view->setDrawViewCube(false);

  const bool stackingPuzzle = stacking::isStacking(*puzzle);
  if (prob->getNumberOfSavedSolutions() > 0)
    /* For a sliding or stacking puzzle, its start. */
    view->showAssembly(prob, 0);
  else if (stackingPuzzle)
    view->showStacking(prob, false);
  else if (prob->resultValid())
    view->showSingleShape(puzzle.get(), prob->getResultId());
  else
    view->showSingleShape(puzzle.get(), 0);
  view->showColors(puzzle.get(), voxelFrame_c::pieceColor);
  if (stackingPuzzle)
    view->setStackingView(true);
  else
    view->lookFromCorner(corner);
  /* A board of rods is wide: draw it further back, so all of it shows. */
  if (stackingPuzzle)
    group->setZoom(view->fitZoom());

  /* Let the window settle at its size before the picture is taken. */
  for (int i = 0; i < 20; i++)
    Fl::wait(0.02);

  image_c img(W * AA, H * AA);
  grabber_c grab(view, &img);
  view->setCallback(&grab);
  for (int guard = 0; !grab.finished() && guard < 1000; guard++) {
    view->redraw();
    Fl::flush();
    Fl::wait(0.01);
  }
  view->setCallback(nullptr);
  if (!grab.finished()) {
    fprintf(stderr, "the picture was not finished\n");
    return 1;
  }

  img.transparentize(255, 255, 255);
  img.minimizeWidth(AA * 8, AA);
  img.minimizeHeight(AA * 8, AA);
  img.scaleDown(AA);
  if (!img.saveToPNG(pngFile)) {
    fprintf(stderr, "could not write %s\n", pngFile);
    return 1;
  }
  printf("%s: %u x %u\n", pngFile, img.w(), img.h());
  return 0;
}
