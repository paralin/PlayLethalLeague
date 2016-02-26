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

project(LethalLeagueDLL)

# Find all c++ files
file(GLOB_RECURSE LethalLeagueDLL_Python_SRC "${CMAKE_CURRENT_SOURCE_DIR}/python/*.h*" "${CMAKE_CURRENT_SOURCE_DIR}/python/*.cpp")
source_group(python FILES ${LethalLeagueDLL_Python_SRC})

file(GLOB_RECURSE LethalLeagueDLL_Inject_SRC "${CMAKE_CURRENT_SOURCE_DIR}/injection/*.h*" "${CMAKE_CURRENT_SOURCE_DIR}/injection/*.cpp")
source_group(inject FILES ${LethalLeagueDLL_Inject_SRC})

file(GLOB_RECURSE LethalLeagueDLL_Caves_SRC "${CMAKE_CURRENT_SOURCE_DIR}/caves/*.h*" "${CMAKE_CURRENT_SOURCE_DIR}/caves/*.cpp")
source_group(caves FILES ${LethalLeagueDLL_Caves_SRC})

file(GLOB_RECURSE LethalLeagueDLL_Misc_SRC "${CMAKE_CURRENT_SOURCE_DIR}/misc/*.h*" "${CMAKE_CURRENT_SOURCE_DIR}/misc/*.cpp")
source_group(misc FILES ${LethalLeagueDLL_Misc_SRC})

set(LethalLeagueDLL_SRC ${LethalLeagueDLL_Python_SRC} ${LethalLeagueDLL_Inject_SRC} ${LethalLeagueDLL_Caves_SRC} ${LethalLeagueDLL_Misc_SRC})

# Include ASMJIT
set(ASMJIT_EMBED TRUE)
add_definitions(-DASMJIT_STATIC)
include("${ASMJIT_DIR}/CMakeLists.txt")

# Create a shared library with this project
add_library(LethalLeagueDLL SHARED ${LethalLeagueDLL_SRC} ${ASMJIT_SRC})

# Add include directories
include_directories(${ASMJIT_DIR}/src)
include_directories(${PYTHON_INCLUDE_DIR})
include_directories(${BOOST_INCLUDE_DIR})
include_directories(.)

# Link libraries
target_link_libraries(LethalLeagueDLL shlwapi.lib) # Required for Python
target_link_libraries(LethalLeagueDLL ${PYTHON_LIBRARY_DIR}/python34.lib)
target_link_libraries(LethalLeagueDLL debug ${BOOST_LIBRARY_DIR}/boost_python-vc140-mt-gd-1_60.lib)
target_link_libraries(LethalLeagueDLL optimized ${BOOST_LIBRARY_DIR}/boost_python-vc140-mt-1_60.lib)
target_link_libraries(LethalLeagueDLL ${ASMJIT_DEPS})

# Add preprocessor definitions
add_definitions(-DNOMINMAX -D_CRT_SECURE_NO_WARNINGS=1)
