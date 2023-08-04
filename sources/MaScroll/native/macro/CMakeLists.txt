cmake_minimum_required(VERSION 3.26)

add_subdirectory(lib)

add_executable (macro builtins.cpp exec.cpp main.cpp objects.cpp parse.cpp util.cpp)
target_compile_features(macro PRIVATE cxx_std_20)
target_compile_options(macro PRIVATE /W4 /WX /O1 /Ob0 /guard:cf)
target_link_libraries(macro bcrypt bcryptprimitives crypt32 winhttp)
target_link_options(macro PRIVATE /guard:cf)

add_custom_command(TARGET macro
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:macro>" "${DIST_PATH}/"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_PDB_FILE:macro>" "${DIST_PATH}/"
)

# DEV ONLY
add_custom_command(TARGET macro
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:macro> "%windir%\\System32\\inetsrv\\"
)
