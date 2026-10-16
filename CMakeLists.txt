cmake_minimum_required(VERSION 3.16)

project(XSSTV VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(core)
add_subdirectory(cli)

# Qt 桌面端（默认关闭；需要时用 -DXSSTV_BUILD_DESKTOP=ON 打开，避免没配 Qt 时构建失败）
option(XSSTV_BUILD_DESKTOP "Build the Qt desktop application" OFF)
if(XSSTV_BUILD_DESKTOP)
    add_subdirectory(desktop)
endif()