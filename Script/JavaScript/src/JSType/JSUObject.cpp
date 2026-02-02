#include "JSType/JSUObject.hpp"

#include <DynamicOutput/DynamicOutput.hpp>
#include <Unreal/UObject.hpp>
#include <Unreal/UClass.hpp>
#include <Unreal/FProperty.hpp>
#include <Unreal/Property/FObjectProperty.hpp>
#include <Unreal/Property/FStrProperty.hpp>
#include <Unreal/Property/FNameProperty.hpp>
#include <Unreal/Property/FBoolProperty.hpp>

extern "C" {
#include "quickjs.h"
}

namespace RC::JSScript
{
    // Static class ID
    JSClassID JSUObject::class_id = 0;

    // UObject data stored in opaque pointer
    struct UObjectData
    {
        Unreal::UObject* object;
        bool prevent_gc;  // Prevent GC from collecting this object
    };

    // Destructor for UObject wrapper
    static void js_uobject_finalizer(JSRuntime* rt, JSValue val)
    {
        UObjectData* data = static_cast<UObjectData*>(JS_GetOpaque(val, JSUObject::class_id));
        if (data)
        {
            js_free_rt(rt, data);
        }
    }

    // Get UObject's full name
    static JSValue js_uobject_get_full_name(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv)
    {
        UObjectData* data = static_cast<UObjectData*>(JS_GetOpaque2(ctx, this_val, JSUObject::class_id));
        if (!data || !data->object)
        {
            return JS_ThrowTypeError(ctx, "Invalid UObject");
        }

        std::wstring full_name = data->object->GetFullName();
        std::string utf8_name(full_name.begin(), full_name.end());
        return JS_NewString(ctx, utf8_name.c_str());
    }

    // Get UObject's class
    static JSValue js_uobject_get_class(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv)
    {
        UObjectData* data = static_cast<UObjectData*>(JS_GetOpaque2(ctx, this_val, JSUObject::class_id));
        if (!data || !data->object)
        {
            return JS_ThrowTypeError(ctx, "Invalid UObject");
        }

        Unreal::UClass* obj_class = data->object->GetClassPrivate();
        if (!obj_class)
        {
            return JS_NULL;
        }

        // Return class as another UObject wrapper
        return JSUObject::create(ctx, obj_class);
    }

    // Check if object is instance of a class
    static JSValue js_uobject_is_a(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv)
    {
        if (argc < 1)
        {
            return JS_ThrowTypeError(ctx, "IsA requires a class name argument");
        }

        UObjectData* data = static_cast<UObjectData*>(JS_GetOpaque2(ctx, this_val, JSUObject::class_id));
        if (!data || !data->object)
        {
            return JS_ThrowTypeError(ctx, "Invalid UObject");
        }

        const char* class_name = JS_ToCString(ctx, argv[0]);
        if (!class_name)
        {
            return JS_ThrowTypeError(ctx, "Invalid class name");
        }

        std::wstring wide_name(class_name, class_name + strlen(class_name));
        JS_FreeCString(ctx, class_name);

        // Get the class by name and check if object is an instance
        // This is a simplified implementation
        Unreal::UClass* obj_class = data->object->GetClassPrivate();
        if (obj_class)
        {
            std::wstring obj_class_name = obj_class->GetName();
            if (obj_class_name.find(wide_name) != std::wstring::npos)
            {
                return JS_TRUE;
            }
        }

        return JS_FALSE;
    }

    // Get object's memory address (for debugging)
    static JSValue js_uobject_get_address(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv)
    {
        UObjectData* data = static_cast<UObjectData*>(JS_GetOpaque2(ctx, this_val, JSUObject::class_id));
        if (!data || !data->object)
        {
            return JS_ThrowTypeError(ctx, "Invalid UObject");
        }

        return JS_NewInt64(ctx, reinterpret_cast<int64_t>(data->object));
    }

    // Check if object is valid
    static JSValue js_uobject_is_valid(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv)
    {
        UObjectData* data = static_cast<UObjectData*>(JS_GetOpaque2(ctx, this_val, JSUObject::class_id));
        if (!data || !data->object)
        {
            return JS_FALSE;
        }

        // Check if object pointer is valid (basic null check)
        // More sophisticated checks could be added using UObjectArray::IsValid
        return JS_NewBool(ctx, data->object != nullptr);
    }

    // Get object name
    static JSValue js_uobject_get_name(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv)
    {
        UObjectData* data = static_cast<UObjectData*>(JS_GetOpaque2(ctx, this_val, JSUObject::class_id));
        if (!data || !data->object)
        {
            return JS_ThrowTypeError(ctx, "Invalid UObject");
        }

        std::wstring name = data->object->GetName();
        std::string utf8_name(name.begin(), name.end());
        return JS_NewString(ctx, utf8_name.c_str());
    }

    // Dynamic property getter (exotic object method)
    static int js_uobject_get_own_property(JSContext* ctx, JSPropertyDescriptor* desc,
                                           JSValueConst obj, JSAtom prop)
    {
        UObjectData* data = static_cast<UObjectData*>(JS_GetOpaque(obj, JSUObject::class_id));
        if (!data || !data->object)
        {
            return 0;
        }

        const char* prop_name = JS_AtomToCString(ctx, prop);
        if (!prop_name)
        {
            return 0;
        }

        std::wstring wide_prop_name(prop_name, prop_name + strlen(prop_name));
        JS_FreeCString(ctx, prop_name);

        // Try to find property in UObject
        Unreal::UClass* obj_class = data->object->GetClassPrivate();
        if (!obj_class)
        {
            return 0;
        }

        // This is a simplified implementation - full property access will be added later
        // For now, return 0 to indicate property not found
        return 0;
    }

    // Prototype function list
    static const JSCFunctionListEntry js_uobject_proto_funcs[] = {
        JS_CFUNC_DEF("GetFullName", 0, js_uobject_get_full_name),
        JS_CFUNC_DEF("GetClass", 0, js_uobject_get_class),
        JS_CFUNC_DEF("IsA", 1, js_uobject_is_a),
        JS_CFUNC_DEF("GetAddress", 0, js_uobject_get_address),
        JS_CFUNC_DEF("IsValid", 0, js_uobject_is_valid),
        JS_CFUNC_DEF("GetName", 0, js_uobject_get_name),
    };

    // Class definition
    static JSClassDef js_uobject_class_def = {
        .class_name = "UObject",
        .finalizer = js_uobject_finalizer,
        .gc_mark = nullptr,
        .call = nullptr,
        .exotic = nullptr,
    };

    auto JSUObject::init_class(JSContext* ctx) -> void
    {
        JSRuntime* rt = JS_GetRuntime(ctx);

        // Create class ID if not already created
        if (class_id == 0)
        {
            JS_NewClassID(rt, &class_id);
        }

        // Create the class
        JS_NewClass(rt, class_id, &js_uobject_class_def);

        // Create prototype
        JSValue proto = JS_NewObject(ctx);
        JS_SetPropertyFunctionList(ctx, proto, js_uobject_proto_funcs, 
            sizeof(js_uobject_proto_funcs) / sizeof(js_uobject_proto_funcs[0]));

        // Set class prototype
        JS_SetClassProto(ctx, class_id, proto);

        Output::send<LogLevel::Normal>(STR("[JSScript] UObject class initialized\n"));
    }

    auto JSUObject::create(JSContext* ctx, void* uobject) -> JSValue
    {
        if (!uobject)
        {
            return JS_NULL;
        }

        // Allocate data
        UObjectData* data = static_cast<UObjectData*>(js_malloc(ctx, sizeof(UObjectData)));
        if (!data)
        {
            return JS_EXCEPTION;
        }

        data->object = static_cast<Unreal::UObject*>(uobject);
        data->prevent_gc = false;

        // Create object with class
        JSValue obj = JS_NewObjectClass(ctx, class_id);
        if (JS_IsException(obj))
        {
            js_free(ctx, data);
            return obj;
        }

        JS_SetOpaque(obj, data);
        return obj;
    }

    auto JSUObject::get_uobject(JSContext* ctx, JSValue val) -> void*
    {
        UObjectData* data = static_cast<UObjectData*>(JS_GetOpaque2(ctx, val, class_id));
        if (!data)
        {
            return nullptr;
        }
        return data->object;
    }

} // namespace RC::JSScript
