#pragma once

#include <cstring>
#include <initializer_list>

#include "lug.hpp"

#include "lug_common.h"
#include "lug_value.h"
#include "lug_object.h"
#include "lug_chunk.h"
#include "lug_table.h"
#include "lug_compiler.h"
#include "lug_gc.h"

namespace Lug {

class VMCore;

struct CallFrame {
    ObjClosure* closure{ nullptr };
    std::uint8_t* ip{ nullptr };
    Value* slots{ nullptr };
};


struct Handle {
    Value value;
    
    VMCore* vm{ nullptr };
    Handle* previous { nullptr };
    Handle* next{ nullptr };
};


struct Stack {
    static constexpr std::size_t FRAMES_MAX = 64;
    static constexpr std::size_t STACK_MAX = (FRAMES_MAX * UINT8_COUNT);

    CallFrame frames[FRAMES_MAX];
    std::size_t frameCount{ 0 };
    Value stack[STACK_MAX];
    Value* top{ nullptr };

    inline void AddFrame(ObjClosure* closure, Value* slots, int offset = 0) {
        CallFrame& newFrame = frames[frameCount++];
        newFrame.closure = closure;
        newFrame.ip = closure->function->chunk.bytecode.Data() + offset;
        newFrame.slots = slots;
    }

    inline CallFrame* GetFrame() {
        return &frames[frameCount - 1];
    }

    inline std::filesystem::path CurrentModulePath() {
        ObjString* sourcePath = GetFrame()->closure->module->path;
        return std::filesystem::path(sourcePath->characters);
    }

    inline void RemoveFrame() {
        top = GetFrame()->slots;
        frameCount--;
    }

    inline void Reset() {
        top = stack;
        frameCount = 0;
    }
    
    inline void Print() {
        for (Value* slot = stack; slot < top; slot++) {                        \
            std::printf("[ ");                                                      \
            Value::Print(*slot);                                                    \
            std::printf(" ]");                                                      \
        }                                                                           \
                                                                                    \
        std::printf("\n");  
    }

    inline void Push(Value value) {
        *top = value;
        top++;
    }

    inline Value& Peek(std::size_t distance) const {
        /*
            Clang-cl doesn't like it when I did top[-1 - distance] :(
            it wraps around to a really huge positive number which did
            some really wacky stuff and was a headache to figure out
        */
        return *(top - 1 - distance);
    }

    inline Value Pop() {
        top--;
        return *top;
    }
};


class VMCore {
public:
    VMCore(VMConfig config);
    ~VMCore();
  
    InterpretResult Interpret(std::string_view source, std::filesystem::path sourcePath);
    void DefineNative(const char* name, NativeFn function);
    ObjModule* LoadModule(std::filesystem::path importerPath, std::filesystem::path modulePath);
    
    // Garbage collector methods ----------------

    void AddObject(Obj* obj);
    void CollectGarbage();
    void MarkRoots();

    Handle* MakeHandle(Value value);
    std::shared_ptr<Handle> MakeSharedHandle(Value value);
    void ReleaseHandle(Handle* handle);
    
    inline void AdvanceEpoch() { globalEpoch++; }

    // Function methods -------------------------

    void PushFrame(ObjClosure* closure, std::uint8_t argCount, std::uint8_t shared);
    NO_INLINE void PushFrameSlow(ObjClosure* closure, std::uint8_t argCount, std::uint8_t shared);
    
    bool CallValue(Value value, std::uint8_t argCount, std::uint8_t shared = 0);
    bool CallDunder(Value dunder, std::initializer_list<Value> args, std::uint8_t shared = 0);
    void Call(ObjClosure* closure, std::uint8_t argCount, std::uint8_t shared = 0);
    ObjUpvalue* CaptureUpvalue(Value* local);
    void CloseUpvalues(Value* last);

    // Getter methods ---------------------------

    Variant ToVariant(const Value& value);
    Value ToValue(const Variant& variant, std::string_view name = "");

    ObjThing* GetProto(Value value) const;
    ObjClosure* GetDunder(Value value, const char* name) const;
    Variant GetGlobal(std::string_view name);
    void SetGlobal(std::string_view name, const Variant& variant);
    
    // Error methods ----------------------------

    std::vector<StackFrame> StackTraceback();
    void PushError(Error error);

    Error RuntimeError(Value payload);
    template<typename... Args> Error RuntimeError(const char* format, Args... args);
    template<typename... Args> std::string FormatString(const char* format, Args... args);
    
    // Member variables -------------------

    Compiler compiler;
    GC gc;
    
    Table strings;
    Table builtins;
    Table modules;
    Stack stack;

    VMConfig config;
    
private:
    void Run();
    
    Error lastError;
    Error memoryError;
    
    std::uint64_t globalEpoch{ 0 };
    ObjUpvalue* openUpvalues{ nullptr };
    Handle* handles{ nullptr };
    
    friend class VM;
};


template<typename... Args>
std::string VMCore::FormatString(const char* format, Args... args) {
    std::string formatted;
    if constexpr (sizeof...(args) == 0) {
        formatted = format;
    } else {
        // Compute required length for the std::string
        int length = std::snprintf(nullptr, 0, format, args...);
        LUG_ASSERT(length, "snprintf formatting error");

        formatted.resize(length * sizeof(char));

        // Allocate exactly enough space
        // Format the error message with the args
        std::snprintf(formatted.data(), static_cast<std::size_t>(length) + 1, format, args...);
    } 

    return formatted;
}


template<typename... Args>
Error VMCore::RuntimeError(const char* format, Args... args) {
    std::string errorMessage = FormatString(format, args...);

    ObjString* messageValue = ObjString::FromSource(
        *this, errorMessage.c_str(), 
        std::strlen(errorMessage.c_str())
    );

    return RuntimeError(messageValue);
}

}