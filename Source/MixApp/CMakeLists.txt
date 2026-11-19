set(AM_MIX_CORE_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/StemSeparator.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/MixEngine.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/StemCard.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/MixMainComponent.cpp
    CACHE INTERNAL "")

juce_add_gui_app(AlphaMonsterMix
    PRODUCT_NAME "Alpha Monster Mix"
    VERSION ${PROJECT_VERSION}
    COMPANY_NAME "Alpha Monster"
    BUNDLE_ID "com.alphamonster.mix"
    MICROPHONE_PERMISSION_ENABLED FALSE
    DOCUMENT_EXTENSIONS amref)

target_sources(AlphaMonsterMix PRIVATE Main.cpp ${AM_MIX_CORE_SOURCES})
target_include_directories(AlphaMonsterMix PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
am_use_shared(AlphaMonsterMix TRUE)

target_link_libraries(AlphaMonsterMix PRIVATE
    am_demucs
    juce::juce_audio_utils
    juce::juce_dsp
    juce::juce_gui_extra
    juce::juce_recommended_lto_flags)

# Put the AI model inside the app when it is available at build time
set(AM_MODEL_FILE "" CACHE FILEPATH "Demucs ggml model to bundle with the app (optional)")
if(AM_MODEL_FILE AND EXISTS "${AM_MODEL_FILE}")
    get_filename_component(_model_name "${AM_MODEL_FILE}" NAME)
    if(APPLE)
        add_custom_command(TARGET AlphaMonsterMix POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_BUNDLE_CONTENT_DIR:AlphaMonsterMix>/Resources/Models"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "${AM_MODEL_FILE}" "$<TARGET_BUNDLE_CONTENT_DIR:AlphaMonsterMix>/Resources/Models/${_model_name}")
    else()
        add_custom_command(TARGET AlphaMonsterMix POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:AlphaMonsterMix>/Models"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "${AM_MODEL_FILE}" "$<TARGET_FILE_DIR:AlphaMonsterMix>/Models/${_model_name}")
    endif()
endif()
