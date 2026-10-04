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
 * Compressing large files on every core, in the spirit of pigz: the data is
 * cut into chunks that are packed and unpacked independently and in
 * parallel, a few at a time, so memory stays bounded however large the
 * file. Used for a stacking search's saved files in a .btsolve export.
 *
 * A file of sorted 16-byte keys (a search level) packs far smaller as the
 * gaps between keys: each key is written as its difference from the one
 * before, as a variable-length number, and that is deflated. Anything else
 * is just deflated.
 *
 * Packed form, after a "BTZ1" signature and the file's size (u64): chunks,
 * each [kind u8][raw bytes u64][encoded bytes u64][packed bytes u64][data].
 * Kind 0 is deflated bytes, kind 1 deflated key gaps. Little-endian.
 */
#ifndef __BLOCKPACK_H__
#define __BLOCKPACK_H__

#include <cstdint>
#include <filesystem>
#include <iostream>

namespace blockpack {

/**
 * Pack file into out. With keys, the file is read as sorted 16-byte keys
 * (a chunk that is not falls back to plain deflate). threads 0 for every
 * core. chunkBytes 0 for 64 MB; others, a multiple of 16, are for the
 * tests. Returns the bytes written, or 0 on an error.
 */
uint64_t pack(const std::filesystem::path & file, std::ostream & out, bool keys,
              unsigned int threads = 0, size_t chunkBytes = 0);

/**
 * Unpack bytes bytes of packed data from in into file. False on damaged
 * data or a write error.
 */
bool unpack(std::istream & in, uint64_t bytes, const std::filesystem::path & file,
            unsigned int threads = 0);

}

#endif
