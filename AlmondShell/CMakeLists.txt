cmake_minimum_required(VERSION 3.28)

set(CMAKE_EXPERIMENTAL_CXX_MODULE_DYNDEP 1)
set(CMAKE_CXX_SCAN_FOR_MODULES ON)

project(AlmondShell LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(ALMONDSHELL_SOURCES
    src/aengine.cpp
    src/acontext.cpp
    src/aeditor.cpp
    src/agui.cpp
    src/afontrenderer.cpp
    src/afilewatch.cpp
    src/ascriptingsystem.cpp
    src/acompiler.cpp
    src/updater_bootstrap.cpp
)

if(WIN32)
    list(APPEND ALMONDSHELL_SOURCES
        src/acontextmultiplexer_win.cpp
        src/aopenglplatform_win.cpp
        src/WinResource.rc
    )
elseif(UNIX AND NOT APPLE)
    list(APPEND ALMONDSHELL_SOURCES
        src/acontextmultiplexer_linux.cpp
        src/aopenglplatform_linux.cpp
    )
endif()

add_executable(almondshell ${ALMONDSHELL_SOURCES})

set(ALMONDSHELL_MODULE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/modules)

target_sources(almondshell PRIVATE
    FILE_SET almondshell_modules TYPE CXX_MODULES
        BASE_DIRS ${ALMONDSHELL_MODULE_DIR}
        FILES
            ${ALMONDSHELL_MODULE_DIR}/aengine.ixx
            ${ALMONDSHELL_MODULE_DIR}/aengine.platform.ixx
            ${ALMONDSHELL_MODULE_DIR}/aengine.engine_components.ixx
            ${ALMONDSHELL_MODULE_DIR}/aengine.renderers.ixx
)

target_include_directories(almondshell PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/stb
)

if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    target_compile_options(almondshell PRIVATE /experimental:module)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(almondshell PRIVATE -fmodules-ts)
endif()

option(ALMOND_ENABLE_RAYLIB "Enable the Raylib backend" ON)
option(ALMOND_ENABLE_SDL "Enable the SDL backend" ON)
option(ALMOND_ENABLE_OPENGL "Enable the OpenGL backend" ON)
option(ALMOND_ENABLE_SOFTWARE_RENDERER "Enable the software renderer backend" ON)
option(ALMOND_REQUIRE_OPTIONAL_DEPENDENCIES "Treat missing optional backend dependencies as a configuration error" OFF)

set(ALMOND_RAYLIB_ACTIVE ${ALMOND_ENABLE_RAYLIB})
set(ALMOND_SDL_ACTIVE ${ALMOND_ENABLE_SDL})
set(ALMOND_OPENGL_ACTIVE ${ALMOND_ENABLE_OPENGL})
set(ALMOND_SOFTWARE_RENDERER_ACTIVE ${ALMOND_ENABLE_SOFTWARE_RENDERER})

find_package(PkgConfig QUIET)

if(ALMOND_RAYLIB_ACTIVE)
    find_package(raylib CONFIG QUIET)
    if(raylib_FOUND)
        target_link_libraries(almondshell PRIVATE raylib)
    else()
        find_library(RAYLIB_LIBRARY NAMES raylib)
        find_path(RAYLIB_INCLUDE_DIR raylib.h PATH_SUFFIXES raylib)
        if(RAYLIB_LIBRARY AND RAYLIB_INCLUDE_DIR)
            target_include_directories(almondshell PRIVATE ${RAYLIB_INCLUDE_DIR})
            target_link_libraries(almondshell PRIVATE ${RAYLIB_LIBRARY})
        else()
            if(ALMOND_REQUIRE_OPTIONAL_DEPENDENCIES)
                message(FATAL_ERROR "raylib was not found. Install it via vcpkg, your system package manager, or configure with -DALMOND_ENABLE_RAYLIB=OFF.")
            else()
                message(WARNING "raylib was not found; the Raylib backend will be disabled. Configure with -DALMOND_ENABLE_RAYLIB=OFF to silence this warning or enable ALMOND_REQUIRE_OPTIONAL_DEPENDENCIES to treat it as an error.")
                set(ALMOND_RAYLIB_ACTIVE OFF)
                set(ALMOND_ENABLE_RAYLIB OFF CACHE BOOL "Enable the Raylib backend" FORCE)
            endif()
        endif()
    endif()

    if(ALMOND_RAYLIB_ACTIVE AND UNIX AND NOT APPLE)
        find_package(glfw3 CONFIG QUIET)
        if(glfw3_FOUND)
            target_link_libraries(almondshell PRIVATE glfw)
        elseif(PkgConfig_FOUND)
            pkg_check_modules(GLFW3 QUIET glfw3)
            if(GLFW3_FOUND)
                target_include_directories(almondshell PRIVATE ${GLFW3_INCLUDE_DIRS})
                target_link_libraries(almondshell PRIVATE ${GLFW3_LIBRARIES})
            else()
                if(ALMOND_REQUIRE_OPTIONAL_DEPENDENCIES)
                    message(FATAL_ERROR "glfw3 was not found. Install it or disable the Raylib backend with -DALMOND_ENABLE_RAYLIB=OFF.")
                else()
                    message(WARNING "glfw3 was not found; the Raylib backend will be disabled. Configure with -DALMOND_ENABLE_RAYLIB=OFF to silence this warning or enable ALMOND_REQUIRE_OPTIONAL_DEPENDENCIES to treat it as an error.")
                    set(ALMOND_RAYLIB_ACTIVE OFF)
                    set(ALMOND_ENABLE_RAYLIB OFF CACHE BOOL "Enable the Raylib backend" FORCE)
                endif()
            endif()
        else()
            if(ALMOND_REQUIRE_OPTIONAL_DEPENDENCIES)
                message(FATAL_ERROR "glfw3 was not found. Install it or disable the Raylib backend with -DALMOND_ENABLE_RAYLIB=OFF.")
            else()
                message(WARNING "glfw3 was not found; the Raylib backend will be disabled. Configure with -DALMOND_ENABLE_RAYLIB=OFF to silence this warning or enable ALMOND_REQUIRE_OPTIONAL_DEPENDENCIES to treat it as an error.")
                set(ALMOND_RAYLIB_ACTIVE OFF)
                set(ALMOND_ENABLE_RAYLIB OFF CACHE BOOL "Enable the Raylib backend" FORCE)
            endif()
        endif()
    endif()
endif()

if(ALMOND_SDL_ACTIVE)
    find_package(SDL3 CONFIG QUIET)
    if(SDL3_FOUND)
        target_link_libraries(almondshell PRIVATE SDL3::SDL3)
    else()
        if(PkgConfig_FOUND)
            pkg_check_modules(SDL3 QUIET sdl3)
            if(SDL3_FOUND)
                target_include_directories(almondshell PRIVATE ${SDL3_INCLUDE_DIRS})
                target_link_libraries(almondshell PRIVATE ${SDL3_LIBRARIES})
            else()
                if(ALMOND_REQUIRE_OPTIONAL_DEPENDENCIES)
                    message(FATAL_ERROR "SDL3 was not found. Install it via vcpkg or rerun with -DALMOND_ENABLE_SDL=OFF.")
                else()
                    message(WARNING "SDL3 was not found; the SDL backend will be disabled. Configure with -DALMOND_ENABLE_SDL=OFF to silence this warning or enable ALMOND_REQUIRE_OPTIONAL_DEPENDENCIES to treat it as an error.")
                    set(ALMOND_SDL_ACTIVE OFF)
                    set(ALMOND_ENABLE_SDL OFF CACHE BOOL "Enable the SDL backend" FORCE)
                endif()
            endif()
        else()
            if(ALMOND_REQUIRE_OPTIONAL_DEPENDENCIES)
                message(FATAL_ERROR "SDL3 was not found. Install it via vcpkg or rerun with -DALMOND_ENABLE_SDL=OFF.")
            else()
                message(WARNING "SDL3 was not found; the SDL backend will be disabled. Configure with -DALMOND_ENABLE_SDL=OFF to silence this warning or enable ALMOND_REQUIRE_OPTIONAL_DEPENDENCIES to treat it as an error.")
                set(ALMOND_SDL_ACTIVE OFF)
                set(ALMOND_ENABLE_SDL OFF CACHE BOOL "Enable the SDL backend" FORCE)
            endif()
        endif()
    endif()

    if(ALMOND_SDL_ACTIVE)
        find_package(SDL3_image CONFIG QUIET)
        if(SDL3_image_FOUND)
            target_link_libraries(almondshell PRIVATE SDL3_image::SDL3_image)
        endif()
    endif()
endif()

find_package(OpenGL COMPONENTS OpenGL QUIET)
if(OpenGL_FOUND)
    target_link_libraries(almondshell PRIVATE OpenGL::GL)
elseif(WIN32)
    target_link_libraries(almondshell PRIVATE opengl32)
endif()

if(ALMOND_OPENGL_ACTIVE OR ALMOND_RAYLIB_ACTIVE OR ALMOND_SDL_ACTIVE)
    set(ALMOND_GLAD_NEED_HEADERS ON)
else()
    set(ALMOND_GLAD_NEED_HEADERS OFF)
endif()

set(ALMOND_GLAD_SHOULD_LINK OFF)
if((ALMOND_OPENGL_ACTIVE OR ALMOND_SDL_ACTIVE) AND NOT ALMOND_RAYLIB_ACTIVE)
    set(ALMOND_GLAD_SHOULD_LINK ON)
endif()

if(ALMOND_GLAD_NEED_HEADERS)
    find_package(glad CONFIG QUIET)
    set(ALMOND_GLAD_TARGET "")
    if(TARGET glad::glad)
        set(ALMOND_GLAD_TARGET glad::glad)
    endif()

    set(_almond_builtin_glad_dir ${CMAKE_CURRENT_SOURCE_DIR}/third_party/glad)

    if(ALMOND_GLAD_TARGET)
        if(ALMOND_GLAD_SHOULD_LINK)
            target_link_libraries(almondshell PRIVATE ${ALMOND_GLAD_TARGET})
        else()
            target_include_directories(almondshell PRIVATE $<TARGET_PROPERTY:${ALMOND_GLAD_TARGET},INTERFACE_INCLUDE_DIRECTORIES>)
        endif()
    elseif(ALMOND_GLAD_SHOULD_LINK)
        if(EXISTS ${_almond_builtin_glad_dir}/src/glad.c AND EXISTS ${_almond_builtin_glad_dir}/include/glad/glad.h)
            add_library(almond_builtin_glad STATIC ${_almond_builtin_glad_dir}/src/glad.c)
            target_include_directories(almond_builtin_glad PUBLIC ${_almond_builtin_glad_dir}/include)
            if(NOT TARGET glad::glad)
                add_library(glad::glad ALIAS almond_builtin_glad)
            endif()
            target_link_libraries(almondshell PRIVATE glad::glad)
        else()
            find_path(GLAD_INCLUDE_DIR glad/glad.h)
            find_library(GLAD_LIBRARY NAMES glad)
            if(GLAD_INCLUDE_DIR)
                target_include_directories(almondshell PRIVATE ${GLAD_INCLUDE_DIR})
            endif()
            if(GLAD_LIBRARY)
                target_link_libraries(almondshell PRIVATE ${GLAD_LIBRARY})
            else()
                message(FATAL_ERROR "glad was not found. Install it via vcpkg, your system package manager, or provide a glad::glad target before configuring AlmondShell.")
            endif()
            if(NOT GLAD_INCLUDE_DIR)
                message(FATAL_ERROR "glad headers were not found. Install them alongside the loader or provide a glad::glad target before configuring AlmondShell.")
            endif()
        endif()
    else()
        if(EXISTS ${_almond_builtin_glad_dir}/include/glad/glad.h)
            target_include_directories(almondshell PRIVATE ${_almond_builtin_glad_dir}/include)
        else()
            find_path(GLAD_INCLUDE_DIR glad/glad.h)
            if(GLAD_INCLUDE_DIR)
                target_include_directories(almondshell PRIVATE ${GLAD_INCLUDE_DIR})
            else()
                message(FATAL_ERROR "glad headers were not found. Install them alongside the loader or provide a glad::glad target before configuring AlmondShell.")
            endif()
        endif()
    endif()
endif()

target_compile_definitions(almondshell PRIVATE
    $<$<BOOL:${ALMOND_RAYLIB_ACTIVE}>:ALMOND_FORCE_ENABLE_RAYLIB>
    $<$<NOT:$<BOOL:${ALMOND_RAYLIB_ACTIVE}>>:ALMOND_FORCE_DISABLE_RAYLIB>
    $<$<BOOL:${ALMOND_SDL_ACTIVE}>:ALMOND_FORCE_ENABLE_SDL>
    $<$<NOT:$<BOOL:${ALMOND_SDL_ACTIVE}>>:ALMOND_FORCE_DISABLE_SDL>
    $<$<BOOL:${ALMOND_OPENGL_ACTIVE}>:ALMOND_FORCE_ENABLE_OPENGL>
    $<$<NOT:$<BOOL:${ALMOND_OPENGL_ACTIVE}>>:ALMOND_FORCE_DISABLE_OPENGL>
    $<$<BOOL:${ALMOND_SOFTWARE_RENDERER_ACTIVE}>:ALMOND_FORCE_ENABLE_SOFTWARE_RENDERER>
    $<$<NOT:$<BOOL:${ALMOND_SOFTWARE_RENDERER_ACTIVE}>>:ALMOND_FORCE_DISABLE_SOFTWARE_RENDERER>
)

find_package(glm CONFIG QUIET)
if(glm_FOUND)
    target_link_libraries(almondshell PRIVATE glm::glm)
endif()

find_package(fmt CONFIG QUIET)
if(fmt_FOUND)
    target_link_libraries(almondshell PRIVATE fmt::fmt)
endif()

find_package(asio CONFIG QUIET)
if(asio_FOUND)
    target_link_libraries(almondshell PRIVATE asio::asio)
endif()

find_package(Threads REQUIRED)
target_link_libraries(almondshell PRIVATE Threads::Threads)

if(UNIX AND NOT APPLE)
    find_package(X11 REQUIRED)
    if(X11_INCLUDE_DIR)
        target_include_directories(almondshell PRIVATE ${X11_INCLUDE_DIR})
    endif()
    target_link_libraries(almondshell PRIVATE dl m X11::X11 GL)
    if(TARGET X11::Xrandr)
        target_link_libraries(almondshell PRIVATE X11::Xrandr)
    elseif(X11_Xrandr_LIB)
        target_link_libraries(almondshell PRIVATE ${X11_Xrandr_LIB})
    endif()
endif()

find_package(Doxygen QUIET)

if(DOXYGEN_FOUND)
    add_custom_target(docs
        COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_SOURCE_DIR}/docs/api
        COMMAND ${DOXYGEN_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMENT "Generating AlmondShell API documentation"
        VERBATIM)
else()
    message(STATUS "Doxygen not found; the 'docs' target will be skipped.")
endif()
