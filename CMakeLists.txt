cmake_minimum_required(VERSION 3.23)
project(lug 
    LANGUAGES CXX 
    VERSION 1.0.0
) 

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(BUILD_TESTING "Build Testing Suite" OFF)
option(BUILD_SHARED_LIBS "Build as Shared Library" OFF)
option(BUILD_WEBSITE "Build the website, such as C++ dependent code like the playground" OFF)

#-------- lug IMPLEMENTATION --------
if (BUILD_SHARED_LIBS)
add_library(lug_core SHARED)
else()
add_library(lug_core STATIC)
endif()

target_compile_features(lug_core PUBLIC cxx_std_17)


target_sources(lug_core
    PRIVATE
        src/lug_auxlib.cpp
        src/lug_baselib.cpp
        src/lug_mathlib.cpp
        src/lug_colllib.cpp
        src/lug_stdlib.cpp
        src/lug_strlib.cpp
        src/lug_numlib.cpp
        
        src/lug_chunk.cpp
        src/lug_compiler.cpp
        src/lug_debug.cpp
        src/lug_lexer.cpp
        src/lug_memory.cpp
        src/lug_object.cpp
        src/lug_opcodes.cpp
        src/lug_parser.cpp
        src/lug_table.cpp
        src/lug_value.cpp
        src/lug_variant.cpp
        src/lug_vm.cpp
        src/lug_vmcore.cpp
        src/lug_gc.cpp
        src/lug_filesys.cpp
    PUBLIC 
        FILE_SET HEADERS
        BASE_DIRS include
        FILES 
            include/lug.hpp
            include/lug_variant.hpp
            include/lug_version.hpp
            include/lug_vm.hpp
)

# Let lug_core include any private headers in src :)
target_include_directories(lug_core PRIVATE src)

#-------- lug MAIN EXE --------
add_executable(lug)
target_sources(lug
    PRIVATE 
        main.cpp    
)

# Link the lug_core library with lug
target_link_libraries(lug PRIVATE lug_core)

#-------- BROWSER PLAYGROUND --------
if(BUILD_WEBSITE)
    if(NOT EMSCRIPTEN)
        message(FATAL_ERROR "BUILD_WEBSITE requires the Emscripten toolchain")
    endif()

    add_executable(lug_website)
    target_sources(lug_website
        PRIVATE
            docs/wasm/website.cpp
    )

    target_link_libraries(lug_website PRIVATE lug_core)
    target_compile_options(lug_core PRIVATE -fexceptions)
    target_compile_options(lug_website PRIVATE -fexceptions)
    target_link_options(lug_website PRIVATE
        -fexceptions
        --bind
        -sMODULARIZE=1
        -sEXPORT_ES6=1
        -sEXPORT_NAME=createLugModule
        -sENVIRONMENT=web
        -sALLOW_MEMORY_GROWTH=1
    )

    set_target_properties(lug_website PROPERTIES
        OUTPUT_NAME "lug-website"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/site/js"
    )
endif()

#-------- TESTS --------
include(CTest)
if(BUILD_TESTING)
    add_subdirectory(tests)
endif()

#-------- INSTALLATION --------
install(
    TARGETS lug_core
    EXPORT lug_targets

    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}

    FILE_SET HEADERS
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(
    TARGETS lug
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
    
include(GNUInstallDirs)
install(
    EXPORT lug_targets
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/lug
    NAMESPACE lug::
)
        
include(CMakePackageConfigHelpers)
write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/lug-config-version.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

install(
    FILES
        cmake/lug-config.cmake
        ${CMAKE_CURRENT_BINARY_DIR}/lug-config-version.cmake
    DESTINATION 
        ${CMAKE_INSTALL_LIBDIR}/cmake/lug
)

#-------- PACKING --------
set(CPACK_PACKAGE_NAME lug)
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_VENDOR "skribster")


# literally 90% of this is just to make the outputted
# name of the .zip binary look good.... it should output
# something like:
# lug-<version>-<system_name>-<system_processor>-<compiler_tag>
# like:
# lug-1.0.0-windows-x64-clang
string(TOLOWER "${CMAKE_SYSTEM_NAME}" PKG_OS)
if(PKG_OS STREQUAL "darwin")
    set(PKG_OS "macos")
endif()

string(TOLOWER "${CMAKE_SYSTEM_PROCESSOR}" PKG_ARCH)
if(PKG_ARCH MATCHES "^(x86_64|amd64)$")
    set(PKG_ARCH "x64")
elseif(PKG_ARCH MATCHES "^(i386|i686|x86)$")
    set(PKG_ARCH "x86")
elseif(PKG_ARCH MATCHES "^(aarch64|arm64)$")
    set(PKG_ARCH "arm64")
endif()

if(MSVC)
    if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
        set(PKG_COMPILER "clang-cl")
    else()
        set(PKG_COMPILER "msvc")
    endif()
elseif(MINGW)
    set(PKG_COMPILER "mingw")
elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    set(PKG_COMPILER "clang")
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    set(PKG_COMPILER "gcc")
else()
    string(TOLOWER "${CMAKE_CXX_COMPILER_ID}" PKG_COMPILER)
endif()

set(CPACK_GENERATOR "ZIP")
set(CPACK_PACKAGE_FILE_NAME 
    "${PROJECT_NAME}-${PROJECT_VERSION}-${PKG_OS}-${PKG_ARCH}-${PKG_COMPILER}"
)

include(CPack)