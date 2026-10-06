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
 * The machine: its physical memory, and where to keep large files. Kept
 * apart so the system headers they need reach no other source file.
 */

#include "sysmemory.h"

#include <cstdlib>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#include <sys/sysctl.h>
#include <cstdint>
#include <vector>
#elif defined(__unix__)
#include <unistd.h>
#include <vector>
#endif

unsigned long long physicalMemoryBytes(void) {
#if defined(_WIN32)
  MEMORYSTATUSEX status;
  status.dwLength = sizeof(status);
  if (GlobalMemoryStatusEx(&status))
    return status.ullTotalPhys;
  return 0;
#elif defined(__APPLE__)
  uint64_t bytes = 0;
  size_t len = sizeof(bytes);
  if (sysctlbyname("hw.memsize", &bytes, &len, nullptr, 0) == 0)
    return bytes;
  return 0;
#elif defined(__unix__) && defined(_SC_PHYS_PAGES) && defined(_SC_PAGE_SIZE)
  long pages = sysconf(_SC_PHYS_PAGES);
  long pageSize = sysconf(_SC_PAGE_SIZE);
  if (pages > 0 && pageSize > 0)
    return (unsigned long long)pages * (unsigned long long)pageSize;
  return 0;
#else
  return 0;
#endif
}

std::string userCacheDirectory(void) {
#if defined(_WIN32)
  if (const char * local = std::getenv("LOCALAPPDATA"))
    return std::string(local) + "\\BurrTools";
  return "";
#elif defined(__APPLE__)
  if (const char * home = std::getenv("HOME"))
    return std::string(home) + "/Library/Caches/BurrTools";
  return "";
#else
  if (const char * xdg = std::getenv("XDG_CACHE_HOME"))
    if (*xdg)
      return std::string(xdg) + "/burrtools";
  if (const char * home = std::getenv("HOME"))
    return std::string(home) + "/.cache/burrtools";
  return "";
#endif
}

std::string executableDirectory(void) {
  std::string path;
#if defined(_WIN32)
  wchar_t buf[MAX_PATH * 4];
  const DWORD n = GetModuleFileNameW(nullptr, buf, (DWORD)(sizeof(buf) / sizeof(buf[0])));
  if (n == 0 || n >= sizeof(buf) / sizeof(buf[0]))
    return "";
  const int len = WideCharToMultiByte(CP_UTF8, 0, buf, (int)n, nullptr, 0, nullptr, nullptr);
  if (len <= 0)
    return "";
  path.resize(len);
  WideCharToMultiByte(CP_UTF8, 0, buf, (int)n, &path[0], len, nullptr, nullptr);
  const size_t slash = path.find_last_of("\\/");
#elif defined(__APPLE__)
  uint32_t size = 0;
  _NSGetExecutablePath(nullptr, &size);
  std::vector<char> buf(size + 1, 0);
  if (_NSGetExecutablePath(buf.data(), &size) != 0)
    return "";
  path = buf.data();
  const size_t slash = path.rfind('/');
#elif defined(__unix__)
  std::vector<char> buf(4096, 0);
  const ssize_t n = readlink("/proc/self/exe", buf.data(), buf.size() - 1);
  if (n <= 0)
    return "";
  path.assign(buf.data(), (size_t)n);
  const size_t slash = path.rfind('/');
#else
  const size_t slash = std::string::npos;
#endif
  if (slash == std::string::npos)
    return "";
  return path.substr(0, slash);
}
