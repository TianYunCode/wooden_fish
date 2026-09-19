cmake_minimum_required(VERSION 3.16)
project(wooden_fish LANGUAGES CXX RC)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(muyu WIN32
    src/main.cpp
    src/core/util.cpp
    src/core/settings.cpp
    src/core/autorun.cpp
    src/media/assets.cpp
    src/media/audio_engine.cpp
    src/render/skins.cpp
    src/render/painter.cpp
    src/ui/main_window.cpp
    src/ui/menu.cpp
    resources/app.rc
)

target_include_directories(muyu PRIVATE src resources)

target_compile_definitions(muyu PRIVATE
    UNICODE _UNICODE
    WINVER=0x0A00 _WIN32_WINNT=0x0A00
)

# 源文件即 UTF-8；/utf-8 同时让窄字面量按 UTF-8 编码，与 U8() 转换一致
target_compile_options(muyu PRIVATE /utf-8)

target_link_libraries(muyu PRIVATE
    gdiplus mfplat mfreadwrite mfuuid xaudio2
    shell32 shlwapi ole32 user32 gdi32 advapi32
)

set(OUT_DIR "${CMAKE_SOURCE_DIR}/build")
set_target_properties(muyu PROPERTIES OUTPUT_NAME "电子木鱼")
foreach(cfg Debug Release RelWithDebInfo MinSizeRel)
    string(TOUPPER ${cfg} UC)
    set_target_properties(muyu PROPERTIES RUNTIME_OUTPUT_DIRECTORY_${UC} "${OUT_DIR}")
endforeach()
