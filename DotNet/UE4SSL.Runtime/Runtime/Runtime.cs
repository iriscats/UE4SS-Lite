using System;
using System.Reflection;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;
using System.Runtime.Loader;
using UE4SSL.Plugins;

/*
 *  Unreal Engine .NET 6 integration
 *  Copyright (c) 2021 Stanislav Denisov
 *
 *  Permission is hereby granted, free of charge, to any person obtaining a copy
 *  of this software and associated documentation files (the "Software"), to deal
 *  in the Software without restriction, including without limitation the rights
 *  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 *  copies of the Software, and to permit persons to whom the Software is
 *  furnished to do so, subject to the following conditions:
 *
 *  The above copyright notice and this permission notice shall be included in all
 *  copies or substantial portions of the Software.
 *
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 *  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 *  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 *  SOFTWARE.
 */

namespace UE4SSL.Runtime;

internal enum LogLevel
{
    Default,
    Normal,
    Verbose,
    Warning,
    Error,
}


internal enum ArgumentType
{
    None,
    Single,
    Integer,
    Pointer,
    Callback
}

internal enum CallbackType
{
    ActorOverlapDelegate,
    ActorHitDelegate,
    ActorCursorDelegate,
    ActorKeyDelegate,
    ComponentOverlapDelegate,
    ComponentHitDelegate,
    ComponentCursorDelegate,
    ComponentKeyDelegate,
    CharacterLandedDelegate
}

internal enum CommandType
{
    Initialize = 1,
    LoadAssemblies = 2,
    UnloadAssemblies = 3,
    Find = 4,
    Execute = 5
}

[StructLayout(LayoutKind.Explicit, Size = 16)]
internal unsafe struct Callback
{
    [FieldOffset(0)]
    internal IntPtr* parameters;
    [FieldOffset(8)]
    internal CallbackType type;
}

[StructLayout(LayoutKind.Explicit, Size = 24)]
internal struct Argument
{
    [FieldOffset(0)]
    internal float single;
    [FieldOffset(0)]
    internal uint integer;
    [FieldOffset(0)]
    internal IntPtr pointer;
    [FieldOffset(0)]
    internal Callback callback;
    [FieldOffset(16)]
    internal ArgumentType type;
}

[StructLayout(LayoutKind.Explicit, Size = 40)]
internal unsafe struct Command
{
    // Initialize
    [FieldOffset(0)]
    internal IntPtr* buffer;
    //[FieldOffset(8)]
    //internal int checksum;
    // Find
    [FieldOffset(0)]
    internal IntPtr method;
    [FieldOffset(8)]
    internal int optional;
    // Execute
    [FieldOffset(0)]
    internal IntPtr function;
    [FieldOffset(8)]
    internal Argument value;

    [FieldOffset(16)]
    internal CommandType type;
}

internal sealed class Plugin
{
    internal PluginLoader loader = null;
    internal Assembly assembly = null;
    internal List<Dictionary<int, IntPtr>?> userFunctions;
}

internal sealed class AssembliesContextManager
{
    internal AssemblyLoadContext assembliesContext;

    [MethodImpl(MethodImplOptions.NoInlining)]
    internal WeakReference CreateAssembliesContext()
    {
        assembliesContext = new("UnrealEngine", true);

        return new(assembliesContext, trackResurrection: true);
    }

    [MethodImpl(MethodImplOptions.NoInlining)]
    internal void UnloadAssembliesContext() => assembliesContext?.Unload();
}

public static unsafe class Core
{
    private static AssembliesContextManager assembliesContextManager;
    private static WeakReference assembliesContextWeakReference;
    private static List<Plugin> plugins;
    private static IntPtr sharedEvents;
    private static List<IntPtr> userEvents;

    private static delegate* unmanaged[Cdecl]<LogLevel, string, void> Log;

    public static void StartMod()
    {
        Console.WriteLine("Runtime.StartMod");
        foreach (var userEvent in userEvents.Where(userEvent => ((IntPtr*)userEvent)[0] is not 0))
        {
            ((delegate* unmanaged[Cdecl]<void>)((IntPtr*)userEvent)[0])();
        }
    }

    public static void StopMod()
    {
        Console.WriteLine("Runtime.StopMod");
        foreach (var userEvent in userEvents.Where(userEvent => ((IntPtr*)userEvent)[1] is not 0))
        {
            ((delegate* unmanaged[Cdecl]<void>)((IntPtr*)userEvent)[1])();
        }
    }

    public static void ProgramStart()
    {
        Console.WriteLine("Runtime.ProgramStart");
        foreach (var userEvent in userEvents.Where(userEvent => ((IntPtr*)userEvent)[2] is not 0))
        {
            ((delegate* unmanaged[Cdecl]<void>)((IntPtr*)userEvent)[2])();
        }
    }

    public static void UnrealInit()
    {
        Console.WriteLine("Runtime.UnrealInit");

        foreach (var userEvent in userEvents.Where(userEvent => ((IntPtr*)userEvent)[3] is not 0))
        {
            ((delegate* unmanaged[Cdecl]<void>)((IntPtr*)userEvent)[3])();
        }
    }

    public static void Update()
    {
        foreach (var userEvent in userEvents.Where(userEvent => ((IntPtr*)userEvent)[4] is not 0))
        {
            ((delegate* unmanaged[Cdecl]<void>)((IntPtr*)userEvent)[4])();
        }
    }


    [UnmanagedCallersOnly]
    internal static IntPtr ManagedInitialize(IntPtr* buffer)
    {
        Console.WriteLine("ManagedInitialize");
        try
        {
            assembliesContextManager = new();
            assembliesContextWeakReference = assembliesContextManager.CreateAssembliesContext();

            int position = 0;


            unchecked
            {
                int head = 0;
                IntPtr* runtimeFunctions = (IntPtr*)buffer[position++];
                Log = (delegate* unmanaged[Cdecl]<LogLevel, string, void>)runtimeFunctions[head];
            }

            sharedEvents = buffer[position];

            Console.WriteLine("ManagedInitialize Success");
        }

        catch (Exception exception)
        {
            Console.WriteLine(exception.ToString());
            Log(LogLevel.Error, "Runtime initialization failed\r\n" + exception);
        }

        return new(0xF);
    }


    [UnmanagedCallersOnly]
    internal static IntPtr ManagedLoadAssemblies()
    {
        Log(LogLevel.Default, "ManagedCommand LoadAssemblies");
        try
        {
            userEvents = new List<IntPtr>();
            plugins = new List<Plugin>();
            const string frameworkAssemblyName = "UE4SSL.Framework";

            string assemblyPath = Assembly.GetExecutingAssembly().Location;
            string baseDirectory = Path.GetDirectoryName(assemblyPath)!;
            string modsDirectory = Path.Combine(baseDirectory, "csmods");
            if (!Directory.Exists(modsDirectory))
            {
                Log(LogLevel.Warning, $"Mods directory not found: {modsDirectory}");
                return 0;
            }

            string[] folders = new string[] { modsDirectory };
            foreach (string folder in folders)
            {
                IEnumerable<string> assemblies = Directory.EnumerateFiles(folder, "*.dll", SearchOption.AllDirectories);

                foreach (string assembly in assemblies)
                {
                    Log(LogLevel.Default, "Framework loaded start load " + assembly);
                    AssemblyName? assemblyName = null;
                    try
                    {
                        assemblyName = AssemblyName.GetAssemblyName(assembly);
                    }

                    catch (BadImageFormatException)
                    {
                        continue;
                    }

                    if (assemblyName?.Name == frameworkAssemblyName)
                    {
                        continue;
                    }

                    var currentPlugin = new Plugin();
                    currentPlugin.loader = PluginLoader.CreateFromAssemblyFile(assembly, config =>
                    {
                        config.DefaultContext = assembliesContextManager.assembliesContext;
                        config.IsUnloadable = true;
                        config.LoadInMemory = false;

                    });


                    currentPlugin.assembly = currentPlugin.loader.LoadAssemblyFromPath(assembly);
                    Log(LogLevel.Default, "Framework LoadAssemblyFromPath success" + assembly);

                    currentPlugin.userFunctions = new List<Dictionary<int, IntPtr>?>();
                    AssemblyName[] referencedAssemblies = currentPlugin.assembly.GetReferencedAssemblies();

                    Log(LogLevel.Default, "Framework GetReferencedAssemblies success" + assembly);
                    foreach (AssemblyName referencedAssembly in referencedAssemblies)
                    {
                        if (referencedAssembly.Name == frameworkAssemblyName)
                        {
                            Log(LogLevel.Default, "start loaded frameworkAssemblyName");
                            Assembly framework = currentPlugin.loader.LoadAssembly(referencedAssembly);
                            Log(LogLevel.Default, "loaded frameworkAssemblyName success");

                            using (assembliesContextManager.assembliesContext.EnterContextualReflection())
                            {
                                Log(LogLevel.Default, "start loaded EnterContextualReflection");

                                Type? sharedClass = framework.GetType(frameworkAssemblyName + ".Shared");
                                if (sharedClass is null)
                                {
                                    Log(LogLevel.Default, "framework.GetType is null");
                                    continue;
                                }

                                IntPtr events = Marshal.AllocHGlobal(sizeof(IntPtr) * 5);
                                Unsafe.InitBlockUnaligned((byte*)events, 0, (uint)(sizeof(IntPtr) * 5));
                                Log(LogLevel.Default, "InitBlockUnaligned");

                                var userFunc = (Dictionary<int, IntPtr>)sharedClass
                                     .GetMethod("Load", BindingFlags.NonPublic | BindingFlags.Static)
                                     .Invoke(null,
                                         [events, currentPlugin.assembly]);

                                currentPlugin.userFunctions.Add(userFunc);
                                Log(LogLevel.Default, "userFunctions.Add");

                                userEvents.Add(events);
                                Log(LogLevel.Default, "userEvents.Add");

                                sharedClass?.GetMethod("Load", BindingFlags.NonPublic | BindingFlags.Static).Invoke(null, [sharedEvents, Assembly.GetExecutingAssembly()]);

                                plugins.Add(currentPlugin);

                                Log(LogLevel.Default, "Framework loaded succesfuly for " + assembly);
                            }
                            Log(LogLevel.Default, "leave loaded EnterContextualReflection");
                        }
                    }
                }
            }
        }

        catch (Exception exception)
        {
            Log(LogLevel.Error, "Loading of assemblies failed\r\n" + exception);
            UnloadAssemblies();
        }

        Log(LogLevel.Default, "ManagedCommand LoadAssemblies success");
        return default;
    }


    [UnmanagedCallersOnly]
    internal static IntPtr ManagedExecute(IntPtr function)
    {
        try
        {
            ((delegate* unmanaged[Cdecl]<void>)function)();
        }
        catch (Exception exception)
        {
            try
            {
                Log(LogLevel.Error, exception.ToString());
            }

            catch (FileNotFoundException fileNotFoundException)
            {
                Log(LogLevel.Error,
                    "One of the project dependencies is missed! Please, publish the project instead of building it\r\n" +
                    fileNotFoundException);
            }
        }

        return default;
    }


    [UnmanagedCallersOnly]
    internal static IntPtr ManagedUnloadAssemblies()
    {
        return UnloadAssemblies();
    }


    private static IntPtr UnloadAssemblies()
    {
        try
        {
            foreach (var plugin in plugins)
            {
                plugin.loader.Dispose();
            }

            plugins.Clear();

            assembliesContextManager.UnloadAssembliesContext();
            assembliesContextManager = null;

            if (assembliesContextWeakReference.IsAlive)
            {
                GC.Collect(GC.MaxGeneration, GCCollectionMode.Forced);
                GC.WaitForPendingFinalizers();
            }

            assembliesContextManager = new();
            assembliesContextWeakReference = assembliesContextManager.CreateAssembliesContext();
        }
        catch (Exception exception)
        {
            Log(LogLevel.Error, "Unloading of assemblies failed\r\n" + exception);
        }

        return new(0xF);
    }


}