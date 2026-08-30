cmake_minimum_required(VERSION 3.16)

project(898SimpleMidiPiano VERSION 0.1 LANGUAGES CXX)

# --- Sprawdzenie wsparcia dla LTO (Link Time Optimization) ---
include(CheckIPOSupported)
check_ipo_supported(RESULT lto_supported OUTPUT error)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets Multimedia WebEngineWidgets)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets Multimedia WebEngineWidgets)

# PkgConfig i FluidSynth są wymagane głównie na Linuksie/Unixie
if(UNIX AND NOT APPLE)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(FLUIDSYNTH REQUIRED fluidsynth)
else()
    # Dla Windowsa ustawiamy puste zmienne, żeby CMake nie zgłaszał błędów
    set(FLUIDSYNTH_LIBRARIES "")
    set(FLUIDSYNTH_INCLUDE_DIRS "")
    set(FLUIDSYNTH_CFLAGS_OTHER "")
endif()

# --- Integracja Verovio jako statycznej biblioteki podprojektu ---
set(BUILD_AS_LIBRARY ON CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE) # Wymuszenie biblioteki statycznej unika błędów LNK2001 na MSVC
add_subdirectory(verovio-develop/cmake)

set(PROJECT_SOURCES
    main.cpp
    mainwindow.cpp
    mainwindow.h
    mainwindow.ui
    RtMidi.h
    RtMidi.cpp
    MidiInput.h
    MidiInput.cpp
    resources.qrc
    verovioscorewidget.h
    verovioscorewidget.cpp
)

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(898SimpleMidiPiano MANUAL_FINALIZATION ${PROJECT_SOURCES})
else()
    if(ANDROID)
        add_library(898SimpleMidiPiano SHARED ${PROJECT_SOURCES})
    else()
        add_executable(898SimpleMidiPiano ${PROJECT_SOURCES})
    endif()
endif()

# --- Optymalizacje tylko dla Release ---
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    message(STATUS "Konfiguracja optymalizacji dla Release...")

    # 1. Optymalizacja kodu pod kątem rozmiaru (-Os) i usuwanie nieużywanego kodu
    if(NOT MSVC)
        target_compile_options(898SimpleMidiPiano PRIVATE -Os -ffunction-sections -fdata-sections)
        target_link_options(898SimpleMidiPiano PRIVATE -Wl,--gc-sections)
    endif()

    # 2. Włączenie LTO (Link Time Optimization)
    if(lto_supported)
        set_target_properties(898SimpleMidiPiano PROPERTIES INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE)
    endif()

    # 3. Automatyczny 'strip' po kompilacji (tylko Linux/macOS)
    if(UNIX AND NOT APPLE)
        add_custom_command(TARGET 898SimpleMidiPiano POST_BUILD
            COMMAND ${CMAKE_STRIP} --strip-unneeded "$<TARGET_FILE:898SimpleMidiPiano>"
            COMMENT "Stripping binary for smaller size..."
        )
    endif()
endif()

target_include_directories(898SimpleMidiPiano PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${FLUIDSYNTH_INCLUDE_DIRS}
    "${CMAKE_CURRENT_SOURCE_DIR}/verovio-develop/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/verovio-develop/include/tuning-library"
    "${CMAKE_CURRENT_SOURCE_DIR}/verovio-develop/include/json"
    "${CMAKE_CURRENT_SOURCE_DIR}/verovio-develop/include/vrv"
    "${CMAKE_CURRENT_SOURCE_DIR}/verovio-develop/include/pugi"
    "${CMAKE_CURRENT_SOURCE_DIR}/verovio-develop/libmei"
    "${CMAKE_CURRENT_SOURCE_DIR}/verovio-develop/libmei/addons"
    "${CMAKE_CURRENT_SOURCE_DIR}/verovio-develop/libmei/dist"
    "${CMAKE_CURRENT_SOURCE_DIR}/verovio-develop/src/json"
    "${CMAKE_CURRENT_SOURCE_DIR}/verovio-develop/src/pugi"
)

# Wskazanie konsolidatorowi lokalizacji wygenerowanych plików Verovio dla Visual Studio (Release/Debug)
target_link_directories(898SimpleMidiPiano PRIVATE
    "${CMAKE_CURRENT_BINARY_DIR}/verovio-develop/cmake/$<CONFIG>"
)

target_compile_options(898SimpleMidiPiano PRIVATE ${FLUIDSYNTH_CFLAGS_OTHER})

target_link_libraries(898SimpleMidiPiano PRIVATE
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Multimedia
    Qt${QT_VERSION_MAJOR}::WebEngineWidgets
    ${FLUIDSYNTH_LIBRARIES}
    verovio
)

if(WIN32)
    target_link_libraries(898SimpleMidiPiano PRIVATE winmm)
    target_compile_definitions(898SimpleMidiPiano PRIVATE __WINDOWS_MM__)
elseif(APPLE)
    target_link_libraries(898SimpleMidiPiano PRIVATE "-framework CoreMIDI" "-framework CoreAudio" "-framework CoreFoundation")
    target_compile_definitions(898SimpleMidiPiano PRIVATE __MACOSX_CORE__)
elseif(UNIX)
    target_link_libraries(898SimpleMidiPiano PRIVATE asound pthread)
    target_compile_definitions(898SimpleMidiPiano PRIVATE __LINUX_ALSA__)
endif()

if(${QT_VERSION} VERSION_LESS 6.1.0)
    set(BUNDLE_ID_OPTION MACOSX_BUNDLE_GUI_IDENTIFIER com.example.898SimpleMidiPiano)
endif()

set_target_properties(898SimpleMidiPiano PROPERTIES
    ${BUNDLE_ID_OPTION}
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

include(GNUInstallDirs)
install(TARGETS 898SimpleMidiPiano
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(898SimpleMidiPiano)
endif()
