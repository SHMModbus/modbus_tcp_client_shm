# CMake Project template

This repository is a template for CMake C++ Projects.

## Supported Compiilers

    - gcc
    - clang

## Project structure

### Directory src
Use this directory for all source files of the project.

### Directory libs
Place libraries here. This Directory is added to the include path.

## Scripts

### check_format.sh
This script checks all ```*.cpp``` and ```*.hpp``` files for conformity with the file ```.clang-format```.

### format.sh
This script formats all ```*.cpp``` and ```*.hpp``` files in the src directory with clang format.
The files are changed by it!

## Options

### Target
The name of the executable that is generated.

### STANDARD
The minimum required C++ standard: 98, 03, 11, 14, 17, 20

### ARCHITECTURE
The CPU architecture for which the code is generated.
It is only relevant if the option ```OPTIMIZE_FOR_ARCHITECTURE``` is enabled.

```native``` should be the best choice in most cases.

### BUILD_DOC
Enables the automatic generation of a doxygen documentation.
Doxygen must be installed on the system and a ```Doxyfile.in``` file must be provided.
An additional CMake target is created.

### COMPILER_WARNINGS
Enable/Disable compiler warnings.

### ENABLE_MULTITHREADING
Link the default multithreading library for the current target system.
Prefers ```pthread``` if available.

### MAKE_32_BIT_BINARY
Forces the compiler to generate a 32 bit application by setting the ```-m32``` flag.

### OPENMP
Enables the support for openmp.

### OPTIMIZE_DEBUG
Enables Optimization ```-O3``` also in debug configuration.
Should only be enabled if the resulting binary is too slow.

### CLANG_FORMAT
Enable automatic formatting via clang-format.
An additional CMake target is created.

### CLANG_TIDY
Enable static code checks with clang tidy.
An additional CMake target is created.

Not usable in this version due to contradictory warnings.

### LTO_ENABLED
Enable interprocedural and link time optimizations.

### COMPILER_EXTENSIONS
Enable compiler specific C++ extensions.
Should be disabled for reasons of portability.


