using System.Text;
using UnrealSharp;

namespace UE4SSL.Inspector.Dumper
{
    /// <summary>
    /// 提供用于转储 Unreal Engine SDK 的功能
    /// </summary>
    /// <remarks>
    /// 该类负责从Unreal Engine内存中提取类、结构体、枚举等信息，
    /// 并生成对应C#代码的功能。生成的代码可以用于与Unreal Engine交互。
    /// </remarks>
    internal class SDK
    {
        public static List<string> AllEnumNames { get; } = new List<string>();
        #region 公共方法

        /// <summary>
        /// 将 Unreal Engine 对象转储为 C# SDK
        /// </summary>
        /// <param name="location">SDK 输出位置，默认为进程名称</param>
        public void DumpSdk(string location = "")
        {
            // 设置默认输出位置为进程名称
            if (string.IsNullOrEmpty(location))
            {
                location = UnrealEngine.Memory.Process.ProcessName;
            }

            // 确保输出目录存在
            Directory.CreateDirectory(location);

            // 读取对象列表和数量
            var entityList = UnrealEngine.Memory.ReadProcessMemory<nint>(UnrealEngine.Memory.BaseAddress + UnrealEngine.GObjects);
            var count = UnrealEngine.Memory.ReadProcessMemory<uint>(UnrealEngine.Memory.BaseAddress + UnrealEngine.GObjects + 0x14);
            entityList = UnrealEngine.Memory.ReadProcessMemory<nint>(entityList);

            // 按包组织对象
            var packages = new Dictionary<nint, List<nint>>();
            for (var i = 0; i < count; i++)
            {
                // 读取实体地址
                var entityAddr = UnrealEngine.Memory.ReadProcessMemory<nint>((entityList + 8 * (i >> 16)) + 24 * (i % 0x10000));
                if (entityAddr == 0) continue;

                // 查找最外层对象
                var outer = entityAddr;
                while (true)
                {
                    var tempOuter = UnrealEngine.Memory.ReadProcessMemory<nint>(outer + UEObject.objectOuterOffset);
                    if (tempOuter == 0) break;
                    outer = tempOuter;
                }

                // 将对象添加到对应的包中
                if (!packages.ContainsKey(outer))
                {
                    packages.Add(outer, new List<nint>());
                }
                packages[outer].Add(entityAddr);
            }

            // 创建用于存储处理后的包的集合
            var dumpedPackages = new List<Package>();

            // 处理所有包和类
            foreach (var package in packages)
            {
                var packageObj = new UEObject(package.Key);
                var fullPackageName = packageObj.GetName();

                // 创建包对象
                var sdkPackage = new Package { FullName = fullPackageName };
                var dumpedClasses = new List<string>();

                // 处理包中的每个对象
                foreach (var objAddr in package.Value)
                {
                    // 跳过已处理的类
                    var obj = new UEObject(objAddr);
                    if (dumpedClasses.Contains(obj.ClassName)) continue;
                    dumpedClasses.Add(obj.ClassName);
                    if (obj.ClassName.StartsWith("Package")) continue;

                    // 确定对象类型
                    var typeName = DetermineTypeName(obj.ClassName);
                    var className = obj.GetName();
                    if (typeName == "unk" || className == "Object") continue;

                    // 创建SDK类对象
                    var sdkClass = CreateSdkClass(obj, className, fullPackageName, typeName);

                    // 根据类型处理字段和函数
                    if (typeName == "enum")
                    {
                        ProcessEnumFields(objAddr, sdkClass);
                    }
                    else
                    {
                        ProcessClassFields(obj, className, sdkClass);
                        ProcessClassFunctions(obj, className, sdkClass);
                    }

                    // 添加到包中
                    sdkPackage.Classes.Add(sdkClass);
                }

                dumpedPackages.Add(sdkPackage);
            }
            foreach (var p in dumpedPackages)
            {
                p.Dependencies = new List<Package>();
                foreach (var c in p.Classes)
                {
                    {
                        var fromPackage = dumpedPackages.Find(tp => tp.Classes.Count(tc => tc.Name == c.Parent) > 0);
                        if (fromPackage != null && fromPackage != p && !p.Dependencies.Contains(fromPackage)) p.Dependencies.Add(fromPackage);
                    }
                    foreach (var f in c.Fields)
                    {
                        var fromPackage = dumpedPackages.Find(tp => tp.Classes.Count(tc => tc.Name == f.Type?.Replace("Array<", "").Replace(">", "")) > 0);
                        if (fromPackage != null && fromPackage != p && !p.Dependencies.Contains(fromPackage)) p.Dependencies.Add(fromPackage);
                    }
                    foreach (var f in c.Functions)
                    {
                        foreach (var param in f.Params)
                        {
                            var fromPackage = dumpedPackages.Find(tp => tp.Classes.Count(tc => tc.Name == param.Type?.Replace("Array<", "").Replace(">", "")) > 0);
                            if (fromPackage != null && fromPackage != p && !p.Dependencies.Contains(fromPackage)) p.Dependencies.Add(fromPackage);
                        }
                    }
                }
            }

            foreach (var p in dumpedPackages)
            {
                if (p.Name == "FSD")
                {
                    GeneratePackageFiles(p, location);
                }
            }
        }

        /// <summary>
        /// 为单个包生成SDK文件
        /// </summary>
        /// <param name="package">包对象</param>
        /// <param name="location">输出位置</param>
        private static void GeneratePackageFiles(Package package, string location)
        {
            // 创建StringBuilder用于构建文件内容
            var sb = new StringBuilder();

            // 添加必要的引用
            AddImportStatements(sb, package);

            // 添加命名空间声明
            string namespaceName = "SDK" + package.FullName.Replace("/", ".") + "SDK";
            sb.AppendLine($"namespace {namespaceName}");
            sb.AppendLine("{");

            // 跟踪实际生成的类数量
            var printedClasses = 0;

            // 处理包中的每个类
            foreach (var sdkClass in package.Classes)
            {
                if (sdkClass.Fields.Count > 0) printedClasses++;

                // 根据类型生成不同的代码
                if (sdkClass.SdkType == "enum")
                {
                    GenerateEnumCode(sb, sdkClass);
                }
                else
                {
                    GenerateClassCode(sb, sdkClass);
                }
            }

            sb.AppendLine("}");

            // 如果没有实际生成的类，则不创建文件
            if (printedClasses == 0)
                return;

            // 写入文件
            string filePath = Path.Combine(location, package.Name + ".cs");
            File.WriteAllText(filePath, sb.ToString());
        }

        #endregion

        #region 私有辅助方法

        /// <summary>
        /// 添加导入语句
        /// </summary>
        /// <param name="sb">StringBuilder对象</param>
        /// <param name="package">包对象</param>
        private static void AddImportStatements(StringBuilder sb, Package package)
        {
            sb.AppendLine("using UnrealSharp;");
            sb.AppendLine("using Object = UnrealSharp.UEObject;");
            sb.AppendLine("using Guid = SDK.Script.CoreUObjectSDK.Guid;");
            sb.AppendLine("using Enum = SDK.Script.CoreUObjectSDK.Enum;");
            sb.AppendLine("using DateTime = SDK.Script.CoreUObjectSDK.DateTime;");

            // 添加依赖包的引用
            foreach (var d in package.Dependencies)
            {
                sb.AppendLine("using SDK" + d.FullName.Replace("/", ".") + "SDK;");
            }
        }

        /// <summary>
        /// 生成枚举类型代码
        /// </summary>
        /// <param name="sb">StringBuilder对象</param>
        /// <param name="sdkClass">SDK类对象</param>
        private static void GenerateEnumCode(StringBuilder sb, Package.SDKClass sdkClass)
        {
            sb.AppendLine("    public " + sdkClass.SdkType + " " + sdkClass.Name + " : " + sdkClass.Parent);
            sb.AppendLine("    {");

            // 生成枚举字段
            foreach (var field in sdkClass.Fields)
            {
                sb.AppendLine("        " + field.Name + " = " + field.EnumVal + ",");
            }

            sb.AppendLine("    }");
        }

        ///// <summary>
        ///// 生成类代码
        ///// </summary>
        ///// <param name="sb">StringBuilder对象</param>
        ///// <param name="sdkClass">SDK类对象</param>
        //private static void GenerateClassCode(StringBuilder sb, Package.SDKClass sdkClass)
        //{
        //    sb.AppendLine("    public " + sdkClass.SdkType + " " + sdkClass.Name + ((sdkClass.Parent == null) ? "" : (" : " + sdkClass.Parent)));
        //    sb.AppendLine("    {");

        //    // 添加构造函数
        //    sb.AppendLine("        public " + sdkClass.Name + "(nint addr) : base(addr) { }");

        //    // 生成字段
        //    foreach (var field in sdkClass.Fields)
        //    {
        //        // 跳过特定字段（临时解决方案）
        //        if (field.Name == "RelatedPlayerState") continue;

        //        sb.AppendLine("        public " + field.Type + " " + field.Name + " " + field.GetterSetter);
        //    }

        //    // 生成函数
        //    GenerateClassFunctionCode(sb, sdkClass);

        //    sb.AppendLine("    }");
        //}

        ///// <summary>
        ///// 生成类函数代码
        ///// </summary>
        ///// <param name="sb">StringBuilder对象</param>
        ///// <param name="sdkClass">SDK类对象</param>
        //private static void GenerateClassFunctionCode(StringBuilder sb, Package.SDKClass sdkClass)
        //{
        //    foreach (var function in sdkClass.Functions)
        //    {
        //        // 跳过特定函数（临时解决方案）
        //        if (function.Name == "ClientReceiveLocalizedMessage") continue;

        //        // 确定返回类型
        //        var returnType = function.Params.FirstOrDefault(pa => pa.Name == "ReturnValue")?.Type ?? "void";

        //        // 构建参数列表
        //        var parameters = String.Join(", ", function.Params.FindAll(pa => pa.Name != "ReturnValue").Select(pa => pa.Type + " " + pa.Name));

        //        // 构建参数名称列表
        //        var args = function.Params.FindAll(pa => pa.Name != "ReturnValue").Select(pa => pa.Name).ToList();
        //        args.Insert(0, "nameof(" + function.Name + ")");
        //        var argList = String.Join(", ", args);

        //        // 确定返回类型模板
        //        var returnTypeTemplate = returnType == "void" ? "" : ("<" + returnType + ">");

        //        // 生成函数声明和实现
        //        sb.AppendLine("        public " + returnType + " " + function.Name + "(" + parameters + ") { " +
        //            (returnType == "void" ? "" : "return ") + "Invoke" + returnTypeTemplate + "(" + argList + "); }");
        //    }
        //}

        private static void GenerateClassCode(StringBuilder sb, Package.SDKClass sdkClass)
        {
            //sb.AppendLine($"    public class {sdkClass.Name}(IntPtr pointer) : ObjectBase<UGameFunctionLibrary>(pointer)");
            // 类声明
            if (!string.IsNullOrEmpty(sdkClass.Parent) && sdkClass.Parent != "Object")
                sb.AppendLine($"    public {sdkClass.SdkType} {sdkClass.Name}(nint pointer) : {sdkClass.Parent}(pointer)");
            else if (!string.IsNullOrEmpty(sdkClass.Parent))
                sb.AppendLine($"    public {sdkClass.SdkType} {sdkClass.Name}(nint pointer) : {sdkClass.Parent}");
            else
                sb.AppendLine($"    public {sdkClass.SdkType} {sdkClass.Name}(nint pointer)");

            sb.AppendLine("    {");

            // 构造函数（仅class类型生成）
            //if (sdkClass.SdkType == "class")
            //{
            //    sb.AppendLine($"        public {sdkClass.Name}(nint pointer) : base(pointer)");
            //    sb.AppendLine("        {");
            //    sb.AppendLine("        }");
            //}

            foreach (var field in sdkClass.Fields)
            {
                var type = field.Type;
                var name = field.Name;

                // 支持 T[] 和 Array<T> 两种数组类型
                if (type.EndsWith("[]") || (type.StartsWith("Array<") && type.EndsWith(">")))
                {
                    string elementType;
                    if (type.EndsWith("[]"))
                        elementType = type.Substring(0, type.Length - 2);
                    else
                        elementType = type.Substring(6, type.Length - 7);

                    if (elementType == "Object")
                    {
                        sb.AppendLine($"        public Object[] {name}");
                        sb.AppendLine("        {");
                        sb.AppendLine("            get");
                        sb.AppendLine("            {");
                        sb.AppendLine("                UnArray array = new UnArray();");
                        sb.AppendLine($"                GetArray(\"{name}\", ref array);");
                        sb.AppendLine("                return array.DataToArray<Object>().ToArray();");
                        sb.AppendLine("            }");
                        sb.AppendLine("            set");
                        sb.AppendLine("            {");
                        sb.AppendLine("                UnArray array = new UnArray();");
                        sb.AppendLine("                array.ArrayToData(value ?? Array.Empty<Object>());");
                        sb.AppendLine($"                SetArray(\"{name}\", array);");
                        sb.AppendLine("            }");
                        sb.AppendLine("        }");
                    }
                    else
                    {
                        sb.AppendLine($"        public {elementType}[] {name}");
                        sb.AppendLine("        {");
                        sb.AppendLine("            get");
                        sb.AppendLine("            {");
                        sb.AppendLine("                UnArray array = new UnArray();");
                        sb.AppendLine($"                GetArray(\"{name}\", ref array);");
                        sb.AppendLine($"                return array.DataToArray<{elementType}>();");
                        sb.AppendLine("            }");
                        sb.AppendLine("            set");
                        sb.AppendLine("            {");
                        sb.AppendLine("                UnArray array = new UnArray();");
                        sb.AppendLine("                array.ArrayToData(value);");
                        sb.AppendLine($"                SetArray(\"{name}\", array);");
                        sb.AppendLine("            }");
                        sb.AppendLine("        }");
                    }
                }
                // bool类型专用处理
                else if (type == "bool")
                {
                    sb.AppendLine($"        public bool {name}");
                    sb.AppendLine("        {");
                    sb.AppendLine("            get");
                    sb.AppendLine("            {");
                    sb.AppendLine("                bool b = false;");
                    sb.AppendLine($"                GetBool(\"{name}\", ref b);");
                    sb.AppendLine("                return b;");
                    sb.AppendLine("            }");
                    sb.AppendLine("            set");
                    sb.AppendLine("            {");
                    sb.AppendLine($"                SetBool(\"{name}\", value);");
                    sb.AppendLine("            }");
                    sb.AppendLine("        }");
                }
                // int类型专用处理
                else if (type == "int")
                {
                    sb.AppendLine($"        public int {name}");
                    sb.AppendLine("        {");
                    sb.AppendLine("            get");
                    sb.AppendLine("            {");
                    sb.AppendLine("                int i = int.MinValue;");
                    sb.AppendLine($"                GetInt(\"{name}\", ref i);");
                    sb.AppendLine("                return i;");
                    sb.AppendLine("            }");
                    sb.AppendLine("            set");
                    sb.AppendLine("            {");
                    sb.AppendLine($"                SetInt(\"{name}\", value);");
                    sb.AppendLine("            }");
                    sb.AppendLine("        }");
                }
                // 非数组Object类型
                else if (type == "Object")
                {
                    sb.AppendLine($"        public Object {name}");
                    sb.AppendLine("        {");
                    sb.AppendLine("            get");
                    sb.AppendLine("            {");
                    sb.AppendLine($"                return GetObjectReference(\"{name}\");");
                    sb.AppendLine("            }");
                    sb.AppendLine("            set");
                    sb.AppendLine("            {");
                    sb.AppendLine($"                SetObjectReference(\"{name}\", (ObjectReference)value);");
                    sb.AppendLine("            }");
                    sb.AppendLine("        }");
                }
                // float类型专用处理
                else if (type == "float")
                {
                    sb.AppendLine($"        public float {name}");
                    sb.AppendLine("        {");
                    sb.AppendLine("            get");
                    sb.AppendLine("            {");
                    sb.AppendLine("                float f = float.MinValue;");
                    sb.AppendLine($"                GetFloat(\"{name}\", ref f);");
                    sb.AppendLine("                return f;");
                    sb.AppendLine("            }");
                    sb.AppendLine("            set");
                    sb.AppendLine("            {");
                    sb.AppendLine($"                SetFloat(\"{name}\", value);");
                    sb.AppendLine("            }");
                    sb.AppendLine("        }");
                }
                // double类型专用处理
                else if (type == "double")
                {
                    sb.AppendLine($"        public double {name}");
                    sb.AppendLine("        {");
                    sb.AppendLine("            get");
                    sb.AppendLine("            {");
                    sb.AppendLine("                double d = double.MinValue;");
                    sb.AppendLine($"                GetDouble(\"{name}\", ref d);");
                    sb.AppendLine("                return d;");
                    sb.AppendLine("            }");
                    sb.AppendLine("            set");
                    sb.AppendLine("            {");
                    sb.AppendLine($"                SetDouble(\"{name}\", value);");
                    sb.AppendLine("            }");
                    sb.AppendLine("        }");
                }
                // byte类型专用处理
                else if (type == "byte")
                {
                    sb.AppendLine($"        public byte {name}");
                    sb.AppendLine("        {");
                    sb.AppendLine("            get");
                    sb.AppendLine("            {");
                    sb.AppendLine("                byte b = byte.MinValue;");
                    sb.AppendLine($"                GetByte(\"{name}\", ref b);");
                    sb.AppendLine("                return b;");
                    sb.AppendLine("            }");
                    sb.AppendLine("            set");
                    sb.AppendLine("            {");
                    sb.AppendLine($"                SetByte(\"{name}\", value);");
                    sb.AppendLine("            }");
                    sb.AppendLine("        }");
                }
                // uint类型专用处理
                else if (type == "uint")
                {
                    sb.AppendLine($"        public uint {name}");
                    sb.AppendLine("        {");
                    sb.AppendLine("            get");
                    sb.AppendLine("            {");
                    sb.AppendLine("                uint u = uint.MinValue;");
                    sb.AppendLine($"                GetUInt(\"{name}\", ref u);");
                    sb.AppendLine("                return u;");
                    sb.AppendLine("            }");
                    sb.AppendLine("            set");
                    sb.AppendLine("            {");
                    sb.AppendLine($"                SetUInt(\"{name}\", value);");
                    sb.AppendLine("            }");
                    sb.AppendLine("        }");
                }
                // 枚举类型专用处理
                else if (SDK.AllEnumNames.Contains(type))
                {
                    sb.AppendLine($"        public {type} {name}");
                    sb.AppendLine("        {");
                    sb.AppendLine("            get");
                    sb.AppendLine("            {");
                    sb.AppendLine($"                var e = new {type}();");
                    sb.AppendLine($"                GetEnum<{type}>(\"{name}\", ref e);");
                    sb.AppendLine("                return e;");
                    sb.AppendLine("            }");
                    sb.AppendLine("            set");
                    sb.AppendLine("            {");
                    sb.AppendLine($"                SetEnum(\"{name}\", value);");
                    sb.AppendLine("            }");
                    sb.AppendLine("        }");
                }
                // DateTime类型专用处理
                else if (type == "DateTime" || type == "UEDateTime")
                {
                    sb.AppendLine($"        public DateTime {name}");
                    sb.AppendLine("        { get; set; }");
                }
                // 非数组其它类型（如 MissionShouts）
                else
                {
                    sb.AppendLine($"        public {type} {name}");
                    sb.AppendLine("        {");
                    sb.AppendLine("            get");
                    sb.AppendLine("            {");
                    sb.AppendLine($"                return ({type})GetObjectReference(\"{name}\");");
                    sb.AppendLine("            }");
                    sb.AppendLine("            set");
                    sb.AppendLine("            {");
                    sb.AppendLine($"                SetObjectReference(\"{name}\", (ObjectReference)value);");
                    sb.AppendLine("            }");
                    sb.AppendLine("        }");
                }
            }

            GenerateClassFunctionCode(sb, sdkClass);

            sb.AppendLine("    }");
        }

        private static void GenerateClassFunctionCode(StringBuilder sb, Package.SDKClass sdkClass)
        {
            foreach (var function in sdkClass.Functions)
            {
                if (function.Name == "ClientReceiveLocalizedMessage") continue;

                var returnParam = function.Params.FirstOrDefault(pa => pa.Name == "ReturnValue");
                var returnType = returnParam?.Type ?? "void";

                var parameters = String.Join(", ", function.Params
                    .Where(pa => pa.Name != "ReturnValue")
                    .Select(pa =>
                        (pa.Type.StartsWith("Array<")
                            ? $"List<{pa.Type.Substring(6, pa.Type.Length - 7)}>"
                            : pa.Type)
                        + " " + pa.Name));

                var actualReturnType = returnType.StartsWith("Array<")
                    ? $"List<{returnType.Substring(6, returnType.Length - 7)}>"
                    : returnType;

                sb.AppendLine($"        public {actualReturnType} {function.Name}({parameters})");
                sb.AppendLine("        {");
                sb.AppendLine("            Span<(string name, object value)> @params = [");
                foreach (var param in function.Params)
                {
                    if (param.Name != "ReturnValue")
                        sb.AppendLine($"                (\"{param.Name}\", {param.Name}),");
                }
                sb.AppendLine("            ];");

                if (returnType == "void")
                {
                    sb.AppendLine($"            ProcessEvent(GetFunction(\"{function.Name}\")!, @params);");
                }
                else if (returnType == "Object")
                {
                    sb.AppendLine($"            return ProcessEvent<IntPtr>(GetFunction(\"{function.Name}\")!, @params);");
                }
                else if (returnType == "int")
                {
                    sb.AppendLine($"            return ProcessEvent<int>(GetFunction(\"{function.Name}\")!, @params);");
                }
                else if (returnType == "long")
                {
                    sb.AppendLine($"            return ProcessEvent<IntPtr>(GetFunction(\"{function.Name}\")!, @params).ToInt64();");
                }
                else if (returnType == "bool")
                {
                    sb.AppendLine($"            return ProcessEvent<bool>(GetFunction(\"{function.Name}\")!, @params);");
                }
                else if (returnType == "float")
                {
                    sb.AppendLine($"            return ProcessEvent<float>(GetFunction(\"{function.Name}\")!, @params);");
                }
                else if (returnType == "byte")
                {
                    sb.AppendLine($"            return (byte)ProcessEvent<IntPtr>(GetFunction(\"{function.Name}\")!, @params);");
                }
                else if (returnType == "ObjectReference")
                {
                    sb.AppendLine($"            return new ObjectReference(ProcessEvent<IntPtr>(GetFunction(\"{function.Name}\")!, @params));");
                }
                else if (SDK.AllEnumNames.Contains(returnType))
                {
                    sb.AppendLine($"            return ({returnType})ProcessEvent<IntPtr>(GetFunction(\"{function.Name}\")!, @params);");
                }
                else if (returnType.StartsWith("Array<"))
                {
                    var elementType = returnType.Substring(6, returnType.Length - 7);
                    sb.AppendLine($"            UnArray array = new UnArray();");
                    sb.AppendLine($"            GetArray(\"{function.Name}\", ref array);");
                    sb.AppendLine($"            return array.DataToArray<{elementType}>().ToList();");
                }
                else
                {
                    sb.AppendLine($"            return new {returnType}(ProcessEvent<IntPtr>(GetFunction(\"{function.Name}\")!, @params));");
                }
                sb.AppendLine("        }");
            }
        }




        /// <summary>
        /// 根据字段地址获取对应的 C# 类型和访问器
        /// </summary>
        /// <param name="fName">字段名称</param>
        /// <param name="fType">字段类型</param>
        /// <param name="fAddr">字段地址</param>
        /// <param name="gettersetter">输出的 getter/setter 字符串</param>
        /// <returns>转换后的 C# 类型名称</returns>
        private string GetTypeFromFieldAddr(string fName, string fType, nint fAddr, out string gettersetter)
        {
            gettersetter = "";

            // 处理基本类型
            switch (fType)
            {
                case "BoolProperty":
                    fType = "bool";
                    gettersetter = $"{{ get {{ return this[nameof({fName})].Flag; }} set {{ this[nameof({fName})].Flag = value; }} }}";
                    break;

                case "ByteProperty":
                case "Int8Property":
                    fType = "byte";
                    gettersetter = GetBasicTypeGetterSetter(fName, fType);
                    break;

                case "Int16Property":
                    fType = "short";
                    gettersetter = GetBasicTypeGetterSetter(fName, fType);
                    break;

                case "UInt16Property":
                    fType = "ushort";
                    gettersetter = GetBasicTypeGetterSetter(fName, fType);
                    break;

                case "IntProperty":
                    fType = "int";
                    gettersetter = GetBasicTypeGetterSetter(fName, fType);
                    break;

                case "UInt32Property":
                    fType = "uint";
                    gettersetter = GetBasicTypeGetterSetter(fName, fType);
                    break;

                case "Int64Property":
                    fType = "long";
                    gettersetter = GetBasicTypeGetterSetter(fName, fType);
                    break;

                case "UInt64Property":
                    fType = "ulong";
                    gettersetter = GetBasicTypeGetterSetter(fName, fType);
                    break;

                case "FloatProperty":
                    fType = "float";
                    gettersetter = GetBasicTypeGetterSetter(fName, fType);
                    break;

                case "DoubleProperty":
                    fType = "double";
                    gettersetter = GetBasicTypeGetterSetter(fName, fType);
                    break;

                case "StrProperty":
                case "TextProperty":
                case "NameProperty":
                case "SoftObjectProperty":
                case "SoftClassProperty":
                case "WeakObjectProperty":
                case "LazyObjectProperty":
                case "DelegateProperty":
                case "MulticastSparseDelegateProperty":
                case "MulticastInlineDelegateProperty":
                case "ClassProperty":
                case "MapProperty":
                case "SetProperty":
                case "FieldPathProperty":
                case "InterfaceProperty":
                    fType = "unk";
                    break;

                case "ObjectProperty":
                    var structFieldIndex = UnrealEngine.Memory.ReadProcessMemory<int>(UnrealEngine.Memory.ReadProcessMemory<nint>(fAddr + UEObject.propertySize) + UEObject.nameOffset);
                    fType = UEObject.GetName(structFieldIndex);
                    gettersetter = $"{{ get {{ return this[nameof({fName})].As<{fType}>(); }} set {{ this[\"{fName}\"] = value; }} }}";
                    break;

                case "ClassPtrProperty":
                case "ScriptTypedElementHandle":
                    fType = "Object";
                    gettersetter = $"{{ get {{ return this[nameof({fName})].As<{fType}>(); }} set {{ this[\"{fName}\"] = value; }} }} // {fType}";
                    break;

                case "StructProperty":
                    structFieldIndex = UnrealEngine.Memory.ReadProcessMemory<int>(UnrealEngine.Memory.ReadProcessMemory<nint>(fAddr + UEObject.propertySize) + UEObject.nameOffset);
                    fType = UEObject.GetName(structFieldIndex);
                    gettersetter = $"{{ get {{ return this[nameof({fName})].As<{fType}>(); }} set {{ this[\"{fName}\"] = value; }} }}";
                    break;

                case "EnumProperty":
                    structFieldIndex = UnrealEngine.Memory.ReadProcessMemory<int>(UnrealEngine.Memory.ReadProcessMemory<nint>(fAddr + UEObject.propertySize + 8) + UEObject.nameOffset);
                    fType = UEObject.GetName(structFieldIndex);
                    gettersetter = $"{{ get {{ return ({fType})this[nameof({fName})].GetValue<int>(); }} set {{ this[nameof({fName})].SetValue<int>((int)value); }} }}";
                    break;

                case "ArrayProperty":
                    var inner = UnrealEngine.Memory.ReadProcessMemory<nint>(fAddr + UEObject.propertySize);
                    var innerClass = UnrealEngine.Memory.ReadProcessMemory<nint>(inner + UEObject.fieldClassOffset);
                    structFieldIndex = UnrealEngine.Memory.ReadProcessMemory<int>(innerClass);
                    fType = UEObject.GetName(structFieldIndex);
                    var innerType = GetTypeFromFieldAddr(fName, fType, inner, out gettersetter);
                    gettersetter = $"{{ get {{ return new Array<{innerType}>(this[nameof({fName})].Address); }} }}";
                    fType = $"Array<{innerType}>";
                    break;

                case "Enum":
                    fType = "UEEnum";
                    break;

                case "DateTime":
                    fType = "UEDateTime";
                    break;

                case "Guid":
                    fType = "UEGuid";
                    break;
            }

            // 处理未知类型
            if (fType == "unk")
            {
                fType = "Object";
                gettersetter = $"{{ get {{ return this[nameof({fName})]; }} set {{ this[nameof({fName})] = value; }} }}";
            }

            return fType;
        }

        /// <summary>
        /// 为基本类型生成 getter/setter 字符串
        /// </summary>
        /// <param name="fieldName">字段名称</param>
        /// <param name="fieldType">字段类型</param>
        /// <returns>生成的 getter/setter 字符串</returns>
        private string GetBasicTypeGetterSetter(string fieldName, string fieldType)
        {
            return $"{{ get {{ return this[nameof({fieldName})].GetValue<{fieldType}>(); }} set {{ this[nameof({fieldName})].SetValue<{fieldType}>(value); }} }}";
        }

        /// <summary>
        /// 根据类名确定对象类型
        /// </summary>
        /// <param name="className">类名</param>
        /// <returns>对象类型名称</returns>
        private static string DetermineTypeName(string className)
        {
            if (className.StartsWith("Class")) return "class";
            if (className.StartsWith("ScriptStruct")) return "class";
            if (className.StartsWith("Enum")) return "enum";
            return "unk";
        }

        /// <summary>
        /// 创建SDK类对象
        /// </summary>
        /// <param name="obj">UE对象</param>
        /// <param name="className">类名</param>
        /// <param name="fullPackageName">包名</param>
        /// <param name="typeName">类型名称</param>
        /// <returns>SDK类对象</returns>
        private static Package.SDKClass CreateSdkClass(UEObject obj, string className, string fullPackageName, string typeName)
        {
            var sdkClass = new Package.SDKClass { Name = className, Namespace = fullPackageName, SdkType = typeName };

            if (typeName == "enum")
            {
                sdkClass.Parent = "int";
            }
            else
            {
                var parentClass = UnrealEngine.Memory.ReadProcessMemory<nint>(obj.Address + UEObject.structSuperOffset);
                if (parentClass != 0)
                {
                    var parentNameIndex = UnrealEngine.Memory.ReadProcessMemory<int>(parentClass + UEObject.nameOffset);
                    var parentName = UEObject.GetName(parentNameIndex);
                    sdkClass.Parent = parentName;
                }
                else
                {
                    sdkClass.Parent = "Object";
                }
            }

            return sdkClass;
        }

        /// <summary>
        /// 处理枚举类型的字段
        /// </summary>
        /// <param name="objAddr">对象地址</param>
        /// <param name="sdkClass">SDK类对象</param>
        private static void ProcessEnumFields(nint objAddr, Package.SDKClass sdkClass)
        {
            var enumArray = UnrealEngine.Memory.ReadProcessMemory<nint>(objAddr + 0x40);
            var enumCount = UnrealEngine.Memory.ReadProcessMemory<int>(objAddr + 0x48);

            for (var i = 0; i < enumCount; i++)
            {
                var enumNameIndex = UnrealEngine.Memory.ReadProcessMemory<int>(enumArray + i * 0x10);
                var enumName = UEObject.GetName(enumNameIndex);
                enumName = enumName.Substring(enumName.LastIndexOf(":") + 1);

                var enumNameRepeatedIndex = UnrealEngine.Memory.ReadProcessMemory<int>(enumArray + i * 0x10 + 4);
                if (enumNameRepeatedIndex > 0)
                    enumName += "_" + enumNameRepeatedIndex;

                var enumVal = UnrealEngine.Memory.ReadProcessMemory<int>(enumArray + i * 0x10 + 0x8);
                sdkClass.Fields.Add(new Package.SDKClass.SDKFields { Name = enumName, EnumVal = enumVal });

                // 记录所有枚举名
                if (!AllEnumNames.Contains(sdkClass.Name))
                    AllEnumNames.Add(sdkClass.Name);
            }
        }

        /// <summary>
        /// 处理类的字段
        /// </summary>
        /// <param name="obj">UE对象</param>
        /// <param name="className">类名</param>
        /// <param name="sdkClass">SDK类对象</param>
        private static void ProcessClassFields(UEObject obj, string className, Package.SDKClass sdkClass)
        {
            var field = obj.Address + UEObject.childPropertiesOffset - UEObject.fieldNextOffset;

            while ((field = UnrealEngine.Memory.ReadProcessMemory<nint>(field + UEObject.fieldNextOffset)) > 0)
            {
                var fName = UEObject.GetName(UnrealEngine.Memory.ReadProcessMemory<int>(field + UEObject.fieldNameOffset));
                var fType = obj.GetFieldType(field);
                var offset = (uint)obj.GetFieldOffset(field);
                var gettersetter = "{ get { return new {0}(this[\"{1}\"].Address); } set { this[\"{1}\"] = value; } }";

                var sdk = new SDK();
                fType = sdk.GetTypeFromFieldAddr(fName, fType, field, out gettersetter);

                // 避免字段名与类名冲突
                if (fName == className) fName += "_value";

                sdkClass.Fields.Add(new Package.SDKClass.SDKFields
                {
                    Type = fType,
                    Name = fName,
                    GetterSetter = gettersetter
                });
            }
        }

        /// <summary>
        /// 处理类的函数
        /// </summary>
        /// <param name="obj">UE对象</param>
        /// <param name="className">类名</param>
        /// <param name="sdkClass">SDK类对象</param>
        private static void ProcessClassFunctions(UEObject obj, string className, Package.SDKClass sdkClass)
        {
            var field = obj.Address + UEObject.childrenOffset - UEObject.funcNextOffset;

            while ((field = UnrealEngine.Memory.ReadProcessMemory<nint>(field + UEObject.funcNextOffset)) > 0)
            {
                var fName = UEObject.GetName(UnrealEngine.Memory.ReadProcessMemory<int>(field + UEObject.nameOffset));

                // 避免函数名与类名冲突
                if (fName == className) fName += "_func";

                var func = new Package.SDKClass.SDKFunctions { Name = fName };
                var fField = field + UEObject.childPropertiesOffset - UEObject.fieldNextOffset;

                while ((fField = UnrealEngine.Memory.ReadProcessMemory<nint>(fField + UEObject.fieldNextOffset)) > 0)
                {
                    var pName = UEObject.GetName(UnrealEngine.Memory.ReadProcessMemory<int>(fField + UEObject.fieldNameOffset));
                    var pType = obj.GetFieldType(fField);
                    var sdk = new SDK();
                    pType = sdk.GetTypeFromFieldAddr("", pType, fField, out _);

                    func.Params.Add(new Package.SDKClass.SDKFields
                    {
                        Name = pName,
                        Type = pType
                    });
                }

                sdkClass.Functions.Add(func);
            }
        }

        #endregion

    }
}
