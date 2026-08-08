cmake_minimum_required(VERSION 3.16)

set(TS_FILES viMarkdown_ja.ts)

set(PROJECT_SOURCES
        AutoTest.cpp
        DocWidget.cpp
        DocWidget.h
        main.cpp
        MainWindow.cpp
        MainWindow.h
        MainWindow.ui
        MarkdownEditor.cpp
        MarkdownEditor.h
        MarkdownPreview.cpp
        MarkdownPreview.h
        ReplaceDialog.cpp
        ReplaceDialog.h
        ReplaceDialog.ui
        SettingsDialog.cpp
        SettingsDialog.h
        SettingsDialog.ui
        LangDialog.cpp
        LangDialog.h
        LangDialog.ui
        GrepDialog.h
        GrepDialog.cpp
        GrepDialog.ui
        OutputView.h
        OutputView.cpp
        SlideShow.h
        SlideShow.cpp
        vicmd.cpp
        diff.cpp
        UndoMgr.cpp
        CalendarWidget.cpp
        ver.h
        ${TS_FILES}
)

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(viMarkdown
        MANUAL_FINALIZATION
        ${PROJECT_SOURCES}
        MainWindow.qrc
    )
# Define target properties for Android with Qt 6 as:
#    set_property(TARGET viMarkdown APPEND PROPERTY QT_ANDROID_PACKAGE_SOURCE_DIR
#                 ${CMAKE_CURRENT_SOURCE_DIR}/android)
# For more information, see https://doc.qt.io/qt-6/qt-add-executable.html#target-creation

    qt_create_translation(QM_FILES ${CMAKE_SOURCE_DIR} ${TS_FILES})
else()
    if(ANDROID)
        add_library(viMarkdown SHARED
            ${PROJECT_SOURCES}
        )
# Define properties for Android with Qt 5 after find_package() calls as:
#    set(ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/android")
    else()
        add_executable(viMarkdown
            ${PROJECT_SOURCES}
        )
        get_target_property(MY_INC viMarkdown INCLUDE_DIRECTORIES)
        message(STATUS "INCLUDE_DIRS=${MY_INC}")
    endif()


    qt5_create_translation(QM_FILES ${CMAKE_SOURCE_DIR} ${TS_FILES})
endif()

target_include_directories(viMarkdown PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_SOURCE_DIR}/3rdparty/lunasvg/include"
    "${CMAKE_SOURCE_DIR}/3rdparty/lunasvg/3rdparty/plutovg/include"
)

target_link_libraries(viMarkdown PRIVATE
	Qt${QT_VERSION_MAJOR}::Widgets
	Qt${QT_VERSION_MAJOR}::PrintSupport
	Qt${QT_VERSION_MAJOR}::Svg
	lunasvg
)

# Qt for iOS sets MACOSX_BUNDLE_GUI_IDENTIFIER automatically since Qt 6.1.
# If you are developing for iOS or macOS you should consider setting an
# explicit, fixed bundle identifier manually though.
if(${QT_VERSION} VERSION_LESS 6.1.0)
  set(BUNDLE_ID_OPTION MACOSX_BUNDLE_GUI_IDENTIFIER com.example.viMarkdown)
endif()
set_target_properties(viMarkdown PROPERTIES
    ${BUNDLE_ID_OPTION}
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

include(GNUInstallDirs)
install(TARGETS viMarkdown
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(viMarkdown)
endif()
