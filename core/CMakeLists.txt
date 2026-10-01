add_library(xsstv_core STATIC
    src/image/image_io.cpp
)

target_include_directories(xsstv_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_include_directories(xsstv_core PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/third_party/stb
)