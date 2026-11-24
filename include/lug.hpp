#pragma once

/**
 * Lug's public header. Gives all the basics that you need 
 * to muddy your hands with Lug's C++ API.
 * 
 * If you're a fellow code lurker, hi! Welcome to the
 * mess I've been making for the past two months since
 * writing this. :)
 * 
 * To understand the codebase more and the flow of logic,
 * I suggest this order:
 * 
 * * Lug code is passed through Lug::VM. Interpret and
 * InterpretFile are the two main functions for interpreting
 * Lug code. The VM uses the PIMPL pattern, which means
 * that Lug::VM really is just a wrapper for the beefier,
 * larger Lug::VMCore class that actually holds all the 
 * logic for interpreting everything. To learn more about
 * the general interface between C++ and Lug, inspect the
 * public headers and VMCore to find out more.
 * 
 * * Lug code first gets compiled into an intermediate
 * representation called *bytecode*, then ran through
 * the VM. In Lug::VM's interpret functions, the source
 * code is eventaully passed through Lug::Compiler, where 
 * it writes to a Lug::Chunk that contains the compiled 
 * bytecode. Think of bytecode as just a series of 
 * instructions that Lug::VM will run during... well,
 * runtime. Each byte corresponds to a certain instruction
 * that the VM will execute. To learn more about how source code 
 * is compiled into bytecode, read first: lug_lexer.h
 * to see how source code is converted into tokens, and 
 * finally, lug_compiler.h to see how it uses those tokens
 * to generate bytecode.
 * 
 * * Lug code is interpreted inside Lug::VMCore. This is
 * the 'brain' of the operation, and it's where it goes 
 * through each byte in the bytecode stream, each byte 
 * corresponding to a specific VM instruction.
 * For example, when the VM reaches OP_POP, it will pop the
 * top element of its virtual stack. To learn more about 
 * how the VM handles executing bytecode, read lug_vmcore.h.
 *  
 * * Lug's dynamic values are represented with a Value 
 * struct. Values can either be primitives, where the data
 * is directly stored in the struct, or a larger structure
 * that can vary in size. We call those structures objects,
 * and they are heap-allocated and instead stored as a
 * pointer inside a Value. To learn more about the split 
 * and how Values are passed around and used, check out
 * lug_value.h and lug_object.h.
 * 
 * * Lug uses a mark-and-sweep garbage collector. Memory
 * is precious and, obviously, we want to free up unused
 * memory in the program. To combat memory leaks, Lug uses
 * a stop-the-world mark-and-sweep garbage collector, which
 * frees up Objects in memory. Objects, remember, are
 * allocated on the heap, meaning they have to be managed
 * manually by the program. To learn more about how Lug
 * manages memory, read lug_gc.h.
 * 
 * * Lug's library layout is very similar to Lua's. Each 
 * library module is stored inside its own .cpp file, where
 * it implements a load function from lug_stdlib.h that loads
 * all of its library functions.
 */

#include "lug_version.hpp"
#include "lug_variant.hpp"
#include "lug_vm.hpp"