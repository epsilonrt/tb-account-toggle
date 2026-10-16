# 3.27: CPack Inno Setup generator
cmake_minimum_required(VERSION 3.27)

# Windows: pick the compiler, Ninja and Qt kit of the Qt installation found on PATH
include(cmake/QtMinGWToolchain.cmake)

project(tb-account-toggle VERSION 1.0.0 LANGUAGES CXX)

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

# Core logic (prefs.js editing, Thunderbird process), shared by the app and the tests
add_library(tbcore STATIC
    src/prefs.cpp            src/prefs.h
    src/accountstate.cpp     src/accountstate.h
    src/thunderbird.cpp      src/thunderbird.h
    src/tberror.h
)
target_include_directories(tbcore PUBLIC src)
target_link_libraries(tbcore PUBLIC Qt6::Core)

add_executable(tb-account-toggle WIN32
    src/main.cpp
    src/mainwindow.cpp       src/mainwindow.h
    src/toggleswitch.cpp     src/toggleswitch.h
    resources/resources.qrc
    resources/app.rc
)
target_link_libraries(tb-account-toggle PRIVATE tbcore Qt6::Widgets)
target_compile_definitions(tb-account-toggle PRIVATE APP_VERSION="${PROJECT_VERSION}")

# Translations: English in the sources, French catalog edited with Qt Linguist.
# The "update_translations" target refreshes the .ts files from the sources.
qt_add_translations(TARGETS tb-account-toggle
    SOURCE_TARGETS tb-account-toggle tbcore
    TS_FILES translations/tb-account-toggle_fr.ts
    RESOURCE_PREFIX /i18n)
# Qt's own French strings (standard dialog buttons...), embedded as well
set(qt_translations_dir "${QT6_INSTALL_PREFIX}/${QT6_INSTALL_TRANSLATIONS}")
qt_add_resources(tb-account-toggle qt_translations
    PREFIX /i18n
    BASE "${qt_translations_dir}"
    FILES "${qt_translations_dir}/qtbase_fr.qm")

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
# Install: the app with the Qt runtime it needs
# --------------------------------------------------------------------------
install(TARGETS tb-account-toggle RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

# Only what the app uses: platform, windows11 style, and the ICO reader for the window icon
qt_generate_deploy_app_script(
    TARGET tb-account-toggle
    OUTPUT_SCRIPT deploy_script
    NO_TRANSLATIONS
    NO_COMPILER_RUNTIME
    DEPLOY_TOOL_OPTIONS
        --no-system-d3d-compiler --no-opengl-sw
        --skip-plugin-types generic,iconengines,networkinformation,tls
        --exclude-plugins qgif,qjpeg,qsvg
)
install(SCRIPT ${deploy_script})

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
# Package: Inno Setup installer (per-user, no administrator rights)
# --------------------------------------------------------------------------
set(CPACK_PACKAGE_NAME "Thunderbird Accounts")
set(CPACK_PACKAGE_VENDOR "Pascal JEAN")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Temporarily disables or enables Thunderbird accounts")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "tb-account-toggle")
set(CPACK_PACKAGE_FILE_NAME "tb-account-toggle-setup-${PROJECT_VERSION}")
# Shortcut name localized by the installer (resources/installer-messages.iss)
set(CPACK_PACKAGE_EXECUTABLES "tb-account-toggle;{cm:AppTitle}")
set(CPACK_CREATE_DESKTOP_LINKS tb-account-toggle)
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")

if(WIN32)
    set(CPACK_GENERATOR INNOSETUP)
else()
    set(CPACK_GENERATOR TGZ)
endif()
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
set(CPACK_VERBATIM_VARIABLES ON)
include(CPack)

# Same as the built-in "package" target, but listed by IDEs (VS Code CMake Tools...)
add_custom_target(installer
    COMMAND "${CMAKE_CPACK_COMMAND}" --config "${CMAKE_BINARY_DIR}/CPackConfig.cmake"
    WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
    COMMENT "Building the installer with CPack"
    USES_TERMINAL)
add_dependencies(installer tb-account-toggle)
