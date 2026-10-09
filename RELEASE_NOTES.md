# Release Notes

## Panex Solver Enhancement: Outside Channel Support

**Commit:** a517c6ff  
**Date:** October 7, 2026  
**Author:** Colin Deisenroth

### Overview
Extended Panex puzzle solver to support an outside channel connecting non-adjacent rods. This enables solving puzzles like "Blind Panic" where pieces can move between the first and last rods via a channel that bypasses intermediate columns.

### Key Features

#### New Outside Channel Mechanism
- **Outside Channel Support:** For Panex puzzles with 3+ rods, an optional channel now connects the first and last rods outside the bridge structure
- **Bypass Blocking:** Pieces moving between end rods through the channel are no longer blocked by raised discs on intermediate rods
- **Puzzle Types:** Enables puzzles like "Blind Panic" where end towers can exchange positions directly

#### Technical Improvements
- Added `channelA` and `channelB` fields to track which rods are connected by the outside channel
- Implemented blocking detection that correctly handles channel moves (skips intermediate rod checks when channel is active)
- Backward Compatible: Searches saved before this feature continue to work with the same folder structure
- Fingerprint tracking ensures puzzle identification remains consistent across solver sessions

### Changes Made

**Core Solver Changes**
- `src/lib/panex.cpp`: Added channel detection logic and move validation
- `src/lib/stacking.cpp`: New functions for outside channel detection and validation
- `src/lib/stacking.h`: New data structure fields and API functions

**GUI Updates**
- `src/gui/mainwindow.h`: Added UI state tracking
- `src/gui/mainwindow_layout.cpp`: Layout support for new channel controls
- `src/gui/mainwindow_rods.cpp`: Rod configuration for outside channels
- `src/gui/mainwindow_help.cpp`: Updated help text
- `src/gui/tutorials.cpp`: Updated tutorial references

**Testing**
- `test/test_stacking.cpp`: New comprehensive test cases for outside channel functionality

### API Additions

```cpp
// Check if puzzle board uses an outside channel
bool hasOutsideChannel(const rodSet_c & board);

// Check if a move between two rods uses the outside channel
bool viaOutsideChannel(const rodSet_c & board, unsigned int from, unsigned int to);
```

### Backward Compatibility
✅ Fully backward compatible with existing puzzle files and saved searches. Previous searches maintain their folder structure and continue to function correctly.

### Testing
New regression tests added to verify:
- Outside channel moves are not blocked by intermediate rods
- Channel logic only applies when explicitly enabled
- Fingerprinting correctly identifies puzzles with outside channels

### Files Modified
- 9 files changed
- 135 insertions, 14 deletions

### Known Limitations
- Outside channel is only applicable to Panex column puzzles with 3+ rods
- Channel connects the outermost rods only (first and last)
