namespace UE4SSL.Plugins;

public class PluginReloadedEventArgs(PluginLoader loader) : EventArgs
{
    public PluginLoader Loader { get; } = loader;
}

public delegate void PluginReloadedEventHandler(object sender, PluginReloadedEventArgs eventArgs);