cmake_minimum_required(VERSION 2.8)

project(LethalLeagueAI)

add_subdirectory(PlayLethalLeague)
INCLUDE_EXTERNAL_MSPROJECT(LethalLeagueScripts scripts/scripts.pyproj)
add_subdirectory(TestLoadLethalLeague)
