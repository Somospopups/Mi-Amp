cmake_minimum_required(VERSION 3.22)

if(APPLE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version" FORCE)
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "Universal macOS architectures" FORCE)
endif()

project(MiAmp VERSION 1.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(MIAMP_BUILD_NUMBER "0" CACHE STRING "CI build number for auto-updater")

# Busca JUCE preinstalado, ruta local (-DJUCE_DIR=...), o descarga JUCE 8.0.4 automáticamente
find_package(JUCE CONFIG QUIET)
if(NOT JUCE_FOUND)
    if(DEFINED JUCE_DIR AND EXISTS "${JUCE_DIR}/CMakeLists.txt")
        add_subdirectory("${JUCE_DIR}" JUCE EXCLUDE_FROM_ALL)
    else()
        include(FetchContent)
        FetchContent_Declare(
            JUCE
            GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
            GIT_TAG        8.0.4
            GIT_SHALLOW    TRUE
        )
        FetchContent_MakeAvailable(JUCE)
    endif()
endif()

if(APPLE)
    set(PLUGIN_FORMATS VST3 AU Standalone)
else()
    set(PLUGIN_FORMATS VST3 Standalone)
endif()

juce_add_plugin(MiAmp
    COMPANY_NAME                  "Popups Audio"
    COMPANY_WEBSITE               "https://github.com/Somospopups/Mi-Amp"
    BUNDLE_ID                     "com.popupsaudio.miamp"
    IS_SYNTH                      FALSE
    NEEDS_MIDI_INPUT              FALSE
    NEEDS_MIDI_OUTPUT             FALSE
    IS_MIDI_EFFECT                FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS   TRUE
    COPY_PLUGIN_AFTER_BUILD       FALSE
    PLUGIN_MANUFACTURER_CODE      PpAu
    PLUGIN_CODE                   MiAm
    FORMATS                       ${PLUGIN_FORMATS}
    PRODUCT_NAME                  "MI AMP"
    ICON_BIG                      "${CMAKE_CURRENT_SOURCE_DIR}/Resources/app_icon_512.png"
    ICON_SMALL                    "${CMAKE_CURRENT_SOURCE_DIR}/Resources/app_icon_180.png"
    MICROPHONE_PERMISSION_ENABLED TRUE
    MICROPHONE_PERMISSION_TEXT    "MI AMP uses the audio input to process guitar and bass in real time."
)

juce_generate_juce_header(MiAmp)

target_sources(MiAmp
    PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginProcessor.h
        Source/PluginEditor.cpp
        Source/PluginEditor.h
        Source/PedalCatalogSpecs.h
        Source/ToneFinderEngine.h
        Source/PresetExporter.h
        Source/Localization.h
        Source/AutoUpdater.h
        Source/StandaloneAudioBridge.h
        Source/DSP/MiAmpBassChainDSP.h
        Source/DSP/MiAmpGuitarChainDSP.h
        Source/DSP/MiAmpBoutiquePedalsDSP.h
        Source/DSP/ZenboxCabIRs.h
        Source/DSP/XVBassDSP.h
        Source/DSP/XVCabIRs.h
        Source/DSP/XVBassTuner.h
        Source/DSP/XVPitchShifter.h
)

juce_add_binary_data(MiAmpAssets
    NAMESPACE BinaryData
    SOURCES
        Resources/intro_sky.png
        Resources/intro_logo.png
        Resources/popups_credit.png
        Resources/guitar_knob_ring.png
        Resources/main_bg.png
        Resources/bass_bg.png
        Resources/bass_knob_cap.png
        Resources/toggle_up.png
        Resources/toggle_down.png
        Resources/floor_bg.png
        Resources/board_surface.png
        Resources/xv_active.png
        Resources/xv_bypass.png
        Resources/xv_knob_ptr.png
        Resources/xv_cab_bass.png
        Resources/xv_cab_punk.png
        Resources/xv_cab_clean.png
        Resources/ped2_bass_food_body.png
        Resources/ped2_bass_food_knob.png
        Resources/ped2_mi_87_body.png
        Resources/ped2_mi_87_knob.png
        Resources/ped2_mi_bass_driver_body.png
        Resources/ped2_mi_bass_driver_knob.png
        Resources/ped2_mi_bass_eq_body.png
        Resources/ped2_mi_deep_chorus_body.png
        Resources/ped2_mi_deep_chorus_knob.png
        Resources/ped2_mi_dual_crunch_body.png
        Resources/ped2_mi_dual_crunch_knob.png
        Resources/ped2_mi_gold_body.png
        Resources/ped2_mi_gold_knob.png
        Resources/ped2_mi_graphic_eq_body.png
        Resources/ped2_mi_green_drive_body.png
        Resources/ped2_mi_green_drive_knob.png
        Resources/ped2_mi_ice_comp_body.png
        Resources/ped2_mi_ice_comp_knob.png
        Resources/ped2_mi_lime_phase_body.png
        Resources/ped2_mi_lime_phase_knob.png
        Resources/ped2_mi_olive_body.png
        Resources/ped2_mi_olive_knob.png
        Resources/ped2_mi_orange_body.png
        Resources/ped2_mi_orange_knob.png
        Resources/ped2_mi_pearl_body.png
        Resources/ped2_mi_pearl_knob.png
        Resources/ped2_mi_phaser_body.png
        Resources/ped2_mi_phaser_knob.png
        Resources/ped2_mi_poly_octave_body.png
        Resources/ped2_mi_poly_octave_knob.png
        Resources/ped2_mi_silent_body.png
        Resources/ped2_mi_silent_knob.png
        Resources/ped2_mi_sky_chorus_body.png
        Resources/ped2_mi_sky_chorus_knob.png
        Resources/ped2_mi_studio_body.png
        Resources/ped2_mi_studio_knob.png
        Resources/ped2_mi_sub_octave_body.png
        Resources/ped2_mi_sub_octave_knob.png
        Resources/ped2_mi_sustain_body.png
        Resources/ped2_mi_sustain_knob.png
        Resources/ped2_mi_violet_body.png
        Resources/ped2_mi_violet_knob.png
        Resources/ped2_slider_eq_cap.png
)

target_compile_definitions(MiAmp
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_DISPLAY_SPLASH_SCREEN=0
        DONT_SET_USING_JUCE_NAMESPACE=1
        MIAMP_BUILD_NUMBER=${MIAMP_BUILD_NUMBER}
        MIAMP_APP_VERSION="1.1.${MIAMP_BUILD_NUMBER}"
)

target_compile_options(MiAmp
    PRIVATE
        $<$<CXX_COMPILER_ID:MSVC>:/utf-8>
)

target_link_libraries(MiAmp
    PRIVATE
        MiAmpAssets
        juce::juce_dsp
        juce::juce_audio_utils
        juce::juce_audio_processors
        juce::juce_gui_basics
        juce::juce_graphics
    PUBLIC
        juce::juce_recommended_config_flags
        $<$<NOT:$<PLATFORM_ID:Darwin>>:juce::juce_recommended_lto_flags>
        juce::juce_recommended_warning_flags
)

if(TARGET MiAmp_Standalone)
    target_sources(MiAmp_Standalone PRIVATE Source/StandaloneAudioImpl.cpp)
endif()

if(TARGET MiAmp_VST3)
    target_sources(MiAmp_VST3 PRIVATE Source/StandaloneAudioStub.cpp)
endif()

if(TARGET MiAmp_AU)
    target_sources(MiAmp_AU PRIVATE Source/StandaloneAudioStub.cpp)
endif()
