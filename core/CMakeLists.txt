add_library(xsstv_core STATIC
    src/sstv/robot36/Robot36Encoder.cpp
)

target_include_directories(xsstv_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)