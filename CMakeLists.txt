cmake_minimum_required(VERSION 2.8)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR})

set(PYTHON_DIR "" CACHE FILEPATH "Path to Python")
set(BOOST_INCLUDE_DIR "" CACHE FILEPATH "Path to boost directory")
set(BOOST_LIBRARY_DIR "" CACHE FILEPATH "Path to boost libs")
set(ASMJIT_DIR "${PROJECT_SOURCE_DIR}/thirdparty/asmjit" CACHE FILEPATH "Path to ASMJIT's directory that contains CMakeLists.txt")

# Make sure all the paths are valid
set(PYTHON_INCLUDE_DIR "${PYTHON_DIR}/include")
set(PYTHON_LIBRARY_DIR "${PYTHON_DIR}/libs")

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

# Include ASMJIT
set(ASMJIT_EMBED TRUE)
add_definitions(-DASMJIT_STATIC)
include("${ASMJIT_DIR}/CMakeLists.txt")

# Include directories
include_directories(${ASMJIT_DIR}/src)
include_directories(${PYTHON_INCLUDE_DIR})
include_directories(${BOOST_INCLUDE_DIR})

# Set up some python paths
STRING(REGEX REPLACE "\\\\" "/" PYTHON_DIR ${PYTHON_DIR}) 
add_definitions(-DLL_PYTHON_HOME="${PYTHON_DIR}")

project(LethalLeagueAI)

add_subdirectory(PlayLethalLeague)
INCLUDE_EXTERNAL_MSPROJECT(LethalLeagueScripts scripts/scripts.pyproj)
add_subdirectory(TestLoadLethalLeague)
