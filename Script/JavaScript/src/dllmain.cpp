#include <memory>
#include <string>

#include <Mod/CppUserModBase.hpp>
#include <UE4SSProgram.hpp>

#include "JSMod.hpp"

using namespace RC;

/**
 * JSScriptMod - C++ mod entry point for JavaScript scripting support
 * 
 * This mod provides JavaScript scripting capabilities to UE4SS using QuickJS,
 * similar to the built-in Lua scripting support.
 */
class JSScriptMod : public CppUserModBase
{
private:
    std::unique_ptr<JSScript::JSMod> m_js_mod;
    bool m_unreal_ready = false;      // Set when UE is initialized
    bool m_engine_initialized = false; // Set when JS engine is initialized on event loop thread
    bool m_scripts_loaded = false;     // Set when scripts are loaded

public:
    JSScriptMod() : CppUserModBase()
    {
        ModName = STR("UE4SSL.JavaScript");
        ModVersion = STR("1.0.0");
        ModDescription = STR("JavaScript scripting support via QuickJS engine");
        ModAuthors = STR("UE4SS Community");
        
        Output::send<LogLevel::Normal>(STR("[UE4SSL.JavaScript] Mod loaded, waiting for event loop thread...\n"));
        
        // Create JSMod instance but don't initialize yet
        // All JS operations will happen on the event loop thread for thread safety
        m_js_mod = std::make_unique<JSScript::JSMod>();
    }

    ~JSScriptMod() override
    {
        if (m_js_mod)
        {
            m_js_mod->stop();
        }
    }

    /**
     * Called when UE4 is fully initialized
     * Just set a flag - actual initialization happens on event loop thread
     */
    auto on_unreal_init() -> void override
    {
        Output::send<LogLevel::Normal>(STR("[UE4SSL.JavaScript] on_unreal_init called - Unreal is ready\n"));
        m_unreal_ready = true;
    }

    /**
     * Called every frame on the event loop thread
     * All JS operations happen here for thread safety
     */
    auto on_update() -> void override
    {
        if (!m_js_mod) return;
        
        // Initialize engine on first update (event loop thread)
        if (!m_engine_initialized)
        {
            Output::send<LogLevel::Normal>(STR("[UE4SSL.JavaScript] Initializing JS engine on event loop thread...\n"));
            if (m_js_mod->init_engine())
            {
                m_engine_initialized = true;
                Output::send<LogLevel::Normal>(STR("[UE4SSL.JavaScript] JS engine initialized on event loop thread\n"));
            }
            else
            {
                Output::send<LogLevel::Error>(STR("[UE4SSL.JavaScript] Failed to initialize JS engine\n"));
            }
            return;
        }
        
        // Load scripts once UE is ready (still on event loop thread)
        if (m_unreal_ready && !m_scripts_loaded && m_js_mod->is_initialized())
        {
            Output::send<LogLevel::Normal>(STR("[UE4SSL.JavaScript] Loading scripts on event loop thread...\n"));
            m_js_mod->load_scripts();
            m_scripts_loaded = true;
        }
        
        // Normal tick
        if (m_js_mod->is_initialized())
        {
            m_js_mod->tick();
        }
    }

    /**
     * Called when the program starts (before Unreal init)
     */
    auto on_program_start() -> void override
    {
        Output::send<LogLevel::Normal>(STR("[UE4SSL.JavaScript] on_program_start called\n"));
    }
};

// DLL export functions
#define JS_SCRIPT_MOD_API __declspec(dllexport)

extern "C"
{
    JS_SCRIPT_MOD_API CppUserModBase* start_mod()
    {
        Output::send<LogLevel::Normal>(STR("[UE4SSL.JavaScript] start_mod called\n"));
        return new JSScriptMod();
    }

    JS_SCRIPT_MOD_API void uninstall_mod(CppUserModBase* mod)
    {
        Output::send<LogLevel::Normal>(STR("[UE4SSL.JavaScript] uninstall_mod called\n"));
        delete mod;
    }
}
