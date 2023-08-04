cmake_minimum_required(VERSION 3.26)

add_library (iismodule SHARED module.def main.cpp)
target_compile_features(iismodule PRIVATE cxx_std_20)
target_compile_options(iismodule PRIVATE /W4 /WX /O1 /Ob0 /guard:cf)
target_link_libraries(iismodule ws2_32)
target_link_options(iismodule PRIVATE /guard:cf)

add_custom_command(TARGET iismodule
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:iismodule>" "${DIST_PATH}/"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_PDB_FILE:iismodule>" "${DIST_PATH}/"
)

# DEV ONLY
add_custom_command(TARGET iismodule
    POST_BUILD
    COMMAND iisreset /stop
    COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:iismodule> "%windir%\\System32\\inetsrv\\"
    COMMAND iisreset /start
)
