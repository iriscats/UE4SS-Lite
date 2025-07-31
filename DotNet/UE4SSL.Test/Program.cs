using UE4SSL.Framework;

namespace UE4SSL.Test
{

    public static class Main
    {

        public unsafe static void OnInitCave(IntPtr @this, void*[] @params, void* retValue)
        {

        }


        public unsafe static void OnInitRigSpace(IntPtr @this, void*[] @params, void* retValue)
        {
            var gameFunctionLibrary = GameFunctionLibrary.GetInstance();
            var worldContext = Utils.GetWorldContext();
            var gameMode = gameFunctionLibrary?.GetFSDGameMode(worldContext!);
            if (gameMode is not null)
            {
                var difficultyManager = gameMode.GetDifficultyManager();
                var currentDifficulty = difficultyManager.GetCurrentDifficulty();
                foreach (var item in currentDifficulty.EnemyDamageResistance)
                {
                    Debug.Log(LogLevel.Error, item.ToString());
                }
            }

        }

        public unsafe static void OnPost(IntPtr @this, void*[] @params, void* retValue)
        {

        }

        static bool isHook = false;

        private static void Hook() {

            unsafe
            {
                if (isHook)
                {
                    return;
                }

                isHook = true;
                var initCave = ObjectReference.Find("/Game/ModIntegration/MI_SpawnMods.MI_SpawnMods_C:OnInitCave");
                if (initCave is null)
                {
                    return;
                }
                Debug.Log(LogLevel.Warning, "initCave");
                Hooking.HookUFunction(initCave, OnInitCave, OnPost);

                var initRigSapce = ObjectReference.Find("/Game/ModIntegration/MI_SpawnMods.MI_SpawnMods_C:OnInitRigSpace");
                if (initRigSapce is null)
                {
                    return;
                }
                Hooking.HookUFunction(initRigSapce, OnInitRigSpace, OnPost);
            }

        }


        public static void Update()
        {
            //Debug.Log(LogLevel.Warning, "Update");
            Hook();
        }

        public static void StartMod()
        {
            Debug.Log(LogLevel.Warning, "StartMod");
        }

        public static void UnrealInit()
        {
            Debug.Log(LogLevel.Warning, "UnrealInit");
        }


        public static void ProgramStart()
        {
            Debug.Log(LogLevel.Warning, "ProgramStart");

     
        }
    }
}