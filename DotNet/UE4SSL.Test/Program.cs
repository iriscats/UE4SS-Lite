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
            var gameMode = gameFunctionLibrary?.GetFSDGameState(worldContext!);
            if (gameMode is not null)
            {
               gameMode.PostGameMessage("Hello World");
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