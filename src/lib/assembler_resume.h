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
 * The part of a saved assembler position that assembler_0_c and
 * assembler_1_c share: what a stopped parallel or SIMD search needs to carry
 * on. Each assembler writes its own state first and then this tail, which
 * after the leading flag (see their setPosition) is
 *
 *   flag 3:  " K <solutions already reported>"
 *   flag 2, 4:  " T <tasks>" <each task, as the assembler writes it>
 *            " C <one 0 or 1 per task, finished or not>"
 *            " S <count> <signature>..."
 *
 * The text must stay byte for byte what older versions wrote, so saved
 * puzzles keep loading.
 */
#ifndef __ASSEMBLER_RESUME_H__
#define __ASSEMBLER_RESUME_H__

#include <cstdint>
#include <cstdlib>
#include <ostream>
#include <unordered_set>
#include <vector>

namespace assemblerResume {

/* Numbers and markers read one after the other from a saved position. */
class reader_c {
public:
  explicit reader_c(const char * at) : s(at) {}

  bool next(unsigned long long & v) {
    char * end = nullptr;
    v = std::strtoull(s, &end, 10);
    if (end == s)
      return false;
    s = end;
    return true;
  }

  bool next(unsigned int & v) {
    unsigned long long l = 0;
    if (!next(l))
      return false;
    v = (unsigned int)l;
    return true;
  }

  /* c, after any spaces */
  bool expect(char c) {
    skipSpaces();
    if (*s != c)
      return false;
    s++;
    return true;
  }

  void skipSpaces(void) {
    while (*s == ' ')
      s++;
  }

  const char * s;
};

/* What a stopped search left to resume. */
template <class Task>
struct tail_c {
  uint64_t simdSkip = 0;
  std::vector<Task> tasks;
  std::vector<uint8_t> completed;
  std::unordered_set<uint64_t> signatures;
};

/**
 * Read the tail for flag 2, 3 or 4 from at (4 is assembler_1_c's, laid out as 2). readTask(reader, task) reads one
 * task as writeTail's writeTask wrote it. False on a syntax error. done is
 * set to the number of tasks already finished.
 */
template <class Task, class ReadTask>
bool readTail(const char * at, unsigned int flag, tail_c<Task> & tail, size_t & done, ReadTask readTask) {
  reader_c r(at);
  done = 0;
  unsigned long long n = 0;
  if (flag == 3) {
    if (!r.expect('K') || !r.next(n))
      return false;
    tail.simdSkip = n;
    return true;
  }
  if (!r.expect('T') || !r.next(n))
    return false;
  tail.tasks.resize((size_t)n);
  for (Task & t : tail.tasks)
    if (!readTask(r, t))
      return false;
  if (!r.expect('C'))
    return false;
  r.skipSpaces();
  tail.completed.assign(tail.tasks.size(), 0);
  for (size_t i = 0; i < tail.tasks.size(); i++, r.s++) {
    if (*r.s != '0' && *r.s != '1')
      return false;
    tail.completed[i] = *r.s == '1';
    done += tail.completed[i];
  }
  unsigned long long m = 0;
  if (!r.expect('S') || !r.next(m))
    return false;
  for (unsigned long long i = 0; i < m; i++) {
    unsigned long long sig = 0;
    if (!r.next(sig))
      return false;
    tail.signatures.insert(sig);
  }
  return true;
}

/** Write the tail for flag 2, 3 or 4; nothing for any other flag. */
template <class Task, class WriteTask>
void writeTail(std::ostream & str, unsigned int flag, uint64_t simdReported,
               const std::vector<Task> & tasks, const std::vector<uint8_t> & completed,
               const std::unordered_set<uint64_t> & signatures, WriteTask writeTask) {
  if (flag == 3) {
    str << " K " << simdReported;
  } else if (flag == 2 || flag == 4) {
    str << " T " << tasks.size();
    for (const Task & t : tasks)
      writeTask(str, t);
    str << " C ";
    for (uint8_t c : completed)
      str << (c ? '1' : '0');
    str << " S " << signatures.size();
    for (uint64_t sig : signatures)
      str << " " << sig;
  }
}

} // namespace assemblerResume

#endif
