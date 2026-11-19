# Third-party dependencies.
#
# By default they are downloaded with FetchContent at pinned versions.
# For offline builds point the AM_*_DIR cache variables to local checkouts.

include(FetchContent)

set(AM_JUCE_DIR    "" CACHE PATH "Local JUCE checkout (optional)")
set(AM_EIGEN_DIR   "" CACHE PATH "Local Eigen checkout (optional)")
set(AM_DEMUCS_DIR  "" CACHE PATH "Local demucs.cpp checkout (optional)")

# --- JUCE 8 -----------------------------------------------------------------
if(AM_JUCE_DIR)
    add_subdirectory(${AM_JUCE_DIR} ${CMAKE_BINARY_DIR}/_deps/juce-build)
else()
    FetchContent_Declare(juce
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG        8.0.4
        GIT_SHALLOW    TRUE)
    FetchContent_MakeAvailable(juce)
endif()

# --- Eigen (header only) - same commit demucs.cpp is developed against --------
if(AM_EIGEN_DIR)
    set(AM_EIGEN_INCLUDE ${AM_EIGEN_DIR})
else()
    FetchContent_Declare(eigen
        GIT_REPOSITORY https://github.com/eigen-mirror/eigen.git
        GIT_TAG        dd8c71e62852b2fe429edb6682ac91fd1c578a26)
    FetchContent_GetProperties(eigen)
    if(NOT eigen_POPULATED)
        FetchContent_Populate(eigen)
    endif()
    set(AM_EIGEN_INCLUDE ${eigen_SOURCE_DIR})
endif()

# --- demucs.cpp (MIT) – Demucs v4 inference in plain C++ ----------------------
if(AM_DEMUCS_DIR)
    set(AM_DEMUCS_SOURCE ${AM_DEMUCS_DIR})
else()
    FetchContent_Declare(demucscpp
        GIT_REPOSITORY https://github.com/sevagh/demucs.cpp.git
        GIT_TAG        4edae497a114de1ccb17ac9b4a2b8fc0144714d7)
    FetchContent_GetProperties(demucscpp)
    if(NOT demucscpp_POPULATED)
        FetchContent_Populate(demucscpp)   # no submodules, no CMake of theirs
    endif()
    set(AM_DEMUCS_SOURCE ${demucscpp_SOURCE_DIR})
endif()

file(GLOB AM_DEMUCS_SOURCES ${AM_DEMUCS_SOURCE}/src/*.cpp)
add_library(am_demucs STATIC ${AM_DEMUCS_SOURCES})
target_include_directories(am_demucs SYSTEM PUBLIC ${AM_EIGEN_INCLUDE} ${AM_DEMUCS_SOURCE}/src)
target_compile_definitions(am_demucs PUBLIC EIGEN_MPL2_ONLY)
target_compile_definitions(am_demucs PRIVATE NDEBUG)
set_target_properties(am_demucs PROPERTIES CXX_STANDARD 17)

if(MSVC)
    target_compile_options(am_demucs PRIVATE /O2 /Ob2 /fp:fast /bigobj /W0 /arch:AVX2 /utf-8)
    target_compile_options(am_demucs PUBLIC /bigobj)
else()
    target_compile_options(am_demucs PRIVATE -O3 -fno-math-errno -w)
endif()
