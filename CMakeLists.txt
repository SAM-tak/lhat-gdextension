# ---- The Godot side of L^ --------------------------------------------------
# A GDExtension is a shared library the engine loads at run time and asks for
# classes. It is written against godot-cpp, so this directory is the one part
# of the tree that is C++ -- the language stays C11 and is reached from here
# through include/lhat.h, the same surface any other host names (05 の 8.7).
#
# Two things follow from that and shape everything below:
#
#   * godot-cpp is a dependency from outside the tree, and a large one: it
#     generates its bindings from extension_api.json before it compiles. It is
#     fetched once into a directory shared by every preset rather than per
#     build tree.
#   * the engine decides which file to load from its own name, so the library
#     is named the way godot-cpp names things and is written straight into the
#     demo project's bin/, where demo/lhat.gdextension already points.
cmake_minimum_required(VERSION 3.25)

project(lhat_gdextension C CXX)

# ---- All three runtimes at once --------------------------------------------
# Godot loads a different library for each of three runtimes -- the editor, a
# debug export, a release export -- and which one a tree builds is
# GODOTCPP_TARGET, a cache variable of godot-cpp's. So each is a configure of
# its own, and a preset, being one configure, cannot span them.
#
# This option makes the tree a driver instead: it holds nothing of its own and
# configures and builds the three under build/<target>-<level>, which are the
# very trees the per-runtime presets use, so nothing is built twice. `level`
# is the optimisation, named as the presets name it.
#
#     cmake --preset release && cmake --build --preset release
#
# builds the three optimised libraries into demo/bin. Whatever was set here
# for the core and godot-cpp is handed down; what was not is left to each
# tree's own default, which is the same one.
option(LHAT_ALL_RUNTIMES
    "Build editor, template_debug and template_release together" OFF)
if(LHAT_ALL_RUNTIMES)
    include(ExternalProject)
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        set(level debug)
    elseif(CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
        set(level fastdebug)
    else()
        set(level release)
    endif()
    set(forwarded "")
    foreach(name LHAT_CORE_DIR LHAT_GODOT_CPP_DIR LHAT_GODOT_CPP_TAG)
        if(DEFINED CACHE{${name}})
            list(APPEND forwarded "-D${name}=${${name}}")
        endif()
    endforeach()
    foreach(target editor template_debug template_release)
        ExternalProject_Add(${target}
            SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}"
            BINARY_DIR "${CMAKE_CURRENT_SOURCE_DIR}/build/${target}-${level}"
            CMAKE_ARGS
                -G "${CMAKE_GENERATOR}"
                -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
                -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
                -DGODOTCPP_TARGET=${target}
                -DLHAT_ALL_RUNTIMES=OFF
                ${forwarded}
            # The sub-build is incremental on its own; this only says to ask
            # it every time rather than once.
            BUILD_ALWAYS ON
            INSTALL_COMMAND ""
        )
    endforeach()
    return()
endif()

# コアは兄弟に在るものをそのまま使う。godot-cpp に対して取っているのと同じ
# 立場で、取得は存在させる手段でしかない（../godot も ../lua-5.5.1 も同じ並び）。
set(LHAT_CORE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../lhat"
    CACHE PATH "Where the L^ core lives.")
if(NOT EXISTS "${LHAT_CORE_DIR}/CMakeLists.txt")
    message(FATAL_ERROR
        "no L^ core at ${LHAT_CORE_DIR}. Clone it beside this one, or point "
        "LHAT_CORE_DIR at a checkout.")
endif()

# 拡張が要るのはライブラリだけ。試験も CLI も言語サーバも stdlib も要らない
# ——いまの godot-* プリセットが立てていたのと同じ組み合わせ。
set(LHAT_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(LHAT_BUILD_CLI OFF CACHE BOOL "" FORCE)
set(LHAT_BUILD_LSP OFF CACHE BOOL "" FORCE)
set(LHAT_BUILD_STDLIB OFF CACHE BOOL "" FORCE)
add_subdirectory("${LHAT_CORE_DIR}" lhat-core)

set(LHAT_GODOT_CPP_DIR
    "${CMAKE_SOURCE_DIR}/build/godot-cpp"
    CACHE PATH
    "Where godot-cpp lives. Fetched there unless it is a checkout already."
)
set(LHAT_GODOT_CPP_TAG
    "godot-4.5-stable"
    CACHE STRING
    "godot-cpp tag to fetch. Matches the Godot version the extension loads in."
)

# Fetching is only ever how the directory comes to exist; what is built is
# always the checkout that is there. FetchContent would put the two cases in
# different binary directories, and moving between them rebuilds two thousand
# generated files for no reason.
if(NOT EXISTS "${LHAT_GODOT_CPP_DIR}/CMakeLists.txt")
    message(STATUS "godot-cpp: fetching ${LHAT_GODOT_CPP_TAG} into ${LHAT_GODOT_CPP_DIR}")
    find_package(Git REQUIRED)
    execute_process(
        COMMAND ${GIT_EXECUTABLE} clone --depth 1 --branch ${LHAT_GODOT_CPP_TAG}
                https://github.com/godotengine/godot-cpp.git
                "${LHAT_GODOT_CPP_DIR}"
        RESULT_VARIABLE fetched
    )
    if(NOT fetched EQUAL 0)
        message(FATAL_ERROR
            "could not fetch godot-cpp ${LHAT_GODOT_CPP_TAG}. Point "
            "LHAT_GODOT_CPP_DIR at a checkout, or clone one there yourself.")
    endif()
endif()

# An existing checkout is used as it stands -- that is what pointing
# LHAT_GODOT_CPP_DIR somewhere else is for, and the version there is the
# version the extension is built against, tag or no tag.
#
# And built once per runtime, not once per tree. Which runtime a library is
# for is all godot-cpp needs to know, so it is built in cmake/godot-cpp's
# tree, always optimised, and every level of this tree links the same
# library: the debug build of the extension is not a debug build of
# godot-cpp. As with the checkout, building it is only ever how the export
# comes to exist -- a godot-cpp that changed underneath is rebuilt by
# removing its tree.
set(GODOTCPP_TARGET "template_debug" CACHE STRING
    "Which runtime this is for: editor, template_debug or template_release.")
set(LHAT_GODOT_CPP_BUILD_DIR
    "${CMAKE_SOURCE_DIR}/build/godot-cpp-${GODOTCPP_TARGET}"
    CACHE PATH "Where godot-cpp for this runtime is built, once.")
if(NOT EXISTS "${LHAT_GODOT_CPP_BUILD_DIR}/godot-cpp-targets.cmake")
    message(STATUS "godot-cpp: building ${GODOTCPP_TARGET} into ${LHAT_GODOT_CPP_BUILD_DIR}")
    execute_process(
        COMMAND ${CMAKE_COMMAND}
                -S "${CMAKE_CURRENT_SOURCE_DIR}/cmake/godot-cpp"
                -B "${LHAT_GODOT_CPP_BUILD_DIR}"
                -G "${CMAKE_GENERATOR}"
                -DCMAKE_BUILD_TYPE=Release
                -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}
                -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}
                -DGODOTCPP_TARGET=${GODOTCPP_TARGET}
                -DLHAT_GODOT_CPP_DIR=${LHAT_GODOT_CPP_DIR}
        RESULT_VARIABLE configured
    )
    if(configured EQUAL 0)
        execute_process(
            COMMAND ${CMAKE_COMMAND} --build "${LHAT_GODOT_CPP_BUILD_DIR}"
            RESULT_VARIABLE configured
        )
    endif()
    if(NOT configured EQUAL 0)
        message(FATAL_ERROR "could not build godot-cpp for ${GODOTCPP_TARGET}")
    endif()
endif()
include("${LHAT_GODOT_CPP_BUILD_DIR}/godot-cpp-vars.cmake")
include("${LHAT_GODOT_CPP_BUILD_DIR}/godot-cpp-targets.cmake")
# The one library serves every level, so a Debug or RelWithDebInfo tree is
# told to take the Release location rather than look for one of its own.
set_target_properties(prebuilt::godot-cpp PROPERTIES
    MAP_IMPORTED_CONFIG_DEBUG Release
    MAP_IMPORTED_CONFIG_RELWITHDEBINFO Release)
add_library(godot-cpp ALIAS prebuilt::godot-cpp)

# ---- What linking a static C library into a shared library asks for --------
# Both of these are about `lhat` having been defined long before godot-cpp was
# read, so neither could have been settled where the targets are made.
set_property(TARGET lhat lhatport PROPERTY POSITION_INDEPENDENT_CODE ON)

# godot-cpp links the MSVC runtime statically by default (GODOTCPP_USE_STATIC_CPP),
# and the linker refuses a mix. Its choice is the one that reaches the engine,
# so the language follows it rather than the other way round -- which is also
# why the godot-* presets build the extension alone, with no cli, server or
# tests in the same tree to disagree.
if(MSVC)
    set_property(TARGET lhat lhatport
        PROPERTY MSVC_RUNTIME_LIBRARY "${CMAKE_MSVC_RUNTIME_LIBRARY}")
endif()

# ---- The extension ---------------------------------------------------------
# The icon goes into the binary rather than beside it: what names a .lh in the
# editor has to be there in any project the extension is added to, and a path
# into one project's files would not be. Read at configure time, so touching
# the drawing means reconfiguring -- which is what CONFIGURE_DEPENDS says.
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/lhat-script.svg")
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/lhat-script.svg" LHAT_SCRIPT_ICON_SVG)
configure_file(src/lhat_icon.h.in "${CMAKE_CURRENT_BINARY_DIR}/lhat_icon.h" @ONLY)

add_library(lhat_gdextension SHARED
    src/register_types.cpp
    src/lhat_host.cpp
    src/lhat_godot_module.cpp
    src/lhat_godot_api.gen.cpp
    src/lhat_variant.cpp
    src/lhat_runtime.cpp
    src/lhat_script.cpp
    src/lhat_instance.cpp
    src/lhat_language.cpp
    src/lhat_highlighter.cpp
    src/lhat_receiver.cpp
    src/lhat_godot_values.cpp
    src/lhat_godot_handles.cpp
    src/lhat_godot_packed.cpp
    src/lhat_godot_containers.cpp
    src/lhat_godot_util.cpp
    src/lhat_debugger.cpp
    src/lhat_resource_format.cpp
)

target_link_libraries(lhat_gdextension PRIVATE lhat godot-cpp)
# Where the generated lhat_icon.h lands.
target_include_directories(lhat_gdextension PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")

# LhatString ends in a flexible array member, which C has and C++ does not --
# every compiler here takes it, and each has its own way of being asked to stop
# saying so.
if(MSVC)
    target_compile_options(lhat_gdextension PRIVATE /wd4200)
else()
    target_compile_options(lhat_gdextension PRIVATE -Wno-pedantic)
endif()

# The engine reads the platform, the Godot target and the architecture out of
# the file name, and godot-cpp already assembles that suffix for its own
# library. Taking it from there is what keeps demo/lhat.gdextension right.
set(LHAT_GD_SUFFIX "${GODOTCPP_SUFFIX}")  # from godot-cpp-vars.cmake
set(LHAT_GD_BIN_DIR "${CMAKE_CURRENT_SOURCE_DIR}/demo/bin")

set_target_properties(lhat_gdextension PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
    CXX_VISIBILITY_PRESET ${GODOTCPP_SYMBOL_VISIBILITY}

    POSITION_INDEPENDENT_CODE ON
    BUILD_RPATH_USE_ORIGIN ON

    # Wrapped in a generator expression so that the multi-config generators do
    # not add a configuration directory of their own underneath.
    LIBRARY_OUTPUT_DIRECTORY "$<1:${LHAT_GD_BIN_DIR}>"
    RUNTIME_OUTPUT_DIRECTORY "$<1:${LHAT_GD_BIN_DIR}>"
    ARCHIVE_OUTPUT_DIRECTORY "$<1:${LHAT_GD_BIN_DIR}>"
    PDB_OUTPUT_DIRECTORY "$<1:${LHAT_GD_BIN_DIR}>"

    PREFIX "lib"
    OUTPUT_NAME "lhat${LHAT_GD_SUFFIX}"

    # What the linker leaves beside the library that CMake's clean does not
    # know about -- measured: `--target clean` took the .dll and the .lib and
    # left these three, and a .pdb from a debug level would then sit next to
    # whatever optimised library replaced the .dll.
    ADDITIONAL_CLEAN_FILES
        "${LHAT_GD_BIN_DIR}/liblhat${LHAT_GD_SUFFIX}.pdb;${LHAT_GD_BIN_DIR}/liblhat${LHAT_GD_SUFFIX}.ilk;${LHAT_GD_BIN_DIR}/lhat${LHAT_GD_SUFFIX}.exp"
)
