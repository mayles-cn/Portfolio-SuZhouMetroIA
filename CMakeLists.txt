cmake_minimum_required(VERSION 3.21)

project(SuZhouMetroIA VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# Keep codemodel cleaner for IDE integrations (e.g. CMake Tools launch target).
set(QT_NO_CREATE_VERSIONLESS_TARGETS ON)

# This project does not use Vulkan directly. Disable the optional Qt wrapper
# probe to avoid noisy "Could NOT find WrapVulkanHeaders" configure warnings.
set(CMAKE_DISABLE_FIND_PACKAGE_WrapVulkanHeaders ON)

if(CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_CONFIGURATION_TYPES "Release" CACHE STRING "Build types" FORCE)
else()
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Build type" FORCE)
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

find_package(Qt6 REQUIRED COMPONENTS Widgets)

add_subdirectory(src)
