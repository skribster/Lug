#pragma once

#include "lug_common.h"
#include "lug_chunk.h"

/**
 * This namespace is for internal use,a nd it's mainly
 * to give extra information about the VM's execution at
 * runtime.
 */
namespace Lug::Debug {

/**
 * A function to 'disassemble' a chunk. Essentially, this procedure
 * prints out a chunk's bytecode in an easily-readable format. You
 * can see this format when certain flags, like LUG_DEBUG_PRINT_CODE
 * defined in lug_common.h are turned on.
 */
void DisassembleChunk(Chunk& chunk, const char* name);

/**
 * A function to 'disassemble' a single instruction. It takes an offset
 * into a bytecode stream and displays a readable format for that instruction
 * in the output.
 */
std::size_t DisassembleInstruction(Chunk& chunk, std::size_t offset, bool isShort = false);

}