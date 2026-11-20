#include <filesystem>
#include <string_view>

#include "lug_vm.hpp"
#include "lug_filesys.h"
#include "lug_vmcore.h"
#include "lug_stdlib.h"

namespace Lug {

namespace fs = std::filesystem;

VM::VM(const VMConfig& config) 
: core(std::make_unique<VMCore>(config)) 
, config(config)
{
    
}


VM::~VM() = default;


// Execute functions ----------------------------

InterpretResult VM::InterpretFile(const fs::path& filePath) {
    auto resolved = Filesystem::ResolveModulePath("", filePath);
    if (!resolved) {
        LugError fileError;
        fileError.message = core->ErrorMessage("file", "could not find script '%s'", filePath.string().c_str());
        fileError.type = InterpretResult::FILE_ERROR;
        core->PushError(fileError);

        return InterpretResult::FILE_ERROR;
    }

    auto source = Filesystem::ReadModule(filePath);
    if (!source) {
        LugError fileError;
        fileError.message = core->ErrorMessage("file", "could not load file '%s'", filePath.string().c_str());
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


const LugError& VM::LastError() const {
    return core->lastError;
}


void VM::LoadLibrary(LibInclude libraries) {
    if (libraries & LibInclude::NONE)           return;
    if (libraries & LibInclude::BASE)           Lib::OpenBase(*core);
    if (libraries & LibInclude::MATH)           Lib::OpenMath(*core);
    if (libraries & LibInclude::STRING)         Lib::OpenString(*core);
    if (libraries & LibInclude::COLLECTION)     Lib::OpenCollection(*core);
    if (libraries & LibInclude::NUMBER)         Lib::OpenNumber(*core);
}

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

// Getters and Setters --------------------------

Variant VM::Get(std::string_view name) const {
    return core->GetGlobal(name);
}

void VM::Set(std::string_view name, const Variant& value) {
    core->SetGlobal(name, value);
}


} // Lug