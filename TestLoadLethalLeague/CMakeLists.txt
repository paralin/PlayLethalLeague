cmake_minimum_required(VERSION 2.8)

project(LethalLeagueTestLoad)

# Create a shared library with this project
add_executable(LethalLeagueTestLoad TestLoadLethalLeague.cpp ${LethalLeagueDLL_SRC} ${ASMJIT_SRC})

# Add include directories
include_directories(../PlayLethalLeague)

# Link libraries
target_link_libraries(LethalLeagueTestLoad shlwapi.lib) # Required for Python
target_link_libraries(LethalLeagueTestLoad ${PYTHON_LIBRARY_DIR}/python34.lib)
target_link_libraries(LethalLeagueTestLoad debug ${BOOST_LIBRARY_DIR}/boost_python-vc140-mt-gd-1_60.lib)
target_link_libraries(LethalLeagueTestLoad optimized ${BOOST_LIBRARY_DIR}/boost_python-vc140-mt-1_60.lib)
target_link_libraries(LethalLeagueTestLoad ${ASMJIT_DEPS})
target_link_libraries(LethalLeagueTestLoad LethalLeagueDLL)

# Add preprocessor definitions
add_definitions(-DNOMINMAX -D_CRT_SECURE_NO_WARNINGS=1)
