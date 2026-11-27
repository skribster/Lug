---
title: Embedding
layout: guide.njk
tags: embedding
---

Lug is designed to be a language that you can easily embed in other modern C++ projects, 
compatible with **C++17 and up**. Lug's C++ API was also designed to be easy to work with. 
If you are familiar with the [Lua wrapper sol2](https://github.com/thephd/sol2), 
the shape of Lug's API is very reminiscent of that. 

## Including Lug

Firstly, grab yourself a stable release from the repository. These binaries are static libraries that you
can link to your C++ project. 

Install the static library that is compatible with your C++ compiler. For instance, if you're compiling
your code with GCC/G++, install the binary for *MinGW*. For Clang, install... well... the *Clang* version.
Simple enough?

<div style="display: flex; flex-direction: column; padding: 1rem; gap: 0.5rem;">
    <a class="button button-prominent" href="https://github.com/skribster/Lug/" rel="noopener noreferrer" target="_blank">Source</a>
    <a class="button button-prominent" href="https://github.com/skribster/Lug/releases" rel="noopener noreferrer" target="_blank">Releases</a>
</div>

### CMake Approach

Fortunately for us poor souls that still suffer through CMake, Lug is very much compatible
with your CMake project! When you drag the binary folder over to your project, adding Lug is as simple
as adding **two lines** to your top-level CMakeLists.txt.

Say we have this basic CMakeLists.txt:

```
cmake_minimum_required(VERSION 3.23)
project(Project VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED 17)

add_executable(Project main.cpp)
```

We'll simply find the Lug package and link the library over to our project.
Add these two lines at the end:
```
find_package(lug CONFIG REQUIRED)
target_link_libraries(Project PRIVATE lug::lug_core)
```

Finally, when we're creating our build folder for the first time, we'll pass in 
the location of our library folder as a flag.
```
cmake -S . -B build -DCMAKE_PREFIX_PATH="path/to/Lug/binary"
```

The prefix path can also be a generic vendor/ directory that contains your Lug folder, 
which I prefer when structuring projects with multiple libraries.

### Other Approaches

For other systems, you're gonna have to follow whatever method you use to link static libraries.
Each binary comes supplied with three directories: 
- bin/
- include/
- lib/

The lib/ directory has the actual static library (.lib) file. The include/ directory has all the
public headeres for the Lug C++ API. For IDEs like Visual Studio, it's as
simple as adding the static library to your project's additional libraries, and adding
the includes to your additional includes. 

There's also the option to directly compile the source with your existing project. You can download 
the source from the Lug [GitHub repository](https://github.com/skribster/Lug), and compile everything 
with your project. 

All the implementation details for the language live in the src/ folder, and your public
headers are in the include/ folder. Lug has no third party dependencies, so you don't need to 
worry about any extra steps other than that.

## Creating a VM
The first step in incorporating Lug into your project is to include the public header.
```
#include <lug.hpp>
```

Let's now create a Lug virtual machine. Somewhere in your code, create a Lug::VM. 
```
Lug::VM vm;
```

What's best is that Lug::VM follows C++ RAII, and so you don't have to do any kind of manual
cleanup after you're done like how you would have to do for C-based libraries.

Our new VM is completely empty, and not even the standard libraries are loaded in yet. How can
we load them in? Lug::VM has a member function called LoadLibraries which takes in a Lug::LibInclude
enum. 

```
vm.LoadLibraries(Lug::LibInclude::ALL);
```
LibInclude specifies the libraries we want to include in our VM. For this case, we're saying that we
want to include all the built-in standard libraries. 

LibInclude can also be combined with the '|' operator to act as kind of a whitelist of libraries. 
In this example, we'll load in only three libraries and ignore the rest.

```
vm.LoadLibraries(Lug::LibInclude::BASE | Lug::LibInclude::MATH | Lug::LibInclude::COLLECTION);
```

Now you're ready to execute some code!

## Executing Code

A Lug::VM has two methods to interpret code.

The first option is Lug::VM::Interpret, which expects a string of Lug code, like this.
```
Lug::InterpretResult result = vm.Interpret("print(\"Hello, world!\")")
```

You can also directly pass in a path to a file with Lug::VM::InterpretFile. 
The VM will read the file's contents and execute it.
```
const std::filesystem::path& pathToScript = ...;
Lug::InterpretResult result = vm.InterpretFile(pathToScript)
```

Both cases return a Lug::InterpretResult, which tells you whether or not your program succeeded or 
decided to error on you. If the program failed during compilation, it'll return a 
Lug::InterpretResult::COMPILE_ERROR. During runtime, a Lug::InterpretResult::RUNTIME_ERROR. There's also 
Lug::InterpretResult::MEMORY_ERROR, which, if you get it, you know something definitely screwed up.

```
if (result == Lug::InterpretResult::OK) {
    std::cout << "Program ran successfully." << '\n';
} else if (result == Lug::InterpretResult::COMPILE_ERROR) {
    std::cout << "Compilation error." << '\n'
} else if (result == Lug::InterpretResult::RUNTIME_ERROR) {
    std::cout << "Runtime error." << '\n'
} else if (result == Lug::InterpretResult::MEMORY_ERROR) {
    std::cout << "Memory error." << '\n'
}
```
## Basic Example (Lug Wrapper)

```
#include <iostream>
#include <string>
#include <fstream>
#include <ostream>
#include <sstream>

#include <lug.hpp>

namespace fs = std::filesystem;

static void OnError(const Lug::Error& error) {
    std::cerr << error.Format() << std::endl;
}


static void Repl(Lug::VM& vm) {
    std::cout << "Lug Interactive REPL\n";

    std::string line;
    while (true) {
        std::cout << "> ";
        std::getline(std::cin, line);

        if (line.empty()) break;

        Lug::InterpretResult result = vm.Interpret(line, "repl");
    }
}


static std::string ReadFile(std::string_view path) {
    std::ifstream file(path.data());
    if (!file.is_open()) {
        std::cerr << "could not open file: " << path << std::endl;
        exit(74);
    }

    // Read the entire file buffer into a string stream
    std::ostringstream buffer;
    buffer << file.rdbuf();

    return buffer.str();
}


static void RunFile(Lug::VM& vm, std::string_view path) {
    Lug::InterpretResult result = vm.InterpretFile(path);

    if (result == Lug::InterpretResult::OK) return;

    // Exit codes consistent with the Unix sysexits.h standard
    if (result == Lug::InterpretResult::COMPILE_ERROR) exit(65);
    if (result == Lug::InterpretResult::FILE_ERROR) exit(66);
    if (result == Lug::InterpretResult::RUNTIME_ERROR) exit(70);
    if (result == Lug::InterpretResult::MEMORY_ERROR) exit(1);
}


int main(int argc, char* argv[]) {
    Lug::VMConfig config;
    config.errorFn = OnError;

    Lug::VM vm(config);
    vm.LoadLibrary(Lug::LibInclude::ALL);

    if (argc == 1) {
        Repl(vm);
    } else if (argc == 2) {
        RunFile(vm, argv[1]);
    } else {
        std::cerr << "usage: lug [path]\n";
        exit(64);
    }

    return 0;
}
```