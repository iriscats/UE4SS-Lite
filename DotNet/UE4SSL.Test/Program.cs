using System.Runtime.InteropServices;
using System.Text.Json;
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

        public new void Update()
        {
            base.Update();

            if (Hotkeys.IsPressed(Keys.K))
            {
                Debug.Log(LogLevel.Warning, "IsPressed K");
                TestPostGameMessage("Hello World 你好世界");
            }

            if (Hotkeys.IsPressed(Keys.L))
            {
                Debug.Log(LogLevel.Warning, "IsPressed L");
                var result = Task.Run(static () => GetWeather()).Result;
                TestPostGameMessage(result);
            }
        }

        public void OnInitCave()
        {
            Debug.Log(LogLevel.Warning, "OnInitCave");
        }


        public void OnInitRigSpace()
        {
            Debug.Log(LogLevel.Warning, "OnInitRigSpace");
        }

        public void TestPostGameMessage(string msg)
        {
            var gameFunctionLibrary = GameFunctionLibrary.GetInstance();
            var worldContext = Utils.GetWorldContext();
            var gameMode = gameFunctionLibrary?.GetFSDGameState(worldContext!);
            if (gameMode is not null)
            {
                gameMode.PostGameMessage(msg);
            }
        }


        public async static Task<string> GetWeather()
        {
            var cityId = 101240101;

            using var client = new HttpClient();
            var response = await client.GetAsync("https://aider.meizu.com/app/weather/listWeather?cityIds=" + cityId);
            response.EnsureSuccessStatusCode();

            var json = await response.Content.ReadAsStringAsync();
            string today = DateTime.Now.ToString("yyyy-MM-dd");

            using var doc = JsonDocument.Parse(json);
            var root = doc.RootElement;

            var weathers = root
                .GetProperty("value")[0]
                .GetProperty("weathers")
                .EnumerateArray();

            var todayWeather = weathers
                .FirstOrDefault(w => w.GetProperty("date").GetString() == today);

            string ret = "";
            if (todayWeather.ValueKind != JsonValueKind.Undefined)
            {
                string weather = todayWeather.GetProperty("weather").GetString();
                string tempDay = todayWeather.GetProperty("temp_day_c").GetString();
                string tempNight = todayWeather.GetProperty("temp_night_c").GetString();

                ret = $"今天({today}) {weather}，白天 {tempDay}°C，夜间 {tempNight}°C";
            }

            return ret;
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