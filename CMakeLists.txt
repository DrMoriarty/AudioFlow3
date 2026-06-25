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
    AudioFlow3.qrc
)

qt_add_translations(
    TARGETS AudioFlow3
    TS_FILES AudioFlow3_en_001.ts
)

target_link_libraries(AudioFlow3
    PRIVATE
        Qt::Core
        Qt::Widgets
)

install(TARGETS AudioFlow3
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

qt_generate_deploy_app_script(
    TARGET AudioFlow3
    OUTPUT_SCRIPT deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR
)
install(SCRIPT ${deploy_script})
