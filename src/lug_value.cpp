#include <cstdio>
#include <cstring>

#include "lug_memory.h"
#include "lug_value.h"
#include "lug_object.h"

namespace Lug {

// Helper functions ------------------------------
/**
 * Hashes a double into a uint32_t
 */
static std::uint32_t HashDouble(double value) {
    // Handle -0.0 and 0.0 to hash them equally.
    // For some reason when you hash them apart,
    // they will get different hashes EVEN THOUGH
    // they have the same value!!!! :(
    if (value == 0.0) value = 0.0;

    std::uint64_t raw;
    // Copy the bits of the double into the raw std::uint64_t
    std::memcpy(&raw, &value, sizeof(raw));

    // Fold the std::uint64_t into a std::uint32_t
    std::uint32_t hash = static_cast<std::uint32_t>(raw ^ (raw >> 32));
    
    return hash;
}

// Value methods ------------------------------

#ifndef LUG_NAN_TAGGING

bool Value::operator==(const Value& other) const {
    if (type != other.Type()) return false;

    switch (type) {
        case ValueType::BOOLEAN: {
            return AsBool() == other.AsBool(); 
        }

        case ValueType::NUMBER: {
            return AsNumber() == other.AsNumber();
        }

        case ValueType::OBJ: {
            return AsObj() == other.AsObj();
        }

        case ValueType::NIL: {
            return true;
        }

        default: UNREACHABLE();
    }
}

#else 

bool Value::operator==(const Value& other) const {
    // This is to handle IEEE-754, which states that NaN
    // values are not equal to themselves.
    if (IsNumber() && other.IsNumber()) {
        return AsNumber() == other.AsNumber(); 
    }

    return bits == other.bits;
}

#endif


void Value::Print(const Value& value) {
    switch (value.Type()) {
        case ValueType::NUMBER: {
            std::printf("%g", value.AsNumber());
            break;
        }

        case ValueType::BOOLEAN: {
            std::printf(value.AsBool() ? "true" : "false");
            break;
        }

        case ValueType::NIL: {
            std::printf("nil");
            break;
        }

        case ValueType::OBJ: {
            value.AsObj()->Print();
            break;
        }

        default: {
            std::printf("UNKNOWN_VALUE");
            break;
        }
    }
}


/**
 * Returns a string literal of the type of value.
 */
const char* Value::StringType() const {
    if (IsObj()) {
        return Value::StringType(AsObj()->Type());
    } else {
        return Value::StringType(Type());
    }
}


const char* Value::StringType(ValueType type) {
    switch (type) {
        case ValueType::BOOLEAN:    return "boolean";
        case ValueType::NIL:        return "nil";
        case ValueType::NUMBER:     return "number";

        default: {
            return "unknown";
            UNREACHABLE();
        }
    }
}


const char* Value::StringType(ObjType type) {
    return Obj::StringType(type);
}


std::uint32_t Value::GetHash() const {
    switch (Type()) {
        case ValueType::BOOLEAN:    return AsBool() ? 3 : 5;
        case ValueType::NIL:        return 7;
        case ValueType::NUMBER:     return HashDouble(AsNumber());
        case ValueType::OBJ:        return AsObj()->hash;
        case ValueType::TOMBSTONE:  return 0;

        default: UNREACHABLE();
    }
}


ObjString* Value::ToString(VMCore& vm) const {
    switch (Type()) {
        case ValueType::BOOLEAN: {
            return ObjString::FromLiteral(vm, AsBool() ? "true" : "false");
        }

        case ValueType::NIL: {
            return ObjString::FromLiteral(vm, "nil");
        }

        case ValueType::NUMBER: {
            char buffer[20];
            std::size_t length = std::snprintf(buffer, sizeof(buffer), "%.14g", AsNumber());
            
            return ObjString::FromSource(vm, buffer, length);
        }

        case ValueType::OBJ: {
            return AsObj()->ToString();
        }

        default: {
            std::printf("unknown value type '%d'\n", static_cast<int>(Type()));
            UNREACHABLE();
        }
    }
}


Value Value::Copy() {
    if (IsObj()) return AsObj()->Copy();
    return *this;
}


}