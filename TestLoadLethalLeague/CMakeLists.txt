cmake_minimum_required(VERSION 2.8)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR})

set(PYTHON_INCLUDE_DIR "" CACHE FILEPATH "Path to Python includes")
set(PYTHON_LIBRARY_DIR "" CACHE FILEPATH "Path to Python libs where python34.lib resides")
set(BOOST_INCLUDE_DIR "" CACHE FILEPATH "Path to boost directory")
set(BOOST_LIBRARY_DIR "" CACHE FILEPATH "Path to boost libs")
Set(ASMJIT_DIR "${PROJECT_SOURCE_DIR}/thirdparty/asmjit" CACHE FILEPATH "Path to ASMJIT's directory that contains CMakeLists.txt")

# Make sure all the paths are valid

if(NOT EXISTS "${PYTHON_INCLUDE_DIR}/Python.h")
    message(SEND_ERROR "Can not find Python.h in ${PYTHON_INCLUDE_DIR} (cmake -D PYTHON_INCLUDE_DIR=<PATH> missing?)")
endif()

if(NOT EXISTS "${PYTHON_LIBRARY_DIR}/python34.lib")
    message(SEND_ERROR "Can not find python34.lib in ${PYTHON_LIBRARY_DIR} (cmake -D PYTHON_LIBRARY_DIR=<PATH> missing?)")
endif()

if(NOT EXISTS "${BOOST_INCLUDE_DIR}/boost/python.hpp")
    message(SEND_ERROR "Can not find boost/python.hpp in ${BOOST_INCLUDE_DIR} (cmake -D BOOST_INCLUDE_DIR=<PATH> missing?)")
endif()

if(NOT EXISTS "${BOOST_LIBRARY_DIR}/boost_python-vc140-mt-gd-1_60.lib" OR
   NOT EXISTS "${BOOST_LIBRARY_DIR}/boost_python-vc140-mt-1_60.lib")
    message(SEND_ERROR "Can not find boost_python-vc140-mt-gd-1_60.lib or boost_python-vc140-mt-1_60.lib in ${BOOST_LIBRARY_DIR} (cmake -D BOOST_LIBRARY_DIR=<PATH> missing?)")
endif()

if(NOT EXISTS "${ASMJIT_DIR}/src/asmjit/asmjit.h")
    message(SEND_ERROR "Can not find src/asmjit/asmjit.h in ${ASMJIT_DIR} (cmake -D ASMJIT_DIR=<PATH> missing?)")
endif()

# Find all c++ files of the DLL
file(GLOB_RECURSE LethalLeagueDLL_SRC "${PROJECT_SOURCE_DIR}/PlayLethalLeague/*.h" "${PROJECT_SOURCE_DIR}/PlayLethalLeague/*.hpp" "${PROJECT_SOURCE_DIR}/PlayLethalLeague/*.cpp")

project(LethalLeagueTestLoad)

# Include ASMJIT
set(ASMJIT_EMBED TRUE)
add_definitions(-DASMJIT_STATIC)
include("${ASMJIT_DIR}/CMakeLists.txt")

# Create a shared library with this project
add_executable(LethalLeagueTestLoad TestLoadLethalLeague.cpp ${LethalLeagueDLL_SRC} ${ASMJIT_SRC})

# Add include directories
include_directories(${ASMJIT_DIR}/src)
include_directories(${PYTHON_INCLUDE_DIR})
include_directories(${BOOST_INCLUDE_DIR})
include_directories(../PlayLethalLeague)

# Link libraries
target_link_libraries(LethalLeagueTestLoad shlwapi.lib) # Required for Python
target_link_libraries(LethalLeagueTestLoad ${PYTHON_LIBRARY_DIR}/python34.lib)
target_link_libraries(LethalLeagueTestLoad debug ${BOOST_LIBRARY_DIR}/boost_python-vc140-mt-gd-1_60.lib)
target_link_libraries(LethalLeagueTestLoad optimized ${BOOST_LIBRARY_DIR}/boost_python-vc140-mt-1_60.lib)
target_link_libraries(LethalLeagueTestLoad ${ASMJIT_DEPS})

# Add preprocessor definitions
add_definitions(-DNOMINMAX)
