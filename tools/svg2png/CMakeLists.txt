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

# Host-only rasterizer: SVG → PNG / ICO. Built natively, or via a nested
# qt-cmake against QT_HOST_PATH when the main project targets Android.
cmake_minimum_required(VERSION 3.21)

project(autografo_svg2png LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
foreach(_cfg IN ITEMS DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${_cfg} "${CMAKE_BINARY_DIR}")
endforeach()

find_package(Qt6 REQUIRED COMPONENTS Gui Svg)

if(COMMAND qt_standard_project_setup)
    qt_standard_project_setup()
endif()

add_executable(autografo_svg2png main.cpp)
target_link_libraries(autografo_svg2png PRIVATE Qt6::Gui Qt6::Svg)
set_target_properties(autografo_svg2png PROPERTIES WIN32_EXECUTABLE FALSE)
target_compile_options(autografo_svg2png PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/Zc:__cplusplus /permissive- /utf-8>
)
