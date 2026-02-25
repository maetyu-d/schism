cmake_minimum_required(VERSION 3.22)
project(Schism VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(DEFINED JUCE_DIR AND EXISTS "${JUCE_DIR}/CMakeLists.txt")
  add_subdirectory("${JUCE_DIR}" juce)
else()
  include(FetchContent)
  FetchContent_Declare(
    juce
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.3
  )
  FetchContent_MakeAvailable(juce)
endif()

juce_add_gui_app(Schism
  PRODUCT_NAME "Schism"
)

juce_add_console_app(SchismSelfTest
  PRODUCT_NAME "SchismSelfTest"
)

target_sources(Schism PRIVATE
  Source/Main.cpp
  Source/MainComponent.h
  Source/MainComponent.cpp
  Source/IR/GraphIR.h
  Source/IR/GraphIR.cpp
  Source/Text/GraphLanguage.h
  Source/Text/GraphLanguage.cpp
  Source/Sync/SyncMap.h
  Source/Sync/SyncMap.cpp
  Source/Visual/GraphCanvas.h
  Source/Visual/GraphCanvas.cpp
  Source/DSP/RuntimeEngine.h
  Source/DSP/RuntimeEngine.cpp
  Source/Patch/PatchFile.h
  Source/Patch/PatchFile.cpp
  Source/UI/ProbeWidgets.h
  Source/UI/ProbeWidgets.cpp
)

target_compile_definitions(Schism PRIVATE
  JUCE_WEB_BROWSER=0
  JUCE_USE_CURL=0
  JUCE_DISPLAY_SPLASH_SCREEN=0
  JUCE_REPORT_APP_USAGE=0
)

target_compile_definitions(SchismSelfTest PRIVATE
  JUCE_WEB_BROWSER=0
  JUCE_USE_CURL=0
  JUCE_REPORT_APP_USAGE=0
)

target_link_libraries(Schism PRIVATE
  juce::juce_audio_basics
  juce::juce_audio_devices
  juce::juce_audio_formats
  juce::juce_audio_processors
  juce::juce_audio_utils
  juce::juce_core
  juce::juce_data_structures
  juce::juce_events
  juce::juce_graphics
  juce::juce_gui_basics
  juce::juce_gui_extra
)

target_sources(SchismSelfTest PRIVATE
  Source/SelfTestMain.cpp
  Source/IR/GraphIR.cpp
  Source/Sync/SyncMap.cpp
  Source/Text/GraphLanguage.cpp
  Source/DSP/RuntimeEngine.cpp
  Source/Patch/PatchFile.cpp
)

target_link_libraries(SchismSelfTest PRIVATE
  juce::juce_audio_basics
  juce::juce_core
  juce::juce_data_structures
)
