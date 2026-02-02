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
    bool m_scripts_loaded = false;

public:
    JSScriptMod() : CppUserModBase()
    {
        ModName = STR("UE4SSL.JavaScript");
        ModVersion = STR("1.0.0");
        ModDescription = STR("JavaScript scripting support via QuickJS engine");
        ModAuthors = STR("UE4SS Community");
        
        Output::send<LogLevel::Normal>(STR("[UE4SSL.JavaScript] Initializing JavaScript engine...\n"));
        
        // Only initialize the runtime, but don't load scripts yet
        // Scripts will be loaded in on_unreal_init when UE is ready
        m_js_mod = std::make_unique<JSScript::JSMod>();
        
        if (m_js_mod->init_engine())
        {
            Output::send<LogLevel::Normal>(STR("[UE4SSL.JavaScript] JavaScript engine initialized successfully\n"));
            Output::send<LogLevel::Normal>(STR("[UE4SSL.JavaScript] Waiting for on_unreal_init to load scripts...\n"));
        }
        else
        {
            Output::send<LogLevel::Error>(STR("[UE4SSL.JavaScript] Failed to initialize JavaScript engine\n"));
        }
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
     * This is the safe time to execute scripts that access UE objects
     */
    auto on_unreal_init() -> void override
    {
        Output::send<LogLevel::Normal>(STR("[UE4SSL.JavaScript] on_unreal_init called - Unreal is ready\n"));
        
        // Now it's safe to load and execute scripts
        if (m_js_mod && m_js_mod->is_initialized() && !m_scripts_loaded)
        {
            m_js_mod->load_scripts();
            m_scripts_loaded = true;
        }
    }

    /**
     * Called every frame
     * Used to process async JavaScript tasks and timers
     */
    auto on_update() -> void override
    {
        if (m_js_mod && m_js_mod->is_initialized())
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
