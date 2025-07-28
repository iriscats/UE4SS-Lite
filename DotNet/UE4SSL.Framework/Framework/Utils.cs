using UE4SSDotNetFramework.Framework;

namespace UE4SSL.Framework;

public static class Utils
{
    public static ObjectReference? GetWorldContext()
    {
        var engineRef = ObjectReference.FindFirstOf("Engine");
        return engineRef?.GetObjectReference("GameViewport");
    }
}