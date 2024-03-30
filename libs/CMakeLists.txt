
find_package(cxxshm)

# ---------------------------------------- subdirectories --------------------------------------------------------------
# ======================================================================================================================
include(../modbus.cmake)
target_link_libraries(${Target} PRIVATE modbus)

# ---------------------------------------- link libraries --------------------------------------------------------------
# ======================================================================================================================
target_link_libraries(${Target} PRIVATE rt)

add_subdirectory(cxxopts EXCLUDE_FROM_ALL)
target_link_libraries(${Target} PRIVATE cxxopts)

target_link_libraries(${Target} PRIVATE cxxshm)

add_subdirectory(cxxsemaphore EXCLUDE_FROM_ALL)
target_link_libraries(${Target} PRIVATE cxxsemaphore)
