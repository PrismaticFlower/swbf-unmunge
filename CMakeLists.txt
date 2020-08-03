cmake_minimum_required(VERSION 3.2)

project(unmunger)



#SETTINGS

add_executable(unmunge)

set_property(TARGET unmunge PROPERTY CXX_STANDARD 20)
set_property(TARGET unmunge PROPERTY CXX_STANDARD_REQUIRED ON)

if (NOT ${CMAKE_BUILD_TYPE} EQUAL "DEBUG")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3")
endif()

add_compile_definitions(GLM_FORCE_SWIZZLE GLM_FORCE_CXX17 GLM_FORCE_SILENT_WARNINGS)



#SOURCES

target_include_directories(unmunge PUBLIC src src/unixport)

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

#Harmless redundant options to catch errors on underconfigured systems
if (APPLE)
	target_link_options(unmunge PUBLIC -undefined dynamic_lookup)
	target_link_directories(unmunge PUBLIC /usr/local/lib/)
else()
	target_link_libraries(unmunge PUBLIC dl)
	target_link_libraries(unmunge PUBLIC "-Wl,--no-as-needed")
	target_link_directories(unmunge PUBLIC /usr/lib/x86_64-linux-gnu)
endif()

target_link_libraries(unmunge PUBLIC tbb fmt glm_static pthread)