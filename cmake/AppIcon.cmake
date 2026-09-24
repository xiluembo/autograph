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

# Rasterizes assets/icons/autografo.svg into platform icons at build time.
# Desktop: window icon from the SVG resource; Windows also gets a multi-size .ico.
# Android: mipmap launcher icons + adaptive foreground/background.

set(AUTOGRAFO_ICON_BACKGROUND "#1E1E1E")
set(AUTOGRAFO_ICON_FIT 1.0)
set(AUTOGRAFO_WINDOWS_ICO_SIZES "16,24,32,48,64,128,256")

function(_autografo_append_spec spec_var)
    set(_text "${${spec_var}}")
    foreach(_line IN LISTS ARGN)
        string(APPEND _text "${_line}\n")
    endforeach()
    set(${spec_var} "${_text}" PARENT_SCOPE)
endfunction()

function(_autografo_write_android_adaptive_xml output_dir)
    set(_anydpi "${output_dir}/mipmap-anydpi-v26")
    set(_values "${output_dir}/values")
    file(MAKE_DIRECTORY "${_anydpi}")
    file(MAKE_DIRECTORY "${_values}")

    set(_adaptive
[=[<?xml version="1.0" encoding="utf-8"?>
<adaptive-icon xmlns:android="http://schemas.android.com/apk/res/android">
    <background android:drawable="@color/ic_launcher_background"/>
    <foreground android:drawable="@mipmap/ic_launcher_foreground"/>
    <monochrome android:drawable="@mipmap/ic_launcher_foreground"/>
</adaptive-icon>
]=])
    file(WRITE "${_anydpi}/ic_launcher.xml" "${_adaptive}")
    file(WRITE "${_anydpi}/ic_launcher_round.xml" "${_adaptive}")
    file(WRITE "${_values}/ic_launcher_colors.xml"
         "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n<resources>\n    <color name=\"ic_launcher_background\">${AUTOGRAFO_ICON_BACKGROUND}</color>\n</resources>\n")
endfunction()

function(_autografo_resolve_svg2png out_exe out_qt_bin)
    if(NOT ANDROID)
        add_executable(autografo_svg2png EXCLUDE_FROM_ALL
            "${CMAKE_CURRENT_SOURCE_DIR}/tools/svg2png/main.cpp"
        )
        target_link_libraries(autografo_svg2png PRIVATE Qt6::Gui Qt6::Svg)
        set_target_properties(autografo_svg2png PROPERTIES
            WIN32_EXECUTABLE FALSE
            RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/host-svg2png"
            RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_CURRENT_BINARY_DIR}/host-svg2png"
            RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_BINARY_DIR}/host-svg2png"
            RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_CURRENT_BINARY_DIR}/host-svg2png"
            RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_CURRENT_BINARY_DIR}/host-svg2png"
        )
        set(${out_exe} "$<TARGET_FILE:autografo_svg2png>" PARENT_SCOPE)
        set(${out_qt_bin} "$<TARGET_FILE_DIR:Qt6::Core>" PARENT_SCOPE)
        return()
    endif()

    if(NOT QT_HOST_PATH)
        message(FATAL_ERROR
            "Android icon generation needs QT_HOST_PATH so the host svg2png tool "
            "can be built with desktop Qt.")
    endif()

    set(_host_build "${CMAKE_CURRENT_BINARY_DIR}/host-svg2png")
    if(CMAKE_HOST_WIN32)
        set(_host_exe "${_host_build}/autografo_svg2png.exe")
    else()
        set(_host_exe "${_host_build}/autografo_svg2png")
    endif()

    add_custom_command(
        OUTPUT "${_host_exe}"
        COMMAND "${CMAKE_COMMAND}"
                "-DQT_HOST_PATH=${QT_HOST_PATH}"
                "-DSOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}/tools/svg2png"
                "-DBUILD_DIR=${_host_build}"
                "-DCMAKE_COMMAND_PATH=${CMAKE_COMMAND}"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/BuildHostSvg2Png.cmake"
        DEPENDS
            "${CMAKE_CURRENT_SOURCE_DIR}/tools/svg2png/main.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/tools/svg2png/CMakeLists.txt"
            "${CMAKE_CURRENT_SOURCE_DIR}/cmake/BuildHostSvg2Png.cmake"
        COMMENT "Building host autografo_svg2png"
        VERBATIM
    )
    add_custom_target(autografo_svg2png DEPENDS "${_host_exe}")

    set(${out_exe} "${_host_exe}" PARENT_SCOPE)
    set(${out_qt_bin} "${QT_HOST_PATH}/bin" PARENT_SCOPE)
endfunction()

# autografo_setup_app_icon(<target> SVG <path>)
function(autografo_setup_app_icon target)
    cmake_parse_arguments(ARG "" "SVG" "" ${ARGN})
    if(NOT ARG_SVG)
        message(FATAL_ERROR "autografo_setup_app_icon(${target}) requires SVG <path>")
    endif()
    if(NOT EXISTS "${ARG_SVG}")
        message(FATAL_ERROR "App icon SVG not found: ${ARG_SVG}")
    endif()

    qt_add_resources(${target} app_icon
        PREFIX "/icons"
        BASE "${CMAKE_CURRENT_SOURCE_DIR}/assets/icons"
        FILES "${ARG_SVG}"
    )

    _autografo_resolve_svg2png(_svg2png_exe _qt_bin)

    set(_icon_dir "${CMAKE_CURRENT_BINARY_DIR}/icons")
    set(_spec "${_icon_dir}/icon-spec.txt")
    set(_stamp "${_icon_dir}/autografo_icon_stamp.cpp")
    file(MAKE_DIRECTORY "${_icon_dir}")

    set(_spec_text "svg|${ARG_SVG}\n")
    set(_outputs)

    if(WIN32)
        set(_ico "${_icon_dir}/autografo.ico")
        set(_rc "${_icon_dir}/autografo.rc")
        cmake_path(CONVERT "${_ico}" TO_CMAKE_PATH_LIST _ico_rc_path)
        file(WRITE "${_rc}" "IDI_ICON1 ICON \"${_ico_rc_path}\"\n")
        _autografo_append_spec(_spec_text
            "ico|${AUTOGRAFO_WINDOWS_ICO_SIZES}|${AUTOGRAFO_ICON_BACKGROUND}|${AUTOGRAFO_ICON_FIT}|${_ico}"
        )
        list(APPEND _outputs "${_ico}")
        target_sources(${target} PRIVATE "${_rc}")
        set_source_files_properties("${_rc}" PROPERTIES OBJECT_DEPENDS "${_ico}")
    endif()

    if(ANDROID)
        set(_android_res "${CMAKE_CURRENT_BINARY_DIR}/generated-android-res")
        _autografo_write_android_adaptive_xml("${_android_res}")

        set(_launcher_sizes mdpi:48 hdpi:72 xhdpi:96 xxhdpi:144 xxxhdpi:192)
        foreach(_entry IN LISTS _launcher_sizes)
            string(REPLACE ":" ";" _parts "${_entry}")
            list(GET _parts 0 _density)
            list(GET _parts 1 _size)
            set(_dir "${_android_res}/mipmap-${_density}")
            file(MAKE_DIRECTORY "${_dir}")
            set(_launcher "${_dir}/ic_launcher.png")
            set(_round "${_dir}/ic_launcher_round.png")
            _autografo_append_spec(_spec_text
                "png|${_size}|${_size}|${AUTOGRAFO_ICON_BACKGROUND}|${AUTOGRAFO_ICON_FIT}|${_launcher}"
                "png|${_size}|${_size}|${AUTOGRAFO_ICON_BACKGROUND}|${AUTOGRAFO_ICON_FIT}|${_round}"
            )
            list(APPEND _outputs "${_launcher}" "${_round}")
        endforeach()

        # 108 dp adaptive foreground at each density (mdpi = 108 px).
        set(_foreground_sizes mdpi:108 hdpi:162 xhdpi:216 xxhdpi:324 xxxhdpi:432)
        foreach(_entry IN LISTS _foreground_sizes)
            string(REPLACE ":" ";" _parts "${_entry}")
            list(GET _parts 0 _density)
            list(GET _parts 1 _size)
            set(_foreground "${_android_res}/mipmap-${_density}/ic_launcher_foreground.png")
            _autografo_append_spec(_spec_text
                "png|${_size}|${_size}|none|${AUTOGRAFO_ICON_FIT}|${_foreground}"
            )
            list(APPEND _outputs "${_foreground}")
        endforeach()

        set_target_properties(${target} PROPERTIES
            QT_ANDROID_APP_ICON "@mipmap/ic_launcher"
        )
        set(AUTOGRAFO_GENERATED_ANDROID_ICON_RES "${_android_res}" PARENT_SCOPE)
    endif()

    file(WRITE "${_spec}" "${_spec_text}")
    if(NOT EXISTS "${_stamp}")
        file(WRITE "${_stamp}" "// App icon generation stamp\n")
    endif()

    if(_outputs)
        add_custom_command(
            OUTPUT ${_outputs} "${_stamp}"
            COMMAND "${CMAKE_COMMAND}"
                    "-DSVG2PNG=${_svg2png_exe}"
                    "-DSPEC=${_spec}"
                    "-DQT_BIN=${_qt_bin}"
                    -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/RunSvg2Png.cmake"
            COMMAND "${CMAKE_COMMAND}" -E touch "${_stamp}"
            DEPENDS
                autografo_svg2png
                "${ARG_SVG}"
                "${_spec}"
                "${CMAKE_CURRENT_SOURCE_DIR}/cmake/RunSvg2Png.cmake"
                "${CMAKE_CURRENT_SOURCE_DIR}/tools/svg2png/main.cpp"
            COMMENT "Rasterizing app icons from SVG"
            VERBATIM
        )
        add_custom_target(autografo_app_icons DEPENDS ${_outputs} "${_stamp}")
        target_sources(${target} PRIVATE "${_stamp}")
        set_source_files_properties("${_stamp}" PROPERTIES GENERATED TRUE)
    endif()
endfunction()
