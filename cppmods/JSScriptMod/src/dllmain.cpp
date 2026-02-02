#include <memory>
#include <string>

#include <Mod/CppUserModBase.hpp>
#include <UE4SSProgram.hpp>

#include "JSMod.hpp"

/**
 * JSScriptMod - C++ mod entry point for JavaScript scripting support
 * 
 * This mod provides JavaScript scripting capabilities to UE4SS using QuickJS,
 * similar to the built-in Lua scripting support.
 */
class JSScriptMod : public RC::CppUserModBase
{
private:
    std::unique_ptr<RC::JSScript::JSMod> m_js_mod;

public:
    JSScriptMod() : CppUserModBase()
    {
        ModName = STR("JSScript");
        ModVersion = STR("1.0.0");
        ModDescription = STR("JavaScript scripting support via QuickJS engine");
        ModAuthors = STR("UE4SS Community");
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
     * This is where we initialize the JavaScript engine
     */
    auto on_unreal_init() -> void override
    {
        RC::Output::send<RC::LogLevel::Normal>(STR("[JSScript] Initializing JavaScript engine...\n"));
        
        m_js_mod = std::make_unique<RC::JSScript::JSMod>();
        
        if (m_js_mod->start())
        {
            RC::Output::send<RC::LogLevel::Normal>(STR("[JSScript] JavaScript engine initialized successfully\n"));
        }
        else
        {
            RC::Output::send<RC::LogLevel::Error>(STR("[JSScript] Failed to initialize JavaScript engine\n"));
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
        RC::Output::send<RC::LogLevel::Normal>(STR("[JSScript] JSScript mod loaded\n"));
    }
};

// DLL export functions
#ifdef _WIN32
#define JS_SCRIPT_MOD_API __declspec(dllexport)
#else
#define JS_SCRIPT_MOD_API __attribute__((visibility("default")))
#endif

extern "C"
{
    JS_SCRIPT_MOD_API RC::CppUserModBase* start_mod()
    {
        return new JSScriptMod();
    }

    JS_SCRIPT_MOD_API void uninstall_mod(RC::CppUserModBase* mod)
    {
        delete mod;
    }
}
