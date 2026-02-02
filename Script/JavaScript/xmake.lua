target("UE4SSL.JavaScript")
    add_rules("ue4ss.mod")
    
    -- 添加头文件搜索路径
    add_includedirs("include", "deps/quickjs")
    add_headerfiles("include/**.hpp")
    
    -- QuickJS source files
    add_files(
        "deps/quickjs/quickjs.c",
        "deps/quickjs/quickjs-libc.c",
        "deps/quickjs/cutils.c",
        "deps/quickjs/libregexp.c",
        "deps/quickjs/libunicode.c",
        "deps/quickjs/dtoa.c"
    )
    
    -- Module source files
    add_files(
        "src/dllmain.cpp",
        "src/JSMod.cpp",
        "src/JSType/JSUObject.cpp"
    )
    
    -- QuickJS compile definitions
    add_defines("CONFIG_VERSION=\"2024-01-13\"", "_GNU_SOURCE")
    
    -- Disable warnings for QuickJS C code on MSVC
    if is_plat("windows") then
        add_cflags("/wd4244", "/wd4267", "/wd4996", "/wd4018", "/wd4146", "/wd4334", {tools = "cl"})
    end
