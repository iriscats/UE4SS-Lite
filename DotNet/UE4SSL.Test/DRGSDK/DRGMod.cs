using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using UE4SSL.Framework;

namespace UE4SSL.Test.DRGSDK
{
    public abstract class DRGMod
    {
        public static Action? OnInitCaveCallback;

        public static Action? OnInitRigSpaceCallback;


        public unsafe static void OnInitCave(IntPtr @this, void*[] @params, void* retValue) {
            OnInitCaveCallback?.Invoke();
        }


        public unsafe static void OnInitRigSpace(IntPtr @this, void*[] @params, void* retValue) {
            OnInitRigSpaceCallback?.Invoke();
        }


        private unsafe void OnExecuteScript(IntPtr @this, void*[] @params, void* retValue)
        {
        }


        private unsafe static void OnPost(IntPtr @this, void*[] @params, void* retValue)
        {
        }

        private static bool isHook = false;

        private void Hook()
        {

            unsafe
            {
                Thread.Sleep(100);

                if (isHook)
                {
                    return;
                }

                var initCave = ObjectReference.Find("/Game/ModIntegration/MI_SpawnMods.MI_SpawnMods_C:OnInitCave");
                if (initCave is null)
                {
                    return;
                }

                OnInitRigSpaceCallback?.Invoke();

                Hooking.HookUFunction(initCave, OnInitCave, OnPost);

                var initRigSapce = ObjectReference.Find("/Game/ModIntegration/MI_SpawnMods.MI_SpawnMods_C:OnInitRigSpace");
                if (initRigSapce is null)
                {
                    return;
                }
                Hooking.HookUFunction(initRigSapce, OnInitRigSpace, OnPost);

                var onExecuteScript = ObjectReference.Find("/Game/ModIntegration/MI_SpawnMods.MI_SpawnMods_C:OnExecuteScript");
                if (onExecuteScript is null)
                {
                    return;
                }
                Hooking.HookUFunction(onExecuteScript, OnExecuteScript, OnPost);

                isHook = true;
            }

        }


        public void Update()
        {
            Hook();
        }

        public void StartMod()
        {
            Debug.Log(LogLevel.Warning, "StartMod");
        }

        public void UnrealInit()
        {
            Debug.Log(LogLevel.Warning, "UnrealInit");
        }

        public void ProgramStart()
        {
            Debug.Log(LogLevel.Warning, "ProgramStart");
        }


    }
}
