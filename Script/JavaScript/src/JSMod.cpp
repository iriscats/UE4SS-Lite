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
#include <Unreal/FString.hpp>
#include <Unreal/Property/FStrProperty.hpp>
#include <Unreal/Property/FBoolProperty.hpp>
#include <Unreal/Property/FNumericProperty.hpp>
#include <Unreal/Property/FObjectProperty.hpp>
#include <Unreal/Property/FNameProperty.hpp>
#include <Input/Handler.hpp>

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
    static JSValue js_register_key_bind(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
    static JSValue js_call_function(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
    
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

        // Clean up key bindings
        {
            std::lock_guard<std::mutex> lock(m_key_bindings_mutex);
            for (auto& key_bind : m_key_bindings)
            {
                if (key_bind && key_bind->ctx)
                {
                    JS_FreeValue(key_bind->ctx, key_bind->callback);
                }
            }
            m_key_bindings.clear();
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

        // Recursion guard - prevent re-entry if a callback triggers tick() again
        if (m_in_tick)
        {
            return;
        }
        m_in_tick = true;

        // Process pending keybind callbacks (thread-safety: execute on event loop thread)
        {
            std::vector<KeyBindCallback*> to_execute;
            {
                std::lock_guard<std::mutex> lock(m_pending_keybind_mutex);
                to_execute.swap(m_pending_keybind_callbacks);
            }
            
            for (auto* key_bind : to_execute)
            {
                if (!key_bind || !key_bind->ctx) continue;
                
                // Recursion guard
                if (key_bind->is_executing) continue;
                key_bind->is_executing = true;
                
                // Call the JS callback on event loop thread
                JSValue result = JS_Call(key_bind->ctx, key_bind->callback, JS_UNDEFINED, 0, nullptr);
                if (JS_IsException(result))
                {
                    JSValue exception = JS_GetException(key_bind->ctx);
                    const char* str = JS_ToCString(key_bind->ctx, exception);
                    if (str)
                    {
                        Output::send<LogLevel::Error>(STR("[UE4SSL.JavaScript] KeyBind callback exception: {}\n"), 
                            std::wstring(str, str + strlen(str)));
                        JS_FreeCString(key_bind->ctx, str);
                    }
                    JS_FreeValue(key_bind->ctx, exception);
                }
                JS_FreeValue(key_bind->ctx, result);
                
                key_bind->is_executing = false;
            }
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
        
        m_in_tick = false;
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
        
        // Collect timer info we need (id, callback, is_interval, pointer to timer) to fire
        // We do this in two phases to avoid holding the lock during JS_Call
        struct TimerToFire {
            int32_t id;
            JSValue callback;
            bool is_interval;
            TimerCallback* timer_ptr;  // Pointer to original timer for is_executing flag
        };
        std::vector<TimerToFire> to_fire;
        
        {
            std::lock_guard<std::mutex> lock(m_timers_mutex);
            
            for (auto& timer : m_timers)
            {
                if (timer.cancelled) continue;
                if (timer.is_executing) continue;  // Skip if already executing (recursion guard)
                if (current_time < timer.trigger_time) continue;
                
                // Dup the callback so we have our own reference
                to_fire.push_back({timer.id, JS_DupValue(timer.ctx, timer.callback), timer.is_interval, &timer});
                
                // Set executing flag
                timer.is_executing = true;
                
                if (timer.is_interval)
                {
                    // Reschedule for next interval
                    timer.trigger_time = current_time + timer.interval;
                }
                else
                {
                    // Mark setTimeout for cleanup (but don't free callback yet - we're using the dup'd version)
                    timer.cancelled = true;
                }
            }
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
            
            // Clear executing flag
            if (item.timer_ptr)
            {
                item.timer_ptr->is_executing = false;
            }
        }
        
        // Clean up cancelled timers (separate pass to avoid issues with timer_ptr)
        {
            std::lock_guard<std::mutex> lock(m_timers_mutex);
            for (auto& timer : m_timers)
            {
                if (timer.cancelled && !timer.is_interval && timer.ctx)
                {
                    JS_FreeValue(timer.ctx, timer.callback);
                    timer.ctx = nullptr;  // Mark as freed
                }
            }
            m_timers.erase(
                std::remove_if(m_timers.begin(), m_timers.end(),
                    [](const TimerCallback& t) { return t.cancelled && !t.is_interval; }),
                m_timers.end()
            );
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

        // Set max stack size (8MB) - prevents "Maximum call stack size exceeded" errors
        JS_SetMaxStackSize(m_runtime, 8 * 1024 * 1024);

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
        JS_SetPropertyStr(ctx, global, "RegisterKeyBind",
            JS_NewCFunction(ctx, js_register_key_bind, "RegisterKeyBind", 3));
        JS_SetPropertyStr(ctx, global, "CallFunction",
            JS_NewCFunction(ctx, js_call_function, "CallFunction", 3));
        
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
        void* obj = JSUObject::get_uobject(ctx, argv[0]);
        if (obj)
        {
            // Cast to UFunction
            function = static_cast<Unreal::UFunction*>(static_cast<Unreal::UObject*>(obj));
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
    // Key Binding Functions Implementation
    // ============================================

    // Helper to convert key name string to Input::Key enum
    static Input::Key string_to_key(const char* key_name)
    {
        // Letters A-Z
        if (strlen(key_name) == 1 && key_name[0] >= 'A' && key_name[0] <= 'Z')
        {
            return static_cast<Input::Key>(key_name[0]);
        }
        if (strlen(key_name) == 1 && key_name[0] >= 'a' && key_name[0] <= 'z')
        {
            return static_cast<Input::Key>(key_name[0] - 32);  // Convert to uppercase
        }
        
        // Numbers 0-9
        if (strlen(key_name) == 1 && key_name[0] >= '0' && key_name[0] <= '9')
        {
            return static_cast<Input::Key>(key_name[0]);
        }
        
        // Function keys F1-F12
        if (key_name[0] == 'F' || key_name[0] == 'f')
        {
            int num = atoi(key_name + 1);
            if (num >= 1 && num <= 12)
            {
                return static_cast<Input::Key>(Input::Key::F1 + num - 1);
            }
        }
        
        // Special keys
        if (_stricmp(key_name, "ESCAPE") == 0 || _stricmp(key_name, "ESC") == 0) return Input::Key::ESCAPE;
        if (_stricmp(key_name, "SPACE") == 0) return Input::Key::SPACE;
        if (_stricmp(key_name, "ENTER") == 0 || _stricmp(key_name, "RETURN") == 0) return Input::Key::RETURN;
        if (_stricmp(key_name, "TAB") == 0) return Input::Key::TAB;
        if (_stricmp(key_name, "BACKSPACE") == 0) return Input::Key::BACKSPACE;
        if (_stricmp(key_name, "DELETE") == 0 || _stricmp(key_name, "DEL") == 0) return Input::Key::DEL;
        if (_stricmp(key_name, "INSERT") == 0 || _stricmp(key_name, "INS") == 0) return Input::Key::INS;
        if (_stricmp(key_name, "HOME") == 0) return Input::Key::HOME;
        if (_stricmp(key_name, "END") == 0) return Input::Key::END;
        if (_stricmp(key_name, "PAGEUP") == 0) return Input::Key::PAGE_UP;
        if (_stricmp(key_name, "PAGEDOWN") == 0) return Input::Key::PAGE_DOWN;
        if (_stricmp(key_name, "UP") == 0) return Input::Key::UP_ARROW;
        if (_stricmp(key_name, "DOWN") == 0) return Input::Key::DOWN_ARROW;
        if (_stricmp(key_name, "LEFT") == 0) return Input::Key::LEFT_ARROW;
        if (_stricmp(key_name, "RIGHT") == 0) return Input::Key::RIGHT_ARROW;
        if (_stricmp(key_name, "NUMLOCK") == 0) return Input::Key::NUM_LOCK;
        if (_stricmp(key_name, "CAPSLOCK") == 0) return Input::Key::CAPS_LOCK;
        if (_stricmp(key_name, "SCROLLLOCK") == 0) return Input::Key::SCROLL_LOCK;
        if (_stricmp(key_name, "PAUSE") == 0) return Input::Key::PAUSE;
        if (_stricmp(key_name, "PRINTSCREEN") == 0) return Input::Key::PRINT_SCREEN;
        
        // Numpad
        if (_strnicmp(key_name, "NUM", 3) == 0 && strlen(key_name) == 4)
        {
            char c = key_name[3];
            if (c >= '0' && c <= '9')
            {
                return static_cast<Input::Key>(Input::Key::NUM_ZERO + (c - '0'));
            }
        }
        
        // Default: return 0 (invalid)
        return static_cast<Input::Key>(0);
    }

    // RegisterKeyBind(key, callback, [modifiers])
    // key: string like "F1", "A", "SPACE", etc.
    // callback: function to call when key is pressed
    // modifiers: optional object { ctrl: bool, shift: bool, alt: bool }
    static JSValue js_register_key_bind(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv)
    {
        if (argc < 2)
        {
            return JS_ThrowTypeError(ctx, "RegisterKeyBind requires at least 2 arguments: key, callback [, modifiers]");
        }

        // Get key name
        const char* key_name = JS_ToCString(ctx, argv[0]);
        if (!key_name)
        {
            return JS_ThrowTypeError(ctx, "First argument must be a key name string");
        }

        // Convert key name to key code
        Input::Key key = string_to_key(key_name);
        JS_FreeCString(ctx, key_name);

        if (key == 0)
        {
            return JS_ThrowTypeError(ctx, "Invalid key name");
        }

        // Check callback is a function
        if (!JS_IsFunction(ctx, argv[1]))
        {
            return JS_ThrowTypeError(ctx, "Second argument must be a callback function");
        }

        // Parse modifiers
        bool with_ctrl = false;
        bool with_shift = false;
        bool with_alt = false;

        if (argc >= 3 && JS_IsObject(argv[2]))
        {
            JSValue ctrl_val = JS_GetPropertyStr(ctx, argv[2], "ctrl");
            JSValue shift_val = JS_GetPropertyStr(ctx, argv[2], "shift");
            JSValue alt_val = JS_GetPropertyStr(ctx, argv[2], "alt");

            if (JS_IsBool(ctrl_val)) with_ctrl = JS_ToBool(ctx, ctrl_val);
            if (JS_IsBool(shift_val)) with_shift = JS_ToBool(ctx, shift_val);
            if (JS_IsBool(alt_val)) with_alt = JS_ToBool(ctx, alt_val);

            JS_FreeValue(ctx, ctrl_val);
            JS_FreeValue(ctx, shift_val);
            JS_FreeValue(ctx, alt_val);
        }

        try
        {
            JSMod* mod = get_js_mod(ctx);
            if (!mod)
            {
                return JS_ThrowInternalError(ctx, "Could not get JSMod instance");
            }

            bool success = mod->register_key_bind(ctx, static_cast<uint8_t>(key), argv[1], with_ctrl, with_shift, with_alt);
            
            return JS_NewBool(ctx, success);
        }
        catch (const std::exception& e)
        {
            Output::send<LogLevel::Error>(STR("[UE4SSL.JavaScript] RegisterKeyBind exception: {}\n"), 
                std::wstring(e.what(), e.what() + strlen(e.what())));
            return JS_ThrowInternalError(ctx, "RegisterKeyBind failed due to exception");
        }
        catch (...)
        {
            Output::send<LogLevel::Error>(STR("[UE4SSL.JavaScript] RegisterKeyBind unknown exception\n"));
            return JS_ThrowInternalError(ctx, "RegisterKeyBind failed due to unknown exception");
        }
    }

    // ============================================
    // Function Call Implementation
    // ============================================

    // CallFunction(object, functionName, ...args) - Call a UFunction on an object
    // For string parameters, pass JS strings directly
    static JSValue js_call_function(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv)
    {
        if (argc < 2)
        {
            return JS_ThrowTypeError(ctx, "CallFunction requires at least 2 arguments: object, functionName [, ...args]");
        }

        // Get UObject from first argument
        void* obj_ptr = JSUObject::get_uobject(ctx, argv[0]);
        if (!obj_ptr)
        {
            return JS_ThrowTypeError(ctx, "First argument must be a UObject");
        }
        Unreal::UObject* object = static_cast<Unreal::UObject*>(obj_ptr);

        // Get function name
        const char* func_name = JS_ToCString(ctx, argv[1]);
        if (!func_name)
        {
            return JS_ThrowTypeError(ctx, "Second argument must be a function name string");
        }
        std::wstring wide_func_name(func_name, func_name + strlen(func_name));
        JS_FreeCString(ctx, func_name);

        try
        {
            // Find the function on the object
            Unreal::UFunction* function = object->GetFunctionByNameInChain(wide_func_name.c_str());
            if (!function)
            {
                return JS_ThrowReferenceError(ctx, "Function not found on object");
            }

            // Get function parameter info
            int32_t params_size = function->GetParmsSize();
            
            // Allocate parameters memory
            void* params_memory = nullptr;
            if (params_size > 0)
            {
                params_memory = calloc(1, params_size);
                if (!params_memory)
                {
                    return JS_ThrowInternalError(ctx, "Failed to allocate params memory");
                }
            }

            // Fill in parameters from JS arguments
            int js_arg_index = 2;  // Start after object and function name
            for (Unreal::FProperty* prop : function->ForEachProperty())
            {
                if (!prop->HasAnyPropertyFlags(Unreal::EPropertyFlags::CPF_Parm))
                {
                    continue;
                }
                if (prop->HasAnyPropertyFlags(Unreal::EPropertyFlags::CPF_ReturnParm))
                {
                    continue;
                }

                if (js_arg_index >= argc)
                {
                    break;  // No more JS arguments
                }

                void* prop_addr = prop->ContainerPtrToValuePtr<void>(params_memory);
                
                // Handle different property types
                if (prop->IsA<Unreal::FStrProperty>())
                {
                    // String parameter
                    const char* str_val = JS_ToCString(ctx, argv[js_arg_index]);
                    if (str_val)
                    {
                        std::wstring wide_str(str_val, str_val + strlen(str_val));
                        JS_FreeCString(ctx, str_val);
                        
                        // Construct FString in place
                        new (prop_addr) Unreal::FString(wide_str.c_str());
                    }
                }
                else if (prop->IsA<Unreal::FBoolProperty>())
                {
                    Unreal::FBoolProperty* bool_prop = static_cast<Unreal::FBoolProperty*>(prop);
                    bool_prop->SetPropertyValue(prop_addr, JS_ToBool(ctx, argv[js_arg_index]));
                }
                else if (prop->IsA<Unreal::FNumericProperty>())
                {
                    Unreal::FNumericProperty* num_prop = static_cast<Unreal::FNumericProperty*>(prop);
                    if (num_prop->IsFloatingPoint())
                    {
                        double val;
                        JS_ToFloat64(ctx, &val, argv[js_arg_index]);
                        num_prop->SetFloatingPointPropertyValue(prop_addr, val);
                    }
                    else if (num_prop->IsInteger())
                    {
                        int64_t val;
                        JS_ToInt64(ctx, &val, argv[js_arg_index]);
                        num_prop->SetIntPropertyValue(prop_addr, val);
                    }
                }
                else if (prop->IsA<Unreal::FObjectProperty>())
                {
                    void* arg_obj = JSUObject::get_uobject(ctx, argv[js_arg_index]);
                    if (arg_obj)
                    {
                        *static_cast<Unreal::UObject**>(prop_addr) = static_cast<Unreal::UObject*>(arg_obj);
                    }
                }

                js_arg_index++;
            }

            // Call the function
            object->ProcessEvent(function, params_memory);

            // Clean up
            if (params_memory)
            {
                free(params_memory);
            }

            Output::send<LogLevel::Normal>(STR("[UE4SSL.JavaScript] Called function {}\n"), wide_func_name);
            return JS_TRUE;
        }
        catch (const std::exception& e)
        {
            Output::send<LogLevel::Error>(STR("[UE4SSL.JavaScript] CallFunction exception: {}\n"), 
                std::wstring(e.what(), e.what() + strlen(e.what())));
            return JS_ThrowInternalError(ctx, "CallFunction failed due to exception");
        }
        catch (...)
        {
            Output::send<LogLevel::Error>(STR("[UE4SSL.JavaScript] CallFunction unknown exception\n"));
            return JS_ThrowInternalError(ctx, "CallFunction failed due to unknown exception");
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

    auto JSMod::register_key_bind(JSContext* ctx, uint8_t key, JSValue callback, 
                                  bool with_ctrl, bool with_shift, bool with_alt) -> bool
    {
        // Create key bind data
        auto key_bind = std::make_unique<KeyBindCallback>();
        key_bind->owner = this;
        key_bind->ctx = ctx;
        key_bind->callback = JS_DupValue(ctx, callback);
        key_bind->key = key;
        key_bind->with_ctrl = with_ctrl;
        key_bind->with_shift = with_shift;
        key_bind->with_alt = with_alt;

        // Get raw pointer before moving
        KeyBindCallback* raw_key_bind = key_bind.get();

        // Store in our list
        {
            std::lock_guard<std::mutex> lock(m_key_bindings_mutex);
            m_key_bindings.push_back(std::move(key_bind));
        }

        // Build modifier keys array
        Input::Handler::ModifierKeyArray modifier_keys{};
        size_t mod_idx = 0;
        if (with_ctrl) modifier_keys[mod_idx++] = Input::ModifierKey::CONTROL;
        if (with_shift) modifier_keys[mod_idx++] = Input::ModifierKey::SHIFT;
        if (with_alt) modifier_keys[mod_idx++] = Input::ModifierKey::ALT;

        // Register with UE4SS input system
        auto& program = UE4SSProgram::get_program();
        
        // Lambda to queue keybind callback for execution on event loop thread (thread-safety fix)
        auto queue_keybind_callback = [this](KeyBindCallback* key_bind) {
            if (!key_bind || !key_bind->ctx) return;
            
            // Add to pending queue (thread-safe)
            std::lock_guard<std::mutex> lock(m_pending_keybind_mutex);
            m_pending_keybind_callbacks.push_back(key_bind);
        };

        if (with_ctrl || with_shift || with_alt)
        {
            program.register_keydown_event(
                static_cast<Input::Key>(key),
                modifier_keys,
                [raw_key_bind, queue_keybind_callback]() {
                    queue_keybind_callback(raw_key_bind);
                }
            );
        }
        else
        {
            program.register_keydown_event(
                static_cast<Input::Key>(key),
                [raw_key_bind, queue_keybind_callback]() {
                    queue_keybind_callback(raw_key_bind);
                }
            );
        }

        Output::send<LogLevel::Normal>(STR("[UE4SSL.JavaScript] Registered key bind for key 0x{:X} (ctrl={}, shift={}, alt={})\n"), 
            key, with_ctrl, with_shift, with_alt);
        
        return true;
    }

    // Helper function to convert FProperty value to JSValue
    static JSValue property_to_jsvalue(JSContext* ctx, Unreal::FProperty* prop, void* data)
    {
        if (!prop || !data)
        {
            return JS_NULL;
        }

        // Check for FStrProperty (FString)
        if (prop->IsA<Unreal::FStrProperty>())
        {
            Unreal::FString* fstr = static_cast<Unreal::FString*>(data);
            if (fstr && fstr->GetCharArray())
            {
                const wchar_t* wstr = fstr->GetCharArray();
                // Convert wide string to UTF-8
                std::wstring wide_str(wstr);
                std::string utf8_str(wide_str.begin(), wide_str.end());
                return JS_NewString(ctx, utf8_str.c_str());
            }
            return JS_NewString(ctx, "");
        }

        // Check for FNameProperty (FName)
        if (prop->IsA<Unreal::FNameProperty>())
        {
            Unreal::FName* fname = static_cast<Unreal::FName*>(data);
            if (fname)
            {
                std::wstring wide_str = fname->ToString();
                std::string utf8_str(wide_str.begin(), wide_str.end());
                return JS_NewString(ctx, utf8_str.c_str());
            }
            return JS_NewString(ctx, "");
        }

        // Check for FBoolProperty
        if (prop->IsA<Unreal::FBoolProperty>())
        {
            Unreal::FBoolProperty* bool_prop = static_cast<Unreal::FBoolProperty*>(prop);
            bool value = bool_prop->GetPropertyValue(data);
            return JS_NewBool(ctx, value);
        }

        // Check for numeric properties (int, float, etc.)
        if (prop->IsA<Unreal::FNumericProperty>())
        {
            Unreal::FNumericProperty* num_prop = static_cast<Unreal::FNumericProperty*>(prop);
            if (num_prop->IsFloatingPoint())
            {
                double value = num_prop->GetFloatingPointPropertyValue(data);
                return JS_NewFloat64(ctx, value);
            }
            else if (num_prop->IsInteger())
            {
                int64_t value = num_prop->GetSignedIntPropertyValue(data);
                return JS_NewInt64(ctx, value);
            }
        }

        // Check for FObjectProperty (UObject*)
        if (prop->IsA<Unreal::FObjectProperty>())
        {
            Unreal::UObject** obj_ptr = static_cast<Unreal::UObject**>(data);
            if (obj_ptr && *obj_ptr)
            {
                return JSUObject::create(ctx, *obj_ptr);
            }
            return JS_NULL;
        }

        // Default: return raw pointer as BigInt
        return JS_NewBigInt64(ctx, reinterpret_cast<int64_t>(data));
    }

    void JSMod::js_ufunction_hook_pre(Unreal::UnrealScriptFunctionCallableContext& context, void* custom_data)
    {
        auto* hook_data = static_cast<JSUFunctionHookData*>(custom_data);
        if (!hook_data || !hook_data->ctx || JS_IsUndefined(hook_data->pre_callback))
        {
            return;
        }

        // Recursion guard - prevent re-entry if the hook callback triggers the same function
        if (hook_data->is_executing)
        {
            return;
        }
        hook_data->is_executing = true;

        JSContext* ctx = hook_data->ctx;

        // Get function parameters
        uint16_t return_value_offset = context.TheStack.CurrentNativeFunction()->GetReturnValueOffset();
        hook_data->has_return_value = return_value_offset != 0xFFFF;

        uint8_t num_unreal_params = context.TheStack.CurrentNativeFunction()->GetNumParms();
        if (hook_data->has_return_value)
        {
            --num_unreal_params;
        }

        // Build parameters array with type conversion
        std::vector<std::pair<Unreal::FProperty*, void*>> params;
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

                params.push_back({func_prop, param_data});
            }
        }

        // Create JS arguments: (context_uobject, params_array, return_value_ptr)
        JSValue args[3];
        args[0] = JSUObject::create(ctx, context.Context);  // 'this' UObject

        // Create params array for JS with automatic type conversion
        JSValue js_params = JS_NewArray(ctx);
        for (size_t i = 0; i < params.size(); i++)
        {
            JSValue js_param = property_to_jsvalue(ctx, params[i].first, params[i].second);
            JS_SetPropertyUint32(ctx, js_params, static_cast<uint32_t>(i), js_param);
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
        
        // Clear recursion guard
        hook_data->is_executing = false;
    }

    void JSMod::js_ufunction_hook_post(Unreal::UnrealScriptFunctionCallableContext& context, void* custom_data)
    {
        auto* hook_data = static_cast<JSUFunctionHookData*>(custom_data);
        if (!hook_data || !hook_data->ctx || JS_IsUndefined(hook_data->post_callback))
        {
            return;
        }

        // Note: We share is_executing with pre_hook, so if pre_hook is still executing, skip post_hook too
        // This prevents issues when the hooked function is called recursively
        if (hook_data->is_executing)
        {
            return;
        }
        hook_data->is_executing = true;

        JSContext* ctx = hook_data->ctx;

        uint8_t num_unreal_params = context.TheStack.CurrentNativeFunction()->GetNumParms();
        if (hook_data->has_return_value)
        {
            --num_unreal_params;
        }

        // Build parameters array with type info (same as pre-hook)
        std::vector<std::pair<Unreal::FProperty*, void*>> params;
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

                params.push_back({func_prop, param_data});
            }
        }

        // Create JS arguments: (context_uobject, params_array, return_value_ptr)
        JSValue args[3];
        args[0] = JSUObject::create(ctx, context.Context);  // 'this' UObject

        // Create params array for JS with automatic type conversion
        JSValue js_params = JS_NewArray(ctx);
        for (size_t i = 0; i < params.size(); i++)
        {
            JSValue js_param = property_to_jsvalue(ctx, params[i].first, params[i].second);
            JS_SetPropertyUint32(ctx, js_params, static_cast<uint32_t>(i), js_param);
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
        
        // Clear recursion guard
        hook_data->is_executing = false;
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
