#include <Mod/CppUserModBase.hpp>
#include <UE4SSProgram.hpp>
#include <DotNetLibrary.hpp>

#include <Windows.h>


using namespace RC::Unreal;


namespace RC
{
    CSharpMod::CSharpMod(UE4SSProgram& program, StringType&& mod_name, StringType&& mod_path) : Mod(program, std::move(mod_name), std::move(mod_path))
    {

        wchar_t path[MAX_PATH];
        HMODULE hm = NULL;

        if (GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | 
        GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        reinterpret_cast<LPCWSTR>(&empty), &hm) == 0)
        {
            Output::send<LogLevel::Error>(STR("GetModuleHandle failed, error = {}\n"), GetLastError());
            return;
        }
        if (GetModuleFileName(hm, path, sizeof(path)) == 0)
        {
            Output::send<LogLevel::Error>(STR("GetModuleFileName failed, error = {}\n"), GetLastError());
            return;
        }

        const auto module_path = std::filesystem::path(path).parent_path();
        if (!LoadLibrary((std::wstring(module_path.c_str()) + L"UE4SSL.CSharp.dll").c_str()))
        {
            Output::send<LogLevel::Error>(STR("LoadLibrary failed, error = {}\n"), GetLastError());
            return;
        }

        std::filesystem::path runtime_path(module_path);
        runtime_path = runtime_path.parent_path();
        
        m_runtime = new DotNetLibrary::Runtime(runtime_path);
        m_runtime->initialize();

    }


    ~CSharpMod::CSharpMod() override
    {
        m_runtime->unload_assemblies();
        delete m_runtime;
    }

    auto CSharpMod::start_mod() -> void override
    {
        m_runtime->start_mod();
    }

    auto CSharpMod::uninstall() -> void override
    {
        m_runtime->uninstall();
    }

    auto CSharpMod::fire_unreal_init() -> void override
    {
        m_runtime->fire_unreal_init();
    }

    auto CSharpMod::fire_ui_init() -> void override
    {
        m_runtime->fire_ui_init();
    }

    auto CSharpMod::fire_program_start() -> void override
    {
        m_runtime->fire_program_start();
    }

    auto CSharpMod::fire_update() -> void override
    {
        m_runtime->fire_update();
    }

    auto CSharpMod::fire_dll_load(StringViewType dll_name) -> void override
    {
        m_runtime->fire_dll_load(dll_name);
    }

}

