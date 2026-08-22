# Le coeur metier est isole dans une bibliotheque : l'interface graphique le
# consomme, et les tests peuvent le piloter sans ouvrir de fenetre.
add_library(timeoverlay_core STATIC
    core/Clock.cpp
    core/Timer.cpp
    core/Profile.cpp
    core/PhaseManager.cpp
    core/ColorManager.cpp
    core/OutputEngine.cpp
    core/TextFormatter.cpp
    core/MessageCenter.cpp
    core/ServiceSchedule.cpp
    core/ProgrammeLibrary.cpp
    core/SessionState.cpp

    utils/CSVExporter.cpp
    utils/PathUtils.cpp
    utils/Autostart.cpp
)

target_include_directories(timeoverlay_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(timeoverlay_core PUBLIC
    Qt6::Core
    Qt6::Gui
    Qt6::Multimedia
)

set(TIMEOVERLAY_UI_SOURCES
    main.cpp
    ${CMAKE_SOURCE_DIR}/resources/resources.qrc

    # Alarm depend de QApplication::beep(), donc de QtWidgets : il ne peut pas
    # vivre dans la bibliotheque sans interface, que le test pilote sans
    # ouvrir de fenetre.
    core/Alarm.cpp

    ui/MainWindow.cpp
    ui/OverlayWindow.cpp
    ui/SettingsDialog.cpp
    ui/ProgrammeDialog.cpp
    ui/MessagePanel.cpp
    ui/OutputPathsDialog.cpp
    ui/ProgrammeLibraryDialog.cpp
    ui/HelpDialog.cpp
)

# L'overlay Win32 ne doit etre compile que sous Windows. Il etait auparavant
# compile sur toutes les plateformes, avec un corps entierement neutralise par
# des #ifdef -- du poids mort sous Linux.
if(WIN32)
    list(APPEND TIMEOVERLAY_UI_SOURCES ui/OverlayWindowWin32.cpp)
endif()

add_executable(TimeOverlay ${TIMEOVERLAY_UI_SOURCES})

target_compile_definitions(TimeOverlay PRIVATE
    TIMEOVERLAY_VERSION="${PROJECT_VERSION}")

target_link_libraries(TimeOverlay PRIVATE
    timeoverlay_core
    Qt6::Widgets
)

if(WIN32)
    target_link_libraries(TimeOverlay PRIVATE user32 dwmapi)
endif()
