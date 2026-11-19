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
        Source/AmpCatalogSpecs.h
        Source/MiAmpGlassTheme.h
        Source/RigStageComponents.h
        Source/StudioFxModal.h
        Source/CabPanels.h
        Source/PracticeStudioView.h
        Source/PedalCatalogSpecs.h
        Source/ToneFinderEngine.h
        Source/PresetExporter.h
        Source/Localization.h
        Source/AutoUpdater.h
        Source/StandaloneAudioBridge.h
        Source/DSP/MiAmpBassChainDSP.h
        Source/DSP/MiAmpGuitarChainDSP.h
        Source/DSP/MiAmpCabMicDSP.h
        Source/DSP/MiAmpStudioFxDSP.h
        Source/DSP/MiAmpBoutiquePedalsDSP.h
        Source/DSP/MiAmpPracticeEngine.h
        Source/DSP/MiAmpCabIRs.h
        Source/DSP/XVBassDSP.h
        Source/DSP/XVCabIRs.h
        Source/DSP/XVBassTuner.h
        Source/DSP/XVPitchShifter.h
        Source/ThirdParty/signalsmith/signalsmith-stretch.h
        Source/ThirdParty/signalsmith/signalsmith-linear/stft.h
        Source/ThirdParty/signalsmith/signalsmith-linear/fft.h
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
        Resources/rig/bass_cab_1x15_b15.png
        Resources/rig/bass_cab_4x10_aguilar.png
        Resources/rig/bass_cab_4x10_gk.png
        Resources/rig/bass_cab_6x10_heavy.png
        Resources/rig/bass_cab_8x10_classic.png
        Resources/rig/bass_cab_8x10_mint.png
        Resources/rig/bass_cab_8x10_tangerine.png
        Resources/rig/bass_head_aguilar.png
        Resources/rig/bass_head_b15.png
        Resources/rig/bass_head_bassman.png
        Resources/rig/bass_head_dg900.png
        Resources/rig/bass_head_gk800.png
        Resources/rig/bass_head_mint.png
        Resources/rig/bass_head_orange200.png
        Resources/rig/bass_head_svt.png
        Resources/rig/bass_head_svt3pro.png
        Resources/rig/cab_2x12_classa.png
        Resources/rig/cab_2x12_twin.png
        Resources/rig/cab_4x12_britv30.png
        Resources/rig/cab_4x12_evh.png
        Resources/rig/cab_4x12_green.png
        Resources/rig/cab_4x12_recto.png
        Resources/rig/cab_4x12_tangerine.png
        Resources/rig/cab_4x12_mint.png
        Resources/rig/head_mi6505.png
        Resources/rig/head_mi800.png
        Resources/rig/head_miac30.png
        Resources/rig/head_mibe100.png
        Resources/rig/head_miplexi.png
        Resources/rig/head_mirecto.png
        Resources/rig/head_mirocker.png
        Resources/rig/head_mitwin.png
        Resources/rig/head_miamp_mint.png
        Resources/rig/knob_aguilar_silver.png
        Resources/rig/knob_bass_mint.png
        Resources/rig/knob_be_black.png
        Resources/rig/knob_chicken_cream.png
        Resources/rig/knob_dg_alum.png
        Resources/rig/knob_fender_skirt.png
        Resources/rig/knob_gk_black.png
        Resources/rig/knob_marshall_gold.png
        Resources/rig/knob_mesa_chrome.png
        Resources/rig/knob_orange_black.png
        Resources/rig/knob_peavey_black.png
        Resources/rig/knob_svt_fluted.png
        Resources/rig/knob_miamp_mint.png
        Resources/rig/mic_mi57_clean.png
        Resources/rig/mic_mi87_clean.png
        Resources/rig/fader_cap_hq.png
        Resources/rig/sw_push_round_off.png
        Resources/rig/sw_push_round_on.png
        Resources/rig/sw_push_square_off.png
        Resources/rig/sw_push_square_on.png
        Resources/rig/sw_rocker_black_off.png
        Resources/rig/sw_rocker_black_on.png
        Resources/rig/sw_rocker_red_off.png
        Resources/rig/sw_rocker_red_on.png
        Resources/rig/sw_slide_black_off.png
        Resources/rig/sw_slide_black_on.png
        Resources/rig/sw_toggle_silver_off.png
        Resources/rig/sw_toggle_silver_on.png
        Resources/rig/skatepark_tall_capa0.jpg
)

target_compile_definitions(MiAmp
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_USE_MP3AUDIOFORMAT=1
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_DISPLAY_SPLASH_SCREEN=0
        DONT_SET_USING_JUCE_NAMESPACE=1
        MIAMP_BUILD_NUMBER=${MIAMP_BUILD_NUMBER}
        MIAMP_APP_VERSION="1.1.${MIAMP_BUILD_NUMBER}"
)

# Pure-C++ Offline AI Stem Separation via Microsoft ONNX Runtime 1.23.2 (htdemucs_6s ONNX)
option(MIAMP_ENABLE_DEMUCS "Build embedded ONNX Runtime 6-stem AI engine" ON)
if(MIAMP_ENABLE_DEMUCS)
    set(_ort_dir "${CMAKE_CURRENT_BINARY_DIR}/_deps/onnxruntime")
    set(_ort_zip "${CMAKE_CURRENT_BINARY_DIR}/_deps/microsoft.ml.onnxruntime.1.23.2.zip")
    if(NOT EXISTS "${_ort_dir}/build/native/include/onnxruntime_c_api.h")
        file(MAKE_DIRECTORY "${_ort_dir}")
        if(NOT EXISTS "${_ort_zip}")
            message(STATUS "Downloading Microsoft.ML.OnnxRuntime 1.23.2 (headers + native runtime)...")
            file(DOWNLOAD
                "https://api.nuget.org/v3-flatcontainer/microsoft.ml.onnxruntime/1.23.2/microsoft.ml.onnxruntime.1.23.2.nupkg"
                "${_ort_zip}"
                SHOW_PROGRESS
                STATUS _ort_dl_status
            )
            list(GET _ort_dl_status 0 _ort_dl_code)
            if(NOT _ort_dl_code EQUAL 0)
                list(GET _ort_dl_status 1 _ort_dl_err)
                message(FATAL_ERROR "Failed to download Microsoft.ML.OnnxRuntime 1.23.2: ${_ort_dl_err}")
            endif()
        endif()
        file(ARCHIVE_EXTRACT INPUT "${_ort_zip}" DESTINATION "${_ort_dir}")
    endif()

    target_include_directories(MiAmp SYSTEM PUBLIC "${_ort_dir}/build/native/include")
    target_compile_definitions(MiAmp PUBLIC MIAMP_HAS_DEMUCS=1)

    if(WIN32)
        set(_ort_win_dll "${_ort_dir}/runtimes/win-x64/native/onnxruntime.dll")
        if(EXISTS "${_ort_win_dll}")
            if(TARGET MiAmp_Standalone)
                add_custom_command(TARGET MiAmp_Standalone POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${_ort_win_dll}" "$<TARGET_FILE_DIR:MiAmp_Standalone>/onnxruntime.dll"
                )
            endif()
            if(TARGET MiAmp_VST3)
                add_custom_command(TARGET MiAmp_VST3 POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${_ort_win_dll}" "$<TARGET_FILE_DIR:MiAmp_VST3>/onnxruntime.dll"
                )
            endif()
        else()
            message(FATAL_ERROR "Missing onnxruntime.dll inside NuGet package at ${_ort_win_dll}")
        endif()
    elseif(APPLE)
        set(_ort_mac_arm "${_ort_dir}/runtimes/osx-arm64/native/libonnxruntime.dylib")
        set(_ort_mac_x64 "${_ort_dir}/runtimes/osx-x64/native/libonnxruntime.dylib")
        set(_ort_mac_univ "${_ort_dir}/libonnxruntime_universal.dylib")
        if(EXISTS "${_ort_mac_arm}" AND EXISTS "${_ort_mac_x64}")
            execute_process(
                COMMAND lipo -create "${_ort_mac_arm}" "${_ort_mac_x64}" -output "${_ort_mac_univ}"
                RESULT_VARIABLE _lipo_res
            )
            if(NOT _lipo_res EQUAL 0)
                set(_ort_mac_univ "${_ort_mac_arm}")
            endif()
        elseif(EXISTS "${_ort_mac_arm}")
            set(_ort_mac_univ "${_ort_mac_arm}")
        elseif(EXISTS "${_ort_mac_x64}")
            set(_ort_mac_univ "${_ort_mac_x64}")
        endif()

        if(EXISTS "${_ort_mac_univ}")
            foreach(_mac_tgt IN ITEMS MiAmp_Standalone MiAmp_VST3 MiAmp_AU)
                if(TARGET ${_mac_tgt})
                    add_custom_command(TARGET ${_mac_tgt} POST_BUILD
                        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_BUNDLE_CONTENT_DIR:${_mac_tgt}>/Frameworks"
                        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${_ort_mac_univ}" "$<TARGET_BUNDLE_CONTENT_DIR:${_mac_tgt}>/Frameworks/libonnxruntime.dylib"
                        COMMAND codesign --force --sign - "$<TARGET_BUNDLE_CONTENT_DIR:${_mac_tgt}>/Frameworks/libonnxruntime.dylib"
                    )
                endif()
            endforeach()
        endif()
    endif()
else()
    target_compile_definitions(MiAmp PUBLIC MIAMP_HAS_DEMUCS=0)
endif()

target_compile_options(MiAmp
    PUBLIC
        $<$<CXX_COMPILER_ID:MSVC>:/utf-8 /Zc:__cplusplus>
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
