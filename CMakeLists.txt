cmake_minimum_required(VERSION 3.20)

project(PrintDrop VERSION 0.1.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

option(PD_BUILD_TESTS "Build PrintDrop tests" ON)
option(PD_ENABLE_WERROR "Treat compiler warnings as errors" ON)
option(PD_ENABLE_SANITIZERS "Enable AddressSanitizer and UndefinedBehaviorSanitizer" OFF)

if(MSVC)
    cmake_policy(SET CMP0091 NEW)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

if(PD_ENABLE_SANITIZERS AND NOT MSVC)
    add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
endif()

function(pd_apply_warnings target_name)
    if(MSVC)
        target_compile_options(${target_name} PRIVATE /W4)
        if(PD_ENABLE_WERROR)
            target_compile_options(${target_name} PRIVATE /WX)
        endif()
    else()
        target_compile_options(${target_name} PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow)
        if(PD_ENABLE_WERROR)
            target_compile_options(${target_name} PRIVATE -Werror)
        endif()
    endif()
endfunction()

add_library(printdrop_core STATIC
    src/protocol.c
    src/session.c
    src/transport.c
    src/receiver_session.c
    src/session_url.c
)

target_include_directories(printdrop_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

pd_apply_warnings(printdrop_core)

if(WIN32)
    add_executable(printdrop WIN32 src/win32_main.c)
    target_link_libraries(printdrop PRIVATE printdrop_core user32 gdi32)
    target_compile_definitions(printdrop PRIVATE
        WINVER=0x0601
        _WIN32_WINNT=0x0601
        NTDDI_VERSION=0x06010000
        UNICODE
        _UNICODE
    )
    pd_apply_warnings(printdrop)
endif()

if(PD_BUILD_TESTS)
    enable_testing()

    add_executable(printdrop_core_tests tests/test_core.c)
    target_link_libraries(printdrop_core_tests PRIVATE printdrop_core)
    pd_apply_warnings(printdrop_core_tests)

    add_test(NAME printdrop_core_tests COMMAND printdrop_core_tests)
endif()
