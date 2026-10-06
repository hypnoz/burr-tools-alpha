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

/* mainWindow_c, part: files: new, open, save, autosave, import and export, the paused solver state. */
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
#include "../lib/blockpack.h"
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
#include "gltfexportwindow.h"
#include "piececolor.h"

#include "LFl_Tile.h"

#include "../lib/ps3dloader.h"
#include "../lib/scadloader.h"
#include "../lib/voxel.h"
#include "../lib/puzzle.h"
#include "../lib/problem.h"
#include "../lib/assembler.h"
#include "../lib/solvethread.h"
#include "../lib/disassembly.h"
#include "../lib/solution.h"
#include "../lib/gltfexport.h"
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


bool mainWindow_c::confirmDiscard(const char * action) {

  if (!changed)
    return true;

  char msg[256];
  snprintf(msg, sizeof(msg),
           "The puzzle has unsaved changes.\nSave before you %s?", action);

  switch (fl_choice("%s", "Cancel", "Save", "Don't Save", msg)) {

    case 1:
      cb_Save();
      return !changed;

    case 2:
      return true;

    default:
      return false;
  }
}

void cb_New_stub(Fl_Widget* /*o*/, void* v) { static_cast<mainWindow_c*>(v)->cb_New(); }
void mainWindow_c::cb_New(void) {

  if (threadStopped()) {

    if (!confirmDiscard("create a new puzzle"))
      return;

    gridTypeSelectorWindow_c w;
    gridSelector = &w;
    w.show();

    while (w.visible())
      Fl::wait();
    gridSelector = 0;

    switch (w.result()) {
      case gridTypeSelectorWindow_c::SEL_CANCEL:
        return;
      case gridTypeSelectorWindow_c::SEL_OPEN:
        /* the question about unsaved changes is answered already */
        tryToLoad(bt_file_chooser_open("Open Puzzle", "Puzzle Files\t*.xmpuzzle", ""));
        return;
      case gridTypeSelectorWindow_c::SEL_OK:
      case gridTypeSelectorWindow_c::SEL_TUTORIAL:
        break;
    }

    std::unique_ptr<gridType_c> gt = w.getGridType();
    const int type = gt->getType();
    ReplacePuzzle(new puzzle_c(std::move(gt)));

    if (!fname.empty())
      setFileName("");

    changed = false;

    StatusLine->setText("");
    selectEntitiesTab(true);
    activateShape(0);

    if (w.result() == gridTypeSelectorWindow_c::SEL_TUTORIAL)
      openTutorial(type);
  }
}

void mainWindow_c::startupChooseGrid(void) {

  if (fname.empty() && !changed)
    cb_New();
}

void cb_Load_stub(Fl_Widget* /*o*/, void* v) { static_cast<mainWindow_c*>(v)->cb_Load(); }
void mainWindow_c::cb_Load(void) {

  if (threadStopped()) {

    if (!confirmDiscard("open another puzzle"))
      return;

    const char * f = bt_file_chooser_open("Open Puzzle", "Puzzle Files\t*.xmpuzzle", "");

    tryToLoad(f);
  }
}

static bool hasFileExtension(const char * path, const char * ext)
{
  if (!path || !ext)
    return false;

  size_t n = strlen(path);
  size_t e = strlen(ext);
  if (n < e)
    return false;

  const char * p = path + n - e;
  for (size_t i = 0; i < e; i++) {
    char a = p[i];
    char b = ext[i];
    if (a >= 'A' && a <= 'Z') a = (char)(a - 'A' + 'a');
    if (b >= 'A' && b <= 'Z') b = (char)(b - 'A' + 'a');
    if (a != b)
      return false;
  }
  return true;
}

void cb_Load_Ps3d_stub(Fl_Widget* /*o*/, void* v) { static_cast<mainWindow_c*>(v)->cb_Load_Ps3d(); }
void mainWindow_c::cb_Load_Ps3d(void) {

  if (threadStopped()) {

    if (!confirmDiscard("import another puzzle"))
      return;

    const char * f = bt_file_chooser_open("Import PuzzleSolver3D File",
                                          "PuzzleSolver3D Files\t*.puz",
                                          "");

    if (f) {

      std::ifstream in(f);
      puzzle_c * newPuzzle = loadPuzzlerSolver3D(&in).release();

      if (!newPuzzle) {
        fl_alert("Could not load puzzle, sorry!");
        return;
      }

      /* No file name: the source is another format, which Save must
       * not write over. Save asks where to put the BurrTools file. */
      ReplacePuzzle(newPuzzle);
      setFileName("");

      selectEntitiesTab(true);
      activateShape(PcSel->getSelection());
      StatPieceInfo(PcSel->getSelection());

      changed = true;
    }
  }
}

void cb_Load_Scad_stub(Fl_Widget* /*o*/, void* v) { static_cast<mainWindow_c*>(v)->cb_Load_Scad(); }
void mainWindow_c::cb_Load_Scad(void) {

  if (threadStopped()) {

    if (!confirmDiscard("import another puzzle"))
      return;

    const char * f = bt_file_chooser_open("Import Puzzlecad File",
                                          "OpenSCAD Files\t*.scad",
                                          "");

    if (f) {

      std::ifstream in(f);
      puzzle_c * newPuzzle = loadOpenScadPuzzle(&in).release();

      if (!newPuzzle) {
        fl_alert("Could not load puzzle, sorry!");
        return;
      }

      /* No file name: the source is another format, which Save must
       * not write over. Save asks where to put the BurrTools file. */
      ReplacePuzzle(newPuzzle);
      setFileName("");

      selectEntitiesTab(true);
      activateShape(PcSel->getSelection());
      StatPieceInfo(PcSel->getSelection());

      changed = true;
    }
  }
}

void cb_Save_stub(Fl_Widget* /*o*/, void* v) { static_cast<mainWindow_c*>(v)->cb_Save(); }
/* The recovery copy of this puzzle's autosaved solve: one per puzzle file,
 * in the user's cache folder. Empty for a puzzle with no file. */
std::string mainWindow_c::autosavePath(void) const {
  return autosavePathFor(fname);
}

std::string mainWindow_c::autosavePathFor(const std::string & puzzleFile) const {
  if (puzzleFile.empty())
    return "";
  const std::string cache = userCacheDirectory();
  if (cache.empty())
    return "";
  std::error_code ec;
  const std::filesystem::path file = std::filesystem::absolute(puzzleFile, ec);
  uint64_t h = 1469598103934665603ull;
  for (unsigned char c : file.string()) {
    h ^= c;
    h *= 1099511628211ull;
  }
  char tag[17];
  snprintf(tag, sizeof(tag), "%016llx", (unsigned long long)h);
  return (std::filesystem::path(cache) / "autosave" /
          (file.stem().string() + "-" + tag + ".xmpuzzle")).string();
}

bool mainWindow_c::writePuzzleFile(const std::string & path) {
  const std::string tmp = path + ".tmp";
  std::error_code ec;
  {
    ogzstream ostr(tmp.c_str());
    if (!ostr)
      return false;
    {
      xmlWriter_c xml(ostr);
      puzzle->save(xml);
    }
    /* Closing writes the end of the compressed data; it can fail too. */
    ostr.close();
    if (!ostr) {
      std::filesystem::remove(tmp, ec);
      return false;
    }
  }
  /* Replace the old file only once the new one is whole. */
  std::filesystem::rename(tmp, path, ec);
  if (ec) {
    std::error_code ignored;
    std::filesystem::remove(tmp, ignored);
  }
  return !ec;
}

bool mainWindow_c::writeAutosave(void) {
  const std::string path = autosavePath();
  if (path.empty())
    return false;
  std::error_code ec;
  std::filesystem::create_directories(std::filesystem::path(path).parent_path(), ec);
  return writePuzzleFile(path);
}

void mainWindow_c::removeAutosave(void) {
  const std::string path = autosavePath();
  std::error_code ec;
  if (!path.empty())
    std::filesystem::remove(path, ec);
}

void mainWindow_c::cb_Save(void) {

  if (threadStopped()) {

    if (fname.empty())
      cb_SaveAs();

    else {
      if (!writePuzzleFile(fname))
        fl_alert("The puzzle could not be saved to %s.", fname.c_str());
      else {
        changed = false;
        if (shapeHistory)
          shapeHistory->markSaved();
        /* The file now holds the solve: the recovery copy is not needed. */
        removeAutosave();
      }
    }
  }
}

void cb_Convert_stub(Fl_Widget* /*o*/, void* v) { static_cast<mainWindow_c*>(v)->cb_Convert(); }
void mainWindow_c::cb_Convert(void) {

  convertWindow_c win(puzzle->getGridType()->getType());

  win.show();

  while (win.visible())
    Fl::wait();

  if (win.okSelected())
  {
    puzzle_c * p = doConvert(puzzle, win.getTargetType());

    if (p)
    {
      ReplacePuzzle(p);
      selectEntitiesTab(true);
      activateShape(0);
      changed = true;
    }
  }
}

class voxelTableVector_c : public voxelTable_c
{
  private:

    const std::vector<voxel_c *> *shapes;

  public:

    voxelTableVector_c(const std::vector<voxel_c *> *s) : shapes(s) {}

  protected:

    const voxel_c * findSpace(unsigned int index) const { return (*shapes)[index]; }
};

void cb_AssembliesToShapes_stub(Fl_Widget* /*o*/, void* v) { static_cast<mainWindow_c*>(v)->cb_AssembliesToShapes(); }
void mainWindow_c::cb_AssembliesToShapes(void) {

  assmImportWindow_c win(puzzle);

  win.show();

  while (win.visible())
    Fl::wait();

  if (win.okSelected())
  {
    problem_c * pr = puzzle->getProblem(win.getSrcProblem());

    std::vector<voxel_c *> sh;

    unsigned int filter = win.getFilter();

    voxelTableVector_c voxelTab(&sh);

    problem_c::SolutionsLock solutionsLock(*pr);

    for (unsigned int s = 0; s < pr->getNumberOfSavedSolutions(); s++)
    {
      std::unique_ptr<voxel_c> shape = pr->getSavedSolution(s)->getAssembly()->createSpace(*pr);

      if ((filter & assmImportWindow_c::dropDisconnected) && !shape->connected(0, true, voxel_c::VX_EMPTY))
        continue;

      symmetries_t sym = shape->selfSymmetries();

      if ((filter & assmImportWindow_c::dropMirror) && shape->getGridType()->getSymmetries()->symmetryContainsMirror(sym))
        continue;

      if ((filter & assmImportWindow_c::dropSymmetric) && !unSymmetric(sym))
        continue;

      if ((filter & assmImportWindow_c::dropNonMillable) && !isMillable(shape.get()))
        continue;

      if ((filter & assmImportWindow_c::dropNonNotchable) && !isNotchable(shape.get()))
        continue;

      unsigned int voxels = shape->countState(voxel_c::VX_FILLED);
      if (voxels < win.getShapeMin() || voxels > win.getShapeMax())
        continue;

      // if the user wants no identical shapes, we look up the current
      // shape in the known shapes table and drop it if we find it
      if (filter & assmImportWindow_c::dropIdentical)
      {
        if (voxelTab.getSpace(shape.get()))
          continue;
      }

      sh.push_back(shape.release());

      // we only need to add the current shape to the shape table
      // if the user wants to drop identical shapes and we use the table
      if (filter & assmImportWindow_c::dropIdentical)
      {
        voxelTab.addSpace(sh.size()-1);
      }
    }

    if (win.getAction() == assmImportWindow_c::A_ADD_NEW)
      pr = puzzle->getProblem(puzzle->addProblem());
    else if (win.getAction() == assmImportWindow_c::A_ADD_DST)
      pr = puzzle->getProblem(win.getDstProblem());

    // add the shapes to the problem of the problem tab
    for (unsigned int s = 0; s < sh.size(); s++)
    {
      int i = puzzle->addShape(sh[s]);

      if (win.getAction() == assmImportWindow_c::A_ADD_DST || win.getAction() == assmImportWindow_c::A_ADD_NEW)
      {
        pr->setShapeMaximum(i, win.getMax());
        pr->setShapeMinimum(i, win.getMin());
      }
    }

    changed = true;
    PiecesCountList->redraw();

    updateInterface();
  }
}

void cb_SaveAs_stub(Fl_Widget* /*o*/, void* v) { static_cast<mainWindow_c*>(v)->cb_SaveAs(); }
void mainWindow_c::cb_SaveAs(void) {

  if (threadStopped()) {
    const char * f = bt_file_chooser_save("Save Puzzle As", "Puzzle Files\t*.xmpuzzle", "");

    if (f) {

      if (!fileExists(f) || fl_choice("File exists; overwrite?", "Cancel", "Overwrite", 0)) {

        const std::string f2 = hasFileExtension(f, ".xmpuzzle") ? std::string(f)
                                                                 : std::string(f) + ".xmpuzzle";

        if (!writePuzzleFile(f2)) {
          fl_alert("The puzzle could not be saved to %s.", f2.c_str());
        } else {
          changed = false;
          if (shapeHistory)
            shapeHistory->markSaved();
          setFileName(f2);
          removeAutosave();
        }

      } else {

        fl_message("File not saved!\n");
      }
    }
  }
}

void cb_ImageExportVector_stub(Fl_Widget* /*o*/, void* v) { static_cast<mainWindow_c*>(v)->cb_ImageExportVector(); }
void mainWindow_c::cb_ImageExportVector(void) {

  vectorExportWindow_c w;

  w.show();
  while (w.visible())
    Fl::wait();

  if (!w.cancelled)
    View3D->getView()->exportToVector(w.getFileName(), w.getVectorType());
}

void cb_ImageExport_stub(Fl_Widget* /*o*/, void* v) { static_cast<mainWindow_c*>(v)->cb_ImageExport(); }
void mainWindow_c::cb_ImageExport(void) {
  imageExport_c w(puzzle, fname);
  w.show();

  while (w.visible()) {
    w.update();
    if (w.isWorking())
      Fl::wait(0);
    else
      Fl::wait(1);
  }
}

void cb_STLExport_stub(Fl_Widget* /*o*/, void* v) { static_cast<mainWindow_c*>(v)->cb_STLExport(); }
void mainWindow_c::cb_STLExport(void) {
  stlExport_c w(puzzle, fname);
  w.show();

  while (w.visible()) {
    Fl::wait();
  }
}

void cb_ExportGltf_stub(Fl_Widget* /*o*/, void* v) { static_cast<mainWindow_c*>(v)->cb_ExportGltf(); }
void mainWindow_c::cb_ExportGltf(void) {

  unsigned int prob = solutionProblem->getSelection();
  if (prob >= puzzle->getNumberOfProblems())
    return;
  problem_c * pr = puzzle->getProblem(prob);

  unsigned int sol = (unsigned int)SolutionSel->value() - 1;
  unsigned int steps = 0;
  {
    problem_c::SolutionsLock lock(*pr);
    if (sol < pr->getNumberOfSavedSolutions() && pr->getSavedSolution(sol)->getDisassembly())
      steps = pr->getSavedSolution(sol)->getDisassembly()->sumSteps();
  }
  if (!steps || stacking::isStacking(*puzzle)) {
    fl_alert("The selected solution has no disassembly to animate.");
    return;
  }

  /* the options stay for the next export in this session */
  static gltfExport::options_c opt;

  gltfExportWindow_c w(opt, steps);
  w.show();
  while (w.visible())
    Fl::wait();
  if (!w.wasAccepted())
    return;
  opt = w.options();

  std::string preset;
  if (!fname.empty()) {
    preset = fname;
    if (hasFileExtension(preset.c_str(), ".xmpuzzle"))
      preset.resize(preset.size() - strlen(".xmpuzzle"));
    preset += "-solution" + std::to_string(sol + 1) + ".glb";
  }

  const char * f = bt_file_chooser_save("Export Solution Animation", "glTF Binary\t*.glb", preset.c_str());
  if (!f)
    return;

  const std::string f2 = hasFileExtension(f, ".glb") ? std::string(f) : std::string(f) + ".glb";
  if (f2 != f && fileExists(f2.c_str()) && !fl_choice("File exists; overwrite?", "Cancel", "Overwrite", 0))
    return;

  /* the colours the 3D view paints the pieces in */
  std::vector<gltfExport::color_c> colors;
  for (unsigned int p = 0; p < pr->getNumberOfParts(); p++)
    for (unsigned int q = 0; q < pr->getPartMaximum(p); q++) {
      unsigned int shape = pr->getShapeIdOfPart(p);
      colors.push_back({ darkPieceColor(pieceColorR(shape, q)),
                         darkPieceColor(pieceColorG(shape, q)),
                         darkPieceColor(pieceColorB(shape, q)) });
    }

  std::string err;
  {
    problem_c::SolutionsLock lock(*pr);
    err = gltfExport::writeSolutionAnimation(f2.c_str(), *pr, sol, colors, opt);
  }
  if (!err.empty())
    fl_alert("Could not export the animation:\n%s", err.c_str());
}

void cb_Export_Scad_stub(Fl_Widget* /*o*/, void* v) { static_cast<mainWindow_c*>(v)->cb_Export_Scad(); }
void cb_ExportPaused_stub(Fl_Widget* /*o*/, void* v) { static_cast<mainWindow_c*>(v)->cb_ExportPaused(); }
void cb_ImportPaused_stub(Fl_Widget* /*o*/, void* v) { static_cast<mainWindow_c*>(v)->cb_ImportPaused(); }

/* A paused solve as one file, *.btsolve, to carry on later or elsewhere:
 *   "BTSOLVE2", then sections, each a 32-bit name length, the name, a
 *   64-bit size and the bytes:
 *   "puzzle"          the puzzle, gzip-compressed as BurrTools saves it. A
 *                     brick or sliding problem keeps its paused solve in it.
 *   "problem"         the paused problem's number, as text
 *   "searchz/<name>"  a stacking solve's saved search, file by file, packed
 *                     on every core by blockpack (levels as key gaps); the
 *                     saved levels may be left out
 * An older BurrTools refuses a BTSOLVE2 file rather than import it without
 * its search. (BTSOLVE1, which stored the files unpacked, was never used.)
 */
namespace {

const char SOLVE_MAGIC[8] = {'B', 'T', 'S', 'O', 'L', 'V', 'E', '2'};

bool writeSection(std::ofstream & out, const std::string & name, std::istream & data, uint64_t size) {
  const uint32_t len = (uint32_t)name.size();
  out.write(reinterpret_cast<const char *>(&len), sizeof(len));
  out.write(name.data(), len);
  out.write(reinterpret_cast<const char *>(&size), sizeof(size));
  std::vector<char> buf(1 << 20);
  uint64_t left = size;
  while (left && out) {
    const std::streamsize n = (std::streamsize)std::min<uint64_t>(left, buf.size());
    data.read(buf.data(), n);
    if (data.gcount() != n)
      return false;
    out.write(buf.data(), n);
    left -= (uint64_t)n;
  }
  return (bool)out;
}

bool writeFileSection(std::ofstream & out, const std::string & name, const std::filesystem::path & file) {
  std::error_code ec;
  const uint64_t size = std::filesystem::file_size(file, ec);
  std::ifstream in(file, std::ios::binary);
  return !ec && in && writeSection(out, name, in, size);
}

/* A file packed by blockpack: its size is known only once it is written,
 * so the size field is filled in afterwards. */
bool writePackedSection(std::ofstream & out, const std::string & name,
                        const std::filesystem::path & file, bool keys) {
  const uint32_t len = (uint32_t)name.size();
  out.write(reinterpret_cast<const char *>(&len), sizeof(len));
  out.write(name.data(), len);
  const std::streampos sizeAt = out.tellp();
  uint64_t size = 0;
  out.write(reinterpret_cast<const char *>(&size), sizeof(size));
  size = blockpack::pack(file, out, keys);
  if (!size || !out)
    return false;
  const std::streampos end = out.tellp();
  out.seekp(sizeAt);
  out.write(reinterpret_cast<const char *>(&size), sizeof(size));
  out.seekp(end);
  return (bool)out;
}

bool writeTextSection(std::ofstream & out, const std::string & name, const std::string & text) {
  std::istringstream in(text);
  return writeSection(out, name, in, text.size());
}

/* The next section's name and size; false at the end or on a bad file. */
bool readSectionHeader(std::ifstream & in, std::string & name, uint64_t & size) {
  uint32_t len = 0;
  if (!in.read(reinterpret_cast<char *>(&len), sizeof(len)) || len > 4096)
    return false;
  name.assign(len, '\0');
  if (!in.read(&name[0], len) || !in.read(reinterpret_cast<char *>(&size), sizeof(size)))
    return false;
  return true;
}

bool copySection(std::ifstream & in, uint64_t size, std::ostream & out) {
  std::vector<char> buf(1 << 20);
  while (size) {
    const std::streamsize n = (std::streamsize)std::min<uint64_t>(size, buf.size());
    if (!in.read(buf.data(), n))
      return false;
    out.write(buf.data(), n);
    size -= (uint64_t)n;
  }
  return (bool)out;
}

/* A bare file name, such as "state.bin": nothing that could lead out of
 * the folder it is written into (a file could be crafted to try). */
bool isPlainFileName(const std::string & n) {
  return !n.empty() && n != "." && n != ".." &&
         n.find_first_of("/\\:") == std::string::npos;
}

std::string sizeText(uint64_t bytes) {
  char buf[32];
  if (bytes >= 1000000000ull)
    snprintf(buf, sizeof(buf), "%.1f GB", bytes / 1e9);
  else
    snprintf(buf, sizeof(buf), "%.0f MB", bytes / 1e6);
  return buf;
}

} // namespace

bool mainWindow_c::problemPaused(unsigned int prob) const {
  if (assmThread || !puzzle || prob >= puzzle->getNumberOfProblems())
    return false;
  const problem_c * pr = puzzle->getProblem(prob);
  if (stacking::isStacking(*puzzle))
    return !panex::savedSearch(*pr).empty();
  return pr->getSolveState() == SS_SOLVING;
}

void mainWindow_c::cb_ExportPaused(void) {
  const unsigned int prob = solutionProblem->getSelection();
  if (!problemPaused(prob)) {
    fl_message("Pause a solve first: the problem selected on the Solver tab has no paused solve.");
    return;
  }
  const problem_c * pr = puzzle->getProblem(prob);

  /* A stacking solve's saved search: its state, and the levels if wanted. */
  std::vector<std::filesystem::path> files;
  if (stacking::isStacking(*puzzle)) {
    const std::filesystem::path dir = panex::searchFolder(*pr);
    std::error_code ec;
    uint64_t levelBytes = 0;
    std::vector<std::filesystem::path> levels;
    for (const auto & e : std::filesystem::directory_iterator(dir, ec)) {
      if (e.path().filename() == "state.bin")
        files.push_back(e.path());
      else if (e.path().extension() == ".lvl") {
        levels.push_back(e.path());
        levelBytes += e.file_size(ec);
      }
    }
    bool withLevels = true;
    if (levelBytes > 1000000000ull) {
      const int choice = fl_choice(
          "The saved search keeps %s of levels for finding the path once the search is done.\n\n"
          "Include them? Left out, the file is much smaller, but finding the path at the end "
          "takes longer.",
          "Cancel", "Include", "Leave Out", sizeText(levelBytes).c_str());
      if (choice == 0)
        return;
      withLevels = choice == 1;
    }
    if (withLevels)
      files.insert(files.end(), levels.begin(), levels.end());
  }

  std::string preset = "puzzle";
  if (!fname.empty())
    preset = std::filesystem::path(fname).stem().string();
  preset += "-P" + std::to_string(prob + 1) + ".btsolve";
  const char * f = bt_file_chooser_save("Export Paused Solver State", "Paused Solver State\t*.btsolve",
                                        preset.c_str());
  if (!f)
    return;
  std::string target = f;
  if (!hasFileExtension(target.c_str(), ".btsolve"))
    target += ".btsolve";

  /* The puzzle as BurrTools saves it, then read back as bytes. */
  std::error_code ec;
  const std::filesystem::path tmp = std::filesystem::temp_directory_path(ec) /
                                    ("burrtools-export-" + std::to_string(getpid()) + ".xmpuzzle");
  bool ok = writePuzzleFile(tmp.string());
  if (ok) {
    std::ofstream out(target, std::ios::binary | std::ios::trunc);
    out.write(SOLVE_MAGIC, sizeof(SOLVE_MAGIC));
    ok = (bool)out && writeFileSection(out, "puzzle", tmp) &&
         writeTextSection(out, "problem", std::to_string(prob));
    /* Packing a large saved search takes a while. */
    fl_cursor(FL_CURSOR_WAIT);
    Fl::check();
    for (const auto & file : files)
      ok = ok && writePackedSection(out, "searchz/" + file.filename().string(), file,
                                    file.extension() == ".lvl");
    fl_cursor(FL_CURSOR_DEFAULT);
  }
  std::filesystem::remove(tmp, ec);
  if (!ok) {
    std::filesystem::remove(target, ec);
    fl_alert("Could not export the paused solve to %s.", target.c_str());
    return;
  }
  fl_message("Exported the paused solve of problem %u to %s.", prob + 1, target.c_str());
}

void mainWindow_c::cb_ImportPaused(void) {
  if (!threadStopped())
    return;
  if (!confirmDiscard("import a paused solve"))
    return;
  const char * f = bt_file_chooser_open("Import Paused Solver State", "Paused Solver State\t*.btsolve", "");
  if (!f)
    return;

  std::ifstream in(f, std::ios::binary);
  char magic[sizeof(SOLVE_MAGIC)];
  if (!in.read(magic, sizeof(magic)) || !std::equal(magic, magic + sizeof(magic), SOLVE_MAGIC)) {
    fl_alert("%s is not a paused solver state exported by BurrTools.", f);
    return;
  }

  std::error_code ec;
  const std::filesystem::path tmp = std::filesystem::temp_directory_path(ec) /
                                    ("burrtools-import-" + std::to_string(getpid()) + ".xmpuzzle");
  std::unique_ptr<puzzle_c> loaded;
  unsigned int prob = 0;
  std::filesystem::path searchDir;
  bool ok = true;
  std::string name;
  uint64_t size = 0;
  while (ok && readSectionHeader(in, name, size)) {
    if (name == "puzzle") {
      {
        std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
        ok = copySection(in, size, out);
      }
      auto str = ok ? openGzFile(tmp.string().c_str()) : nullptr;
      if (!str) {
        ok = false;
        break;
      }
      try {
        xmlParser_c pars(*str);
        loaded = std::make_unique<puzzle_c>(pars);
      } catch (xmlParserException_c & e) {
        fl_alert("The puzzle in %s could not be read: %s", f, e.what());
        std::filesystem::remove(tmp, ec);
        return;
      }
    } else if (name == "problem") {
      std::ostringstream text;
      ok = copySection(in, size, text);
      prob = (unsigned int)strtoul(text.str().c_str(), nullptr, 10);
      if (loaded && stacking::isStacking(*loaded) && prob < loaded->getNumberOfProblems()) {
        /* Where Continue looks for this problem's saved search. */
        searchDir = panex::searchFolder(*loaded->getProblem(prob));
        if (searchDir.empty()) {
          ok = false;
          break;
        }
        std::filesystem::remove_all(searchDir, ec);
        std::filesystem::create_directories(searchDir, ec);
      }
    } else if (name.rfind("searchz/", 0) == 0 && !searchDir.empty() &&
               isPlainFileName(name.substr(8))) {
      fl_cursor(FL_CURSOR_WAIT);
      Fl::check();
      ok = blockpack::unpack(in, size, searchDir / name.substr(8));
      fl_cursor(FL_CURSOR_DEFAULT);
    } else {
      in.seekg((std::streamoff)size, std::ios::cur);
    }
  }
  std::filesystem::remove(tmp, ec);
  if (!ok || !loaded || prob >= loaded->getNumberOfProblems()) {
    fl_alert("%s could not be read completely.", f);
    return;
  }

  /* A new, untitled puzzle: the user saves it where they like. */
  setFileName("");
  ReplacePuzzle(loaded.release());
  changed = true;
  View3D->getView()->showColors(puzzle, StatusLine->getColorMode());

  TaskSelectionTab->value(TabSolve);
  solutionProblem->setSelection(prob);
  applySolverOptions(prob);
  cb_TaskSelectionTab(TaskSelectionTab);
  updateInterface();
  fl_message("Imported the paused solve of problem %u. Press Continue on the Solver tab to carry it on, "
             "and save the puzzle to keep it.", prob + 1);
}
void mainWindow_c::cb_Export_Scad(void) {

  if (puzzle->getGridType()->getType() != gridType_c::GT_BRICKS) {
    fl_alert("Puzzlecad export is only available for cube-grid puzzles.");
    return;
  }

  if (puzzle->getNumberOfShapes() == 0) {
    fl_alert("Nothing to export.");
    return;
  }

  const char * preset = fname.c_str();

  const char * f = bt_file_chooser_save("Export Puzzlecad File", "OpenSCAD Files\t*.scad", preset);

  if (!f)
    return;

  if (fileExists(f) && !fl_choice("File exists; overwrite?", "Cancel", "Overwrite", 0))
    return;

  const std::string f2 = hasFileExtension(f, ".scad") ? std::string(f) : std::string(f) + ".scad";

  std::ofstream out(f2);
  unsigned int prob = 0;
  if (puzzle->getNumberOfProblems() &&
      problemSelector->getSelection() < puzzle->getNumberOfProblems())
    prob = problemSelector->getSelection();

  if (!out || !saveOpenScadPuzzle(out, puzzle, fname.empty() ? f2.c_str() : fname.c_str(), prob)) {
    fl_alert("Could not export puzzlecad file.");
    return;
  }
}
