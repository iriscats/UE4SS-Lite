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

        public static void Update() 
        {
            Debug.Log(LogLevel.Warning, "Update");

        }

        public static void StartMod()
        {
            Debug.Log(LogLevel.Warning, "StartMod");
        }

        public static void UnrealInit()
        {
            Debug.Log(LogLevel.Warning, "UnrealInit");

            unsafe {
                var initCave = ObjectReference.Find("/Game/ModIntegration/MI_SpawnMods.MI_SpawnMods_C");
                Hooking.HookUFunction(initCave, OnInitCave, null);

                var initRigSapce = ObjectReference.Find("/Game/ModIntegration/MI_SpawnMods.MI_SpawnMods_C");
                Hooking.HookUFunction(initRigSapce, OnInitRigSpace, null);
            }
        }


        public static void ProgramStart()
        {
            Debug.Log(LogLevel.Warning, "ProgramStart");

        }
    }
}