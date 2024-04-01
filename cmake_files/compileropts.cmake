#
# Copyright (C) 2022 Nikolas Koesling <nikolas@koesling.info>.
# This template is free software. You can redistribute it and/or modify it under the terms of the MIT License.
#

# options that are valid for gcc and clang
function(commonopts target)
    # more debugging information
    if(OPTIMIZE_DEBUG)
        SET(CMAKE_CXX_FLAGS_DEBUG "-g3 -O3")
    else()
        SET(CMAKE_CXX_FLAGS_DEBUG "-g3")
    endif()

    if(MAKE_32_BIT_BINARY)
        message(STATUS "Compiling as 32 bit binary.")
        target_compile_options(${target} PUBLIC -m32)
    endif()

    if(OPTIMIZE_FOR_ARCHITECTURE)
        message(STATUS "using architecture specific code generator: ${ARCHITECTURE}")
        target_compile_options(${target} PUBLIC -march=${ARCHITECTURE})
    endif()
endfunction()

function(set_options target use_omp)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
        commonopts(${target})
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        commonopts(${target})

        if(CMAKE_SYSTEM_NAME MATCHES "Windows")
            # TODO check options
            target_compile_options(${target} PUBLIC -D_DLL -D_MT -Xclang --dependent-lib=msvcrtd)
            SET(CMAKE_CXX_FLAGS_DEBUG "-g3 -D_DEBUG")
        endif()
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
        # more debugging information
        SET(CMAKE_CXX_FLAGS_DEBUG "/Zi")

        if(ENABLE_MULTITHREADING AND OPENMP)
            target_compile_options(${target} PUBLIC /OpenMP)
        endif()
    else()
        message(AUTHOR_WARNING
                "You are using a compiler other than gcc/clang. Only gcc/clang are fully supported by this template.")
    endif()

    if(use_omp)
        find_package(OpenMP)
        if (OPENMP_FOUND)
            set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${OpenMP_C_FLAGS}")
            set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OpenMP_CXX_FLAGS}")
            set (CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${OpenMP_EXE_LINKER_FLAGS}")
            target_link_libraries(${target} PRIVATE OpenMP::OpenMP_CXX)
            message(STATUS "openmp enabled")
        endif()
    endif()
endfunction()
