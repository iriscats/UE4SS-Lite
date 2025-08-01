namespace UE4SSL.Test.DRGSDK;

using System.Runtime.InteropServices;
using UE4SSL.Framework;

public class ObjectBase<TObjType> : ObjectReference where TObjType : unmanaged
{
    public TObjType Instance => Marshal.PtrToStructure<TObjType>(Pointer);

    public ObjectBase(nint pointer)
    {
        Pointer = pointer;
    }
}