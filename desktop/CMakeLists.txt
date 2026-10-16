cmake_minimum_required(VERSION 3.16)

# 需要 Qt6 的 Widgets、Multimedia、Concurrent 模块
find_package(Qt6 REQUIRED COMPONENTS Widgets Multimedia Concurrent)

set(CMAKE_AUTOMOC ON)

add_executable(xsstv_desktop
    main.cpp
    MainWindow.h
    MainWindow.cpp
    TitleBar.h
    TitleBar.cpp
    EncodePage.h
    EncodePage.cpp
    DecodePage.h
    DecodePage.cpp
    ResourcePage.h
    ResourcePage.cpp
    ResourceManager.h
    ResourceManager.cpp
    ImageLabel.h
    ImageLabel.cpp
    imageconvert.h
    apppaths.h
)

# 链接 Core 静态库 + Qt（不重复编译 Core 源码）
target_link_libraries(xsstv_desktop PRIVATE
    xsstv_core
    Qt6::Widgets
    Qt6::Multimedia
    Qt6::Concurrent
)

# GUI 子系统：双击运行不弹控制台黑框
set_target_properties(xsstv_desktop PROPERTIES WIN32_EXECUTABLE ON)
