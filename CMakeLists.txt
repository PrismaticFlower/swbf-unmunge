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
#set_property(TARGET unmunge PROPERTY CMAKE_CXX_EXTENSIONS ON)

add_compile_definitions(GLM_FORCE_SWIZZLE)
add_compile_definitions(GLM_FORCE_CXX17)
add_compile_definitions(GLM_FORCE_SILENT_WARNINGS)
add_compile_definitions(FX_GLTF_HAS_CPP_17)


if (UNIX AND NOT APPLE)
	target_link_libraries(unmunge PUBLIC "-Wl,--no-as-needed")
elseif(APPLE)
	target_link_options(unmunge PUBLIC -undefined dynamic_lookup)
	target_compile_options(unmunge PUBLIC "-Wno-user-defined-literals")
endif()

if (APPLE)
	find_package(OpenCV REQUIRED)
	include_directories(unmunge PUBLIC src)
	include_directories(unmunge PUBLIC /Users/will/projects/cpprtx/libs/glm)
else ()
	find_package(PkgConfig REQUIRED)
	pkg_check_modules(OpenCV REQUIRED opencv)
endif()

target_include_directories(unmunge PUBLIC ${OpenCV_INCLUDE_DIRS})


include_directories(unmunge PUBLIC /usr/local/include)
#include_directories(unmunge PUBLIC /home/will/projects/cpprtx/libs/glm)
#include_directories(unmunge PUBLIC /home/will/projects/swbf2wine/gli)


file(GLOB SOURCE_FILES src/*.cpp)
file(GLOB SOURCE_FILES_UNIX src/*unix*)
file(GLOB TEXTURE_FILES src/handle_texture*)
file(GLOB IMAGE_FILES src/save_image*)
file(GLOB WINDOWS_DEP src/mapped_file.cpp)
file(GLOB GLTF_FILES src/*gltf*)

list(REMOVE_ITEM SOURCE_FILES ${TEXTURE_FILES})
list(REMOVE_ITEM SOURCE_FILES ${IMAGE_FILES})
list(REMOVE_ITEM SOURCE_FILES ${WINDOWS_DEP})
list(REMOVE_ITEM SOURCE_FILES ${GLTF_FILES})
list(APPEND SOURCE_FILES ${SOURCE_FILES_UNIX})

message(STATUS "Filtered source files (UNIX): ${SOURCE_FILES_UNIX}")

target_sources(unmunge PUBLIC ${SOURCE_FILES})

if (UNIX AND NOT APPLE)
	target_link_libraries(unmunge PUBLIC -ldl) #handled automatically on Mac
endif()

target_include_directories(unmunge PUBLIC /Users/will/projects/swbf2/gli)

target_link_directories(unmunge PUBLIC ${OpenCV_LIBRARY_DIRS})
target_link_libraries(unmunge PUBLIC ${OpenCV_LIBRARIES})

target_link_libraries(unmunge PUBLIC -lpthread)
target_link_libraries(unmunge PUBLIC m)


if(NOT APPLE)

target_link_libraries(unmunge PUBLIC /usr/lib/x86_64-linux-gnu/libtbb.so)
target_link_libraries(unmunge PUBLIC /usr/lib/x86_64-linux-gnu/libboost_iostreams.so.1.65.1)
target_link_libraries(unmunge PUBLIC /usr/lib/libfmt.a)
target_link_directories(unmunge PUBLIC /home/will/projects/cpprtx/libs/glm/build/glm)

else()

target_link_directories(unmunge PUBLIC /usr/local/lib/)

target_link_libraries(unmunge PUBLIC -ltbb)
target_link_libraries(unmunge PUBLIC -lboost_iostreams)
target_link_libraries(unmunge PUBLIC -lfmt)
target_link_directories(unmunge PUBLIC /Users/will/projects/cpprtx/libs/glm/build/glm)

endif()

target_link_libraries(unmunge PUBLIC glm_static)