#
# Autograph
# Copyright (C) 2026 Andrius da Costa Ribas <andriusmao@gmail.com>
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
# License as published by the Free Software Foundation; either
# version 2.1 of the License, or (at your option) any later version.
#
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with this library; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
#

# Invoked as: cmake -P BuildHostSvg2Png.cmake
# Required -D variables: QT_HOST_PATH, SOURCE_DIR, BUILD_DIR
# Optional: CMAKE_COMMAND_PATH (defaults to the cmake running this script)

if(NOT QT_HOST_PATH OR NOT SOURCE_DIR OR NOT BUILD_DIR)
    message(FATAL_ERROR "BuildHostSvg2Png.cmake requires QT_HOST_PATH, SOURCE_DIR and BUILD_DIR")
endif()

if(NOT CMAKE_COMMAND_PATH)
    set(CMAKE_COMMAND_PATH "${CMAKE_COMMAND}")
endif()

cmake_path(SET QT_HOST_PATH NORMALIZE "${QT_HOST_PATH}")
set(_toolchain "${QT_HOST_PATH}/lib/cmake/Qt6/qt.toolchain.cmake")
set(_qt6_dir "${QT_HOST_PATH}/lib/cmake/Qt6")
if(NOT EXISTS "${_toolchain}")
    message(FATAL_ERROR "Host Qt toolchain not found: ${_toolchain}")
endif()

# The Android CMake process exports Qt toolchain env vars. If they leak into
# this nested configure, find_package(Qt6) can pick MinGW headers while MSVC
# compiles, or chainload the NDK toolchain.
foreach(_var
    CMAKE_TOOLCHAIN_FILE
    CMAKE_PREFIX_PATH
    CMAKE_FIND_ROOT_PATH
    CMAKE_MODULE_PATH
    CMAKE_GENERATOR
    CMAKE_GENERATOR_INSTANCE
    CMAKE_GENERATOR_PLATFORM
    CMAKE_GENERATOR_TOOLSET
    CMAKE_C_COMPILER
    CMAKE_CXX_COMPILER
    CMAKE_SYSROOT
    QT_CHAINLOAD_TOOLCHAIN_FILE
    QT_HOST_PATH
    Qt6_DIR
    Qt6Core_DIR
    Qt6Gui_DIR
    Qt6Svg_DIR
    ANDROID_NDK
    ANDROID_NDK_ROOT
    ANDROID_ABI
    ANDROID_PLATFORM
    ANDROID_STL
    ANDROID_SDK_ROOT
    _QT_TOOLCHAIN_VARS_INITIALIZED
    _QT_TOOLCHAIN_QT_CHAINLOAD_TOOLCHAIN_FILE
    _QT_TOOLCHAIN_QT_TOOLCHAIN_INCLUDE_FILE
    _QT_TOOLCHAIN_QT_TOOLCHAIN_RELOCATABLE_CMAKE_DIR
    _QT_TOOLCHAIN_QT_TOOLCHAIN_RELOCATABLE_PREFIX
    _QT_TOOLCHAIN_QT_ADDITIONAL_PACKAGES_PREFIX_PATH
)
    unset(ENV{${_var}})
endforeach()

if(CMAKE_HOST_WIN32)
    set(ENV{PATH} "${QT_HOST_PATH}/bin;$ENV{PATH}")
else()
    set(ENV{PATH} "${QT_HOST_PATH}/bin:$ENV{PATH}")
endif()

file(MAKE_DIRECTORY "${BUILD_DIR}")
file(REMOVE "${BUILD_DIR}/CMakeCache.txt")
file(REMOVE_RECURSE "${BUILD_DIR}/CMakeFiles")

set(_configure_args
    -S "${SOURCE_DIR}"
    -B "${BUILD_DIR}"
    -DCMAKE_BUILD_TYPE=Release
    "-DCMAKE_PREFIX_PATH=${QT_HOST_PATH}"
    "-DCMAKE_TOOLCHAIN_FILE=${_toolchain}"
    "-DQt6_DIR=${_qt6_dir}"
    "-DQT_CHAINLOAD_TOOLCHAIN_FILE:FILEPATH="
    "-DQT_HOST_PATH:PATH="
)

if(QT_HOST_PATH MATCHES "[Mm]ingw")
    find_program(_ninja NAMES ninja ninja.exe)
    find_program(_gxx NAMES g++.exe g++)
    if(NOT _ninja)
        message(FATAL_ERROR "Ninja is required to build host svg2png against MinGW Qt")
    endif()
    if(NOT _gxx)
        message(FATAL_ERROR "g++ is required to build host svg2png against MinGW Qt")
    endif()
    list(APPEND _configure_args
        -G Ninja
        "-DCMAKE_CXX_COMPILER=${_gxx}"
    )
endif()

execute_process(
    COMMAND "${CMAKE_COMMAND_PATH}" ${_configure_args}
    RESULT_VARIABLE _configure_result
)
if(NOT _configure_result EQUAL 0)
    message(FATAL_ERROR "Host svg2png configure failed with exit code ${_configure_result}")
endif()

execute_process(
    COMMAND "${CMAKE_COMMAND_PATH}" --build "${BUILD_DIR}" --config Release
    RESULT_VARIABLE _build_result
)
if(NOT _build_result EQUAL 0)
    message(FATAL_ERROR "Host svg2png build failed with exit code ${_build_result}")
endif()
