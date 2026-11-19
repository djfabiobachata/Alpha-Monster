set(AM_MASTER_FORMATS VST3 AU Standalone)
if(AM_HEADLESS_LINUX)
    set(AM_MASTER_FORMATS Standalone)
elseif(NOT APPLE)
    set(AM_MASTER_FORMATS VST3 Standalone)
endif()

juce_add_plugin(AlphaMonsterMaster
    PRODUCT_NAME "Alpha Monster Master"
    VERSION ${PROJECT_VERSION}
    COMPANY_NAME "Alpha Monster"
    COMPANY_WEBSITE "https://github.com/djfabiobachata/Alpha-Monster"
    BUNDLE_ID "com.alphamonster.master"
    PLUGIN_MANUFACTURER_CODE AlMo
    PLUGIN_CODE AmMs
    FORMATS ${AM_MASTER_FORMATS}
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Fx Mastering Dynamics EQ
    AU_MAIN_TYPE kAudioUnitType_Effect
    MICROPHONE_PERMISSION_ENABLED TRUE
    MICROPHONE_PERMISSION_TEXT "Alpha Monster Master standalone uses the audio input to process sound.")

target_sources(AlphaMonsterMaster PRIVATE
    MasterProcessor.cpp
    MasterEditor.cpp
)

target_include_directories(AlphaMonsterMaster PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
am_use_shared(AlphaMonsterMaster TRUE)

target_compile_definitions(AlphaMonsterMaster PUBLIC
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(AlphaMonsterMaster PRIVATE
    juce::juce_audio_utils
    juce::juce_dsp
    juce::juce_recommended_lto_flags)
