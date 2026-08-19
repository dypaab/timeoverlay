cmake_minimum_required(VERSION 3.20)
# 2.3.0 : bibliotheque de programmes (enregistrement automatique, historique),
#         invite d'enregistrement en quittant, lancement au demarrage de la
#         session, aide integree, overlay deplacable sous Wayland.
project(TimeOverlay VERSION 2.3.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Windows-specific
if(WIN32)
    set(CMAKE_WIN32_EXECUTABLE TRUE)
    add_compile_definitions(UNICODE _UNICODE NOMINMAX)
endif()

# Les sources contiennent des caracteres accentues dans les libelles de
# l'interface. GCC et Clang supposent l'UTF-8 par defaut, mais MSVC lit les
# fichiers dans la page de codes locale et corromprait les accents.
if(MSVC)
    add_compile_options(/utf-8)
endif()

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Multimedia)

add_subdirectory(src)

# Test de fumee sans interface graphique. Active par defaut : il tourne en
# quatre secondes et verifie le comportement dont depend la regie.
option(TIMEOVERLAY_BUILD_TESTS "Compiler le test de fumee" ON)
if(TIMEOVERLAY_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# ---------------------------------------------------------------- install

if(UNIX AND NOT APPLE)
    include(GNUInstallDirs)

    # GNUInstallDirs deduit le dossier de documentation du nom du projet, ce
    # qui donnerait /usr/share/doc/TimeOverlay. La convention Debian veut le
    # nom du paquet, en minuscules.
    set(CMAKE_INSTALL_DOCDIR "${CMAKE_INSTALL_DATADIR}/doc/timeoverlay")

    install(TARGETS TimeOverlay RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

    install(FILES TimeOverlay.desktop
            DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)

    install(FILES resources/TimeOverlay.png
            DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/256x256/apps)

    # Script OBS et programme d'exemple : l'utilisateur doit pouvoir les
    # retrouver apres une installation par paquet.
    install(FILES obs/TimeOverlay_Helper.lua example_culte.timerproject
            DESTINATION ${CMAKE_INSTALL_DATADIR}/timeoverlay)

    install(FILES README.md
            DESTINATION ${CMAKE_INSTALL_DOCDIR})

    # ------------------------------------------------------------ paquet .deb
    #
    # Genere avec : cpack -G DEB
    # Les dependances Qt sont deduites automatiquement des bibliotheques
    # reellement liees, plutot que listees a la main : une dependance oubliee
    # produirait un paquet qui s'installe mais ne demarre pas.

    set(CPACK_PACKAGE_NAME "timeoverlay")
    set(CPACK_PACKAGE_VENDOR "TimeOverlay")
    set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
    set(CPACK_PACKAGE_DESCRIPTION_SUMMARY
        "Chronometrage pour la diffusion en direct (OBS)")
    set(CPACK_PACKAGE_HOMEPAGE_URL "")
    set(CPACK_PACKAGE_CONTACT "TimeOverlay <timeoverlay@localhost>")

    set(CPACK_DEBIAN_PACKAGE_SECTION "video")
    set(CPACK_DEBIAN_PACKAGE_PRIORITY "optional")
    set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)

    # dpkg-shlibdeps ne voit que les bibliotheques liees au binaire. Le
    # greffon d'affichage de Qt est charge dynamiquement a l'execution : il
    # echappe a la detection automatique, et libqt6gui6 ne le tire pas non
    # plus. Sans lui, l'application meurt au demarrage avec
    # "Could not find the Qt platform plugin xcb". Il doit donc etre declare
    # a la main.
    set(CPACK_DEBIAN_PACKAGE_DEPENDS "qt6-qpa-plugins")

    # Sessions Wayland : Qt passe sinon par XWayland, ce qui fonctionne mais
    # affiche un avertissement et rend le placement des fenetres moins fiable.
    set(CPACK_DEBIAN_PACKAGE_RECOMMENDS "qt6-wayland")
    set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
    set(CPACK_DEBIAN_PACKAGE_DESCRIPTION
"Chronometrage pour la diffusion en direct
 Ecrit l'heure, un compte a rebours et un chronometre dans des fichiers
 texte lus par OBS. Concu pour la regie de culte : minuter chaque moment
 du programme et signaler a l'intervenant qu'il doit conclure.
 .
 Quand le compte a rebours atteint zero, un fichier distinct prend le
 relais et compte le depassement, ce qui permet de lui donner sa propre
 mise en forme dans OBS.")

    include(CPack)
endif()
