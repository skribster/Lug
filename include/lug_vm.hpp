#pragma once

#include <memory>
#include <filesystem>
#include <functional>
#include <optional>
#include <vector>

#include <string_view>
#include <string>

#include "lug_variant.hpp"

namespace Lug {

enum class InterpretResult {
    OK,
    COMPILE_ERROR,
    RUNTIME_ERROR,
    MEMORY_ERROR,
    FILE_ERROR,
};


enum class LibInclude : unsigned {
    NONE            = 0,
    BASE            = 1 << 0,   // 0001
    MATH            = 1 << 1,   // 0010
    STRING          = 1 << 2,   // 0100...
    COLLECTION      = 1 << 3,
    NUMBER          = 1 << 4,
    ALL             = BASE | MATH | STRING | COLLECTION | NUMBER
};


constexpr LibInclude operator|(LibInclude a, LibInclude b) {
    return static_cast<LibInclude>(
        static_cast<unsigned int>(a) | static_cast<unsigned int>(b)
    );
}


constexpr bool operator&(LibInclude a, LibInclude b) {
    return static_cast<unsigned int>(a) & static_cast<unsigned int>(b);
}  


struct SourceLocation {
    int line;
    int column;
    int tokenLength;
    std::string file;
};


struct StackFrame {
    SourceLocation location;
    std::string function;   // (add(), script, <anonymous>, etc.)
};


struct Error {
    // The type of error.
    InterpretResult type;
    
    // The primary location where the error happened.
    SourceLocation location;

    // The error message.
    std::string message;

    // The string token where the error happened. This member is only for compile errors. 
    std::string token;

    // The full line from the source that the error occurred on. This member is only for compile errors.
    std::string sourceLine;

    // The stack traceback of frames leading up to the error. This member is only for runtime errors.
    std::vector<StackFrame> stackTraceback;

    /**
     * Returns a formatted user-friendly representation of a Lug::Error with 
     * all the necessary information needed.
     */
    std::string Format() const;
};


/**
 * Not to be used directly. This class is the
 * internal implementation to the external VM
 * class.
 */
class VMCore;

// VM -------------------------------------------

struct Handle;

std::optional<std::filesystem::path> DefaultResolveModule(
    const std::filesystem::path& importerPath, 
    const std::filesystem::path& requestedPath
);

std::optional<std::string> DefaultFileLoader(const std::filesystem::path& filePath);
void DefaultWriter(std::string_view string);

/**
 * The configuration for a single Lug VM.
 */
struct VMConfig {
    using ResolveModuleFn = std::function<std::optional<std::filesystem::path>
        (const std::filesystem::path& importerPath, const std::filesystem::path& requestedPath)>;

    using LoadModuleFn = std::function<std::optional<std::string>(const std::filesystem::path& filePath)>;
    using WriteFn = std::function<void(std::string_view s)>;
    using ErrorFn = std::function<void(const Error& error)>;

    /**
     * A function to resolve a module's relative path into an absolute path.
     */
    ResolveModuleFn resolveModuleFn { DefaultResolveModule };

    /**
     * A function to load a .lug module and return back its contents.
     */
    LoadModuleFn loadModuleFn { DefaultFileLoader };
    
    /**
     * A function for when the program wants to write something to
     * a console.
     */
    WriteFn writeFn { DefaultWriter };

    /**
     * A function that runs immediately after an error.
     */
    ErrorFn errorFn { [](const Error& error) {} };
};

/**
 * A virtual machine for executing Lug bytecode. 
 * It manages the execution state, including the stack, 
 * instruction pointer, and other resources needed to run
 * Lug programs.
 */
class VM {
public:
    explicit VM(const VMConfig& config = {});
    ~VM();

    // VM is not copyable
    VM(const VM&) = delete;
    VM& operator=(const VM&) = delete;

    /**
     * Loads the given library into the VM's environment.
     * This takes in an enum of LibInclude, which you can also
     * use bitmasking or the | opreator to include multiple 
     * libraries. For instance, 
     * LoadLibrary(LibInclude::BASE | LibInclude::MATH) will
     * include both the base and the math library.
     */
    void LoadLibrary(LibInclude libraries);

    /**
     * Collects the VM's garbage immediately.
     */
    void CollectGarbage();


    // Execute functions ------------------------

    /** 
     * Takes in a Lug program and interprets it. Returns the result
     * of the program.
     */
    [[nodiscard]] InterpretResult InterpretFile(const std::filesystem::path& module);

    /**
     * Takes in a string of Lug code and interprets it. Returns the
     * result of the program. You may also define the name of the top 
     * script as the second parameter.
     */
    [[nodiscard]] InterpretResult Interpret(std::string_view source, std::string_view scriptName = "script");

    // Value functions --------------------------

    Variant Get(std::string_view name) const;
    void Set(std::string_view name, const Variant& value);

    /**
     * Tries to fetch a global VM variable from the given name.
     */
    template<typename T>
    std::optional<T> Get(std::string_view name) const {
        return Get(name).As<T>();
    };

    Field<VM, std::string> operator[](std::string_view name);
    Variant operator[](std::string_view name) const;

    // Value factory functions ------------------

    Lug::Thing NewThing();
    Lug::List NewList(std::initializer_list<Variant> initializer = {});
    Lug::Dict NewDict();

    // Error functions --------------------------

    /**
     * Returns the last error of the program.
     * Consider using VMConfig's 'errorFn' to define a callback
     * for when the program errors.
     */
    const Error& LastError() const;

    /**
     * Returns a stack traceback, or the stack of call frames 
     * leading to where the VM currently is in the script.
     * 
     * If the VM isn't running when you call this function, it
     * will simply return an empty vector.
     */
    std::vector<StackFrame> StackTraceback() const;

private:
    std::unique_ptr<VMCore> core;
    VMConfig config;

};

// Template definitions -------------------------

} // Lug