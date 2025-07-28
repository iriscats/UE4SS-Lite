#pragma once

#include <Mod/CppUserModBase.hpp>
#include <Mod/Mod.hpp>





namespace RC
{
    class CSharpMod : public Mod
    {
        private:
            typedef CppUserModBase* (*start_type)();
            typedef void (*uninstall_type)(CppUserModBase*);

            DotNetLibrary::Runtime* m_runtime;

        public:
            CSharpMod(UE4SSProgram&, StringType&& mod_name, StringType&& mod_path);
            CSharpMod(CppMod&) = delete;
            CSharpMod(CppMod&&) = delete;
            ~CSharpMod() override;

        public:
            auto start_mod() -> void override;
            auto uninstall() -> void override;
            auto fire_unreal_init() -> void override;
            auto fire_ui_init() -> void override;
            auto fire_program_start() -> void override;
            auto fire_update() -> void override;
            auto fire_dll_load(StringViewType dll_name) -> void;

    }
}


