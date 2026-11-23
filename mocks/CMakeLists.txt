cmake_minimum_required(VERSION 3.28)

project(LauncherEBookMocks LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)

if(MSVC)
    set(_ebook_mock_default_qt_root "C:/Qt/6.12.0/msvc2022_64")
else()
    set(_ebook_mock_default_qt_root "C:/Qt/6.12.0/mingw_64")
endif()

set(EBOOK_MOCK_QT_ROOT
    "${_ebook_mock_default_qt_root}"
    CACHE PATH "Qt 6 root used to build the mock tools"
)

if(NOT EXISTS "${EBOOK_MOCK_QT_ROOT}/lib/cmake/Qt6/Qt6Config.cmake")
    if(DEFINED ENV{QTDIR} AND EXISTS "$ENV{QTDIR}/lib/cmake/Qt6/Qt6Config.cmake")
        set(EBOOK_MOCK_QT_ROOT "$ENV{QTDIR}" CACHE PATH "Qt 6 root used to build the mock tools" FORCE)
    endif()
endif()

if(NOT EXISTS "${EBOOK_MOCK_QT_ROOT}/lib/cmake/Qt6/Qt6Config.cmake")
    message(FATAL_ERROR "Qt6Config.cmake not found. Set EBOOK_MOCK_QT_ROOT or QTDIR.")
endif()

list(PREPEND CMAKE_PREFIX_PATH "${EBOOK_MOCK_QT_ROOT}")

find_package(Qt6 6.12 REQUIRED COMPONENTS Core Gui Widgets)

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

get_filename_component(EBOOK_PROJECT_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/.." ABSOLUTE)

if(MSVC)
    set(_ebook_mock_default_variant "msvc")
elseif(MINGW)
    set(_ebook_mock_default_variant "mingw")
else()
    set(_ebook_mock_default_variant "mingw")
endif()

set(EBOOK_MOCK_DISTRIBUTION_VARIANT
    "${_ebook_mock_default_variant}"
    CACHE STRING "Mock distribution variant folder suffix, for example mingw or msvc"
)

set(EBOOK_MOCK_OUTPUT_DIR
    "${EBOOK_PROJECT_ROOT}/build/mocks_${EBOOK_MOCK_DISTRIBUTION_VARIANT}"
    CACHE PATH "Runtime directory for mock executables"
)

set(EBOOK_MOCK_DISTRIBUTION_ROOT
    "D:/MISCELANEAS/VSCode/Launcher_EBook"
    CACHE PATH "External directory used to stage Launcher EBook mock products"
)

find_program(EBOOK_MOCK_POWERSHELL
    NAMES pwsh.exe pwsh powershell.exe powershell
)

file(MAKE_DIRECTORY "${EBOOK_MOCK_OUTPUT_DIR}")

function(configure_ebook_mock target_name)
    target_compile_features(${target_name} PRIVATE cxx_std_23)
    if(MSVC)
        target_compile_options(${target_name} PRIVATE /W4 /permissive- /EHsc /utf-8 /Zc:__cplusplus)
    elseif(MINGW)
        target_compile_options(${target_name} PRIVATE -Wall -Wextra -Wpedantic)
    else()
        target_compile_options(${target_name} PRIVATE -Wall -Wextra -Wpedantic)
    endif()

    target_include_directories(${target_name} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
    target_compile_definitions(${target_name} PRIVATE
        EBOOK_PROJECT_ROOT_PATH="${EBOOK_PROJECT_ROOT}"
        EBOOK_MOCK_QT_ROOT_PATH="${EBOOK_MOCK_QT_ROOT}"
    )
    set_target_properties(${target_name} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${EBOOK_MOCK_OUTPUT_DIR}"
    )
endfunction()

add_executable(compile_translations WIN32
    compile_translations.cpp
    compile_translations.h
)
target_link_libraries(compile_translations PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets)
configure_ebook_mock(compile_translations)

add_executable(find_VersionEditor WIN32
    find_VersionEditor.cpp
    find_VersionEditor.h
)
target_link_libraries(find_VersionEditor PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets)
configure_ebook_mock(find_VersionEditor)

set(EBOOK_MOCK_DEPLOY_DIR "${EBOOK_MOCK_DISTRIBUTION_ROOT}/mocks_${EBOOK_MOCK_DISTRIBUTION_VARIANT}")

if(WIN32)
    resolve_qt_target_location(Qt6::Core QT_CORE_RUNTIME)
    if(QT_CORE_RUNTIME)
        get_filename_component(QT_RUNTIME_DIR "${QT_CORE_RUNTIME}" DIRECTORY)
    else()
        set(QT_RUNTIME_DIR "${EBOOK_MOCK_QT_ROOT}/bin")
    endif()

    find_program(WINDEPLOYQT_EXECUTABLE
        NAMES windeployqt windeployqt.exe
        HINTS "${QT_RUNTIME_DIR}" "${EBOOK_MOCK_QT_ROOT}/bin"
        NO_DEFAULT_PATH
        REQUIRED
    )

    add_custom_target(deploy_mocks ALL
        COMMAND "${WINDEPLOYQT_EXECUTABLE}"
            --release
            --no-translations
            --no-compiler-runtime
            --no-opengl-sw
            --no-system-d3d-compiler
            --no-system-dxc-compiler
            --no-ffmpeg
            --skip-plugin-types generic,iconengines,imageformats,networkinformation,tls,platforminputcontexts
            "$<TARGET_FILE:compile_translations>"
        DEPENDS compile_translations find_VersionEditor
        COMMENT "Deploying minimal Qt runtime and synchronizing mocks to ${EBOOK_MOCK_DEPLOY_DIR}"
        VERBATIM
    )

    add_custom_command(TARGET deploy_mocks POST_BUILD
        COMMAND "${WINDEPLOYQT_EXECUTABLE}"
            --release
            --no-translations
            --no-compiler-runtime
            --no-opengl-sw
            --no-system-d3d-compiler
            --no-system-dxc-compiler
            --no-ffmpeg
            --skip-plugin-types generic,iconengines,imageformats,networkinformation,tls,platforminputcontexts
            "$<TARGET_FILE:find_VersionEditor>"
        VERBATIM
    )

    set(_mock_sync_toolchain "Auto")
    if(MSVC)
        set(_mock_sync_toolchain "MSVC")
        set(_msvc_redist_crt_dir "")
        if(DEFINED ENV{VCToolsRedistDir} AND NOT "$ENV{VCToolsRedistDir}" STREQUAL "")
            file(TO_CMAKE_PATH "$ENV{VCToolsRedistDir}" _vc_redist_root)
            if(EXISTS "${_vc_redist_root}/x64/Microsoft.VC145.CRT")
                set(_msvc_redist_crt_dir "${_vc_redist_root}/x64/Microsoft.VC145.CRT")
            endif()
        endif()
        if(NOT _msvc_redist_crt_dir AND EXISTS "C:/Program Files/Microsoft Visual Studio/18/Community/VC/Redist/MSVC/14.52.37025/x64/Microsoft.VC145.CRT")
            set(_msvc_redist_crt_dir "C:/Program Files/Microsoft Visual Studio/18/Community/VC/Redist/MSVC/14.52.37025/x64/Microsoft.VC145.CRT")
        endif()

        if(NOT _msvc_redist_crt_dir)
            message(FATAL_ERROR "MSVC v145 CRT redist directory not found.")
        endif()

        foreach(_crt_dll IN ITEMS
                msvcp140.dll
                msvcp140_1.dll
                msvcp140_2.dll
                vcruntime140.dll
                vcruntime140_1.dll)
            if(NOT EXISTS "${_msvc_redist_crt_dir}/${_crt_dll}")
                message(FATAL_ERROR "Required MSVC CRT DLL not found: ${_msvc_redist_crt_dir}/${_crt_dll}")
            endif()
            add_custom_command(TARGET deploy_mocks POST_BUILD
                COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                    "${_msvc_redist_crt_dir}/${_crt_dll}"
                    "${EBOOK_MOCK_OUTPUT_DIR}/${_crt_dll}"
                VERBATIM
            )
        endforeach()
    elseif(MINGW)
        set(_mock_sync_toolchain "MinGW")
        get_filename_component(_mingw_runtime_dir "${CMAKE_CXX_COMPILER}" DIRECTORY)
        foreach(_mingw_dll IN ITEMS libgcc_s_seh-1.dll libstdc++-6.dll libwinpthread-1.dll)
            if(NOT EXISTS "${_mingw_runtime_dir}/${_mingw_dll}")
                message(FATAL_ERROR "MinGW runtime DLL not found: ${_mingw_runtime_dir}/${_mingw_dll}")
            endif()
            add_custom_command(TARGET deploy_mocks POST_BUILD
                COMMAND "${CMAKE_COMMAND}" -E copy
                    "${_mingw_runtime_dir}/${_mingw_dll}"
                    "${EBOOK_MOCK_OUTPUT_DIR}/${_mingw_dll}"
                VERBATIM
            )
        endforeach()
    endif()

    if(EBOOK_MOCK_POWERSHELL)
        add_custom_command(TARGET deploy_mocks POST_BUILD
            COMMAND "${EBOOK_MOCK_POWERSHELL}"
                -NoProfile
                -ExecutionPolicy Bypass
                -File "${EBOOK_PROJECT_ROOT}/repo/sync_distribution.ps1"
                -SourceDir "${EBOOK_MOCK_OUTPUT_DIR}"
                -DestinationDir "${EBOOK_MOCK_DEPLOY_DIR}"
                -Mode Runtime
                -Toolchain "${_mock_sync_toolchain}"
            VERBATIM
        )
    else()
        message(WARNING "PowerShell was not found; external mock distribution sync is disabled.")
    endif()
else()
    add_custom_target(deploy_mocks ALL
        DEPENDS compile_translations find_VersionEditor
    )
endif()
