/* BurrTools
 *
 * The machine: its physical memory, and where to keep large files.
 */

#ifndef __SYSMEMORY_H__
#define __SYSMEMORY_H__

#include <string>

/** Physical memory installed in the machine, in bytes; 0 when it cannot be read. */
unsigned long long physicalMemoryBytes(void);

/**
 * Where BurrTools may keep large files between runs: ~/Library/Caches/BurrTools
 * on macOS, %LOCALAPPDATA%\BurrTools on Windows, and $XDG_CACHE_HOME/burrtools
 * or ~/.cache/burrtools elsewhere. Empty when none of those can be found.
 */
std::string userCacheDirectory(void);

#endif
