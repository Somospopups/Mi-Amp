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
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_DISPLAY_SPLASH_SCREEN=0
        DONT_SET_USING_JUCE_NAMESPACE=1
        MIAMP_BUILD_NUMBER=${MIAMP_BUILD_NUMBER}
        MIAMP_APP_VERSION="1.1.${MIAMP_BUILD_NUMBER}"
)

# Pure-C++ Offline AI Stem Separation (Eigen dd8c71e6 + sevagh/demucs.cpp htdemucs_6s)
option(MIAMP_ENABLE_DEMUCS "Build embedded pure-C++ Demucs 6-stem AI engine" ON)
if(MIAMP_ENABLE_DEMUCS)
    include(FetchContent)
    if(POLICY CMP0169)
        cmake_policy(SET CMP0169 OLD)
    endif()

    if(POLICY CMP0135)
        cmake_policy(SET CMP0135 NEW)
    endif()
    FetchContent_Declare(
        eigen
        URL https://gitlab.com/libeigen/eigen/-/archive/dd8c71e62852b2fe429edb6682ac91fd1c578a26/eigen-dd8c71e62852b2fe429edb6682ac91fd1c578a26.tar.gz
    )
    FetchContent_GetProperties(eigen)
    if(NOT eigen_POPULATED)
        FetchContent_Populate(eigen)
    endif()

    FetchContent_Declare(
        demucs_cpp
        GIT_REPOSITORY https://github.com/sevagh/demucs.cpp.git
        GIT_TAG        main
        GIT_SHALLOW    TRUE
        GIT_SUBMODULES ""
    )
    FetchContent_GetProperties(demucs_cpp)
    if(NOT demucs_cpp_POPULATED)
        FetchContent_Populate(demucs_cpp)
    endif()

    # Patch upstream sevagh/demucs.cpp bugs that crash Windows/macOS plugins on song load:
    # 1) model_load.cpp: uninitialized n_dims/length before late feof(f) check causes stack smash / bad_alloc at EOF
    # 2) model_load.cpp: fopen() on Windows fails on UTF-8 paths with accents -> use _wfopen
    # 3) layers.cpp / crosstransformer.cpp: std::exit(1) terminates entire host DAW/app -> throw std::runtime_error
    set(_demucs_ml "${demucs_cpp_SOURCE_DIR}/src/model_load.cpp")
    if(EXISTS "${_demucs_ml}")
        file(READ "${_demucs_ml}" _ml_src)
        string(FIND "${_ml_src}" "miamp_open_model_rb" _has_patch)
        if(_has_patch EQUAL -1)
            string(REPLACE
                "#include <vector>"
                "#include <vector>\n#ifdef _WIN32\n#include <windows.h>\n#endif\nstatic FILE* miamp_open_model_rb(const std::string& p) {\n#ifdef _WIN32\n    int wlen = MultiByteToWideChar(CP_UTF8, 0, p.c_str(), -1, nullptr, 0);\n    if (wlen > 0) {\n        std::wstring wp(static_cast<size_t>(wlen), L'\\0');\n        MultiByteToWideChar(CP_UTF8, 0, p.c_str(), -1, &wp[0], wlen);\n        if (FILE* wf = _wfopen(wp.c_str(), L\"rb\")) return wf;\n    }\n#endif\n    return fopen(p.c_str(), \"rb\");\n}"
                _ml_src "${_ml_src}")
            string(REPLACE
                "FILE *f = fopen(model_file.c_str(), \"rb\");"
                "FILE *f = miamp_open_model_rb(model_file);"
                _ml_src "${_ml_src}")
            string(REPLACE
                "        int32_t n_dims;\n        int32_t length;\n\n        fread(&n_dims, sizeof(int32_t), 1, f);\n        fread(&length, sizeof(int32_t), 1, f);\n\n        int32_t nelements = 1;\n\n        // we are loading up to 4d tensors, so allocate 4 dims\n        int32_t ne[4] = {1, 1, 1, 1};\n        for (int i = 0; i < n_dims; ++i)\n        {\n            fread(&ne[i], sizeof(int32_t), 1, f);\n            nelements *= ne[i];\n        }\n\n        std::string name;\n        std::vector<char> tmp(length);               // create a buffer\n        fread(&tmp[0], sizeof(char), tmp.size(), f); // read to buffer\n        name.assign(&tmp[0], tmp.size());\n\n        // check if we reached eof of the open file f\n        if (feof(f))\n        {\n            break;\n        }"
                "        int32_t n_dims = 0;\n        int32_t length = 0;\n\n        if (fread(&n_dims, sizeof(int32_t), 1, f) != 1 ||\n            fread(&length, sizeof(int32_t), 1, f) != 1)\n        {\n            break;\n        }\n        if (n_dims <= 0 || n_dims > 4 || length <= 0 || length > 4096)\n        {\n            fclose(f);\n            return false;\n        }\n\n        int32_t nelements = 1;\n\n        // we are loading up to 4d tensors, so allocate 4 dims\n        int32_t ne[4] = {1, 1, 1, 1};\n        for (int i = 0; i < n_dims; ++i)\n        {\n            if (fread(&ne[i], sizeof(int32_t), 1, f) != 1 || ne[i] <= 0)\n            {\n                fclose(f);\n                return false;\n            }\n            nelements *= ne[i];\n        }\n\n        std::string name;\n        std::vector<char> tmp(static_cast<size_t>(length));\n        if (fread(tmp.data(), sizeof(char), tmp.size(), f) != tmp.size())\n        {\n            break;\n        }\n        name.assign(tmp.data(), tmp.size());"
                _ml_src "${_ml_src}")
            string(REPLACE
                "        if (loaded_size == 0)\n        {\n            my_fprintf(stderr, \"%s: failed to load %s\\n\", __func__,\n                       name.c_str());\n            return false;\n        }"
                "        if (loaded_size == 0)\n        {\n            my_fprintf(stderr, \"%s: failed to load %s\\n\", __func__,\n                       name.c_str());\n            fclose(f);\n            return false;\n        }"
                _ml_src "${_ml_src}")
            file(WRITE "${_demucs_ml}" "${_ml_src}")
        endif()
    endif()

    foreach(_demucs_guard_file IN ITEMS
        "${demucs_cpp_SOURCE_DIR}/src/layers.cpp"
        "${demucs_cpp_SOURCE_DIR}/src/crosstransformer.cpp")
        if(EXISTS "${_demucs_guard_file}")
            file(READ "${_demucs_guard_file}" _dg_src)
            string(FIND "${_dg_src}" "std::exit(1);" _has_exit)
            if(NOT _has_exit EQUAL -1)
                string(FIND "${_dg_src}" "<stdexcept>" _has_stdexcept)
                if(_has_stdexcept EQUAL -1)
                    string(REPLACE "#include \"model.hpp\"" "#include \"model.hpp\"\n#include <stdexcept>" _dg_src "${_dg_src}")
                endif()
                string(REPLACE "std::exit(1);" "throw std::runtime_error(\"demucs invalid tensor dimension\");" _dg_src "${_dg_src}")
                file(WRITE "${_demucs_guard_file}" "${_dg_src}")
            endif()
        endif()
    endforeach()

    file(GLOB DEMUCS_CORE_SOURCES CONFIGURE_DEPENDS "${demucs_cpp_SOURCE_DIR}/src/*.cpp")
    add_library(demucs_core STATIC ${DEMUCS_CORE_SOURCES})
    set_target_properties(demucs_core PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED ON
        POSITION_INDEPENDENT_CODE ON
    )
    target_include_directories(demucs_core SYSTEM PUBLIC
        "${demucs_cpp_SOURCE_DIR}/src"
        "${eigen_SOURCE_DIR}"
    )
    target_compile_definitions(demucs_core PUBLIC
        $<$<PLATFORM_ID:Windows>:NOMINMAX WIN32_LEAN_AND_MEAN _CRT_SECURE_NO_WARNINGS>
    )
    target_compile_options(demucs_core PRIVATE
        $<$<CXX_COMPILER_ID:MSVC>:/utf-8 /O2 /bigobj /Zc:__cplusplus>
        $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-O2 -Wno-sign-conversion -Wno-unused-parameter>
    )
    target_link_libraries(MiAmp PRIVATE demucs_core)
    target_compile_definitions(MiAmp PUBLIC MIAMP_HAS_DEMUCS=1)
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
