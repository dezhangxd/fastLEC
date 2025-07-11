cmake_minimum_required(VERSION 3.16)
project(fastLEC VERSION 0.1.0 LANGUAGES C CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

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
else()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O3")
endif()

# Find required packages
find_package(Threads REQUIRED)

# ========================================
# Submodule Configuration
# ========================================

# XGBoost submodule
option(USE_XGBOOST "Enable XGBoost support" ON)
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
    message(STATUS "XGBoost submodule found")
else()
    message(WARNING "XGBoost submodule not found. Run: git submodule update --init --recursive")
    set(XGBOOST_FOUND FALSE)
endif()

# AIGER submodule
if(EXISTS "${CMAKE_SOURCE_DIR}/deps/aiger/aiger.h")
    set(AIGER_FOUND TRUE)
    set(AIGER_INCLUDE_DIRS "${CMAKE_SOURCE_DIR}/deps/aiger")
    message(STATUS "AIGER submodule found")
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

# ========================================
# AIGER Library
# ========================================
if(AIGER_FOUND)
    add_library(aiger STATIC
        deps/aiger/aiger.c
    )
    
    target_include_directories(aiger PUBLIC ${AIGER_INCLUDE_DIRS})
    target_compile_definitions(aiger PUBLIC NDEBUG)
    
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
    # Kissat is typically built as a standalone executable
    # We'll create a library target for linking
    add_library(kissat_lib INTERFACE)
    target_include_directories(kissat_lib INTERFACE ${KISSAT_INCLUDE_DIRS})
    
    # If you want to build Kissat as a library, you might need to modify its build system
    message(STATUS "Kissat interface library created")
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
    src/es.cpp
    src/AIG.cpp
)

# Header files
set(HEADERS
    src/fastLEC.hpp
    src/basic.hpp
    src/AIG.hpp
    src/XAG.hpp
    src/CNF.hpp
    src/parser.hpp
)

# Check if source files exist and add them to the list
set(EXISTING_SOURCES "")
foreach(SOURCE ${SOURCES})
    if(EXISTS "${CMAKE_SOURCE_DIR}/${SOURCE}")
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
    
    if(AIGER_FOUND)
        target_link_libraries(fastLEC PRIVATE aiger)
        target_include_directories(fastLEC PRIVATE ${AIGER_INCLUDE_DIRS})
    endif()
    
    if(KISSAT_FOUND)
        target_link_libraries(fastLEC PRIVATE kissat_lib)
        target_include_directories(fastLEC PRIVATE ${KISSAT_INCLUDE_DIRS})
    endif()
    
    if(XGBOOST_FOUND)
        # XGBoost linking with proper error handling
        find_library(XGBOOST_LIBRARY xgboost PATHS ${CMAKE_BINARY_DIR}/deps/xgboost/lib NO_DEFAULT_PATH)
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
    
    if(AIGER_FOUND)
        target_link_libraries(fastLEC_lib PUBLIC aiger)
    endif()
    
    if(KISSAT_FOUND)
        target_link_libraries(fastLEC_lib PUBLIC kissat_lib)
    endif()
    
    if(XGBOOST_FOUND)
        find_library(XGBOOST_LIBRARY xgboost PATHS ${CMAKE_BINARY_DIR}/deps/xgboost/lib NO_DEFAULT_PATH)
        if(XGBOOST_LIBRARY)
            target_link_libraries(fastLEC_lib PUBLIC ${XGBOOST_LIBRARY})
        endif()
    endif()
    
    set_target_properties(fastLEC_lib PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
    )
    
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
message(STATUS "Use XGBoost: ${USE_XGBOOST}")
message(STATUS "XGBoost: ${XGBOOST_FOUND}")
message(STATUS "AIGER: ${AIGER_FOUND}")
message(STATUS "Kissat: ${KISSAT_FOUND}")
message(STATUS "Build tests: ${BUILD_TESTS}")
message(STATUS "Build AIGER tools: ${BUILD_AIGER_TOOLS}")
message(STATUS "Build library: ${BUILD_LIBRARY}")
message(STATUS "Source files: ${EXISTING_SOURCES}")
message(STATUS "=====================================")
message(STATUS "") 