cmake_minimum_required(VERSION 3.6)
project(os-berries)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
aux_source_directory(src SRC_DIR)

# compile to a static library
add_subdirectory(test)

link_directories(deps)
set(SOURCE_FILES main.cpp ${SRC_DIR})
set(DEP_LIBS uv potatoes)

add_executable(berries ${SOURCE_FILES})
target_link_libraries(berries ${DEP_LIBS})