#pragma once

#include <type_traits>
#include <optional>
#include <variant>
#include <tuple>

#include <string>
#include <string_view>
#include <vector>
#include <functional>
#include <memory>

#include <utility>
#include <stdexcept>

namespace Lug {

struct UserError : std::runtime_error {
    using std::runtime_error::runtime_error;
};

// Forward declarations so C++ doesn't get mad at me

class VM;
class VMCore;
struct Variant;
struct Handle;


/**
 * Little helper class to implement the [] operator in classes like
 * Thing, Dict, VM.
 */
template<typename Owner, typename Key>
class Field {
public:
    Field(Owner* owner, Key key)
        : owner(owner), key(key) {}

    template<typename V>
    Field& operator=(V&& value) { 
        owner->Set(key, value); 
        return *this; 
    }

    template<typename T> 
    std::optional<T> As() const;

    template<typename T>
    T AsOr(T fallback) const;

    operator Variant() const;

private:
    Owner* owner;
    Key key;
};

// Object types ---------------------------------

namespace Impl {

/**
 * Inheritors of this class represent a Variant type that
 * holds a reference to an VM-managed Obj*.
 */
struct ObjectRef {
public:
    ObjectRef() = delete;
    ObjectRef(std::shared_ptr<Handle> handle) : handle(std::move(handle)) {}

    bool IsValid() const;
    explicit operator bool() const { return IsValid(); }
    
protected:
    std::shared_ptr<Handle> handle;
};

} // Impl


// List --------------------------

struct List : Impl::ObjectRef {
public:
    Variant Get(int index) const;
    void Set(int index, const Variant& value);
    void Append(const Variant& item);

    Field<List, int> operator[](int index);
    Variant operator[](int index) const;

private:
    explicit List(std::shared_ptr<Handle> handle) 
        : ObjectRef(std::move(handle)) {} 

    friend class VM;
    friend class VMCore;
};

// Dict --------------------------

struct Dict : Impl::ObjectRef {
public:
    void Set(const Variant& key, const Variant& value);
    Variant Get(const Variant& key) const;

    Field<Dict, Variant> operator[](const Variant& key);
    Variant operator[](const Variant& key) const;
    
private:
    explicit Dict(std::shared_ptr<Handle> handle) 
        : ObjectRef(std::move(handle)) {}

    friend class VM;
    friend class VMCore;
};


// Thing -------------------------

struct Thing : Impl::ObjectRef {
public:
    Variant Get(std::string_view attribute) const;
    void Set(std::string_view key, const Variant& value);
    
    template<typename T>
    std::optional<T> Get(std::string_view key) const;
    
    template<typename T>
    T GetOr(std::string_view key, T fallback) const;
    
    Field<Thing, std::string> operator[](std::string_view attribute);
    Variant operator[](std::string_view attribute) const;
    
private:
    explicit Thing(std::shared_ptr<Handle> handle) 
        : ObjectRef(std::move(handle)) {}
    
    friend class VM;
    friend class VMCore;
};


// Other types......

using Nil = std::monostate;
using Function = std::function<Variant(const std::vector<Variant>& args)>;

namespace Impl {
    
//! the awful function metaprogramming part of the codebase i hate ------------
/**
 * This is the FunctionTraits pattern, and by passing in a function/lambda/callable
 * to Traits<T>, we are able to extract some of its features, like its
 * arity, arg types, etc. 
 * 
 * e.g. to get the arg types, we can pass in the function and get the Args variable.
 * Traits<some_function_here>::Args
 */

template<typename L> 
struct LambdaTraits;

/**
 * Because lambdas's operator() function is technically a member function, decltype(&F::operator())
 * would make it match to the Traits specialization that passes in the object as its first parameter.
 * However, for lambdas, we don't actually want that kind of behavior. To combat this, we make a 
 * separate FunctionTraits for lambdas that doesn't pass in Cls& as its first arg.
 */
template<typename Cls, typename Ret, typename... A>
struct LambdaTraits<Ret(Cls::*)(A...)> { 
    using Return = Ret;
    using Args = std::tuple<A...>; 
};

template<typename Cls, typename Ret, typename... A>
struct LambdaTraits<Ret(Cls::*)(A...) const> { 
    using Return = Ret; 
    using Args = std::tuple<A...>; 
};
    
/**
 * This is the entry point. If we want to get the traits of a function, we simply
 * do Traits<Fn>. We can then access those traits like:
 * 
 * using T = Traits<someRandomFunction>;
 * using Return = T::Return;    // get return type
 * using Args = T::Args;        // get arg types
 */
template<typename Fn>
struct FunctionTraits : LambdaTraits<decltype(&Fn::operator())> {};
/*  NOTE:               ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
    The 'inheritance' part is to defer lambdas/functors to their respective LambdaTraits
    cases. Lambdas are basically classes, and so they won't get matched with any of
    the other Traits<T> patterns which only match for function pointers. 
    
    Ret(*)(A...)                 !=          Lambda (has unnamed class type)
    Ret(Cls::*)(A...)            !=          Lambda (has unnamed class type)
    Ret(Cls::*)(A...) const      !=          Lambda (has unnamed class type)
    
    Instead, what we do is use its operator() function to match it to a separate LambdaTraits
    struct that handles all the lambda cases. &F::operator() gets the function pointer
    to its operator().
    
    Note that we could have done...
    struct Traits : Traits<decltype(&Fn::operator())> {};
    ...to instead defer the operator() to one of Traits's patterns. However, also note that if you
    pass in a member function pointer, such as lambda's operator(), it will automatically pass in
    the object as its first parameter. Since this behavior is not wanted for lambdas, we
    have to split it to another FunctionTraits to exclusively handle lambdas.
*/

// This specialization matches for a basic function pointer, which is as follows:
// ReturnType(* pointerName)(arg1, arg2, arg3, ...)
// e.g. int (* add)(int a, int b)
// (i know, c and c++ is stupid for having that kind of syntax for function pointers......)
template<typename Ret, typename... A>
struct FunctionTraits<Ret(*)(A...)> {
    using Return = Ret;
    using Args = std::tuple<A...>;
};

// This specialization matches for a class member function pointer, or a function
// part of a struct/class. The structure is as follows:
// ReturnType (ClassName::* pointerName)(ArgTypes...)
template<typename Cls, typename Ret, typename... A>
struct FunctionTraits<Ret(Cls::*)(A...)> {
    using Return = Ret;
    // For member functions, we actually pass the object as the first parameter, which is
    // why we pass Cls& as the first type in std::tuple<T...>.
    using Args = std::tuple<Cls&, A...>;
};

// This specialization matches for a const class member function pointer.
// mouthful to say oh my god but just think of it as a reference to a class
// function, which is also qualified as const
template<typename Cls, typename Ret, typename... A>
struct FunctionTraits<Ret(Cls::*)(A...) const> {
    using Return = Ret;
    using Args = std::tuple<const Cls&, A...>;
};

//! The actual stuff -----------------------------

template<typename T> T GetArg(const std::vector<Variant>& args, std::size_t i);
template<typename Fn> Lug::Function Wrap(Fn&& function);

template<typename Fn, typename Traits, std::size_t... Arity> 
Variant Invoke(Fn& function, const std::vector<Variant>& args, std::index_sequence<Arity...>);

// Helper functions to check whether a passed-in value is callable or not.
// The default case is false.
template<typename F, typename Enable = void> 
struct IsCallable : std::false_type {};

// If a value has an operator(), then it's callable.
template<typename F> 
struct IsCallable<F, std::void_t<decltype(&F::operator())>> : std::true_type {};

// If a value matches the structure of a function pointer, then it's callable.
template<typename R, typename... A>   
struct IsCallable<R(*)(A...)> : std::true_type {};

} // Impl

/**
 * This is a helper Variant type to make it easier
 * for the API to juggle C++ value types with Lug::Values.
 * This is NOT how Values are implemented in the VM. To see the 
 * actual Value implementation, go to lug_value.h/cpp.
 */
struct Variant {
public:
    Variant() = default;            // std::monostate (nil)
    Variant(bool boolean)           : data(boolean) {} 
    Variant(const char* string)     : data(std::string(string)) {}
    Variant(std::string string)     : data(string) {}
    Variant(List list)              : data(std::move(list)) {}
    Variant(Dict dict)              : data(std::move(dict)) {}
    Variant(Thing thing)            : data(std::move(thing)) {}

    // Constructor for a Lug::Function object.
    Variant(Function function)      : data(std::move(function)) {}

    // This is a shortcut constructor for when you pass in a function with C++ types.
    // It will wrap your function inside a Lug::Function object that uses Variants instead.
    template<typename Fn, typename = std::enable_if_t<Impl::IsCallable<std::decay_t<Fn>>::value>>
    Variant(Fn&& function) : data(Impl::Wrap(std::decay_t<Fn>(std::forward<Fn>(function)))) {}

    template<typename T, typename = std::enable_if_t<std::is_arithmetic_v<T> && !std::is_same_v<T, bool>>>
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

    template<typename T>
    bool Is() const {
        return std::holds_alternative<T>(data);
    }

    template<typename T>
    T AsOr(T fallback) const {
        std::optional<T> as = As<T>();
        if (!as) return fallback;
        return *as;
    }

    const auto& Data() const { return data; }

private:
    std::variant<
        Nil,                // Nil
        double,             // Number
        bool,               // Boolean
        std::string,        // String
        List,               // List
        Dict,               // Dict
        Thing,              // Thing
        Function            // Function
    > data;
};

// Out-of-line definitions that need 
// the full definition of Variant..........

template<typename Owner, typename Key>
template<typename T>
std::optional<T> Field<Owner, Key>::As() const {
    return owner->Get(key).template As<T>();
}

template<typename Owner, typename Key>
template<typename T>
T Field<Owner, Key>::AsOr(T fallback) const {
    return As<T>().value_or(fallback);
}

template<typename Owner, typename Key>
Field<Owner, Key>::operator Variant() const {
    return owner->Get(key);
}

template<typename T>
std::optional<T> Thing::Get(std::string_view key) const {
    Variant v = Get(key);          // non-template overload
    return v.template As<T>();
}

template<typename T>
T Thing::GetOr(std::string_view key, T fallback) const {
    return Get<T>(key).value_or(fallback);
}

namespace Impl {

/**
 * Takes in a Variant arglist and an argument index.
 * Returns the argument of that index, but ensures that
 * it's off type T. If not, it throws an error
 */
template<typename T>
T GetArg(const std::vector<Variant>& args, std::size_t index) {
    Variant arg = args[index];
    std::optional<T> value = arg.As<T>();

    // If As<T> ends up null, then that means it's a wrong type.
    if (!value) throw UserError("bad argument #" + std::to_string(index + 1));
    return *value;
}


template<typename Fn, typename Traits, std::size_t... Indices>
Variant Invoke(Fn& function, const std::vector<Variant>& args, std::index_sequence<Indices...>) {
    using Args = typename Traits::Args;

    /*
        This is some weird syntax, but here is the basic explanation:
        
        This code will invoke the raw function (that, remember, uses C++ types instead
        of Variants) with all the args of const List& args. To do this, we actually have
        to convert all of the Variants of args into C++ objects. We can do this by utilizing
        a mix of GetArg and stupid template magic

        Note that 'Indices' right now is a packed sequence of std::size_t's. 
        We deduced it from the passed-in std::index_sequence. For instance, if
        we passed in std::make_index_sequence<5>{}, Indices... = 1, 2, 3, 4, 5
        
        GetArg<...> is the mechanism that we use to convert all our args in const List&
        into actual C++ primitive types. The ... at the end is to expand GetArg to all the
        other indices.
        
        GetArg<ArgType<Index>>(args, Indices)...
        If Indices... = 0, 1, 2, this becomes:
        GetArg<ArgType<0>>(args, 0), GetArg<ArgType<1>>(args, 1), GetArg<ArgType<2>>(args, 2)

        Let's walk through a real case. Say we have an Add function of sorts:
        vm.Set("add", [](double a, double b) {
            return a + b;
        })

        Then...
        Args = std::tuple<double, double>
        Arity = 2
        Indices... = 0, 1

        std::invoke(function, GetArg<std::decay_t<std::tuple_element_t<Indices, Args>>>(args, Indices)...);
        will expand to:
        std::invoke(function, GetArg<double>(args, 0), GetArg<double>(args, 1));

        Not so bad, right?
    */
   if constexpr (std::is_void_v<typename Traits::Return>) {
       std::invoke(function, GetArg<std::decay_t<std::tuple_element_t<Indices, Args>>>(args, Indices)...);
       // If the function won't return anything, return nil Variant.
        return Variant{ };
    } 

    // Otherwise, return what it returned.
    else {
        return std::invoke(function, GetArg<std::decay_t<std::tuple_element_t<Indices, Args>>>(args, Indices)...);
    }
}


/**
 * This function inputs whatever C++ function you give it and wraps it 
 * into a native Lug::Function. 
 * 
 * For instance, if passed in:
 * 
 * Function = [](int a, int b) -> int {}
 * 
 * it will wrap :
 * [](const List& args) -> Variant {
 *      return Function(args, arity);
 * }
 * 
 * This makes it callable in the VM's side which uses Variants and Values 
 * instead of pure C++ types.
 */
template<typename Fn>
Lug::Function Wrap(Fn&& function) {
    using Traits = FunctionTraits<Fn>;
    constexpr std::size_t arity = std::tuple_size_v<typename Traits::Args>;

    return [function](const std::vector<Variant>& args) mutable -> Variant {
        if (args.size() != arity) {
            // Throw an error if the function wasn't called with the correct argCount/arity.
            throw UserError("expected " + std::to_string(arity) + " argument(s) but got " + std::to_string(args.size()));
        }

        // The function body of this Lug::Function will call our function.
        return Invoke<Fn, Traits>(function, args, std::make_index_sequence<arity>{});
    };
}
    
} // Impl

} // Lug