add_executable(xsstv_cli main.cpp)

target_link_libraries(xsstv_cli PRIVATE xsstv_core)

target_compile_definitions(xsstv_cli PRIVATE
    XSSTV_ROOT="${CMAKE_SOURCE_DIR}"
)