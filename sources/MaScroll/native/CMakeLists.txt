cmake_minimum_required(VERSION 3.26)

project(native)

set(DIST_PATH ${CMAKE_SOURCE_DIR}/../../../services/MaScroll/native)

add_subdirectory(iis)
add_subdirectory(macro)
