#include <iostream>
#include <string>
#include <fstream>
#include <ostream>
#include <sstream>

#include "lug.hpp"

namespace fs = std::filesystem;

struct Player {
    int health;
    void Heal(int h) { health += h; }
};

static void OnError(const Lug::Error& error) {
    std::cerr << "\033[31m" << error.Format() << "\033[0m" << std::endl;
}


static std::optional<fs::path> OnResolve(fs::path importer, const fs::path requested) {
    fs::path path = fs::path("bootstrap/") / requested;
    path.replace_extension(".lug");
    if (!fs::is_regular_file(path)) return std::nullopt;

    return path;
}


static void Repl(Lug::VM& vm) {
    std::cout << "Lug Interactive REPL\n";

    std::string line;
    while (true) {
        std::cout << "> ";
        std::getline(std::cin, line);

        if (line.empty()) break;

        Lug::InterpretResult result = vm.Interpret(line, "repl");
        if (result != Lug::InterpretResult::OK) {
            const Lug::Error& error = vm.LastError();
            std::cerr << error.Format() << '\n';
        }
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
    config.resolveModuleFn = OnResolve;
    config.errorFn = OnError;

    Lug::VM vm(config);
    vm.LoadLibrary(Lug::LibInclude::ALL);

    Lug::Thing player = vm.NewThing();
    player["health"] = 20;
    player["takeDamage"] = [](Lug::Thing self, double damage) {
        self["health"] = self["health"].AsOr<double>(0) - damage;
    };

    vm["Player"] = player;

    Lug::Dict dict = vm.NewDict();
    dict[2] = "whatever";
    vm["what"] = dict;

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