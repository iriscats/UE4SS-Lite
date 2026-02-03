#pragma once

#include <filesystem>
#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <mutex>
#include <thread>
#include <memory>

// QuickJS headers
extern "C" {
#include "quickjs.h"
}

// Forward declarations for Unreal types
namespace RC::Unreal
{
    class UFunction;
    class UObject;
    class UnrealScriptFunctionCallableContext;
    using CallbackId = int32_t;
}

namespace RC::JSScript
{
    class JSMod;  // Forward declaration

    /**
     * JSMod - JavaScript scripting engine manager based on QuickJS
     * 
     * This class manages the QuickJS runtime and context, handles script loading
     * and execution, and provides the bridge between JavaScript and UE4.
     */
    class JSMod
    {
    public:
        // JavaScript UFunction Hook data
        struct JSUFunctionHookData
        {
            JSMod* owner;                      // Pointer to JSMod instance
            JSContext* ctx;                    // JS context
            JSValue pre_callback;              // JS pre-hook callback function
            JSValue post_callback;             // JS post-hook callback function
            Unreal::UFunction* function;       // The hooked UFunction
            Unreal::CallbackId pre_id;         // Pre-hook callback ID
            Unreal::CallbackId post_id;        // Post-hook callback ID
            bool has_return_value;             // Does the function have a return value
        };

        // Legacy callback data for hooks (kept for compatibility)
        struct HookCallback
        {
            JSContext* ctx;
            JSValue callback;  // Reference to JS callback function
            int32_t ref_id;    // Registry reference ID
        };
        
        // Timer data for setTimeout/setInterval
        struct TimerCallback
        {
            JSContext* ctx;
            JSValue callback;       // Reference to JS callback function
            int32_t id;             // Timer ID
            double trigger_time;    // When to trigger (in seconds since start)
            double interval;        // Interval for setInterval (0 for setTimeout)
            bool is_interval;       // true = setInterval, false = setTimeout
            bool cancelled;         // Timer was cancelled
        };

    public:
        // UFunction hook management (public for access from global functions)
        std::vector<std::unique_ptr<JSUFunctionHookData>> m_ufunction_hooks;
        std::mutex m_ufunction_hooks_mutex;

        // Legacy hook management (public for access from global functions)
        std::vector<HookCallback> m_hook_callbacks;
        std::mutex m_hooks_mutex;
        
        // Timer management (public for access from global functions)
        std::vector<TimerCallback> m_timers;
        std::mutex m_timers_mutex;
        int32_t m_next_timer_id{1};
        double m_start_time{0.0};
        
        // Module cache (public for access from module loader)
        std::unordered_map<std::string, bool> m_loaded_modules;

    private:
        std::filesystem::path m_mods_directory;
        JSRuntime* m_runtime{nullptr};
        JSContext* m_main_ctx{nullptr};
        
        bool m_initialized{false};

    public:
        JSMod();
        ~JSMod();

        // Lifecycle
        auto start() -> bool;           // Full start (init + load scripts) - for backward compatibility
        auto init_engine() -> bool;     // Initialize engine only (safe before UE init)
        auto load_scripts() -> void;    // Load and execute scripts (call after UE init)
        auto stop() -> void;
        auto tick() -> void;  // Called every frame
        
        // Script execution
        auto load_and_execute_script(const std::filesystem::path& script_path) -> bool;
        auto execute_string(const std::string& code, const std::string& filename = "<eval>") -> bool;
        
        // Timer management
        auto add_timer(JSContext* ctx, JSValue callback, double delay_ms, bool is_interval) -> int32_t;
        auto cancel_timer(int32_t id) -> bool;
        auto process_timers() -> void;
        
        // UFunction hook management
        auto register_ufunction_hook(JSContext* ctx, Unreal::UFunction* function, 
                                     JSValue pre_callback, JSValue post_callback) -> std::pair<int32_t, int32_t>;
        auto unregister_ufunction_hook(Unreal::CallbackId pre_id, Unreal::CallbackId post_id) -> bool;

        // Static hook callbacks for UE4SS hook system
        static void js_ufunction_hook_pre(Unreal::UnrealScriptFunctionCallableContext& context, void* custom_data);
        static void js_ufunction_hook_post(Unreal::UnrealScriptFunctionCallableContext& context, void* custom_data);
        
        // Getters
        [[nodiscard]] auto get_runtime() const -> JSRuntime* { return m_runtime; }
        [[nodiscard]] auto get_main_context() const -> JSContext* { return m_main_ctx; }
        [[nodiscard]] auto get_mods_directory() const -> const std::filesystem::path& { return m_mods_directory; }
        [[nodiscard]] auto is_initialized() const -> bool { return m_initialized; }
        [[nodiscard]] auto get_current_time() const -> double;

    private:
        // Initialization
        auto init_runtime() -> bool;
        auto init_context() -> bool;
        auto setup_global_functions(JSContext* ctx) -> void;
        auto setup_classes(JSContext* ctx) -> void;
        auto setup_module_loader() -> void;
        
        // Script discovery
        auto find_scripts() -> std::vector<std::filesystem::path>;
        
        // Error handling
        auto log_exception(JSContext* ctx) -> void;
    };

} // namespace RC::JSScript
