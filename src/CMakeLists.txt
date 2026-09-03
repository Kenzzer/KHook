add_library(khook_lib SHARED
    "detour.cpp"
    "ranges.cpp"
)

target_include_directories(khook_lib PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

target_compile_definitions(khook_lib PRIVATE
    KHOOK_STANDALONE
    KHOOK_EXPORT
)

target_link_libraries(khook_lib PUBLIC safetyhook)

add_executable(khook
    "main.cpp"
)

target_compile_definitions(khook PRIVATE
    KHOOK_STANDALONE
)

target_link_libraries(khook PRIVATE khook_lib)