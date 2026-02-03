#include "JSMod.hpp"
#include "JSType/JSUObject.hpp"

#include <fstream>
#include <sstream>
#include <chrono>

#include <DynamicOutput/DynamicOutput.hpp>
#include <UE4SSProgram.hpp>
#include <Unreal/UObjectGlobals.hpp>
#include <Unreal/UObject.hpp>
#include <Unreal/UClass.hpp>
#include <Unreal/UFunction.hpp>
#include <Unreal/FProperty.hpp>
#include <Unreal/UFunctionStructs.hpp>
#include <Unreal/FFrame.hpp>
#include <Unreal/UnrealFlags.hpp>

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
    static JSValue js_hook_ufunction(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
    static JSValue js_unregister_hook(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
    static JSValue js_notify_on_new_object(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
    
    // Timer functions
    static JSValue js_set_timeout(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
    static JSValue js_set_interval(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
    static JSValue js_clear_timeout(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
    static JSValue js_clear_interval(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
    
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
        m_mods_directory = program.get_mods_directory();
        
        // Initialize start time for timers
        auto now = std::chrono::steady_clock::now();
        m_start_time = std::chrono::duration<double>(now.time_since_epoch()).count();
        
        Output::send<LogLevel::Normal>(STR("[UE4SSL.JavaScript] Mods directory: {}\n"), m_mods_directory.wstring());
    }

    JSMod::~JSMod()
    {
        stop();
    }

    auto JSMod::start() -> bool
    {
        // Full start for backward compatibility
        if (!init_engine())
        {
            return false;
        }
        load_scripts();
        return true;
    }
    
    auto JSMod::init_engine() -> bool
    {
        if (m_initialized)
        {
            Output::send<LogLevel::Warning>(STR("[UE4SSL.JavaScript] Already initialized\n"));
            return true;
        }

        Output::send<LogLevel::Normal>(STR("[UE4SSL.JavaScript] Starting JavaScript engine...\n"));

        // Initialize QuickJS runtime
        if (!init_runtime())
        {
            Output::send<LogLevel::Error>(STR("[UE4SSL.JavaScript] Failed to initialize runtime\n"));
            return false;
        }

        // Initialize main context
        if (!init_context())
        {
            Output::send<LogLevel::Error>(STR("[UE4SSL.JavaScript] Failed to initialize context\n"));
            stop();
            return false;
        }

        // Setup module loader
        setup_module_loader();

        m_initialized = true;
        return true;
    }
    
    auto JSMod::load_scripts() -> void
    {
        if (!m_initialized)
        {
            Output::send<LogLevel::Error>(STR("[UE4SSL.JavaScript] Engine not initialized, cannot load scripts\n"));
            return;
        }
        
        // Find and execute scripts
        auto scripts = find_scripts();
        if (scripts.empty())
        {
            Output::send<LogLevel::Normal>(STR("[UE4SSL.JavaScript] No scripts found in {}\n"), m_mods_directory.wstring());
        }
        else
        {
            Output::send<LogLevel::Normal>(STR("[UE4SSL.JavaScript] Found {} script(s)\n"), scripts.size());
            for (const auto& script : scripts)
            {
                Output::send<LogLevel::Normal>(STR("[UE4SSL.JavaScript] Loading script: {}\n"), script.filename().wstring());
                load_and_execute_script(script);
            }
        }
    }

    auto JSMod::stop() -> void
    {
        if (!m_initialized && !m_runtime)
        {
            return;
        }

        Output::send<LogLevel::Normal>(STR("[UE4SSL.JavaScript] Stopping JavaScript engine...\n"));

        // Clean up UFunction hooks
        {
            std::lock_guard<std::mutex> lock(m_ufunction_hooks_mutex);
            for (auto& hook : m_ufunction_hooks)
            {
                if (hook && hook->function)
                {
                    // Unregister the hooks from UE4SS
                    if (hook->pre_id != 0)
                    {
                        hook->function->UnregisterHook(hook->pre_id);
                    }
                    if (hook->post_id != 0)
                    {
                        hook->function->UnregisterHook(hook->post_id);
                    }
                    // Free JS callback values
                    if (hook->ctx)
                    {
                        if (!JS_IsUndefined(hook->pre_callback))
                        {
                            JS_FreeValue(hook->ctx, hook->pre_callback);
                        }
                        if (!JS_IsUndefined(hook->post_callback))
                        {
                            JS_FreeValue(hook->ctx, hook->post_callback);
                        }
                    }
                }
            }
            m_ufunction_hooks.clear();
        }

        // Clean up legacy hook callbacks
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
        
        // Clean up timer callbacks
        {
            std::lock_guard<std::mutex> lock(m_timers_mutex);
            for (auto& timer : m_timers)
            {
                if (timer.ctx && !timer.cancelled)
                {
                    JS_FreeValue(timer.ctx, timer.callback);
                }
            }
            m_timers.clear();
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

        Output::send<LogLevel::Normal>(STR("[UE4SSL.JavaScript] JavaScript engine stopped\n"));
    }

    auto JSMod::tick() -> void
    {
        if (!m_initialized || !m_main_ctx)
        {
            return;
        }

        // Process timers
        process_timers();

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
    
    auto JSMod::get_current_time() const -> double
    {
        auto now = std::chrono::steady_clock::now();
        return std::chrono::duration<double>(now.time_since_epoch()).count();
    }
    
    auto JSMod::add_timer(JSContext* ctx, JSValue callback, double delay_ms, bool is_interval) -> int32_t
    {
        std::lock_guard<std::mutex> lock(m_timers_mutex);
        
        int32_t id = m_next_timer_id++;
        double trigger_time = get_current_time() + (delay_ms / 1000.0);
        
        TimerCallback timer;
        timer.ctx = ctx;
        timer.callback = JS_DupValue(ctx, callback);  // Prevent GC
        timer.id = id;
        timer.trigger_time = trigger_time;
        timer.interval = is_interval ? (delay_ms / 1000.0) : 0.0;
        timer.is_interval = is_interval;
        timer.cancelled = false;
        
        m_timers.push_back(timer);
        return id;
    }
    
    auto JSMod::cancel_timer(int32_t id) -> bool
    {
        std::lock_guard<std::mutex> lock(m_timers_mutex);
        
        for (auto& timer : m_timers)
        {
            if (timer.id == id && !timer.cancelled)
            {
                timer.cancelled = true;
                return true;
            }
        }
        return false;
    }
    
    auto JSMod::process_timers() -> void
    {
        if (!m_main_ctx) return;
        
        double current_time = get_current_time();
        
        // Collect timer info we need (id, callback, is_interval) to fire
        // We do this in two phases to avoid holding the lock during JS_Call
        struct TimerToFire {
            int32_t id;
            JSValue callback;
            bool is_interval;
        };
        std::vector<TimerToFire> to_fire;
        
        {
            std::lock_guard<std::mutex> lock(m_timers_mutex);
            
            for (auto& timer : m_timers)
            {
                if (timer.cancelled) continue;
                if (current_time < timer.trigger_time) continue;
                
                // Dup the callback so we have our own reference
                to_fire.push_back({timer.id, JS_DupValue(timer.ctx, timer.callback), timer.is_interval});
                
                if (timer.is_interval)
                {
                    // Reschedule for next interval
                    timer.trigger_time = current_time + timer.interval;
                }
                else
                {
                    // Mark setTimeout for cleanup
                    timer.cancelled = true;
                    JS_FreeValue(timer.ctx, timer.callback);
                }
            }
            
            // Clean up cancelled non-interval timers
            m_timers.erase(
                std::remove_if(m_timers.begin(), m_timers.end(),
                    [](const TimerCallback& t) { return t.cancelled && !t.is_interval; }),
                m_timers.end()
            );
        }
        
        // Now fire callbacks without holding the lock
        for (auto& item : to_fire)
        {
            JSValue result = JS_Call(m_main_ctx, item.callback, JS_UNDEFINED, 0, nullptr);
            if (JS_IsException(result))
            {
                log_exception(m_main_ctx);
            }
            JS_FreeValue(m_main_ctx, result);
            JS_FreeValue(m_main_ctx, item.callback);  // Free our dup'd reference
        }
    }

    auto JSMod::init_runtime() -> bool
    {
        m_runtime = JS_NewRuntime();
        if (!m_runtime)
        {
            Output::send<LogLevel::Error>(STR("[UE4SSL.JavaScript] Failed to create QuickJS runtime\n"));
            return false;
        }

        // Set memory limit (100MB)
        JS_SetMemoryLimit(m_runtime, 100 * 1024 * 1024);

        // Set GC threshold
        JS_SetGCThreshold(m_runtime, 1024 * 1024);

        // Store this pointer for accessing JSMod from callbacks
        JS_SetRuntimeOpaque(m_runtime, this);

        Output::send<LogLevel::Normal>(STR("[UE4SSL.JavaScript] QuickJS runtime created\n"));
        return true;
    }

    auto JSMod::init_context() -> bool
    {
        m_main_ctx = JS_NewContext(m_runtime);
        if (!m_main_ctx)
        {
            Output::send<LogLevel::Error>(STR("[UE4SSL.JavaScript] Failed to create QuickJS context\n"));
            return false;
        }

        // Add standard helpers (console, etc.)
        js_std_add_helpers(m_main_ctx, 0, nullptr);

        // Setup global functions (print, FindFirstOf, etc.)
        setup_global_functions(m_main_ctx);

        // Setup UE4 class bindings
        setup_classes(m_main_ctx);

        Output::send<LogLevel::Normal>(STR("[UE4SSL.JavaScript] QuickJS context created\n"));
        return true;
    }

    auto JSMod::setup_module_loader() -> void
    {
        JS_SetModuleLoaderFunc(m_runtime, js_module_normalize, js_module_loader, this);
        Output::send<LogLevel::Normal>(STR("[UE4SSL.JavaScript] Module loader configured\n"));
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
            JS_NewCFunction(ctx, js_register_hook, "RegisterHook", 3));
        JS_SetPropertyStr(ctx, global, "HookUFunction",
            JS_NewCFunction(ctx, js_hook_ufunction, "HookUFunction", 3));
        JS_SetPropertyStr(ctx, global, "UnregisterHook",
            JS_NewCFunction(ctx, js_unregister_hook, "UnregisterHook", 2));
        JS_SetPropertyStr(ctx, global, "NotifyOnNewObject",
            JS_NewCFunction(ctx, js_notify_on_new_object, "NotifyOnNewObject", 2));
        
        // Register timer functions
        JS_SetPropertyStr(ctx, global, "setTimeout",
            JS_NewCFunction(ctx, js_set_timeout, "setTimeout", 2));
        JS_SetPropertyStr(ctx, global, "setInterval",
            JS_NewCFunction(ctx, js_set_interval, "setInterval", 2));
        JS_SetPropertyStr(ctx, global, "clearTimeout",
            JS_NewCFunction(ctx, js_clear_timeout, "clearTimeout", 1));
        JS_SetPropertyStr(ctx, global, "clearInterval",
            JS_NewCFunction(ctx, js_clear_interval, "clearInterval", 1));

        // Create UE4SS namespace object
        JSValue ue4ss = JS_NewObject(ctx);
        JS_SetPropertyStr(ctx, ue4ss, "version", JS_NewString(ctx, "1.0.0"));
        JS_SetPropertyStr(ctx, global, "UE4SS", ue4ss);

        JS_FreeValue(ctx, global);
        
        Output::send<LogLevel::Normal>(STR("[UE4SSL.JavaScript] Global functions registered\n"));
    }

    auto JSMod::setup_classes(JSContext* ctx) -> void
    {
        // Initialize UObject class binding
        JSUObject::init_class(ctx);
        
        Output::send<LogLevel::Normal>(STR("[UE4SSL.JavaScript] UE4 class bindings initialized\n"));
    }

    auto JSMod::find_scripts() -> std::vector<std::filesystem::path>
    {
        std::vector<std::filesystem::path> scripts;

        if (!std::filesystem::exists(m_mods_directory))
        {
            Output::send<LogLevel::Warning>(STR("[UE4SSL.JavaScript] Mods directory does not exist: {}\n"), 
                m_mods_directory.wstring());
            return scripts;
        }

        // Scan all subdirectories in mods directory for js/main.js
        std::error_code ec;
        for (const auto& entry : std::filesystem::directory_iterator(m_mods_directory, ec))
        {
            if (!entry.is_directory())
            {
                continue;
            }

            // Check for js/main.js in this mod directory
            auto js_script = entry.path() / "js" / "main.js";
            if (std::filesystem::exists(js_script))
            {
                Output::send<LogLevel::Normal>(STR("[UE4SSL.JavaScript] Found script: {}\n"), js_script.wstring());
                scripts.push_back(js_script);
            }
        }

        if (ec)
        {
            Output::send<LogLevel::Warning>(STR("[UE4SSL.JavaScript] Error scanning mods directory: {}\n"), 
                std::wstring(ec.message().begin(), ec.message().end()));
        }

        return scripts;
    }

    auto JSMod::load_and_execute_script(const std::filesystem::path& script_path) -> bool
    {
        if (!m_main_ctx)
        {
            Output::send<LogLevel::Error>(STR("[UE4SSL.JavaScript] Context not initialized\n"));
            return false;
        }

        // Read file content
        std::ifstream file(script_path, std::ios::binary);
        if (!file.is_open())
        {
            Output::send<LogLevel::Error>(STR("[UE4SSL.JavaScript] Failed to open script: {}\n"), 
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
        Output::send<LogLevel::Normal>(STR("[UE4SSL.JavaScript] Script executed successfully: {}\n"), 
            script_path.filename().wstring());
        return true;
    }

    auto JSMod::execute_string(const std::string& code, const std::string& filename) -> bool
    {
        if (!m_main_ctx)
        {
            Output::send<LogLevel::Error>(STR("[UE4SSL.JavaScript] Context not initialized\n"));
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
            Output::send<LogLevel::Error>(STR("[UE4SSL.JavaScript] Exception: {}\n"), 
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
                Output::send<LogLevel::Error>(STR("[UE4SSL.JavaScript] Stack: {}\n"), 
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
        
        Output::send<LogLevel::Normal>(STR("[UE4SSL.JavaScript] {}\n"), output);
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

        // Find the object using UE4SS API with exception handling
        try
        {
            Unreal::UObject* found_obj = Unreal::UObjectGlobals::FindFirstOf(wide_name);
            
            if (!found_obj)
            {
                return JS_NULL;
            }

            // Return as UObject wrapper
            return JSUObject::create(ctx, found_obj);
        }
        catch (const std::exception& e)
        {
            Output::send<LogLevel::Error>(STR("[UE4SSL.JavaScript] FindFirstOf exception: {}\n"), 
                std::wstring(e.what(), e.what() + strlen(e.what())));
            return JS_NULL;
        }
        catch (...)
        {
            Output::send<LogLevel::Error>(STR("[UE4SSL.JavaScript] FindFirstOf unknown exception\n"));
            return JS_NULL;
        }
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

        // Find all objects with exception handling
        try
        {
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
        catch (const std::exception& e)
        {
            Output::send<LogLevel::Error>(STR("[UE4SSL.JavaScript] FindAllOf exception: {}\n"), 
                std::wstring(e.what(), e.what() + strlen(e.what())));
            return JS_NewArray(ctx); // Return empty array
        }
        catch (...)
        {
            Output::send<LogLevel::Error>(STR("[UE4SSL.JavaScript] FindAllOf unknown exception\n"));
            return JS_NewArray(ctx); // Return empty array
        }
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

        // Find the object with exception handling
        try
        {
            Unreal::UObject* found_obj = Unreal::UObjectGlobals::StaticFindObject(nullptr, nullptr, wide_path);

            if (!found_obj)
            {
                return JS_NULL;
            }

            // Return as UObject wrapper
            return JSUObject::create(ctx, found_obj);
        }
        catch (const std::exception& e)
        {
            Output::send<LogLevel::Error>(STR("[UE4SSL.JavaScript] StaticFindObject exception: {}\n"), 
                std::wstring(e.what(), e.what() + strlen(e.what())));
            return JS_NULL;
        }
        catch (...)
        {
            Output::send<LogLevel::Error>(STR("[UE4SSL.JavaScript] StaticFindObject unknown exception\n"));
            return JS_NULL;
        }
    }

    static JSValue js_register_hook(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv)
    {
        if (argc < 2)
        {
            return JS_ThrowTypeError(ctx, "RegisterHook requires at least 2 arguments: function_name, pre_callback [, post_callback]");
        }

        // Get function name/path
        const char* func_name = JS_ToCString(ctx, argv[0]);
        if (!func_name)
        {
            return JS_ThrowTypeError(ctx, "Invalid function name");
        }

        // Check pre_callback is a function (or null/undefined for no pre-hook)
        JSValue pre_callback = JS_UNDEFINED;
        JSValue post_callback = JS_UNDEFINED;

        if (JS_IsFunction(ctx, argv[1]))
        {
            pre_callback = argv[1];
        }
        else if (!JS_IsNull(argv[1]) && !JS_IsUndefined(argv[1]))
        {
            JS_FreeCString(ctx, func_name);
            return JS_ThrowTypeError(ctx, "Second argument must be a callback function or null");
        }

        // Check optional post_callback
        if (argc >= 3)
        {
            if (JS_IsFunction(ctx, argv[2]))
            {
                post_callback = argv[2];
            }
            else if (!JS_IsNull(argv[2]) && !JS_IsUndefined(argv[2]))
            {
                JS_FreeCString(ctx, func_name);
                return JS_ThrowTypeError(ctx, "Third argument must be a callback function or null");
            }
        }

        std::wstring wide_func_name(func_name, func_name + strlen(func_name));
        JS_FreeCString(ctx, func_name);

        // Find the UFunction with exception handling
        try
        {
            Unreal::UFunction* unreal_function = Unreal::UObjectGlobals::StaticFindObject<Unreal::UFunction*>(
                nullptr, nullptr, wide_func_name);

            if (!unreal_function)
            {
                Output::send<LogLevel::Warning>(STR("[UE4SSL.JavaScript] RegisterHook: UFunction not found: {}\n"), wide_func_name);
                return JS_ThrowReferenceError(ctx, "UFunction not found");
            }

            // Get JSMod instance
            JSMod* mod = get_js_mod(ctx);
            if (!mod)
            {
                return JS_ThrowInternalError(ctx, "Could not get JSMod instance");
            }

            // Register the hook with UE4SS hook system
            auto [pre_id, post_id] = mod->register_ufunction_hook(ctx, unreal_function, pre_callback, post_callback);

            Output::send<LogLevel::Normal>(STR("[UE4SSL.JavaScript] RegisterHook: Successfully registered hook for {} (pre_id={}, post_id={})\n"), 
                wide_func_name, pre_id, post_id);

            // Return hook IDs (pre and post)
            JSValue result = JS_NewArray(ctx);
            JS_SetPropertyUint32(ctx, result, 0, JS_NewInt32(ctx, pre_id));
            JS_SetPropertyUint32(ctx, result, 1, JS_NewInt32(ctx, post_id));
            return result;
        }
        catch (const std::exception& e)
        {
            Output::send<LogLevel::Error>(STR("[UE4SSL.JavaScript] RegisterHook exception: {}\n"), 
                std::wstring(e.what(), e.what() + strlen(e.what())));
            return JS_ThrowInternalError(ctx, "RegisterHook failed due to exception");
        }
        catch (...)
        {
            Output::send<LogLevel::Error>(STR("[UE4SSL.JavaScript] RegisterHook unknown exception\n"));
            return JS_ThrowInternalError(ctx, "RegisterHook failed due to unknown exception");
        }
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

        Output::send<LogLevel::Normal>(STR("[UE4SSL.JavaScript] NotifyOnNewObject: Registered for class {}\n"), wide_class_name);
        
        // Note: Full implementation would integrate with UE4SS's object creation notification system
        // This is a placeholder that acknowledges the registration

        return JS_UNDEFINED;
    }

    // HookUFunction - Direct equivalent of C# Hooking.HookUFunction
    // Usage: HookUFunction(ufunction_object, pre_callback, post_callback)
    // ufunction_object can be obtained via StaticFindObject
    static JSValue js_hook_ufunction(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv)
    {
        if (argc < 2)
        {
            return JS_ThrowTypeError(ctx, "HookUFunction requires at least 2 arguments: ufunction, pre_callback [, post_callback]");
        }

        // Get UFunction from first argument (should be a UObject wrapper)
        Unreal::UFunction* function = nullptr;
        
        // Check if it's a UObject wrapper
        Unreal::UObject* obj = JSUObject::get_native(ctx, argv[0]);
        if (obj)
        {
            // Cast to UFunction
            function = static_cast<Unreal::UFunction*>(obj);
        }
        else
        {
            return JS_ThrowTypeError(ctx, "First argument must be a UFunction object");
        }

        if (!function)
        {
            return JS_ThrowTypeError(ctx, "Invalid UFunction object");
        }

        // Check pre_callback
        JSValue pre_callback = JS_UNDEFINED;
        JSValue post_callback = JS_UNDEFINED;

        if (JS_IsFunction(ctx, argv[1]))
        {
            pre_callback = argv[1];
        }
        else if (!JS_IsNull(argv[1]) && !JS_IsUndefined(argv[1]))
        {
            return JS_ThrowTypeError(ctx, "Second argument must be a callback function or null");
        }

        // Check optional post_callback
        if (argc >= 3)
        {
            if (JS_IsFunction(ctx, argv[2]))
            {
                post_callback = argv[2];
            }
            else if (!JS_IsNull(argv[2]) && !JS_IsUndefined(argv[2]))
            {
                return JS_ThrowTypeError(ctx, "Third argument must be a callback function or null");
            }
        }

        try
        {
            // Get JSMod instance
            JSMod* mod = get_js_mod(ctx);
            if (!mod)
            {
                return JS_ThrowInternalError(ctx, "Could not get JSMod instance");
            }

            // Register the hook
            auto [pre_id, post_id] = mod->register_ufunction_hook(ctx, function, pre_callback, post_callback);

            Output::send<LogLevel::Normal>(STR("[UE4SSL.JavaScript] HookUFunction: Successfully hooked {} (pre_id={}, post_id={})\n"), 
                function->GetFullName(), pre_id, post_id);

            // Return hook IDs as array [pre_id, post_id]
            JSValue result = JS_NewArray(ctx);
            JS_SetPropertyUint32(ctx, result, 0, JS_NewInt32(ctx, pre_id));
            JS_SetPropertyUint32(ctx, result, 1, JS_NewInt32(ctx, post_id));
            return result;
        }
        catch (const std::exception& e)
        {
            Output::send<LogLevel::Error>(STR("[UE4SSL.JavaScript] HookUFunction exception: {}\n"), 
                std::wstring(e.what(), e.what() + strlen(e.what())));
            return JS_ThrowInternalError(ctx, "HookUFunction failed due to exception");
        }
        catch (...)
        {
            Output::send<LogLevel::Error>(STR("[UE4SSL.JavaScript] HookUFunction unknown exception\n"));
            return JS_ThrowInternalError(ctx, "HookUFunction failed due to unknown exception");
        }
    }

    // UnregisterHook - Unregister a previously registered hook
    // Usage: UnregisterHook(pre_id, post_id)
    static JSValue js_unregister_hook(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv)
    {
        if (argc < 2)
        {
            return JS_ThrowTypeError(ctx, "UnregisterHook requires 2 arguments: pre_id, post_id");
        }

        int32_t pre_id = 0;
        int32_t post_id = 0;

        if (JS_ToInt32(ctx, &pre_id, argv[0]) != 0)
        {
            return JS_ThrowTypeError(ctx, "First argument must be a number (pre_id)");
        }

        if (JS_ToInt32(ctx, &post_id, argv[1]) != 0)
        {
            return JS_ThrowTypeError(ctx, "Second argument must be a number (post_id)");
        }

        try
        {
            JSMod* mod = get_js_mod(ctx);
            if (!mod)
            {
                return JS_ThrowInternalError(ctx, "Could not get JSMod instance");
            }

            bool success = mod->unregister_ufunction_hook(pre_id, post_id);
            
            if (success)
            {
                Output::send<LogLevel::Normal>(STR("[UE4SSL.JavaScript] UnregisterHook: Successfully unregistered hook (pre_id={}, post_id={})\n"), 
                    pre_id, post_id);
            }
            else
            {
                Output::send<LogLevel::Warning>(STR("[UE4SSL.JavaScript] UnregisterHook: Hook not found (pre_id={}, post_id={})\n"), 
                    pre_id, post_id);
            }

            return JS_NewBool(ctx, success);
        }
        catch (const std::exception& e)
        {
            Output::send<LogLevel::Error>(STR("[UE4SSL.JavaScript] UnregisterHook exception: {}\n"), 
                std::wstring(e.what(), e.what() + strlen(e.what())));
            return JS_ThrowInternalError(ctx, "UnregisterHook failed due to exception");
        }
        catch (...)
        {
            Output::send<LogLevel::Error>(STR("[UE4SSL.JavaScript] UnregisterHook unknown exception\n"));
            return JS_ThrowInternalError(ctx, "UnregisterHook failed due to unknown exception");
        }
    }

    // ============================================
    // Timer Functions Implementation
    // ============================================

    static JSValue js_set_timeout(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv)
    {
        if (argc < 2)
        {
            return JS_ThrowTypeError(ctx, "setTimeout requires 2 arguments: callback, delay");
        }

        // Check callback is a function
        if (!JS_IsFunction(ctx, argv[0]))
        {
            return JS_ThrowTypeError(ctx, "First argument must be a callback function");
        }

        // Get delay in milliseconds
        double delay_ms;
        if (JS_ToFloat64(ctx, &delay_ms, argv[1]) != 0)
        {
            return JS_ThrowTypeError(ctx, "Second argument must be a number (delay in ms)");
        }

        // Ensure non-negative delay
        if (delay_ms < 0) delay_ms = 0;

        JSMod* mod = get_js_mod(ctx);
        if (!mod)
        {
            return JS_ThrowInternalError(ctx, "Could not get JSMod instance");
        }

        int32_t timer_id = mod->add_timer(ctx, argv[0], delay_ms, false);
        return JS_NewInt32(ctx, timer_id);
    }

    static JSValue js_set_interval(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv)
    {
        if (argc < 2)
        {
            return JS_ThrowTypeError(ctx, "setInterval requires 2 arguments: callback, interval");
        }

        // Check callback is a function
        if (!JS_IsFunction(ctx, argv[0]))
        {
            return JS_ThrowTypeError(ctx, "First argument must be a callback function");
        }

        // Get interval in milliseconds
        double interval_ms;
        if (JS_ToFloat64(ctx, &interval_ms, argv[1]) != 0)
        {
            return JS_ThrowTypeError(ctx, "Second argument must be a number (interval in ms)");
        }

        // Ensure minimum interval (prevent runaway loops)
        if (interval_ms < 10) interval_ms = 10;

        JSMod* mod = get_js_mod(ctx);
        if (!mod)
        {
            return JS_ThrowInternalError(ctx, "Could not get JSMod instance");
        }

        int32_t timer_id = mod->add_timer(ctx, argv[0], interval_ms, true);
        return JS_NewInt32(ctx, timer_id);
    }

    static JSValue js_clear_timeout(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv)
    {
        if (argc < 1)
        {
            return JS_UNDEFINED; // Silent fail like browsers
        }

        int32_t timer_id;
        if (JS_ToInt32(ctx, &timer_id, argv[0]) != 0)
        {
            return JS_UNDEFINED; // Silent fail
        }

        JSMod* mod = get_js_mod(ctx);
        if (mod)
        {
            mod->cancel_timer(timer_id);
        }

        return JS_UNDEFINED;
    }

    static JSValue js_clear_interval(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv)
    {
        // Same implementation as clearTimeout
        return js_clear_timeout(ctx, this_val, argc, argv);
    }

    // ============================================
    // UFunction Hook Implementation
    // ============================================

    auto JSMod::register_ufunction_hook(JSContext* ctx, Unreal::UFunction* function, 
                                        JSValue pre_callback, JSValue post_callback) -> std::pair<int32_t, int32_t>
    {
        if (!function)
        {
            Output::send<LogLevel::Error>(STR("[UE4SSL.JavaScript] register_ufunction_hook: function is null\n"));
            return {0, 0};
        }

        // Create hook data
        auto hook_data = std::make_unique<JSUFunctionHookData>();
        hook_data->owner = this;
        hook_data->ctx = ctx;
        hook_data->function = function;
        hook_data->has_return_value = false;
        hook_data->pre_id = 0;
        hook_data->post_id = 0;

        // Duplicate JS values to prevent GC
        if (JS_IsFunction(ctx, pre_callback))
        {
            hook_data->pre_callback = JS_DupValue(ctx, pre_callback);
        }
        else
        {
            hook_data->pre_callback = JS_UNDEFINED;
        }

        if (JS_IsFunction(ctx, post_callback))
        {
            hook_data->post_callback = JS_DupValue(ctx, post_callback);
        }
        else
        {
            hook_data->post_callback = JS_UNDEFINED;
        }

        // Get raw pointer before moving into vector
        JSUFunctionHookData* raw_hook_data = hook_data.get();

        // Store in our list
        {
            std::lock_guard<std::mutex> lock(m_ufunction_hooks_mutex);
            m_ufunction_hooks.push_back(std::move(hook_data));
        }

        // Register hooks with UE4SS hook system
        Unreal::CallbackId pre_id = 0;
        Unreal::CallbackId post_id = 0;

        if (!JS_IsUndefined(raw_hook_data->pre_callback))
        {
            pre_id = function->RegisterPreHook(js_ufunction_hook_pre, raw_hook_data);
            raw_hook_data->pre_id = pre_id;
            Output::send<LogLevel::Normal>(STR("[UE4SSL.JavaScript] Registered pre-hook (id={}) for {}\n"), 
                pre_id, function->GetFullName());
        }

        if (!JS_IsUndefined(raw_hook_data->post_callback))
        {
            post_id = function->RegisterPostHook(js_ufunction_hook_post, raw_hook_data);
            raw_hook_data->post_id = post_id;
            Output::send<LogLevel::Normal>(STR("[UE4SSL.JavaScript] Registered post-hook (id={}) for {}\n"), 
                post_id, function->GetFullName());
        }

        return {pre_id, post_id};
    }

    auto JSMod::unregister_ufunction_hook(Unreal::CallbackId pre_id, Unreal::CallbackId post_id) -> bool
    {
        std::lock_guard<std::mutex> lock(m_ufunction_hooks_mutex);

        for (auto it = m_ufunction_hooks.begin(); it != m_ufunction_hooks.end(); ++it)
        {
            auto& hook = *it;
            if (hook && (hook->pre_id == pre_id || hook->post_id == post_id))
            {
                if (hook->function)
                {
                    if (hook->pre_id != 0)
                    {
                        hook->function->UnregisterHook(hook->pre_id);
                    }
                    if (hook->post_id != 0)
                    {
                        hook->function->UnregisterHook(hook->post_id);
                    }
                }
                if (hook->ctx)
                {
                    if (!JS_IsUndefined(hook->pre_callback))
                    {
                        JS_FreeValue(hook->ctx, hook->pre_callback);
                    }
                    if (!JS_IsUndefined(hook->post_callback))
                    {
                        JS_FreeValue(hook->ctx, hook->post_callback);
                    }
                }
                m_ufunction_hooks.erase(it);
                return true;
            }
        }
        return false;
    }

    void JSMod::js_ufunction_hook_pre(Unreal::UnrealScriptFunctionCallableContext& context, void* custom_data)
    {
        auto* hook_data = static_cast<JSUFunctionHookData*>(custom_data);
        if (!hook_data || !hook_data->ctx || JS_IsUndefined(hook_data->pre_callback))
        {
            return;
        }

        JSContext* ctx = hook_data->ctx;

        // Get function parameters
        uint16_t return_value_offset = context.TheStack.CurrentNativeFunction()->GetReturnValueOffset();
        hook_data->has_return_value = return_value_offset != 0xFFFF;

        uint8_t num_unreal_params = context.TheStack.CurrentNativeFunction()->GetNumParms();
        if (hook_data->has_return_value)
        {
            --num_unreal_params;
        }

        // Build parameters array
        std::vector<void*> params;
        bool has_properties_to_process = hook_data->has_return_value || num_unreal_params > 0;
        if (has_properties_to_process && (context.TheStack.Locals() || context.TheStack.OutParms()))
        {
            for (Unreal::FProperty* func_prop : context.TheStack.CurrentNativeFunction()->ForEachProperty())
            {
                if (!func_prop->HasAnyPropertyFlags(Unreal::EPropertyFlags::CPF_Parm))
                {
                    continue;
                }
                if (func_prop->HasAnyPropertyFlags(Unreal::EPropertyFlags::CPF_ReturnParm))
                {
                    continue;
                }

                void* param_data = nullptr;
                if (func_prop->HasAnyPropertyFlags(Unreal::EPropertyFlags::CPF_OutParm) && !func_prop->HasAnyPropertyFlags(Unreal::EPropertyFlags::CPF_ConstParm))
                {
                    param_data = context.TheStack.OutParms();
                    for (auto out_param = context.TheStack.OutParms(); out_param; out_param = out_param->NextOutParm)
                    {
                        if (out_param->Property == func_prop)
                        {
                            param_data = out_param->PropAddr;
                            break;
                        }
                    }
                }
                else
                {
                    param_data = func_prop->ContainerPtrToValuePtr<void>(context.TheStack.Locals());
                }

                params.push_back(param_data);
            }
        }

        // Create JS arguments: (context_uobject, params_array, return_value_ptr)
        JSValue args[3];
        args[0] = JSUObject::create(ctx, context.Context);  // 'this' UObject

        // Create params array for JS
        JSValue js_params = JS_NewArray(ctx);
        for (size_t i = 0; i < params.size(); i++)
        {
            // For now, pass raw pointers as BigInt (can be extended to convert to proper JS types)
            JS_SetPropertyUint32(ctx, js_params, static_cast<uint32_t>(i), 
                JS_NewBigInt64(ctx, reinterpret_cast<int64_t>(params[i])));
        }
        args[1] = js_params;

        // Return value pointer
        args[2] = JS_NewBigInt64(ctx, reinterpret_cast<int64_t>(context.RESULT_DECL));

        // Call the JS callback
        JSValue result = JS_Call(ctx, hook_data->pre_callback, JS_UNDEFINED, 3, args);
        if (JS_IsException(result))
        {
            // Log exception
            JSValue exception = JS_GetException(ctx);
            const char* str = JS_ToCString(ctx, exception);
            if (str)
            {
                Output::send<LogLevel::Error>(STR("[UE4SSL.JavaScript] Pre-hook exception: {}\n"), 
                    std::wstring(str, str + strlen(str)));
                JS_FreeCString(ctx, str);
            }
            JS_FreeValue(ctx, exception);
        }

        // Cleanup
        JS_FreeValue(ctx, result);
        JS_FreeValue(ctx, args[0]);
        JS_FreeValue(ctx, args[1]);
        JS_FreeValue(ctx, args[2]);
    }

    void JSMod::js_ufunction_hook_post(Unreal::UnrealScriptFunctionCallableContext& context, void* custom_data)
    {
        auto* hook_data = static_cast<JSUFunctionHookData*>(custom_data);
        if (!hook_data || !hook_data->ctx || JS_IsUndefined(hook_data->post_callback))
        {
            return;
        }

        JSContext* ctx = hook_data->ctx;

        uint8_t num_unreal_params = context.TheStack.CurrentNativeFunction()->GetNumParms();
        if (hook_data->has_return_value)
        {
            --num_unreal_params;
        }

        // Build parameters array (same as pre-hook)
        std::vector<void*> params;
        bool has_properties_to_process = hook_data->has_return_value || num_unreal_params > 0;
        if (has_properties_to_process && (context.TheStack.Locals() || context.TheStack.OutParms()))
        {
            for (Unreal::FProperty* func_prop : context.TheStack.CurrentNativeFunction()->ForEachProperty())
            {
                if (!func_prop->HasAnyPropertyFlags(Unreal::EPropertyFlags::CPF_Parm))
                {
                    continue;
                }
                if (func_prop->HasAnyPropertyFlags(Unreal::EPropertyFlags::CPF_ReturnParm))
                {
                    continue;
                }

                void* param_data = nullptr;
                if (func_prop->HasAnyPropertyFlags(Unreal::EPropertyFlags::CPF_OutParm) && !func_prop->HasAnyPropertyFlags(Unreal::EPropertyFlags::CPF_ConstParm))
                {
                    param_data = context.TheStack.OutParms();
                    for (auto out_param = context.TheStack.OutParms(); out_param; out_param = out_param->NextOutParm)
                    {
                        if (out_param->Property == func_prop)
                        {
                            param_data = out_param->PropAddr;
                            break;
                        }
                    }
                }
                else
                {
                    param_data = func_prop->ContainerPtrToValuePtr<void>(context.TheStack.Locals());
                }

                params.push_back(param_data);
            }
        }

        // Create JS arguments: (context_uobject, params_array, return_value_ptr)
        JSValue args[3];
        args[0] = JSUObject::create(ctx, context.Context);  // 'this' UObject

        // Create params array for JS
        JSValue js_params = JS_NewArray(ctx);
        for (size_t i = 0; i < params.size(); i++)
        {
            JS_SetPropertyUint32(ctx, js_params, static_cast<uint32_t>(i), 
                JS_NewBigInt64(ctx, reinterpret_cast<int64_t>(params[i])));
        }
        args[1] = js_params;

        // Return value pointer
        args[2] = JS_NewBigInt64(ctx, reinterpret_cast<int64_t>(context.RESULT_DECL));

        // Call the JS callback
        JSValue result = JS_Call(ctx, hook_data->post_callback, JS_UNDEFINED, 3, args);
        if (JS_IsException(result))
        {
            // Log exception
            JSValue exception = JS_GetException(ctx);
            const char* str = JS_ToCString(ctx, exception);
            if (str)
            {
                Output::send<LogLevel::Error>(STR("[UE4SSL.JavaScript] Post-hook exception: {}\n"), 
                    std::wstring(str, str + strlen(str)));
                JS_FreeCString(ctx, str);
            }
            JS_FreeValue(ctx, exception);
        }

        // Cleanup
        JS_FreeValue(ctx, result);
        JS_FreeValue(ctx, args[0]);
        JS_FreeValue(ctx, args[1]);
        JS_FreeValue(ctx, args[2]);
    }

    // ============================================
    // Module Loader Implementation
    // ============================================

    static char* js_module_normalize(JSContext* ctx, const char* base_name, const char* name, void* opaque)
    {
        (void)opaque; // Unused, kept for API compatibility
        
        // Resolve module path relative to the importing script
        std::filesystem::path result;
        std::filesystem::path base(base_name);
        
        if (name[0] == '.' && (name[1] == '/' || (name[1] == '.' && name[2] == '/')))
        {
            // Relative path - resolve from base script's directory
            result = base.parent_path() / name;
        }
        else
        {
            // Module name without ./ prefix - also resolve from base script's directory
            result = base.parent_path() / name;
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
