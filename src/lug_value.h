#pragma once

#include <cmath>
#include <type_traits>

#include "lug_vector.h"

namespace Lug {

enum class ValueType {
    NUMBER,
    BOOLEAN,
    NIL,
    OBJ,

    // Not to be used directly.
    TOMBSTONE
};


// Empty sentinels, mainly for type checking
struct NaN {};

struct VMCore;
struct Obj;
struct ObjString;
enum class ObjType;

#ifdef LUG_NAN_TAGGING

/**
 * Represents a single Lug dynamic value.
 */
struct Value {
public:
    Value() : bits(QNAN | TAG_NIL) {}

    bool operator==(const Value& other) const;
    explicit operator bool() const { return !IsNil(); }
    
    Value(double number)    : bits(DoubleToBits(number)) {}
    Value(int number)       : bits(DoubleToBits(static_cast<double>(number))) {}
    Value(bool boolean)     : bits(QNAN | (boolean ? TAG_TRUE : TAG_FALSE)) {}
    Value(Obj* obj)         : bits(SIGN_BIT | QNAN | reinterpret_cast<std::uintptr_t>(obj) ) {}

    /*
        This makes it so that Value can accept object pointers (including
        pointers whose class inherits Obj, aka ObjString, ObjFunction, etc.)
        but not any other pointer.
    */
    template<typename T, typename = std::enable_if_t<!std::is_base_of_v<Obj, T>>>
    Value(T* anyOtherPointer) = delete;

    // Named singleton constructors
    inline static Value Nil() { return FromBits(NIL_VAL); }
    inline static Value Tombstone() { return FromBits(TOMBSTONE_VAL); }

    // Casting methods for converting a struct Value to a native C++ value
    inline double AsNumber() const  { return BitsToDouble(bits); }
    inline int AsInteger() const    { return static_cast<int>(AsNumber()); }
    inline bool AsBool() const      { return bits == TRUE_VAL; }
    inline Obj* AsObj() const       { return reinterpret_cast<Obj*>(static_cast<std::uintptr_t>(bits & ~(SIGN_BIT | QNAN))); }
    
    template<typename T> 
    T* AsObj() const { return static_cast<T*>(AsObj()); }

    // Type-checking methods for checking the type of a struct Value
    inline bool IsNumber() const    { return (bits & QNAN) != QNAN; }
    inline bool IsNan() const       { return IsNumber() && std::isnan(AsNumber()); }
    inline bool IsInteger() const   { return IsNumber() && static_cast<int>(AsNumber()) == AsNumber(); }

    inline bool IsBool() const      { return (bits | 1) == TRUE_VAL; }
    inline bool IsNil() const       { return bits == NIL_VAL; }
    inline bool IsObj() const       { return (bits & (QNAN | SIGN_BIT)) == (QNAN | SIGN_BIT); }
    inline bool IsTombstone() const { return bits == TOMBSTONE_VAL; }

    template<typename T>
    inline bool Is() const {
        if constexpr (std::is_same_v<T, double>)            return IsNumber();
        else if constexpr (std::is_same_v<T, int>)          return IsInteger();
        else if constexpr (std::is_same_v<T, Lug::NaN>)     return IsNan();
        else if constexpr (std::is_same_v<T, bool>)         return IsBool();
        else if constexpr (std::is_same_v<T, Lug::Nil>)     return IsNil();
        else if constexpr (std::is_same_v<T, Obj>)          return IsObj();
        else if constexpr (std::is_base_of_v<Obj, T>)       return IsObj(T::STATIC_TYPE);
        else static_assert(sizeof(T) == 0, "unsupported type for Value::Is<T>");
    }

    template<typename T>
    inline auto As() const {
        if constexpr (std::is_same_v<T, double>)            return AsNumber();
        else if constexpr (std::is_same_v<T, int>)          return AsInteger();
        else if constexpr (std::is_same_v<T, bool>)         return AsBool();
        else if constexpr (std::is_same_v<T, Obj>)          return AsObj();
        else if constexpr (std::is_base_of_v<Obj, T>)       return AsObj<T>();
        else static_assert(sizeof(T) == 0, "unsupported type for Value::As<T>");
    }

    // Defined in lug_object.h
    inline bool IsObj(ObjType type) const;

    static void Print(const Value& value);
    static const char* StringType(ValueType type);
    static const char* StringType(ObjType type);
    inline ValueType Type() const {
        if (IsNumber()) return ValueType::NUMBER;
        if (IsObj())    return ValueType::OBJ;
        if (IsNil())    return ValueType::NIL;
        if (IsBool())   return ValueType::BOOLEAN;
        return ValueType::TOMBSTONE;
    }

    const char* StringType() const;
    std::uint32_t GetHash() const;
    ObjString* ToString(VMCore& vm) const;
    Value Copy();

private:
    std::uint64_t bits;

    // number constants............ ahhhhhhhhh.......................
    static constexpr std::uint64_t SIGN_BIT         = 0x8000000000000000ULL;
    static constexpr std::uint64_t QNAN             = 0x7ffc000000000000ULL;

    // Tags 
    static constexpr std::uint64_t TAG_NIL          = 0b001;
    static constexpr std::uint64_t TAG_FALSE        = 0b010;
    static constexpr std::uint64_t TAG_TRUE         = 0b011;
    static constexpr std::uint64_t TAG_TOMBSTONE    = 0b100;

    // Values
    static constexpr std::uint64_t NIL_VAL          = QNAN | TAG_NIL;
    static constexpr std::uint64_t TRUE_VAL         = QNAN | TAG_TRUE;
    static constexpr std::uint64_t FALSE_VAL        = QNAN | TAG_FALSE;
    static constexpr std::uint64_t TOMBSTONE_VAL    = QNAN | TAG_TOMBSTONE;

    static inline std::uint64_t DoubleToBits(double d) {
        std::uint64_t b;
        std::memcpy(&b, &d, sizeof(b));   // NOT reinterpret_cast: that's UB
        return b;
    }
    
    static inline double BitsToDouble(std::uint64_t b) {
        double d;
        std::memcpy(&d, &b, sizeof(d));
        return d;
    }

    static inline Value FromBits(std::uint64_t bits) {
        Value value;
        value.bits = bits;
        return value;
    }
};

#else

/**
 * Represents a single Lug dynamic value.
 */
struct Value {
public:
    Value() : type(ValueType::NIL) { as.number = 0; }

    bool operator==(const Value& other) const;
    
    Value(double number) : type(ValueType::NUMBER)          { as.number = number; }
    Value(int number) : type(ValueType::NUMBER)             { as.number = static_cast<double>(number); }
    Value(bool boolean) : type(ValueType::BOOLEAN)          { as.boolean = boolean; }
    Value(Obj* obj) : type(ValueType::OBJ)                  { as.obj = static_cast<Obj*>(obj); }

    /*
        This makes it so that Value can accept object pointers (including
        pointers whose class inherits Obj, aka ObjString, ObjFunction, etc.)
        but not any other pointer.
    */
    template<typename T, typename = std::enable_if_t<!std::is_base_of_v<Obj, T>>>
    Value(T* anyOtherPointer) = delete;

    // Named singleton constructors
    inline static Value Nil()           { return Value(ValueType::NIL); }
    inline static Value Tombstone()     { return Value(ValueType::TOMBSTONE); }

    // Casting methods for converting a struct Value to a native C++ value
    inline double AsNumber() const      { return as.number; }
    inline int AsInteger() const        { return static_cast<int>(AsNumber()); }
    inline bool AsBool() const          { return as.boolean; }
    inline Obj* AsObj() const           { return as.obj; }

    template<typename T> 
    T* AsObj() const { return static_cast<T*>(AsObj()); }

    // Type-checking methods for checking the type of a struct Value
    inline bool IsNumber() const        { return type == ValueType::NUMBER; }
    inline bool IsNan() const           { return IsNumber() && std::isnan(AsNumber()); }
    inline bool IsInteger() const       { return IsNumber() && static_cast<int>(AsNumber()) == AsNumber(); }

    inline bool IsBool() const          { return type == ValueType::BOOLEAN; }
    inline bool IsNil() const           { return type == ValueType::NIL; }
    inline bool IsObj() const           { return type == ValueType::OBJ; }
    inline bool IsTombstone() const     { return type == ValueType::TOMBSTONE; }
    
    // Defined in lug_object.h
    inline bool IsObj(ObjType type) const;

    template<typename T>
    inline bool Is() const {
        if constexpr (std::is_same_v<T, double>)            return IsNumber();
        else if constexpr (std::is_same_v<T, int>)          return IsInteger();
        else if constexpr (std::is_same_v<T, Lug::NaN>)     return IsNan();
        else if constexpr (std::is_same_v<T, bool>)         return IsBool();
        else if constexpr (std::is_same_v<T, Lug::Nil>)     return IsNil();
        else if constexpr (std::is_same_v<T, Obj>)          return IsObj();
        else if constexpr (std::is_base_of_v<Obj, T>)       return IsObj(T::STATIC_TYPE);
        else static_assert(sizeof(T) == 0, "unsupported type for Value::Is<T>");
    }

    template<typename T>
    inline auto As() const {
        if constexpr (std::is_same_v<T, double>)            return AsNumber();
        else if constexpr (std::is_same_v<T, int>)          return AsInteger();
        else if constexpr (std::is_same_v<T, bool>)         return AsBool();
        else if constexpr (std::is_same_v<T, Obj>)          return AsObj();
        else if constexpr (std::is_base_of_v<Obj, T>)       return AsObj<T>();
        else static_assert(sizeof(T) == 0, "unsupported type for Value::As<T>");
    }

    // I like the convention of passing in the value to Print()
    // because it looks better..........
    static void Print(const Value& value);
    static const char* StringType(ValueType type);
    static const char* StringType(ObjType type);
    inline ValueType Type() const { return type; }

    const char* StringType() const;
    std::uint32_t GetHash() const;
    ObjString* ToString(VMCore& vm) const;
    Value Copy();
    
private:
    // Tag-only constructor used ONLY internally for building a singleton type
    explicit Value(ValueType type) : type(type) { as.number = 0; }
    ValueType type;
    
    /*
    We store a value as a struct that holds the current
    type of the value, and a union that's the actual
    value of the... well, value.
    
    Visualized
    ----------------------------------
    (Keep in mind that the extra padding is due to
    struct alignment, where each item is aligned
    in byte multiples of the largest item in size)
    
    [****] [     ] [********]
         ^^^^   ^^^^^   ^^^^^^^^
         Type  Padding    'as'
                        ^^^^^^^^
                          number 
                        ^
                     boolean

    */
    union {
        double number;
        bool boolean;
        Obj* obj;
    } as;
};

#endif

}