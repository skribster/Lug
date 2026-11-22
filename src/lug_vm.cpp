#include <filesystem>
#include <string_view>
#include <sstream>

#include "lug_vm.hpp"
#include "lug_filesys.h"
#include "lug_vmcore.h"
#include "lug_stdlib.h"

namespace Lug {

namespace fs = std::filesystem;

// Config ---------------------------------------

std::optional<fs::path> DefaultResolveModule(
    const fs::path& importerPath, 
    const fs::path& requestedPath
) {
    return Filesystem::ResolveModulePath(importerPath, requestedPath);
}


std::optional<std::string> DefaultFileLoader(const fs::path& filePath) {
    return Filesystem::ReadModule(filePath);
}


void DefaultWriter(std::string_view string) {
    std::fwrite(string.data(), sizeof(char), string.size(), stdout);
}

// Error ----------------------------------------

std::string Error::Format() const {
    std::ostringstream out;

    if (type == InterpretResult::COMPILE_ERROR) {
        out << "error";
        if (!token.empty()) out << " at '" << token << "'";
        out << ": " << message << "\n";

        std::string lineNo = std::to_string(location.line);
        std::string lineNumPadding(lineNo.size(), ' ');

        out << lineNumPadding << "--> " << location.file << ":" << location.line << ":" << location.column << "\n";
        out << lineNumPadding       << " |\n"
            << lineNo               << " | " << sourceLine << "\n"
            << lineNumPadding       << " | ";

        out << std::string(std::max(location.column - 1, 0), ' ')
            << std::string(std::max(location.tokenLength, 1), '^');
    }

    else if (type == InterpretResult::RUNTIME_ERROR) {
        out << "runtime error: " << message << "\nstack traceback:\n";
        for (auto& frame : stackTraceback) {
            const SourceLocation& location = frame.location;
            out << "    " << location.file << ":" << location.line << ":" << location.column
                << ": in " << frame.function;

            // Don't insert newline for the last message
            if (&frame != &stackTraceback.back()) {
                out << "\n";
            }
        }
    }
    return out.str();
}

// VM -------------------------------------------

VM::VM(const VMConfig& config) 
: core(std::make_unique<VMCore>(config)) 
, config(config)
{
    
}


VM::~VM() = default;


void VM::LoadLibrary(LibInclude libraries) {
    if (libraries & LibInclude::NONE)           return;
    if (libraries & LibInclude::BASE)           Lib::OpenBase(*core);
    if (libraries & LibInclude::MATH)           Lib::OpenMath(*core);
    if (libraries & LibInclude::STRING)         Lib::OpenString(*core);
    if (libraries & LibInclude::COLLECTION)     Lib::OpenCollection(*core);
    if (libraries & LibInclude::NUMBER)         Lib::OpenNumber(*core);
}


// Execute functions ----------------------------

InterpretResult VM::InterpretFile(const fs::path& filePath) {
    auto resolved = Filesystem::ResolveModulePath("", filePath);
    if (!resolved) {
        Lug::Error fileError;
        fileError.message = core->FormatString("file", "could not find script '%s'", filePath.string().c_str());
        fileError.type = InterpretResult::FILE_ERROR;
        core->PushError(fileError);

        return InterpretResult::FILE_ERROR;
    }

    auto source = Filesystem::ReadModule(filePath);
    if (!source) {
        Lug::Error fileError;
        fileError.message = core->FormatString("could not load file '%s'", filePath.string().c_str());
        fileError.type = InterpretResult::FILE_ERROR;
        core->PushError(fileError);

        return InterpretResult::FILE_ERROR;
    }

    return core->Interpret(*source, filePath);
}


InterpretResult VM::Interpret(std::string_view source, std::string_view scriptName) {
    return core->Interpret(source, scriptName);
}


void VM::CollectGarbage() { 
    core->CollectGarbage(); 
}


// Error functions ------------------------------

const Error& VM::LastError() const {
    return core->lastError;
}


std::vector<StackFrame> VM::StackTraceback() const {
    return core->StackTraceback();
}


// Value functions --------------------------

Variant VM::Get(std::string_view name) const {
    return core->GetGlobal(name);
}


void VM::Set(std::string_view name, const Variant& value) {
    core->SetGlobal(name, value);
}


Field<VM, std::string> VM::operator[](std::string_view name) {
    return { this, std::string(name) };
}


Variant VM::operator[](std::string_view name) const {
    return Get(name);
}


// Value functions ------------------------------

Thing VM::NewThing() {
    ObjThing* thing = ObjThing::New(*core);
    return Thing(core->MakeSharedHandle(thing));
}


List VM::NewList(std::initializer_list<Variant> initializer) {
    ObjList* list = ObjList::New(*core);
    core->stack.Push(list);
    
    for (const Variant& item : initializer) {
        Value value = core->ToValue(item);
        
        core->stack.Push(value);   
        list->Append(value);
        core->stack.Pop(); // value
    }
    
    core->stack.Pop(); // list 
    return List(core->MakeSharedHandle(list));
}


Dict VM::NewDict() {
    ObjDict* dict = ObjDict::New(*core);
    return Dict(core->MakeSharedHandle(dict));
}

} // Lug