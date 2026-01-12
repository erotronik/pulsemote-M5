cmake_minimum_required(VERSION 3.16.0)

# Add component search directories (supports both lib/ and components/)
set(EXTRA_COMPONENT_DIRS components lib)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(pulsemote-M5)