# This file was automatically generated for projects
# without default 'CMakeLists.txt' file.

FILE(GLOB_RECURSE app_sources ${CMAKE_SOURCE_DIR}/src/*.*) # bundles all paths in /src/ folder into app_sources variable

idf_component_register(
    SRCS ${app_sources}) # registers all paths in app_sources as main component for esp-idf
    


