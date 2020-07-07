cmake_minimum_required(VERSION 3.2)

project(unmunging)

option(BUILD_SHARED_LIB "Build shared lib" OFF)
option(BUILD_STATIC_LIB "Build static lib" OFF)

if (BUILD_SHARED_LIB)
	add_library(unmunge SHARED)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC") #not sure how this works with executables
elseif (BUILD_STATIC_LIB)
	add_library(unmunge STATIC)
else()
	add_executable(unmunge)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC") #not sure how this works with executables
endif()

set_property(TARGET unmunge PROPERTY CXX_STANDARD 20)
set_property(TARGET unmunge PROPERTY CXX_STANDARD_REQUIRED ON)
set_property(TARGET unmunge PROPERTY CMAKE_CXX_EXTENSIONS ON)

add_compile_definitions(GLM_FORCE_SWIZZLE)
add_compile_definitions(GLM_FORCE_CXX17)
add_compile_definitions(GLM_FORCE_SILENT_WARNINGS)
add_compile_definitions(NOMINMAX)

if (UNIX AND NOT APPLE)
	target_link_libraries(unmunge PUBLIC "-Wl,--no-as-needed")
elseif(APPLE)
	target_link_options(unmunge PUBLIC -undefined dynamic_lookup)
endif()



include_directories(unmunge PUBLIC src)
include_directories(unmunge PRIVATE /home/will/projects/cpprtx/libs/glm)


file(GLOB SOURCE_FILES src/*.cpp)
file(GLOB TEXTURE_FILES src/handle_texture*)
file(GLOB IMAGE_FILES src/save_image*)
file(GLOB WINDOWS_DEP src/mapped_file.cpp)

list(REMOVE_ITEM SOURCE_FILES ${TEXTURE_FILES})
list(REMOVE_ITEM SOURCE_FILES ${IMAGE_FILES})
list(REMOVE_ITEM SOURCE_FILES ${WINDOWS_DEP})

message(STATUS "Filtered source files: ${SOURCE_FILES}")

target_sources(unmunge PUBLIC ${SOURCE_FILES})

if (UNIX AND NOT APPLE)
	target_link_libraries(unmunge PUBLIC -ldl) #handled automatically on Mac
endif()

target_link_libraries(unmunge PUBLIC -lpthread)
target_link_libraries(unmunge PUBLIC m)

target_link_directories(unmunge PUBLIC /usr/lib/x86_64-linux-gnu)
target_link_libraries(unmunge PUBLIC libboost_iostreams)

target_link_directories(unmunge PUBLIC /home/will/projects/cpprtx/libs/glm/build/glm)
target_link_libraries(unmunge PUBLIC glm_static)