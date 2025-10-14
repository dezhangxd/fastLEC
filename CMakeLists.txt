cmake_minimum_required(VERSION 3.16)

# Set CUDA option before project declaration
option(USE_CUDA "Enable CUDA support" ON)

# Conditionally include CUDA language
if(USE_CUDA)
    project(fastLEC VERSION 0.1.0 LANGUAGES C CXX CUDA)
    # Set CUDA policy to suppress warnings
    cmake_policy(SET CMP0104 NEW)
else()
    project(fastLEC VERSION 0.1.0 LANGUAGES C CXX)
endif()

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Set CUDA standard (only if CUDA is enabled)
if(USE_CUDA)
    set(CMAKE_CUDA_STANDARD 17)
    set(CMAKE_CUDA_STANDARD_REQUIRED ON)
endif()

# Set build type if not specified
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Compiler flags
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra")

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -O0")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -O0")
    if(USE_CUDA)
        set(CMAKE_CUDA_FLAGS "${CMAKE_CUDA_FLAGS} -g -O0")
    endif()
else()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O3")
    if(USE_CUDA)
        set(CMAKE_CUDA_FLAGS "${CMAKE_CUDA_FLAGS} -O3")
    endif()
endif()

# Find required packages
find_package(Threads REQUIRED)

# ========================================
# CUDA Configuration
# ========================================
if(USE_CUDA)
    # Try modern CUDA toolkit first
    find_package(CUDAToolkit QUIET)
    if(CUDAToolkit_FOUND)
        message(STATUS "CUDAToolkit found: ${CUDAToolkit_VERSION}")
        set(CUDA_ENABLED TRUE)
        # Find CUDA libraries with proper paths
        find_library(CUDA_LIBRARY cuda PATHS ${CUDAToolkit_LIBRARY_DIRS} NO_DEFAULT_PATH)
        find_library(CUDART_LIBRARY cudart PATHS ${CUDAToolkit_LIBRARY_DIRS} NO_DEFAULT_PATH)
        if(CUDA_LIBRARY AND CUDART_LIBRARY)
            set(CUDA_LIBRARIES ${CUDA_LIBRARY} ${CUDART_LIBRARY})
            message(STATUS "CUDA libraries found: ${CUDA_LIBRARIES}")
        else()
            # Fallback to library names
            set(CUDA_LIBRARIES cuda cudart)
            message(STATUS "Using fallback CUDA library names")
        endif()
        set(CUDA_INCLUDE_DIRS ${CUDAToolkit_INCLUDE_DIRS})
    else()
        # Fallback to legacy CUDA finder
        find_package(CUDA QUIET)
        if(CUDA_FOUND)
            message(STATUS "CUDA found: ${CUDA_VERSION}")
            message(STATUS "CUDA include directories: ${CUDA_INCLUDE_DIRS}")
            message(STATUS "CUDA libraries: ${CUDA_LIBRARIES}")
            set(CUDA_ENABLED TRUE)
            # Ensure CUDA libraries are properly set
            if(NOT CUDA_LIBRARIES)
                set(CUDA_LIBRARIES cuda cudart)
            endif()
        else()
            message(WARNING "CUDA not found, disabling CUDA support")
            set(CUDA_ENABLED FALSE)
        endif()
    endif()
    
    if(CUDA_ENABLED)
        # Set CUDA architectures (only newer ones to avoid deprecation warnings)
        set(CMAKE_CUDA_ARCHITECTURES 75 80 86)
        
        # Print CUDA environment information for debugging
        message(STATUS "CUDA_HOME: $ENV{CUDA_HOME}")
        message(STATUS "CUDA_PATH: $ENV{CUDA_PATH}")
        message(STATUS "CUDAToolkit_LIBRARY_DIRS: ${CUDAToolkit_LIBRARY_DIRS}")
        message(STATUS "CUDAToolkit_INCLUDE_DIRS: ${CUDAToolkit_INCLUDE_DIRS}")
        
        # Set CUDA specific flags
        set(CMAKE_CUDA_FLAGS "${CMAKE_CUDA_FLAGS} --expt-relaxed-constexpr")
        set(CMAKE_CUDA_FLAGS "${CMAKE_CUDA_FLAGS} --expt-extended-lambda")
        
        # Enable separable compilation
        set(CMAKE_CUDA_SEPARABLE_COMPILATION ON)
        
        # Additional CUDA library search paths
        if(CUDAToolkit_FOUND)
            list(APPEND CMAKE_LIBRARY_PATH ${CUDAToolkit_LIBRARY_DIRS})
        endif()
        
        # Platform-specific CUDA library paths
        if(APPLE)
            # macOS CUDA paths
            list(APPEND CMAKE_LIBRARY_PATH "/usr/local/cuda/lib")
            list(APPEND CMAKE_LIBRARY_PATH "/opt/cuda/lib")
        else()
            # Linux CUDA paths
            list(APPEND CMAKE_LIBRARY_PATH "/usr/local/cuda-12.4/targets/x86_64-linux/lib")
            list(APPEND CMAKE_LIBRARY_PATH "/usr/lib/x86_64-linux-gnu")
            list(APPEND CMAKE_LIBRARY_PATH "/usr/local/cuda-12.4/targets/x86_64-linux/lib")
        endif()
        
        # Try to find CUDA libraries in common locations
        if(APPLE)
            # macOS CUDA library paths
            find_library(CUDA_LIBRARY cuda 
                PATHS 
                /usr/local/cuda/lib
                /opt/cuda/lib
                ${CUDAToolkit_LIBRARY_DIRS}
                NO_DEFAULT_PATH
            )
            find_library(CUDART_LIBRARY cudart
                PATHS 
                /usr/local/cuda/lib
                /opt/cuda/lib
                ${CUDAToolkit_LIBRARY_DIRS}
                NO_DEFAULT_PATH
            )
        else()
            # Linux CUDA library paths
            find_library(CUDA_LIBRARY cuda 
                PATHS 
                /usr/lib/x86_64-linux-gnu
                /usr/local/cuda-12.4/targets/x86_64-linux/lib
                /usr/local/cuda/lib64
                /usr/local/cuda/lib
                ${CUDAToolkit_LIBRARY_DIRS}
                /opt/cuda/lib64
                /opt/cuda/lib
                NO_DEFAULT_PATH
            )
            find_library(CUDART_LIBRARY cudart
                PATHS 
                /usr/local/cuda-12.4/targets/x86_64-linux/lib
                /usr/lib/x86_64-linux-gnu
                /usr/local/cuda/lib64
                /usr/local/cuda/lib
                ${CUDAToolkit_LIBRARY_DIRS}
                /opt/cuda/lib64
                /opt/cuda/lib
                NO_DEFAULT_PATH
            )
        endif()
        
        if(CUDA_LIBRARY AND CUDART_LIBRARY)
            set(CUDA_LIBRARIES ${CUDA_LIBRARY} ${CUDART_LIBRARY})
            message(STATUS "Found CUDA libraries: ${CUDA_LIBRARIES}")
        else()
            # If libraries not found, try to use pkg-config or system paths
            message(STATUS "CUDA libraries not found in expected locations, trying system paths...")
            find_library(CUDA_LIBRARY cuda)
            find_library(CUDART_LIBRARY cudart)
            if(CUDA_LIBRARY AND CUDART_LIBRARY)
                set(CUDA_LIBRARIES ${CUDA_LIBRARY} ${CUDART_LIBRARY})
                message(STATUS "Found CUDA libraries in system paths: ${CUDA_LIBRARIES}")
            else()
                message(WARNING "CUDA libraries not found anywhere, using library names")
                set(CUDA_LIBRARIES cuda cudart)
            endif()
        endif()
    endif()
else()
    set(CUDA_ENABLED FALSE)
    message(STATUS "CUDA support disabled")
endif()

# If CUDA is not available, disable CUDA language requirement
if(NOT CUDA_ENABLED)
    # Remove CUDA from project languages if not available
    set(CMAKE_PROJECT_LANGUAGES C CXX)
    message(STATUS "CUDA language removed from project requirements")
endif()

# ========================================
# Submodule Configuration
# ========================================

# XGBoost submodule
option(USE_XGBOOST "Enable XGBoost support" OFF)
if(USE_XGBOOST AND EXISTS "${CMAKE_SOURCE_DIR}/deps/xgboost/CMakeLists.txt")
    # Set XGBoost build options to avoid ABI issues
    set(XGBOOST_USE_CUDA OFF CACHE BOOL "Build XGBoost with CUDA support" FORCE)
    set(XGBOOST_USE_OPENMP ON CACHE BOOL "Build XGBoost with OpenMP support" FORCE)
    set(XGBOOST_USE_NCCL OFF CACHE BOOL "Build XGBoost with NCCL support" FORCE)
    set(XGBOOST_USE_HIP OFF CACHE BOOL "Build XGBoost with HIP support" FORCE)
    set(XGBOOST_USE_SYCL OFF CACHE BOOL "Build XGBoost with SYCL support" FORCE)
    set(XGBOOST_USE_MPI OFF CACHE BOOL "Build XGBoost with MPI support" FORCE)
    set(XGBOOST_USE_FEDERATED OFF CACHE BOOL "Build XGBoost with Federated Learning support" FORCE)
    
    # Ensure consistent C++ standard
    set(CMAKE_CXX_STANDARD 17 CACHE STRING "C++ standard" FORCE)
    set(CMAKE_CXX_STANDARD_REQUIRED ON CACHE BOOL "C++ standard required" FORCE)
    
    add_subdirectory(deps/xgboost)
    set(XGBOOST_FOUND TRUE)
    message(STATUS "XGBoost submodule found and enabled")
else()
    set(XGBOOST_FOUND FALSE)
    if(USE_XGBOOST)
        message(WARNING "XGBoost submodule not found. Run: git submodule update --init --recursive")
    else()
        message(STATUS "XGBoost support disabled")
    endif()
endif()

# AIGER submodule
if(EXISTS "${CMAKE_SOURCE_DIR}/deps/aiger/aiger.h")
    set(AIGER_FOUND TRUE)
    set(AIGER_INCLUDE_DIRS "${CMAKE_SOURCE_DIR}/deps/aiger")
    message(STATUS "AIGER submodule found")
    
    # Check for problematic files that might cause compilation errors
    # These files contain non-C++ content but might be accidentally included
    if(EXISTS "${CMAKE_SOURCE_DIR}/deps/aiger/format")
        message(STATUS "Renaming AIGER format file to prevent compilation conflicts")
        execute_process(
            COMMAND ${CMAKE_COMMAND} -E rename 
            "${CMAKE_SOURCE_DIR}/deps/aiger/format" 
            "${CMAKE_SOURCE_DIR}/deps/aiger/format.bak"
            RESULT_VARIABLE RENAME_RESULT
        )
        if(RENAME_RESULT EQUAL 0)
            message(STATUS "  - format -> format.bak")
        else()
            message(WARNING "Failed to rename format file")
        endif()
    endif()
    
    if(EXISTS "${CMAKE_SOURCE_DIR}/deps/aiger/VERSION")
        message(STATUS "Renaming AIGER VERSION file to prevent compilation conflicts")
        execute_process(
            COMMAND ${CMAKE_COMMAND} -E rename 
            "${CMAKE_SOURCE_DIR}/deps/aiger/VERSION" 
            "${CMAKE_SOURCE_DIR}/deps/aiger/VERSION.bak"
            RESULT_VARIABLE RENAME_RESULT
        )
        if(RENAME_RESULT EQUAL 0)
            message(STATUS "  - VERSION -> VERSION.bak")
        else()
            message(WARNING "Failed to rename VERSION file")
        endif()
    endif()
    
    # Check for lowercase version file as well
    if(EXISTS "${CMAKE_SOURCE_DIR}/deps/aiger/version")
        message(STATUS "Renaming AIGER version file to prevent compilation conflicts")
        execute_process(
            COMMAND ${CMAKE_COMMAND} -E rename 
            "${CMAKE_SOURCE_DIR}/deps/aiger/version" 
            "${CMAKE_SOURCE_DIR}/deps/aiger/version.bak"
            RESULT_VARIABLE RENAME_RESULT
        )
        if(RENAME_RESULT EQUAL 0)
            message(STATUS "  - version -> version.bak")
        else()
            message(WARNING "Failed to rename version file")
        endif()
    endif()
    
else()
    message(WARNING "AIGER submodule not found. Run: git submodule update --init --recursive")
    set(AIGER_FOUND FALSE)
endif()

# Kissat submodule
if(EXISTS "${CMAKE_SOURCE_DIR}/deps/kissat/src/kissat.h")
    set(KISSAT_FOUND TRUE)
    set(KISSAT_INCLUDE_DIRS "${CMAKE_SOURCE_DIR}/deps/kissat/src")
    message(STATUS "Kissat submodule found")
else()
    message(WARNING "Kissat submodule not found. Run: git submodule update --init --recursive")
    set(KISSAT_FOUND FALSE)
endif()

# CUDD submodule
option(USE_CUDD "Enable CUDD support" ON)
if(USE_CUDD AND EXISTS "${CMAKE_SOURCE_DIR}/deps/cudd/configure.ac")
    set(CUDD_FOUND TRUE)
    set(CUDD_INCLUDE_DIRS 
        "${CMAKE_SOURCE_DIR}/deps/cudd"
        "${CMAKE_SOURCE_DIR}/deps/cudd/cudd"
        "${CMAKE_SOURCE_DIR}/deps/cudd/mtr"
        "${CMAKE_SOURCE_DIR}/deps/cudd/st"
        "${CMAKE_SOURCE_DIR}/deps/cudd/util"
        "${CMAKE_SOURCE_DIR}/deps/cudd/epd"
        "${CMAKE_SOURCE_DIR}/deps/cudd/dddmp"
        "${CMAKE_SOURCE_DIR}/deps/cudd/cplusplus"
    )
    message(STATUS "CUDD submodule found and enabled")
else()
    set(CUDD_FOUND FALSE)
    if(USE_CUDD)
        message(WARNING "CUDD submodule not found. Run: git submodule update --init --recursive")
    else()
        message(STATUS "CUDD support disabled")
    endif()
endif()

# Sylvan submodule
option(USE_SYLVAN "Enable Sylvan support" ON)
if(USE_SYLVAN AND EXISTS "${CMAKE_SOURCE_DIR}/deps/sylvan/CMakeLists.txt")
    set(SYLVAN_FOUND TRUE)
    message(STATUS "Sylvan submodule found and enabled")
else()
    set(SYLVAN_FOUND FALSE)
    if(USE_SYLVAN)
        message(WARNING "Sylvan submodule not found. Run: git submodule update --init --recursive")
    else()
        message(STATUS "Sylvan support disabled")
    endif()
endif()

# ========================================
# AIGER Library
# ========================================
if(AIGER_FOUND)
    add_library(aiger STATIC
        deps/aiger/aiger.c
    )
    
    target_include_directories(aiger PUBLIC ${AIGER_INCLUDE_DIRS})
    target_compile_definitions(aiger PUBLIC NDEBUG)
    
    # Suppress warnings for AIGER library
    target_compile_options(aiger PRIVATE -w)
    
    # AIGER tools (optional)
    option(BUILD_AIGER_TOOLS "Build AIGER tools" OFF)
    if(BUILD_AIGER_TOOLS)
        # Add AIGER tools here if needed
        message(STATUS "AIGER tools build disabled by default")
    endif()
endif()

# ========================================
# Kissat Library
# ========================================
if(KISSAT_FOUND)
    # Check if kissat has been built
    set(KISSAT_BUILD_DIR "${CMAKE_SOURCE_DIR}/deps/kissat/build")
    set(KISSAT_LIB_PATH "${KISSAT_BUILD_DIR}/libkissat.a")
    
    if(EXISTS ${KISSAT_LIB_PATH})
        # Use the pre-built kissat library
        add_library(kissat_lib STATIC IMPORTED)
        set_target_properties(kissat_lib PROPERTIES
            IMPORTED_LOCATION ${KISSAT_LIB_PATH}
            INTERFACE_INCLUDE_DIRECTORIES ${KISSAT_INCLUDE_DIRS}
        )
        message(STATUS "Using pre-built kissat library: ${KISSAT_LIB_PATH}")
    else()
        message(STATUS "Kissat library not found, building it now...")
        
        # Configure kissat
        execute_process(
            COMMAND ${CMAKE_SOURCE_DIR}/deps/kissat/configure --quiet
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/deps/kissat
            RESULT_VARIABLE CONFIGURE_RESULT
            OUTPUT_VARIABLE CONFIGURE_OUTPUT
            ERROR_VARIABLE CONFIGURE_ERROR
        )
        
        if(NOT CONFIGURE_RESULT EQUAL 0)
            message(FATAL_ERROR "Failed to configure kissat: ${CONFIGURE_ERROR}")
        endif()
        
        # Build kissat
        execute_process(
            COMMAND make -j${CMAKE_BUILD_PARALLEL_LEVEL}
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/deps/kissat
            RESULT_VARIABLE BUILD_RESULT
            OUTPUT_VARIABLE BUILD_OUTPUT
            ERROR_VARIABLE BUILD_ERROR
        )
        
        if(NOT BUILD_RESULT EQUAL 0)
            message(FATAL_ERROR "Failed to build kissat: ${BUILD_ERROR}")
        endif()
        
        # Check if the library was created
        if(EXISTS ${KISSAT_LIB_PATH})
            add_library(kissat_lib STATIC IMPORTED)
            set_target_properties(kissat_lib PROPERTIES
                IMPORTED_LOCATION ${KISSAT_LIB_PATH}
                INTERFACE_INCLUDE_DIRECTORIES ${KISSAT_INCLUDE_DIRS}
            )
            message(STATUS "Successfully built and linked kissat library: ${KISSAT_LIB_PATH}")
        else()
            message(FATAL_ERROR "Kissat library was not created after build")
        endif()
    endif()
endif()

# ========================================
# CUDD Library
# ========================================
if(CUDD_FOUND)
    # Check if CUDD has been built
    set(CUDD_BUILD_DIR "${CMAKE_SOURCE_DIR}/deps/cudd")
    set(CUDD_LIB_PATH "${CUDD_BUILD_DIR}/cudd/.libs/libcudd.a")
    
    if(EXISTS ${CUDD_LIB_PATH})
        # Use the pre-built CUDD library
        add_library(cudd_lib STATIC IMPORTED)
        set_target_properties(cudd_lib PROPERTIES
            IMPORTED_LOCATION ${CUDD_LIB_PATH}
        )
        target_include_directories(cudd_lib INTERFACE ${CUDD_INCLUDE_DIRS})
        message(STATUS "Using pre-built CUDD library: ${CUDD_LIB_PATH}")
    else()
        message(STATUS "CUDD library not found, building it now...")
        
        # Generate build files with autoreconf
        # For macOS, use the correct autoreconf command
        if(APPLE)
            execute_process(
                COMMAND autoreconf -f -i
                WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/deps/cudd
                RESULT_VARIABLE AUTORECONF_RESULT
                OUTPUT_VARIABLE AUTORECONF_OUTPUT
                ERROR_VARIABLE AUTORECONF_ERROR
            )
        else()
            set(ENV{PATH} "/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin")
            execute_process(
                COMMAND autoreconf -i
                WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/deps/cudd
                RESULT_VARIABLE AUTORECONF_RESULT
                OUTPUT_VARIABLE AUTORECONF_OUTPUT
                ERROR_VARIABLE AUTORECONF_ERROR
            )
        endif()
        
        if(NOT AUTORECONF_RESULT EQUAL 0)
            message(FATAL_ERROR "Failed to run autoreconf for CUDD: ${AUTORECONF_ERROR}")
        endif()
        
        # Configure CUDD
        if(APPLE)
            # For macOS, use simpler configure options
            execute_process(
                COMMAND ${CMAKE_SOURCE_DIR}/deps/cudd/configure --quiet
                WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/deps/cudd
                RESULT_VARIABLE CONFIGURE_RESULT
                OUTPUT_VARIABLE CONFIGURE_OUTPUT
                ERROR_VARIABLE CONFIGURE_ERROR
            )
        else()
            set(ENV{PATH} "/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin")
            execute_process(
                COMMAND ${CMAKE_SOURCE_DIR}/deps/cudd/configure --quiet --enable-silent-rules --enable-shared --enable-obj
                WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/deps/cudd
                RESULT_VARIABLE CONFIGURE_RESULT
                OUTPUT_VARIABLE CONFIGURE_OUTPUT
                ERROR_VARIABLE CONFIGURE_ERROR
            )
        endif()
        
        if(NOT CONFIGURE_RESULT EQUAL 0)
            message(FATAL_ERROR "Failed to configure CUDD: ${CONFIGURE_ERROR}")
        endif()
        
        # Build CUDD
        if(APPLE)
            # For macOS, don't set PATH environment
            execute_process(
                COMMAND make -j${CMAKE_BUILD_PARALLEL_LEVEL}
                WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/deps/cudd
                RESULT_VARIABLE BUILD_RESULT
                OUTPUT_VARIABLE BUILD_OUTPUT
                ERROR_VARIABLE BUILD_ERROR
            )
        else()
            set(ENV{PATH} "/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin")
            execute_process(
                COMMAND make -j${CMAKE_BUILD_PARALLEL_LEVEL}
                WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/deps/cudd
                RESULT_VARIABLE BUILD_RESULT
                OUTPUT_VARIABLE BUILD_OUTPUT
                ERROR_VARIABLE BUILD_ERROR
            )
        endif()
        
        if(NOT BUILD_RESULT EQUAL 0)
            message(FATAL_ERROR "Failed to build CUDD: ${BUILD_ERROR}")
        endif()
        
        # Check if the library was created
        if(EXISTS ${CUDD_LIB_PATH})
            add_library(cudd_lib STATIC IMPORTED)
            set_target_properties(cudd_lib PROPERTIES
                IMPORTED_LOCATION ${CUDD_LIB_PATH}
            )
            target_include_directories(cudd_lib INTERFACE ${CUDD_INCLUDE_DIRS})
            message(STATUS "Successfully built and linked CUDD library: ${CUDD_LIB_PATH}")
        else()
            message(FATAL_ERROR "CUDD library was not created after build")
        endif()
    endif()
endif()

# ========================================
# Sylvan Library
# ========================================
if(SYLVAN_FOUND)
    # Check if Sylvan has been built
    set(SYLVAN_BUILD_DIR "${CMAKE_SOURCE_DIR}/deps/sylvan/build")
    set(SYLVAN_LIB_PATH "${SYLVAN_BUILD_DIR}/src/lib/libsylvan.a")
    
    if(EXISTS ${SYLVAN_LIB_PATH})
        # Use the pre-built Sylvan library
        add_library(sylvan_lib STATIC IMPORTED)
        set_target_properties(sylvan_lib PROPERTIES
            IMPORTED_LOCATION ${SYLVAN_LIB_PATH}
        )
        target_include_directories(sylvan_lib INTERFACE 
            "${CMAKE_SOURCE_DIR}/deps/sylvan/src"
            "${SYLVAN_BUILD_DIR}/_deps/lace-src/src"
            "${SYLVAN_BUILD_DIR}/_deps/lace-build"
        )
        message(STATUS "Using pre-built Sylvan library: ${SYLVAN_LIB_PATH}")
    else()
        message(STATUS "Sylvan library not found, building it now...")
        
        # Create build directory
        file(MAKE_DIRECTORY ${SYLVAN_BUILD_DIR})
        
        # Configure Sylvan
        execute_process(
            COMMAND ${CMAKE_COMMAND} -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE} -DSYLVAN_BUILD_TESTS=OFF -DSYLVAN_BUILD_EXAMPLES=OFF -DSYLVAN_BUILD_DOCS=OFF -DSYLVAN_STATS=ON ..
            WORKING_DIRECTORY ${SYLVAN_BUILD_DIR}
            RESULT_VARIABLE CONFIGURE_RESULT
            OUTPUT_VARIABLE CONFIGURE_OUTPUT
            ERROR_VARIABLE CONFIGURE_ERROR
        )
        
        if(NOT CONFIGURE_RESULT EQUAL 0)
            message(FATAL_ERROR "Failed to configure Sylvan: ${CONFIGURE_ERROR}")
        endif()
        
        # Build Sylvan
        execute_process(
            COMMAND ${CMAKE_COMMAND} --build . --parallel ${CMAKE_BUILD_PARALLEL_LEVEL}
            WORKING_DIRECTORY ${SYLVAN_BUILD_DIR}
            RESULT_VARIABLE BUILD_RESULT
            OUTPUT_VARIABLE BUILD_OUTPUT
            ERROR_VARIABLE BUILD_ERROR
        )
        
        if(NOT BUILD_RESULT EQUAL 0)
            message(FATAL_ERROR "Failed to build Sylvan: ${BUILD_ERROR}")
        endif()
        
        # Check if the library was created
        if(EXISTS ${SYLVAN_LIB_PATH})
            add_library(sylvan_lib STATIC IMPORTED)
            set_target_properties(sylvan_lib PROPERTIES
                IMPORTED_LOCATION ${SYLVAN_LIB_PATH}
            )
            target_include_directories(sylvan_lib INTERFACE 
                "${CMAKE_SOURCE_DIR}/deps/sylvan/src"
                "${SYLVAN_BUILD_DIR}/_deps/lace-src/src"
                "${SYLVAN_BUILD_DIR}/_deps/lace-build"
            )
            message(STATUS "Successfully built and linked Sylvan library: ${SYLVAN_LIB_PATH}")
        else()
            message(FATAL_ERROR "Sylvan library was not created after build")
        endif()
    endif()
endif()

# ========================================
# Main Application
# ========================================

# Create include directory if it doesn't exist
if(NOT EXISTS "${CMAKE_SOURCE_DIR}/include")
    file(MAKE_DIRECTORY "${CMAKE_SOURCE_DIR}/include")
endif()

# Source files - explicitly list them for better control
set(SOURCES
    src/main.cpp
    src/fastLEC.cpp
    src/basic.cpp
    src/pSAT.cpp
    src/AIG.cpp
    src/XAG.cpp
    src/CNF.cpp
    src/seqSAT.cpp
    src/cudd.cpp
    src/sylvan.cpp
    src/simu_seq.cpp
    src/simu_para.cpp
    src/sweeper.cpp
    src/pSAT_heuristics.cpp
    src/pSAT_task.cpp
)

# CUDA source files
if(CUDA_ENABLED)
    set(CUDA_SOURCES
        src/gES.cu
    )
    list(APPEND SOURCES ${CUDA_SOURCES})
else()
    # Create a dummy CUDA source file when CUDA is not available
    set(DUMMY_CUDA_SOURCE "${CMAKE_CURRENT_BINARY_DIR}/dummy_gES.cpp")
    configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake/dummy_gES.cpp.in"
        "${DUMMY_CUDA_SOURCE}"
        @ONLY
    )
    list(APPEND SOURCES "${DUMMY_CUDA_SOURCE}")
    message(STATUS "Using dummy CUDA source file: ${DUMMY_CUDA_SOURCE}")
endif()

# Header files
set(HEADERS
    src/fastLEC.hpp
    src/basic.hpp
    src/AIG.hpp
    src/XAG.hpp
    src/CNF.hpp
    src/parser.hpp
    src/gES.h
)

# Check if source files exist and add them to the list
set(EXISTING_SOURCES "")
foreach(SOURCE ${SOURCES})
    if(EXISTS "${CMAKE_SOURCE_DIR}/${SOURCE}")
        list(APPEND EXISTING_SOURCES ${SOURCE})
    elseif(EXISTS "${SOURCE}")
        # For generated files like dummy_gES.cpp
        list(APPEND EXISTING_SOURCES ${SOURCE})
    endif()
endforeach()

if(EXISTING_SOURCES)
    add_executable(fastLEC ${EXISTING_SOURCES} ${HEADERS})
    
    # Include directories
    target_include_directories(fastLEC PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/include
    )
    
    # Compiler definitions
    target_compile_definitions(fastLEC PRIVATE
        $<$<CONFIG:Debug>:DEBUG>
        $<$<CONFIG:Release>:NDEBUG>
    )
    
    # Link libraries
    target_link_libraries(fastLEC PRIVATE Threads::Threads)
    
    # Link CUDA libraries
    if(CUDA_ENABLED)
        target_link_libraries(fastLEC PRIVATE ${CUDA_LIBRARIES})
        target_include_directories(fastLEC PRIVATE ${CUDA_INCLUDE_DIRS})
        
        # Add CUDA library directories to linker
        if(CUDAToolkit_FOUND AND CUDAToolkit_LIBRARY_DIRS)
            target_link_directories(fastLEC PRIVATE ${CUDAToolkit_LIBRARY_DIRS})
            message(STATUS "Added CUDA library directories: ${CUDAToolkit_LIBRARY_DIRS}")
        endif()
        
        # Add runtime library path
        if(CUDAToolkit_FOUND AND CUDAToolkit_LIBRARY_DIRS)
            set_target_properties(fastLEC PROPERTIES
                BUILD_WITH_INSTALL_RPATH TRUE
                INSTALL_RPATH "${CUDAToolkit_LIBRARY_DIRS}"
            )
        endif()
        
        set_target_properties(fastLEC PROPERTIES
            CUDA_SEPARABLE_COMPILATION ON
        )
    endif()
    
    if(AIGER_FOUND)
        target_link_libraries(fastLEC PRIVATE aiger)
        target_include_directories(fastLEC PRIVATE ${AIGER_INCLUDE_DIRS})
    endif()
    
    if(KISSAT_FOUND)
        target_link_libraries(fastLEC PRIVATE kissat_lib)
        target_include_directories(fastLEC PRIVATE ${KISSAT_INCLUDE_DIRS})
    endif()
    
    if(CUDD_FOUND)
        target_link_libraries(fastLEC PRIVATE cudd_lib)
        target_include_directories(fastLEC PRIVATE ${CUDD_INCLUDE_DIRS})
    endif()
    
    if(SYLVAN_FOUND)
        target_link_libraries(fastLEC PRIVATE sylvan_lib)
        # Link lace library for Sylvan
        target_link_libraries(fastLEC PRIVATE "${SYLVAN_BUILD_DIR}/_deps/lace-build/lib/liblace.a")
    endif()
    
    if(XGBOOST_FOUND)
        # XGBoost linking with proper error handling
        find_library(XGBOOST_LIBRARY xgboost PATHS ${CMAKE_SOURCE_DIR}/deps/xgboost/lib NO_DEFAULT_PATH)
        if(XGBOOST_LIBRARY)
            target_link_libraries(fastLEC PRIVATE ${XGBOOST_LIBRARY})
            message(STATUS "Linked XGBoost library: ${XGBOOST_LIBRARY}")
        else()
            message(WARNING "XGBoost library not found, skipping XGBoost linking")
        endif()
    endif()
    
    # Set properties
    set_target_properties(fastLEC PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
    )
    
    # Set CUDA properties for the target
    if(CUDA_ENABLED)
        set_target_properties(fastLEC PROPERTIES
            CUDA_ARCHITECTURES 75 80 86
        )
    endif()
    
    message(STATUS "Main executable 'fastLEC' configured with ${EXISTING_SOURCES}")
else()
    message(STATUS "No source files found in src/ directory")
endif()

# ========================================
# Library Target (optional)
# ========================================
option(BUILD_LIBRARY "Build fastLEC as a library" OFF)
if(BUILD_LIBRARY AND EXISTING_SOURCES)
    add_library(fastLEC_lib STATIC ${EXISTING_SOURCES} ${HEADERS})
    
    target_include_directories(fastLEC_lib PUBLIC
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/include
    )
    
    target_link_libraries(fastLEC_lib PUBLIC Threads::Threads)
    
    # Link CUDA libraries for library target
    if(CUDA_ENABLED)
        target_link_libraries(fastLEC_lib PUBLIC ${CUDA_LIBRARIES})
        target_include_directories(fastLEC_lib PUBLIC ${CUDA_INCLUDE_DIRS})
        
        # Add CUDA library directories to linker
        if(CUDAToolkit_FOUND AND CUDAToolkit_LIBRARY_DIRS)
            target_link_directories(fastLEC_lib PUBLIC ${CUDAToolkit_LIBRARY_DIRS})
        endif()
        
        set_target_properties(fastLEC_lib PROPERTIES
            CUDA_SEPARABLE_COMPILATION ON
        )
    endif()
    
    if(AIGER_FOUND)
        target_link_libraries(fastLEC_lib PUBLIC aiger)
    endif()
    
    if(KISSAT_FOUND)
        target_link_libraries(fastLEC_lib PUBLIC kissat_lib)
    endif()
    
    if(CUDD_FOUND)
        target_link_libraries(fastLEC_lib PUBLIC cudd_lib)
    endif()
    
    if(SYLVAN_FOUND)
        target_link_libraries(fastLEC_lib PUBLIC sylvan_lib)
        # Link lace library for Sylvan
        target_link_libraries(fastLEC_lib PUBLIC "${SYLVAN_BUILD_DIR}/_deps/lace-build/lib/liblace.a")
    endif()
    
    if(XGBOOST_FOUND)
        find_library(XGBOOST_LIBRARY xgboost PATHS ${CMAKE_SOURCE_DIR}/deps/xgboost/lib NO_DEFAULT_PATH)
        if(XGBOOST_LIBRARY)
            target_link_libraries(fastLEC_lib PUBLIC ${XGBOOST_LIBRARY})
        endif()
    endif()
    
    set_target_properties(fastLEC_lib PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
    )
    
    # Set CUDA properties for the library target
    if(CUDA_ENABLED)
        set_target_properties(fastLEC_lib PROPERTIES
            CUDA_ARCHITECTURES 75 80 86
        )
    endif()
    
    message(STATUS "Library target 'fastLEC_lib' configured")
endif()

# ========================================
# Testing
# ========================================
option(BUILD_TESTS "Build tests" OFF)
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# ========================================
# Installation
# ========================================
install(TARGETS fastLEC
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)

# Install headers if they exist
if(EXISTS "${CMAKE_SOURCE_DIR}/include")
    install(DIRECTORY include/ DESTINATION include)
endif()

# ========================================
# Package Configuration
# ========================================
include(CMakePackageConfigHelpers)
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/fastLECConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY AnyNewerVersion
)

# ========================================
# Print Configuration Summary
# ========================================
message(STATUS "")
message(STATUS "=== fastLEC Configuration Summary ===")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ standard: ${CMAKE_CXX_STANDARD}")
if(CUDA_ENABLED)
    message(STATUS "CUDA standard: ${CMAKE_CUDA_STANDARD}")
endif()
message(STATUS "Use XGBoost: ${USE_XGBOOST}")
message(STATUS "XGBoost: ${XGBOOST_FOUND}")
message(STATUS "Use CUDD: ${USE_CUDD}")
message(STATUS "CUDD: ${CUDD_FOUND}")
message(STATUS "Use Sylvan: ${USE_SYLVAN}")
message(STATUS "Sylvan: ${SYLVAN_FOUND}")
message(STATUS "CUDA: ${CUDA_ENABLED}")
message(STATUS "AIGER: ${AIGER_FOUND}")
message(STATUS "Kissat: ${KISSAT_FOUND}")
message(STATUS "Build tests: ${BUILD_TESTS}")
message(STATUS "Build AIGER tools: ${BUILD_AIGER_TOOLS}")
message(STATUS "Build library: ${BUILD_LIBRARY}")
message(STATUS "Source files: ${EXISTING_SOURCES}")
message(STATUS "=====================================")
message(STATUS "") 