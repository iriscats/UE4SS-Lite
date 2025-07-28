using System.Numerics;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;
using System.Text;

namespace UE4SSL.Framework 
{
	internal static class ArrayPool {
		[ThreadStatic]
		private static byte[] stringBuffer;

		public static byte[] GetStringBuffer() {
			if (stringBuffer == null)
				stringBuffer = GC.AllocateUninitializedArray<byte>(8192, pinned: true);

			Array.Clear(stringBuffer, 0, stringBuffer.Length);

			return stringBuffer;
		}
	}

	internal static class Collector {
		[ThreadStatic]
		private static List<object> references;

		public static IntPtr GetFunctionPointer(Delegate reference) {
			if (references == null)
				references = new();

			references.Add(reference);

			return Marshal.GetFunctionPointerForDelegate(reference);
		}
	}

	internal static class Extensions {
		[MethodImpl(MethodImplOptions.AggressiveInlining)]
		internal static T GetOrAdd<S, T>(this IDictionary<S, T> dictionary, S key, Func<T> valueCreator) => dictionary.TryGetValue(key, out var value) ? value : dictionary[key] = valueCreator();

		[MethodImpl(MethodImplOptions.AggressiveInlining)]
		internal static byte[] StringToBytes(this string value) {
			if (value != null)
				return Encoding.UTF8.GetBytes(value);

			return null;
		}

		[MethodImpl(MethodImplOptions.AggressiveInlining)]
		internal static string BytesToString(this byte[] buffer) {
			int end;

			for (end = 0; end < buffer.Length && buffer[end] != 0; end++);

			unsafe {
				fixed (byte* pinnedBuffer = buffer) {
					return new((sbyte*)pinnedBuffer, 0, end);
				}
			}
		}
	}
	
	// Public

	public partial struct UnArray
	{
		public Type GetDataType()
		{
			switch (Type)
			{
				case PropertyType.ObjectProperty:
					return typeof(ObjectReference);
				case PropertyType.Int8Property:
					return typeof(sbyte);
				case PropertyType.Int16Property:
					return typeof(short);
				case PropertyType.IntProperty:
					return typeof(int);
				case PropertyType.Int64Property:
					return typeof(long);
				case PropertyType.ByteProperty:
					return typeof(byte);
				case PropertyType.UInt16Property:
					return typeof(ushort);
				case PropertyType.UInt32Property:
					return typeof(uint);
				case PropertyType.UInt64Property:
					return typeof(ulong);
				case PropertyType.StructProperty:
					return typeof(StructReference);
				case PropertyType.ArrayProperty:
					return typeof(UnArray);
				case PropertyType.FloatProperty:
					return typeof(float);
				case PropertyType.DoubleProperty:
					return typeof(double);
				case PropertyType.BoolProperty:
					return typeof(bool);
				case PropertyType.EnumProperty:
					return typeof(Enum);
				case PropertyType.WeakObjectProperty:
					return typeof(WeakObjectPtr);
				case PropertyType.NameProperty:
				case PropertyType.StrProperty:
				case PropertyType.TextProperty:
					return typeof(string);
			}

			return typeof(object);
		}
		
		public T[] DataToArray<T>()
		{
			var size = Marshal.SizeOf(typeof(T));
			var managedArray = new T[Length];

			for (int i = 0; i < (int)Length; i++)
			{
				IntPtr ins = new IntPtr(Data.ToInt64() + i * size);
				managedArray[i] = Marshal.PtrToStructure<T>(ins);
			}

			return managedArray;
		}

		public void ArrayToData<T>(T[] array)
		{
			long longPtr = Data.ToInt64(); // Must work both on x86 and x64
			for (int I = 0; I < array.Length; I++)
			{
				IntPtr rectPtr = new IntPtr(longPtr);
				Marshal.StructureToPtr(array[I], rectPtr, false); // You do not need to erase struct in this case
				longPtr += Marshal.SizeOf(typeof(T));
			}
		}
	}
	
	/// <summary>
	/// Defines the log level for an output log message
	/// </summary>
	public enum LogLevel
	{
		Default,
		Normal,
		Verbose,
		Warning,
		Error,
	}

	public static unsafe partial class Runtime
	{
		public static void AddUnrealInitCallback(delegate* unmanaged[Cdecl]<void> callback)
		{
			AddUnrealInitCallbackInternal(callback);
		}
		
		public static void AddUpdateCallback(delegate* unmanaged[Cdecl]<void> callback)
		{
			AddUpdateCallbackInternal(callback);
		}
	}
	
	/// <summary>
	/// Functionality for debugging
	/// </summary>
	public static partial class Debug
	{
		private enum Protection {
			ReadWrite = 0x04,
		}
    
		[DllImport("kernel32.dll", SetLastError = true)]
		[return: MarshalAs(UnmanagedType.Bool)]
		private static extern bool VirtualProtect(IntPtr lpAddress, uint dwSize,
			Protection flNewProtect, out Protection lpflOldProtect);

		private static byte[] ToByteArray(object? value, int maxLength)
		{
			int rawsize = Marshal.SizeOf(value);
			byte[] rawdata = new byte[rawsize];
			GCHandle handle =
				GCHandle.Alloc(rawdata,
					GCHandleType.Pinned);
			Marshal.StructureToPtr(value,
				handle.AddrOfPinnedObject(),
				false);
			handle.Free();
			if (maxLength < rawdata.Length) {
				byte[] temp = new byte[maxLength];
				Array.Copy(rawdata, temp, maxLength);
				return temp;
			} else {
				return rawdata;
			}
		}
		
		public static unsafe bool WriteToProtectedMemory<T>(IntPtr addr, T? value) where T : unmanaged
		{
			var array = ToByteArray(value, sizeof(T));
			var success = VirtualProtect(addr, (uint)array.Length, Protection.ReadWrite, out var old);
			if (!success) return false;
        
			Marshal.Copy(array, 0, addr, array.Length);
			return VirtualProtect(addr, (uint)array.Length, old, out old);
		}
		
		/// <summary>
		/// Logs a message in accordance to the specified level, omitted in builds with the <a href="https://docs.unrealengine.com/en-US/Programming/Development/BuildConfigurations/index.html#buildconfigurationdescriptions">Shipping</a> configuration
		/// </summary>
		public static void Log(LogLevel level, string message)
		{
			if (message == null)
				throw new ArgumentNullException(nameof(message));

			Console.WriteLine(message);
			Log(level, message.StringToBytes());
		}
	}

	[InlineArray(0x30)]
	public struct FlowStackType
	{
		private byte _element0;
	}

	/// <summary>
	/// A representation of the engine's object reference
	/// </summary>
	[StructLayout(LayoutKind.Sequential)]
	public unsafe struct FOutParmRec
	{
		public IntPtr Property;
		public byte* PropAddr;
		public FOutParmRec* NextOutParm;
	}
	
	public static partial class Hooking
	{
		public static IntPtr SigScan(string signature)
		{
			return SigScan(signature.StringToBytes());
		}
		
		public static IntPtr Hook(IntPtr address, IntPtr hook, ref IntPtr original)
		{
			return HookInternal(address, hook, ref original);
		}
		
		public static unsafe long HookUFunction(
			ObjectReference function, 
			UFunctionCallback preCallback, 
			UFunctionCallback postCallback)
		{
			return HookUFunction(
				function.Pointer,
				Marshal.GetFunctionPointerForDelegate(preCallback),
				Marshal.GetFunctionPointerForDelegate(postCallback));
		}
		
		public static void Unhook(IntPtr hook)
		{
			UnhookInternal(hook);
		}
		
		public static bool UnhookUFunction(ObjectReference function, long callbackIds)
		{
			return UnhookUFunction(function.Pointer, callbackIds);
		}
	}

	/// <summary>
	/// A representation of the engine's object reference
	/// </summary>
	[StructLayout(LayoutKind.Sequential)]
	public class ObjectReference : IEquatable<ObjectReference> {
		private IntPtr pointer;
		
		public ObjectReference(IntPtr pointer) => Pointer = pointer;

		public ObjectReference()
		{
			pointer = 0;
		}

		public IntPtr Pointer {
			get {
				if (!IsCreated)
					throw new InvalidOperationException();

				return pointer;
			}

			set {
				if (value == IntPtr.Zero)
					throw new InvalidOperationException();

				pointer = value;
			}
		}

		/// <summary>
		/// Tests for equality between two objects
		/// </summary>
		public static bool operator ==(ObjectReference left, ObjectReference right) => left.Equals(right);

		/// <summary>
		/// Tests for inequality between two objects
		/// </summary>
		public static bool operator !=(ObjectReference left, ObjectReference right) => !left.Equals(right);

		/// <summary>
		/// Returns <c>true</c> if the object is created
		/// </summary>
		public bool IsCreated => pointer != IntPtr.Zero && Object.IsValid(pointer);

		/// <summary>
		/// Returns the name of the object
		/// </summary>
		public string Name {
			get {
				byte[] stringBuffer = ArrayPool.GetStringBuffer();

				Object.GetName(Pointer, stringBuffer);

				return stringBuffer.BytesToString();
			}
		}

		/// <summary>
		/// Finds an object by name
		/// </summary>
		/// <returns>An object or <c>null</c> on failure</returns>
		public static ObjectReference? Find(string name)
		{
            ArgumentNullException.ThrowIfNull(name);

            var pointer = Object.Find(name.StringToBytes());

			return pointer != IntPtr.Zero ? new ObjectReference(pointer) : null;
		}

		/// <summary>
		/// Finds an object by name
		/// </summary>
		/// <returns>An object or <c>null</c> on failure</returns>
		public static ObjectReference? FindFirstOf(string name)
		{
			ArgumentNullException.ThrowIfNull(name);

			var pointer = Object.FindFirstOf(name.StringToBytes());

			return pointer != IntPtr.Zero ? new ObjectReference(pointer) : null;
		}

		public unsafe void ProcessEvent(ObjectReference function, Span<(string name, object value)> @params)
		{
			var size = Function.GetParmsSize(function.Pointer);
			var paramsPtr = Marshal.AllocHGlobal(size);

			foreach (var entry in @params)
			{
				var offset = Function.GetOffsetOfParam(function.Pointer, entry.name.StringToBytes());
				var paramSize = Function.GetSizeOfParam(function.Pointer, entry.name.StringToBytes());
				var memory = Marshal.AllocHGlobal(paramSize);
				Marshal.StructureToPtr(entry.value, memory, true);
			    
				Buffer.MemoryCopy((void*)memory, (void*)(paramsPtr + offset), size - offset, paramSize);
			}
		    
			Object.Invoke(Pointer, function.Pointer, paramsPtr);
			Marshal.FreeHGlobal(paramsPtr);
		}

		public void ProcessEvent(ObjectReference function)
		{
			var size = Function.GetParmsSize(function.Pointer);
			var paramsPtr = Marshal.AllocHGlobal(size);
		    
			Object.Invoke(Pointer, function.Pointer, paramsPtr);
		}
		
	    public unsafe T ProcessEvent<T>(ObjectReference function, Span<(string name, object value)> @params) where T : unmanaged
	    {
		    var size = Function.GetParmsSize(function.Pointer);
		    var paramsPtr = Marshal.AllocHGlobal(size);

		    foreach (var entry in @params)
		    {
			    var offset = Function.GetOffsetOfParam(function.Pointer, entry.name.StringToBytes());
			    var paramSize = Function.GetSizeOfParam(function.Pointer, entry.name.StringToBytes());
			    var memory = Marshal.AllocHGlobal(paramSize);
			    Marshal.StructureToPtr(entry.value, memory, true);
			    
			    Buffer.MemoryCopy((void*)memory, (void*)(paramsPtr + offset), size - offset, paramSize);
		    }
		    
		    Object.Invoke(Pointer, function.Pointer, paramsPtr);

		    var returnVal = Marshal.PtrToStructure<T>(paramsPtr + Function.GetReturnValueOffset(function.Pointer));
		    Marshal.FreeHGlobal(paramsPtr);
		    return returnVal;
	    }
	    
	    public T ProcessEvent<T>(ObjectReference function) where T : unmanaged
	    {
		    var size = Function.GetParmsSize(function.Pointer);
		    var paramsPtr = Marshal.AllocHGlobal(size);
		    
		    Object.Invoke(Pointer, function.Pointer, paramsPtr);

		    var returnVal = Marshal.PtrToStructure<T>(paramsPtr + Function.GetReturnValueOffset(function.Pointer));
		    Marshal.FreeHGlobal(paramsPtr);
		    return returnVal;
	    }

		/// <summary>
		/// Retrieves the value of the object property
		/// </summary>
		public ObjectReference? GetObjectReference(string name) {
            ArgumentNullException.ThrowIfNull(name);

            IntPtr valuePtr = 0;
			
			return Object.GetObjectReference(Pointer, name.StringToBytes(), ref valuePtr) ? new ObjectReference(valuePtr) : null;
		}

		/// <summary>
		/// Retrieves the value of the object property
		/// </summary>
		public ObjectReference? GetFunction(string name) {
            ArgumentNullException.ThrowIfNull(name);

            IntPtr valuePtr = 0;
			
			return Object.GetFunction(Pointer, name.StringToBytes(), ref valuePtr) ? new ObjectReference(valuePtr) : null;
		}

		/// <summary>
		/// Retrieves the value of the bool property
		/// </summary>
		public bool GetBool(string name, ref bool value) {
            ArgumentNullException.ThrowIfNull(name);

            return Object.GetBool(Pointer, name.StringToBytes(), ref value);
		}

		/// <summary>
		/// Retrieves the value of the byte property
		/// </summary>
		/// <returns><c>true</c> on success</returns>
		public bool GetByte(string name, ref byte value) {
            ArgumentNullException.ThrowIfNull(name);

            return Object.GetByte(Pointer, name.StringToBytes(), ref value);
		}

		/// <summary>
		/// Retrieves the value of the short property
		/// </summary>
		/// <returns><c>true</c> on success</returns>
		public bool GetShort(string name, ref short value) {
			ArgumentNullException.ThrowIfNull(name);

			return Object.GetShort(Pointer, name.StringToBytes(), ref value);
		}

		/// <summary>
		/// Retrieves the value of the integer property
		/// </summary>
		/// <returns><c>true</c> on success</returns>
		public bool GetInt(string name, ref int value) {
			ArgumentNullException.ThrowIfNull(name);

			return Object.GetInt(Pointer, name.StringToBytes(), ref value);
		}

		/// <summary>
		/// Retrieves the value of the long property
		/// </summary>
		/// <returns><c>true</c> on success</returns>
		public bool GetLong(string name, ref long value) {
			ArgumentNullException.ThrowIfNull(name);

			return Object.GetLong(Pointer, name.StringToBytes(), ref value);
		}

		/// <summary>
		/// Retrieves the value of the unsigned short property
		/// </summary>
		/// <returns><c>true</c> on success</returns>
		public bool GetUShort(string name, ref ushort value) {
			ArgumentNullException.ThrowIfNull(name);

			return Object.GetUShort(Pointer, name.StringToBytes(), ref value);
		}

		/// <summary>
		/// Retrieves the value of the unsigned integer property
		/// </summary>
		/// <returns><c>true</c> on success</returns>
		public bool GetUInt(string name, ref uint value) {
			ArgumentNullException.ThrowIfNull(name);

			return Object.GetUInt(Pointer, name.StringToBytes(), ref value);
		}

		/// <summary>
		/// Retrieves the value of the unsigned long property
		/// </summary>
		/// <returns><c>true</c> on success</returns>
		public bool GetULong(string name, ref ulong value) {
			ArgumentNullException.ThrowIfNull(name);

			return Object.GetULong(Pointer, name.StringToBytes(), ref value);
		}

		/// <summary>
		/// Retrieves the value of the struct property
		/// </summary>
		/// <returns><c>true</c> on success</returns>
		public StructReference? GetStruct(string name) {
			ArgumentNullException.ThrowIfNull(name);

			IntPtr valuePtr = 0;
			
			return Object.GetStruct(Pointer, name.StringToBytes(), ref valuePtr) ? new StructReference(valuePtr) : null;
		}

		public Vector3 GetVector(string name)
		{
			var vec = GetStruct(name);
			if (vec is null) return Vector3.Zero;
            
			float x = 0, y = 0, z = 0;
            
			vec.GetFloat("X", ref x);
			vec.GetFloat("Y", ref y);
			vec.GetFloat("Z", ref z);
                
			var ret = new Vector3(x, y ,z);
			return ret;
		}

		public Vector3 GetRotator(string name)
		{
			var vec = GetStruct(name);
			if (vec is null) return Vector3.Zero;
            
			float x = 0, y = 0, z = 0;
            
			vec.GetFloat("Roll", ref x);
			vec.GetFloat("Pitch", ref y);
			vec.GetFloat("Yaw", ref z);
                
			var ret = new Vector3(x, y ,z);
			return ret;
		}
		
		/// <summary>
		/// Retrieves the value of the array property
		/// </summary>
		/// <returns><c>true</c> on success</returns>
		public bool GetArray(string name, ref UnArray value) {
			ArgumentNullException.ThrowIfNull(name);

			return Object.GetArray(Pointer, name.StringToBytes(), ref value);
		}

		/// <summary>
		/// Retrieves the value of the float property
		/// </summary>
		/// <returns><c>true</c> on success</returns>
		public bool GetFloat(string name, ref float value) {
			ArgumentNullException.ThrowIfNull(name);

			return Object.GetFloat(Pointer, name.StringToBytes(), ref value);
		}

		/// <summary>
		/// Retrieves the value of the double property
		/// </summary>
		/// <returns><c>true</c> on success</returns>
		public bool GetDouble(string name, ref double value) {
			ArgumentNullException.ThrowIfNull(name);

			return Object.GetDouble(Pointer, name.StringToBytes(), ref value);
		}

		/// <summary>
		/// Retrieves the value of the enum property
		/// </summary>
		/// <returns><c>true</c> on success</returns>
		public bool GetEnum<T>(string name, ref T value) where T : Enum {
			ArgumentNullException.ThrowIfNull(name);

			long data = 0;

			if (!Object.GetEnum(Pointer, name.StringToBytes(), ref data)) return false;
			value = (T)Enum.ToObject(typeof(T), data);

			return true;

		}

		public bool GetWeakObject(string name, ref WeakObjectPtr value)
		{
            ArgumentNullException.ThrowIfNull(name);

            return Object.GetWeakObject(Pointer, name.StringToBytes(), ref value);
		}

		/// <summary>
		/// Retrieves the value of the string property
		/// </summary>
		/// <returns><c>true</c> on success</returns>
		public bool GetString(string name, ref string value) {
			ArgumentNullException.ThrowIfNull(name);

			var stringBuffer = ArrayPool.GetStringBuffer();

			if (!Object.GetString(Pointer, name.StringToBytes(), stringBuffer)) return false;
			value = stringBuffer.BytesToString();

			return true;

		}

		/// <summary>
		/// Retrieves the value of the text property
		/// </summary>
		/// <returns><c>true</c> on success</returns>
		public bool GetText(string name, ref string value) {
			ArgumentNullException.ThrowIfNull(name);

			var stringBuffer = ArrayPool.GetStringBuffer();

			if (!Object.GetText(Pointer, name.StringToBytes(), stringBuffer)) return false;
			value = stringBuffer.BytesToString();

			return true;

		}

		/// <summary>
		/// Sets the value of the bool property
		/// </summary>
		public bool SetBool(string name, bool value) {
            ArgumentNullException.ThrowIfNull(name);

            return Object.SetBool(Pointer, name.StringToBytes(), value);
		}
		
		/// <summary>
		/// Sets the value of the object property
		/// </summary>
		/// <returns><c>true</c> on success</returns>
		public bool SetObjectReference(string name, ObjectReference value) {
			ArgumentNullException.ThrowIfNull(name);

			return Object.SetObjectReference(Pointer, name.StringToBytes(), value.Pointer);
		}

		/// <summary>
		/// Sets the value of the byte property
		/// </summary>
		/// <returns><c>true</c> on success</returns>
		public bool SetByte(string name, byte value) {
			ArgumentNullException.ThrowIfNull(name);

			return Object.SetByte(Pointer, name.StringToBytes(), value);
		}

		/// <summary>
		/// Sets the value of the short property
		/// </summary>
		/// <returns><c>true</c> on success</returns>
		public bool SetShort(string name, short value) {
			ArgumentNullException.ThrowIfNull(name);

			return Object.SetShort(Pointer, name.StringToBytes(), value);
		}

		/// <summary>
		/// Sets the value of the integer property
		/// </summary>
		/// <returns><c>true</c> on success</returns>
		public bool SetInt(string name, int value) {
			ArgumentNullException.ThrowIfNull(name);

			return Object.SetInt(Pointer, name.StringToBytes(), value);
		}

		/// <summary>
		/// Sets the value of the long property
		/// </summary>
		/// <returns><c>true</c> on success</returns>
		public bool SetLong(string name, long value) {
			ArgumentNullException.ThrowIfNull(name);

			return Object.SetLong(Pointer, name.StringToBytes(), value);
		}

		/// <summary>
		/// Sets the value of the unsigned short property
		/// </summary>
		/// <returns><c>true</c> on success</returns>
		public bool SetUShort(string name, ushort value) {
			ArgumentNullException.ThrowIfNull(name);

			return Object.SetUShort(Pointer, name.StringToBytes(), value);
		}

		/// <summary>
		/// Sets the value of the unsigned integer property
		/// </summary>
		/// <returns><c>true</c> on success</returns>
		public bool SetUInt(string name, uint value) {
			ArgumentNullException.ThrowIfNull(name);

			return Object.SetUInt(Pointer, name.StringToBytes(), value);
		}

		/// <summary>
		/// Sets the value of the unsigned long property
		/// </summary>
		/// <returns><c>true</c> on success</returns>
		public bool SetULong(string name, ulong value) {
			ArgumentNullException.ThrowIfNull(name);

			return Object.SetULong(Pointer, name.StringToBytes(), value);
		}
		
		/// <summary>
		/// Sets the value of the object property
		/// </summary>
		/// <returns><c>true</c> on success</returns>
		public bool SetStruct(string name, StructReference value) {
			ArgumentNullException.ThrowIfNull(name);

			return Object.SetStruct(Pointer, name.StringToBytes(), value.Pointer);
		}
		
		public void SetVector(string name, Vector3 val)
		{
			var vec = GetStruct(name);
			if (vec is null) return;
            
			vec.SetFloat("X", val.X);
			vec.SetFloat("Y", val.Y);
			vec.SetFloat("Z", val.Z);
		}
		
		public void SetRotator(string name, Vector3 val)
		{
			var vec = GetStruct(name);
			if (vec is null) return;
            
			vec.SetFloat("Roll", val.X);
			vec.SetFloat("Pitch", val.Y);
			vec.SetFloat("Yaw", val.Z);
		}
		
		/// <summary>
		/// Sets the value of the object property
		/// </summary>
		/// <returns><c>true</c> on success</returns>
		public bool SetArray(string name, UnArray value) {
			ArgumentNullException.ThrowIfNull(name);

			return Object.SetArray(Pointer, name.StringToBytes(), value);
		}

		/// <summary>
		/// Sets the value of the float property
		/// </summary>
		/// <returns><c>true</c> on success</returns>
		public bool SetFloat(string name, float value) {
			ArgumentNullException.ThrowIfNull(name);

			return Object.SetFloat(Pointer, name.StringToBytes(), value);
		}

		/// <summary>
		/// Sets the value of the double property
		/// </summary>
		/// <returns><c>true</c> on success</returns>
		public bool SetDouble(string name, double value) {
			ArgumentNullException.ThrowIfNull(name);

			return Object.SetDouble(Pointer, name.StringToBytes(), value);
		}

		/// <summary>
		/// Sets the value of the enum property
		/// </summary>
		/// <returns><c>true</c> on success</returns>
		public bool SetEnum<T>(string name, T value) where T : Enum {
			ArgumentNullException.ThrowIfNull(name);

			return Object.SetEnum(Pointer, name.StringToBytes(), Convert.ToInt32(value));
		}

		/// <summary>
		/// Sets the value of the string property
		/// </summary>
		/// <returns><c>true</c> on success</returns>
		public bool SetString(string name, string value) {
			ArgumentNullException.ThrowIfNull(name);

            ArgumentNullException.ThrowIfNull(value);

            return Object.SetString(Pointer, name.StringToBytes(), value.StringToBytes());
		}

		/// <summary>
		/// Sets the value of the text property
		/// </summary>
		/// <returns><c>true</c> on success</returns>
		public bool SetText(string name, string value) {
            ArgumentNullException.ThrowIfNull(name);

            ArgumentNullException.ThrowIfNull(value);

            return Object.SetText(Pointer, name.StringToBytes(), value.StringToBytes());
		}
		
		/// <summary>
		/// Indicates equality of objects
		/// </summary>
		public bool Equals(ObjectReference? other) => other is not null && IsCreated && pointer == other.pointer;

		/// <summary>
		/// Indicates equality of objects
		/// </summary>
		public override bool Equals(object? value) {
			if (value == null)
				return false;

			return ReferenceEquals(value.GetType(), typeof(ObjectReference)) && Equals((ObjectReference)value);
		}

		/// <summary>
		/// Returns a hash code for the object
		/// </summary>
		public override int GetHashCode() => pointer.GetHashCode();
	}

	public unsafe class StructReference : ObjectReference, IEquatable<StructReference>
	{
		public StructReference(IntPtr pointer) : base(pointer)
		{
			Pointer = pointer;
		}

		public ClassReference GetSuperStruct()
		{
			IntPtr valuePtr = Struct.GetSuperStruct(Pointer);
			return new ClassReference(valuePtr);
		}

		public void ForEachFunction(delegate* unmanaged[Cdecl]<IntPtr, void> callback)
		{
			Struct.ForEachFunction(Pointer, callback);
		}

		public void ForEachProperty(delegate* unmanaged[Cdecl]<IntPtr, void> callback)
		{
			Struct.ForEachProperty(Pointer, callback);
		}
		
		/// <summary>
		/// Indicates equality of objects
		/// </summary>
		public bool Equals(StructReference? other) => other is not null && IsCreated && Pointer == other.Pointer;

		/// <summary>
		/// Indicates equality of objects
		/// </summary>
		public override bool Equals(object? value) {
			if (value == null)
				return false;

			if (!ReferenceEquals(value.GetType(), typeof(StructReference)))
				return false;

			return Equals((StructReference)value);
		}

		/// <summary>
		/// Returns a hash code for the object
		/// </summary>
		public override int GetHashCode() => Pointer.GetHashCode();
	}

	public class ClassReference : StructReference, IEquatable<ClassReference>
	{
		public ClassReference(IntPtr pointer) : base(pointer)
		{
			Pointer = pointer;
		}

		public ObjectReference GetCdo()
		{
			IntPtr valuePtr = 0;

			Class.GetCDO(Pointer, ref valuePtr);
			return new ObjectReference(valuePtr);
		}

		public bool IsChildOf(ClassReference parent)
		{
			return Class.IsChildOf(Pointer, parent.Pointer);
		}
		
		/// <summary>
		/// Indicates equality of objects
		/// </summary>
		public bool Equals(ClassReference? other) => other is not null && IsCreated && Pointer == other.Pointer;

		/// <summary>
		/// Indicates equality of objects
		/// </summary>
		public override bool Equals(object? value) {
			if (value == null)
				return false;

			if (!ReferenceEquals(value.GetType(), typeof(ClassReference)))
				return false;

			return Equals((ClassReference)value);
		}

		/// <summary>
		/// Returns a hash code for the object
		/// </summary>
		public override int GetHashCode() => Pointer.GetHashCode();
	}
	
	public partial struct WeakObjectPtr
	{
		public WeakObjectPtr(int objectIndex, int objectSerialNumber)
		{
			ObjectIndex = objectIndex;
			ObjectSerialNumber = objectSerialNumber;
		}
	}
}