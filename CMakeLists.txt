cmake_minimum_required(VERSION 3.2)

project(unmunger)

option(BUILD_SHARED_LIB "Build shared lib" OFF)
option(BUILD_STATIC_LIB "Build static lib" OFF)
option(DEBUG "Debug build" OFF)






#SETTINGS

if (BUILD_SHARED_LIB)
	add_library(unmunge SHARED)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC")
elseif (BUILD_STATIC_LIB)
	add_library(unmunge STATIC)
else()
	add_executable(unmunge)
endif()

set_property(TARGET unmunge PROPERTY CXX_STANDARD 20)
set_property(TARGET unmunge PROPERTY CXX_STANDARD_REQUIRED ON)

if (DEBUG)
	set(CMAKE_BUILD_TYPE Debug)
else()
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3")
endif()

add_compile_definitions(GLM_FORCE_SWIZZLE)
add_compile_definitions(GLM_FORCE_CXX17)
add_compile_definitions(GLM_FORCE_SILENT_WARNINGS)





#SOURCES

target_include_directories(unmunge PUBLIC src)
target_include_directories(unmunge PUBLIC src/unixport)

if(APPLE)
	target_include_directories(unmunge PUBLIC /usr/local/include)
endif()

file(GLOB SOURCE_FILES src/*.cpp)
file(GLOB SOURCE_FILES_UNIX src/unixport/*.cpp)
file(GLOB TEXTURE_FILES src/handle_texture*)
file(GLOB IMAGE_FILES src/save_image*)
file(GLOB WINDOWS_DEP src/mapped_file.cpp)
file(GLOB DETEX_FILES src/unixport/detex/*.c)

list(REMOVE_ITEM SOURCE_FILES ${TEXTURE_FILES})
list(REMOVE_ITEM SOURCE_FILES ${IMAGE_FILES})
list(APPEND SOURCE_FILES ${SOURCE_FILES_UNIX})
list(APPEND SOURCE_FILES ${DETEX_FILES})
list(REMOVE_ITEM SOURCE_FILES ${WINDOWS_DEP})
list(APPEND SOURCE_FILES ${DETEX_FILES})

target_sources(unmunge PUBLIC ${SOURCE_FILES})





#LINKING

if (APPLE)
	target_link_options(unmunge PUBLIC -undefined dynamic_lookup)
else()
	target_link_libraries(unmunge PUBLIC -ldl)
	target_link_libraries(unmunge PUBLIC "-Wl,--no-as-needed")
endif()

if (APPLE)
	target_link_directories(unmunge PUBLIC /usr/local/lib/)
else()
	target_link_directories(unmunge PUBLIC /usr/lib/x86_64-linux-gnu)
endif()

target_link_libraries(unmunge PUBLIC tbb)
target_link_libraries(unmunge PUBLIC fmt)
target_link_libraries(unmunge PUBLIC glm_static)
target_link_libraries(unmunge PUBLIC -lpthread)
target_link_libraries(unmunge PUBLIC m)