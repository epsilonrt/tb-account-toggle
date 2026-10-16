# 3.27: CPack Inno Setup generator
cmake_minimum_required(VERSION 3.27)

# Windows: pick the compiler, Ninja and Qt kit of the Qt installation found on PATH
include(cmake/QtMinGWToolchain.cmake)

project(tb-account-toggle VERSION 1.0.1 LANGUAGES CXX)

# Normalized install() destinations (e.g. "./licenses"), CMake >= 3.31
if(POLICY CMP0177)
    cmake_policy(SET CMP0177 NEW)
endif()

# Release by default with single-config generators (Ninja, Makefiles)
get_property(is_multi_config GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
if(NOT is_multi_config AND NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS Debug Release RelWithDebInfo MinSizeRel)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

# Flat install folder on Windows (exe, Qt DLLs and qt.conf side by side).
# Must be set before find_package(Qt6), which records it for the deploy script.
if(WIN32)
    set(CMAKE_INSTALL_BINDIR "." CACHE PATH "Executables and DLLs, relative to the install prefix")
endif()
include(GNUInstallDirs)

find_package(Qt6 6.7 REQUIRED COMPONENTS Widgets Test LinguistTools)

# --------------------------------------------------------------------------
# Targets
# --------------------------------------------------------------------------

# Core logic (prefs.js editing, Thunderbird process), shared by the app and the tests
add_library(tbcore STATIC
    src/prefs.cpp            src/prefs.h
    src/accountstate.cpp     src/accountstate.h
    src/thunderbird.cpp      src/thunderbird.h
    src/tberror.h
)
if(WIN32)
    target_sources(tbcore PRIVATE src/thunderbird_win.cpp)
else()
    target_sources(tbcore PRIVATE src/thunderbird_unix.cpp)
endif()
target_include_directories(tbcore PUBLIC src)
target_link_libraries(tbcore PUBLIC Qt6::Core)

add_executable(tb-account-toggle WIN32 MACOSX_BUNDLE
    src/main.cpp
    src/mainwindow.cpp       src/mainwindow.h
    src/toggleswitch.cpp     src/toggleswitch.h
    resources/resources.qrc
)
target_link_libraries(tb-account-toggle PRIVATE tbcore Qt6::Widgets)
target_compile_definitions(tb-account-toggle PRIVATE APP_VERSION="${PROJECT_VERSION}")

if(WIN32)
    target_sources(tb-account-toggle PRIVATE resources/app.rc)
elseif(APPLE)
    set(icns "${CMAKE_CURRENT_SOURCE_DIR}/resources/app.icns")
    target_sources(tb-account-toggle PRIVATE "${icns}")
    set_source_files_properties("${icns}" PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
    set_target_properties(tb-account-toggle PROPERTIES
        MACOSX_BUNDLE_BUNDLE_NAME "Thunderbird Accounts"
        MACOSX_BUNDLE_GUI_IDENTIFIER "fr.epsilonrt.tb-account-toggle"
        MACOSX_BUNDLE_ICON_FILE app.icns
        MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}")
endif()

# Translations: English in the sources, French catalog edited with Qt Linguist.
# The "update_translations" target refreshes the .ts files from the sources.
qt_add_translations(TARGETS tb-account-toggle
    SOURCE_TARGETS tb-account-toggle tbcore
    TS_FILES translations/tb-account-toggle_fr.ts
    RESOURCE_PREFIX /i18n
    LUPDATE_OPTIONS -no-obsolete)
# Qt's own French strings (standard dialog buttons...), embedded as well
set(qt_translations_dir "${QT6_INSTALL_PREFIX}/${QT6_INSTALL_TRANSLATIONS}")
if(EXISTS "${qt_translations_dir}/qtbase_fr.qm")
    qt_add_resources(tb-account-toggle qt_translations
        PREFIX /i18n
        BASE "${qt_translations_dir}"
        FILES "${qt_translations_dir}/qtbase_fr.qm")
else()
    message(WARNING "qtbase_fr.qm not found in ${qt_translations_dir}: Qt's standard buttons will stay in English")
endif()

enable_testing()
add_executable(tst_accountstate tests/tst_accountstate.cpp)
target_link_libraries(tst_accountstate PRIVATE tbcore Qt6::Test)
add_test(NAME tst_accountstate COMMAND tst_accountstate)
if(WIN32)
    # Run the tests against the Qt and compiler runtime used for the build,
    # whatever is on the user's PATH
    get_filename_component(compiler_bin_dir "${CMAKE_CXX_COMPILER}" DIRECTORY)
    set_tests_properties(tst_accountstate PROPERTIES ENVIRONMENT_MODIFICATION
        "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>;PATH=path_list_prepend:${compiler_bin_dir}")
endif()

# --------------------------------------------------------------------------
# Install
# --------------------------------------------------------------------------
install(TARGETS tb-account-toggle
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    BUNDLE DESTINATION .)

if(WIN32 OR APPLE)
    # Self-contained app: bundle the Qt runtime (windeployqt / macdeployqt)
    if(WIN32)
        # Only what the app uses: platform, windows11 style, and the ICO reader
        set(deploy_options
            --no-system-d3d-compiler --no-opengl-sw
            --skip-plugin-types generic,iconengines,networkinformation,tls
            --exclude-plugins qgif,qjpeg,qsvg)
    endif()
    qt_generate_deploy_app_script(
        TARGET tb-account-toggle
        OUTPUT_SCRIPT deploy_script
        NO_TRANSLATIONS
        NO_COMPILER_RUNTIME
        DEPLOY_TOOL_OPTIONS ${deploy_options})
    install(SCRIPT ${deploy_script})
else()
    # Linux: system Qt, desktop entry and icon
    install(FILES resources/tb-account-toggle.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
    install(FILES resources/app.png DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/256x256/apps
        RENAME tb-account-toggle.png)
endif()

# MinGW runtime, taken from the toolchain that built Qt and the app
if(MINGW)
    get_filename_component(MINGW_BIN_DIR "${CMAKE_CXX_COMPILER}" DIRECTORY)
    install(FILES
        "${MINGW_BIN_DIR}/libgcc_s_seh-1.dll"
        "${MINGW_BIN_DIR}/libstdc++-6.dll"
        "${MINGW_BIN_DIR}/libwinpthread-1.dll"
        DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()

# --------------------------------------------------------------------------
# Licenses: this project's (BSD 3-Clause), and those of the bundled third-party code
# --------------------------------------------------------------------------
if(WIN32 OR APPLE)
    # Qt (LGPL-3.0) and, on Windows, the MinGW runtime are shipped with the app
    set(NOTICE_MINGW "")
    set(mingw_licenses "")
    if(MINGW)
        # License texts shipped with the MinGW toolchain that built the app
        get_filename_component(mingw_licenses_dir "${MINGW_BIN_DIR}/../licenses" ABSOLUTE)
        set(winpthreads_copying "${mingw_licenses_dir}/winpthreads/COPYING")
        set(gcc_runtime_exception "${mingw_licenses_dir}/gcc/COPYING.RUNTIME")
        if(EXISTS "${winpthreads_copying}" AND EXISTS "${gcc_runtime_exception}")
            set(mingw_licenses "${winpthreads_copying}" "${gcc_runtime_exception}")
        else()
            message(WARNING "MinGW license texts not found in ${mingw_licenses_dir}")
        endif()
        string(CONCAT NOTICE_MINGW
            "\n\nMinGW-w64 runtime\n-----------------\n"
            "libgcc_s_seh-1.dll and libstdc++-6.dll are the GCC runtime libraries,\n"
            "Copyright (C) Free Software Foundation, Inc., https://gcc.gnu.org. They are\n"
            "under the GPL-3.0 with the GCC Runtime Library Exception 3.1, which allows\n"
            "distributing them with programs under any license: see licenses/GPL-3.0.txt\n"
            "and licenses/GCC-RUNTIME-EXCEPTION.txt.\n\n"
            "libwinpthread-1.dll is the winpthreads library of the MinGW-w64 project,\n"
            "https://www.mingw-w64.org, under the MIT license: see\n"
            "licenses/winpthreads-COPYING.txt.\n")
    endif()
    configure_file(licenses/THIRD-PARTY-NOTICES.txt.in THIRD-PARTY-NOTICES.txt @ONLY)

    if(WIN32)
        set(license_dir ${CMAKE_INSTALL_BINDIR})
    else()
        set(license_dir .)  # disk image root, next to the application bundle
    endif()
    install(FILES LICENSE DESTINATION ${license_dir} RENAME LICENSE.txt)
    install(FILES "${CMAKE_CURRENT_BINARY_DIR}/THIRD-PARTY-NOTICES.txt" DESTINATION ${license_dir})
    install(FILES licenses/LGPL-3.0.txt licenses/GPL-3.0.txt DESTINATION ${license_dir}/licenses)
    if(mingw_licenses)
        list(GET mingw_licenses 0 winpthreads_copying)
        list(GET mingw_licenses 1 gcc_runtime_exception)
        install(FILES "${winpthreads_copying}" DESTINATION ${license_dir}/licenses RENAME winpthreads-COPYING.txt)
        install(FILES "${gcc_runtime_exception}" DESTINATION ${license_dir}/licenses RENAME GCC-RUNTIME-EXCEPTION.txt)
    endif()
else()
    # Linux: Qt comes from the distribution; Debian location for the package's license
    install(FILES LICENSE DESTINATION ${CMAKE_INSTALL_DATADIR}/doc/tb-account-toggle RENAME copyright)
endif()

# --------------------------------------------------------------------------
# Package: Inno Setup (Windows), disk image (macOS), Debian package (Linux)
# --------------------------------------------------------------------------
set(CPACK_PACKAGE_NAME "Thunderbird Accounts")
set(CPACK_PACKAGE_VENDOR "Pascal JEAN")
set(CPACK_PACKAGE_CONTACT "Pascal JEAN <epsilonrt@gmail.com>")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Temporarily disables or enables Thunderbird accounts")
set(CPACK_PACKAGE_HOMEPAGE_URL "https://github.com/epsilonrt/tb-account-toggle")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "tb-account-toggle")
set(CPACK_VERBATIM_VARIABLES ON)

if(WIN32)
    set(CPACK_GENERATOR INNOSETUP)
    set(CPACK_PACKAGE_FILE_NAME "tb-account-toggle-setup-${PROJECT_VERSION}")
    set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
    # Shortcut name localized by the installer (resources/installer-messages.iss)
    set(CPACK_PACKAGE_EXECUTABLES "tb-account-toggle;{cm:AppTitle}")
    set(CPACK_CREATE_DESKTOP_LINKS tb-account-toggle)
    set(CPACK_INNOSETUP_USE_MODERN_WIZARD ON)
    set(CPACK_INNOSETUP_LANGUAGES english french)
    set(CPACK_INNOSETUP_EXTRA_SCRIPTS "${CMAKE_CURRENT_SOURCE_DIR}/resources/installer-messages.iss")
    set(CPACK_INNOSETUP_ICON_FILE "${CMAKE_CURRENT_SOURCE_DIR}/resources/app.ico")
    set(CPACK_INNOSETUP_INSTALL_ROOT "{localappdata}\\Programs")
    set(CPACK_INNOSETUP_PROGRAM_MENU_FOLDER ".")
    set(CPACK_INNOSETUP_SETUP_PrivilegesRequired lowest)
    set(CPACK_INNOSETUP_SETUP_AppId "{{6B0C3E2A-8F4D-4C1B-9A57-3E2D1F0B7C64}")
    set(CPACK_INNOSETUP_SETUP_UninstallDisplayIcon "{app}\\tb-account-toggle.exe")
    set(CPACK_INNOSETUP_SETUP_UninstallDisplayName "{cm:AppTitle}")
elseif(APPLE)
    set(CPACK_GENERATOR DragNDrop)
    set(CPACK_PACKAGE_FILE_NAME "tb-account-toggle-${PROJECT_VERSION}-macos")
else()
    set(CPACK_GENERATOR DEB)
    set(CPACK_DEBIAN_PACKAGE_NAME tb-account-toggle)
    set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
    set(CPACK_DEBIAN_PACKAGE_SECTION mail)
    set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
    # Qt platform plugins are loaded at run time, so dpkg-shlibdeps cannot see them
    set(CPACK_DEBIAN_PACKAGE_DEPENDS "qt6-qpa-plugins")
    set(CPACK_DEBIAN_PACKAGE_RECOMMENDS "qt6-wayland")
    set(CPACK_DEBIAN_PACKAGE_SUGGESTS "thunderbird")
endif()
include(CPack)

# Same as the built-in "package" target, but listed by IDEs (VS Code CMake Tools...)
add_custom_target(installer
    COMMAND "${CMAKE_CPACK_COMMAND}" --config "${CMAKE_BINARY_DIR}/CPackConfig.cmake"
    WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
    COMMENT "Building the package with CPack"
    USES_TERMINAL)
add_dependencies(installer tb-account-toggle)
