cmake_minimum_required(VERSION 2.8)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR})

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

# Create a shared library with this project
add_library(LethalLeagueDLL SHARED ${LethalLeagueDLL_SRC} ${ASMJIT_SRC})

# Add include directories
include_directories(.)

# Link libraries
target_link_libraries(LethalLeagueDLL shlwapi.lib) # Required for Python
target_link_libraries(LethalLeagueDLL ${PYTHON_LIBRARY_DIR}/python34.lib)
target_link_libraries(LethalLeagueDLL debug ${BOOST_LIBRARY_DIR}/boost_python-vc140-mt-gd-1_60.lib)
target_link_libraries(LethalLeagueDLL optimized ${BOOST_LIBRARY_DIR}/boost_python-vc140-mt-1_60.lib)
target_link_libraries(LethalLeagueDLL ${ASMJIT_DEPS})

# Add preprocessor definitions
add_definitions(-DNOMINMAX -D_CRT_SECURE_NO_WARNINGS=1)
