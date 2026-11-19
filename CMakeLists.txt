cmake_minimum_required(VERSION 3.22)

if(APPLE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "10.15" CACHE STRING "Minimum macOS version")
endif()

if(MSVC OR WIN32)
    # static runtime: plugins and apps run without the VC++ redistributable
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

project(AlphaMonster VERSION 1.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

option(AM_BUILD_MIX_APP  "Build the Alpha Monster Mix standalone app" ON)
option(AM_BUILD_MASTER   "Build the Alpha Monster Master plugin"      ON)
option(AM_BUILD_TOOLS    "Build command line test tools"              ON)
option(AM_HEADLESS_LINUX "Linux build without ALSA/X extensions (dev checks only)" OFF)

include(cmake/Dependencies.cmake)

# ---------------------------------------------------------------------------
# Shared resources (fonts) and shared code.
# JUCE modules are compiled per target, so shared sources are added to each
# product through am_use_shared() instead of a separate static library.
# ---------------------------------------------------------------------------
juce_add_binary_data(am_binary_data
    NAMESPACE AMBinary
    SOURCES
        Resources/Fonts/Michroma-Regular.ttf
        Resources/Fonts/BarlowCondensed-SemiBold.ttf
        Resources/Fonts/BarlowCondensed-Bold.ttf
        Resources/Fonts/ShareTechMono-Regular.ttf)

set(AM_SHARED_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/Source/Shared/Analysis/AudioAnalyzer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Source/Shared/Analysis/Loudness.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Source/Shared/Analysis/Profiles.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Source/Shared/Analysis/FilterDesign.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Source/Shared/Analysis/AudioFileUtils.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Source/Shared/Dsp/Dynamics.cpp)

set(AM_SHARED_UI_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/Source/Shared/UI/AlphaLookAndFeel.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Source/Shared/UI/Widgets.cpp)

set(AM_JUCE_DEFS
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_REPORT_APP_USAGE=0
    JUCE_MODAL_LOOPS_PERMITTED=1
    JUCE_SILENCE_XCODE_15_LINKER_WARNING=1
    JUCE_USE_MP3AUDIOFORMAT=1
    JUCE_USE_WINDOWS_MEDIA_FORMAT=1
    AM_VERSION_STRING="${PROJECT_VERSION}")

if(AM_HEADLESS_LINUX)
    list(APPEND AM_JUCE_DEFS JUCE_ALSA=0 JUCE_JACK=0 JUCE_USE_XRANDR=0
         JUCE_USE_XINERAMA=0 JUCE_USE_XCURSOR=0 JUCE_USE_XSHM=0)
endif()

function(am_use_shared target with_ui)
    target_sources(${target} PRIVATE ${AM_SHARED_SOURCES})
    if(with_ui)
        target_sources(${target} PRIVATE ${AM_SHARED_UI_SOURCES})
    endif()
    target_include_directories(${target} PRIVATE ${CMAKE_SOURCE_DIR}/Source/Shared)
    target_compile_definitions(${target} PUBLIC ${AM_JUCE_DEFS})
    target_link_libraries(${target} PRIVATE
        am_binary_data
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)
endfunction()

# ---------------------------------------------------------------------------
# Products
# ---------------------------------------------------------------------------
if(AM_BUILD_MIX_APP)
    add_subdirectory(Source/MixApp)
endif()

if(AM_BUILD_MASTER)
    add_subdirectory(Source/MasterPlugin)
endif()

if(AM_BUILD_TOOLS)
    add_subdirectory(Tools)
endif()
