cmake_minimum_required(VERSION 3.15)
project(JRoaring C)

set(CMAKE_C_STANDARD 11)

include_directories(
        includes
        includes/java/jdk
        includes/java/jdk/win32
)

link_directories(lib)

add_library(JRoaring SHARED library.c hash_map.c MurmurHash3.c)
add_executable(JRoaringTest hash_map.c MurmurHash3.c test.c)

target_link_libraries(JRoaring PRIVATE Roaring)
#target_link_libraries(JRoaringTest PRIVATE RoaringBitmap)