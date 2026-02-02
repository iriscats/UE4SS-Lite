#include "JSMod.hpp"
#include "JSType/JSUObject.hpp"

#include <fstream>
#include <sstream>

#include <DynamicOutput/DynamicOutput.hpp>
#include <UE4SSProgram.hpp>
#include <Unreal/UObjectGlobals.hpp>
#include <Unreal/UObject.hpp>
#include <Unreal/UClass.hpp>
#include <Unreal/UFunction.hpp>
#include <Unreal/FProperty.hpp>

// QuickJS headers
extern "C" {
#include "quickjs.h"
#include "quickjs-libc.h"
}

namespace RC::JSScript
{
    // Forward declarations for global functions
    static JSValue js_print(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
    static JSValue js_find_first_of(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
    static JSValue js_find_all_of(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
    static JSValue js_static_find_object(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
    static JSValue js_register_hook(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
    static JSValue js_notify_on_new_object(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
    
    // Module loader functions
    static char* js_module_normalize(JSContext* ctx, const char* base_name, const char* name, void* opaque);
    static JSModuleDef* js_module_loader(JSContext* ctx, const char* module_name, void* opaque);

    // Helper to get JSMod instance from context
    static JSMod* get_js_mod(JSContext* ctx)
    {
        return static_cast<JSMod*>(JS_GetRuntimeOpaque(JS_GetRuntime(ctx)));
    }

    JSMod::JSMod()
    {
        // Get the mods directory from UE4SS
        auto& program = UE4SSProgram::get_program();
        auto mods_dir = program.get_mods_directory();
        
        // Set scripts path to JSScripts folder in Mods directory
        m_scripts_path = std::filesystem::path(mods_dir) / STR("JSScripts") / STR("js");
        
        Output::send<LogLevel::Normal>(STR("[JSScript] Scripts path: {}\n"), m_scripts_path.wstring());
    }

    JSMod::~JSMod()
    {
        stop();
    }

    auto JSMod::start() -> bool
    {
        if (m_initialized)
        {
            Output::send<LogLevel::Warning>(STR("[JSScript] Already initialized\n"));
            return true;
        }

        Output::send<LogLevel::Normal>(STR("[JSScript] Starting JavaScript engine...\n"));

        // Initialize QuickJS runtime
        if (!init_runtime())
        {
            Output::send<LogLevel::Error>(STR("[JSScript] Failed to initialize runtime\n"));
            return false;
        }

        // Initialize main context
        if (!init_context())
        {
            Output::send<LogLevel::Error>(STR("[JSScript] Failed to initialize context\n"));
            stop();
            return false;
        }

        // Setup module loader
        setup_module_loader();

        m_initialized = true;

        // Find and execute scripts
        auto scripts = find_scripts();
        if (scripts.empty())
        {
            Output::send<LogLevel::Normal>(STR("[JSScript] No scripts found in {}\n"), m_scripts_path.wstring());
        }
        else
        {
            Output::send<LogLevel::Normal>(STR("[JSScript] Found {} script(s)\n"), scripts.size());
            for (const auto& script : scripts)
            {
                Output::send<LogLevel::Normal>(STR("[JSScript] Loading script: {}\n"), script.filename().wstring());
                load_and_execute_script(script);
            }
        }

        return true;
    }

    auto JSMod::stop() -> void
    {
        if (!m_initialized && !m_runtime)
        {
            return;
        }

        Output::send<LogLevel::Normal>(STR("[JSScript] Stopping JavaScript engine...\n"));

        // Clean up hook callbacks
        {
            std::lock_guard<std::mutex> lock(m_hooks_mutex);
            for (auto& hook : m_hook_callbacks)
            {
                if (hook.ctx)
                {
                    JS_FreeValue(hook.ctx, hook.callback);
                }
            }
            m_hook_callbacks.clear();
        }

        // Free context
        if (m_main_ctx)
        {
            JS_FreeContext(m_main_ctx);
            m_main_ctx = nullptr;
        }

        // Free runtime
        if (m_runtime)
        {
            JS_FreeRuntime(m_runtime);
            m_runtime = nullptr;
        }

        m_initialized = false;
        m_loaded_modules.clear();

        Output::send<LogLevel::Normal>(STR("[JSScript] JavaScript engine stopped\n"));
    }

    auto JSMod::tick() -> void
    {
        if (!m_initialized || !m_main_ctx)
        {
            return;
        }

        // Execute pending jobs (promises, timers, etc.)
        JSContext* ctx;
        int err;
        while ((err = JS_ExecutePendingJob(m_runtime, &ctx)) > 0)
        {
            // Job executed
        }
        if (err < 0)
        {
            log_exception(ctx);
        }
    }

    auto JSMod::init_runtime() -> bool
    {
        m_runtime = JS_NewRuntime();
        if (!m_runtime)
        {
            Output::send<LogLevel::Error>(STR("[JSScript] Failed to create QuickJS runtime\n"));
            return false;
        }

        // Set memory limit (100MB)
        JS_SetMemoryLimit(m_runtime, 100 * 1024 * 1024);

        // Set GC threshold
        JS_SetGCThreshold(m_runtime, 1024 * 1024);

        // Store this pointer for accessing JSMod from callbacks
        JS_SetRuntimeOpaque(m_runtime, this);

        Output::send<LogLevel::Normal>(STR("[JSScript] QuickJS runtime created\n"));
        return true;
    }

    auto JSMod::init_context() -> bool
    {
        m_main_ctx = JS_NewContext(m_runtime);
        if (!m_main_ctx)
        {
            Output::send<LogLevel::Error>(STR("[JSScript] Failed to create QuickJS context\n"));
            return false;
        }

        // Add standard helpers (console, etc.)
        js_std_add_helpers(m_main_ctx, 0, nullptr);

        // Setup global functions (print, FindFirstOf, etc.)
        setup_global_functions(m_main_ctx);

        // Setup UE4 class bindings
        setup_classes(m_main_ctx);

        Output::send<LogLevel::Normal>(STR("[JSScript] QuickJS context created\n"));
        return true;
    }

    auto JSMod::setup_module_loader() -> void
    {
        JS_SetModuleLoaderFunc(m_runtime, js_module_normalize, js_module_loader, this);
        Output::send<LogLevel::Normal>(STR("[JSScript] Module loader configured\n"));
    }

    auto JSMod::setup_global_functions(JSContext* ctx) -> void
    {
        JSValue global = JS_GetGlobalObject(ctx);

        // Register global functions
        JS_SetPropertyStr(ctx, global, "print",
            JS_NewCFunction(ctx, js_print, "print", 1));
        JS_SetPropertyStr(ctx, global, "FindFirstOf",
            JS_NewCFunction(ctx, js_find_first_of, "FindFirstOf", 1));
        JS_SetPropertyStr(ctx, global, "FindAllOf",
            JS_NewCFunction(ctx, js_find_all_of, "FindAllOf", 1));
        JS_SetPropertyStr(ctx, global, "StaticFindObject",
            JS_NewCFunction(ctx, js_static_find_object, "StaticFindObject", 1));
        JS_SetPropertyStr(ctx, global, "RegisterHook",
            JS_NewCFunction(ctx, js_register_hook, "RegisterHook", 2));
        JS_SetPropertyStr(ctx, global, "NotifyOnNewObject",
            JS_NewCFunction(ctx, js_notify_on_new_object, "NotifyOnNewObject", 2));

        // Create UE4SS namespace object
        JSValue ue4ss = JS_NewObject(ctx);
        JS_SetPropertyStr(ctx, ue4ss, "version", JS_NewString(ctx, "1.0.0"));
        JS_SetPropertyStr(ctx, global, "UE4SS", ue4ss);

        JS_FreeValue(ctx, global);
        
        Output::send<LogLevel::Normal>(STR("[JSScript] Global functions registered\n"));
    }

    auto JSMod::setup_classes(JSContext* ctx) -> void
    {
        // Initialize UObject class binding
        JSUObject::init_class(ctx);
        
        Output::send<LogLevel::Normal>(STR("[JSScript] UE4 class bindings initialized\n"));
    }

    auto JSMod::find_scripts() -> std::vector<std::filesystem::path>
    {
        std::vector<std::filesystem::path> scripts;

        if (!std::filesystem::exists(m_scripts_path))
        {
            // Try to create the directory
            std::error_code ec;
            std::filesystem::create_directories(m_scripts_path, ec);
            if (ec)
            {
                Output::send<LogLevel::Warning>(STR("[JSScript] Could not create scripts directory: {}\n"), 
                    m_scripts_path.wstring());
            }
            return scripts;
        }

        // Look for main.js first
        auto main_script = m_scripts_path / "main.js";
        if (std::filesystem::exists(main_script))
        {
            scripts.push_back(main_script);
        }

        return scripts;
    }

    auto JSMod::load_and_execute_script(const std::filesystem::path& script_path) -> bool
    {
        if (!m_main_ctx)
        {
            Output::send<LogLevel::Error>(STR("[JSScript] Context not initialized\n"));
            return false;
        }

        // Read file content
        std::ifstream file(script_path, std::ios::binary);
        if (!file.is_open())
        {
            Output::send<LogLevel::Error>(STR("[JSScript] Failed to open script: {}\n"), 
                script_path.wstring());
            return false;
        }

        std::stringstream buffer;
        buffer << file.rdbuf();
        std::string content = buffer.str();
        file.close();

        // Execute script as module
        std::string filename = script_path.filename().string();
        JSValue result = JS_Eval(m_main_ctx, content.c_str(), content.size(),
            filename.c_str(), JS_EVAL_TYPE_MODULE);

        if (JS_IsException(result))
        {
            log_exception(m_main_ctx);
            JS_FreeValue(m_main_ctx, result);
            return false;
        }

        JS_FreeValue(m_main_ctx, result);
        Output::send<LogLevel::Normal>(STR("[JSScript] Script executed successfully: {}\n"), 
            script_path.filename().wstring());
        return true;
    }

    auto JSMod::execute_string(const std::string& code, const std::string& filename) -> bool
    {
        if (!m_main_ctx)
        {
            Output::send<LogLevel::Error>(STR("[JSScript] Context not initialized\n"));
            return false;
        }

        JSValue result = JS_Eval(m_main_ctx, code.c_str(), code.size(),
            filename.c_str(), JS_EVAL_TYPE_GLOBAL);

        if (JS_IsException(result))
        {
            log_exception(m_main_ctx);
            JS_FreeValue(m_main_ctx, result);
            return false;
        }

        JS_FreeValue(m_main_ctx, result);
        return true;
    }

    auto JSMod::log_exception(JSContext* ctx) -> void
    {
        JSValue exception = JS_GetException(ctx);
        
        const char* str = JS_ToCString(ctx, exception);
        if (str)
        {
            Output::send<LogLevel::Error>(STR("[JSScript] Exception: {}\n"), 
                std::wstring(str, str + strlen(str)));
            JS_FreeCString(ctx, str);
        }

        // Try to get stack trace
        JSValue stack = JS_GetPropertyStr(ctx, exception, "stack");
        if (!JS_IsUndefined(stack))
        {
            const char* stack_str = JS_ToCString(ctx, stack);
            if (stack_str)
            {
                Output::send<LogLevel::Error>(STR("[JSScript] Stack: {}\n"), 
                    std::wstring(stack_str, stack_str + strlen(stack_str)));
                JS_FreeCString(ctx, stack_str);
            }
        }
        JS_FreeValue(ctx, stack);
        JS_FreeValue(ctx, exception);
    }

    // ============================================
    // Global Functions Implementation
    // ============================================

    static JSValue js_print(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv)
    {
        std::wstring output;
        
        for (int i = 0; i < argc; i++)
        {
            if (i > 0)
            {
                output += L" ";
            }
            
            const char* str = JS_ToCString(ctx, argv[i]);
            if (str)
            {
                output += std::wstring(str, str + strlen(str));
                JS_FreeCString(ctx, str);
            }
        }
        
        Output::send<LogLevel::Normal>(STR("[JSScript] {}\n"), output);
        return JS_UNDEFINED;
    }

    static JSValue js_find_first_of(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv)
    {
        if (argc < 1)
        {
            return JS_ThrowTypeError(ctx, "FindFirstOf requires a class name argument");
        }

        const char* class_name = JS_ToCString(ctx, argv[0]);
        if (!class_name)
        {
            return JS_ThrowTypeError(ctx, "Invalid class name");
        }

        // Convert to wide string for UE4
        std::wstring wide_name(class_name, class_name + strlen(class_name));
        JS_FreeCString(ctx, class_name);

        // Find the object using UE4SS API
        Unreal::UObject* found_obj = Unreal::UObjectGlobals::FindFirstOf(wide_name);
        
        if (!found_obj)
        {
            return JS_NULL;
        }

        // Return as UObject wrapper
        return JSUObject::create(ctx, found_obj);
    }

    static JSValue js_find_all_of(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv)
    {
        if (argc < 1)
        {
            return JS_ThrowTypeError(ctx, "FindAllOf requires a class name argument");
        }

        const char* class_name = JS_ToCString(ctx, argv[0]);
        if (!class_name)
        {
            return JS_ThrowTypeError(ctx, "Invalid class name");
        }

        // Convert to wide string for UE4
        std::wstring wide_name(class_name, class_name + strlen(class_name));
        JS_FreeCString(ctx, class_name);

        // Find all objects
        std::vector<Unreal::UObject*> found_objects;
        Unreal::UObjectGlobals::FindAllOf(wide_name, found_objects);

        // Create JavaScript array
        JSValue result = JS_NewArray(ctx);
        for (size_t i = 0; i < found_objects.size(); i++)
        {
            // Return as UObject wrapper
            JS_SetPropertyUint32(ctx, result, static_cast<uint32_t>(i), 
                JSUObject::create(ctx, found_objects[i]));
        }

        return result;
    }

    static JSValue js_static_find_object(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv)
    {
        if (argc < 1)
        {
            return JS_ThrowTypeError(ctx, "StaticFindObject requires an object path argument");
        }

        const char* object_path = JS_ToCString(ctx, argv[0]);
        if (!object_path)
        {
            return JS_ThrowTypeError(ctx, "Invalid object path");
        }

        // Convert to wide string for UE4
        std::wstring wide_path(object_path, object_path + strlen(object_path));
        JS_FreeCString(ctx, object_path);

        // Find the object
        Unreal::UObject* found_obj = Unreal::UObjectGlobals::StaticFindObject(nullptr, nullptr, wide_path);

        if (!found_obj)
        {
            return JS_NULL;
        }

        // Return as UObject wrapper
        return JSUObject::create(ctx, found_obj);
    }

    static JSValue js_register_hook(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv)
    {
        if (argc < 2)
        {
            return JS_ThrowTypeError(ctx, "RegisterHook requires at least 2 arguments: function_name, callback");
        }

        // Get function name
        const char* func_name = JS_ToCString(ctx, argv[0]);
        if (!func_name)
        {
            return JS_ThrowTypeError(ctx, "Invalid function name");
        }

        // Check callback is a function
        if (!JS_IsFunction(ctx, argv[1]))
        {
            JS_FreeCString(ctx, func_name);
            return JS_ThrowTypeError(ctx, "Second argument must be a callback function");
        }

        std::wstring wide_func_name(func_name, func_name + strlen(func_name));
        JS_FreeCString(ctx, func_name);

        // Find the UFunction
        Unreal::UFunction* unreal_function = Unreal::UObjectGlobals::StaticFindObject<Unreal::UFunction*>(
            nullptr, nullptr, wide_func_name);

        if (!unreal_function)
        {
            Output::send<LogLevel::Error>(STR("[JSScript] RegisterHook: UFunction not found: {}\n"), wide_func_name);
            return JS_ThrowReferenceError(ctx, "UFunction not found");
        }

        // Store the callback reference
        JSMod* mod = get_js_mod(ctx);
        if (!mod)
        {
            return JS_ThrowInternalError(ctx, "Could not get JSMod instance");
        }

        // Duplicate the callback to prevent GC
        JSValue callback = JS_DupValue(ctx, argv[1]);

        // Store hook callback data
        {
            std::lock_guard<std::mutex> lock(mod->m_hooks_mutex);
            mod->m_hook_callbacks.push_back({ctx, callback, 0});
        }

        Output::send<LogLevel::Normal>(STR("[JSScript] RegisterHook: Registered hook for {}\n"), wide_func_name);
        
        // Note: Full UFunction hook integration requires more work with UE4SS's hook system
        // This is a simplified implementation that stores the callback
        // Full integration would use unreal_function->RegisterPreHook/RegisterPostHook

        // Return hook IDs (pre and post)
        JSValue result = JS_NewArray(ctx);
        JS_SetPropertyUint32(ctx, result, 0, JS_NewInt32(ctx, 0)); // pre_id
        JS_SetPropertyUint32(ctx, result, 1, JS_NewInt32(ctx, 0)); // post_id
        return result;
    }

    static JSValue js_notify_on_new_object(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv)
    {
        if (argc < 2)
        {
            return JS_ThrowTypeError(ctx, "NotifyOnNewObject requires 2 arguments: class_name, callback");
        }

        // Get class name
        const char* class_name = JS_ToCString(ctx, argv[0]);
        if (!class_name)
        {
            return JS_ThrowTypeError(ctx, "Invalid class name");
        }

        // Check callback is a function
        if (!JS_IsFunction(ctx, argv[1]))
        {
            JS_FreeCString(ctx, class_name);
            return JS_ThrowTypeError(ctx, "Second argument must be a callback function");
        }

        std::wstring wide_class_name(class_name, class_name + strlen(class_name));
        JS_FreeCString(ctx, class_name);

        Output::send<LogLevel::Normal>(STR("[JSScript] NotifyOnNewObject: Registered for class {}\n"), wide_class_name);
        
        // Note: Full implementation would integrate with UE4SS's object creation notification system
        // This is a placeholder that acknowledges the registration

        return JS_UNDEFINED;
    }

    // ============================================
    // Module Loader Implementation
    // ============================================

    static char* js_module_normalize(JSContext* ctx, const char* base_name, const char* name, void* opaque)
    {
        auto* mod = static_cast<JSMod*>(opaque);
        
        // If the name starts with './', resolve relative to base
        std::filesystem::path result;
        
        if (name[0] == '.' && (name[1] == '/' || (name[1] == '.' && name[2] == '/')))
        {
            // Relative path
            std::filesystem::path base(base_name);
            result = base.parent_path() / name;
        }
        else
        {
            // Absolute or module name - look in scripts directory
            result = mod->get_scripts_path() / name;
        }

        // Add .js extension if not present
        if (result.extension() != ".js")
        {
            result += ".js";
        }

        // Normalize path
        result = result.lexically_normal();

        // Return as C string (QuickJS will free it)
        std::string path_str = result.string();
        char* ret = static_cast<char*>(js_malloc(ctx, path_str.size() + 1));
        if (ret)
        {
            memcpy(ret, path_str.c_str(), path_str.size() + 1);
        }
        return ret;
    }

    static JSModuleDef* js_module_loader(JSContext* ctx, const char* module_name, void* opaque)
    {
        auto* mod = static_cast<JSMod*>(opaque);
        
        // Check if already loaded
        std::string name_str(module_name);
        if (mod->m_loaded_modules.count(name_str) > 0)
        {
            // Module already loaded, return null to use cached version
            // Actually QuickJS handles caching itself, so this shouldn't happen
        }

        // Read file
        std::ifstream file(module_name, std::ios::binary);
        if (!file.is_open())
        {
            JS_ThrowReferenceError(ctx, "Could not open module '%s'", module_name);
            return nullptr;
        }

        std::stringstream buffer;
        buffer << file.rdbuf();
        std::string content = buffer.str();
        file.close();

        // Compile module
        JSValue func_val = JS_Eval(ctx, content.c_str(), content.size(),
            module_name, JS_EVAL_TYPE_MODULE | JS_EVAL_FLAG_COMPILE_ONLY);

        if (JS_IsException(func_val))
        {
            return nullptr;
        }

        // Mark as loaded
        mod->m_loaded_modules[name_str] = true;

        // Get module definition from function value
        JSModuleDef* m = static_cast<JSModuleDef*>(JS_VALUE_GET_PTR(func_val));
        JS_FreeValue(ctx, func_val);
        
        return m;
    }

} // namespace RC::JSScript
