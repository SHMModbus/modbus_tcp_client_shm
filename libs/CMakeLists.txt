
# ---------------------------------------- subdirectories --------------------------------------------------------------
# ======================================================================================================================
include(../modbus.cmake)
target_link_libraries(${Target} PRIVATE modbus)

# ---------------------------------------- link libraries --------------------------------------------------------------
# ======================================================================================================================
target_link_libraries(${Target} PRIVATE rt)

add_subdirectory(cxxopts EXCLUDE_FROM_ALL)
target_link_libraries(${Target} PRIVATE cxxopts)

add_subdirectory(cxxshm EXCLUDE_FROM_ALL)
target_link_libraries(${Target} PRIVATE cxxshm)
