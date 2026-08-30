# ---- godot-cpp, built once per runtime ------------------------------------
# godot-cpp is a thousand files behind a generator, and nothing about the
# extension's own build level reaches it: which runtime it is for
# (GODOTCPP_TARGET) is the whole of what it needs to know, and an optimised
# library links into an unoptimised extension as well as into an optimised
# one -- both take the static runtime (/MT), so MSVC's mismatch check has
# nothing to object to.
#
# So it is built here, in a tree of its own per runtime, always Release, and
# what the extension's trees get is the export below: the library, its
# include directories and the definitions its headers were compiled under,
# which the extension has to share. The root CMakeLists.txt configures and
# builds this tree the first time it finds the export missing, the way it
# fetches the checkout the first time it finds that missing.
cmake_minimum_required(VERSION 3.25)

project(lhat_godot_cpp CXX)

set(LHAT_GODOT_CPP_DIR "" CACHE PATH "The godot-cpp checkout to build.")
if(NOT EXISTS "${LHAT_GODOT_CPP_DIR}/CMakeLists.txt")
    message(FATAL_ERROR "no godot-cpp checkout at '${LHAT_GODOT_CPP_DIR}'")
endif()

add_subdirectory("${LHAT_GODOT_CPP_DIR}" godot-cpp)

# godot-cpp aims its .pdb at the consuming project's source tree
# (windows.cmake: ${CMAKE_SOURCE_DIR}/bin), which here would be a bin/ beside
# this file -- created empty, since a Release build writes no .pdb. Beside
# the .lib is where it belongs.
set_target_properties(godot-cpp PROPERTIES
    PDB_OUTPUT_DIRECTORY "$<1:${CMAKE_BINARY_DIR}/bin>")

# The target with everything PUBLIC on it -- measured to carry the include
# directories, the definitions and the link options through, the latter with
# their $<CONFIG:...> intact for the consumer to evaluate.
export(TARGETS godot-cpp NAMESPACE prebuilt::
       FILE "${CMAKE_BINARY_DIR}/godot-cpp-targets.cmake")

# What the root reads off godot-cpp besides the target: the suffix the
# library is named with (a generator expression, so it is written out here
# where it can be expanded), and two settings godot-cpp decides for whoever
# links it.
get_target_property(suffix godot-cpp GODOTCPP_SUFFIX)
file(GENERATE OUTPUT "${CMAKE_BINARY_DIR}/godot-cpp-vars.cmake" CONTENT
"set(GODOTCPP_SUFFIX \"${suffix}\")
set(GODOTCPP_SYMBOL_VISIBILITY \"${GODOTCPP_SYMBOL_VISIBILITY}\")
set(CMAKE_MSVC_RUNTIME_LIBRARY \"${CMAKE_MSVC_RUNTIME_LIBRARY}\")
")
