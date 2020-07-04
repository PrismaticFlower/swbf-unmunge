cmake_minimum_required(VERSION 3.2)

project(unmunging)

OPTION(xbox)
OPTION(ps2)


if (BUILD_SHARED_LIB)
	add_library(unmunge SHARED)
elif(BUILD_STATIC_LIB)
	add_library(unmunge STATIC)
else()
	add_executable(unmunge SHARED)
endif()

set_property(TARGET basement PROPERTY CXX_STANDARD 17)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC") #not sure how this works with executables


if (UNIX AND NOT APPLE)
	target_link_libraries(basement PUBLIC "-Wl,--no-as-needed")
elseif(APPLE)
	target_link_options(basement PUBLIC -undefined dynamic_lookup)
endif()

find_package(PkgConfig REQUIRED)


include_directories(unmunge PUBLIC src)

add_subdirectory(src)




if (UNIX AND NOT APPLE)
	target_link_libraries(basement PUBLIC -ldl) #handled automatically on Mac
endif()

target_link_libraries(basement PUBLIC -lpthread)
target_link_libraries(basement PUBLIC m)