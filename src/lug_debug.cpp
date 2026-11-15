#include <cstdio>

#include "lug_debug.h"
#include "lug_opcodes.h"
#include "lug_object.h"

namespace Luggage::Debug {

// Read functions -------------------------

/**
 * Reads a uint16_t from the given chunk and instruction offset.
 * This function uses the two bytes RIGHT AFTER the passed-in
 * instruction offset.
 */
static std::uint16_t readShort(Chunk& chunk, std::size_t at) {
    std::uint16_t sh = (chunk.bytecode[at + 1] << 8);
    sh |= chunk.bytecode[at + 2];

    return sh;
}

/**
 * Returns a uint8_t RIGHT AFTER the given instruction offset
 * of a chunk. 
 */
static std::uint8_t readByte(Chunk& chunk, std::size_t at) {
    return chunk.bytecode[at + 1];
}

// Instruction printing helper functions -------------------------

static std::size_t simpleInstruction(const char* name, std::size_t offset, bool isShort) {
    // OP_<name>
    // +offset : 1

    std::printf("%s\n", name);
    return offset + 1;
}


static std::size_t constantInstruction(const char* name, Chunk& chunk, std::size_t offset, bool isShort) {
    // OP_CONSTANT <shortable>
    // +offset: 2 or 3

    std::uint16_t constantIndex;

    if (isShort) {
        constantIndex = readShort(chunk, offset);
    } else {
        constantIndex = readByte(chunk, offset);
    }

    std::printf("%-20s %4d '", name, constantIndex);
    Value::Print(chunk.constants[constantIndex]);
    isShort ? std::printf("' (short)\n") : std::printf("'\n");

    return isShort ? (offset + 3) : (offset + 2);
}


static int jumpInstruction(const char* name, int sign, Chunk& chunk, std::size_t offset, bool isShort) {
    // OP_JUMP <short>
    // +offset : 3

    std::uint16_t jump = (std::uint16_t)(chunk.bytecode[offset + 1] << 8);
    jump |= chunk.bytecode[offset + 2];
    std::printf("%-20s %4zu -> %zu\n", name, offset,
            offset + 3 + sign * jump);
    return offset + 3;
}


static int patchInstruction(const char* name, Chunk& chunk, std::size_t offset, bool isShort) {
    // OP_PATCH <short>
    // +offset : 3

    std::uint16_t jump = (std::uint16_t)(chunk.bytecode[offset + 1] << 8);
    jump |= chunk.bytecode[offset + 2];

    if (jump == UINT16_MAX) {
        std::printf("%-20s %4zu -> 'NO_BREAK'\n", name, offset, isShort);
        return offset + 3;
    }

    std::printf("%-20s %4zu -> %zu\n", name, offset,
            offset - jump);

    return offset + 3;
}


static std::size_t byteInstruction(const char* name, Chunk& chunk, std::size_t offset, bool isShort) {
    // OP_<byteInstruction> <byte>
    // +offset : 2

    std::uint8_t slot = chunk.bytecode[offset + 1];
    std::printf("%-20s %4d\n", name, slot);
    return offset + 2;
}


static std::size_t sharedInstruction(const char* name, Chunk& chunk, std::size_t offset, bool isShort) {
    // OP_CALL_SHARED <fnSlot> <sharedMask>
    // +offset : 3

    std::uint8_t slot = chunk.bytecode[offset + 1];
    std::uint16_t sharedArgs = chunk.bytecode[offset + 2];
    
    std::printf("%-20s %4d '", name, slot); 

    // Print sharedArgs as binary representation
    for (int i = 7; i >= 0; i--) {
        // Shift bit to the 1st position and mask it
        std::printf("%d", (sharedArgs >> i) & 1);
        
        // Add a space every 4 bits for easy readability
        if (i % 4 == 0 && i != 0) {
            printf(" ");
        }
    }

    std::printf("'\n");

    return offset + 3;
}


static std::size_t shortInstruction(const char* name, Chunk& chunk, std::size_t offset, bool isShort) {
    // OP_SHORT <realOpcode> ...
    // +offset : depends

    std::printf("\r");
    OpCode::Type realOpcode = static_cast<OpCode::Type>(readByte(chunk, offset));  
    std::size_t realOffset = DisassembleInstruction(chunk, offset + 1, true);

    return offset + realOffset; // to account for OP_SHORT at the start
}


static std::size_t getInstruction(const char* name, Chunk& chunk, std::size_t offset, bool isShort) {
    // OP_GET_ATTRIBUTE <shortable> <byte>
    // +offset : 3 or 4
    std::uint16_t attributeConstant;
    if (isShort) {
        attributeConstant = readShort(chunk, offset);
        offset += 2;
    } else {
        attributeConstant = readByte(chunk, offset);
        offset += 1;
    }

    std::printf("%-20s %4d '", name, attributeConstant);
    Value::Print(chunk.constants[attributeConstant]);
    isShort ? std::printf("' (short) ") : std::printf("' ");
    std::printf("%4d \n", readByte(chunk, offset++));

    return offset;
}


// Debug methods -------------------------

std::size_t DisassembleInstruction(Chunk& chunk, std::size_t offset, bool isShort) {
    // Print out the bytecode offset on the left.
    std::printf("%04zu ", offset);

    // Display line information
    // If we have two instructions that are the same line,
    // we simply display a | to symbolize that the line
    // continues on for these instructions. Only when we
    // encounter a new line do we display the line number.
    if (offset > 0 && chunk.GetSourcePosition(offset).line == chunk.GetSourcePosition(offset - 1).line) {
        std::printf("   | ");
    } else {
        std::printf("%4zu ", chunk.GetSourcePosition(offset).line);
    }

    std::uint8_t instruction = chunk.bytecode[offset];
    switch (static_cast<OpCode::Type>(instruction)) {
        case OpCode::CONSTANT: {
            return constantInstruction("OP_CONSTANT", chunk, offset, isShort);
        }

        case OpCode::SHORT: {
            return shortInstruction("OP_SHORT", chunk, offset, isShort);
        }

        case OpCode::NEGATE: {
            return simpleInstruction("OP_NEGATE", offset, isShort);
        }

        case OpCode::NOT: {
            return simpleInstruction("OP_NOT", offset, isShort);
        }

        case OpCode::TRUE: {
            return simpleInstruction("OP_TRUE", offset, isShort);
        }

        case OpCode::FALSE: {
            return simpleInstruction("OP_FALSE", offset, isShort);
        }

        case OpCode::NIL: {
            return simpleInstruction("OP_NIL", offset, isShort);
        }

        case OpCode::ADD: {
            return simpleInstruction("OP_ADD", offset, isShort);
        }

        case OpCode::SUBTRACT: {
            return simpleInstruction("OP_SUBTRACT", offset, isShort);
        }

        case OpCode::MULTIPLY: {
            return simpleInstruction("OP_MULTIPLY", offset, isShort);
        }

        case OpCode::DIVIDE: {
            return simpleInstruction("OP_DIVIDE", offset, isShort);
        }
        
        case OpCode::MODULO: {
            return simpleInstruction("OP_MODULO", offset, isShort);
        }
        
        case OpCode::EQUAL: {
            return simpleInstruction("OP_EQUAL", offset, isShort);
        }
        
        case OpCode::GREATER: {
            return simpleInstruction("OP_GREATER", offset, isShort);
        }
        
        case OpCode::GREATER_EQUAL: {
            return simpleInstruction("OP_GREATER_EQUAL", offset, isShort);
        }
        
        case OpCode::LESS: {
            return simpleInstruction("OP_LESS", offset, isShort);
        }
        
        case OpCode::LESS_EQUAL: {
            return simpleInstruction("OP_LESS_EQUAL", offset, isShort);
        }

        case OpCode::JUMP: {
            return jumpInstruction("OP_JUMP", 1, chunk, offset, isShort);
        }

        case OpCode::JUMP_IF_FALSE: {
            return jumpInstruction("OP_JUMP_IF_FALSE", 1, chunk, offset, isShort);
        }   
        
        case OpCode::JUMP_IF_TRUE: {
            return jumpInstruction("OP_JUMP_IF_TRUE", 1, chunk, offset, isShort);
        }

        case OpCode::JUMP_IF_FALSE_OR_POP: {
            return jumpInstruction("OP_JUMP_IF_FALSE_OR_POP", 1, chunk, offset, isShort);
        }

        case OpCode::JUMP_IF_TRUE_OR_POP: {
            return jumpInstruction("OP_JUMP_IF_TRUE_OR_POP", 1, chunk, offset, isShort);
        }

        case OpCode::POP_JUMP_IF_FALSE: {
            return jumpInstruction("OP_POP_JUMP_IF_FALSE", 1, chunk, offset, isShort);
        }

        case OpCode::POP_JUMP_IF_TRUE: {
            return jumpInstruction("OP_POP_JUMP_IF_TRUE", 1, chunk, offset, isShort);
        }
  
        case OpCode::LOOP: {
            return jumpInstruction("OP_LOOP", -1, chunk, offset, isShort);
        }

        case OpCode::COUNT_PREP: {
            return jumpInstruction("OP_COUNT_PREP", 1, chunk, offset, isShort);
        }

        case OpCode::COUNT_LOOP: {
            return jumpInstruction("OP_COUNT_LOOP", -1, chunk, offset, isShort);
        }

        case OpCode::FOREACH_PREP: {
            return jumpInstruction("OP_FOREACH_PREP", 1, chunk, offset, isShort);
        }

        case OpCode::FOREACH_LOOP: {
            return jumpInstruction("OP_FOREACH_LOOP", -1, chunk, offset, isShort);
        }

        case OpCode::PLACEHOLDER: {
            return patchInstruction("OP_PLACEHOLDER", chunk, offset, isShort);
        }

        case OpCode::POP: {
            return simpleInstruction("OP_POP", offset, isShort);
        }

        case OpCode::DUPLICATE: {
            return simpleInstruction("OP_DUPLICATE", offset, isShort);
        }

        case OpCode::COPY: {
            return simpleInstruction("OP_COPY", offset, isShort);
        }

        case OpCode::DEFINE_GLOBAL: {
            return constantInstruction("OP_DEFINE_GLOBAL", chunk, offset, isShort);
        }
        
        case OpCode::GET_GLOBAL: {
            return constantInstruction("OP_GET_GLOBAL", chunk, offset, isShort);
        }
        
        case OpCode::SET_GLOBAL: {
            return constantInstruction("OP_SET_GLOBAL", chunk, offset, isShort);
        }
        
        case OpCode::GET_LOCAL: {
            return byteInstruction("OP_GET_LOCAL", chunk, offset, isShort);
        }

        case OpCode::SET_LOCAL: {
            return byteInstruction("OP_SET_LOCAL", chunk, offset, isShort);
        }

        case OpCode::GET_UPVALUE: {
            return byteInstruction("OP_GET_UPVALUE", chunk, offset, isShort);
        }

        case OpCode::SET_UPVALUE: {
            return byteInstruction("OP_SET_UPVALUE", chunk, offset, isShort);
        }

        case OpCode::CLOSE_UPVALUE: {
            return simpleInstruction("OP_CLOSE_UPVALUE", offset, isShort);
        }

        case OpCode::CALL: {
            return byteInstruction("OP_CALL", chunk, offset, isShort);
        }

        case OpCode::CALL_SHARED: {
            return sharedInstruction("OP_CALL_SHARED", chunk, offset, isShort);
        }

        case OpCode::SELF: {
            return constantInstruction("OP_SELF", chunk, offset, isShort);
        }

        case OpCode::CLOSURE: {
            offset++;
            
            std::uint8_t constant = chunk.bytecode[offset++];
            std::printf("%-20s %4d ", "OP_CLOSURE", constant);
            Value::Print(chunk.constants[constant]);
            std::printf("\n");

            ObjFunction* function = chunk.constants[constant].AsObj<ObjFunction>();
            for (int j = 0; j < function->upvalueCount; j++) {
                int isLocal = chunk.bytecode[offset++];
                int index = chunk.bytecode[offset++];

                std::printf(
                    "%04zu      |                         %s %d\n",
                    offset - 2, 
                    isLocal ? "local" : "upvalue", 
                    index
                );
            }

            return offset;
        }

        case OpCode::LIST: {
            return simpleInstruction("OP_LIST", offset, isShort);
        }

        case OpCode::DICT: {
            return simpleInstruction("OP_DICT", offset, isShort);
        }

        case OpCode::THING: {
            return simpleInstruction("OP_THING", offset, isShort);
        }

        case OpCode::CATEGORY: {
            return simpleInstruction("OP_CATEGORY", offset, isShort);
        }

        case OpCode::APPEND: {
            return simpleInstruction("OP_APPEND", offset, isShort);
        }

        case OpCode::DEFINE_ENTRY: {
            return simpleInstruction("OP_DEFINE_ENTRY", offset, isShort);
        }

        case OpCode::GET_INDEX: {
            return simpleInstruction("OP_GET_INDEX", offset, isShort);
        }
        
        case OpCode::SET_INDEX: {
            return simpleInstruction("OP_SET_INDEX", offset, isShort);
        }

        case OpCode::GET_ATTRIBUTE: {
            return getInstruction("OP_GET_ATTRIBUTE", chunk, offset, isShort);
        }

        case OpCode::SET_ATTRIBUTE: {
            return constantInstruction("OP_SET_ATTRIBUTE", chunk, offset, isShort);
        }
        
        case OpCode::RETURN: {
            return simpleInstruction("OP_RETURN", offset, isShort);
        }

        default: {
            std::printf("unknown opcode '%d'\n", instruction);
            return offset + 1;
        }
    }
}



void DisassembleChunk(Chunk& chunk, const char* name) {
    std::printf("== %s ==\n", name);

    /**
     * We're going to loop through the bytecode stream and print
     * each instruction.
     */
    for (std::size_t offset = 0; offset < chunk.bytecode.Count();) {
        offset = DisassembleInstruction(chunk, offset);
    }
}


}