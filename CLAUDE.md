# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project

BalatroQt is a Qt6/C++17 reimplementation of the card-roguelike *Balatro*. It is a single desktop executable built with CMake + Qt Widgets + QtOpenGL. There is no test suite, no linter, and no CI — building the target with CMake is the only verification step.

The Lua source of the original game lives in `balatro_original_code/` and is **reference material only** — it is not compiled, not in the resource bundle, and is excluded from `CMakeLists.txt`. Read it when you need to confirm original-game behavior (scoring formulas, joker effects, shop rates, etc.), but never edit it as part of a feature.

## Build

Qt 6.5+ is required (`Core Widgets OpenGL OpenGLWidgets`). The active build tree is `build/Desktop_Qt_6_10_0_MinGW_64_bit-Debug/` (Ninja, MinGW). Configure/build from PowerShell:

```powershell
cmake -S . -B build/Desktop_Qt_6_10_0_MinGW_64_bit-Debug -G Ninja
cmake --build build/Desktop_Qt_6_10_0_MinGW_64_bit-Debug
build\Desktop_Qt_6_10_0_MinGW_64_bit-Debug\BalatroQt.exe
```

`main.cpp` calls `showFullScreen()`; on a dev machine you may want to change that locally while iterating on UI.

## Adding source files

`CMakeLists.txt` enumerates every `.h`/`.cpp` explicitly — there is no glob. **Any new source file must be added to the `qt_add_executable(BalatroQt ...)` list.** Likewise, every shipped image / shader / font is listed under one of the three `qt_add_resources` blocks (`textures`, `shader_sources`, `fonts`); new assets need both a file in `resources/` and a line in that block. The `resources/resources.qrc` file mirrors the textures/fonts (but not shaders) — keep it in sync.

Resources are accessed as `:/textures/images/<name>.png`, `:/shaders/shaders/<name>.fs`, `:/fonts/fonts/<name>.ttf`.

## Architecture

Three layers, each its own directory under `src/`, with strict dependency direction `ui → game → card → utils`:

- `src/card/` — Pure data + per-entity `QGraphicsObject` views.
  - `carddata.h` `CardData` is the value type: `Suit`, `Rank`, `Enhancement` (Bonus/Mult/Wild/Glass/Steel/Stone/Gold/Lucky), `Edition` (Foil/Holographic/Polychrome/Negative), `Seal` (Gold/Red/Blue/Purple), debuff flag, permanent bonus chips, and a process-unique `uid` for animation tracking.
  - `joker.h`/`joker.cpp` defines `JokerType` (50+ jokers), the `TriggerTiming` taxonomy (`OnScoringCard`, `OnPlayedHand`, `OnDiscard`, `OnRoundEnd`, `Passive`), and the `JokerEffect = std::function<void(TriggerContext&)>` callback model. Each `Joker` carries its own effect lambda + name/description/sellValue/counter.
  - `consumable.*` — Tarots, Planets, Spectrals share the `Consumable` type.
  - `carditem` / `jokeritem` / `consumableitem` are the corresponding `QGraphicsObject` views with hover-tilt, drag, juice, and flip animations. They each have a static `loadResources()` called once from `main.cpp` before `MainWindow` construction.

- `src/game/` — Pure model. **No Qt UI dependencies** beyond `QObject`/signals. `GameState` (in `gamestate.h`) is the single source of truth; the UI never mutates game state directly, only via member functions, and reacts to its signals (`handChanged`, `scoreChanged`, `goldChanged`, `roundWon`, `gameOver`, `handPlayed`, `endRoundCardTriggered`, `jokersChanged`, `shopChanged`, `handLevelsChanged`, `consumablesChanged`, `blindSelectEntered`, `blindStarted`).
  - `Deck` holds the player's 52-card pool and draws into the hand.
  - `HandEvaluator::evaluate` returns a `HandResult { type, scoringCards, chips, mult, xmult, level, name, baseChips, baseMult, events }`. Scoring is **event-driven**: each chip/mult contribution becomes a `ScoreEvent` (kinds: `ScoringCardChip`, `EnhancementMult/XMult`, `EditionChip/Mult/XMult`, `SteelXMult`, `JokerChip/Mult/XMult`, `DollarGain`, `RedSealRetrigger`, `JokerRetrigger`, `GlassShatter`, `BlueSealPlanet`). The UI replays these events as animations in order, so new scoring effects must emit a `ScoreEvent` rather than silently bumping a number.
  - `Shop` rolls offers of `OfferKind` (Joker/Tarot/Planet/Spectral/PlayingCard/Pack/Voucher) into three slot rows (`mShopOffers`, `mVoucherOffers`, `mBoosterOffers`). Vouchers (`VoucherType`) mutate shop parameters (slot count, rates, discount, reroll cost, joker edition rate) via setters on `Shop` and tracked extras on `GameState` (`mExtraConsumableSlots`, `mExtraHandSize`, etc.).
  - `BossBlind` enumerates boss-blind debuffs and their effects (`applyBossDebuffs`, `applyBossPostPlay`, `bossBlocksPlayedHand`).
  - `Tag` handles skip-tag rewards.
  - `BoosterPack` and `PackContent` describe the booster pack pool.
  - Tunables live in `src/utils/constants.h` (hand size, initial gold/hands/discards, joker/consumable slots, blind multipliers, base chips+mult per `HandType`). Edit these instead of hard-coding new values.

- `src/ui/` — `QGraphicsScene`-based view. `MainWindow` (~3.9k LOC) wires every game signal to a scene update and owns the scene-graph items, animations, and overlays. The play screen, blind-select, shop, pack-open, deck-view, round-end, splash, and options menu are all sibling widgets/overlays driven by `setContextPage()` / `GamePhase` transitions. Animation timing is choreographed across many `QPropertyAnimation`s and `QTimer`s, so when changing a scoring step also check that `playScoreEvent`, `animateScoreTotalThenFinalize`, `animatePlayedCardsToDiscardThen`, and the various `mLastJokerDragTo`/`mLastHandCardDragTo` guards still line up.

- `src/utils/` — `constants.h` (gameplay tunables, do not duplicate elsewhere), `shadereffects.*` (CPU fallback), and `gpueffectsrenderer.*` (GPU path).

## GPU shader effects

Card editions (Foil/Holo/Polychrome/Negative), card debuff, gold seal, splash, dynamic background, booster, played-card glow, dissolve, flame, vortex, voucher, hologram, and CRT are all GLSL fragment shaders in `resources/shaders/*.fs`. `BalatroShaders::renderEditionPixmapGpu` / `renderShaderPixmapGpu` (in `gpueffectsrenderer.cpp`) render them in an offscreen FBO and hand a `QPixmap` back to Qt Widgets. If GL init fails, **callers are expected to fall back to the CPU path in `shadereffects.*`** — keep both paths in sync when adding a new edition.

The flame intensity on the chips/mult tiles is driven each tick by `mDisplayedChips × mDisplayedMult` versus `mTargetScore` (see `updateFlameIntensity`), not by a one-shot bool. Comments in `mainwindow.h` (around `mChipFlame` / `mMultFlame`) note this matches the original `log5` formula — preserve that behavior when touching scoring animations.

`MainWindow` notes a known issue (line ~290 of `mainwindow.h`): native `QDialog` over a fullscreen `QOpenGLWidget` scene causes a black-screen rebuild on some drivers. The options menu therefore uses an in-scene overlay (`mOptionsOverlay` / `showOptionsOverlay`) instead of a `QDialog`. Apply the same pattern for any new modal.

## Conventions

- Comments and identifiers throughout the codebase mix English and Simplified Chinese (Chinese for gameplay term annotations, e.g. `// 出牌`, `// 小丑牌槽位数`). Match the surrounding style when editing a file.
- The fonts `m6x11plus.ttf` (pixel UI font) and `NotoSansSC-Bold.ttf` (Chinese fallback) are loaded in `MainWindow::loadFonts()` and stored as `mPixelFont` / `mCNFont`. Reuse those rather than constructing new `QFont` instances.
- Scoring events: when adding a new joker or enhancement effect, push a `ScoreEvent` into `HandResult::events` so the UI can animate it; do not mutate `chips`/`mult` silently.
- Card identity: animations track cards by `CardData::uid`, not by index. Preserve `uid` across moves and call `assignNewUid()` only when genuinely creating a new card (e.g. DNA copy).
