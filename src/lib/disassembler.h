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
#ifndef __DISASSEMBLER_H__
#define __DISASSEMBLER_H__

#include <memory>
#include "disassembly.h"

class assembly_c;

class helperPool_c;

/**
 * What a take-apart search under way is doing, for showing progress.
 *
 * The search goes level by level: every position one move further from the
 * start than the last level. How many levels there will be is not known
 * before the end, but how far the level at hand is done is, and so is how
 * many of the separations needed to get every piece out have been found.
 */
struct disassemblyProgress_c {
  /** a take-apart is running; nothing else is set when it is not */
  bool active = false;
  /** milliseconds since this take-apart began */
  unsigned long long elapsedMs = 0;
  /** pieces of the assembly, and of the part being searched now */
  unsigned int pieces = 0;
  unsigned int searchPieces = 0;
  /** separations found so far; every piece is out after pieces - 1 */
  unsigned int separations = 0;
  /** how many parts deep the search is: 1 for the whole assembly */
  unsigned int depth = 0;
  /** moves from the start of the part being searched */
  unsigned int level = 0;
  /** positions of this level looked at, and how many it has */
  unsigned long levelDone = 0;
  unsigned long levelSize = 0;
  /** positions found for the next level so far */
  unsigned long nextLevelSize = 0;
  /** positions looked at since this take-apart began */
  unsigned long long nodes = 0;
  /** threads that worked on the level at hand */
  unsigned int threads = 1;
};

/**
 * Base class for a disassembler.
 *
 * The interface is simple:
 * -# construct the class with whatever parameters the concrete subclass requires
 * -# call diassemble for each assembly found and evaluate the result
 *
 * some subclasses may be able to handle several assemblies, others may only
 * disassemble one, that depends on the concrete disassembler you use
 */
class disassembler_c {

public:

  disassembler_c(void) {}

  virtual ~disassembler_c(void) {}

  /**
   * Try to disassemble an assembly.
   *
   * Because we can only have or don't have a disassembly sequence
   * we don't need the same complicated call-back interface. The function
   * returns either the disassembly sequence or a null pointer.
   */
  virtual std::unique_ptr<separation_c> disassemble(const assembly_c * /*assembly*/) { return nullptr; }

  /** request abort of an in-progress disassemble call */
  virtual void stop(void) {}

  /** microseconds spent in 90° rotation move search; default 0 */
  virtual unsigned long long getRotationSearchUs(void) const { return 0; }

  /** microseconds spent in sliding / linear move search; default 0 */
  virtual unsigned long long getLinearSearchUs(void) const { return 0; }

  /**
   * What the take-apart under way is doing; may be called from another
   * thread at any time. False when this disassembler does not say.
   */
  virtual bool getProgress(disassemblyProgress_c & /*p*/) const { return false; }

  /**
   * Threads to spread a level of the search over; null (the default) to
   * search on the calling thread alone. The pool must outlive the
   * disassembler. Not to be called while a take-apart runs.
   */
  virtual void setHelperPool(helperPool_c * /*pool*/) {}


  // no copying and assigning
  disassembler_c(const disassembler_c&) = delete;
  disassembler_c& operator=(const disassembler_c&) = delete;

};

#endif
