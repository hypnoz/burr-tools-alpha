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
#include "lib/helperpool.h"
#include "lib/solvertype.h"
#include "lib/bt2_assemble.h"
#include "lib/disassembly.h"
#include "lib/print.h"
#include "lib/voxel.h"
#include "lib/solution.h"
#include "lib/sliding.h"
#include "lib/stacking.h"
#include "lib/panex.h"
#include "tools/xml.h"
#include "tools/gzstream.h"

#include <stdlib.h>
#include <string.h>

#ifndef _WIN32
#include <fcntl.h>
#include <unistd.h>
#endif

#include <atomic>
#include <chrono>
#include <csignal>
#include <fstream>
#include <iostream>
#include <map>
#include <mutex>
#include <thread>
#include <cstdio>
#include <string>

using namespace std;

static unsigned int slideStates(void);

bool disassemble;
bool checkRotations;
bool strictColors;
bool nestedSlides;
bool highMemory;
/* Set from --solver once the file shows a sliding puzzle. */
bool deepSearch;
bool fullSearch;
/* --solver panex, for stacking puzzles with Panex columns. */
bool panexSolver;
/* --restart: discard a saved Panex search instead of carrying it on. */
bool panexRestart;
/* --panex-dir, --panex-disk: where saved Panex searches go, and how much disk they may take. */
std::string panexDir;
double panexDiskGB = 0;

/* Ctrl-C during a Panex search stops it and saves it; a second one quits at once. */
static std::atomic<bool> panexStop{false};
static void onInterrupt(int) {
  if (panexStop.load())
    std::_Exit(130);
  panexStop.store(true);
}

/* The puzzle type decides which solvers and options apply. */
enum puzzleKind_e { PK_BRICK, PK_SLIDING, PK_STACKING, PK_ANY };
bool allProblems;
bool printDisassemble;
bool printSolutions;
bool quiet;
bool jsonOutput;
solverType_e solverType;

/* Threads a take-apart spreads a level of its search over. Before d, so
 * that it outlives the disassembler that uses it. */
std::unique_ptr<helperPool_c> helperPool;
/* --progress: say on stderr, once a second, what the solve is doing */
bool showProgress = false;
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
  /* Assemblies and Solutions again, for the thread that prints progress */
  std::atomic<unsigned long> seenAssemblies{0}, seenSolutions{0};
  std::mutex cbMutex;
  std::map<std::string, unsigned int> slideBestMoves;
  /* Sliding starts whose search stopped at its limit, not at its end. */
  unsigned int slideStartsCut = 0;
  bool slideMemoryCut = false;
  std::string slideDiskError;

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
    seenAssemblies.fetch_add(1, std::memory_order_relaxed);

    if (disassemble) {

      /* Sliding "disassemble" is the start-to-goal slide, not brick take-apart. */
      std::unique_ptr<separation_c> da;
      if (sliding::isSliding(*puzzle)) {
        sliding::slideSearch_c search;
        search.maxStates = slideStates();
        search.nested = nestedSlides;
        search.highMemory = highMemory;
        da = sliding::findSlidePath(*puzzle, *a, search);
        if (search.outcome == sliding::SLIDE_LIMIT || search.outcome == sliding::SLIDE_MEMORY ||
            search.outcome == sliding::SLIDE_DISK)
          slideStartsCut++;
        if (search.outcome == sliding::SLIDE_MEMORY)
          slideMemoryCut = true;
        if (search.outcome == sliding::SLIDE_DISK)
          slideDiskError = search.error;
      } else {
        da = d->disassemble(a.get());
      }

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

  /* A stacking problem is one start; solved or not, with its transfers. */
  void addStacking(bool solved, unsigned int moves) {
    assemblies++;
    if (!solved)
      return;
    solutions++;
    if (!hasBest || (int)moves < bestLevel) {
      hasBest = true;
      snprintf(bestDotlevel, sizeof(bestDotlevel), "%u", moves);
      bestLevel = (int)moves;
      bestTotalmoves = (int)moves;
      bestMoves = (int)moves;
      bestRotations = 0;
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

static unsigned int slideStates(void) {
  if (fullSearch)
    return sliding::FULL_SEARCH;
  return deepSearch ? sliding::DEEP_SEARCH_STATES : sliding::SEARCH_STATES;
}

static const char * kindName(puzzleKind_e k) {
  switch (k) {
    case PK_SLIDING: return "sliding";
    case PK_STACKING: return "stacking";
    default: return "brick";
  }
}

/* Lower case, letters and digits only, so "Sliding Fast Solver (250k depth)"
 * and "sliding-fast" both match. */
static std::string squash(const char * s) {
  std::string out;
  for (; s && *s; s++) {
    char c = *s;
    if (c >= 'A' && c <= 'Z')
      c = (char)(c - 'A' + 'a');
    if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9'))
      out += c;
  }
  return out;
}

void usage(puzzleKind_e kind = PK_ANY) {

  cout << "burrTxt [options] file [options]\n\n";
  cout << "  file: puzzle file with the puzzle definition to solve\n";
  cout << "  The file's puzzle type (brick, sliding or stacking) decides which solvers\n";
  cout << "  and options apply. Brick covers every assembly and take-apart grid.\n\n";
  cout << "  -h [TYPE], --help [TYPE]\n";
  cout << "          show this help; with TYPE brick, sliding or stacking, also show\n";
  cout << "          that type's options and solvers\n";
  cout << "  --json  machine-readable result for batch tools (implies -d -q -r;\n";
  cout << "          prints one JSON object with the highest disassembly level.\n";
  cout << "          Fields: assemblies, solutions, dotlevel, level, totalmoves,\n";
  cout << "          solvetime (seconds); with -R also moves (linear) and rotations)\n";
  cout << "  --progress\n";
  cout << "          print to stderr, once a second, how far the solve is (with\n";
  cout << "          --json stderr is silenced, so leave that out to see it)\n";
  cout << "  Short options may be combined (e.g. -dR, -rq).\n";
  cout << "  -d      solve: take apart (brick), find the slide path (sliding) or the rod\n";
  cout << "          transfers (stacking); only print what solves\n";
  cout << "  -p      print the moves of each solution\n";
  cout << "  -s      print the assembly\n";
  cout << "  -q      be quiet and only print statistics\n";
  cout << "  -n      don't print a newline at the end of the line\n";
  cout << "  -o n    select the problem to solve\n";
  cout << "  -o all  solves all problems in file\n";
  cout << "  -x      only redisassemble the given solutions\n";
  cout << "  --solver TYPE\n";
  cout << "          solver to use; the choices depend on the puzzle type (see -h TYPE).\n";
  cout << "          Quotes are needed if TYPE has spaces; case and spaces are ignored.\n";
  cout << "  -a      ask for information about the current puzzle, the next letters must be:\n";
  cout << "     s0   print solutions with the only the used pieces\n";
  cout << "     s1   print solutions including the assemblies\n";
  cout << "     c    print comment\n";

  if (kind == PK_ANY) {
    cout << "\n  For the options and solvers of one puzzle type:\n";
    cout << "    burrTxt -h brick      burrTxt -h sliding      burrTxt -h stacking\n";
    return;
  }

  cout << "\n" << kindName(kind) << " puzzles:\n";
  if (kind == PK_BRICK) {
    cout << "  -r      reduce the placements before starting to solve the puzzle\n";
    cout << "  -R      also try 90 degree piece rotations during disassembly (implies -d)\n";
    cout << "  -C      strict color restrictions: a voxel fits only a result voxel of the same color\n";
    cout << "          (a colored voxel does not also fit a neutral one, and a neutral voxel\n";
    cout << "          fits only a neutral result voxel)\n";
    cout << "  -t n    set number of worker threads for assembler (0 = auto)\n";
    cout << "  --solver TYPE   (default: BurrTools Classic)\n";
    cout << "            \"BurrTools Classic\"  complete take-apart (also: classic)\n";
    cout << "            \"Andrew Crowell\"     90° take-apart heuristics (also: crowell)\n";
    cout << "            \"BurrTools 2\"        Classic take-apart + dancing-cells assembly (also: bt2)\n";
  } else if (kind == PK_SLIDING) {
    cout << "  -r      reduce the placements before finding the start layouts\n";
    cout << "  -t n    set number of worker threads for finding start layouts and for the\n";
    cout << "          Sliding Full Solver (0 = auto)\n";
    cout << "  --nested-slides\n";
    cout << "          a piece may carry the pieces nested inside its outline, such as a\n";
    cout << "          piece in another piece's pocket. Pieces that only touch never move\n";
    cout << "          together.\n";
    cout << "  --high-memory\n";
    cout << "          let the search hold up to half of this computer's memory in\n";
    cout << "          arrangements, instead of stopping at about 2 GB\n";
    cout << "  --solver TYPE   (default: Sliding Full Solver)\n";
    cout << "            \"Sliding Fast Solver\"  search up to 250,000 arrangements (also: fast)\n";
    cout << "            \"Sliding Deep Solver\"  search up to 1,000,000 arrangements (also: deep)\n";
    cout << "            \"Sliding Full Solver\"  no limit: finds the fewest moves or proves there is\n";
    cout << "                                   no solution, on every core (-t n to limit);\n";
    cout << "                                   keeps only the newest moves in memory and the\n";
    cout << "                                   rest on disk, so free disk space limits it\n";
    cout << "                                   (also: full)\n";
    cout << "  The Sliding Full Solver keeps its first 256 MB of older moves in memory, then\n";
    cout << "  writes to $BURRTOOLS_SLIDE_DIR, or the user's cache folder, and deletes its\n";
    cout << "  files when done. BURRTOOLS_SLIDE_DISK_GB limits the disk it may take (default:\n";
    cout << "  all but 2 GB or a twentieth of the disk, but at least 1 GB or half the free\n";
    cout << "  space, whichever is less).\n";
  } else {
    cout << "  The start and goal stacks come from the file. A move is one disc going\n";
    cout << "  from the top of one rod to the top of another. With Panex columns a\n";
    cout << "  disc of size n sits at most n places below the top of its rod.\n";
    cout << "  --high-memory\n";
    cout << "          let the search hold up to half of this computer's memory in\n";
    cout << "          stackings, instead of stopping at about 2 GB\n";
    cout << "  --solver TYPE   (default: Stacking Solver)\n";
    cout << "            \"Stacking Solver\"  fewest rod transfers (also: stacking)\n";
    cout << "            \"Panex Solver\"     fewest rod transfers for Panex columns, built for\n";
    cout << "                                large towers: searches from both ends, holds only the\n";
    cout << "                                newest levels and uses every core (-t n to limit)\n";
    cout << "                                (also: panex)\n";
    cout << "  Panex Solver searches are saved when stopped (Ctrl-C), when they run out\n";
    cout << "  of memory, and every 20 minutes; the same command carries a saved search on.\n";
    cout << "  --restart       discard a saved search and start over\n";
    cout << "  --panex-dir DIR keep saved searches in DIR (default: the user's cache folder)\n";
    cout << "  --panex-disk GB disk saved levels may take (default: half the free space,\n";
    cout << "                  at most 200); less disk makes tracing the path back slower\n";
  }
}

/* --solver for the puzzle's type. False, with a message, when TYPE belongs
 * to another type or to none. */
static bool pickSolver(puzzleKind_e kind, const char * name) {
  const std::string n = squash(name);
  if (kind == PK_SLIDING) {
    if (n == "fast" || n == "slidingfast" || n == "slidingfastsolver" ||
        n == "slidingfastsolver250kdepth") {
      deepSearch = false;
      fullSearch = false;
      return true;
    }
    if (n == "deep" || n == "slidingdeep" || n == "slidingdeepsolver" ||
        n == "slidingdeepsolver1mildepth") {
      deepSearch = true;
      fullSearch = false;
      return true;
    }
    if (n == "full" || n == "slidingfull" || n == "slidingfullsolver" ||
        n == "slidingfullsolverfulldepth") {
      deepSearch = false;
      fullSearch = true;
      return true;
    }
    fprintf(stderr, "burrTxt: unknown solver '%s' for a sliding puzzle\n", name);
    fprintf(stderr, "         use \"Sliding Fast Solver\", \"Sliding Deep Solver\" or \"Sliding Full Solver\"\n");
    fprintf(stderr, "         (also: fast, deep, full)\n");
    return false;
  }
  if (kind == PK_STACKING) {
    if (n == "stacking" || n == "stackingsolver") {
      panexSolver = false;
      return true;
    }
    if (n == "panex" || n == "panexsolver") {
      panexSolver = true;
      return true;
    }
    fprintf(stderr, "burrTxt: unknown solver '%s' for a stacking puzzle\n", name);
    fprintf(stderr, "         use \"Stacking Solver\" or \"Panex Solver\" (also: stacking, panex)\n");
    return false;
  }
  if (solverTypeFromName(name, &solverType))
    return true;
  fprintf(stderr, "burrTxt: unknown solver '%s' for a brick puzzle\n", name);
  fprintf(stderr, "         use \"BurrTools Classic\", \"Andrew Crowell\", or \"BurrTools 2\"\n");
  fprintf(stderr, "         (also: classic, crowell, bt2)\n");
  return false;
}

/* One line per rod transfer of a stacking path: lift, cross and drop are
 * three states, and a disc's x is the x of the rod it is on. */
static void printStackPlan(const separation_c & path, const problem_c & prob) {
  const std::vector<int> rodX = stacking::layoutBoard(prob, false).rodX;
  const stacking::rodSet_c & board = prob.getPuzzle().getRodSet(prob.getRodSetId());
  auto rodAt = [&](int x) {
    for (unsigned int r = 0; r < rodX.size(); r++)
      if (rodX[r] == x)
        return stacking::isPocket(board, r) ? std::string("pocket") : "rod " + std::to_string(r + 1);
    return std::string("?");
  };
  std::vector<std::string> names;
  for (unsigned int part = 0; part < prob.getNumberOfParts(); part++)
    for (unsigned int j = 0; j < prob.getPartMaximum(part); j++) {
      std::string nm = "S" + std::to_string(prob.getShapeIdOfPart(part) + 1);
      if (prob.getPartMaximum(part) > 1)
        nm += " copy " + std::to_string(j + 1);
      names.push_back(nm);
    }
  const unsigned int n = path.getPieceNumber();
  const unsigned int moves = stacking::logicalMoves(path);
  for (unsigned int m = 0; m < moves; m++) {
    const state_c * a = path.getState(m * stacking::STEPS_PER_MOVE);
    const state_c * b = path.getState((m + 1) * stacking::STEPS_PER_MOVE);
    for (unsigned int i = 0; i < n; i++) {
      if (a->getX(i) == b->getX(i))
        continue;
      printf("%3u: %s from %s to %s\n", m + 1,
             i < names.size() ? names[i].c_str() : "?",
             rodAt(a->getX(i)).c_str(), rodAt(b->getX(i)).c_str());
      break;
    }
  }
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
  nestedSlides = false;
  highMemory = false;
  deepSearch = false;
  fullSearch = false;
  panexSolver = false;
  panexRestart = false;
  const char * solverName = nullptr;
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

      if (strcmp(args[i], "--progress") == 0) {
        showProgress = true;
        continue;
      }
      if (strcmp(args[i], "--json") == 0) {
        jsonOutput = true;
        disassemble = true;
        quiet = true;
        reduce = true;
      } else if (strcmp(args[i], "-h") == 0 || strcmp(args[i], "--help") == 0) {
        puzzleKind_e k = PK_ANY;
        if (i + 1 < argv) {
          const std::string t = squash(args[i+1]);
          if (t == "brick") k = PK_BRICK;
          else if (t == "sliding" || t == "slider") k = PK_SLIDING;
          else if (t == "stacking") k = PK_STACKING;
        }
        usage(k);
        return 0;
      } else if (strcmp(args[i], "--nested-slides") == 0) {
        nestedSlides = true;
      } else if (strcmp(args[i], "--restart") == 0) {
        panexRestart = true;
      } else if (strcmp(args[i], "--panex-dir") == 0 && i + 1 < argv) {
        panexDir = args[++i];
      } else if (strcmp(args[i], "--panex-disk") == 0 && i + 1 < argv) {
        panexDiskGB = atof(args[++i]);
      } else if (strcmp(args[i], "--high-memory") == 0) {
        highMemory = true;
      } else if (strcmp(args[i], "--solver") == 0) {
        if (i + 1 >= argv) {
          usage();
          return 2;
        }
        /* Checked once the file shows which puzzle type it is. */
        solverName = args[i+1];
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
        /* also for what picks its own number of threads */
        setSolveThreadLimit(threads);
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

  const puzzleKind_e kind = stacking::isStacking(p) ? PK_STACKING
                          : sliding::isSliding(p) ? PK_SLIDING
                          : PK_BRICK;
  if (nestedSlides && kind != PK_SLIDING) {
    fprintf(stderr, "burrTxt: --nested-slides is for sliding puzzles; this is a %s puzzle\n", kindName(kind));
    return 2;
  }
  if (highMemory && kind == PK_BRICK) {
    fprintf(stderr, "burrTxt: --high-memory is for sliding and stacking puzzles; this is a %s puzzle\n", kindName(kind));
    return 2;
  }
  if ((checkRotations || strictColors) && kind != PK_BRICK) {
    fprintf(stderr, "burrTxt: -R and -C are for brick puzzles; this is a %s puzzle\n", kindName(kind));
    return 2;
  }
  /* Sliding defaults to the full search, as in the GUI. */
  deepSearch = false;
  fullSearch = (kind == PK_SLIDING);
  if (solverName && !pickSolver(kind, solverName))
    return 2;

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
          /* The saved ones: a solution limit keeps fewer than were found. */
          for (unsigned int s = 0; s < p.getProblem(i)->getNumberOfSavedSolutions(); s++) {

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
      if (problem >= p.getNumberOfProblems()) {
        fprintf(stderr, "burrTxt: there is no problem %u; the file has %u (numbered from 0)\n",
                problem, p.getNumberOfProblems());
        return 2;
      }
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

      /* Stacking has no assembly: the start is the file's start stacks. */
      if (kind == PK_STACKING) {
        if (allProblems && !jsonOutput)
          cout << "problem: " << problem->getName() << endl;
        std::string err = stacking::setupError(*problem);
        if (!err.empty()) {
          if (jsonOutput)
            fprintf(stderr, "%s\n", err.c_str());
          else
            printf("%s\n", err.c_str());
          return jsonOutput ? 1 : 0;
        }
        const auto stackStart = std::chrono::steady_clock::now();
        std::unique_ptr<separation_c> path;
        stacking::stackSearch_c search;
        search.highMemory = highMemory;
        panex::panexSearch_c panexSearch;
        panexSearch.highMemory = highMemory;
        panexSearch.threads = threads;
        /* As in the GUI: the Stacking Solver runs on the Panex search too
         * whenever that search can hold the puzzle; the older search is
         * for the rest (more rods or discs than it packs). */
        const bool usePanex = disassemble &&
            (panexSolver || panex::unsupported(*problem, true).empty());
        if (usePanex) {
          std::string why = panex::unsupported(*problem, !panexSolver);
          if (!why.empty()) {
            fprintf(stderr, "burrTxt: %s\n", why.c_str());
            return 2;
          }
          panexSearch.anyRules = !panexSolver;
          panexSearch.workDir = panexDir;
          panexSearch.diskBudget = (unsigned long long)(panexDiskGB * 1e9);
          panexSearch.stop = &panexStop;
          if (panexRestart)
            panex::discardSaved(*problem, panexDir);
          const std::string saved = panex::savedSearch(*problem, panexDir);
          if (!saved.empty() && !jsonOutput)
            cout << "continuing " << saved << endl;
          panexSearch.resume = true;
          panexStop.store(false);
          void (*was)(int) = std::signal(SIGINT, onInterrupt);
          path = panex::solve(*problem, panexSearch);
          std::signal(SIGINT, was);
        } else if (disassemble)
          path = stacking::findStackPath(*problem, search);
        const double secs = std::chrono::duration<double>(
            std::chrono::steady_clock::now() - stackStart).count();
        const unsigned int moves = path ? stacking::logicalMoves(*path) : 0;
        if (jsonOutput) {
          jsonStats.addStacking(path != nullptr, moves);
          jsonStats.addSolveTime(secs);
        } else {
          if (path && (!quiet || allProblems))
            printf("level: %u\n", moves);
          if (path && printDisassemble)
            printStackPlan(*path, *problem);
          char timeBuf[64];
          snprintf(timeBuf, sizeof(timeBuf), "%.3f", secs);
          cout << "1 assemblies and " << (path ? 1 : 0) << " solutions found in "
               << timeBuf << " seconds";
          if (!disassemble)
            cout << " (use -d to search for the moves)";
          if (newline)
            cout << endl;
          if (usePanex) {
            cout << (panexSolver ? "panex search: " : "stack search: ") << panexSearch.depthReached << " moves deep, "
                 << panexSearch.found << " stackings, peak "
                 << panexSearch.peakMemory / 1048576 << " MB in memory";
            if (panexSearch.diskBytes)
              cout << ", " << panexSearch.diskBytes / 1048576 << " MB on disk ("
                   << (panexSearch.levelInterval == 1
                           ? std::string("every level")
                           : "every " + std::to_string(panexSearch.levelInterval) + " levels")
                   << ")";
            cout << endl;
            if (!path && panexSearch.saved)
              cout << (panexSearch.outcome == panex::PANEX_MEMORY
                           ? "the search outgrew its memory"
                           : std::string("stopped"))
                   << "; it is saved: run the same command again to carry on"
                   << (panexSearch.outcome == panex::PANEX_MEMORY && !highMemory
                           ? " (with --high-memory)" : "")
                   << ", or add --restart to start over" << endl;
            else if (!path)
              cout << (panexSearch.outcome == panex::PANEX_MEMORY
                           ? "the search outgrew its memory" +
                                 std::string(highMemory ? "" : " (try --high-memory)")
                           : panexSearch.outcome == panex::PANEX_ERROR
                           ? panexSearch.error
                           : std::string("every reachable stacking was searched: no solution"))
                   << endl;
          } else if (disassemble && !path)
            cout << (search.outcome == stacking::STACK_MEMORY
                         ? "the search reached its memory limit, so a solution may be missing" +
                               std::string(highMemory ? "" : " (try --high-memory)")
                         : std::string("every reachable stacking was searched: no solution"))
                 << endl;
        }
        continue;
      }

      if (sliding::isSliding(*problem)) {
        sliding::syncMaxHoles(*problem);
        sliding::refreshStartLocks(*problem);
        strictColors = true;
      }

      auto assm = p.getGridType()->findAssembler(*problem, jsonOutput, solverType);
      if (threads > 0)
        assm->setNumThreads(threads);

      /* A sliding start is judged against a fixed goal map, so rotated and
       * mirrored starts are different starts. Keep them all. */
      const bool keepAll = sliding::isSliding(*problem);
      switch (assm->createMatrix(keepAll, keepAll, false, strictColors)) {
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
      if (disassemble && !sliding::isSliding(*problem)) {
        d = createDisassembler(*problem, checkRotations, solverType);
        if (!helperPool)
          helperPool = std::make_unique<helperPool_c>(threads > 0 ? threads : solveThreadBudget());
        if (d)
          d->setHelperPool(helperPool.get());
      }

      /* --progress: a line a second to stderr saying how far the assembly
       * search is and what the take-apart under way is doing */
      std::atomic<bool> progressDone{false};
      std::thread progressThread;
      if (showProgress)
        progressThread = std::thread([&]() {
          unsigned int tick = 0;
          while (!progressDone.load(std::memory_order_relaxed)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            if (++tick % 10)
              continue;
            const double t = std::chrono::duration<double>(
                std::chrono::steady_clock::now() - solveStart).count();
            fprintf(stderr, "%7.1f s  assembly %5.1f%%, %lu found", t,
                    100.0 * assm->getFinished(), a.seenAssemblies.load(std::memory_order_relaxed));
            disassemblyProgress_c dp;
            if (d && d->getProgress(dp) && dp.active) {
              fprintf(stderr, "  disassemble %lu: level %u: %lu/%lu, %llu positions",
                      a.seenAssemblies.load(std::memory_order_relaxed), dp.level,
                      dp.levelDone, dp.levelSize, dp.nodes);
              if (dp.pieces > 2)
                fprintf(stderr, ", split %u/%u", dp.separations, dp.pieces - 1);
              if (dp.threads > 1)
                fprintf(stderr, ", %u threads", dp.threads);
            }
            fprintf(stderr, "\n");
          }
        });
      struct progressEnd_c {
        std::atomic<bool> & done;
        std::thread & t;
        ~progressEnd_c() {
          done.store(true, std::memory_order_relaxed);
          if (t.joinable())
            t.join();
        }
      } progressEnd{progressDone, progressThread};

      if (solverType == SOLVER_BT2)
        bt2Assemble(assm.get(), &a, bt2ChooseAssemblerWorkers(assm.get()));
      else
        assm->assemble(&a);

      progressDone.store(true, std::memory_order_relaxed);
      if (progressThread.joinable())
        progressThread.join();

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

        /* Whether "0 solutions" is proven, or the search gave up. */
        if (disassemble && sliding::isSliding(*problem)) {
          if (a.slideStartsCut > 0)
            cout << a.slideStartsCut << " start layout(s) not searched completely: "
                 << (a.slideDiskError.empty() ? std::string("the search limit was reached")
                                              : a.slideDiskError.substr(0, a.slideDiskError.size() - 1))
                 << ", so solutions may be missing"
                 << (a.slideMemoryCut ? (highMemory ? "" : " (try --high-memory)")
                                      : (fullSearch ? "" : " (try --solver full)")) << endl;
          else if (a.Solutions == 0 && a.Assemblies > 0)
            cout << "every reachable arrangement was searched: no solution"
                 << (nestedSlides ? "" : " (without --nested-slides)") << endl;
        }
      }

      d.reset();
    }

    if (jsonOutput)
      print_json_result(jsonStats);
  } else {

    for (unsigned int pr = firstProblem ; pr < lastProblem; pr ++) {

      problem_c * problem = p.getProblem(pr);

      if (kind == PK_STACKING) {
        if (problem->getNumberOfSavedSolutions() == 0)
          continue;
        auto path = stacking::findStackPath(*problem);
        if (path) {
          if (!quiet)
            printf("level: %u\n", stacking::logicalMoves(*path));
          if (printDisassemble)
            printStackPlan(*path, *problem);
        }
        continue;
      }

      const bool slide = sliding::isSliding(*problem);
      if (!slide) {
        d = createDisassembler(*problem, checkRotations, solverType);
        if (!helperPool)
          helperPool = std::make_unique<helperPool_c>(threads > 0 ? threads : solveThreadBudget());
        if (d)
          d->setHelperPool(helperPool.get());
      }

      for (unsigned int sol = 0; sol < problem->getNumberOfSavedSolutions(); sol++) {

        if (problem->getSavedSolution(sol)->getAssembly()) {

          std::unique_ptr<separation_c> da;
          if (slide) {
            sliding::slideSearch_c search;
            search.maxStates = slideStates();
            search.nested = nestedSlides;
            search.highMemory = highMemory;
            da = sliding::findSlidePath(*problem, *problem->getSavedSolution(sol)->getAssembly(), search);
          } else {
            da = d->disassemble(problem->getSavedSolution(sol)->getAssembly());
          }

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

