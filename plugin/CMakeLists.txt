add_subdirectory (JUCE)

juce_add_plugin (KerrStinn
    COMPANY_NAME "Shuriken"
    COMPANY_WEBSITE "http://www.shuriken.se"
    BUNDLE_ID "eu.janschroeder.kerrstinn"
    PLUGIN_MANUFACTURER_CODE Shrk
    PLUGIN_CODE Kstn
    FORMATS VST3 AU Standalone
    PRODUCT_NAME "KerrStinn"
    DESCRIPTION "Slicer, glitcher and loop dissector"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_CATEGORIES Instrument Sampler
    AU_MAIN_TYPE kAudioUnitType_MusicDevice
    COPY_PLUGIN_AFTER_BUILD TRUE)

# The original artwork (CC BY-SA 4.0, see LICENSES.md).
juce_add_binary_data (KerrStinnAssets SOURCES
    ../original/bmp00128.png
    ../original/bmp00129.png
    ../original/bmp00131.png
    ../original/bmp00132.png)

set (KERR_PLUGIN_SOURCES Source/PluginProcessor.cpp Source/PluginEditor.cpp)
set (KERR_DEFINITIONS JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0)

target_sources (KerrStinn PRIVATE ${KERR_PLUGIN_SOURCES})
target_compile_definitions (KerrStinn PUBLIC ${KERR_DEFINITIONS})
target_link_libraries (KerrStinn
    PRIVATE KerrStinnAssets kerr_engine juce::juce_audio_utils
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags juce::juce_recommended_warning_flags)

# Drives the processor like a host and compares it with the engine.
juce_add_console_app (host_test PRODUCT_NAME "host_test")
set_target_properties (host_test PROPERTIES EXCLUDE_FROM_ALL TRUE)
target_sources (host_test PRIVATE Tests/host_test.cpp ${KERR_PLUGIN_SOURCES})
target_compile_definitions (host_test PRIVATE ${KERR_DEFINITIONS}
    KERR_FJORD="${CMAKE_SOURCE_DIR}/original/fjord.wav")
target_link_libraries (host_test PRIVATE KerrStinnAssets kerr_engine juce::juce_audio_utils
    PUBLIC juce::juce_recommended_config_flags)

# Renders the editor to PNGs for a visual check.
juce_add_console_app (gui_snapshot PRODUCT_NAME "gui_snapshot")
set_target_properties (gui_snapshot PROPERTIES EXCLUDE_FROM_ALL TRUE)
target_sources (gui_snapshot PRIVATE Tests/gui_snapshot.cpp ${KERR_PLUGIN_SOURCES})
target_compile_definitions (gui_snapshot PRIVATE ${KERR_DEFINITIONS})
target_link_libraries (gui_snapshot PRIVATE KerrStinnAssets kerr_engine juce::juce_audio_utils
    PUBLIC juce::juce_recommended_config_flags)
