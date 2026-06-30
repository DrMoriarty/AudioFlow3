cmake_minimum_required(VERSION 3.19)
project(AudioFlow3 LANGUAGES CXX)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Widgets LinguistTools)

qt_standard_project_setup()

qt_add_executable(AudioFlow3
    WIN32 MACOSX_BUNDLE
    main.cpp
    mainwindow.cpp
    mainwindow.h
    mainwindow.ui
    collapsibleblock.cpp
    collapsibleblock.h
    knobwidget.cpp
    knobwidget.h
    toggleswitch.cpp
    toggleswitch.h
        src/audioflow.cpp
        src/audioflow.h
        src/permission.mm
    src/processing.cpp
    src/processing.h
    src/fileutils/globals.cpp
    src/fileutils/globals.h
    src/fileutils/config.cpp
    src/fileutils/config.h
    src/fileutils/readIRFile.cpp
    src/fileutils/readIRFile.h
    src/processing/audioProcessor.cpp
    src/processing/audioProcessor.h
    src/processing/amplifier.cpp
    src/processing/amplifier.h
    src/processing/convolutionReverb.cpp
    src/processing/convolutionReverb.h
    src/processing/equalizer.cpp
    src/processing/equalizer.h
    src/processing/iirFilter.cpp
    src/processing/iirFilter.h
    src/processing/smoother.cpp
    src/processing/smoother.h
    AudioFlow3.qrc
)

set_target_properties(AudioFlow3 PROPERTIES
    MACOSX_BUNDLE_INFO_FILE ${CMAKE_SOURCE_DIR}/Info.plist
    XCODE_ATTRIBUTE_CODE_SIGN_ENTITLEMENTS ${CMAKE_SOURCE_DIR}/AudioFlow3.entitlements
)

qt_add_translations(
    TARGETS AudioFlow3
    TS_FILES AudioFlow3_en_001.ts
)

find_library(COREAUDIO_LIB CoreAudio)
find_library(ACCELERATE_LIB Accelerate)
find_library(AVFOUNDATION_LIB AVFoundation)
find_library(FOUNDATION_LIB Foundation)

target_link_libraries(AudioFlow3
    PRIVATE
        Qt::Core
        Qt::Widgets
        ${COREAUDIO_LIB}
        ${ACCELERATE_LIB}
        ${AVFOUNDATION_LIB}
        ${FOUNDATION_LIB}
)

install(TARGETS AudioFlow3
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

add_custom_command(TARGET AudioFlow3 POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
        ${CMAKE_SOURCE_DIR}/Info.plist
        $<TARGET_BUNDLE_CONTENT_DIR:AudioFlow3>/Info.plist
    COMMENT "Copying custom Info.plist to app bundle"
)

add_custom_command(TARGET AudioFlow3 POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
        ${CMAKE_SOURCE_DIR}/icon.icns
        $<TARGET_BUNDLE_CONTENT_DIR:AudioFlow3>/Resources/icon.icns
    COMMENT "Copying application icon to app bundle"
)

add_custom_command(TARGET AudioFlow3 POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/assets
        $<TARGET_BUNDLE_CONTENT_DIR:AudioFlow3>/Resources
    COMMENT "Copying assets to app bundle Resources"
)

qt_generate_deploy_app_script(
    TARGET AudioFlow3
    OUTPUT_SCRIPT deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR
)
install(SCRIPT ${deploy_script})
