#include "lug_object.h"
#include "lug_variant.hpp"
#include "lug_vmcore.h"

namespace Lug {

bool Impl::ObjectRef::IsValid() const { 
    return handle && handle->vm; 
}

// Thing functions ------------------------------

Variant Thing::Get(std::string_view key) const {
    if (!IsValid()) throw UserError("can't use an object whose VM has already been destroyed");
    
    ObjThing* thing = handle->value.AsObj<ObjThing>();
    const Value* value = thing->Get(key.data());
    if (value == nullptr) return Variant{ };
    
    return handle->vm->ToVariant(*value);
}


void Thing::Set(std::string_view key, const Variant& value) {
    if (!IsValid()) throw UserError("can't use an object whose VM has already been destroyed");
   
    ObjThing* thing = handle->value.AsObj<ObjThing>();
    
    const char* name = key.data();
    thing->Set(name, handle->vm->ToValue(value, name));
}


Field<Thing, std::string> Thing::operator[](std::string_view attribute) {
    return Field{ this, std::string(attribute) };
}


Lug::Variant Thing::operator[](std::string_view attribute) const {
    return Get(attribute);
}

// List functions ------------------------------



Variant List::Get(int index) const {
    if (!IsValid()) throw UserError("can't use an object whose VM has already been destroyed");
    
    ObjList* list = handle->value.AsObj<ObjList>();
    VMCore* vm = handle->vm;
    
    Value* value = list->Get(index);
    if (value == nullptr) return Variant{ };
    
    return vm->ToVariant(*value);
}


void List::Set(int index, const Variant& value) {
    if (!IsValid()) throw UserError("can't use an object whose VM has already been destroyed");

    ObjList* list = handle->value.AsObj<ObjList>();
    VMCore* vm = handle->vm;
    
    Value newValue = vm->ToValue(value);

    vm->stack.Push(newValue);
    list->Set(index, newValue);
    vm->stack.Pop(); // newValue
}


void List::Append(const Variant& item) {
    if (!IsValid()) throw UserError("can't use an object whose VM has already been destroyed");
    
    ObjList* list = handle->value.AsObj<ObjList>();
    VMCore* vm = handle->vm;
    
    list->Append(vm->ToValue(item));
}


Field<List, int> List::operator[](int index) {
    return { this, index };
}


Variant List::operator[](int index) const { 
    if (!IsValid()) throw UserError("can't use an object whose VM has already been destroyed");
    
    return Get(index);
} 

// Dict functions ------------------------------

void Dict::Set(const Variant& k, const Variant& v) {
    if (!IsValid()) throw UserError("can't use an object whose VM has already been destroyed");
    
    ObjDict* dict = handle->value.AsObj<ObjDict>();
    VMCore* vm = handle->vm;
    
    Value key = vm->ToValue(k);
    Value value = vm->ToValue(v);
    
    vm->stack.Push(key);
    vm->stack.Push(value);
    
    dict->Set(key, value);
    
    vm->stack.Pop(); // value
    vm->stack.Pop(); // key
};


Variant Dict::Get(const Variant& k) const {
    if (!IsValid()) throw UserError("can't use an object whose VM has already been destroyed");
    
    ObjDict* dict = handle->value.AsObj<ObjDict>();
    VMCore* vm = handle->vm;

    Value key = vm->ToValue(k);
    const Value* value = dict->Get(key);

    if (value == nullptr) return Variant{ };

    return vm->ToVariant(*value);
}


Field<Dict, Variant> Dict::operator[](const Variant& key) {
    return { this, key };
}

}