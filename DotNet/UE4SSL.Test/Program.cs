using System.Runtime.InteropServices;
using UE4SSL.Framework;
using UE4SSL.Test.DRGSDK;

namespace UE4SSL.Test
{

    class MyMod : DRGMod {

        public MyMod()
        {
            OnInitRigSpaceCallback += OnInitRigSpace;
            OnInitCaveCallback += OnInitCave;
        }


        public void OnInitCave()
        {
            Debug.Log(LogLevel.Warning, "OnInitCave");
        }


        public void OnInitRigSpace()
        {
            Debug.Log(LogLevel.Warning, "OnInitRigSpace");

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
    }


    public static class Main
    {
        private static MyMod mod = new MyMod();

        public static void Update()
        {
            mod.Update();
        }

        public static void StartMod()
        {
            mod.StartMod();
        }

    }
}