project(SWBF-unmunge)


#
# CMake
#

cmake_minimum_required(VERSION 3.9)


#
# Paths
#

set(DIR_ROOT "${PROJECT_SOURCE_DIR}")
set(DIR_LIBRARY "${DIR_ROOT}/lib")

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_DEBUG "${DIR_LIBRARY}/deb")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_DEBUG "${DIR_LIBRARY}/deb")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${DIR_ROOT})
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_RELEASE "${DIR_LIBRARY}/rel")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_RELEASE "${DIR_LIBRARY}/rel")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${DIR_ROOT})

set(DIR_VCPKG_INSTALL "${DIR_VCPKG_ROOT}/installed/${VCPKG_TARGET_TRIPLET}")
set(DIR_VCPKG_INCLUDE "${DIR_VCPKG_INSTALL}/include")
set(DIR_VCPKG_LIBRARY "${DIR_VCPKG_INSTALL}/lib")
set(DIR_VCPKG_LIBRARY_DEBUG "${DIR_VCPKG_INSTALL}/debug/lib")


if (CMAKE_BUILD_TYPE EQUAL "DEBUG")
    set(LIBRARY_DIRECTORIES
            ${DIR_VCPKG_LIBRARY}
            ${DIR_VCPKG_LIBRARY}/deb)
else()
    set(LIBRARY_DIRECTORIES
            ${DIR_VCPKG_LIBRARY}
            ${DIR_VCPKG_LIBRARY}/rel)
endif()


#
# Compiler
#

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang") # Clang
    #
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "GNU") # GCC
    set(COMPILER_FLAGS "-Wall -Wextra -ansi -pedantic")
    set(COMPILER_FLAGS_DEBUG "-MTd")
    set(COMPILER_FLAGS_RELEASE "-O3 -DNDEBUG -MT")
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC") # MSVC
    set(COMPILER_FLAGS "")
    set(COMPILER_FLAGS_DEBUG "/Zi /Ob0 /Od /RTC1 /MDd")
    set(COMPILER_FLAGS_RELEASE "/O2 /Ob2 /DNDEBUG /MD")
endif ()

set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} ${COMPILER_FLAGS} ${COMPILER_FLAGS_DEBUG}")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} ${COMPILER_FLAGS} ${COMPILER_FLAGS_RELEASE}")

add_compile_definitions(NOMINMAX)
add_compile_definitions(GLM_FORCE_SWIZZLE)


#
# Link Dependencies
#

link_directories(${DIR_LIBRARY} ${LIBRARY_DIRECTORIES})


find_package(fmt CONFIG REQUIRED)
find_package(directxtex CONFIG REQUIRED)
find_package(Microsoft.GSL CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(TBB CONFIG REQUIRED)


#
# Includes
#

include_directories(src)

SET(FILES
        src/app_options.cpp
        src/assemble_chunks.cpp
        src/chunk_processor.cpp
        src/explode_chunk.cpp
        src/file_saver.cpp
        src/handle_cloth.cpp
        src/handle_collision.cpp
        src/handle_config.cpp
        src/handle_localization.cpp
        src/handle_lvl_child.cpp
        src/handle_misc.cpp
        src/handle_model.cpp
        src/handle_object.cpp
        src/handle_path.cpp
        src/handle_planning.cpp
        src/handle_planning_swbf1.cpp
        src/handle_primitives.cpp
        src/handle_skeleton.cpp
        src/handle_terrain.cpp
        src/handle_texture.cpp
        src/handle_texture_ps2.cpp
        src/handle_texture_xbox.cpp
        src/handle_ucfb.cpp
        src/handle_unknown.cpp
        src/handle_world.cpp
        src/mapped_file.cpp
        src/model_builder.cpp
        src/model_gltf_save.cpp
        src/model_msh_save.cpp
        src/model_scene.cpp
        src/model_topology_converter.cpp
        src/save_image.cpp
        src/save_image_tga.cpp
        src/swbf_fnv_hashes.cpp
        src/terrain_builder.cpp
        src/ucfb_builder.cpp
        src/ucfb_reader.cpp
        src/vbuf_reader.cpp
        src/lua/lua4_decompiler.cpp
        )

#
# Builds
#

# Build and link 'SWBF-unmunge' binary
add_executable(${PROJECT_NAME} ${FILES} src/main.cpp)
target_link_libraries(${PROJECT_NAME} ${LIBRARIES})
target_link_libraries(${PROJECT_NAME} PRIVATE fmt::fmt)     # fmt::fmt-header-only
target_link_libraries(${PROJECT_NAME} PRIVATE glm::glm)
target_link_libraries(${PROJECT_NAME} PRIVATE Microsoft::DirectXTex)
target_link_libraries(${PROJECT_NAME} PRIVATE Microsoft.GSL::GSL)
target_link_libraries(${PROJECT_NAME} PRIVATE nlohmann_json nlohmann_json::nlohmann_json)
target_link_libraries(${PROJECT_NAME} PRIVATE TBB::tbb)
