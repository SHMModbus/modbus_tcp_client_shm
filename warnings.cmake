#
# Copyright (C) 2022 Nikolas Koesling <nikolas@koesling.info>.
# This program is free software. You can redistribute it and/or modify it under the terms of the MIT License.
#

# warnings that are valid for gcc and clang
function(commonwarn target)
    target_compile_options(${target} PUBLIC -Wall -Wextra -pedantic -pedantic-errors)

    # see https://gcc.gnu.org/onlinedocs/gcc-4.3.2/gcc/Warning-Options.html for more details

    target_compile_options(${target} PUBLIC -Wnull-dereference)
    target_compile_options(${target} PUBLIC -Wold-style-cast)
    target_compile_options(${target} PUBLIC -Wdouble-promotion)
    target_compile_options(${target} PUBLIC -Wformat=2)
    target_compile_options(${target} PUBLIC -Winit-self)
    target_compile_options(${target} PUBLIC -Wsequence-point)
    target_compile_options(${target} PUBLIC -Wswitch-default)
    target_compile_options(${target} PUBLIC -Wswitch-enum -Wno-error=switch-enum)
    target_compile_options(${target} PUBLIC -Wconversion)
    target_compile_options(${target} PUBLIC -Wcast-align)
    target_compile_options(${target} PUBLIC -Wfloat-equal)
    target_compile_options(${target} PUBLIC -Wundef)
    target_compile_options(${target} PUBLIC -Wcast-qual)
endfunction()

# gcc specific warnings
function(gccwarn target)
    # see https://gcc.gnu.org/onlinedocs/gcc-4.3.2/gcc/Warning-Options.html for more details

    target_compile_options(${target} PUBLIC -Wduplicated-cond)
    target_compile_options(${target} PUBLIC -Wduplicated-branches)
    target_compile_options(${target} PUBLIC -Wlogical-op)
    target_compile_options(${target} PUBLIC -Wrestrict)
    target_compile_options(${target} PUBLIC -Wuseless-cast -Wno-error=useless-cast)
    target_compile_options(${target} PUBLIC -Wshadow=local -Wno-error=shadow)

    target_compile_options(${target} PUBLIC -Wno-error=switch-default)
    target_compile_options(${target} PUBLIC -Wno-error=attributes)
endfunction()

# clang specific warnings
function(clangwarn target)
    # enable all
    target_compile_options(${target} PUBLIC -Weverything)

    # and remove "useless" ones
    target_compile_options(${target} PUBLIC -Wno-c++98-compat)
    target_compile_options(${target} PUBLIC -Wno-c++98-c++11-c++14-compat)
    target_compile_options(${target} PUBLIC -Wno-c++98-compat-pedantic)
    target_compile_options(${target} PUBLIC -Wno-error=covered-switch-default)
    target_compile_options(${target} PUBLIC -Wno-shadow-field-in-constructor)
    target_compile_options(${target} PUBLIC -Wno-padded)
    target_compile_options(${target} PUBLIC -Wno-shadow-field)
    target_compile_options(${target} PUBLIC -Wno-weak-vtables)
    target_compile_options(${target} PUBLIC -Wno-exit-time-destructors)
    target_compile_options(${target} PUBLIC -Wno-global-constructors)
    target_compile_options(${target} PUBLIC -Wno-error=unreachable-code-return)
    target_compile_options(${target} PUBLIC -Wno-error=unreachable-code)
    target_compile_options(${target} PUBLIC -Wno-error=documentation)
    target_compile_options(${target} PUBLIC -Wno-error=unused-exception-parameter)
    target_compile_options(${target} PUBLIC -Wno-nested-anon-types)
    target_compile_options(${target} PUBLIC -Wno-gnu-anonymous-struct)
    target_compile_options(${target} PUBLIC -Wno-source-uses-openmp)

endfunction()

function(enable_warnings target)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
        commonwarn(${target})
        gccwarn(${target})
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        commonwarn(${target})
        clangwarn(${target})
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
        target_compile_options(${target} PUBLIC /Wall /WX)
    endif()
endfunction()

function(disable_warnings target)
    target_compile_options(${target} PUBLIC -w)
endfunction()
