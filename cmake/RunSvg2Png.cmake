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

# Invoked as: cmake -P RunSvg2Png.cmake
# Required -D variables: SVG2PNG, SPEC, QT_BIN

if(NOT SVG2PNG OR NOT SPEC OR NOT QT_BIN)
    message(FATAL_ERROR "RunSvg2Png.cmake requires SVG2PNG, SPEC and QT_BIN")
endif()

cmake_path(SET _qt_plugins NORMALIZE "${QT_BIN}/../plugins")

if(CMAKE_HOST_WIN32)
    set(ENV{PATH} "${QT_BIN};$ENV{PATH}")
else()
    set(ENV{PATH} "${QT_BIN}:$ENV{PATH}")
endif()
set(ENV{QT_PLUGIN_PATH} "${_qt_plugins}")
if(NOT DEFINED ENV{QT_QPA_PLATFORM})
    set(ENV{QT_QPA_PLATFORM} "offscreen")
endif()

execute_process(
    COMMAND "${SVG2PNG}" --spec "${SPEC}"
    RESULT_VARIABLE _svg2png_result
)
if(NOT _svg2png_result EQUAL 0)
    message(FATAL_ERROR "autografo_svg2png failed with exit code ${_svg2png_result}")
endif()
