-- 设置项目名
local projectName = "CSharpLoader"

-- 设置编译模式和 C++ 标准
set_languages("cxx23")

-- 添加目标
target(projectName)
    add_rules("ue4ss.mod")
    set_kind("binary")

    -- 添加源文件
    add_files("src/**.cpp")

    -- 添加头文件搜索路径
    add_includedirs("include", {public = true})

    -- 链接库（公共）
    add_links("libnethost")
    add_linkdirs("lib-vc2022")

    -- 设置为 public 的链接（可选）
    -- set_policy("build.merge_archive", false) -- 避免静态库被合并进来，可根据需求修改
