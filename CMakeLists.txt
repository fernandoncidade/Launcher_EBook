cmake_minimum_required(VERSION 3.28)

project(LauncherEBook
    VERSION 2026.11.22.0
    DESCRIPTION "Launcher EBook built with Qt Widgets"
    LANGUAGES CXX
)

set(APP_TARGET LauncherEBook)
set(APP_DISPLAY_NAME "Launcher EBook")
set(APP_TRANSLATIONS_TARGET LauncherEBookTranslations)

set(LAUNCHER_EBOOK_DISTRIBUTION_ROOT
    "D:/MISCELANEAS/VSCode/Launcher_EBook"
    CACHE PATH "External directory used to stage Launcher EBook distribution products"
)
set(LAUNCHER_EBOOK_DISTRIBUTION_VARIANT
    ""
    CACHE STRING "Distribution variant folder suffix, for example mingw or msvc"
)

find_program(LAUNCHER_EBOOK_POWERSHELL
    NAMES pwsh.exe pwsh powershell.exe powershell
)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 6.12 REQUIRED COMPONENTS Core Gui Widgets LinguistTools)

qt_standard_project_setup()

function(resolve_qt_target_location target_name out_var)
    set(_resolved "")
    if(TARGET ${target_name})
        foreach(_prop IN ITEMS
                IMPORTED_LOCATION_RELEASE
                IMPORTED_LOCATION_RELWITHDEBINFO
                IMPORTED_LOCATION
                IMPORTED_LOCATION_DEBUG)
            get_target_property(_loc ${target_name} ${_prop})
            if(_loc AND NOT _loc MATCHES "-NOTFOUND$" AND EXISTS "${_loc}")
                set(_resolved "${_loc}")
                break()
            endif()
        endforeach()
    endif()
    set(${out_var} "${_resolved}" PARENT_SCOPE)
endfunction()

set(APP_ICON_RESOURCE)
if(WIN32)
    set(APP_ICON_FILE "${CMAKE_CURRENT_SOURCE_DIR}/assets/icon/launcherebook2_256-256.ico")
    if(EXISTS "${APP_ICON_FILE}")
        set(APP_ICON_RESOURCE "${CMAKE_CURRENT_BINARY_DIR}/${APP_TARGET}.rc")
        file(GENERATE
            OUTPUT "${APP_ICON_RESOURCE}"
            CONTENT "IDI_ICON1 ICON \"${APP_ICON_FILE}\"\n"
        )
    endif()
endif()

set(PROJECT_SOURCES
    main.cpp
    source/MainWindow.hpp
    source/MainWindow.cpp
    source/AppChooserDialog.hpp
    source/AppChooserDialog.cpp
    source/AssociatedApps.hpp
    source/AssociatedApps.cpp
    ${APP_ICON_RESOURCE}
)

qt_add_executable(${APP_TARGET}
    MANUAL_FINALIZATION
    ${PROJECT_SOURCES}
)

set_target_properties(${APP_TARGET} PROPERTIES
    OUTPUT_NAME "${APP_DISPLAY_NAME}"
)

target_compile_features(${APP_TARGET} PRIVATE cxx_std_23)

target_include_directories(${APP_TARGET}
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/source
)

target_compile_definitions(${APP_TARGET}
    PRIVATE
        EBOOKLAUNCHER_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
        EBOOKLAUNCHER_VERSION="${PROJECT_VERSION}"
)

target_link_libraries(${APP_TARGET}
    PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
)

if(MSVC)
    target_compile_options(${APP_TARGET} PRIVATE /W4 /permissive- /EHsc /utf-8 /Zc:__cplusplus)
elseif(MINGW)
    target_compile_options(${APP_TARGET} PRIVATE -Wall -Wextra -Wpedantic)
else()
    target_compile_options(${APP_TARGET} PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(NOT LAUNCHER_EBOOK_DISTRIBUTION_VARIANT)
    if(MSVC)
        set(LAUNCHER_EBOOK_DISTRIBUTION_VARIANT "msvc")
    elseif(MINGW)
        set(LAUNCHER_EBOOK_DISTRIBUTION_VARIANT "mingw")
    else()
        set(LAUNCHER_EBOOK_DISTRIBUTION_VARIANT "mingw")
    endif()
endif()

set(LAUNCHER_EBOOK_DISTRIBUTION_DIR
    "${LAUNCHER_EBOOK_DISTRIBUTION_ROOT}/Launcher_EBook_${LAUNCHER_EBOOK_DISTRIBUTION_VARIANT}"
)

set(QT_RUNTIME_DIR "")
set(LAUNCHER_EBOOK_SYNC_TOOLCHAIN "Auto")

if(WIN32)
    target_link_libraries(${APP_TARGET} PRIVATE Shlwapi Shell32)
    set_target_properties(${APP_TARGET} PROPERTIES WIN32_EXECUTABLE TRUE)

    resolve_qt_target_location(Qt6::Core QT_CORE_RUNTIME)
    if(NOT QT_CORE_RUNTIME)
        message(FATAL_ERROR "Could not locate Qt6::Core runtime library.")
    endif()

    get_filename_component(QT_RUNTIME_DIR "${QT_CORE_RUNTIME}" DIRECTORY)
    find_program(WINDEPLOYQT_EXECUTABLE
        NAMES windeployqt windeployqt.exe
        HINTS "${QT_RUNTIME_DIR}"
        NO_DEFAULT_PATH
        REQUIRED
    )

    if(MSVC)
        set(LAUNCHER_EBOOK_SYNC_TOOLCHAIN "MSVC")
        add_custom_command(TARGET ${APP_TARGET} POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E echo "Deploying minimal Qt runtime (MSVC) with ${WINDEPLOYQT_EXECUTABLE}"
            COMMAND "${WINDEPLOYQT_EXECUTABLE}"
                --release
                --no-translations
                --no-compiler-runtime
                --no-opengl-sw
                --no-system-d3d-compiler
                --no-system-dxc-compiler
                --no-ffmpeg
                --skip-plugin-types generic,iconengines,networkinformation,tls,platforminputcontexts
                --exclude-plugins qgif,qicns,qjpeg,qpdf,qsvg,qtga,qtiff,qwbmp,qwebp
                "$<TARGET_FILE:${APP_TARGET}>"
            VERBATIM
        )

        set(MSVC_REDIST_CRT_DIR "")
        if(DEFINED ENV{VCToolsRedistDir} AND NOT "$ENV{VCToolsRedistDir}" STREQUAL "")
            file(TO_CMAKE_PATH "$ENV{VCToolsRedistDir}" _vc_redist_root)
            if(EXISTS "${_vc_redist_root}/x64/Microsoft.VC145.CRT")
                set(MSVC_REDIST_CRT_DIR "${_vc_redist_root}/x64/Microsoft.VC145.CRT")
            endif()
        endif()
        if(NOT MSVC_REDIST_CRT_DIR AND EXISTS "C:/Program Files/Microsoft Visual Studio/18/Community/VC/Redist/MSVC/14.52.37025/x64/Microsoft.VC145.CRT")
            set(MSVC_REDIST_CRT_DIR "C:/Program Files/Microsoft Visual Studio/18/Community/VC/Redist/MSVC/14.52.37025/x64/Microsoft.VC145.CRT")
        endif()

        if(NOT MSVC_REDIST_CRT_DIR)
            message(FATAL_ERROR "MSVC v145 CRT redist directory not found.")
        endif()

        foreach(_crt_dll IN ITEMS
                msvcp140.dll
                msvcp140_1.dll
                msvcp140_2.dll
                vcruntime140.dll
                vcruntime140_1.dll)
            if(NOT EXISTS "${MSVC_REDIST_CRT_DIR}/${_crt_dll}")
                message(FATAL_ERROR "Required MSVC CRT DLL not found: ${MSVC_REDIST_CRT_DIR}/${_crt_dll}")
            endif()
            add_custom_command(TARGET ${APP_TARGET} POST_BUILD
                COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                    "${MSVC_REDIST_CRT_DIR}/${_crt_dll}"
                    "$<TARGET_FILE_DIR:${APP_TARGET}>/${_crt_dll}"
                VERBATIM
            )
        endforeach()
    elseif(MINGW)
        set(LAUNCHER_EBOOK_SYNC_TOOLCHAIN "MinGW")
        get_filename_component(MINGW_RUNTIME_DIR "${CMAKE_CXX_COMPILER}" DIRECTORY)
        add_custom_command(TARGET ${APP_TARGET} POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E echo "Deploying minimal Qt runtime (MinGW) with ${WINDEPLOYQT_EXECUTABLE}"
            COMMAND "${WINDEPLOYQT_EXECUTABLE}"
                --release
                --no-translations
                --no-compiler-runtime
                --no-opengl-sw
                --no-system-d3d-compiler
                --no-system-dxc-compiler
                --no-ffmpeg
                --skip-plugin-types generic,iconengines,networkinformation,tls,platforminputcontexts
                --exclude-plugins qgif,qicns,qjpeg,qpdf,qsvg,qtga,qtiff,qwbmp,qwebp
                "$<TARGET_FILE:${APP_TARGET}>"
            VERBATIM
        )
        foreach(_mingw_dll IN ITEMS libgcc_s_seh-1.dll libstdc++-6.dll libwinpthread-1.dll)
            if(NOT EXISTS "${MINGW_RUNTIME_DIR}/${_mingw_dll}")
                message(FATAL_ERROR "MinGW runtime DLL not found: ${MINGW_RUNTIME_DIR}/${_mingw_dll}")
            endif()
            add_custom_command(TARGET ${APP_TARGET} POST_BUILD
                COMMAND "${CMAKE_COMMAND}" -E copy
                    "${MINGW_RUNTIME_DIR}/${_mingw_dll}"
                    "$<TARGET_FILE_DIR:${APP_TARGET}>/${_mingw_dll}"
                VERBATIM
            )
        endforeach()
    else()
        add_custom_command(TARGET ${APP_TARGET} POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E echo "Deploying Qt runtime with ${WINDEPLOYQT_EXECUTABLE}"
            COMMAND "${WINDEPLOYQT_EXECUTABLE}" --release --no-translations "$<TARGET_FILE:${APP_TARGET}>"
            VERBATIM
        )
    endif()
endif()

set(TRANSLATION_TS_FILES
    "${CMAKE_CURRENT_SOURCE_DIR}/locale/Launcher EBook_pt_BR.ts"
    "${CMAKE_CURRENT_SOURCE_DIR}/locale/Launcher EBook_en_US.ts"
)

set(QT_BIN_HINTS)
foreach(QT_PREFIX_PATH IN LISTS CMAKE_PREFIX_PATH)
    list(APPEND QT_BIN_HINTS "${QT_PREFIX_PATH}/bin")
endforeach()
if(QT_RUNTIME_DIR)
    list(APPEND QT_BIN_HINTS "${QT_RUNTIME_DIR}")
endif()

resolve_qt_target_location(Qt6::lrelease LRELEASE_EXECUTABLE)
if(NOT LRELEASE_EXECUTABLE)
    find_program(LRELEASE_EXECUTABLE
        NAMES lrelease lrelease.exe
        HINTS ${QT_BIN_HINTS}
        NO_DEFAULT_PATH
        REQUIRED
    )
endif()

set(TRANSLATION_QM_FILES)
foreach(TS_FILE IN LISTS TRANSLATION_TS_FILES)
    get_filename_component(TS_NAME_WE "${TS_FILE}" NAME_WE)
    set(QM_FILE "${CMAKE_CURRENT_BINARY_DIR}/locale/${TS_NAME_WE}.qm")
    add_custom_command(
        OUTPUT "${QM_FILE}"
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/locale"
        COMMAND "${LRELEASE_EXECUTABLE}" -silent "${TS_FILE}" -qm "${QM_FILE}"
        DEPENDS "${TS_FILE}"
        VERBATIM
    )
    list(APPEND TRANSLATION_QM_FILES "${QM_FILE}")
endforeach()

add_custom_target(${APP_TRANSLATIONS_TARGET} ALL DEPENDS ${TRANSLATION_QM_FILES})
add_dependencies(${APP_TARGET} ${APP_TRANSLATIONS_TARGET})

add_custom_command(TARGET ${APP_TARGET} POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E make_directory
        "$<TARGET_FILE_DIR:${APP_TARGET}>/assets/icon"
    COMMAND "${CMAKE_COMMAND}" -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/about"
        "$<TARGET_FILE_DIR:${APP_TARGET}>/assets/about"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/icon/launcherebook2_256-256.ico"
        "$<TARGET_FILE_DIR:${APP_TARGET}>/assets/icon/launcherebook2_256-256.ico"
    COMMAND "${CMAKE_COMMAND}" -E make_directory
        "$<TARGET_FILE_DIR:${APP_TARGET}>/locale"
    VERBATIM
)

foreach(QM_FILE IN LISTS TRANSLATION_QM_FILES)
    add_custom_command(TARGET ${APP_TARGET} POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "${QM_FILE}"
            "$<TARGET_FILE_DIR:${APP_TARGET}>/locale/"
        VERBATIM
    )
endforeach()

if(WIN32 AND LAUNCHER_EBOOK_POWERSHELL)
    add_custom_command(TARGET ${APP_TARGET} POST_BUILD
        COMMAND "${LAUNCHER_EBOOK_POWERSHELL}"
            -NoProfile
            -ExecutionPolicy Bypass
            -File "${CMAKE_CURRENT_SOURCE_DIR}/repo/sync_distribution.ps1"
            -SourceDir "$<TARGET_FILE_DIR:${APP_TARGET}>"
            -DestinationDir "${LAUNCHER_EBOOK_DISTRIBUTION_DIR}"
            -Mode Main
            -Toolchain "${LAUNCHER_EBOOK_SYNC_TOOLCHAIN}"
        COMMENT "Synchronizing distribution product to ${LAUNCHER_EBOOK_DISTRIBUTION_DIR}"
        VERBATIM
    )
elseif(WIN32)
    message(WARNING "PowerShell was not found; external distribution sync is disabled.")
endif()

install(TARGETS ${APP_TARGET}
    BUNDLE DESTINATION .
    RUNTIME DESTINATION bin
)

qt_finalize_executable(${APP_TARGET})
