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

#include "lib/puzzle.h"
#include "lib/problem.h"
#include "lib/solvethread.h"
#include "lib/bt_assert.h"
#include "lib/gridtype.h"
#include "lib/voxel.h"
#include "lib/solvertype.h"
#include "tools/xml.h"
#include "tools/gzstream.h"

#include <chrono>
#include <cstdio>
#include <fstream>
#include <thread>
#include <iostream>

#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/time.h>

#ifdef _WIN32
#include <winsock.h>
#endif

using namespace std;

void usage(void) {

  cout << "burrTxt [options] file [options]\n\n";
  cout << "  file: puzzle file with the puzzle definition to solve\n\n";
  cout << "  -R    restart and throw away all found solutions, otherwise continue\n";
  cout << "  -d    try to disassemble and only keep solutions that do disassemble\n";
  cout << "  -c    just count solutions\n";
  cout << "  -m    keep mirror solutions\n";
  cout << "  -r    keep rotated solutions\n";
  cout << "  -p    drop disassemblies and replace by information about disassembly\n";
  cout << "  -b    selecte problem, else 0\n";
  cout << "  --rotations   try 90 degree piece rotations when disassembling\n";
  cout << "  --solver TYPE classic, crowell or bt2\n";
  cout << "  --progress    print what the solver is doing to stderr once a second\n";
}


bool checkInput(void)
{
  /* Initialize the file descriptor set. */
  fd_set set;
  FD_ZERO (&set);
  FD_SET (STDIN_FILENO, &set);

  /* Initialize the timeout data structure. */
  struct timeval timeout;
  timeout.tv_sec = 1;
  timeout.tv_usec = 0;

  /* select returns 0 if timeout, 1 if input available, -1 if error. */
  return select(FD_SETSIZE, &set, NULL, NULL, &timeout);
}


static int solve(int argv, char* args[]) {

  if (argv < 1) {
    usage();
    return 2;
  }

  int par = solveThread_c::PAR_REDUCE;
  bool restart = false;
  int filenumber = 0;
  int firstProblem = 0;
  int lastProblem = 1;
  bool showProgress = false;
  solverType_e solverType = SOLVER_CLASSIC;

  for(int i = 1; i < argv; i++) {

    if (strcmp(args[i], "-R") == 0)
      restart = true;
    else if (strcmp(args[i], "-d") == 0)
      par |= solveThread_c::PAR_DISASSM;
    else if (strcmp(args[i], "-c") == 0)
      par |= solveThread_c::PAR_JUST_COUNT;
    else if (strcmp(args[i], "-m") == 0)
      par |= solveThread_c::PAR_KEEP_MIRROR;
    else if (strcmp(args[i], "-r") == 0)
      par |= solveThread_c::PAR_KEEP_ROTATIONS;
    else if (strcmp(args[i], "-p") == 0)
      par |= solveThread_c::PAR_DROP_DISASSEMBLIES;
    else if (strcmp(args[i], "--rotations") == 0)
      par |= solveThread_c::PAR_CHECK_ROTATIONS;
    else if (strcmp(args[i], "--progress") == 0)
      showProgress = true;
    else if (strcmp(args[i], "--solver") == 0 && i + 1 < argv) {
      if (!solverTypeFromName(args[i+1], &solverType)) {
        cout << "unknown solver type \"" << args[i+1] << "\"\n";
        return 2;
      }
      i++;
    }
    else if (strcmp(args[i], "-b") == 0) {
      firstProblem = atoi(args[i+1]);
      lastProblem = firstProblem + 1;
      i++;
    }
    else
      filenumber = i;
  }

  if (filenumber == 0) {
    usage();
    return 1;
  }

  auto str = openGzFile(args[filenumber]);
  if (!str) {
    cout << "could not open input file \"" << args[filenumber] << "\"\n";
    return 2;
  }
  xmlParser_c pars(*str);
  puzzle_c p(pars);

  if (firstProblem < 0 || (unsigned int)firstProblem >= p.getNumberOfProblems()) {
    cout << "the file has " << p.getNumberOfProblems() << " problem(s), there is no problem "
         << firstProblem << "\n";
    return 2;
  }

  /* not every space grid has a solver, and fewer still have a disassembler.
   * Check before we build one: the disassembler asserts on the movement cache
   * a grid without CAP_DISASSEMBLE does not provide.
   */
  const unsigned int caps = p.getGridType()->getCapabilities();

  if (!(caps & gridType_c::CAP_ASSEMBLE)) {
    cout << "Sorry, this space grid doesn't have an assembler (yet)\n";
    return 2;
  }

  if ((par & solveThread_c::PAR_DISASSM) && !(caps & gridType_c::CAP_DISASSEMBLE)) {
    cout << "Sorry, this space grid doesn't have a disassembler (yet)\n";
    cout << "drop -d to just assemble the puzzle\n";
    return 2;
  }

  std::string outname = args[filenumber];
  outname += "ttt";
  cout << "outputting into " << outname << std::endl;

  ofstream ostr(outname.c_str());
  if (!ostr)
  {
    cout << "Can not open output file, aborting\n";
    return 2;
  }

  for (unsigned int i = 0; i < p.getNumberOfShapes(); i++)
    p.getShape(i)->initHotspot();


  int exitCode = 0;

  for (int pr = firstProblem ; pr < lastProblem ; pr++) {

    problem_c * problem = p.getProblem(pr);

    if (restart)
      problem->removeAllSolutions();

    /* without -R we continue where an earlier run left off. A problem that is
     * already finished, or whose information was invalidated by editing, has
     * nothing to continue from, and setAssembler's precondition rejects one.
     */
    if (!problem->canStartSolving()) {
      cout << "problem " << pr << " (" << problem->getName() << ") ";
      if (problem->getSolveState() == SS_SOLVED)
        cout << "is already solved: " << problem->getNumAssemblies() << " assemblies, "
             << problem->getNumSolutions() << " solutions\n";
      else
        cout << "has no state to continue from\n";
      cout << "use -R to solve it again\n";
      continue;
    }

    solveThread_c assmThread(*problem, par);
    assmThread.setSolverType(solverType);

    if (!assmThread.start(false)) {
      cout << "Could not start Solver\n";
      continue;
    }

    if (showProgress) {
      /* what the GUI shows on its Solver tab, a line a second */
      for (;;) {
        const unsigned int act = assmThread.currentAction();
        if (act == solveThread_c::ACT_FINISHED || act == solveThread_c::ACT_PAUSING ||
            act == solveThread_c::ACT_ERROR || act == solveThread_c::ACT_ASSERT)
          break;
        const solveProgress_c pg = assmThread.getProgressSnapshot();
        fprintf(stderr, "%7.1f s  %5.1f%%  %s", (double)pg.elapsedMs / 1000.0,
                100.0 * pg.overall, pg.activity().c_str());
        if (pg.stage == solveProgress_c::STAGE_ASSEMBLE)
          fprintf(stderr, "  [assembly %.1f%%, %lu found]", 100.0 * pg.assemblyFraction, pg.assemblies);
        if (pg.secondsLeft >= 0)
          fprintf(stderr, "  about %.0f s left", pg.secondsLeft);
        fprintf(stderr, "\n");
        std::this_thread::sleep_for(std::chrono::seconds(1));
      }
    }

    assmThread.waitUntilFinished();

    if (assmThread.currentAction() == solveThread_c::ACT_ERROR) {
      cout << "error in solver\n";
      exitCode = 1;
    } else if (assmThread.currentAction() == solveThread_c::ACT_ASSERT) {
      cout << "Exception in Solver\n";
      cout << " file      : " << assmThread.getAssertException().file << "\n";
      cout << " function  : " << assmThread.getAssertException().function << "\n";
      cout << " line      : " << assmThread.getAssertException().line << "\n";
      cout << " expression: " << assmThread.getAssertException().expr << "\n";
      exitCode = 1;
    }
  }

  xmlWriter_c xml(ostr);
  p.save(xml);

  return exitCode;
}

int main(int argv, char* args[]) {

  try {
    return solve(argv, args);
  }

  catch (assert_exception & a) {
    cout << "Exception\n";
    cout << " file      : " << a.file << "\n";
    cout << " function  : " << a.function << "\n";
    cout << " line      : " << a.line << "\n";
    cout << " expression: " << a.expr << "\n";
    return 1;
  }
}


