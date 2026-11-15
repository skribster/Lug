#pragma once

#include "lug_common.h"
#include "lug_value.h"
#include "lug_vector.h"
#include "lug_lexer.h"

// Forward declaration for VMCore since
// we need it for memory operations

namespace Luggage {
    
class VMCore;
struct ObjThing;

struct SourcePosition {
    std::size_t line;       // The line number.
    std::size_t column;     // The column number.
    std::size_t offset;     // The index of the first instruction of that line.
};

enum class FunctionType {
    FUNCTION,
    SCRIPT
};

/**
 * Optimization: Inline caching!
 * 
 * Whenever we encounter an attribute getter during compile
 * time, we create a new attribute cache inside our chunk.
 * 
 * We can then use that cache to save ourselves from looking
 * through a lengthy `thing` lookup chain. We'll save the value
 * that we got when we looked up the attribute name on the 'thing'
 * during runtime.
 */
struct AttributeCache {
    // The object that we're trying to access.
    Obj* receiver{ nullptr };

    // For cache invalidation, the VM's global epoch that it
    // currently was when it searched the lookup chain. Recall
    // that the VM's global epoch increments when:
    // * a table gains a new key
    // * a key is deleted
    // * a table rehashes or grows (the slot pointer would dangle)
    // * an object's proto is changed
    // * an object is freed by the GC
    std::uint64_t cachedEpoch{ 0 };

    // The value that the attribute name yields.
    const Value* value{ nullptr };
};

struct Chunk {
public: 
    using ConstantIndex = std::uint16_t;

    explicit Chunk(VMCore& vm) 
        : vm(vm), constants(vm), bytecode(vm), positions(vm), attributeCaches(vm)
    {};
    ~Chunk();

    void Write(std::uint8_t byte, const Token& where);
    void WriteShort(std::uint16_t byte, const Token& where);
    void WriteConstant(Value value, const Token& where);
    ConstantIndex AddConstant(Value value);
    std::uint16_t AddCache();
    SourcePosition GetSourcePosition(std::size_t byteOffset);

    Vector<std::uint8_t> bytecode;
    Vector<SourcePosition> positions;
    Vector<Value> constants;

    Vector<AttributeCache> attributeCaches;

private:
    VMCore& vm;
};


}