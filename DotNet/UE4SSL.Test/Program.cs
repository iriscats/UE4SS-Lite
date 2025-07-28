namespace UE4SSL.Test
{
    public static class Main
    {
        public static void StartMod()
        {
            Debug.Log(LogLevel.Warning, "StartMod");
        }

        public static void UnrealInit()
        {
            Debug.Log(LogLevel.Warning, "UnrealInit");

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


        public static void ProgramStart()
        {
            Debug.Log(LogLevel.Warning, "ProgramStart");
        }
    }
}