add_executable(xsstv main.cpp)

target_link_libraries(xsstv PRIVATE xsstv_core)

target_compile_definitions(xsstv PRIVATE
    XSSTV_ROOT="${CMAKE_SOURCE_DIR}"
)