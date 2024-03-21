
# ---------------------------------------- subdirectories --------------------------------------------------------------
# ======================================================================================================================
include(../modbus.cmake)


# ---------------------------------------- fetch content ---------------------------------------------------------------
# ======================================================================================================================
include(FetchContent)

# cxxopts (jarro2783)
FetchContent_Declare(
        cxxopts
        GIT_REPOSITORY https://github.com/jarro2783/cxxopts.git
        GIT_TAG v3.2.0
        EXCLUDE_FROM_ALL
)

set(CXXOPTS_STANDALONE_PROJECT ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(cxxopts)

# cxxshm
FetchContent_Declare(
        cxxshm
        GIT_REPOSITORY https://github.com/NikolasK-source/cxxshm.git
        GIT_TAG v1.1.0
        EXCLUDE_FROM_ALL
)

FetchContent_MakeAvailable(cxxshm)

# cxxsemaphore
FetchContent_Declare(
        cxxsemaphore
        GIT_REPOSITORY https://github.com/NikolasK-source/cxxsemaphore.git
        GIT_TAG v1.0.1
        EXCLUDE_FROM_ALL
)

FetchContent_MakeAvailable(cxxsemaphore)

# ---------------------------------------- link libraries --------------------------------------------------------------
# ======================================================================================================================
target_link_libraries(${Target} PRIVATE modbus)
target_link_libraries(${Target} PRIVATE rt)
target_link_libraries(${Target} PRIVATE cxxopts)
target_link_libraries(${Target} PRIVATE cxxshm)
target_link_libraries(${Target} PRIVATE cxxsemaphore)
