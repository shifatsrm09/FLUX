cmake_minimum_required(VERSION 3.20)

project(FLUX VERSION 0.0.3 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Find Qt 6 modules
find_package(Qt6 6.5 REQUIRED COMPONENTS
    Core
    Gui
    Qml
    Quick
    QuickControls2
    Network
)

# libVLC configuration
set(VLC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/vlc")
set(VLC_INCLUDE_DIRS
    "${VLC_DIR}/include"
)
set(VLC_LIBRARIES
    "${VLC_DIR}/lib/libvlc.lib"
    "${VLC_DIR}/lib/libvlccore.lib"
)

include_directories(
    src
    ${VLC_INCLUDE_DIRS}
)

# Project executable (Windows GUI subsystem)
qt_add_executable(FLUX WIN32
    src/main.cpp
    src/app/Application.h
    src/app/Application.cpp
    src/media/VLCInstance.h
    src/media/VLCInstance.cpp
    src/media/VLCPlayer.h
    src/media/VLCPlayer.cpp
    src/media/VLCVideoItem.h
    src/media/VLCVideoItem.cpp
    src/models/TestMedia.h
    src/models/TestMedia.cpp
    src/search/SearchResult.h
    src/search/SearchResult.cpp
    src/search/MediaLibrary.h
    src/search/MediaLibrary.cpp
    src/search/SearchManager.h
    src/search/SearchManager.cpp
    src/search/FolderBrowser.h
    src/search/FolderBrowser.cpp
    src/downloads/DownloadManager.h
    src/downloads/DownloadManager.cpp
    src/core/Logger.h
    src/core/Logger.cpp
    src/core/UserStore.h
    src/core/UserStore.cpp
    resources/flux.rc
)

set_target_properties(FLUX PROPERTIES
    WIN32_EXECUTABLE TRUE
)

# Qt Quick / QML Module
qt_add_qml_module(FLUX
    URI FLUX
    VERSION 1.0
    QML_FILES
        qml/Main.qml
        qml/SearchPage.qml
        qml/PlayerView.qml
        qml/components/MediaCard.qml
        qml/components/PlaybackControls.qml
        qml/components/SearchResultItem.qml
        qml/components/SearchResultCard.qml
        qml/components/CategorySelector.qml
        qml/components/ClassicTitleBar.qml
        qml/components/WindowResizeBorders.qml
        qml/components/CategoryCheckpoints.qml
        qml/components/CategoryFilterSection.qml
        qml/components/MediaFormatter.js
        qml/components/Theme.js
        qml/components/FluxIcon.qml
        qml/components/FluxButton.qml
        qml/components/IconButton.qml
        qml/components/FluxToolTip.qml
        qml/components/FluxScrollBar.qml
        qml/components/FluxSpinner.qml
        qml/components/GlowOrb.qml
        qml/components/Equalizer.qml
        qml/components/PosterArt.qml
        qml/components/ToggleSwitch.qml
        qml/components/Breadcrumbs.qml
        qml/components/NextEpisodeCard.qml
        qml/components/ResumeToast.qml
        qml/components/SettingsPanel.qml
        qml/components/DownloadsPanel.qml
        qml/components/FolderPicker.qml
)

target_link_libraries(FLUX PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::Network
    ${VLC_LIBRARIES}
)

# Windows desktop subsystem and runtime DLL handling
if(WIN32)
    # Automatically copy libVLC runtime DLLs and plugins to output dir for running
    set(SYSTEM_VLC_DIR "C:/Program Files/VideoLAN/VLC")
    if(EXISTS "${SYSTEM_VLC_DIR}/libvlc.dll")
        add_custom_command(TARGET FLUX POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${SYSTEM_VLC_DIR}/libvlc.dll"
                "$<TARGET_FILE_DIR:FLUX>"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${SYSTEM_VLC_DIR}/libvlccore.dll"
                "$<TARGET_FILE_DIR:FLUX>"
            COMMAND ${CMAKE_COMMAND} -E make_directory
                "$<TARGET_FILE_DIR:FLUX>/resources"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${CMAKE_CURRENT_SOURCE_DIR}/resources/media_library.json"
                "$<TARGET_FILE_DIR:FLUX>/resources/media_library.json"
            COMMENT "Copying runtime assets and libVLC binaries to build output directory"
        )
    endif()
endif()

# Standalone CLI test for SearchManager
add_executable(flux_test_search
    tests/test_search.cpp
    src/search/SearchResult.h
    src/search/SearchResult.cpp
    src/search/MediaLibrary.h
    src/search/MediaLibrary.cpp
    src/search/SearchManager.h
    src/search/SearchManager.cpp
    src/core/Logger.h
    src/core/Logger.cpp
)
target_link_libraries(flux_test_search PRIVATE
    Qt6::Core
    Qt6::Network
)

# Standalone CLI test for category switching and stale-result prevention
add_executable(flux_test_category_switch
    tests/test_category_switch.cpp
    src/search/SearchResult.h
    src/search/SearchResult.cpp
    src/search/MediaLibrary.h
    src/search/MediaLibrary.cpp
    src/search/SearchManager.h
    src/search/SearchManager.cpp
    src/core/Logger.h
    src/core/Logger.cpp
)
target_link_libraries(flux_test_category_switch PRIVATE
    Qt6::Core
    Qt6::Network
)

# Standalone CLI test for libVLC playback pipeline
add_executable(flux_test_playback
    tests/test_playback.cpp
    src/media/VLCInstance.h
    src/media/VLCInstance.cpp
    src/media/VLCPlayer.h
    src/media/VLCPlayer.cpp
    src/core/Logger.h
    src/core/Logger.cpp
)
target_link_libraries(flux_test_playback PRIVATE
    Qt6::Core
    Qt6::Network
    ${VLC_LIBRARIES}
)
