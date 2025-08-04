using System;
using System.Collections.Generic;
using System.Linq;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading.Tasks;

namespace UE4SSL.Framework
{

    [StructLayout(LayoutKind.Sequential)]
    public struct FStringStruct
    {
        public IntPtr Data;  // 指向 Unicode 字符串的指针
        public int Num;      // 字符数
        public int Max;      // 容量
    };

    public class FString
    {
        public static FStringStruct makeString(string str)
        {
            int num = str.Length;      // UTF-16 code units
            int max = num + 1;         // 至少比 num 大 1
            int sizeInBytes = max * 2; // 每个 UTF-16 占 2 字节

            IntPtr buffer = Marshal.AllocHGlobal(sizeInBytes);

            // 复制 UTF-16 数据
            var chars = str.ToCharArray();
            Marshal.Copy(chars, 0, buffer, num);

            // 写 UTF-16 终止符 ？
            //Marshal.WriteInt16(buffer, num * 2, 0);

            return new FStringStruct
            {
                Data = buffer,
                Num = max,
                Max = max
            };
        }


        public static void FreeFakeFString(FStringStruct fstring)
        {
            if (fstring.Data != IntPtr.Zero)
                Marshal.FreeHGlobal(fstring.Data);
        }
    }




}
