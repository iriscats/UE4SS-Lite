#pragma once

#include <filesystem>
#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <mutex>
#include <thread>

// QuickJS headers
extern "C" {
#include "quickjs.h"
}

namespace RC::JSScript
{
    /**
     * JSMod - JavaScript scripting engine manager based on QuickJS
     * 
     * This class manages the QuickJS runtime and context, handles script loading
     * and execution, and provides the bridge between JavaScript and UE4.
     */
    class JSMod
    {
    public:
        // Callback data for hooks
        struct HookCallback
        {
            JSContext* ctx;
            JSValue callback;  // Reference to JS callback function
            int32_t ref_id;    // Registry reference ID
        };

    public:
        // Hook management (public for access from global functions)
        std::vector<HookCallback> m_hook_callbacks;
        std::mutex m_hooks_mutex;
        
        // Module cache (public for access from module loader)
        std::unordered_map<std::string, bool> m_loaded_modules;

    private:
        std::filesystem::path m_scripts_path;
        JSRuntime* m_runtime{nullptr};
        JSContext* m_main_ctx{nullptr};
        
        bool m_initialized{false};

    public:
        JSMod();
        ~JSMod();

        // Lifecycle
        auto start() -> bool;
        auto stop() -> void;
        auto tick() -> void;  // Called every frame
        
        // Script execution
        auto load_and_execute_script(const std::filesystem::path& script_path) -> bool;
        auto execute_string(const std::string& code, const std::string& filename = "<eval>") -> bool;
        
        // Getters
        [[nodiscard]] auto get_runtime() const -> JSRuntime* { return m_runtime; }
        [[nodiscard]] auto get_main_context() const -> JSContext* { return m_main_ctx; }
        [[nodiscard]] auto get_scripts_path() const -> const std::filesystem::path& { return m_scripts_path; }
        [[nodiscard]] auto is_initialized() const -> bool { return m_initialized; }

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
