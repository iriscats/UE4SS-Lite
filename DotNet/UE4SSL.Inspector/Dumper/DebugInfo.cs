using System.Text;
using UnrealSharp;


namespace UE4SSL.Inspector.Dumper
{
    internal class DebugInfo
    {
        public static void DumpGNames()
        {
            var testObj = new UEObject(0);
            var sb = new StringBuilder();
            var i = 0;
            while (true)
            {
                var name = UEObject.GetName(i);

                if (name == "badIndex")
                {
                    if ((i & 0xffff) > 0xff00)
                    {
                        i += 0x10000 - (i % 0x10000);
                        continue;
                    }
                    break;
                }
                sb.AppendLine("[" + i + " | " + (i).ToString("X") + "] " + name);
                i += name.Length / 2 + name.Length % 2 + 1;
            }
            DirectoryInfo directoryInfo = Directory.CreateDirectory(UnrealEngine.Memory.Process.ProcessName);
            File.WriteAllText(UnrealEngine.Memory.Process.ProcessName + @"\GNamesDump.txt", sb.ToString());
        }
    }
}
