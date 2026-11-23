#include <cstdio>
#include <algorithm>
#include <cmath>
#include <unordered_map>

#include "lug.hpp"
#include "lug_vmcore.h"
#include "lug_opcodes.h"
#include "lug_debug.h"
#include "lug_memory.h"
#include "lug_stdlib.h"
#include "lug_filesys.h"
#include <iostream>

namespace fs = std::filesystem;

namespace Lug {

// Helper functions -----------------------------------

/**
 * A value is falsy if it's explicitly set as 'nil' or 'false'.
 * Anything else is truthy.
 */
static inline bool IsFalsy(Value value) {
    return value.IsNil() || value.IsBool() && !value.AsBool();
}


static bool IsTruthy(Value value) {
    return !IsFalsy(value);
}


/**
 * Counts the trailing zeroes in a std::uint8_t byte
 * before the first set-bit.
 */
static int CountTrailingZeroes(std::uint8_t byte) {
    if (byte == 0) return 8;

    int index = 0;
    while (((byte >> index) & 1) == 0) index++;

    return index;
}


static void PrintMask(std::uint8_t mask) {
    // Print mask as binary representation
    for (int i = 7; i >= 0; i--) {
        // Shift bit to the 1st position and mask it
        std::printf("%d", (mask >> i) & 1);
        
        // Add a space every 4 bits for easy readability
        if (i % 4 == 0 && i != 0) {
            printf(" ");
        }
    }
    std::printf("\n");
}


// VMCore methods -----------------------------------

VMCore::VMCore(VMConfig config) 
    : strings(*this)
    , builtins(*this)
    , gc(*this)
    , compiler(*this)
    , modules(*this)
    , config(config)
{
    // We'll initialize the memory LugError at the start of the program
    // so that we don't have to allocate it later on.
    memoryError.type = InterpretResult::MEMORY_ERROR;
    memoryError.message = "unable to allocate further memory";

    stack.Reset();
}

VMCore::~VMCore() {
    for (
        Handle* handle = handles; 
        handle != nullptr; 
        handle = handle->next
    ) {
        handle->vm = nullptr;
    }
}


InterpretResult VMCore::Interpret(std::string_view source, fs::path sourcePath) {
    try {
        ObjModule* module = compiler.Compile(source.data(), sourcePath);
        stack.Push(module);

        ObjClosure* closure = ObjClosure::New(*this, module->function, module);
        stack.Pop(); // module
        stack.Push(closure);
        Call(closure, 0);

        // Pop the return value. During the REPL, the last OP_RETURN
        // started to push the 'nil' value onto the very bottom of the
        // stack, causing a bunch of 'nil' values to accumulate at the bottom. 
        stack.Pop();

    } catch (Error& error) {
        PushError(error);
        
        //! Not the best way right now to handle errors. Ideally the user'd like the stack to
        // rewind to how it was, but this is a good temporary fix.
        stack.Reset();
        return error.type;

    } catch (std::bad_alloc& error) {
        PushError(memoryError);
        stack.Reset();

        return lastError.type;
    } 

    return InterpretResult::OK;
}


Value VMCore::ToValue(const Variant& variant, std::string_view name) {
    return std::visit([&](const auto& arg) -> Value {
        using VariantType = std::decay_t<decltype(arg)>;

        if constexpr (std::is_same_v<VariantType, std::monostate>) {
            return Value::Nil();
        } 

        else if constexpr (std::is_same_v<VariantType, bool>) {
            return arg;
        } 

        else if constexpr (std::is_same_v<VariantType, double>) {
            return arg;
        } 
        
        else if constexpr (std::is_same_v<VariantType, std::string>) {
            return ObjString::FromSource(*this, arg.c_str(), arg.size());
        }
        
        else if constexpr (std::is_same_v<VariantType, List>) {
            return arg.handle->value;
        } 

        else if constexpr (std::is_same_v<VariantType, Dict>) {
            return arg.handle->value;
        } 

        else if constexpr (std::is_same_v<VariantType, Thing>) {
            return arg.handle->value;
        }

        else if constexpr (std::is_same_v<VariantType, Function>) {
            return ObjNative::New(*this, std::string(name).c_str(), arg);
        }

        return Value::Nil();
    }, variant.Data());
}


Variant VMCore::ToVariant(const Value& value) {
    switch (value.Type()) {
        // Primitive types ----------------------
        case ValueType::BOOLEAN: return Variant{ value.AsBool() };
        case ValueType::NUMBER: return Variant{ value.AsNumber() };

        case ValueType::OBJ: {
            if (value.Is<ObjString>()) {
                ObjString* string = value.As<ObjString>();
                return Variant{ std::string(string->characters, string->length) };
            }
            
            else if (value.Is<ObjList>()) {
                ObjList* list = value.As<ObjList>();
                Lug::List newList(MakeSharedHandle(list));

                return Variant{ newList }; 
            }
            
            else if (value.Is<ObjDict>()) {
                ObjDict* dict = value.As<ObjDict>();
                Lug::Dict newDict(MakeSharedHandle(dict));

                return Variant{ newDict }; 
            }

            else if (value.Is<ObjThing>()) {
                ObjThing* thing = value.As<ObjThing>();
                Lug::Thing newThing(MakeSharedHandle(thing));

                return Variant{ newThing };
            }

            else if (value.Is<ObjNative>()) {
                ObjNative* native = value.As<ObjNative>();

                if (native->userFunction) {
                    return Variant{ native->userFunction };
                }

                return Variant{ };
            }
        }

        default: return Variant{ };
    }
}


/**
 * Gets a value's proto, if there is any.
 */
ObjThing* VMCore::GetProto(Value value) const {
    Value* proto = builtins.Get(value.StringType());
    if (proto == nullptr || !proto->Is<ObjThing>()) {
        return nullptr;
    }

    return proto->As<ObjThing>();
}


/**
 * Attempts to find a dunder method of a given value.
 * If the value is not already an ObjThing*, it'll look up its proto
 * and try to find the dunder there.
 */
ObjClosure* VMCore::GetDunder(Value value, const char* name) const {
    if (value.Is<ObjThing>()) {
        ObjThing* thing = value.As<ObjThing>();
        const Value* dunder = thing->Get(name);

        if (dunder == nullptr) return nullptr;
        if (!(dunder->Is<ObjClosure>() || dunder->Is<ObjNative>())) return nullptr;

        return dunder->As<ObjClosure>();
    }

    if (ObjThing* proto = GetProto(value)) {
        const Value* dunder = proto->Get(name);

        if (dunder == nullptr) return nullptr;
        if (!dunder->Is<ObjClosure>()) return nullptr;

        return dunder->As<ObjClosure>();
    }

    return nullptr;
}


void VMCore::SetGlobal(std::string_view name, const Variant& variant) {
    stack.Push(ToValue(variant, name));
    stack.Push(ObjString::FromSource(*this, name.data(), name.size()));

    builtins.Set(stack.Peek(0).As<ObjString>(), stack.Peek(1));

    stack.Pop(); // ObjString
    stack.Pop(); // Value
}


Variant VMCore::GetGlobal(std::string_view name) {
    ObjString* key = ObjString::FromSource(*this, name.data(), name.size());
    stack.Push(key);

    Value* found = builtins.Get(key);
    Variant result = found ? ToVariant(*found) : Variant{} ;
    
    stack.Pop(); // Key

    return result;
}


/**
 * Returns a string of the script's stack traceback.
 */
std::vector<StackFrame> VMCore::StackTraceback() {
    std::vector<StackFrame> traceback;

    for (int i = stack.frameCount - 1; i >= 0; i--) {
        StackFrame stackFrame;

        CallFrame& frame = stack.frames[i];
        ObjModule* module = frame.closure->module;
        ObjFunction* function = frame.closure->function;
        std::size_t instruction = frame.ip - function->chunk.bytecode.Data() - 1;
        SourcePosition sourcePosition = function->chunk.GetSourcePosition(instruction);;

        // Set 'from'
        fs::path path{ module->path->characters };
        stackFrame.location.file = path.filename().string();
        stackFrame.location.line = sourcePosition.line;
        stackFrame.location.column = sourcePosition.column;

        // Set 'function'
        if (function->name != nullptr) {
            std::string functionName(function->name->characters);
            functionName.append("()");
            stackFrame.function = std::move(functionName);
        } else {
            stackFrame.function = std::string("script");
        }
        
        traceback.push_back(stackFrame);
    }

    return traceback;
}


/**
 * 'Pushing' an error just means updating the lastError
 * and also calling the user-defined error callback.
 */
void VMCore::PushError(Error error) {
    lastError = error;
    config.errorFn(error);
}


Error VMCore::RuntimeError(Value payload) {
    Error error;
    error.type = InterpretResult::RUNTIME_ERROR;

    if (payload.Is<ObjString>()) {
        error.message = payload.As<ObjString>()->characters;
    } else {
        ObjString* errorMessage = payload.ToString(*this);
        error.message = errorMessage->characters;
    }
    
    error.stackTraceback = StackTraceback();
    error.location = error.stackTraceback.front().location;

    stack.Push(payload);

    return error;
}


/**
 * Defines a single native function as a global with 
 * the given name.
 */
void VMCore::DefineNative(const char* name, NativeFn function) {
    stack.Push(ObjString::FromLiteral(*this, name));
    stack.Push(ObjNative::New(*this, name, function));

    builtins.Set(stack.Peek(1).As<ObjString>(), stack.Peek(0));

    stack.Pop();
    stack.Pop();
}


ObjModule* VMCore::LoadModule(fs::path importerPath, fs::path modulePath) {
    auto name = config.resolveModuleFn(importerPath, modulePath);
    if (!name) {
        throw RuntimeError("could not find file '%s'\n", modulePath.string().c_str());
    }

    // If it's already cached in the vm...
    ObjString* moduleName = ObjString::FromLiteral(*this, modulePath.string().c_str());
    stack.Push(moduleName);
    Value* cached = modules.Get(moduleName);
    if (cached && cached->Is<ObjModule>()) {
        stack.Pop();
        return cached->As<ObjModule>();
    }

    auto source = config.loadModuleFn(*name);
    if (!source) {
        throw RuntimeError("could not load file '%s'\n", modulePath.string().c_str());
    }

    ObjModule* module = compiler.Compile((*source).data(), modulePath);
    stack.Push(module);
    modules.Set(moduleName, module);
    stack.Pop(); // module
    stack.Pop(); // moduleName

    return module;
}


/**
 * Adds an object to the VM's object linked list.
 */
void VMCore::AddObject(Obj* obj) {
    gc.AddObject(obj);
}


/**
 * Frees all the VM's unused memory.
 */
void VMCore::CollectGarbage() {
    gc.Collect();
}


/**
 * This marks all of the values that the VM can reach directly
 * without going through a reference, otherwise known as the roots.
 */
void VMCore::MarkRoots() {
    /*
        The VM can access all the values on the stack. Let's loop 
        through the stack and mark any object.
    */
    for (Value* slot = stack.stack; slot < stack.top; slot++) {
        gc.MarkValue(*slot);
    }

    /*
        The VM can also access all of the global variables without
        going through a reference. It is also able to access
        the cached modules. Let's mark those as well.
    */
    gc.MarkTable(builtins);
    gc.MarkTable(modules);

    /*
        Each call frame also has data that the VM can access.
        Each call frame has a closure that the VM uses for accessing
        upvalues, constants, etc. that may be of Obj type.

        Call frame -> Closure -> Chunk -> Constants -> other objects
        Call frame -> Closure -> Upvalues -> other objects
    */
    for (std::size_t i = 0; i < stack.frameCount; i++) {
        gc.MarkObject(stack.frames[i].closure);
    }

    /*
        The open upvalue list is another set of values that the
        VM can directly reach. Let's traverse the linked list and
        mark each one.
    */
    for (
        ObjUpvalue* upvalue = openUpvalues; 
        upvalue != nullptr; 
        upvalue = upvalue->next
    ) {
        gc.MarkObject(upvalue);
    }

    /*
        We also want to mark all the handles so that the user of the
        API doesn't get a dangling reference to a Lug object.
    */
    for (
        Handle* handle = handles; 
        handle != nullptr; 
        handle = handle->next
    ) {
        gc.MarkValue(handle->value);
    }


    /*
        The compiler also grabs memory from the heap for literals
        and the constant table. We also want to keep track of those
        objects as well.
    */
    compiler.MarkRoots();
}


/**
 * Makes a new handle to a VM object. This prevents it from being
 * collected by the GC.
 */
Handle* VMCore::MakeHandle(Value value) {
    Handle* newHandle = new Handle{ value, this, nullptr, handles };    
   
    // Push the new handle to the front of the linked list
    if (handles) handles->previous = newHandle;
    handles = newHandle;

    return newHandle;
}


std::shared_ptr<Handle> VMCore::MakeSharedHandle(Value value) {
    return std::shared_ptr<Handle>(MakeHandle(value), [](Handle* handle) {
        // If the VM is still alive when we're accessing the handle,
        // we should unlink it from the handle linked list.
        if (handle->vm) {
            handle->vm->ReleaseHandle(handle);
        }
        
        // Otherwise, if this handle outlived its VM, we'll just delete
        // the node.
        else {
            delete handle;
        }
    });
}


/**
 * Releases the handle from the handle linked list. This will let
 * the GC collect the value.
 */
void VMCore::ReleaseHandle(Handle* handle) {
    if (handle->previous) {
        handle->previous->next = handle->next;
    } else {
        handles = handle->next;
    }

    if (handle->next) {
        handle->next->previous = handle->previous;
    }

    delete handle;
}


inline void VMCore::PushFrame(ObjClosure* closure, std::uint8_t argCount, std::uint8_t sharedMask) {
    const ObjFunction::Arguments& parameters = closure->function->parameters;

    // Hot path ---------------------------------

    if (LIKELY(parameters.simple && parameters.arity == argCount && stack.frameCount < Stack::FRAMES_MAX)) {
        stack.AddFrame(closure, stack.top - argCount - 1);
        return;
    }

    PushFrameSlow(closure, argCount, sharedMask);
}


NO_INLINE void VMCore::PushFrameSlow(ObjClosure* closure, std::uint8_t argCount, std::uint8_t sharedMask) {
    const ObjFunction::Arguments& parameters = closure->function->parameters;

    // Check arity ------------------------------
    /*
        Errors at:
        box a <- function(a, b) end
        a(1)
    */
    if (!parameters.variadic && argCount != parameters.arity && parameters.arity == parameters.minArity) {
        throw RuntimeError("expected %d argument(s) but got %d", parameters.arity, argCount);
    }

    /*
        Errors at:
        box a <- function(a, b, c <- 2) end
        a(1)
    */
    if (argCount < parameters.minArity) {
        throw RuntimeError("expected at least %d argument(s) but got %d", parameters.minArity, argCount);
    }

    /*
        Errors at:
        box a <- function(a <- 1, b <- 2) end
        a(1, 2, 3)
    */
    else if (argCount > parameters.arity && !parameters.variadic) {
        throw RuntimeError("expected at most %d argument(s) but got %d", parameters.arity, argCount);
    }

    // Handle varargs ---------------------------

    // If we have any varargs, we will pack all the 
    // extra args into a list.
    int numVarargs = std::max(argCount - parameters.arity, 0);
    if (parameters.variadic && numVarargs > 0) {
        ObjList* packed = ObjList::New(*this);

        for (int i = 0; i < numVarargs; i++) {
            Value arg = stack.Peek(0);

            stack.Push(packed);
            packed->Insert(0, arg);
            stack.Pop(); // list

            stack.Pop(); // arg
        }

        stack.Push(packed);
    }

    // Handle shared ----------------------------

    /*
        If the 'shared' parameters don't align, we error.
        The argCountMask is to make sure that this only checks for
        the arguments we passed IN. If we had a shared default argument
        and we don't supply it in the call site, we don't want to error.
    */
    std::uint8_t argCountMask = argCount >= 8 ? 0xFF : (1u << (argCount)) - 1;
    std::uint8_t masked = parameters.shared & ~sharedMask & argCountMask;

    if ((masked) != 0) {
        int index = CountTrailingZeroes(masked);
        throw RuntimeError("expected a shared argument at argument %d", index + 1);
    }

    if (stack.frameCount == Stack::FRAMES_MAX) {
        throw RuntimeError("stack overflow");
    }

    // Handle default args ----------------------

    int numOmitted = std::max(parameters.arity - argCount, 0);
    // We'll also account for the varargs and say that it's technically
    // another parameter that we omitted. 
    if (parameters.variadic && numVarargs == 0) numOmitted++;

    const Vector<std::size_t>& defaults = closure->function->defaults;
    int startOffset = 0;

    /*
        The start offset of the function is determined by how many parameters
        have been omitted. Recall that a function has a prebody to fill in
        the omitted parameters. i.e. a function that has:

        box test <- function(a <- 1, b <- 2) end
        test()

        will have a function chunk such as:
        == test ==
        * --------- MARK DEFAULT (0)
        0000    2 OP_CONSTANT             0 '1'
        * --------- MARK DEFAULT (2)
        0002    | OP_CONSTANT             1 '2'
        * --------- MARK DEFAULT (4 END)
        ... (rest of the function)

        Then, we can choose a starting offset, and when that function starts and
        goes through the prebody, those missing values will be filled in on the
        stack.
     */
    if (defaults.Count() > 0) {
        startOffset = defaults[defaults.Count() - numOmitted - 1];
    }

    if (parameters.variadic && numVarargs > 0) {
        /*
            If we have function a:
            box a <- function(req, args...) end

            And we pass in 1, 2, 3 as varargs to a function:
            [ <fn a >][ "required" ][ 1 ][ 2 ][ 3 ]
                                                   ^^^^^^^^^ stackTop

            We convert varargs to a list on the stack like this:
            [ <fn a> ][ "required" ][ [1, 2, 3] ]

            Therefore argCount goes from 4 -> 2. The formula is
            argCount = argCount - numVarargs (3) + 1 (the list)
         */
        argCount = argCount - numVarargs + 1;
    }
    /*
        The -1 is to account for stack slot zero, which we use
        to hold the function object on the stack.

        [ <fn a> ] [ "required" ][ [1, 2, 3] ]
                  ^ stackTop is right here...

         [ <fn a> ][ "required" ][ [1, 2, 3] ]
        ^ The -1 places it where it's supposed to go!
         GET_LOCAL(1) would now correctly get "required".
     */
    Value* slots = stack.top - argCount - 1;
    stack.AddFrame(closure, slots, startOffset);
}


/**
 * This function will attempt to call any value of any type.
 * Returns whether a new stack frame was pushed.
 */
bool VMCore::CallValue(Value value, std::uint8_t argCount, std::uint8_t shared) {
    if (!value.IsObj()) {
        throw RuntimeError("attempted to call a '%s' value", value.StringType());
    }

    switch (value.AsObj()->Type()) {
        case ObjType::CLOSURE: {
            PushFrame(value.As<ObjClosure>(), argCount, shared);
            return true;
        }
            
        case ObjType::NATIVE: {               
            ObjNative* native = value.As<ObjNative>();

            Value result;
            if (native->function) {
                result = native->function(*this, stack.top - argCount, argCount);
            } 
            
            else if (native->userFunction) { 
                std::vector<Variant> args;
                for (std::size_t i = 0; i < argCount; i++) {
                    Variant arg = ToVariant(stack.Peek(argCount - 1 - i));
                    args.push_back(std::move(arg));
                }

                try {
                    Variant returned = native->userFunction(args);
                    result = ToValue(returned);
                } catch (const std::exception& error) {
                    Lug::Error runtimeError = RuntimeError(error.what());
                    throw runtimeError;
                }
            }
                
            // Rewind the stack right before the ObjNative.
            stack.top -= argCount + 1;
                
            // Push the result of the function.
            stack.Push(result);
            return false;
        }
            
        case ObjType::THING: {
            // Check if the 'thing' has a '__call__' dunder 
            // attached to it.
            ObjThing* thing = value.As<ObjThing>();
            const Value* callDunder = thing->Get("__call__");
            if (callDunder == nullptr) break;
                
            return CallValue(*callDunder, argCount, shared);
        }

        default: {
            ObjThing* valueProto = GetProto(value);
            if (valueProto != nullptr) return CallValue(valueProto, argCount, shared);
            return false;
        }
    }

    return false;
}


bool VMCore::CallDunder(Value dunder, std::initializer_list<Value> args, std::uint8_t shared) {
    stack.Push(dunder);
    for (const Value& arg : args) {
        stack.Push(arg);
    }

    std::size_t argCount = args.size();
    return CallValue(dunder, argCount, shared);
}


/**
 * This function will call a given callee and run the interpreter
 * until the function finishes.
 */
void VMCore::Call(ObjClosure* closure, std::uint8_t argCount, std::uint8_t shared) {
    PushFrame(closure, argCount, shared);
    Run();
}


ObjUpvalue* VMCore::CaptureUpvalue(Value* local) {
    ObjUpvalue* previousUpvalue = nullptr;
    ObjUpvalue* upvalue = openUpvalues;

    while (upvalue != nullptr && upvalue->location > local) {
        previousUpvalue = upvalue;
        upvalue = upvalue->next;
    }

    if (upvalue != nullptr && upvalue->location == local) {
        return upvalue;
    }

    ObjUpvalue* createdUpvalue = ObjUpvalue::New(*this, local);
    createdUpvalue->next = upvalue;
    if (previousUpvalue == nullptr) {
        openUpvalues = createdUpvalue;
    } else {
        previousUpvalue->next = createdUpvalue;
    }

    return createdUpvalue;
}


void VMCore::CloseUpvalues(Value* last) {
    while (openUpvalues != nullptr && openUpvalues->location >= last) {
        ObjUpvalue* upvalue = openUpvalues;
        upvalue->closed = *upvalue->location;
        upvalue->location = &upvalue->closed;
        openUpvalues = upvalue->next;
    }
}


/**
 * The actual interpreter loop where it runs 
 * each instruction from its chunk.
 */
void VMCore::Run() {
    std::size_t entryDepth = stack.frameCount - 1;
    CallFrame* frame = stack.GetFrame();
    ObjModule* module = frame->closure->module;

    bool isShortOp = false;

    #define READ_BYTE() (*frame->ip++)
    #define READ_STRING() (READ_CONSTANT().As<ObjString>())
    #define READ_SHORT() ((READ_BYTE() << 8) | READ_BYTE())
    #define READ_CONSTANT() (frame->closure->function->chunk.constants[isShortOp ? READ_SHORT() : READ_BYTE()])

    #define JUMP_FORWARD(bytes) (frame->ip += bytes)
    #define JUMP_BACKWARD(bytes) (frame->ip -= bytes)

    #define BINARY_OP(op)                                       \
    do {                                                        \
        Value a = stack.Peek(1);                                \
        Value b = stack.Peek(0);                                \
                                                                \
        if (a.IsNumber() && b.IsNumber()) {                     \
            stack.Pop(); /* b */                                \
            stack.Pop(); /* a */                                \
                                                                \
            stack.Push(a.AsNumber() op b.AsNumber());           \
            DISPATCH();                                         \
        }                                                       \
    } while (false)


    #define BINARY_DUNDER(name)                                 \
    do {                                                        \
        Value left = stack.Peek(1);                             \
        Value right = stack.Peek(0);                            \
                                                                \
        if (ObjClosure* leftAdd = GetDunder(left, name)) {      \
            stack.Pop(); /* right */                            \
            stack.Pop(); /* left */                             \
                                                                \
            CALL_DUNDER(leftAdd, { left, right }, 0b11);        \
            DISPATCH();                                         \
        }                                                       \
                                                                \
        if (ObjClosure* rightAdd = GetDunder(right, name)) {    \
            stack.Pop(); /* right */                            \
            stack.Pop(); /* left */                             \
                                                                \
            CALL_DUNDER(rightAdd, { right, left }, 0b11);       \
            DISPATCH();                                         \
        }                                                       \
    } while (false)


    #define COMPARISON_OP(op)                                                           \
    do {                                                                                \
        Value b = stack.Pop();                                                          \
        Value a = stack.Pop();                                                          \
                                                                                        \
        if (b.IsNumber() && a.IsNumber()) {                                             \
            stack.Push(a.AsNumber() op b.AsNumber());                                   \
        }                                                                               \
                                                                                        \
        else if (b.Is<ObjString>() && a.Is<ObjString>()) {                              \
            stack.Push(*a.As<ObjString>() op *b.As<ObjString>());                       \
        }                                                                               \
                                                                                        \
        else {                                                                          \
            throw RuntimeError("operands must be two numbers or two strings");          \
        }                                                                               \
    } while (false)

    #define REFRESH_FRAME()                                                             \
    do {                                                                                \
        frame = stack.GetFrame();                                                       \
        module = frame->closure->module;                                                \
    } while (false)

    #define CALL_VALUE(...)                                                             \
    do {                                                                                \
        if (CallValue(__VA_ARGS__)) REFRESH_FRAME();                                    \
    } while (false)  

    #define CALL_DUNDER(...)                                                            \
    do {                                                                                \
        if (CallDunder(__VA_ARGS__)) REFRESH_FRAME();                                   \
    } while (false)


    // Dispatch table shenanigans -------------------
#if LUG_DEBUG_TRACE_EXECUTION
    #define TRACE_EXECUTION()                                                               \
    do {                                                                                    \
        std::printf("          ");                                                          \
        stack.Print();                                                                      \
                                                                                            \
        Debug::DisassembleInstruction(                                                      \
            frame->closure->function->chunk,                                                \
            static_cast<int>((frame->ip - frame->closure->function->chunk.bytecode.Data())) \
        );                                                                                  \
    } while (false)

#else
    #define TRACE_EXECUTION() ((void)0)
#endif

#if LUG_COMPUTED_GOTO
    static void* dispatchTable[] = {
        #define OPCODE(name) &&CASE_##name,
        OPCODE_LIST
        #undef OPCODE
    };

    #define GOTO_INSTRUCTION()                  \
        do {                                    \
            TRACE_EXECUTION();                  \
            goto *dispatchTable[READ_BYTE()];   \
        } while (false)

    #define DISPATCH()                          \
        do {                                    \
            isShortOp = false;                  \
            GOTO_INSTRUCTION();                 \
        } while (false)

    #define CASE(name) CASE_##name
    #define INTERPRET_LOOP DISPATCH();
#else
    #define CASE(name) case OpCode::name
    #define GOTO_INSTRUCTION() goto loop
    #define DISPATCH() do { isShortOp = false; GOTO_INSTRUCTION(); } while (false)

    #define INTERPRET_LOOP                                      \
        loop:                                                   \
            TRACE_EXECUTION();                                  \
            switch (static_cast<OpCode::Type>(READ_BYTE()))     
#endif

    // Dispatch loop!!! -------------------
    INTERPRET_LOOP 
    {
        CASE(CONSTANT): {
            Value constant = READ_CONSTANT();
            stack.Push(constant);
            DISPATCH();
        }

        CASE(SHORT): {
            isShortOp = true;
            GOTO_INSTRUCTION();
        }

        CASE(NEGATE): {
            Value value = stack.Pop();
            if (!value.IsNumber()) {
                throw RuntimeError("operand must be a number");
            }

            stack.Push(-value.AsNumber());
            DISPATCH();
        }

        CASE(NOT): {
            Value value = stack.Pop();
            stack.Push(IsFalsy(value));
            DISPATCH();
        }

        CASE(TRUE): {
            stack.Push(true);
            DISPATCH();
        }

        CASE(FALSE): {
            stack.Push(false);
            DISPATCH();
        }

        CASE(NIL): {
            stack.Push(Value::Nil());
            DISPATCH();
        }

        CASE(ADD): {
            if (stack.Peek(0).IsNumber() && stack.Peek(1).IsNumber()) {
                double b = stack.Pop().AsNumber();
                double a = stack.Pop().AsNumber();

                stack.Push(a + b);

            } else if (stack.Peek(0).Is<ObjString>() && stack.Peek(1).Is<ObjString>()) {
                ObjString* b = stack.Peek(0).As<ObjString>();
                ObjString* a = stack.Peek(1).As<ObjString>();
                ObjString* concatenated = a->Concatenate(b);
                
                stack.Pop();
                stack.Pop();
                stack.Push(concatenated);

            } else {
                BINARY_DUNDER("__add__");

                throw RuntimeError("can't add operands of type '%s' and '%s' together", 
                    stack.Peek(1).StringType(), 
                    stack.Peek(0).StringType()
                );
            }

            DISPATCH();
        }

        CASE(SUBTRACT): {
            BINARY_OP(-);
            BINARY_DUNDER("__subtract__");

            throw RuntimeError(
                "can't add operands of type '%s' and %s' together", 
                stack.Peek(0).StringType(), 
                stack.Peek(1).StringType()
            );
        }
        
        CASE(MULTIPLY): {
            BINARY_OP(*);
            BINARY_DUNDER("__multiply__");

            throw RuntimeError(
                "can't add operands of type '%s' and %s' together", 
                stack.Peek(0).StringType(), 
                stack.Peek(1).StringType()
            );
        }

        CASE(DIVIDE): {
            if (Value right = stack.Peek(0); right.IsNumber() && right.AsNumber() == 0) {
                throw RuntimeError("can't divide by zero");
            }

            BINARY_OP(/);
            BINARY_DUNDER("__divide__");

            throw RuntimeError(
                "can't add operands of type '%s' and %s' together", 
                stack.Peek(0).StringType(), 
                stack.Peek(1).StringType()
            );
        }

        CASE(MODULO): {
            Value bValue = stack.Pop();
            Value aValue = stack.Pop();
            if (!aValue.IsNumber() || !bValue.IsNumber()) {           
                throw RuntimeError("operands must be numbers");
            }    

            double a = aValue.AsNumber();
            double b = bValue.AsNumber();
            stack.Push(a - std::floor(a / b) * b);
            
            DISPATCH();
        }

        CASE(EQUAL): {
            Value b = stack.Pop();
            Value a = stack.Pop();
            stack.Push(a == b);
            DISPATCH();
        }

        CASE(GREATER): {
            COMPARISON_OP(>);
            DISPATCH();
        }
        
        CASE(GREATER_EQUAL): {
            COMPARISON_OP(>=);
            DISPATCH();
        }
        
        CASE(LESS): {
            COMPARISON_OP(<);
            DISPATCH();
        }
        
        CASE(LESS_EQUAL): {
            COMPARISON_OP(<=);
            DISPATCH();
        }

        CASE(JUMP): {
            std::uint16_t numBytesToJump = READ_SHORT();
            JUMP_FORWARD(numBytesToJump);
            DISPATCH();
        }

        CASE(JUMP_IF_FALSE): {
            std::uint16_t numBytesToJump = READ_SHORT();
            if (IsFalsy(stack.Peek(0))) {
                JUMP_FORWARD(numBytesToJump);
            }
            DISPATCH();
        }

        CASE(JUMP_IF_TRUE): {
            std::uint16_t numBytesToJump = READ_SHORT();
            if (IsTruthy(stack.Peek(0))) {
                JUMP_FORWARD(numBytesToJump);
            }
            DISPATCH();
        }

        CASE(JUMP_IF_FALSE_OR_POP): {
            std::uint16_t numBytesToJump = READ_SHORT();
            if (IsFalsy(stack.Peek(0))) {
                JUMP_FORWARD(numBytesToJump);
            } else {
                stack.Pop();
            }
            DISPATCH();
        }

        CASE(JUMP_IF_TRUE_OR_POP): {
            std::uint16_t numBytesToJump = READ_SHORT();
            if (IsTruthy(stack.Peek(0))) {
                JUMP_FORWARD(numBytesToJump);
            } else {
                stack.Pop();
            }
            DISPATCH();
        }

        CASE(POP_JUMP_IF_FALSE): {
            std::uint16_t numBytesToJump = READ_SHORT();
            if (IsFalsy(stack.Pop())) {
                JUMP_FORWARD(numBytesToJump);
            }

            DISPATCH();
        }

        CASE(POP_JUMP_IF_TRUE): {
            std::uint16_t numBytesToJump = READ_SHORT();
            if (IsTruthy(stack.Pop())) {
                JUMP_FORWARD(numBytesToJump);
            }

            DISPATCH();
        }

        CASE(LOOP): {
            std::uint16_t numBytes = READ_SHORT();
            JUMP_BACKWARD(numBytes);
            DISPATCH();
        }

        CASE(COUNT_PREP): {
            std::uint16_t jumpToEnd = READ_SHORT();

            Value start = stack.Peek(2);
            Value stop = stack.Peek(1);
            Value step = stack.Peek(0);

            if (!start.IsNumber()) throw RuntimeError("'count' initial value must be a number");
            if (!stop.IsNumber()) throw RuntimeError("'count' limit value must be a number");
            if (!step.IsNumber()) throw RuntimeError("'count' step value must be a number");
            
            // Check if the loop would even run at all.
            if (step.AsNumber() == 0) {
                throw RuntimeError("'count' step value cannot be zero");
            }
            
            // This handles cases like:
            // count a from 0 to 10 by -1 do ... end
            // count b from 10 to 0 do ... end
            // where the loop would never run because the step is in the wrong direction.
            if (
                step.AsNumber() > 0 ? 
                start.AsNumber() > stop.AsNumber() :
                start.AsNumber() < stop.AsNumber()
            ) {
                JUMP_FORWARD(jumpToEnd);
            }

            DISPATCH();
        }

        CASE(COUNT_LOOP): {
            /* 
                When COUNT_LOOP is reached, the stack will look like this:
                [ 00000 ][ 00005 ][ 00001 ]
                ^^^^^    ^^^^^    ^^^^^    ^^^^^
                start    limit     step    stackTop

                ...where stack.Peek(0) = step, stack.Peek(1) = limit, and stack.Peek(2) = start.
            */

            std::uint16_t jumpToStart = READ_SHORT();

            Value& start = stack.Peek(2);
            Value& limit = stack.Peek(1);
            Value& step = stack.Peek(0);

            // COUNT_PREP should've already checked if these were numbers or not.
            // If we still have them as not a number, then we should assert.
            LUG_ASSERT(start.IsNumber(), "invalid start");
            LUG_ASSERT(limit.IsNumber(), "invalid limit");
            LUG_ASSERT(step.IsNumber(), "invalid step");   

            // Increment index.
            start = start.AsNumber() + step.AsNumber();
            if (step.AsNumber() > 0 ? 
                start.AsNumber() <= limit.AsNumber() :
                start.AsNumber() >= limit.AsNumber()
            ) {
                // Repeat loop
                JUMP_BACKWARD(jumpToStart);
            }

            DISPATCH();
        }

        CASE(FOREACH_PREP): {
            /*
                When FOREACH_PREP is reached, the stack may look like this:
                [  nil  ][  nil  ][  nil  ][   [1, 2, 3]   ]
                ^^^^^^^  ^^^^^^^  ^^^^^^^     ^^^^^^^^^     ^^^^^
                probe idx  value  key/index    iterable     stackTop   

                ...where the probing index, key, and value values are nil.
            */

            std::uint16_t jumpToEnd = READ_SHORT();

            Value& value = stack.Peek(2);
            Value& key = stack.Peek(1);
            Value& iterable = stack.Peek(0);
            
            if (iterable.Is<ObjList>()) {
                ObjList* list = iterable.As<ObjList>();
                if (!key.IsNumber()) key = 0;

                // Check if the index is the same or higher as the count of the list
                if (static_cast<std::size_t>(key.AsInteger()) >= list->Count()) {
                    JUMP_FORWARD(jumpToEnd);
                    DISPATCH();
                }

                // list->Get may return nullptr if the index is out of range.
                const Value* listValue = list->Get(key.AsNumber());
                LUG_ASSERT(listValue != nullptr, "the foreach index is out of range");
                value = *listValue;
            } 
            
            else if (iterable.Is<ObjDict>()) {
                ObjDict* dict = iterable.As<ObjDict>();
                Value& probingIndex = stack.Peek(3);
                if (!probingIndex.IsNumber()) probingIndex = 0;

                std::size_t index = static_cast<std::size_t>(probingIndex.AsInteger());
                while (
                    index < dict->entries.capacity 
                    && dict->entries.entries[index].IsEmpty()
                ) index++;

                if (index == dict->entries.capacity) {
                    JUMP_FORWARD(jumpToEnd);
                }

                Entry& currentEntry = dict->entries.entries[index];
                key = currentEntry.key;
                value = currentEntry.value;
                probingIndex = static_cast<int>(index);
            } 
            
            else {
                throw RuntimeError("'foreach' must iterate over an iterable type, got '%s' instead", iterable.StringType());
            }

            DISPATCH();
        }

        CASE(FOREACH_LOOP): {
            /*
                When FOREACH_PREP is reached, the stack may look like this:
                [  nil  ][   1   ][   0   ][   [1, 2, 3]   ]
                ^^^^^^^  ^^^^^^^  ^^^^^^^     ^^^^^^^^^     ^^^^^
                probe idx  value  key/index    iterable     stackTop   
            */

            std::uint16_t jumpToStart = READ_SHORT();

            Value& value = stack.Peek(2);
            Value& key = stack.Peek(1);
            Value& iterable = stack.Peek(0);

            if (iterable.Is<ObjList>()) {
                ObjList* list = iterable.As<ObjList>();
                key = key.AsInteger() + 1;

                // Check if we exceeded the index count. If so, that means
                // we're done with iterating through the list. Exit out of
                // the loop.
                if (key.AsInteger() < list->Count()) {
                    // In case we screwed something up by letting the index go out of
                    // range of the list, causing list->Get to return nullptr, we assert
                    const Value* listValue = list->Get(key.AsNumber());
                    LUG_ASSERT(listValue != nullptr, "the foreach index is out of range");
                    value = *listValue;

                    JUMP_BACKWARD(jumpToStart);
                }
            } 

            else if (iterable.Is<ObjDict>()) {
                ObjDict* dict = iterable.As<ObjDict>();
                Value& probingIndex = stack.Peek(3);
                LUG_ASSERT(probingIndex.IsInteger(), "invalid type for probing index");

                std::size_t index = static_cast<std::size_t>(probingIndex.AsInteger());
                do index++; while (
                    index < dict->entries.capacity 
                    && dict->entries.entries[index].IsEmpty()
                );

                if (index < dict->entries.capacity) {
                    Entry& currentEntry = dict->entries.entries[index];
                    key = currentEntry.key;
                    value = currentEntry.value;
                    probingIndex = static_cast<int>(index);
                    JUMP_BACKWARD(jumpToStart);
                }
            }
            
            else {
                LUG_ASSERT(false, "invalid iterable");
            }

            DISPATCH();
        }

        CASE(PLACEHOLDER): {
            LUG_ASSERT(false, "placeholder found during runtime");
        }
        
        CASE(POP): {
            stack.Pop();
            DISPATCH();
        }

        CASE(DUPLICATE): {
            stack.Push(stack.Peek(0));
            DISPATCH();
        }

        CASE(COPY): {
            Value value = stack.Peek(0);
            if (value.IsObj()) {
                Value copy = value.AsObj()->Copy();
                stack.Pop();
                stack.Push(copy);
            };


            DISPATCH();
        }

        CASE(DEFINE_GLOBAL): {
            /*
                The operand of DEFINE_GLOBAL is a
                string constant, or the name of the
                global. The value of the global
                is the value on top of the stack.
            */
            ObjString* string = READ_STRING();
            module->globals.Set(string, stack.Peek(0));
            stack.Pop();

            DISPATCH();
        }

        CASE(GET_GLOBAL): {
            ObjString* name = READ_STRING();
            Value* value = module->globals.Get(name);

            // If we can't find it in the module's globals, try getting it from
            // the builtins.
            if (value == nullptr) value = builtins.Get(name);
            if (value == nullptr) {
                throw RuntimeError("undefined variable '%s'", name->characters);
            }

            stack.Push(*value);
            DISPATCH();
        }
        
        CASE(SET_GLOBAL): {
            ObjString* name = READ_STRING();

            // We assign the name to the value at the top of the stack.
            Value& value = stack.Peek(0);
            bool isNewKey = module->globals.Set(name, value);
            
            if (isNewKey) {
                module->globals.Delete(name);

                // Check if it exists in builtins
                bool isNewKey = builtins.Set(name, value);
                if (isNewKey) {
                    builtins.Delete(name);
                    throw RuntimeError("undefined variable '%s'", name->characters);
                }
            }

            DISPATCH();
        }

        CASE(GET_LOCAL): {
            std::uint8_t slot = READ_BYTE();
            stack.Push(frame->slots[slot]);

            DISPATCH();
        }

        CASE(SET_LOCAL): {
            std::uint8_t slot = READ_BYTE();
            frame->slots[slot] = stack.Peek(0);
            
            DISPATCH();
        }

        CASE(GET_UPVALUE): {
            std::uint8_t slot = READ_BYTE();
            stack.Push(*frame->closure->upvalues[slot]->location);

            DISPATCH();
        }

        CASE(SET_UPVALUE): {
            std::uint8_t slot = READ_BYTE();
            *frame->closure->upvalues[slot]->location = stack.Peek(0);

            DISPATCH();
        }

        CASE(CLOSE_UPVALUE): {
            CloseUpvalues(stack.top - 1);
            stack.Pop();

            DISPATCH();
        }

        CASE(CALL): {
            std::uint8_t argCount = READ_BYTE();
            
            // Attempt to call the callee, whether it is of any value type.
            CALL_VALUE(stack.Peek(argCount), argCount);
            
            DISPATCH();
        }
        
        CASE(CALL_SHARED): {
            std::uint8_t argCount = READ_BYTE();
            std::uint8_t shared = READ_BYTE();

            CALL_VALUE(stack.Peek(argCount), argCount, shared);
            
            DISPATCH();
        }

        CASE(SELF): {
            ObjString* methodName = READ_STRING();
            Value object = stack.Peek(0);
            ObjThing* thing = nullptr;

            // If the object is of type 'thing', then 'thing' is
            // the object.
            if (object.Is<ObjThing>()) {
                thing = object.As<ObjThing>();

            // If it's not a 'thing', we'll see if the object has a
            // proto.
            } else if (GetProto(object) != nullptr) {
                thing = GetProto(object);
            }

            // If we still haven't gotten the 'thing' yet, we throw an error.
            if (thing == nullptr || thing->Get(methodName) == nullptr) {
                throw RuntimeError(
                    "method '%s' does not exist in value of type '%s'", 
                    methodName->characters, 
                    object.StringType()
                );
            }
            
            const Value* value = thing->Get(methodName);
            
            stack.Pop(); // object
            stack.Push(*value);
            stack.Push(object);

            DISPATCH();
        }

        CASE(CLOSURE): {
            ObjFunction* function = READ_CONSTANT().As<ObjFunction>();
            ObjClosure* closure = ObjClosure::New(*this, function, module);
            stack.Push(closure);

            for (std::size_t i = 0; i < closure->upvalueCount; i++) {
                std::uint8_t isLocal = READ_BYTE();
                std::uint8_t index = READ_BYTE();
                
                // When we emitted OP_CLOSURE, we were done parsing
                // a function declaration. Now, the current function is
                // the SURROUNDING function of that declaration, so we can
                // read the upvalue right from the current call frame to grab
                // it from the enclosing function.
                if (isLocal) {
                    closure->upvalues[i] = CaptureUpvalue(frame->slots + index);
                } else {
                    closure->upvalues[i] = frame->closure->upvalues[index];
                }
            }

            DISPATCH();
        }
        
        CASE(LIST): {
            ObjList* newList = ObjList::New(*this);
            stack.Push(newList);

            DISPATCH();
        }

        CASE(DICT): {
            ObjDict* newDict = ObjDict::New(*this);
            stack.Push(newDict);

            DISPATCH();
        }

        CASE(THING): {
            ObjThing* newThing = ObjThing::New(*this);
            stack.Push(newThing);

            DISPATCH();
        }

        CASE(CATEGORY): {
            ObjCategory* newCategory = ObjCategory::New(*this);
            stack.Push(newCategory);

            DISPATCH();
        }

        CASE(APPEND): {
            ObjList* list = stack.Peek(1).As<ObjList>();

            // Keep the value on the stack until after we append
            // to prevent the GC from collecting it in the case that
            // 'append' triggers a collection
            Value value = stack.Peek(0);
            list->Append(value);
            stack.Pop();

            DISPATCH();
        }

        CASE(DEFINE_ENTRY): {
            ObjDict* dictionary = stack.Peek(2).As<ObjDict>();
            const Value& key = stack.Peek(1);
            const Value& value = stack.Peek(0);

            dictionary->Set(key, value);

            stack.Pop();
            stack.Pop();

            DISPATCH();
        }

        CASE(GET_INDEX): {
            //    stack.Peek(1)     stack.Peek(0)
            // [ collection ][ index ]
            //                 ^^^^^ stackTop. You are here.
            Value& collection = stack.Peek(1);
            
            if (collection.Is<ObjList>()) {
                if (!stack.Peek(0).IsInteger()) throw RuntimeError("list index must be an integer");
                
                ObjList* list = collection.As<ObjList>();

                int index = stack.Peek(0).AsInteger();
                const Value* value = list->Get(index);
                if (value == nullptr) throw RuntimeError("index '%d' out of range", index);

                stack.Pop(); // Index
                stack.Pop(); // Collection

                // Leave only the value of the indexed list.
                stack.Push(*value);

                DISPATCH();
            }

            if (collection.Is<ObjDict>()) {
                ObjDict* dict = collection.As<ObjDict>();

                Value& key = stack.Peek(0);
                const Value* value = dict->Get(key);
                if (value == nullptr) {
                    throw RuntimeError("could not find key in dictionary");
                }

                stack.Pop(); // Key
                stack.Pop(); // Collection
                stack.Push(*value);

                DISPATCH();
            }

            if (collection.Is<ObjString>()) {
                if (!stack.Peek(0).IsInteger()) throw RuntimeError("string index must be an integer");
                ObjString* string = collection.As<ObjString>();
                int index = stack.Pop().AsInteger();

                stack.Pop(); // collection
                ObjString* character = ObjString::FromSource(*this, string->CharAt(index), 1);
                stack.Push(character);
                
                DISPATCH();
            }

            if (collection.Is<ObjThing>()) {
                if (ObjClosure* indexDunder = GetDunder(collection, "__index__")) {
                    Value index = stack.Pop(); // Index
                    stack.Pop(); // Collection

                    CALL_DUNDER(indexDunder, { collection, index }, 0b1);

                    DISPATCH();
                }
            }

            throw RuntimeError("value of type '%s' is not indexable", collection.StringType());
        }
        
        CASE(SET_INDEX): {
            //     stack.Peek(2)    stack.Peek(1)    stack.Peek(0)
            // [ collection ][ index ][ newValue ]
            //                                    ^^^^^^ stackTop. You are here.
            Value& collection = stack.Peek(2);
            if (!collection.Is<ObjList>() && !collection.Is<ObjDict>()) {
                throw RuntimeError("value of type '%s' is not indexable", stack.Peek(2).StringType());
            }

            Value newValue = stack.Peek(0);

            switch (collection.AsObj()->Type()) {
                case ObjType::LIST: {
                    // Lists must always have integers as indices.
                    if (!stack.Peek(1).IsInteger()) throw RuntimeError("list index must be an integer");
                    
                    ObjList* list = collection.As<ObjList>();
                    int index = stack.Peek(1).AsInteger();
                    list->Set(index, newValue); 

                    break;
                }

                case ObjType::DICT: {
                    ObjDict* dictionary = collection.As<ObjDict>();

                    // Dictionaries have Lua semantics, where setting a key to nil
                    // removes the entry entirely.
                    // (i.e. a["hello"] <- nil removes the "hello" key as well, and
                    // won't be counted in loops.)

                    Value key = stack.Peek(1);
                    if (newValue.IsNil()) {
                        dictionary->Delete(key);
                    } else {
                        dictionary->Set(key, newValue);
                    }

                    break;
                }

                default: break;
            }

            stack.Pop(); // New value
            stack.Pop(); // Index
            stack.Pop(); // Collection

            stack.Push(newValue);

            DISPATCH();
        }

        CASE(GET_ATTRIBUTE): {
            ObjString* attributeName = READ_STRING();
            std::uint16_t cacheId = READ_SHORT();
            AttributeCache& cache = frame->closure->function->chunk.attributeCaches[cacheId];

            const Value& receiver = stack.Peek(0);
            if (receiver.Is<Obj>() && receiver.As<Obj>() == cache.receiver && globalEpoch == cache.cachedEpoch) {
                stack.Pop(); // Receiver
                stack.Push(*cache.value);
                DISPATCH();
            } 
            
            if (stack.Peek(0).Is<ObjCategory>()) {
                ObjCategory* category = stack.Peek(0).As<ObjCategory>();
                const Value* type = category->GetType(attributeName);
                if (type == nullptr) {
                    throw RuntimeError("type '%s' does not exist in category", attributeName->characters);
                }
                
                stack.Pop(); // Category
                stack.Push(*type);

                cache = { category, globalEpoch, type };
                DISPATCH();
            }
            
            if (stack.Peek(0).Is<ObjThing>()) {
                ObjThing* thing = stack.Peek(0).As<ObjThing>();
                const Value* value = thing->Get(attributeName);

                if (value == nullptr) {
                    throw RuntimeError("thing has no attribute named '%s'", attributeName->characters);
                }
                
                stack.Pop(); // Thing
                stack.Push(*value);

                cache = { thing, globalEpoch, value };
                
                DISPATCH();
            }

            ObjThing* proto = GetProto(stack.Peek(0));
            if (proto) {
                const Value* value = proto->Get(attributeName);
                if (value == nullptr) {
                    throw RuntimeError("could not find '%s' in value", attributeName->characters);
                }

                stack.Pop();
                stack.Push(*value);

                DISPATCH();
            }
            
            throw RuntimeError(
                "type '%s' does not support attributes", 
                stack.Peek(0).StringType()
            );
        }

        CASE(SET_ATTRIBUTE): {
            // [ <thing> ][ 5 ]
            //   ^^^^^^^   ^^^   ^^^^^^
            //   object   value  stackTop
            //   stack.Peek(1)  stack.Peek(0)

            ObjString* attributeName = READ_STRING();
            Value object = stack.Peek(1);
            Value value = stack.Peek(0);

            if (object.Is<ObjThing>()) {
                ObjThing* thing = object.As<ObjThing>();
                thing->Set(attributeName, value);

                stack.Pop();
                stack.Pop();
                stack.Push(value); // Push the value back since assignments leave the value on the stack.
                
                DISPATCH();
            }

            if (object.Is<ObjCategory>()) {
                ObjCategory* category = object.As<ObjCategory>();

                // Note that we cannot add new kinds after the category literal.
                // category->AddType will error for us when we try and re-define
                // an attribute that has already been defined.
                category->AddType(attributeName, value);
                stack.Pop();

                DISPATCH();
            }

            throw RuntimeError(
                "attribute '%s' does not exist in value of type '%s'", 
                attributeName->characters, 
                stack.Peek(0).StringType()
            );
        }

        CASE(SLICE): {
            // [ object ][ start ][ end ]
            //     2         1       0

            if (!stack.Peek(1).IsInteger()) throw RuntimeError("slice start value must be an integer");
            if (!stack.Peek(0).IsInteger()) throw RuntimeError("slice end value must be an integer");

            int end = stack.Pop().AsInteger();
            int start = stack.Pop().AsInteger();

            Value object = stack.Pop();

            if (object.Is<ObjList>()) {
                stack.Push(object.As<ObjList>()->Slice(start, end));
                DISPATCH();
            }

            if (object.Is<ObjString>()) {
                ObjString* string = object.As<ObjString>();
                stack.Push(string->Sub(start, end));

                DISPATCH();
            }

            if (object.Is<ObjThing>()) {
                ObjThing* thing = object.As<ObjThing>();

                const Value* sliceDunder = thing->Get("__slice__");
                if (sliceDunder != nullptr) {
                    // [ __slice__ ][ object ][ start ][ end ]
                    //   ^^^^^^^^^    ^^^^^^^^^^^^^^^^^^^^^^
                    //    dunder              args (x3)

                    CALL_DUNDER(*sliceDunder, { 
                        object /* shared */, 
                        start, 
                        end 
                    }, 0b001);

                    DISPATCH();
                }

            }

            throw RuntimeError("value of type '%s' is not sliceable", object.StringType());
        }

        CASE(RETURN): {
            Value result = stack.Pop();
            CloseUpvalues(frame->slots);

            stack.RemoveFrame();
            stack.Push(result);

            if (stack.frameCount == entryDepth) return;

            frame = stack.GetFrame();
            module = frame->closure->module;

            DISPATCH();
        }
    }

    LUG_ASSERT(false, "Unknown opcode.");

    #undef READ_BYTE
    #undef READ_STRING
    #undef READ_SHORT
    #undef READ_CONSTANT

    #undef JUMP_FORWARD
    #undef JUMP_BACKWARD

    #undef REFRESH_FRAME
    #undef CALL_VALUE

    #undef BINARY_OP
}

}