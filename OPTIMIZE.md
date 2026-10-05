# OPTIMIZE.md — staged cleanup plan

This file lists the larger cleanup and architecture work found in a code
review on 2026-10-03 (branch `panex`). The small fixes from that review are
already applied (see "Done in this pass" at the end).

## How to use this file (for the AI applying it)

- Work one item at a time, in stage order. Items in the same stage do not
  depend on each other.
- Before starting, read the item's files. Line numbers are from 2026-10-03
  and will drift, so search for the function names given.
- After each item: `just build`, `just test`, `just check` (cppcheck; 5
  known findings are intentional, listed under "Known cppcheck findings").
  Where an item says so, also run the GUI by hand.
- When an item is finished and verified, change its `- [ ]` to `- [x]` and
  add one line under it: `Done YYYY-MM-DD: <what changed, anything left>`.
- Do not commit unless the user asks. Commit messages end with
  `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.
- `just test-regression` had failures before this review. Do not treat
  them as caused by your change; compare against the baseline in item 1.1.

---

## Stage 1 — Safety net and correctness (do first)

- [x] **1.1 Record the test-regression baseline**
  Done 2026-10-03: all 7 failures were one cause — the reference holds
  0.7.1 move notation, the fork prints one segment per piece removal (by
  design, commit c3403689). Assembly/solution counts all matched. Added
  test/known_good/examples_fork_moves.json (fork move strings for those 7
  puzzles) and load_fork_moves() in test_examples_regression.py. Now 18/18
  pass. If disassembly notation changes again, regenerate that file.
  - Problem: `just test-regression` fails on some puzzles from before this
    work, so nobody can tell whether a change breaks it further.
  - Steps: run `just test-regression 2>&1 | tee tmp/regression-baseline.txt`.
    List the failing puzzles in this item. For each, decide whether the
    expected data is stale (the solver now finds a different but valid
    count, e.g. after rotation-rule changes) or a real bug. Fix the stale
    expectations or write a separate item for each real bug.
  - Verify: `just test-regression` matches the recorded baseline.
  - Risk: low (tests only).

- [x] **1.2 GUI reads the assembler from the GUI thread while it runs**
  Done 2026-10-03: added solveThread_c::assemblerFinished() (reads the
  published `assm` atomic). The progress bar uses it while the solver runs
  for that problem; "optimize piece" uses currentActionParameter() only.
  Placement browser returns early when there is no assembler or a solver
  is running (it would dereference null). Not tried under TSan.
  - Where: `src/gui/mainwindow.cpp`, the update function that handles
    `solveThread_c::ACT_REDUCE` (search `optimize piece`).
  - Problem: it calls `pr->getAssembler()->getReducePiece()` while the
    solver thread may be creating or deleting that assembler. A data race;
    can crash if the assembler is freed between the check and the call.
  - Steps: have the solver thread publish the reduce piece itself. In
    `src/lib/solvethread.cpp`, where the action becomes `ACT_REDUCE`, store
    the piece in the same atomic that backs `currentActionParameter()` (or
    add an atomic `reducePiece` to solveThread_c, updated from the
    assembler's reduce loop through a callback). Then the GUI uses only
    `assmThread->currentActionParameter()` and the `pr->getAssembler()`
    branch is removed. Also grep the GUI for any other
    `getAssembler()` call made while `assmThread` is running.
  - Verify: solve a big puzzle (examples/SolidSixPieceBurrs.xmpuzzle) and
    watch "optimize piece N" count up. Build once with
    `-Db_sanitize=thread` if possible and check for no report there.
  - Risk: low.

- [x] **1.3 Panex saved state keeps seconds, not milliseconds**
  Done 2026-10-03: state.bin magic is now BTPANEX4 with `ms`; BTPANEX3
  files load with seconds × 1000. `panex::savedSeconds` → `savedMs`;
  solvethread uses addTimeMs. Test in test_stacking.cpp ("a stopped search
  carries on") rewrites a save as v3 and reads it back. GUI pause/resume not
  tried by hand.
  - Where: `src/lib/panex.cpp` — state struct (`uint64_t seconds`, around
    line 344), `put/get` of `st.seconds` (~446, ~477), `secondsSoFar()`
    (~559); `panex::savedSeconds()` in `panex.h`; its callers in
    `src/lib/solvethread.cpp` and the GUI.
  - Problem: everything else stores Time used in milliseconds (`usedMs`,
    `timeMs` attribute). A Panex run resumed from disk loses up to a second
    per pause, and the conversion code is special-cased.
  - Steps: add a state.bin version bump; store `milliseconds`; when reading
    an old version, multiply by 1000. Rename `savedSeconds` to `savedMs`
    and update callers so they no longer convert.
  - Verify: `./build/test_burrtools "[panex]"`; pause and resume a Panex 6
    run in the GUI and check Time used carries over. Load a state.bin saved
    by the old build (keep one in tmp/ before changing the code).
  - Risk: medium (on-disk format). Keep reading the old format.

- [x] **1.4 Fix the docs that name things that do not exist**
  Done 2026-10-03: bench/ is on `upstream/master` only (never merged into
  this fork) and needs upstream's `puzzles/BTFiles` corpus, so it was not
  imported. AGENTS.md (CLAUDE.md is a symlink to it): removed test-slow,
  test-all described as same as test, rule 7 softened, and a note at the
  top of section 4 with a burrTxt --json timing loop. If upstream is
  merged later, revert that note.
  - Where: `CLAUDE.md` and `AGENTS.md`.
  - Problem: they describe `bench/` (bench_solve.py, run_suite.sh,
    run_snapshot.sh), `just bench` and `just test-slow`. None exist. An AI
    following them will waste time or invent results.
  - Steps: ask the user whether the bench directory exists elsewhere (it
    may be on another branch). If so, restore it; if not, remove those
    sections, or replace them with a short note on how to time a solve
    with `./build/burrTxt` on the examples. Remove `test-slow` or add a
    recipe for it.
  - Verify: every `just` recipe named in the docs appears in `just --list`.
  - Risk: none.

## Stage 2 — Efficiency

- [x] **2.1 Live sort re-sorts the whole list after every assembly**
  Done 2026-10-03 (different approach from below): problem_c tracks
  `sortedBy`/`sortedUpTo` (forgotten on clear, edit erases and solver-method
  sorts; adjusted by removeSolution and positional inserts). New
  `keepSolutionsSorted(by)` places only the new tail (upper_bound + rotate,
  or stable_sort + inplace_merge for >64) and falls back to a full sort when
  the method changed. applyLiveSort uses it. Two tests in
  test_puzzle_model.cpp ("keepSolutionsSorted …"). GUI not tried by hand.
  - Where: `src/lib/solvethread.cpp`, `solveThread_c::applyLiveSort()`
    (~line 792), called from the assembly callback (~787) and two other
    places (~902, ~940).
  - Problem: the comment says the list is bounded by the solution limit,
    but with no limit set it grows without bound, so each new solution
    costs O(n log n) and holds the solutions lock while it sorts. With
    100k solutions this dominates.
  - Steps: replace the full sort per solution with a sorted insert. Add
    `problem_c::insertSolutionSorted(sol, method)` that binary-searches
    with the same comparison that `sortSolutions` uses (`std::upper_bound`)
    and inserts there. In the callback, when `liveSort >= 0`, use it
    instead of `addSolution` + `applyLiveSort`. Keep the full sort for the
    moment the user picks a sort method (the GUI already calls it).
    Check the "keep only the best N" path (solution limit with a drop
    method) still removes the right entry.
  - Verify: `just test`; in the GUI, sort by "moves" during a solve of a
    puzzle with many solutions and check the order matches a re-sort
    afterwards.
  - Risk: medium. The comparison must be identical to sortSolutions'.

- [x] **2.2 GUI update loop polls once every half second**
  Done 2026-10-03 (simpler than planned): the old loop timed with
  `time(0)` (1 s granularity), so update() ran every 0.5–1.5 s. main.cpp
  now has runWithUpdates(): steady_clock, update() every 500 ms, Fl::wait
  sleeps until then. Kept the loop in main instead of Fl::add_timeout on
  purpose: an assert_exception from update() must reach main's rescue save
  without unwinding through the macOS run loop. Not tried by hand — check
  Solve/Pause/Continue/Stop and that the progress moves smoothly.
  - Where: `src/gui/main.cpp` (~line 66, `wait(0.5)`), and the update
    function in mainwindow.cpp it drives.
  - Problem: the solver display refreshes only when the loop wakes, so
    progress looks jumpy and the window feels slow to react after a solve
    ends. It also wakes when nothing is running.
  - Steps: register an `Fl::add_timeout(0.25, cb)` that calls the update
    and re-arms itself with `Fl::repeat_timeout` only while a solver
    thread is running (start it when Solve/Continue is pressed). Main loop
    becomes plain `Fl::run()`. Have the solver thread call
    `Fl::awake()` when it finishes so the end is shown at once.
  - Verify: GUI by hand: solve, pause, continue, stop, finish. Idle CPU of
    the app is ~0%.
  - Risk: medium (GUI event flow).

## Stage 3 — Remove duplicated code

- [x] **3.1 Stacking rules exist twice** — decided against merging
  Done 2026-10-03: checked the two agree (panexTooDeep after adding the
  mover ⟺ the target column is raised; blocking by raised columns between
  is the same). moveAllowed works on vectors, rules_c on the packed hot
  path; sharing code would slow Panex for little gain. Instead added test
  "panex: the two solvers agree on the Panex swap" (n = 2, 3; no pocket)
  next to the existing Panex Jr (pocket) and plain-rod agreement tests, so
  drift fails the suite.
  - Where: `src/lib/stacking.cpp` `moveAllowed()` (~205) and the
    `rules_c` class in `src/lib/panex.cpp`.
  - Problem: both encode which transfers are legal (Panex columns, pocket
    column, heights). A rule fix must be made twice and they can drift.
  - Steps: move the rules into one class in `stacking.h`
    (`stacking::rules_c`) built once from `rodSet_c` + pieces. Make
    `moveAllowed` a thin wrapper over it, and have panex.cpp use it. Keep
    the hot path in panex inline (the check runs billions of times), so
    measure Panex 7 solve time before and after.
  - Verify: `./build/test_burrtools "[stacking]"` and `"[panex]"`; Panex 7
    time within 5% of before.
  - Risk: medium (performance).

- [x] **3.2 Retire the old stacking search (findStackPath)** — keep it
  Done 2026-10-03: not retired. panex::unsupported refuses > 8 rods
  (MAX_RODS) and > 31 discs / the symbol limit; findStackPath is the
  fallback there (solvethread.cpp, burrTxt.cpp). The GUI and burrTxt
  already route every puzzle panex can take to panex::solve.
  (Correction 2026-10-04: burrTxt did not — its Stacking Solver always used
  findStackPath. Fixed in R2.11.)
  - Where: `stacking::findStackPath` (stacking.h ~197/201, stacking.cpp);
    callers: `src/burrTxt.cpp` ~922 and ~1102, `src/lib/solvethread.cpp`
    ~179.
  - Problem: since `panex::solve` with `anyRules = true` handles plain
    stacking rules, there are two engines for the same job.
  - Steps: do 3.1 first. Run both engines on every stacking example and
    the test cases and confirm the same move counts. Then route all three
    callers through `panex::solve` with `anyRules = true`, keeping the
    Stacking Solver menu name. Keep findStackPath only if a test proves a
    case panex cannot do (note it here). Remove `stackSearch_c` if unused.
  - Verify: `just test`; burrTxt on a stacking example gives the same
    answer.
  - Risk: medium.

- [x] **3.3 Retire the sliding legacySearch fallback** — keep it
  Done 2026-10-03: not retired. buildTables refuses a piece with no cells
  or nowhere to go, a start off the floor, and > 64-bit position counts;
  legacySearch covers those. BURRTOOLS_SLIDE_LEGACY stays as the A/B and
  test hook (test_sliding.cpp reads it). Both engines pass all 18
  `[sliding]` tests (`BURRTOOLS_SLIDE_LEGACY=1 ./build/test_burrtools
  "[sliding]"`). A 128-bit key in tableSearch would remove only the last of
  those reasons; not worth it until a real puzzle needs it.
  - Where: `src/lib/sliding.cpp` `legacySearch()` (~975) and the
    `BURRTOOLS_SLIDE_LEGACY` switch.
  - Problem: kept for A/B runs and for puzzles whose state does not fit 64
    bits. Two engines to maintain.
  - Steps: find which puzzles need >64 bits. If the position-table engine
    can use a 128-bit key (as panex's `pkey_c` does) for those, add that
    and remove legacySearch and the env switch. If not, keep it only for
    that case and remove the env switch.
  - Verify: `./build/test_burrtools "[sliding]"`; compare move counts on
    all sliding examples against the old build.
  - Risk: medium.

- [x] **3.4 Save/resume logic duplicated in assembler_0 and assembler_1**
  Done 2026-10-03: new header-only src/lib/assembler_resume.h
  (assemblerResume::reader_c, tail_c, readTail, writeTail) holds the K / T /
  C / S tail; each assembler passes a lambda for its own task fields. Text
  written is unchanged. checkStopAndResume in test_solver.cpp now also checks
  save → restore → save gives the same position (signatures compared
  sorted: they come from an unordered_set, so their order was never
  stable, in the old code either). Behaviour kept: only assembler_0 sets
  totalTasks/completedTasks on restore.
  - Where: `src/lib/assembler_0.cpp`, `src/lib/assembler_1.cpp`: the
    SIMD and parallel save/resume code (save flags 2 and 3, task lists,
    completion flags, signatures, skip count K).
  - Steps: move the shared parts into a helper (e.g.
    `src/lib/assemblerresume.{h,cpp}`) with functions to write and read
    the `<pending>`/task XML. Keep the XML byte-for-byte identical. Each
    assembler keeps only what differs.
  - Verify: `just test`; save a paused solve with the old build, resume
    with the new one, and get the same solution count.
  - Risk: medium-high (save format). Do it with a saved file from the old
    build ready.

- [x] **3.5 Two copies of the main menu**
  Done 2026-10-03: every writer of menu_MainMenu (view-mode marks,
  Undo/Redo, Images/STL, Paused solver state) already updated the live
  menu through liveMenuIndex(); the static table was only "kept in sync".
  Removed menu_MainMenu, findMenuEntry and viewModeMenuIdx. initViewMenuIcons'
  `names` moved into the non-Apple branch. macOS build + `--self-check` OK;
  the Linux/Windows branch of initViewMenuIcons was not compiled here
  (check in CI). GUI not tried by hand: check Undo/Redo greying and the
  View radio marks.
  - Where: `mainWindow_c::menu_MainMenu[]` in mainwindow.cpp (~4812) and
    `menu_Portable[]` / `menu_Mac[]` in `src/gui/mainmenu.cpp`.
  - Problem: the old table is still used for view-mode check marks
    (`viewModeMenuIdx`, ~3996). New menu items are added in mainmenu.cpp
    only, so the old one is stale.
  - Steps: find what still reads menu_MainMenu. Make the view-mode marks
    use the items in the current menu (find them by callback or
    `find_item`), then delete menu_MainMenu and viewModeMenuIdx.
  - Verify: GUI by hand on macOS: switch view modes, check the marks.
  - Risk: low.

## Stage 4 — Memory and buffer safety (mechanical)

- [x] **4.1 Replace raw new/delete with owners** (~155 deletes remain)
  - Do one file per sub-step, in this order, building and testing after
    each:
    - [x] `src/lib/sliding.cpp` `slideRoute` (`delete v`) → unique_ptr (done 2026-10-03)
    - [x] `src/lib/movementanalysator.cpp` (8) — left as is (2026-10-03):
      all are refcounted or node-set-owned disassemblerNode_c; completeFind
      defers deletes on purpose because the node set still holds them.
    - [x] `src/lib/burrgrower.cpp` (8) — left as is (2026-10-03): dead code,
      nothing includes burrgrower.h. Kept to stay close to upstream; delete
      it (and its meson.build line) if upstream merges no longer matter.
    - [x] `src/lib/disassembler_*.cpp` — left as is (2026-10-03): node
      trees with their own caches/refcounts, no local new/delete pairs.
    - [x] `src/halfedge/polyhedron.cpp` (6) — left as is (2026-10-03):
      the container owns raw Face/HalfEdge/Vertex pointers by design;
      changing that is a library-wide refactor.
    - [x] `src/gui/voxelframe.cpp` (18) — done 2026-10-03: curAssembly,
      rotater, viewCube are unique_ptr; new dropMeshes() replaces 4 copies of
      list/poly freeing. Fixed a leak: the placement browser's placeOnly path
      overwrote shapes[0].shape without deleting it. shapeInfo::shape/poly
      stay raw (58 uses in drawing code). Note: pickPoly is never created
      anywhere — dead field.
    - [x] `src/gui/mainwindow.cpp` (43) — done 2026-10-03: groups editor,
      placement and movement browsers on the stack; the Disassembling...
      window a unique_ptr; Convert assemblies to pieces uses unique_ptr
      (7 deletes gone). assmThread is now a std::unique_ptr (5 delete/null pairs → reset()).
      Left: puzzle/ggt/disassemble members, cb_TransformPreview's owned preview.
  - Find them: `grep -n "delete " <file>`.
  - Risk: low per step. Watch for objects handed to code that frees them.

- [x] **4.2 Fixed-size char buffers** (~100 remain)
  Done 2026-10-03 (the ones that matter): every file-path buffer is now a
  std::string — Puzzlecad export (mainwindow.cpp, was 1000), STL export
  chooser and exportSTL (stlexport.cpp, 500/1000), image export pages
  (imageexport.cpp, 1000), STL-of-solution folder and file names
  (stlexportsolution.cpp, 500/1200). Paths could exceed those on macOS.
  Reviewed the rest: numeric formats with room to spare, or `static`
  buffers that must outlive Fl_Widget::label() (which does not copy) —
  left as they are; converting them buys nothing.
  - Find: `grep -rn "char [a-zA-Z_]*\[[0-9]*\]" src`.
  - Problem: snprintf truncates silently (a level text overflowed a stack
    buffer earlier in this branch), and %i/%u mistakes slip by.
  - Steps: where the text goes into an FLTK widget, build a `std::string`
    (use a small `format()` helper based on `snprintf` with size probe, or
    `std::format` if the compilers in the release builds support it —
    check the Linux and Windows cross builds first) and use `copy_label` /
    `value()`. Leave buffers that are fed to C APIs with a fixed size.
  - Verify: `just build` with `-Wformat=2`; `just check`.
  - Risk: low. Note: `Fl_Widget::label()` does not copy; use copy_label.

## Stage 5 — Split mainwindow.cpp (~8,000 lines)

- [x] **5.1 Split by area, no behaviour change**
  Done 2026-10-03 with a script (contiguous ranges moved unchanged; checked
  that every original line lands in exactly one file). Files and areas:
  - mainwindow.cpp (3,400): colours, shapes, problems, tab switching,
    status/info, activate*, updateInterface, update, handle.
  - mainwindow_rods.cpp: sliding start/goal states and stacking rod sets,
    rods, discs (applySlidingGridMode … syncStackingChrome).
  - mainwindow_solve.cpp: cb_BtnPrepare … cb_Status (solve, pause, the
    solution list, disassemblies).
  - mainwindow_file.cpp: confirmDiscard … cb_SaveAs, and
    cb_ImageExportVector … cb_Export_Scad (files, autosave, exports,
    .btsolve).
  - mainwindow_help.cpp: About, Tutorial, solver/sort help.
  - mainwindow_layout.cpp: Create*Tab, debug pane, constructor, destructor.
  - mainwindow_internal.h: prototypes of the ~100 callback stubs and
    helpers that are now shared (their `static` was dropped), fileExists.
  - rodbars.h: StackValidBar_c, RodTickBar_c, RodIndexBar_c (were local
    classes in mainwindow.cpp).
  The split differs from the plan below (by area of code, not by tab).
  Also dropped a now-redundant `assmThread = nullptr` (cppcheck).
  - Move groups of callbacks into new files, one commit-sized step each:
    - `mainwindow_file.cpp`: new/open/save/export/import, autosave,
      recovery, .btsolve.
    - `mainwindow_solver.cpp`: Solve/Pause/Continue/Stop, the update
      function, Activity texts, Time used, live sort.
    - `mainwindow_sliding.cpp` and `mainwindow_stacking.cpp`: the sliding
      and stacking/Panex tabs.
    - `mainwindow_edit.cpp`: entities, shape editing, colours.
  - Each is still a member of mainWindow_c; add the files to
    `src/gui/meson.build`. Do not rename or reorder functions while
    moving, so the diff is a pure move.
  - Verify: build, test, and click through every tab once.
  - Risk: low, but large diff. Do after Stage 3.5.

## Stage 6 — Small leftovers

- [x] **6.1 TODO/FIXME markers** — Done 2026-10-03: removed one stale
  marker (assembler_1.cpp "also add when piece ranges are used" — the
  condition already has `|| pieceRanges`). The other 13 are upstream
  design ideas (piece-choice heuristics in assembler_0/1/bt2, mirror-pair
  exchange in assembly.cpp, quad drawing in voxelframe.cpp, halfedge
  vertex reuse) or vendored FLTK code (Fl_Table, LFl_Tile); left as they
  are. Original text of this item: — `grep -rn "TODO\|FIXME" src` (14 in
  .cpp/.h). For each: fix it if small, or write an item here and delete
  the marker if it is obsolete.
- [x] **6.2 2D editor still shows S# labels**
  Done 2026-10-04 (user said yes): while stamping starts/goals on a
  start/goal tray, gridEditor_c::draw fills each marked cell in the
  piece's own colour (chequered light/dark like piece tiles, the same
  pieceColor the 3D view uses) instead of writing "S#". Marks for a shape
  number past the last shape are not drawn. slidingColors::LABEL_* is now
  unused (the 3D view's cellLabels path has no caller either). Not tried
  by hand. — the sliding start/goal
  preview now shows coloured voxels; check the 2D grid editor
  (`src/gui/` grid/editor widgets; search `"S%"`) and show the piece
  colour the same way, if the user still wants the label gone there.
  Ask the user first.

## Stage 7 — Modules not yet reviewed in depth

Review each like the first pass: easy fixes applied directly, bigger ones
added to this file as new items.

- [x] 7.1 `src/lib/assembler_*.cpp` (beyond 3.4)
  Reviewed 2026-10-03 with a one-off clang build adding -Wformat=2
  -Wimplicit-fallthrough -Wunreachable-code -Wloop-analysis
  -Wconditional-uninitialized -Wmove (cppcheck and clang-tidy are the
  only analysers set up; clang-tidy/scan-build are not installed here).
  assembler_1's fall-throughs are the intended state machine. Removed
  placementFinder_c::prob (unused; was the one warning in every build).
  getenv calls run once per solver, not in hot loops.
- [x] 7.2 `src/lib/disassembler_*.cpp`, `movementanalysator.cpp`
  Reviewed 2026-10-03: nothing flagged by the extra warnings; ownership is
  refcounted/node-set based on purpose (see 4.1).
- [x] 7.3 `src/lib/voxel*.cpp` (rotation tables, hot loops)
  Reviewed 2026-10-03: nothing flagged; no allocation in loops found.
  Also fixed under this pass: src/lib/ps3dloader.cpp (PuzzleSolver3D
  import) used sx/sy/sz uninitialised on a malformed size line, read past
  the end of short rows, and wrapped `s - 1` on a file with no pieces.
  Test "ps3d loader: …" in test_puzzle_model.cpp.
- [x] 7.4 XML load/save (`src/tools/xml.cpp`, problem/puzzle load and save)
  Done 2026-10-03: xmlParser_c::get(pos) built a string of the whole text
  buffer then took a substring — a full copy per tag/attribute name, and
  it cut at any NUL. Now constructs just the wanted range. Writer reviewed:
  fine. No unbounded sprintf/strcpy/strcat anywhere outside vendored Lua.
- [x] 7.5 `src/halfedge/`
  Reviewed 2026-10-03: nothing flagged; raw-pointer ownership is the
  library's design (see 4.1).
- [x] 7.6 `src/burrTxt.cpp`, `burrTxt2.cpp`, `lib_interface.cpp`
  Reviewed 2026-10-03: no raw new/delete or unbounded string calls.
  lib_interface.cpp: `getUsedTime` asserts on an unsolved problem (it
  calls problem_c::getUsedTime), and `solve()` has an unused `filenumber`;
  harmless, left to stay close to upstream.
- [x] 7.7 `src/lua/`, `src/python/` bindings
  Reviewed 2026-10-03: src/lua is vendored (AGENTS.md: never edit).
  src/python uses pybind11 with shared_ptr ownership and releases the GIL
  while solving; nothing to fix.

---

## Round 2 — fresh rescan (started 2026-10-04)

Order: highest bug risk first. Each area is ticked when read end to end;
fixes are listed under it. Items needing a decision are marked NEEDS THE
USER. Verify after each area: `just build`, `just test`, `just check`
(5 known findings), `just test-regression` (18/18).

- [x] R2.1 src/lib/solvethread.{h,cpp} — threads, pause/stop, autosave
  Done 2026-10-04:
  - Use-after-free races: the published assembler was freed while still
    published (createMatrix error path frees new_assm; the assert catch
    called removeAllSolutions first; at the end of run the GUI may remove
    the problem's assembler). Now every GUI-side use goes through
    withAssembler() under assmMutex, and publishAssembler(nullptr) runs
    before any free (explicitly on early returns, a scope guard otherwise).
    assemblerFinished() returns the last value after withdrawal.
  - Data races: statsPhase, phaseOrigin, assemblerThreadCount were plain
    fields written by the worker and read by getStats(); now atomic, with
    beginPhase()/endPhase() replacing six copies of the timing code.
  - Hang: start() looped for ever with solution limit 0 (FLTK value inputs
    accept typed values outside their bounds); a drop of 0 divided by zero
    in trimSavedSolutions. setSolutionLimits sanitises drop; the loop skips
    limit 0. Test "Solver: no solution limit keeps every solution".
  - Count-only (Just count) runs saved single-piece assemblies anyway.
  - processDisassembly: 8 copies of the add-solution code → add()/trim().
  - Removed unused getTime()/startTime; `= delete` copying; fixed a
    misplaced doc comment.
- [x] R2.2 src/lib/sliding.cpp
  Done 2026-10-04:
  - Speed: tableSearch's visited map is now parentMap_c (open addressing,
    two flat arrays) instead of std::unordered_map: the new benchmark
    (hidden test "sliding: benchmark, fifteen pieces in a 4x4 tray", run
    `/usr/bin/time -l ./build/test_burrtools "[.bench]"`) went 2.8 s → 1.3 s,
    peak 315 → 293 MB for 5.7 M arrangements. TABLE_STATE_BYTES 64 → 56
    (measured 51 at peak, including the moment the table doubles).
  - Nested slides: the current arrangement is decoded once, not per piece.
  - The goal-map API (ensureSetup, placeStart/Goal, hasStart/Goal,
    clearStart/Goal, clearStartAt, toggleGoal) is no longer used by the
    editor (it uses start/goal shapes) but the tests build goal-map puzzles
    with it, and old files may hold such goals; kept, grouped and labelled.
    Note placeGoal refuses a goal on a cell that holds any start colour.
- [x] R2.3 src/lib/stacking.cpp
  Done 2026-10-04:
  - boardSpan took rodX.back() as the widest rod, but with a pocket column
    the last rod is the pocket, drawn at x 0: the solution animation of a
    Panex board with a pocket got a span one rod wide. Now the widest rod.
  - loadRodSets read counts with atoi into unsigned fields: a negative or
    junk value became ~4 billion. countAttribute() keeps values in range
    (0..1000, flags 0/1) and falls back to the default otherwise.
  - The size-rule message said "rod N" even for the pocket; uses rodName.
  - Tests: "the board span covers every rod, the pocket too", "a rod set
    with impossible numbers in the file loads sanely".
- [x] R2.4 src/lib/panex.cpp
  Done 2026-10-04: read end to end — bidirectional meeting (level-by-level
  check is exact), mirror/twin canonical keys, disk levels and trace-back,
  save/resume and cleanup all hold up. Only two misplaced comments fixed.
  Known limit, not changed: autosave is checked once per level, so a
  single level that takes longer than 20 minutes is not saved part way.
- [x] R2.5 src/lib/problem.{h,cpp} (solutions, pending, save/load)
  Done 2026-10-04:
  - setAssembler: on a version/syntax restore error it set the state to
    unsolved but kept assemblerState, so the next solve asserted
    (bt_assert(solveState == SS_SOLVING)). Every restore error now drops
    the unusable state and partial results (the message already says
    "start from the beginning"). Test "Solver: a damaged saved position
    errors once, then solves from the start".
  - Loading: the `state` attribute went unchecked into the enum; now only
    known states are taken.
- [x] R2.6 src/gui/mainwindow_file.cpp (save, autosave, .btsolve)
  Done 2026-10-04 (GUI not tried by hand):
  - Save and Save As wrote straight over the puzzle file: a failed write
    (disk full, crash) destroyed it. New writePuzzleFile(): writes
    <file>.tmp, closes it (gzip's final flush can fail too), then renames
    over the target. Save, Save As, autosave and the .btsolve export's
    temporary copy all use it. Save As uses hasFileExtension (case-blind).
  - Importing a PuzzleSolver3D (.puz) or Puzzlecad (.scad) file set the
    window's file name to that source, so File > Save wrote gzipped
    BurrTools XML over the original. Imports are now untitled and marked
    changed: Save asks where.
  - .btsolve import wrote "search/<name>" sections into the search folder
    with only ".." refused: an absolute or nested name could write
    elsewhere. Only plain file names (isPlainFileName) are taken now.
- [x] R2.7 src/gui/mainwindow_solve.cpp, mainwindow.cpp update()/updateInterface()
  Done 2026-10-04 (GUI not tried by hand):
  - Use-after-free while a solve runs: activateSolution handed the shown
    solution's take-apart to disasmToMoves_c and the views, which kept it
    after the lock; the solver can drop that solution (trim to the limit,
    a shorter slide path) and the Move slider then read freed memory.
    problem_c::solutions is now vector<shared_ptr<solution_c>>, with
    shareSavedSolution(); mainWindow_c::shownSolution holds the one shown.
    `disassemble` is a unique_ptr.
  - Delete solutions / Delete or Add disassembly were active during a solve
    of the same problem and raced with it (Delete All counted, then removed
    one by one while the solver trimmed; Add Disassembly held a solution
    pointer for seconds). Greyed out while solving that problem, and the
    callbacks check solvingProblem() too. Sorting stays (live sort).
  - Start with no problem selected dereferenced it; Stop asserted when the
    solve had just ended. Both now guarded.
  - updateInterface (~900 lines) read for races only; splitting it per tab
    is left for R2.13.
- [x] R2.8 src/gui/mainwindow_rods.cpp, rodbars.h
  Done 2026-10-04 (GUI not tried by hand): the rod-set rule fields stayed
  editable during a solve, and an edit calls removeAllSolutions() on every
  problem using that rod set — including one being solved (its state
  freed under the solver). The fields are now greyed out while solving,
  and cb_RodField puts them back if it is reached anyway. Removed a
  duplicate relayoutTab declaration.
- [x] R2.9 src/lib/rotationrules.cpp, rotationmoves_*.cpp
  Done 2026-10-04 — speed of Check Rotations (same answers):
  - Profiled `burrTxt -R examples/PelikanBurr.xmpuzzle` (runs > 10 min):
    the time went to building a std::set of every occupied cell for every
    rotation tried, and to malloc/free.
  - rotationrules.cpp: occupancy_c (flat grid over the bounding box + the
    cell list) replaces the sets on the hot path; rotationRules_c keeps the
    last one (occCache), since a disassembler passes the same occupied
    cells for every pivot, axis and sense. Arc sweep: static squares built
    once per call and grouped by layer, the SAT normals' cos/sin once per
    step (same calls, same values), far squares skipped by a conservative
    distance test. perpPlaneClear: per-slice counts in one pass and two
    grid lookups per moving cell instead of rescanning every static cell.
  - rotationmoves_0.cpp: the subset's cells and the others' are gathered
    once per node and subset (rebuildPivotCells), not per pivot × axis ×
    sense (the Crowell variant already cached them).
  - Guard: test "rotation rules: the same answers as before (hash)" pins
    every allowRotation/axisBlocked answer on 2,400 random rotations;
    hidden "[.bench][rotation]" timed 20,000 cases: 3.89 s → 0.56 s.
- [x] R2.10 src/lib/scadloader.cpp, stlexportsolution.cpp, shapehistory.cpp
  Done 2026-10-04:
  - Undo (shapehistory) in a stacking puzzle emptied the rods: restore()
    drops every part to 0, which takes those discs off the start/goal
    stacks (trimStacks), and the stacks were not in the snapshot. They are
    now. Test "stacking: undo of a disc edit keeps the stacks" (fails
    without the fix). shapehistory.cpp is now linked into test_burrtools
    (it uses no FLTK). Snapshots and their shapes are unique_ptr.
  - Puzzlecad import: skipValue recursed once per nested '[', so a crafted
    file could overflow the stack; capped at 200 levels. The loader now
    returns unique_ptr, like loadPuzzlerSolver3D. Test "scad loader: …".
  - Export solution to STL: file names come from piece names through
    safeName(), so two pieces with the same (or same-looking) name wrote
    the same file, the second over the first. A clash now gets "-S<n>".
- [x] R2.11 src/burrTxt.cpp
  Done 2026-10-04:
  - `-o N` past the last problem crashed in getProblem; now an error.
  - `-a s0/s1` looped to getNumSolutions() but indexed the saved list,
    which a solution limit keeps shorter: read past the end. Uses
    getNumberOfSavedSolutions().
  - The Stacking Solver always used findStackPath; it now runs the Panex
    search (anyRules) whenever that can hold the puzzle, as the GUI does,
    and prints "stack search: …" stats.
- [x] R2.12 remaining GUI: voxelframe, viewcube, tooltabs, debugstatspanel
  Done 2026-10-04 (targeted: indexing, divisions, GL resources):
  - tooltabs.cpp resizeSpace: with "apply to all" on, shrinking did
    nothing (the loop only grows, and the selected shape's resize sat in an
    `else`, against its own comment — the same upstream). Now always.
  - viewcube/debugstatspanel divisions are all guarded.
  - Known, not changed: voxelframe frees display lists from wherever a view
    changes, which may be outside the GL context (upstream behaviour);
    setDrawingMode does not bounds-check `nr` like its siblings.
- [x] R2.13 consistency pass over fork-owned files (naming, comments, idioms)
  Done 2026-10-04:
  - The standard GPL header added to the 14 files this fork created that
    lacked it (panex, sliding, stacking, sysmemory, rodbars,
    mainwindow_internal, assembler_resume, debugstatspanel, slidingcolors);
    files that also exist upstream without it were left alone.
  - 31 "no copying" declarations (private, never defined) are `= delete`:
    misuse is now a compile error, not a link error. One was wrong:
    assembler_bt2_c is copied once per worker; its comment now says so.
    That surfaced an unused field (rotationMoves_0_c::problem), removed.
  - mainWindow_c::updateInterface (896 lines) split: updateEntitiesTab()
    and updateSolverTab() hold the two big tab branches verbatim;
    updateInterface is 316 lines.
  - Left on purpose: `0` vs nullptr, naming and raw-pointer ownership in
    code shared with upstream (churn there makes upstream merges harder
    for no behaviour gain).

## Round 3 — solver speed (2026-10-04)

Every solver's search loop was read for missing algorithmic ideas. What was
done, with numbers from this machine (10 cores). Each item has a switch so
old and new run from one build. Answers are unchanged unless said.

Benchmarks: the examples all solve in under 0.1 s and show nothing. Use
`tictest/performance/puzzles` with `burrTxt -dR --json --solver classic` for
rotations, and the hidden tests:
`./build/test_burrtools "[.bench][klotski]"`, `"[.bench][pentomino]"`,
`"[.bench][panex]"` (BT_PANEX_BENCH_N=7 for the big one), `"[.bench][rotation]"`.

- [x] R3.1 Classic rotation generator (rotationmoves_0.cpp, rotationrules.cpp)
  143 corpus puzzles: 96.3 s → 15.6 s, the same JSON for every one.
  - Each piece's cells once per node, the subset's once per subset (were
    rebuilt per subset and axis).
  - rotationRules_c::setBodies / preparedAxisBlocked / allowPrepared: the two
    tests that do not depend on the pivot run once per (subset, axis) and a
    blocked axis skips its pivots; end cells stop at the first overlap;
    pivots off the grid (in-plane coordinates of different parity) are not
    listed. Test "the prepared path agrees with allowRotation".
  - Arc sweep: cosines and sines per step from a table; per moving cell only
    the static squares in its ring about the pivot; corners worked out only
    when a square is near; each wiggle first looks where the last one was
    stopped. The pinned hash test is unchanged.
  - Switch: BURRTOOLS_NO_ROT_FAST=1.
- [x] R3.2 A bug in the rotation rules, fixed (the user confirmed: HoleyTIC
  is level 17, which only the fixed rules find)
  The arc sweep's overlap test takes the moving square's edge normals at the
  angle turned so far. In the plane's own (u, v) a turn about Y runs the
  other way round from one about X or Z, but had the same sign, so for Y
  the normals were the mirror image of the square's edges (the Fortran takes
  them at the turned angle for every axis). It only erred towards refusing.
  - 29 of the 143 corpus puzzles changed: 19 that Classic called unsolvable
    get a take-apart (TriumviraTIC, XITIC, EclipTIC, HolisTIC, ...), most of
    the rest a shorter one (HoleyTIC level 23 → 17). The pinned hash in
    test_rotation_solvers.cpp was updated. tictest/performance/results_*.csv
    still hold the old answers and times: rerun run_performance.py (the
    whole Classic corpus now takes a few minutes, CornerCube 13 s).
  - The rules now give the same answer however the scene is turned (test
    "a turned scene gets the same answers"). So rotation moves keep the
    first piece's orientation fixed (rotationMoves_0_c::anchor) and an
    arrangement is one node, not up to 24: corpus 15.6 s → 7.7 s.
    BURRTOOLS_NO_ROT_ANCHOR=1 turns that off.
  - test/performance/brick_rotations: 12.7 s at the start of the round, 0.5 s.
- [x] R3.3 Sliding: like pieces searched as one (sliding.cpp tableSearch)
  Copies of a piece (same cells, layer and goal) are kept in sorted order,
  so arrangements that differ only by which copy sits where are one state;
  the path is rebuilt with every piece keeping its identity. Klotski: 81
  moves (the published answer), 9,950,042 → 23,691 arrangements, 4.7 s →
  0.007 s. Also: the search stopped draining its queue after finding the
  goal. Switch: BURRTOOLS_NO_SLIDE_SYMMETRY=1.
- [x] R3.4 BurrTools 2 assembler (bt2_dancingcells.cpp, bt2_assemble.cpp)
  - Bug: with more than one thread the split handed a copy the whole rest of
    the search, so assemblies were found several times over (Dracula 128
    for 84, Bermuda 7 for 1) and the threads redid each other's work. Fixed;
    test "Pentominoes: every solver type finds the published counts".
  - Bug: when the first step of the search was forced, no copy was ever
    made and the run stayed on one thread. Idle places now steal later.
  - SET holds node numbers, so hiding an option from an item is O(1) (was
    a search of the option, twice); no allocation per step. The split gives
    away the shallowest open branch, not the deepest.
  - 6x10 pentominoes (2339): 6.7 s → 2.2 s on one thread, 2.2 s → 0.49 s
    on eight (Classic: 1.08 s on one).
- [x] R3.5 Panex (panex.cpp)
  mergeParts cuts the keys into one stretch per thread and merges each on
  its own thread (the last pairwise merges ran on one); the newest level is
  written to disk on its own thread while the next is searched. Seven-disc
  swap (260 M stackings): 12.0 s → 10.5 s on all threads, 43.9 s → 41.0 s
  on one.
- [x] R3.6 Take-apart without rotations (movementanalysator.cpp)
  test/performance/brick_normal (12 pieces, 1536 assemblies): 10.3 s → 8.1 s,
  the same answers on the corpus and the examples.
  - prepare() asks the movement cache only about pairs of pieces that lie
    differently from the last node prepared (was: only when that node was
    the parent, which a breadth-first search seldom hands over).
  - closeFrom(): one Floyd–Warshall pass per direction on a dense copy, in
    place of sweeps until nothing changes.
  - checkmovementMasks(): which pieces a piece takes along, as bit masks
    kept per node, direction and step (up to 64 pieces).
  - Switch: BURRTOOLS_NO_DISASM_FAST=1. What is left is the number of nodes
    (about a million here). burrTxt takes assemblies apart one at a time;
    the GUI already spreads them over its workers.
- [x] R3.7 Stacking: the classic tower needs no search (panex.cpp classicTower)
  Three plain rods, size rule, all sizes different, the whole tower from one
  rod to another: the well-known recursion is a shortest path. 16 discs
  (test/performance/stacking_performance): 3.6 s → 0.1 s, and towers too
  tall to search (up to 22 discs) now solve. Switch: BURRTOOLS_NO_TOWER_RULE=1.
- [x] R3.8 Panex, second pass: batches sorted by radix (11 bits a pass)
  instead of comparisons, the mirrored level too; subtract looks for each
  key from where the last was found. Seven-disc swap: 10.5 s → 9.1 s on all
  threads, 41.0 s → 33.2 s on one (12.0 s and 43.9 s before the round).
- [x] R3.9 Sliding, second pass (sliding.cpp)
  - Nested slides: nesting is a mask test against the outer piece's outline
    at its position, and the group's shifts are flooded on a grid; no sets,
    no allocation. test/performance/sliding_nested: 0.67 s → 0.11 s.
  - tableSearch is a template on the key: 64 bits, or 128 when the tray
    needs more, where it used to fall back to legacySearch (text keys).
- Decided against, with reasons:
  - Sliding from both ends: needs a goal that places every piece; with a
    goal for some pieces only (the usual case, and both performance files)
    the goal is a set of arrangements too large to start from.
  - Sliding parent kept as a move: 12 bytes a slot for 16, a quarter more
    arrangements in the same memory; not worth the path-rebuilding code.
  - Panex levels held compressed in memory (sorted keys as differences):
    the one thing that would let an eight- or nine-disc swap fit, but
    expansion, merge and subtract would all have to stream compressed
    blocks. A project of its own.
  - Checkerboard parity pre-check for assemblies: it can only prove that a
    puzzle has no assembly, changes nothing for one that has, and a wrong
    "impossible" would be worse than a slow search.

## Round 4 — threads and progress (2026-10-04)

How every solver splits its work over threads was read, and how a solve
reports progress. Numbers from this machine (4 fast + 6 slow cores), old
and new binaries run one after the other. Answers are unchanged: the 161
corpus puzzles give the same JSON with Classic and with Crowell.

- [x] R4.1 A take-apart spreads each level of its search over the free cores
  (disassembler_a_c::searchLevels, helperpool.cpp). Classic and Crowell now
  share that one search; it was two copies.
  - The positions of a level are searched on several threads, each with its
    own movementAnalysator_c. What they find is put into the new front by
    one thread in the order of the level, so the same disassembly comes out
    as on one thread (test "a take-apart spread over threads gives the same
    disassembly as on one").
  - A level is only spread when what is left of it is reckoned to take
    longer than 400 microseconds; short searches stay on one thread.
  - Threads are lent only while cores are free: the assembly search and
    every take-apart under way count as load.
  - `burrTxt -dR --json --solver classic`: Climburr_OldVer 1.46 s -> 0.34 s,
    Climburr 2.24 -> 0.49, CoverUp3 5.40 -> 1.46, CornerCube 13.3 -> 5.6;
    corpus 36.3 s -> 15.7 s. Crowell: CornerCube 3.64 -> 1.70, Climburr
    0.58 -> 0.14.
  - Switch: BURRTOOLS_NO_DISASM_PAR=1.
- [x] R4.2 Rotation puzzles get the usual number of take-apart workers
  (solvethread.cpp chooseDisasmWorkerCount); it was fixed at one. With R4.1,
  GUI path (`burrTxt2 -R -d --rotations`): Climburr_OldVer 1.48 s -> 0.31 s,
  CoverUp3 5.40 -> 1.11, CornerCube 13.4 -> 2.84.
  Switch: BURRTOOLS_DISASM_WORKERS=n.
- [x] R4.3 The queue of assemblies waiting to be taken apart is bounded (64,
  or 8 for each worker): the thread that found one waits when it is full.
- [x] R4.4 BurrTools 2 assembly driver (bt2_assemble.cpp): threads that stay
  for the whole run; a busy one splits its own search between two slices when
  another waits. It was new threads every 8000 steps with the splitting done
  between rounds. A split copies the search state only, not the matrix.
  - Bug: with more than one thread Stop and Pause did nothing (each slice
    cleared the flag). Fixed; the branches not finished are kept and
    Continue goes on with them (test "BurrTools 2: a search split over
    threads stops and continues").
  - Bug: the progress list could hold a freed search; the count of
    iterations was about double; the shape caches were not filled before the
    threads started. Fixed.
  - 6x10 pentominoes, back to back: 8 threads 0.79 s -> 0.64 s; 4 threads
    0.87 s -> 0.93 s (slower: to look at).
- [x] R4.5 One picture of a solve's progress: solveProgress_c from
  solveThread_c::getProgressSnapshot(), used by the Solver tab, the Debug
  pane, `burrTxt2 --progress` and (its own lines) `burrTxt --progress`.
  - A take-apart says the level it is on, how many of the level's positions
    are done, how many separations it has of the ones it needs, and how many
    threads work on it (disassemblyProgress_c).
  - The bar no longer creeps by the clock. While only take-aparts of unknown
    length are left it shows the level at hand.
  - ACT_DISASSEMBLING is now set while the queue drains.
  - Classic assembly: tasks have weights and each thread reports how far it
    is inside its task; it was tasks done out of tasks.
  - assembler_1: a continued search split over threads showed 0% all
    through; a SIMD search ended as "paused". Fixed.
  - Stop reaches into the move search (rotation subsets too); a stopped
    sub-search is no longer taken for a group of pieces that stays together.
  - problem_c solveState and usedMs are atomic; the assembler's progress is
    not read while it is being prepared (found by `just build-tsan`).
- Looked at, not done: finer or dynamic tasks for assembler_0. 64 or 256
  tasks for each thread, 6 or 8 levels deep, made the pentominoes slower,
  not faster; on one thread Classic runs its SIMD search, on several the
  plain one, and that, not the split, is what holds it back there.
  Also not done: the "Add disassembly" dialogs still show nothing while one
  runs; saving a BurrTools 2 search that is split (it starts again).

## Open items found while working

- [ ] **8.1 One unexplained test-regression failure** — on 2026-10-03,
  one run reported 17/18 right after `just check`; 15 later runs (one
  under load) were all 18/18 and the failing puzzle was not captured. If
  it happens again, rerun with
  `python3 test/test_examples_regression.py 2>&1 | grep -A3 FAILED` and
  note the puzzle here: a different solution count would mean a
  nondeterministic solver, a timeout would mean load.

## Known cppcheck findings (intentional)

`just check` reports 5: `mainWindow_c::show` hides the base class `show`
(on purpose, it wraps it), and the `lastSolveStats` initialisation ones.
Anything beyond these 5 is new.

## Done in this pass (2026-10-03)

- [x] `mainWindow_c::fname` is now `std::string`, set through
  `setFileName()`, with `autosavePathFor()`. Removed manual
  new[]/strcpy/delete[]. This fixed a crash: image and STL export were
  given a null file name when the puzzle had never been saved.
- [x] Format strings: `%i` → `%u` for unsigned values in the piece and
  problem statistics, "solved %u of %u disassemblies", "prepare/optimize
  piece %u".
- [x] C-style casts → `static_cast` in mainwindow.cpp and BlockList.cpp.
- [x] Default member initialisers in scadloader.cpp, solvethread.h
  (disasmTask_c), sliding.cpp (Place), tooltabs.h, voxelframe.h,
  shapehistory.h.
- [x] sliding.cpp `findSlidePath`: pieces owned by
  `std::vector<std::unique_ptr<voxel_c>>` instead of manual deletes.
- [x] solvethread.cpp `stopInternal()` / `stopSoft()`: read the published
  atomic assembler pointer once, closing a race with the solver thread
  replacing it.
- [x] rotationrules.cpp: `BT_ROT_DEBUG` and `BT_ROT_DUMP` are read once
  instead of `getenv` on every rotation checked (hot path).
- [x] justfile: `test-all` recipe (alias of `test`) as the docs name it.
