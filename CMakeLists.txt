cmake_minimum_required(VERSION 3.20)
project(input-bridge VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Standalone Asio
include(FetchContent)
FetchContent_Declare(
    asio
    URL https://github.com/chriskohlhoff/asio/archive/refs/tags/asio-1-30-2.tar.gz
)
FetchContent_MakeAvailable(asio)
add_library(asio_interface INTERFACE)
target_include_directories(asio_interface INTERFACE ${asio_SOURCE_DIR}/asio/include)
target_compile_definitions(asio_interface INTERFACE ASIO_STANDALONE)

# Main executable
add_executable(input-bridge
    src/main.cpp
    src/network/client.cpp
    src/network/server.cpp
)

target_link_libraries(input-bridge PRIVATE asio_interface)

# Platform specific libraries
if(APPLE)
    target_sources(input-bridge PRIVATE src/input/macos_inject.cpp)
    find_library(CORE_GRAPHICS CoreGraphics)
    find_library(APPLICATION_SERVICES ApplicationServices)
    target_link_libraries(input-bridge PRIVATE ${CORE_GRAPHICS} ${APPLICATION_SERVICES})
elseif(UNIX AND NOT APPLE)
    target_sources(input-bridge PRIVATE src/input/linux_capture.cpp src/input/config.cpp)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(LIBEVDEV REQUIRED libevdev)
    pkg_check_modules(LIBUDEV REQUIRED libudev)
    target_include_directories(input-bridge PRIVATE ${LIBEVDEV_INCLUDE_DIRS} ${LIBUDEV_INCLUDE_DIRS})
    target_link_libraries(input-bridge PRIVATE ${LIBEVDEV_LIBRARIES} ${LIBUDEV_LIBRARIES})
endif()

include(GNUInstallDirs)
install(TARGETS input-bridge
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
