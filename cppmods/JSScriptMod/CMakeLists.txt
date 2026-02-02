project(JSScriptMod)

set(TARGET JSScriptMod)

# QuickJS source files
set(QUICKJS_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/deps/quickjs/quickjs.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/deps/quickjs/quickjs-libc.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/deps/quickjs/cutils.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/deps/quickjs/libregexp.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/deps/quickjs/libunicode.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/deps/quickjs/dtoa.c"
)

# Module source files
set(${TARGET}_Sources
    "${CMAKE_CURRENT_SOURCE_DIR}/src/dllmain.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/JSMod.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/JSType/JSUObject.cpp"
    ${QUICKJS_SOURCES}
)

add_library(${TARGET} SHARED ${${TARGET}_Sources})

target_include_directories(${TARGET} PRIVATE 
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/deps/quickjs"
)

# QuickJS compile definitions
target_compile_definitions(${TARGET} PRIVATE 
    CONFIG_VERSION="2024-01-13"
    _GNU_SOURCE
)

# Disable some warnings for QuickJS C code on MSVC
if(MSVC)
    set_source_files_properties(${QUICKJS_SOURCES} PROPERTIES
        COMPILE_FLAGS "/wd4244 /wd4267 /wd4996 /wd4018 /wd4146 /wd4334"
    )
endif()

target_link_libraries(${TARGET} PUBLIC UE4SS)
