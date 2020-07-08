cmake_minimum_required(VERSION 3.5)

# Uses an extra component for common functions such as Wi-Fi and Ethernet connection.
set(EXTRA_COMPONENT_DIRS $ENV{IDF_PATH}/examples/common_components/protocol_examples_common)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(calendarclock)
if (NOT IDF_VER STREQUAL "v4.2-dev-1303-gcd4fa46f5")
    message(FATAL_ERROR "Invalid IDF version(${IDF_VER}")
endif()
