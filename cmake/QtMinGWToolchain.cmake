# Match the toolchain to a Qt MinGW kit installed with the Qt online installer, so that
#   cmake -S . -B build -G Ninja
# works without any path: <QT_ROOT>/<version>/mingw_64 is used for Qt, and the MinGW
# and Ninja shipped in <QT_ROOT>/Tools for the compiler and the build tool.
#
# QT_ROOT is detected from the qtpaths/qmake found on PATH, then C:/Qt; it can also be
# set on the command line or in cmake-gui. Nothing is changed when the compiler or Qt
# are given explicitly (CMAKE_CXX_COMPILER, CXX, Qt6_DIR, CMAKE_PREFIX_PATH).
#
# Must be included before project().

if(NOT CMAKE_HOST_WIN32)
    return()
endif()

# Visual Studio generators always use MSVC, which cannot link with a Qt MinGW kit
if(CMAKE_GENERATOR MATCHES "^Visual Studio")
    message(FATAL_ERROR
        "The '${CMAKE_GENERATOR}' generator (MSVC) is not supported.\n"
        "Use Ninja: cmake -S . -B <build dir> -G Ninja\n"
        "(VS Code CMake Tools: set \"cmake.generator\" to \"Ninja\" and delete the build folder.)")
endif()

if(DEFINED Qt6_DIR OR CMAKE_PREFIX_PATH)
    return()
endif()

# --- Locate the Qt installation: <root>/<version>/<kit>/bin/qtpaths.exe
if(NOT QT_ROOT OR NOT QT_KIT_VERSION)
    find_program(_qt_tool NAMES qtpaths6 qtpaths qmake6 qmake NO_CACHE)
    if(_qt_tool)
        get_filename_component(_kit_dir "${_qt_tool}/../.." ABSOLUTE)
        get_filename_component(_version_dir "${_kit_dir}" DIRECTORY)
        get_filename_component(_root "${_version_dir}" DIRECTORY)
        if(EXISTS "${_root}/Tools")
            get_filename_component(_version "${_version_dir}" NAME)
        else()
            unset(_root)
        endif()
    endif()
    if(NOT _root AND EXISTS "C:/Qt/Tools")
        set(_root "C:/Qt")
        file(GLOB _versions LIST_DIRECTORIES true RELATIVE "${_root}" "${_root}/6.*")
        list(SORT _versions COMPARE NATURAL ORDER DESCENDING)
        list(GET _versions 0 _version)
    endif()
    if(_root AND _version)
        set(QT_ROOT "${_root}" CACHE PATH "Qt installation root (contains Tools/ and <version>/)")
        set(QT_KIT_VERSION "${_version}" CACHE STRING "Qt version to use under QT_ROOT")
    endif()
endif()

if(NOT QT_ROOT OR NOT EXISTS "${QT_ROOT}/${QT_KIT_VERSION}/mingw_64")
    return()  # no installer layout: leave it to the standard CMake search
endif()

set(_qt_kit "${QT_ROOT}/${QT_KIT_VERSION}/mingw_64")

# --- Compiler: the MinGW shipped with Qt (newest one if several)
if(NOT DEFINED CMAKE_CXX_COMPILER AND NOT DEFINED ENV{CXX})
    file(GLOB _gxx "${QT_ROOT}/Tools/mingw*_64/bin/g++.exe")
    if(_gxx)
        list(SORT _gxx COMPARE NATURAL ORDER DESCENDING)
        list(GET _gxx 0 _gxx)
        set(CMAKE_CXX_COMPILER "${_gxx}" CACHE FILEPATH "C++ compiler")
        # Resource compiler from the same toolchain, not whatever windres is on PATH
        get_filename_component(_mingw_bin "${_gxx}" DIRECTORY)
        if(NOT DEFINED CMAKE_RC_COMPILER AND EXISTS "${_mingw_bin}/windres.exe")
            set(CMAKE_RC_COMPILER "${_mingw_bin}/windres.exe" CACHE FILEPATH "RC compiler")
        endif()
    endif()
endif()

# --- Build tool: Ninja from PATH, else the one shipped with Qt
if(CMAKE_GENERATOR MATCHES "Ninja" AND NOT CMAKE_MAKE_PROGRAM)
    find_program(CMAKE_MAKE_PROGRAM NAMES ninja HINTS "${QT_ROOT}/Tools/Ninja")
endif()

# --- Qt itself
list(PREPEND CMAKE_PREFIX_PATH "${_qt_kit}")
message(STATUS "Qt kit: ${_qt_kit}")
