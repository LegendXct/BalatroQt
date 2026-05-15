cmake_minimum_required(VERSION 3.19)
project(BalatroQt LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Widgets OpenGL OpenGLWidgets)

qt_standard_project_setup()

qt_add_executable(BalatroQt
    WIN32 MACOSX_BUNDLE
    main.cpp
    src/ui/mainwindow.cpp src/ui/mainwindow.h src/ui/mainwindow.ui
    src/utils/constants.h
    src/utils/shadereffects.h src/utils/shadereffects.cpp
    src/utils/gpueffectsrenderer.h src/utils/gpueffectsrenderer.cpp
    src/card/carddata.h src/card/carddata.cpp
    src/card/joker.h src/card/joker.cpp
    src/game/deck.h src/game/deck.cpp
    src/game/handevaluator.h src/game/handevaluator.cpp
    src/game/gamestate.h src/game/gamestate.cpp
    src/card/carditem.h src/card/carditem.cpp
    src/ui/roundendoverlay.h src/ui/roundendoverlay.cpp
    src/card/jokeritem.h src/card/jokeritem.cpp
    src/game/shop.h src/game/shop.cpp
    src/game/tag.h src/game/tag.cpp
    src/ui/shopwidget.h src/ui/shopwidget.cpp
    src/game/bossblind.h src/game/bossblind.cpp
    src/card/consumable.h src/card/consumable.cpp
    src/card/consumableitem.h src/card/consumableitem.cpp
    src/game/boosterpack.h src/game/boosterpack.cpp
    src/ui/packopenwidget.h src/ui/packopenwidget.cpp
    src/ui/blindselectwidget.h src/ui/blindselectwidget.cpp
    src/ui/floatingscore.h src/ui/floatingscore.cpp
    src/ui/shopsignwidget.h src/ui/shopsignwidget.cpp
    src/ui/deckviewwidget.h src/ui/deckviewwidget.cpp
    src/ui/dynamicbackgrounditem.h src/ui/dynamicbackgrounditem.cpp
    src/ui/splashshaderoverlay.h src/ui/splashshaderoverlay.cpp
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
        Qt6::OpenGL
        Qt6::OpenGLWidgets
)

qt_add_resources(BalatroQt "textures"
    PREFIX "/textures"
    BASE "resources"
    FILES
        resources/images/8BitDeck.png
        resources/images/Enhancers.png
        resources/images/Jokers.png
        resources/images/Tarots.png
        resources/images/boosters.png
        resources/images/BlindChips.png
        resources/images/chips.png
        resources/images/tags.png
        resources/images/ui_assets.png
        resources/images/icons.png
        resources/images/balatro.png
        resources/images/ShopSignAnimation.png
        resources/images/Vouchers.png
)


qt_add_resources(BalatroQt "shader_sources"
    PREFIX "/shaders"
    BASE "resources"
    FILES
        resources/shaders/CRT.fs
        resources/shaders/background.fs
        resources/shaders/booster.fs
        resources/shaders/debuff.fs
        resources/shaders/dissolve.fs
        resources/shaders/flame.fs
        resources/shaders/flash.fs
        resources/shaders/foil.fs
        resources/shaders/gold_seal.fs
        resources/shaders/holo.fs
        resources/shaders/hologram.fs
        resources/shaders/negative.fs
        resources/shaders/negative_shine.fs
        resources/shaders/played.fs
        resources/shaders/polychrome.fs
        resources/shaders/skew.fs
        resources/shaders/splash.fs
        resources/shaders/vortex.fs
        resources/shaders/voucher.fs
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
