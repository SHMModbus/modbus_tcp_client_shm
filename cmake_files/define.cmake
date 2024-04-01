#
# Copyright (C) 2022 Nikolas Koesling <nikolas@koesling.info>.
# This program is free software. You can redistribute it and/or modify it under the terms of the MIT License.
#

function(set_definitions target)
    # compiler
    if (CMAKE_CXX_COMPILER_ID MATCHES "GNU")
        # compiler specific defines
        target_compile_definitions(${target} PUBLIC "COMPILER_GNU")
        target_compile_definitions(${target} PUBLIC "COMPILER_GNU_CLANG")
    elseif (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        # compiler specific defines
        target_compile_definitions(${target} PUBLIC "COMPILER_CLANG")
        target_compile_definitions(${target} PUBLIC "COMPILER_GNU_CLANG")
    elseif (CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
        target_compile_definitions(${target} PUBLIC "COMPILER_MSVC")
        message(AUTHOR_WARNING
                "You are using a compiler other than gcc/clang. Only gcc/clang are fully supported by this template.")
    else ()
        target_compile_definitions(${target} PUBLIC "COMPILER_UNKNOWN")
        message(AUTHOR_WARNING
                "You are using a compiler other than gcc/clang. Only gcc/clang are fully supported by this template.")
    endif ()

    # project
    target_compile_definitions(${target} PUBLIC "PROJECT_VERSION=\"${PROJECT_VERSION}\"")
    target_compile_definitions(${target} PUBLIC "PROJECT_VERSION_MAJOR=${PROJECT_VERSION_MAJOR}")
    target_compile_definitions(${target} PUBLIC "PROJECT_VERSION_MINOR=${PROJECT_VERSION_MINOR}")
    target_compile_definitions(${target} PUBLIC "PROJECT_VERSION_PATCH=${PROJECT_VERSION_PATCH}")
    target_compile_definitions(${target} PUBLIC "PROJECT_NAME=\"${PROJECT_NAME}\"")
    target_compile_definitions(${target} PUBLIC "COMPILER_INFO=\"${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}\"")
    target_compile_definitions(${target} PUBLIC "SYSTEM_INFO=\"${CMAKE_SYSTEM_NAME} ${CMAKE_SYSTEM_VERSION} ${CMAKE_HOST_SYSTEM_PROCESSOR}\"")
    string(REPLACE "-" "_" project_name_rcs "${PROJECT_NAME}")
    string(REPLACE " " "_" project_name_rcs "${project_name_rcs}")
    target_compile_definitions(${target} PUBLIC "RCS_HASH=${project_name_rcs}\_version_info::GIT_HASH")

    # system
    if (CMAKE_SYSTEM_NAME MATCHES "Linux")
        target_compile_definitions(${target} PUBLIC "OS_LINUX")
        target_compile_definitions(${target} PUBLIC "OS_POSIX")
    elseif (CMAKE_SYSTEM_NAME MATCHES "FreeBSD")
        target_compile_definitions(${target} PUBLIC "OS_FREEBSD")
        target_compile_definitions(${target} PUBLIC "OS_POSIX")
    elseif (CMAKE_SYSTEM_NAME MATCHES "Windows")
        target_compile_definitions(${target} PUBLIC "OS_WINDOWS")
    elseif (CMAKE_SYSTEM_NAME MATCHES "Darwin")
        target_compile_definitions(${target} PUBLIC "OS_DARWIN")
        target_compile_definitions(${target} PUBLIC "OS_POSIX")
    endif ()

    # architecture defines
    target_compile_definitions(${target} PUBLIC CPU_WORD_BYTES=${CMAKE_SIZEOF_VOID_P})
endfunction()
