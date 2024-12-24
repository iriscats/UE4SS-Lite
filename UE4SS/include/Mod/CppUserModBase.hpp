#pragma once

#include <memory>
#include <vector>

#include <Common.hpp>
#include <File/Macros.hpp>
#include <Input/Handler.hpp>

#include <String/StringType.hpp>

namespace RC
{
    struct ModMetadata
    {
        const StringType ModName{};
        const StringType ModVersion{};
        const StringType ModDescription{};
        const StringType ModAuthors{};
        const StringType ModIntendedSDKVersion{};
    };

    // When making C++ mods, keep in mind that they will break if UE4SS and the mod don't use the same C Runtime library version
    // This includes them being compiled in different configurations (Debug/Release).
    class CppUserModBase
    {
      protected:
        //std::vector<std::shared_ptr<GUI::GUITab>> GUITabs{};

      public:
        StringType ModName{};
        StringType ModVersion{};
        StringType ModDescription{};
        StringType ModAuthors{};
        StringType ModIntendedSDKVersion{};

      public:
        RC_UE4SS_API CppUserModBase();
        RC_UE4SS_API virtual ~CppUserModBase();

      public:
        RC_UE4SS_API virtual auto on_update() -> void
        {
        }

        // The 'Unreal' module has been initialized.
        // Before this fires, you cannot use anything in the 'Unreal' namespace.
        RC_UE4SS_API virtual auto on_unreal_init() -> void
        {
        }

        // The UI module has been initialized.
        // This is where you need to use the 'UE4SS_ENABLE_IMGUI' macro if you want to utilize the imgui context of UE4SS.
        RC_UE4SS_API virtual auto on_ui_init() -> void
        {
        }

        RC_UE4SS_API virtual auto on_program_start() -> void
        {
        }

        RC_UE4SS_API virtual auto on_dll_load(StringViewType dll_name) -> void
        {
        }

        RC_UE4SS_API virtual auto render_tab() -> void{};

      protected:
        //RC_UE4SS_API auto register_tab(StringViewType tab_name, GUI::GUITab::RenderFunctionType) -> void;
        RC_UE4SS_API auto register_keydown_event(Input::Key, const Input::EventCallbackCallable&, uint8_t custom_data = 0) -> void;
        RC_UE4SS_API auto register_keydown_event(Input::Key, const Input::Handler::ModifierKeyArray&, const Input::EventCallbackCallable&, uint8_t custom_data = 0)
                -> void;
    };
} // namespace RC
