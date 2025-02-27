#pragma once

#include <vector>

#include <Unreal/Core/Windows/MinimalWindowsApi.hpp>

#include <Mod/CppUserModBase.hpp>
#include <Mod/Mod.hpp>

#include <String/StringType.hpp>

namespace RC
{
    class CppMod : public Mod
    {
      private:
        typedef CppUserModBase* (*start_type)();
        typedef void (*uninstall_type)(CppUserModBase*);

      private:
        std::filesystem::path m_dlls_path;

        Unreal::Windows::HMODULE m_main_dll_module = NULL;
        void* m_dlls_path_cookie = NULL;
        start_type m_start_mod_func = nullptr;
        uninstall_type m_uninstall_mod_func = nullptr;

        CppUserModBase* m_mod = nullptr;

      public:
        CppMod(UE4SSProgram&, StringType&& mod_name, StringType&& mod_path);
        CppMod(CppMod&) = delete;
        CppMod(CppMod&&) = delete;
        ~CppMod() override;

      public:
        auto start_mod() -> void override;
        auto uninstall() -> void override;
        auto fire_unreal_init() -> void override;
        auto fire_ui_init() -> void override;
        auto fire_program_start() -> void override;
        auto fire_update() -> void override;
        auto fire_dll_load(StringViewType dll_name) -> void;
    };
} // namespace RC
