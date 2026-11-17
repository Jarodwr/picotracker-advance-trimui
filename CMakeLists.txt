cmake_minimum_required(VERSION 3.22)
project(picoTrackerAdvanceDesktop LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(PICOTRACKER_UPSTREAM_ROOT
  "${CMAKE_CURRENT_SOURCE_DIR}/upstream/advance-firmware" CACHE PATH
  "Advance firmware checkout used by the host wrapper")
set(UPSTREAM ${PICOTRACKER_UPSTREAM_ROOT}/sources)
if(NOT EXISTS ${UPSTREAM}/Application/Model/Song.cpp)
  message(FATAL_ERROR "Initialize the upstream submodule or set PICOTRACKER_UPSTREAM_ROOT")
endif()
option(PICOTRACKER_APPLE_STRLEN_WORKAROUND
  "Use a temporary literal-length workaround for unpatched Apple builds" ON)

find_package(SDL2 CONFIG REQUIRED)

# Compile the firmware's view graph independently while the remaining host
# services are being implemented. This target is not linked into the scaffold.
add_library(picotracker_application_probe OBJECT EXCLUDE_FROM_ALL
  ${UPSTREAM}/Application/AppWindow.cpp
  ${UPSTREAM}/Application/Views/RecordView.cpp
)
target_compile_definitions(picotracker_application_probe PRIVATE ADV
  "strlen=__builtin_strlen")
target_compile_options(picotracker_application_probe PRIVATE
  "-Dsection(x)=unused" -Wno-deprecated-declarations)
target_include_directories(picotracker_application_probe PRIVATE
  platform/desktop/include
  ${UPSTREAM}
  ${UPSTREAM}/config
  ${UPSTREAM}/Externals/etl/include
  ${UPSTREAM}/Foundation/Variables
  ${UPSTREAM}/Application/Views
  ${UPSTREAM}/System/Console
)

add_executable(host_filesystem_check
  tests/host_filesystem_check.cpp
  platform/desktop/HostFileSystem.cpp
)
target_include_directories(host_filesystem_check PRIVATE
  platform/desktop
  ${UPSTREAM}
  ${UPSTREAM}/config
  ${UPSTREAM}/Externals/etl/include
)

add_executable(host_event_check
  tests/host_event_check.cpp
  platform/desktop/HostEvent.cpp
  platform/desktop/HostQuitDialog.cpp
  platform/desktop/HostVolumeOverlay.cpp
  platform/desktop/HostTimer.cpp
  platform/desktop/SDLWindow.cpp
  ${UPSTREAM}/UIFramework/BasicDatas/GUIRect.cpp
  ${UPSTREAM}/UIFramework/BasicDatas/GUIEvent.cpp
  ${UPSTREAM}/UIFramework/Interfaces/I_GUIWindowImp.cpp
  ${UPSTREAM}/UIFramework/SimpleBaseClasses/GUIWindow.cpp
  ${UPSTREAM}/UIFramework/SimpleBaseClasses/EventManager.cpp
  ${UPSTREAM}/Foundation/Observable.cpp
  ${UPSTREAM}/System/Timer/Timer.cpp
)
target_compile_definitions(host_event_check PRIVATE ADV)
target_include_directories(host_event_check PRIVATE
  platform/desktop
  ${UPSTREAM}
  ${UPSTREAM}/config
  ${UPSTREAM}/Externals/etl/include
)
target_link_libraries(host_event_check PRIVATE SDL2::SDL2)

add_executable(host_input_map_check
  tests/host_input_map_check.cpp
  platform/desktop/HostInputMap.cpp
)
target_include_directories(host_input_map_check PRIVATE
  platform/desktop
  ${UPSTREAM}
)

add_executable(host_quit_dialog_check
  tests/host_quit_dialog_check.cpp
  platform/desktop/HostQuitDialog.cpp
  platform/desktop/HostInputMap.cpp
)
target_include_directories(host_quit_dialog_check PRIVATE
  platform/desktop
  ${UPSTREAM}
)

add_executable(host_battery_check
  tests/host_battery_check.cpp
  platform/desktop/HostBattery.cpp
)
target_include_directories(host_battery_check PRIVATE platform/desktop)

add_executable(host_volume_check
  tests/host_volume_check.cpp
  platform/desktop/HostVolume.cpp
  platform/desktop/HostVolumeOverlay.cpp
)
target_include_directories(host_volume_check PRIVATE
  platform/desktop
  ${UPSTREAM}
)

add_executable(host_input_profile_check
  tests/host_input_profile_check.cpp
  platform/desktop/HostInputProfile.cpp
  platform/desktop/HostInputMap.cpp
)
target_include_directories(host_input_profile_check PRIVATE
  platform/desktop
  ${UPSTREAM}
)

add_executable(host_display_layout_check
  tests/host_display_layout_check.cpp
  platform/desktop/HostDisplayGeometry.cpp
)
target_include_directories(host_display_layout_check PRIVATE platform/desktop)

# Compile host audio/MIDI adapters before linking the full application graph.
add_library(picotracker_service_probe OBJECT
  platform/desktop/HostAudio.cpp
  platform/desktop/HostMidi.cpp
  platform/desktop/HostSamplePool.cpp
  platform/desktop/SDLAudioDriver.cpp
)
target_compile_definitions(picotracker_service_probe PRIVATE ADV)
target_compile_options(picotracker_service_probe PRIVATE
  "-Dsection(x)=unused" -Wno-deprecated-declarations)
target_include_directories(picotracker_service_probe PRIVATE
  platform/desktop/include
  ${UPSTREAM}
  ${UPSTREAM}/config
  ${UPSTREAM}/Externals/etl/include
  ${UPSTREAM}/Foundation/Variables
  platform/desktop
)
target_link_libraries(picotracker_service_probe PRIVATE SDL2::SDL2)

# Diagnostic compile target for the full shared application graph. Keep it
# separate from the runnable scaffold until every host service is linked.
file(GLOB_RECURSE PICOTRACKER_SHARED_CPP CONFIGURE_DEPENDS
  ${UPSTREAM}/Application/*.cpp
  ${UPSTREAM}/Services/*.cpp
  ${UPSTREAM}/System/*.cpp
  ${UPSTREAM}/Foundation/*.cpp
  ${UPSTREAM}/UIFramework/*.cpp
)
list(FILTER PICOTRACKER_SHARED_CPP EXCLUDE REGEX "/T_(Factory|Singleton)\\.cpp$")
list(FILTER PICOTRACKER_SHARED_CPP EXCLUDE REGEX "/OpalInstrument\\.cpp$")
list(FILTER PICOTRACKER_SHARED_CPP EXCLUDE REGEX "/Trace\\.cpp$")
list(FILTER PICOTRACKER_SHARED_CPP EXCLUDE REGEX "/GUI(Bitmap|ParentGraphics)\\.cpp$")
add_library(picotracker_full_compile_probe OBJECT EXCLUDE_FROM_ALL
  ${PICOTRACKER_SHARED_CPP})
target_compile_definitions(picotracker_full_compile_probe PRIVATE ADV HOST_TEST)
target_compile_options(picotracker_full_compile_probe PRIVATE
  "-Dsection(x)=unused" -Wno-deprecated-declarations)
if(APPLE AND PICOTRACKER_APPLE_STRLEN_WORKAROUND)
  target_compile_definitions(picotracker_full_compile_probe PRIVATE
    "strlen=__builtin_strlen")
else()
  target_compile_options(picotracker_full_compile_probe PRIVATE -include cstring)
endif()
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  target_compile_options(picotracker_full_compile_probe PRIVATE
    -Wno-inconsistent-missing-override)
endif()
target_include_directories(picotracker_full_compile_probe PRIVATE
  platform/desktop/include
  ${UPSTREAM}
  ${UPSTREAM}/config
  ${UPSTREAM}/Externals/etl/include
  ${UPSTREAM}/Foundation/Variables
  ${UPSTREAM}/Application/Model
  ${UPSTREAM}/Application/Utils
  ${UPSTREAM}/Application/Views
  ${UPSTREAM}/Application/Views/BaseClasses
  ${UPSTREAM}/Services/Midi
  ${UPSTREAM}/System/Console
  ${UPSTREAM}/Externals/SRC/include
)

add_executable(picotracker_desktop
  platform/desktop/main.cpp
  platform/desktop/SDLWindow.cpp
  platform/desktop/HostDisplayGeometry.cpp
  platform/desktop/HostSystem.cpp
  platform/desktop/HostTrace.cpp
  platform/desktop/HostFileSystem.cpp
  platform/desktop/HostTimer.cpp
  platform/desktop/HostEvent.cpp
  platform/desktop/HostQuitDialog.cpp
  platform/desktop/HostBattery.cpp
  platform/desktop/HostVolume.cpp
  platform/desktop/HostVolumeOverlay.cpp
  platform/desktop/HostPlatform.cpp
  platform/desktop/HostRecord.cpp
  platform/desktop/HostDisplay.cpp
  platform/desktop/HostFirmwareInfo.cpp
  platform/desktop/HostUSB.cpp
  platform/desktop/HostNanoprintf.cpp
  ${UPSTREAM}/Adapters/adv/system/assets/error_bitmaps.c
  $<TARGET_OBJECTS:picotracker_full_compile_probe>
  $<TARGET_OBJECTS:picotracker_service_probe>
)
target_compile_definitions(picotracker_desktop PRIVATE ADV HOST_TEST)
target_compile_options(picotracker_desktop PRIVATE
  "-Dsection(x)=unused" -Wno-deprecated-declarations)
target_include_directories(picotracker_desktop BEFORE PRIVATE
  platform/desktop/include
  ${UPSTREAM}
  ${UPSTREAM}/config
  ${UPSTREAM}/Externals/etl/include
  ${UPSTREAM}/Foundation/Variables
  ${UPSTREAM}/Application/Model
  ${UPSTREAM}/Application/Utils
  ${UPSTREAM}/Application/Views
  ${UPSTREAM}/System/Console
  ${UPSTREAM}/Externals/SRC/include
)
add_library(picotracker_braids STATIC
  ${UPSTREAM}/Externals/braids/macro_oscillator.cc
  ${UPSTREAM}/Externals/braids/analog_oscillator.cc
  ${UPSTREAM}/Externals/braids/digital_oscillator.cc
  ${UPSTREAM}/Externals/braids/resources.cc
  ${UPSTREAM}/Externals/braids/settings.cc
  ${UPSTREAM}/Externals/braids/random.cc
  ${UPSTREAM}/Externals/braids/quantizer.cc
)
target_include_directories(picotracker_braids PRIVATE
  ${UPSTREAM}/Externals/braids ${UPSTREAM})

add_library(picotracker_samplerate STATIC
  ${UPSTREAM}/Externals/SRC/samplerate.c
  ${UPSTREAM}/Externals/SRC/src_linear.c
  ${UPSTREAM}/Externals/SRC/src_sinc.c
)
target_compile_definitions(picotracker_samplerate PRIVATE HAVE_STDBOOL_H
  ENABLE_SINC_FAST_CONVERTER ENABLE_SINC_MEDIUM_CONVERTER
  PACKAGE="libsamplerate" VERSION="0.2.2")
target_include_directories(picotracker_samplerate PRIVATE
  ${UPSTREAM}/Externals/SRC/include)

add_library(picotracker_xml STATIC
  ${UPSTREAM}/Externals/TinyXML2/tinyxml2_printer_only.cpp
  ${UPSTREAM}/Externals/TinyXML2/tinyxml2adapter.cpp
  ${UPSTREAM}/Externals/yxml/yxml.c
)
target_include_directories(picotracker_xml PRIVATE
  ${UPSTREAM} ${UPSTREAM}/Externals/TinyXML2)

if(APPLE)
  # macOS backend: CoreMIDI devices plus the composition that installs them.
  target_sources(picotracker_desktop PRIVATE
    platform/macos/CoreMidiIn.cpp
    platform/macos/CoreMidiOut.cpp
    platform/macos/MacBackend.cpp
  )
  target_include_directories(picotracker_desktop PRIVATE
    platform/desktop platform/macos)
  target_link_libraries(picotracker_desktop PRIVATE
    "-framework CoreMIDI" "-framework CoreFoundation")
endif()

if(WIN32 AND TARGET SDL2::SDL2main)
  target_link_libraries(picotracker_desktop PRIVATE SDL2::SDL2main)
endif()
target_link_libraries(picotracker_desktop PRIVATE SDL2::SDL2
  picotracker_braids picotracker_samplerate picotracker_xml)

if(APPLE)
  # Local .app bundle (ADR 0015): cmake --build <dir> --target picotracker_macos_app
  set(PICOTRACKER_BUNDLE_VERSION "0.1.0")
  if(CMAKE_OSX_DEPLOYMENT_TARGET)
    set(CMAKE_OSX_DEPLOYMENT_TARGET_OR_DEFAULT "${CMAKE_OSX_DEPLOYMENT_TARGET}")
  else()
    set(CMAKE_OSX_DEPLOYMENT_TARGET_OR_DEFAULT "11.0")
  endif()
  configure_file(packaging/macos/Info.plist.in
    ${CMAKE_BINARY_DIR}/packaging/Info.plist @ONLY)
  add_custom_target(picotracker_macos_app
    COMMAND ${CMAKE_SOURCE_DIR}/packaging/macos/make-app.sh
      $<TARGET_FILE:picotracker_desktop> ${CMAKE_BINARY_DIR}/bundle
      ${CMAKE_BINARY_DIR}/packaging/Info.plist
      ${PICOTRACKER_UPSTREAM_ROOT} ${CMAKE_SOURCE_DIR}
    DEPENDS picotracker_desktop
    VERBATIM)
endif()

include(cmake/linux.cmake)

# ADR 0016 section 4: SDL joystick source for the host event loop.
foreach(target picotracker_desktop host_event_check)
  target_sources(${target} PRIVATE
    platform/desktop/SDLJoystickSource.cpp
    platform/desktop/HostInputMap.cpp
    platform/desktop/HostInputProfile.cpp)
endforeach()

# ADR 0018: host audio capture adapter and its check.
add_executable(host_audio_capture_check
  tests/host_audio_capture_check.cpp
  platform/desktop/HostAudioCapture.cpp
)
target_include_directories(host_audio_capture_check PRIVATE platform/desktop)
target_link_libraries(host_audio_capture_check PRIVATE SDL2::SDL2)
target_sources(picotracker_desktop PRIVATE platform/desktop/HostAudioCapture.cpp)

# Host recording (record.h) and its check. The check links the full
# application graph so a recording loads back through the real sample pool.
add_executable(host_record_check
  tests/host_record_check.cpp
  platform/desktop/HostRecord.cpp
  platform/desktop/HostAudioCapture.cpp
  platform/desktop/SDLWindow.cpp
  platform/desktop/HostDisplayGeometry.cpp
  platform/desktop/HostSystem.cpp
  platform/desktop/HostTrace.cpp
  platform/desktop/HostFileSystem.cpp
  platform/desktop/HostTimer.cpp
  platform/desktop/HostEvent.cpp
  platform/desktop/HostQuitDialog.cpp
  platform/desktop/HostBattery.cpp
  platform/desktop/HostVolume.cpp
  platform/desktop/HostVolumeOverlay.cpp
  platform/desktop/HostPlatform.cpp
  platform/desktop/HostDisplay.cpp
  platform/desktop/HostFirmwareInfo.cpp
  platform/desktop/HostUSB.cpp
  platform/desktop/HostNanoprintf.cpp
  platform/desktop/SDLJoystickSource.cpp
  platform/desktop/HostInputMap.cpp
  platform/desktop/HostInputProfile.cpp
  ${UPSTREAM}/Adapters/adv/system/assets/error_bitmaps.c
  $<TARGET_OBJECTS:picotracker_full_compile_probe>
  $<TARGET_OBJECTS:picotracker_service_probe>
)
target_compile_definitions(host_record_check PRIVATE ADV HOST_TEST)
target_compile_options(host_record_check PRIVATE
  "-Dsection(x)=unused" -Wno-deprecated-declarations)
target_include_directories(host_record_check BEFORE PRIVATE
  platform/desktop/include
  platform/desktop
  ${UPSTREAM}
  ${UPSTREAM}/config
  ${UPSTREAM}/Externals/etl/include
  ${UPSTREAM}/Foundation/Variables
  ${UPSTREAM}/Application/Model
  ${UPSTREAM}/Application/Utils
  ${UPSTREAM}/Application/Views
  ${UPSTREAM}/System/Console
  ${UPSTREAM}/Externals/SRC/include
)
if(APPLE)
  target_sources(host_record_check PRIVATE
    platform/macos/CoreMidiIn.cpp
    platform/macos/CoreMidiOut.cpp
    platform/macos/MacBackend.cpp)
  target_include_directories(host_record_check PRIVATE platform/macos)
  target_link_libraries(host_record_check PRIVATE
    "-framework CoreMIDI" "-framework CoreFoundation")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  target_sources(host_record_check PRIVATE platform/linux/LinuxBackend.cpp)
  target_include_directories(host_record_check PRIVATE platform/linux)
  target_compile_definitions(host_record_check PRIVATE PICOTRACKER_LINUX_BACKEND)
endif()
target_link_libraries(host_record_check PRIVATE SDL2::SDL2
  picotracker_braids picotracker_samplerate picotracker_xml)
