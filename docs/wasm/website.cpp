#include <emscripten/bind.h>
#include <sstream>

#include <sstream>
#include <string>
#include <string_view>
#include <lug.hpp>

std::string run(const std::string& source) {
    std::ostringstream out;
    Lug::VMConfig config;
    config.writeFn = [&out](std::string_view text) {
        out << text;
    };
    config.errorFn = [&out](const Lug::Error& error) {
        out << error.Format() << '\n';
    };

    Lug::VM vm(config);
    vm.LoadLibrary(Lug::LibInclude::ALL);
    vm.Interpret(source);

    return out.str();
}

EMSCRIPTEN_BINDINGS(lug) {
    emscripten::function("run", &run);
}