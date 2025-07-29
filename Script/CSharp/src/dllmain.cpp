#include <Mod/CppUserModBase.hpp>
#include <UE4SSProgram.hpp>
#include <DotNetLibrary.hpp>

#include <Windows.h>

using namespace RC;
using namespace RC::Unreal;

class CSharpLoaderProxy : public CppUserModBase
{
public:
    DotNetLibrary::Runtime* m_runtime;
    
    CSharpLoaderProxy()
    {
        ModName = STR("UE4SSL.CSharp");
        ModVersion = STR("1.0");
        ModDescription = STR("");
        ModAuthors = STR("");
        
        wchar_t path[MAX_PATH]{};
        HMODULE hm = NULL;
        if (GetModuleFileName(hm, path, sizeof(path)) == 0)
        {
            Output::send<LogLevel::Error>(STR("GetModuleFileName failed, error = {}\n"), GetLastError());
            return;
        }

        std::filesystem::path runtime_path(path);
        m_runtime = new DotNetLibrary::Runtime(runtime_path.parent_path());
        m_runtime->initialize();
    }

    ~CSharpLoaderProxy() override
    {
        m_runtime->unload_assemblies();
        delete m_runtime;
    }

    auto on_program_start() -> void override
    {
        m_runtime->fire_program_start();
    }

    auto on_unreal_init() -> void override
    {
        m_runtime->fire_unreal_init();
    }

    auto on_update() -> void override
    {
        m_runtime->fire_update();
    }
};

#define CSHARPLOADERPROXY_API __declspec(dllexport)
extern "C"
{
    CSHARPLOADERPROXY_API CppUserModBase* start_mod()
    {
        return new CSharpLoaderProxy();
    }

    CSHARPLOADERPROXY_API void uninstall_mod(CppUserModBase* mod)
    {
        delete mod;
    }
}