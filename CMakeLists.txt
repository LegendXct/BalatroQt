cmake_minimum_required(VERSION 3.19)
project(BalatroQt LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Widgets)
find_package(Qt6 REQUIRED COMPONENTS Widgets)
find_package(Qt6 REQUIRED COMPONENTS Core)
find_package(Qt6 REQUIRED COMPONENTS Widgets)

qt_standard_project_setup()

qt_add_executable(BalatroQt
    WIN32 MACOSX_BUNDLE
    main.cpp
    src/ui/mainwindow.cpp src/ui/mainwindow.h src/ui/mainwindow.ui
    src/utils/constants.h
    src/card/carddata.h src/card/carddata.cpp
    src/card/joker.h src/card/joker.cpp
    src/game/deck.h src/game/deck.cpp
    src/game/handevaluator.h src/game/handevaluator.cpp
    src/game/gamestate.h src/game/gamestate.cpp
    src/card/carditem.h src/card/carditem.cpp
    src/ui/roundendoverlay.h src/ui/roundendoverlay.cpp
    src/card/jokeritem.h src/card/jokeritem.cpp
    src/game/shop.h src/game/shop.cpp
    src/ui/shopwidget.h src/ui/shopwidget.cpp
)

target_include_directories(BalatroQt
    PRIVATE
        src/card
        src/game
        src/ui
        src/utils
)
target_link_libraries(BalatroQt
    PRIVATE
        Qt6::Core
        Qt6::Widgets
)
target_link_libraries(BalatroQt PRIVATE Qt6::Widgets)
target_link_libraries(BalatroQt PRIVATE Qt6::Core)
target_link_libraries(BalatroQt PRIVATE Qt6::Widgets)

qt_add_resources(BalatroQt "textures"
    PREFIX "/textures"
    BASE "resources"
    FILES
        resources/images/8BitDeck.png
        resources/images/Enhancers.png
        resources/images/Jokers.png
)

qt_add_resources(BalatroQt "fonts"
    PREFIX "/fonts"
    BASE "resources"
    FILES
        resources/fonts/m6x11plus.ttf
        resources/fonts/NotoSansSC-Bold.ttf
)

include(GNUInstallDirs)

install(TARGETS BalatroQt
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

qt_generate_deploy_app_script(
    TARGET BalatroQt
    OUTPUT_SCRIPT deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR
)
install(SCRIPT ${deploy_script})
