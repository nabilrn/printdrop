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

include(FetchContent)
FetchContent_Declare(
    qrcodegen
    GIT_REPOSITORY https://github.com/nayuki/QR-Code-generator.git
    GIT_TAG 2c9044de6b049ca25cb3cd1649ed7e27aa055138
    GIT_SHALLOW FALSE
)
FetchContent_GetProperties(qrcodegen)
if(NOT qrcodegen_POPULATED)
    FetchContent_Populate(qrcodegen)
endif()

add_library(printdrop_qrcodegen STATIC
    ${qrcodegen_SOURCE_DIR}/c/qrcodegen.c
)
target_include_directories(printdrop_qrcodegen SYSTEM
    PUBLIC
        ${qrcodegen_SOURCE_DIR}/c
)
set_target_properties(printdrop_qrcodegen PROPERTIES
    C_STANDARD 11
    C_STANDARD_REQUIRED ON
    C_EXTENSIONS OFF
)

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
    src/qr.c
    src/frame.c
    src/filename.c
    src/file_receive.c
    src/job_id.c
    src/integrity.c
    src/file_begin.c
    src/receiver_protocol.c
    src/secure_zero.c
    src/ack.c
    src/relay_message.c
)

target_include_directories(printdrop_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(printdrop_core PRIVATE printdrop_qrcodegen)

pd_apply_warnings(printdrop_core)

if(WIN32)
    set(BUILD_CURL_EXE OFF CACHE BOOL "" FORCE)
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
    set(BUILD_STATIC_LIBS ON CACHE BOOL "" FORCE)
    set(CURL_STATIC_CRT ON CACHE BOOL "" FORCE)
    set(CURL_DISABLE_INSTALL ON CACHE BOOL "" FORCE)
    set(CURL_TARGET_WINDOWS_VERSION "0x0601" CACHE STRING "" FORCE)
    set(CURL_USE_SCHANNEL ON CACHE BOOL "" FORCE)
    set(CURL_USE_OPENSSL OFF CACHE BOOL "" FORCE)
    set(CURL_USE_LIBPSL OFF CACHE BOOL "" FORCE)
    set(CURL_USE_LIBSSH2 OFF CACHE BOOL "" FORCE)
    set(USE_NGHTTP2 OFF CACHE BOOL "" FORCE)
    set(CURL_ZLIB OFF CACHE STRING "" FORCE)
    set(CURL_BROTLI OFF CACHE STRING "" FORCE)
    set(CURL_ZSTD OFF CACHE STRING "" FORCE)
    set(BUILD_LIBCURL_DOCS OFF CACHE BOOL "" FORCE)
    set(BUILD_MISC_DOCS OFF CACHE BOOL "" FORCE)
    set(ENABLE_CURL_MANUAL OFF CACHE BOOL "" FORCE)
    set(CURL_DISABLE_DICT ON CACHE BOOL "" FORCE)
    set(CURL_DISABLE_FILE ON CACHE BOOL "" FORCE)
    set(CURL_DISABLE_FTP ON CACHE BOOL "" FORCE)
    set(CURL_DISABLE_GOPHER ON CACHE BOOL "" FORCE)
    set(CURL_DISABLE_IMAP ON CACHE BOOL "" FORCE)
    set(CURL_DISABLE_LDAP ON CACHE BOOL "" FORCE)
    set(CURL_DISABLE_LDAPS ON CACHE BOOL "" FORCE)
    set(CURL_DISABLE_MQTT ON CACHE BOOL "" FORCE)
    set(CURL_DISABLE_POP3 ON CACHE BOOL "" FORCE)
    set(CURL_DISABLE_RTSP ON CACHE BOOL "" FORCE)
    set(CURL_DISABLE_SMTP ON CACHE BOOL "" FORCE)
    set(CURL_DISABLE_TELNET ON CACHE BOOL "" FORCE)
    set(CURL_DISABLE_TFTP ON CACHE BOOL "" FORCE)
    set(CURL_DISABLE_WEBSOCKETS OFF CACHE BOOL "" FORCE)
    set(CURL_DISABLE_ALTSVC ON CACHE BOOL "" FORCE)
    set(CURL_DISABLE_COOKIES ON CACHE BOOL "" FORCE)
    set(CURL_DISABLE_DOH ON CACHE BOOL "" FORCE)
    set(CURL_DISABLE_MIME ON CACHE BOOL "" FORCE)
    set(CURL_DISABLE_NETRC ON CACHE BOOL "" FORCE)
    set(CURL_DISABLE_PARSEDATE ON CACHE BOOL "" FORCE)
    set(CURL_DISABLE_PROGRESS_METER ON CACHE BOOL "" FORCE)

    FetchContent_Declare(
        curl
        GIT_REPOSITORY https://github.com/curl/curl.git
        GIT_TAG 68720b4837284335b2d63cb358f8f6ce65f5bc55
        GIT_SHALLOW FALSE
    )
    FetchContent_MakeAvailable(curl)

    target_sources(printdrop_core PRIVATE
        src/win32_file_sink.c
        src/win32_sha256.c
        src/win32_random.c
        src/win32_receive_handler.c
        src/win32_relay_client.c
    )
    target_link_libraries(printdrop_core PRIVATE bcrypt ws2_32 CURL::libcurl_static)

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

    add_executable(printdrop_qr_tests tests/test_qr.c)
    target_link_libraries(printdrop_qr_tests PRIVATE printdrop_core)
    pd_apply_warnings(printdrop_qr_tests)

    add_executable(printdrop_frame_tests tests/test_frame.c)
    target_link_libraries(printdrop_frame_tests PRIVATE printdrop_core)
    pd_apply_warnings(printdrop_frame_tests)

    add_executable(printdrop_filename_tests tests/test_filename.c)
    target_link_libraries(printdrop_filename_tests PRIVATE printdrop_core)
    pd_apply_warnings(printdrop_filename_tests)

    add_executable(printdrop_file_receive_tests tests/test_file_receive.c)
    target_link_libraries(printdrop_file_receive_tests PRIVATE printdrop_core)
    pd_apply_warnings(printdrop_file_receive_tests)

    add_executable(printdrop_job_id_tests tests/test_job_id.c)
    target_link_libraries(printdrop_job_id_tests PRIVATE printdrop_core)
    pd_apply_warnings(printdrop_job_id_tests)

    add_executable(printdrop_integrity_tests tests/test_integrity.c)
    target_link_libraries(printdrop_integrity_tests PRIVATE printdrop_core)
    pd_apply_warnings(printdrop_integrity_tests)

    add_executable(printdrop_file_integrity_tests tests/test_file_integrity.c)
    target_link_libraries(printdrop_file_integrity_tests PRIVATE printdrop_core)
    pd_apply_warnings(printdrop_file_integrity_tests)

    add_executable(printdrop_file_begin_tests tests/test_file_begin.c)
    target_link_libraries(printdrop_file_begin_tests PRIVATE printdrop_core)
    pd_apply_warnings(printdrop_file_begin_tests)

    add_executable(printdrop_receiver_protocol_tests tests/test_receiver_protocol.c)
    target_link_libraries(printdrop_receiver_protocol_tests PRIVATE printdrop_core)
    pd_apply_warnings(printdrop_receiver_protocol_tests)

    add_executable(printdrop_ack_tests tests/test_ack.c)
    target_link_libraries(printdrop_ack_tests PRIVATE printdrop_core)
    pd_apply_warnings(printdrop_ack_tests)

    add_executable(printdrop_relay_message_tests tests/test_relay_message.c)
    target_link_libraries(printdrop_relay_message_tests PRIVATE printdrop_core)
    pd_apply_warnings(printdrop_relay_message_tests)

    add_test(NAME printdrop_core_tests COMMAND printdrop_core_tests)
    add_test(NAME printdrop_qr_tests COMMAND printdrop_qr_tests)
    add_test(NAME printdrop_frame_tests COMMAND printdrop_frame_tests)
    add_test(NAME printdrop_filename_tests COMMAND printdrop_filename_tests)
    add_test(NAME printdrop_file_receive_tests COMMAND printdrop_file_receive_tests)
    add_test(NAME printdrop_job_id_tests COMMAND printdrop_job_id_tests)
    add_test(NAME printdrop_integrity_tests COMMAND printdrop_integrity_tests)
    add_test(NAME printdrop_file_integrity_tests COMMAND printdrop_file_integrity_tests)
    add_test(NAME printdrop_file_begin_tests COMMAND printdrop_file_begin_tests)
    add_test(NAME printdrop_receiver_protocol_tests COMMAND printdrop_receiver_protocol_tests)
    add_test(NAME printdrop_ack_tests COMMAND printdrop_ack_tests)
    add_test(NAME printdrop_relay_message_tests COMMAND printdrop_relay_message_tests)

    if(WIN32)
        add_executable(printdrop_win32_file_sink_tests tests/test_win32_file_sink.c)
        target_link_libraries(printdrop_win32_file_sink_tests PRIVATE printdrop_core)
        target_compile_definitions(printdrop_win32_file_sink_tests PRIVATE
            WINVER=0x0601
            _WIN32_WINNT=0x0601
            NTDDI_VERSION=0x06010000
            UNICODE
            _UNICODE
        )
        pd_apply_warnings(printdrop_win32_file_sink_tests)
        add_test(NAME printdrop_win32_file_sink_tests COMMAND printdrop_win32_file_sink_tests)

        add_executable(printdrop_win32_sha256_tests tests/test_win32_sha256.c)
        target_link_libraries(printdrop_win32_sha256_tests PRIVATE printdrop_core)
        target_compile_definitions(printdrop_win32_sha256_tests PRIVATE
            WINVER=0x0601
            _WIN32_WINNT=0x0601
            NTDDI_VERSION=0x06010000
        )
        pd_apply_warnings(printdrop_win32_sha256_tests)
        add_test(NAME printdrop_win32_sha256_tests COMMAND printdrop_win32_sha256_tests)

        add_executable(printdrop_win32_receive_handler_tests tests/test_win32_receive_handler.c)
        target_link_libraries(printdrop_win32_receive_handler_tests PRIVATE printdrop_core)
        target_compile_definitions(printdrop_win32_receive_handler_tests PRIVATE
            WINVER=0x0601
            _WIN32_WINNT=0x0601
            NTDDI_VERSION=0x06010000
            UNICODE
            _UNICODE
        )
        pd_apply_warnings(printdrop_win32_receive_handler_tests)
        add_test(NAME printdrop_win32_receive_handler_tests COMMAND printdrop_win32_receive_handler_tests)

        add_executable(printdrop_win32_relay_client_tests tests/test_win32_relay_client.c)
        target_link_libraries(printdrop_win32_relay_client_tests PRIVATE printdrop_core)
        target_compile_definitions(printdrop_win32_relay_client_tests PRIVATE
            WINVER=0x0601
            _WIN32_WINNT=0x0601
            NTDDI_VERSION=0x06010000
            UNICODE
            _UNICODE
        )
        pd_apply_warnings(printdrop_win32_relay_client_tests)
        add_test(NAME printdrop_win32_relay_client_tests COMMAND printdrop_win32_relay_client_tests)
    endif()
endif()
