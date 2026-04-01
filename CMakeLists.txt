cmake_minimum_required(VERSION 3.19)
project(BalatroQt LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Widgets)
find_package(Qt6 REQUIRED COMPONENTS Core)

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
)

target_link_libraries(BalatroQt
    PRIVATE
        Qt::Core
        Qt::Widgets
)
target_link_libraries(BalatroQt PRIVATE Qt6::Core)

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
