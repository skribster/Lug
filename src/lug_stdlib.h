#pragma once

#include "lug_common.h"
#include "lug_vmcore.h"
#include "lug_object.h"

namespace Lug::Lib { 

struct NativeEntry {
    const char* name;
    NativeFn function;
};


ObjThing* NewLib(VMCore& vm, const char* libraryName);

/**
 * Helper function for defining all the native functions of
 * a given library and adding them inside the VM's environment.
 * 
 * If the given library name isn't specified (nullptr), all of the
 * entries will be defined in the global scope. Otherwise, all entries
 * will be stuffed into one `thing` global.
 */
template<std::size_t N>
void DefineLib(VMCore& vm, ObjThing* library, const NativeEntry (&entries)[N]) {
    if (library == nullptr) {
        for (auto& entry : entries) {
            vm.DefineNative(entry.name, entry.function);
        }

        return;
    }

    vm.stack.Push(library);

    for (const NativeEntry& entry : entries) {
        ObjString* entryName = ObjString::FromSource(vm, entry.name, std::strlen(entry.name));
        vm.stack.Push(entryName);
        ObjNative* entryFunction = ObjNative::New(vm, entry.name, entry.function);
        vm.stack.Push(entryFunction);

        library->Set(entryName, entryFunction);

        vm.stack.Pop();
        vm.stack.Pop();
    }

    vm.stack.Pop(); // Library
}

// Library open functions -------------------------

/*
    All standard libraries will have a function defined here
    that gets implemented in their respective .cpp implementation. 
    (e.g. OpenBase is defined in lug_baselib.cpp.)
    
    All of these functions eventually gets called in Lug::VM 
    where it loads a certain library by using a function
    from here.

    For example, if I wanted to load the math library in VM,
    I'd call OpenMath(vm).
*/

extern void OpenBase(VMCore& vm);
extern void OpenMath(VMCore& vm);
extern void OpenString(VMCore& vm);
extern void OpenNumber(VMCore& vm);
extern void OpenCollection(VMCore& vm);

}