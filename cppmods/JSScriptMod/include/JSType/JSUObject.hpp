#pragma once

extern "C" {
#include "quickjs.h"
}

namespace RC::JSScript
{
    /**
     * JSUObject - JavaScript binding for Unreal UObject
     * 
     * This class provides the bridge between JavaScript and UE4's UObject system,
     * enabling JavaScript scripts to access and manipulate UObject instances.
     */
    class JSUObject
    {
    public:
        // Class ID for QuickJS
        static JSClassID class_id;

        // Initialize the UObject class in the given context
        static auto init_class(JSContext* ctx) -> void;
        
        // Create a new UObject wrapper
        static auto create(JSContext* ctx, void* uobject) -> JSValue;
        
        // Get the UObject pointer from a JS value
        static auto get_uobject(JSContext* ctx, JSValue val) -> void*;
    };

} // namespace RC::JSScript
