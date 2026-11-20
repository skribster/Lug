#pragma once

#include <optional>
#include <variant>
#include <string>
#include <vector>
#include <utility>
#include <type_traits>
#include <unordered_map>
#include <functional>
#include <stdexcept>

namespace Lug {

struct Variant;
using List = std::vector<Variant>;
using Dict = std::vector<std::pair<Variant, Variant>>;
using Thing = std::unordered_map<std::string, Variant>;
using Function = std::function<Variant(const List& args)>;

struct UserError : std::runtime_error {
    using std::runtime_error::runtime_error;
};

struct Variant {
public:
    Variant() = default;            // std::monostate (nil)
    Variant(bool boolean)           : data(boolean) {} 
    Variant(const char* string)     : data(std::string(string)) {}
    Variant(std::string string)     : data(string) {}
    Variant(List list)              : data(std::move(list)) {}
    Variant(Dict dict)              : data(std::move(dict)) {}
    Variant(Thing thing)            : data(std::move(thing)) {}

    // Only enable this constructor if the function we are passing is callable
    template<typename F, typename = std::enable_if_t<std::is_invocable_r_v<Variant, std::decay_t<F>&, const List&>>>
    Variant(F&& function)      : data(std::move(function)) {}

    template <typename T, typename = std::enable_if_t<std::is_arithmetic_v<T> && !std::is_same_v<T, bool>>>
    Variant(T number) : data(static_cast<double>(number)) {}

    template<typename T>
    std::optional<T> As() const {
        if constexpr (std::is_same_v<T, bool>) {
            if (auto* as = std::get_if<bool>(&data)) return *as;
        }
        else if constexpr (std::is_arithmetic_v<T>) {
            if (auto* as = std::get_if<double>(&data)) return static_cast<T>(*as);
        }
        else if constexpr (std::is_same_v<T, std::string>) {
            if (auto* as = std::get_if<std::string>(&data)) return *as;
        }
        else if constexpr (std::is_same_v<T, List>) {
            if (auto* as = std::get_if<List>(&data)) return *as;
        }
        else if constexpr (std::is_same_v<T, Dict>) {
            if (auto* as = std::get_if<Dict>(&data)) return *as;
        } 
        else if constexpr (std::is_same_v<T, Thing>) {
            if (auto* as = std::get_if<Thing>(&data)) return *as; 
        } 
        else if constexpr (std::is_same_v<T, Function>) {
            if (auto* as = std::get_if<Function>(&data)) return *as;
        }
        else {
            static_assert(sizeof(T) == 0, "Lug: unsupported type for Var::As");
        }

        return std::nullopt;
    }

    const auto& Data() const { return data; }

private:
    std::variant<
        std::monostate,     // Nil
        double,             // Number
        bool,               // Boolean
        std::string,        // String
        List,               // List
        Dict,               // Dict
        Thing,              // Thing
        Function            // Function
    > data;
};

}