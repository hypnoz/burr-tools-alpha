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
#include "lib/assembler.h"
#include "lib/assembly.h"
#include "lib/disassembler.h"
#include "lib/disassembler_factory.h"
#include "lib/solvertype.h"
#include "lib/bt2_assemble.h"
#include "lib/disassembly.h"
#include "lib/print.h"
#include "lib/voxel.h"
#include "lib/solution.h"
#include "lib/sliding.h"
#include "tools/xml.h"
#include "tools/gzstream.h"

#include <stdlib.h>
#include <string.h>

#ifndef _WIN32
#include <fcntl.h>
#include <unistd.h>
#endif

#include <chrono>
#include <fstream>
#include <iostream>
#include <map>
#include <mutex>
#include <string>

using namespace std;

bool disassemble;
bool checkRotations;
bool strictColors;
bool allProblems;
bool printDisassemble;
bool printSolutions;
bool quiet;
bool jsonOutput;
solverType_e solverType;

std::unique_ptr<disassembler_c> d;

#ifndef _WIN32
/** Silence library diagnostics on stderr during batch solves. */
class stderr_redirect_c {
  int saved_;

public:

  explicit stderr_redirect_c(bool enable) : saved_(-1) {
    if (!enable)
      return;
    fflush(stderr);
    saved_ = dup(STDERR_FILENO);
    if (saved_ < 0)
      return;
    int nullfd = open("/dev/null", O_WRONLY);
    if (nullfd >= 0) {
      dup2(nullfd, STDERR_FILENO);
      close(nullfd);
    }
  }

  ~stderr_redirect_c(void) {
    if (saved_ >= 0) {
      fflush(stderr);
      dup2(saved_, STDERR_FILENO);
      close(saved_);
    }
  }

private:

  stderr_redirect_c(const stderr_redirect_c&);
  void operator=(const stderr_redirect_c&);
};
#endif

/** Parsed disassembly level string (plain `5.1.3` or rotation form `5R0.1R3`). */
struct dotlevel_stats_c {
  int level;       /**< slides+rots for the first piece removal */
  int totalmoves;  /**< sum of all slides and rotations */
  int moves;       /**< sum of linear slides only */
  int rotations;   /**< sum of rotation counts after each R */
};

/**
 * Parse movesText() output.
 * Plain form: each dotted part is a slide count (rots treated as 0).
 * Rotation form: each part is `slidesRrots` (e.g. `5R0.1R3`).
 */
static bool parse_dotlevel(const char * dotlevel, dotlevel_stats_c * out) {

  int firstSlides = -1;
  int firstRots = 0;
  int moves = 0;
  int rotations = 0;
  int segments = 0;

  const char * p = dotlevel;
  while (*p) {
    if (*p < '0' || *p > '9')
      return false;

    int slides = 0;
    while (*p >= '0' && *p <= '9')
      slides = slides * 10 + (*p++ - '0');

    int rots = 0;
    if (*p == 'R') {
      p++;
      if (*p < '0' || *p > '9')
        return false;
      while (*p >= '0' && *p <= '9')
        rots = rots * 10 + (*p++ - '0');
    }

    if (segments == 0) {
      firstSlides = slides;
      firstRots = rots;
    }
    moves += slides;
    rotations += rots;
    segments++;

    if (*p == 0)
      break;
    if (*p != '.')
      return false;
    p++;
  }

  if (segments == 0 || firstSlides < 0)
    return false;

  out->level = firstSlides + firstRots;
  out->totalmoves = moves + rotations;
  out->moves = moves;
  out->rotations = rotations;
  return true;
}

static bool level_is_better(int level, int totalmoves, int bestLevel, int bestTotalmoves) {
  if (level > bestLevel)
    return true;
  if (level == bestLevel && totalmoves > bestTotalmoves)
    return true;
  return false;
}

class asm_cb : public assembler_cb {

public:

  int Assemblies;
  int Solutions;
  int pn;
  problem_c * puzzle;
  std::mutex cbMutex;
  std::map<std::string, unsigned int> slideBestMoves;

  bool hasBest;
  char bestDotlevel[200];
  int bestLevel;
  int bestTotalmoves;
  int bestMoves;
  int bestRotations;

  asm_cb(problem_c * p) :
    Assemblies(0), Solutions(0), pn(p->getNumberOfPieces()), puzzle(p),
    hasBest(false), bestLevel(0), bestTotalmoves(0), bestMoves(0), bestRotations(0)
  {
    bestDotlevel[0] = 0;
  }

  void considerLevel(separation_c * da) {

    char lev[200];
    snprintf(lev, sizeof(lev), "%s", da->movesText().c_str());

    dotlevel_stats_c stats;
    if (!parse_dotlevel(lev, &stats))
      return;

    if (!hasBest || level_is_better(stats.level, stats.totalmoves, bestLevel, bestTotalmoves)) {
      hasBest = true;
      strncpy(bestDotlevel, lev, sizeof(bestDotlevel));
      bestDotlevel[sizeof(bestDotlevel) - 1] = 0;
      bestLevel = stats.level;
      bestTotalmoves = stats.totalmoves;
      bestMoves = stats.moves;
      bestRotations = stats.rotations;
    }
  }

  bool assembly(std::unique_ptr<assembly_c> a) override {

    std::lock_guard<std::mutex> lock(cbMutex);

    Assemblies++;

    if (disassemble) {

      /* Sliding "disassemble" is the start-to-goal slide, not brick take-apart. */
      std::unique_ptr<separation_c> da = sliding::isSliding(*puzzle)
          ? sliding::findSlidePath(*puzzle, *a)
          : d->disassemble(a.get());

      if (da) {
        bool countIt = true;
        if (sliding::isSliding(*puzzle)) {
          const std::string key = sliding::finalPlacementKey(*da);
          const unsigned int moves = da->getMoves();
          auto it = slideBestMoves.find(key);
          if (it == slideBestMoves.end())
            slideBestMoves.emplace(key, moves);
          else if (moves < it->second)
            it->second = moves;
          else
            countIt = false;
        }
        if (countIt) {
          Solutions++;

          if (jsonOutput) {
            considerLevel(da.get());
          } else {
            if (printSolutions)
              print(a.get(), puzzle);

            if (!quiet || allProblems)
              printf("level: %s\n", da->movesText().c_str());

            if (printDisassemble)
              print(da.get(), a.get(), puzzle);
          }
        }
      }

    } else if (printSolutions)
      print(a.get(), puzzle);

    return true;
  }
};

struct json_result_c {

  int assemblies;
  int solutions;
  bool hasBest;
  char bestDotlevel[200];
  int bestLevel;
  int bestTotalmoves;
  int bestMoves;
  int bestRotations;
  double solvetime;

  json_result_c(void) :
    assemblies(0), solutions(0), hasBest(false), bestLevel(0), bestTotalmoves(0),
    bestMoves(0), bestRotations(0), solvetime(0)
  {
    bestDotlevel[0] = 0;
  }

  void merge(const asm_cb & a) {

    assemblies += a.Assemblies;
    solutions += a.Solutions;

    if (a.hasBest && (!hasBest ||
          level_is_better(a.bestLevel, a.bestTotalmoves, bestLevel, bestTotalmoves))) {
      hasBest = true;
      strncpy(bestDotlevel, a.bestDotlevel, sizeof(bestDotlevel));
      bestDotlevel[sizeof(bestDotlevel) - 1] = 0;
      bestLevel = a.bestLevel;
      bestTotalmoves = a.bestTotalmoves;
      bestMoves = a.bestMoves;
      bestRotations = a.bestRotations;
    }
  }

  void addSolveTime(double seconds) {
    solvetime += seconds;
  }
};

static void print_json_result(const json_result_c & stats) {

  printf("{\"assemblies\":%d,\"solutions\":%d",
      stats.assemblies, stats.solutions);

  if (stats.hasBest) {
    printf(",\"dotlevel\":\"%s\",\"level\":%d,\"totalmoves\":%d",
        stats.bestDotlevel, stats.bestLevel, stats.bestTotalmoves);
    if (checkRotations)
      printf(",\"moves\":%d,\"rotations\":%d",
          stats.bestMoves, stats.bestRotations);
  } else {
    printf(",\"dotlevel\":null,\"level\":null,\"totalmoves\":null");
    if (checkRotations)
      printf(",\"moves\":null,\"rotations\":null");
  }

  printf(",\"solvetime\":%.3f}\n", stats.solvetime);
}

void usage(void) {

  cout << "burrTxt [options] file [options]\n\n";
  cout << "  file: puzzle file with the puzzle definition to solve\n\n";
  cout << "  --json  machine-readable result for batch tools (implies -d -q -r;\n";
  cout << "          prints one JSON object with the highest disassembly level.\n";
  cout << "          Fields: assemblies, solutions, dotlevel, level, totalmoves,\n";
  cout << "          solvetime (seconds); with -R also moves (linear) and rotations)\n";
  cout << "  Short options may be combined (e.g. -dR, -rq).\n";
  cout << "  -d      try to disassemble and only print solutions that do disassemble\n";
  cout << "  -p      print the disassembly plan\n";
  cout << "  -r      reduce the placements before starting to solve the puzzle\n";
  cout << "  -R      also try 90 degree piece rotations during disassembly (implies -d, brick grids)\n";
  cout << "  -C      strict color restrictions: a voxel fits only a result voxel of the same color\n";
  cout << "          (a colored voxel does not also fit a neutral one, and a neutral voxel\n";
  cout << "          fits only a neutral result voxel)\n";
  cout << "  -s      print the assembly\n";
  cout << "  -q      be quiet and only print statistics\n";
  cout << "  -n      don't print a newline at the end of the line\n";
  cout << "  -o n    select the problem to solve\n";
  cout << "  -o all  solves all problems in file\n";
  cout << "  -x      only redisassemble the given solutions\n";
  cout << "  -t n    set number of worker threads for assembler (0 = auto)\n";
  cout << "  --solver TYPE\n";
  cout << "          solver engine. If omitted, BurrTools Classic is used.\n";
  cout << "          TYPE (case-insensitive; quotes needed if it has spaces):\n";
  cout << "            \"BurrTools Classic\"  complete take-apart (also: classic)\n";
  cout << "            \"Andrew Crowell\"     90° take-apart heuristics (also: crowell)\n";
  cout << "            \"BurrTools 2\"        Classic take-apart + dancing-cells assembly (also: bt2)\n";
  cout << "  -a      ask for information about the current puzzle, the next letters must be:\n";
  cout << "     s0   print solutions with the only the used pieces\n";
  cout << "     s1   print solutions including the assemblies\n";
  cout << "     c    print comment\n";
}

int main(int argv, char* args[]) {

  if (argv < 1) {
    usage();
    return 2;
  }

  int state = 0;
  disassemble = false;
  checkRotations = false;
  strictColors = false;
  allProblems = false;
  printDisassemble = false;
  printSolutions = false;
  quiet = false;
  jsonOutput = false;
  solverType = SOLVER_CLASSIC;
  bool assemble = true;
  unsigned int problem = 0;
  unsigned int firstProblem = 0;
  unsigned int lastProblem = 0;
  int filenumber = 0;
  bool reduce = false;
  bool newline = true;
  bool ask = false;
  unsigned int threads = 0;
  enum {
    W_NUM_SOLUTIONS,
    W_SOLUTION_PIECES,
    W_SOLUTION_ASSM,
    W_COMMENT
  } what = W_COMMENT;

  for(int i = 1; i < argv; i++) {

    switch (state) {

    case 0:

      if (strcmp(args[i], "--json") == 0) {
        jsonOutput = true;
        disassemble = true;
        quiet = true;
        reduce = true;
      } else if (strcmp(args[i], "--solver") == 0) {
        if (i + 1 >= argv) {
          usage();
          return 2;
        }
        if (!solverTypeFromName(args[i+1], &solverType)) {
          fprintf(stderr, "burrTxt: unknown solver '%s'\n", args[i+1]);
          fprintf(stderr, "         use \"BurrTools Classic\", \"Andrew Crowell\", or \"BurrTools 2\"\n");
          fprintf(stderr, "         (also: classic, crowell, bt2)\n");
          return 2;
        }
        i++;
      } else if (strcmp(args[i], "-t") == 0) {
        /* -t as the last argument used to pass the null terminator to atoi
         * before dereferencing; strtol rather than atoi so that a negative or
         * non-numeric value is rejected instead of wrapping into a huge
         * unsigned thread count
         */
        if (i + 1 >= argv) {
          cout << "-t requires a numeric argument\n";
          return 2;
        }
        char *end = nullptr;
        long t = strtol(args[i+1], &end, 10);
        if (!end || *end || t < 0) {
          cout << "-t requires a non-negative number\n";
          return 2;
        }
        threads = (unsigned int)t;
        i++;
      } else if (strcmp(args[i], "-o") == 0) {
        if (i + 1 >= argv) {
          usage();
          return 2;
        }
        if (strcmp(args[i+1],"all")==0)
          allProblems = true;
        else
          problem = atoi(args[i+1]);
        i++;
      } else if (strcmp(args[i], "-a") == 0) {

        if (i + 1 >= argv) {
          usage();
          return 2;
        }

        ask = true;
        what = W_NUM_SOLUTIONS;

        if (strcmp(args[i+1], "s0") == 0)
          what = W_SOLUTION_PIECES;
        else if (strcmp(args[i+1], "s1") == 0)
          what = W_SOLUTION_ASSM;
        else if (strcmp(args[i+1], "c") == 0)
          what = W_COMMENT;
        else
        {
          usage();
          return 2;
        }

        i++;

      } else if (args[i][0] == '-' && args[i][1] != '-' && args[i][1] != 0) {

        for (int j = 1; args[i][j]; j++) {
          switch (args[i][j]) {
          case 'd':
            disassemble = true;
            break;
          case 'p':
            printDisassemble = true;
            break;
          case 's':
            printSolutions = true;
            break;
          case 'r':
            reduce = true;
            break;
          case 'R':
            checkRotations = true;
            disassemble = true;
            break;
          case 'C':
            strictColors = true;
            break;
          case 'n':
            newline = false;
            break;
          case 'x':
            assemble = false;
            break;
          case 'q':
            quiet = true;
            printDisassemble = false;
            printSolutions = false;
            break;
          case 'o':
          case 'a':
            fprintf(stderr, "burrTxt: -%c cannot be clustered; give it as its own argument\n", args[i][j]);
            return 2;
          default:
            fprintf(stderr, "burrTxt: unknown option -%c\n", args[i][j]);
            usage();
            return 2;
          }
        }

      } else
        filenumber = i;

      break;
    }
  }

  if (filenumber == 0) {
    usage();
    return 1;
  }

  if (jsonOutput && (ask || !assemble || allProblems || printDisassemble || printSolutions)) {
    fprintf(stderr, "burrTxt: --json cannot be combined with -a, -x, -o all, -p, or -s\n");
    return 2;
  }

  auto str = openGzFile(args[filenumber]);
  if (!str) {
    printf("could not open input file \"%s\"\n", args[filenumber]);
    return 2;
  }
  xmlParser_c pars(*str);
  puzzle_c p(pars);

  if (ask) {

    switch (what) {
      case W_COMMENT:
        printf("%s\n", p.getComment().c_str());
        break;
      case W_NUM_SOLUTIONS:
        for (unsigned int i = 0; i < p.getNumberOfProblems(); i++)
          printf("number of solutions for problem %u: %lu\n", i, p.getProblem(i)->getNumSolutions());
        break;
      case W_SOLUTION_PIECES:
      case W_SOLUTION_ASSM:
        for (unsigned int i = 0; i < p.getNumberOfProblems(); i++) {
          printf("problem %u\n", i);
          for (unsigned int s = 0; s < p.getProblem(i)->getNumSolutions(); s++) {

            printf("%03u: ", s+1);
            const assembly_c * a = p.getProblem(i)->getSavedSolution(s)->getAssembly();

            unsigned int pnum = 0;

            for (unsigned int pie = 0; pie < p.getProblem(i)->getNumberOfParts(); pie++) {
              for (unsigned int pp = 0; pp < p.getProblem(i)->getPartMaximum(pie); pp++) {
                if (a->isPlaced(pnum)) {
                  printf("S%u ", p.getProblem(i)->getShapeIdOfPart(pie)+1);
                }
                pnum++;
              }
            }

            printf("\n");
            if (what == W_SOLUTION_ASSM)
              print(a, p.getProblem(i));
          }
        }


        break;

    }

    return 0;
  }

  if (allProblems)
    {
      firstProblem = 0;
      lastProblem = p.getNumberOfProblems();
    }
  else
    {
      firstProblem = problem;
      lastProblem = problem+1;
    }
  if (assemble) {

    for (unsigned int i = 0; i < p.getNumberOfShapes(); i++)
      p.getShape(i)->initHotspot();

    if (!quiet && !jsonOutput) {
      cout << " The puzzle:\n\n";
      print(&p);
    }

    json_result_c jsonStats;

    for (unsigned int pr = firstProblem ; pr < lastProblem ; pr++) {

      problem_c * problem = p.getProblem(pr);

      if (sliding::isSliding(*problem)) {
        sliding::syncMaxHoles(*problem);
        sliding::refreshStartLocks(*problem);
        strictColors = true;
      }

      auto assm = p.getGridType()->findAssembler(*problem, jsonOutput, solverType);
      if (threads > 0)
        assm->setNumThreads(threads);

      switch (assm->createMatrix(false, false, false, strictColors)) {
      case assembler_c::ERR_TOO_MANY_UNITS:
        if (jsonOutput)
          fprintf(stderr, "%i units too many for the result shape\n", assm->getErrorsParam());
        else
          printf("%i units too many for the result shape\n", assm->getErrorsParam());
        return jsonOutput ? 1 : 0;
      case assembler_c::ERR_TOO_FEW_UNITS:
        if (jsonOutput)
          fprintf(stderr, "%i units too few for the result shape\n", assm->getErrorsParam());
        else
          printf("%i units too few for the result shape\n", assm->getErrorsParam());
        return jsonOutput ? 1 : 0;
      case assembler_c::ERR_CAN_NOT_PLACE:
        if (jsonOutput)
          fprintf(stderr, "Piece %i can be place nowhere in the result shape\n", assm->getErrorsParam());
        else
          printf("Piece %i can be place nowhere in the result shape\n", assm->getErrorsParam());
        return jsonOutput ? 1 : 0;
      case assembler_c::ERR_NONE:
        /* no error case */
        break;
      case assembler_c::ERR_PUZZLE_UNHANDABLE:
        if (jsonOutput)
          fprintf(stderr, "The puzzles contains features not yet supported by burrTxt\n");
        else
          printf("The puzzles contains features not yet supported by burrTxt\n");
        return jsonOutput ? 1 : 0;
      case assembler_c::ERR_CAN_NOT_RESTORE_VERSION:
      case assembler_c::ERR_CAN_NOT_RESTORE_SYNTAX:
      case assembler_c::ERR_CAN_NOT_RESTORE_INTERRUPTED:
        /* all other errors should not occur */
        if (jsonOutput)
          fprintf(stderr, "Oops internal error\n");
        else
          printf("Oops internal error\n");
        return jsonOutput ? 1 : 0;
      }

#ifndef _WIN32
      stderr_redirect_c stderrQuiet(jsonOutput);
#endif

      const auto solveStart = std::chrono::steady_clock::now();

      if (reduce) {
        if (!quiet && !jsonOutput)
          cout << "start reduce\n\n";
        assm->reduce();
        if (!quiet && !jsonOutput)
          cout << "finished reduce\n\n";
      }

      if (allProblems && !jsonOutput)
        cout << "problem: " << problem->getName() << endl;

      asm_cb a(problem);

      d.reset();
      if (disassemble && !sliding::isSliding(*problem))
        d = createDisassembler(*problem, checkRotations, solverType);

      if (solverType == SOLVER_BT2)
        bt2Assemble(assm.get(), &a, bt2ChooseAssemblerWorkers(assm.get()));
      else
        assm->assemble(&a);

      const double solveSeconds = std::chrono::duration<double>(
          std::chrono::steady_clock::now() - solveStart).count();

      if (jsonOutput) {
        jsonStats.merge(a);
        jsonStats.addSolveTime(solveSeconds);
      } else {
        char timeBuf[64];
        snprintf(timeBuf, sizeof(timeBuf), "%.3f", solveSeconds);
        cout << a.Assemblies << " assemblies and " << a.Solutions
             << " solutions found with " << assm->getIterations()
             << " iterations in " << timeBuf << " seconds";

        if (newline)
          cout << endl;
      }

      d.reset();
    }

    if (jsonOutput)
      print_json_result(jsonStats);
  } else {

    for (unsigned int pr = firstProblem ; pr < lastProblem; pr ++) {

      problem_c * problem = p.getProblem(pr);

      const bool slide = sliding::isSliding(*problem);
      if (!slide)
        d = createDisassembler(*problem, checkRotations, solverType);

      for (unsigned int sol = 0; sol < problem->getNumberOfSavedSolutions(); sol++) {

        if (problem->getSavedSolution(sol)->getAssembly()) {

          auto da = slide
              ? sliding::findSlidePath(*problem, *problem->getSavedSolution(sol)->getAssembly())
              : d->disassemble(problem->getSavedSolution(sol)->getAssembly());

          if (da) {
            if (printSolutions)
              print(problem->getSavedSolution(sol)->getAssembly(), problem);

            if (!quiet)
              printf("level: %u\n", da->getMoves());

            if (printDisassemble)
              print(da.get(), problem->getSavedSolution(sol)->getAssembly(),problem);
          }
        }
      }

      d.reset();
    }
  }

  return 0;
}

