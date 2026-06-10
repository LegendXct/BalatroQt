# 队列牌组（Queue Deck）Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 新增多态"游戏牌组"体系（基础/队列两种），队列牌组下手牌按抽牌顺序排列、仅前 6 张可选、每回合 +1 出牌 +1 弃牌，开局前经牌组选择界面进入游戏。

**Architecture:** `GameDeckType` 抽象基类 + 派生类（继承/多态/纯虚函数），`GameState` 经 `std::unique_ptr` 持有并在 startBlind/选牌校验/排序锁定处查询；UI 侧新增 `DeckSelectWidget` overlay（项目规矩：不用原生 QDialog），主菜单"开始游戏"→ 选牌组 → 开局。

**Tech Stack:** Qt 6.11 / C++17 / CMake+Ninja(MinGW)。**本项目无测试套件**（见项目 CLAUDE.md），每个 Task 的验证 = 构建通过；最后一个 Task 是手动 QA 清单。

构建命令（仓库根目录执行）：

```powershell
$env:PATH = 'D:\Qt\6.11.0\mingw_64\bin;D:\Qt\Tools\mingw1310_64\bin;D:\Qt\Tools\Ninja;D:\Qt\Tools\CMake_64\bin;' + $env:PATH
cmake --build build/Desktop_Qt_6_11_0_MinGW_64_bit-Debug
```

预期：`ninja: build stopped` 不出现，最后链接出 `BalatroQt.exe`。

---

### Task 1: GameDeckType 类体系 + 常量

**Files:**
- Create: `src/game/gamedeck.h`
- Create: `src/game/gamedeck.cpp`
- Modify: `src/utils/constants.h`（namespace 尾部）
- Modify: `CMakeLists.txt`（`qt_add_executable` 列表，`src/game/gamestate.h src/game/gamestate.cpp` 行附近）

- [ ] **Step 1.1: constants.h 加队列牌组常量**

在 `src/utils/constants.h` 的 `namespace Constants` 内（`BASE_FLUSH_FIVE_MULT` 行之后）追加：

```cpp
    // 队列牌组（游戏牌组体系，见 src/game/gamedeck.h）
    constexpr int QUEUE_DECK_WINDOW = 6;         // 队首窗口：仅最前 6 张手牌可选
    constexpr int QUEUE_DECK_EXTRA_HANDS = 1;    // 每回合出牌次数补偿
    constexpr int QUEUE_DECK_EXTRA_DISCARDS = 1; // 每回合弃牌次数补偿
```

- [ ] **Step 1.2: 新建 src/game/gamedeck.h**

```cpp
#ifndef GAMEDECK_H
#define GAMEDECK_H

#include <QString>
#include <memory>
#include "../utils/constants.h"

// 游戏牌组体系：与 DeckSkin（纯卡面换肤）正交，牌组改变游戏核心规则。
// 抽象基类 + 派生类，GameState 经 unique_ptr 持有，开局前由 UI 注入。
class GameDeckType
{
public:
    virtual ~GameDeckType() = default;
    virtual QString name() const = 0;
    virtual QString description() const = 0;         // 选牌组界面的效果文案
    virtual int  extraHands()    const { return 0; }  // 每回合出牌次数修正
    virtual int  extraDiscards() const { return 0; }  // 每回合弃牌次数修正
    virtual int  selectionWindow() const { return 1 << 30; } // 可选牌窗口（默认不限）
    virtual bool allowHandSort() const { return true; }       // 是否允许排序/拖拽重排
};

// 基础牌组：现状行为，全部默认。
class BaseGameDeck : public GameDeckType
{
public:
    QString name() const override;
    QString description() const override;
};

// 队列牌组：手牌为先进先出队列，仅队首窗口内可选；+1 出牌 +1 弃牌补偿。
class QueueGameDeck : public GameDeckType
{
public:
    QString name() const override;
    QString description() const override;
    int  extraHands()    const override { return Constants::QUEUE_DECK_EXTRA_HANDS; }
    int  extraDiscards() const override { return Constants::QUEUE_DECK_EXTRA_DISCARDS; }
    int  selectionWindow() const override { return Constants::QUEUE_DECK_WINDOW; }
    bool allowHandSort() const override { return false; }
};

// 牌组 id（UI 枚举/记忆所选牌组用）与工厂。
enum class GameDeckId { Base = 0, Queue = 1 };
std::unique_ptr<GameDeckType> createGameDeck(GameDeckId id);

#endif // GAMEDECK_H
```

- [ ] **Step 1.3: 新建 src/game/gamedeck.cpp**

```cpp
#include "gamedeck.h"

QString BaseGameDeck::name() const { return QStringLiteral("基础牌组"); }
QString BaseGameDeck::description() const
{
    return QStringLiteral("标准 52 张牌组，无特殊规则。");
}

QString QueueGameDeck::name() const { return QStringLiteral("队列牌组"); }
QString QueueGameDeck::description() const
{
    return QStringLiteral("手牌按抽牌顺序排列（先进先出），禁止整理；"
                          "只能选取最前 %1 张牌出牌或弃牌。\n"
                          "每回合 +%2 出牌次数、+%3 弃牌次数。")
        .arg(Constants::QUEUE_DECK_WINDOW)
        .arg(Constants::QUEUE_DECK_EXTRA_HANDS)
        .arg(Constants::QUEUE_DECK_EXTRA_DISCARDS);
}

std::unique_ptr<GameDeckType> createGameDeck(GameDeckId id)
{
    switch (id) {
    case GameDeckId::Queue: return std::make_unique<QueueGameDeck>();
    case GameDeckId::Base:  break;
    }
    return std::make_unique<BaseGameDeck>();
}
```

- [ ] **Step 1.4: CMakeLists.txt 注册新文件**

在 `qt_add_executable(BalatroQt` 列表中 `src/game/gamestate.h src/game/gamestate.cpp` 一行之后插入：

```cmake
    src/game/gamedeck.h src/game/gamedeck.cpp
```

- [ ] **Step 1.5: 构建验证**

运行上方构建命令。预期：编译链接通过。

- [ ] **Step 1.6: Commit**

```powershell
git add src/game/gamedeck.h src/game/gamedeck.cpp src/utils/constants.h CMakeLists.txt
git commit -m "队列牌组：GameDeckType 多态牌组体系与常量"
```

---

### Task 2: GameState 接入（补偿/校验/排序锁定/蔚蓝铃铛）

**Files:**
- Modify: `src/game/gamestate.h`（include 区、preview 函数 ~76-96 行、public 函数区、private 成员区）
- Modify: `src/game/gamestate.cpp`（构造函数、`sortHandByRank/BySuit` ~391、`playCards` ~586、`discardCards` ~1321、`moveHandCard` ~1682、`startGame` ~2915、`startBlind` ~3034/3051、`refreshCeruleanForced` ~3237）

- [ ] **Step 2.1: gamestate.h 持有牌组**

include 区（`#include "tag.h"` 附近）加：

```cpp
#include <memory>
#include "gamedeck.h"
```

public 区（如 `jokerSlots()` 声明附近）加：

```cpp
    // 游戏牌组（基础/队列…）：仅开局前注入；任何时刻 mGameDeck 非空。
    void setGameDeck(std::unique_ptr<GameDeckType> deck) { if (deck) mGameDeck = std::move(deck); }
    const GameDeckType &gameDeck() const { return *mGameDeck; }
```

private 成员区加：

```cpp
    std::unique_ptr<GameDeckType> mGameDeck = std::make_unique<BaseGameDeck>();
```

`extraHandsPerRoundPreview()`（gamestate.h:76-83）的 `int delta = mExtraHandsPerRound;` 改为：

```cpp
        int delta = mExtraHandsPerRound + mGameDeck->extraHands();
```

`extraDiscardsPerRoundPreview()`（gamestate.h:88-96）的 `int delta = mExtraDiscardsPerRound;` 改为：

```cpp
        int delta = mExtraDiscardsPerRound + mGameDeck->extraDiscards();
```

- [ ] **Step 2.2: 排序/重排锁定**

`gamestate.cpp` `sortHandByRank()`（~391）与 `sortHandBySuit()`（~397）函数体首行各加：

```cpp
    if (!mGameDeck->allowHandSort()) return;   // 队列牌组：禁止整理手牌
```

`moveHandCard()`（~1682）函数体首行加：

```cpp
    if (!mGameDeck->allowHandSort()) return false;   // 队列牌组：禁止拖拽重排
```

`startGame()`（~2915）`mSortMode = HandSortMode::ByRank;` 改为：

```cpp
    mSortMode = mGameDeck->allowHandSort() ? HandSortMode::ByRank
                                           : HandSortMode::Manual;  // 队列牌组锁定抽牌序
```

- [ ] **Step 2.3: startBlind 补偿 +1/+1**

`startBlind()` 中（~3034）：

```cpp
    mHandsLeft = qMax(1, Constants::INITIAL_HANDS + mExtraHandsPerRound
                             + mGameDeck->extraHands() + handsJokerDelta);
```

（~3051）：

```cpp
    mDiscardLeft = qMax(0, Constants::INITIAL_DISCARDS + mExtraDiscardsPerRound
                               + mGameDeck->extraDiscards() + discardJokerDelta);
```

- [ ] **Step 2.4: 出牌/弃牌窗口校验（模型层防御）**

`playCards()`（~594）现有 bounds 循环：

```cpp
    for (int i : sorted) {
        if (i < 0 || i >= mHand.size()) return;
    }
```

改为：

```cpp
    const int win = mGameDeck->selectionWindow();
    for (int i : sorted) {
        if (i < 0 || i >= mHand.size()) return;
        if (i >= win) { qWarning("playCards: 队列窗口外的牌 idx=%d", i); return; }
    }
```

`discardCards()`（~1323，`if (mDiscardLeft <= 0) return;` 之后、`for (int i : indices) discarded.append(...)` 之前）插入：

```cpp
    const int win = mGameDeck->selectionWindow();
    for (int i : indices) {
        if (i < 0 || i >= mHand.size()) return;
        if (i >= win) { qWarning("discardCards: 队列窗口外的牌 idx=%d", i); return; }
    }
```

（`qWarning` 需要 `<QtGlobal>`，gamestate.cpp 已含 Qt 头，无需新增 include。）

- [ ] **Step 2.5: 蔚蓝铃铛锁定限定在窗口内**

`refreshCeruleanForced()`（~3247）：

```cpp
    mCeruleanForcedUid = mHand[QRandomGenerator::global()->bounded(mHand.size())].uid;
```

改为：

```cpp
    // 队列牌组：锁定目标限定在队首窗口内，避免与"窗口外不可选"冲突。
    const int pickRange = qMin(mHand.size(), mGameDeck->selectionWindow());
    mCeruleanForcedUid = mHand[QRandomGenerator::global()->bounded(pickRange)].uid;
```

- [ ] **Step 2.6: 构建验证**（同 Task 1 命令，预期通过）

- [ ] **Step 2.7: Commit**

```powershell
git add src/game/gamestate.h src/game/gamestate.cpp
git commit -m "队列牌组：GameState 接入（+1/+1 补偿、窗口校验、排序锁定、蔚蓝铃铛窗口内锁定）"
```

---

### Task 3: 局内 UI——窗口选牌限制、禁排序/拖拽、锁定牌视觉

**Files:**
- Modify: `src/ui/mainwindow.h`（private 成员区）
- Modify: `src/ui/mainwindow.cpp`（`onCardClicked` ~5426、`onHandCardDragMoved` ~5950 函数体首、`onHandCardDragReleased` ~6000、`layoutHandCards` ~4946 循环、`startNewRunFromOptions` ~2196）

- [ ] **Step 3.1: mainwindow.h 加成员**

private 成员区（`mMainMenuOverlay` 声明附近）加：

```cpp
    QGraphicsTextItem *mQueueHeadLabel = nullptr;  // 队列牌组：手牌左上"队首"标记
    void updateSortButtonsForDeck();               // 按当前牌组启/禁排序按钮
```

- [ ] **Step 3.2: onCardClicked 窗口拦截**

`onCardClicked()`（~5439）`} else {` 分支开头（`if (mSelected.size() < 5)` 之前）插入：

```cpp
        // 队列牌组：窗口外的"排队中"手牌不可选，给拒绝反馈。
        if (idx >= mGameState->gameDeck().selectionWindow()) {
            card->juiceUp(0.92, 140);
            AudioManager::instance()->play(QStringLiteral("cardSlide2"), 0.8, 0.4);
            return;
        }
```

- [ ] **Step 3.3: 禁拖拽重排**

`onHandCardDragMoved()` 函数体首行加：

```cpp
    if (!mGameState->gameDeck().allowHandSort()) return;   // 队列牌组：禁止重排
```

`onHandCardDragReleased()`（6000）函数体首（`mLastHandCardDragTo = -1;` 之后）加：

```cpp
    if (!mGameState->gameDeck().allowHandSort()) {   // 队列牌组：松手弹回原位
        layoutHandCards();
        refreshCounters();
        updateHandPreview();
        return;
    }
```

- [ ] **Step 3.4: layoutHandCards 锁定牌视觉 + 队首标记**

`layoutHandCards()` 末尾的 per-card 循环（~4946）内，`mHandCards[i]->moveTo(...)` 之后加：

```cpp
        // 队列牌组：窗口外的牌压暗表示"排队中"。基础牌组 win 极大，恒为 1.0。
        mHandCards[i]->setOpacity(i < mGameState->gameDeck().selectionWindow() ? 1.0 : 0.55);
```

循环结束后（函数末尾）加：

```cpp
    // 队首标记：仅队列牌组显示，挂在第一张手牌上方。
    const bool queueDeck = mGameState->gameDeck().selectionWindow() < (1 << 30);
    if (queueDeck && !mQueueHeadLabel) {
        mQueueHeadLabel = mScene->addText(QStringLiteral("队首 →"));
        QFont qf = mCNFont; qf.setPixelSize(18); qf.setBold(true);
        mQueueHeadLabel->setFont(qf);
        mQueueHeadLabel->setDefaultTextColor(QColor("#c8d8d8"));
        mQueueHeadLabel->setZValue(300);
    }
    if (mQueueHeadLabel) {
        mQueueHeadLabel->setVisible(queueDeck && n > 0);
        if (queueDeck && n > 0)
            mQueueHeadLabel->setPos(startX, mHandY - 30);
    }
```

- [ ] **Step 3.5: 排序按钮启/禁**

`mainwindow.cpp` 任意函数区（建议紧跟 `startNewRunFromOptions` 定义之后）新增：

```cpp
void MainWindow::updateSortButtonsForDeck()
{
    const bool allow = mGameState->gameDeck().allowHandSort();
    if (mBtnSortNum)  mBtnSortNum->setEnabled(allow);
    if (mBtnSortSuit) mBtnSortSuit->setEnabled(allow);
}
```

`startNewRunFromOptions()`（~2196）`mGameState->startGame();` 之后加一行：

```cpp
    updateSortButtonsForDeck();
```

- [ ] **Step 3.6: 构建验证**（同前，预期通过）

- [ ] **Step 3.7: Commit**

```powershell
git add src/ui/mainwindow.h src/ui/mainwindow.cpp
git commit -m "队列牌组：局内 UI 窗口限制、禁排序/拖拽、排队牌压暗与队首标记"
```

---

### Task 4: DeckSelectWidget 选牌组界面 + 主菜单流程接入

**Files:**
- Modify: `src/card/carditem.h`（public 区）、`src/card/carditem.cpp`
- Create: `src/ui/deckselectwidget.h`
- Create: `src/ui/deckselectwidget.cpp`
- Modify: `src/ui/mainwindow.h`、`src/ui/mainwindow.cpp`（btnPlay ~4282、`startNewRunFromOptions` ~2196、resizeEvent ~6487）
- Modify: `CMakeLists.txt`

- [ ] **Step 4.1: CardItem 暴露卡背图**

`carditem.h` public 区加：

```cpp
    static QPixmap cardBackPixmap();   // 卡背贴图（牌组选择界面预览用）
```

`carditem.cpp`（`paintBack` 定义之后）加：

```cpp
QPixmap CardItem::cardBackPixmap() {
    if (!sEnhSheet || sEnhSheet->isNull()) return QPixmap();
    return sEnhSheet->copy(0, 0, SRC_W, SRC_H);
}
```

- [ ] **Step 4.2: 新建 src/ui/deckselectwidget.h**

```cpp
#ifndef DECKSELECTWIDGET_H
#define DECKSELECTWIDGET_H

#include <QWidget>
#include <QVector>
#include "../game/gamedeck.h"

class QLabel;
class QPushButton;

// 开局牌组选择层（overlay，遵循项目"不用原生 QDialog"的规矩）。
// 主菜单"开始游戏"→ 本层 → startRequested(deckId) → MainWindow 注入 GameState 后开局。
class DeckSelectWidget : public QWidget
{
    Q_OBJECT
public:
    explicit DeckSelectWidget(const QFont &cnFont, QWidget *parent = nullptr);
    GameDeckId selectedDeck() const { return mSelected; }

signals:
    void startRequested(GameDeckId id);
    void cancelled();

private:
    void select(int idx);
    static QPixmap hueShifted(const QPixmap &src, int dh);

    GameDeckId mSelected = GameDeckId::Base;
    QVector<QWidget*> mOptionFrames;
    QLabel *mDescLabel = nullptr;
};

#endif // DECKSELECTWIDGET_H
```

- [ ] **Step 4.3: 新建 src/ui/deckselectwidget.cpp**

```cpp
#include "deckselectwidget.h"
#include "../card/carditem.h"
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QMouseEvent>

namespace {
// 选项卡片（卡背 + 牌组名），整块可点。
class DeckOptionFrame : public QWidget
{
public:
    DeckOptionFrame(const QPixmap &back, const QString &name,
                    const QFont &cnFont, std::function<void()> onClick,
                    QWidget *parent)
        : QWidget(parent), mOnClick(std::move(onClick))
    {
        setAttribute(Qt::WA_StyledBackground, true);
        setCursor(Qt::PointingHandCursor);
        auto *v = new QVBoxLayout(this);
        v->setContentsMargins(14, 14, 14, 10);
        v->setSpacing(8);
        auto *img = new QLabel(this);
        img->setPixmap(back.scaled(142, 190, Qt::KeepAspectRatio,
                                   Qt::FastTransformation));   // 像素风：最近邻放大
        img->setAlignment(Qt::AlignCenter);
        v->addWidget(img);
        auto *nameLab = new QLabel(name, this);
        QFont f = cnFont; f.setPixelSize(20); f.setBold(true);
        nameLab->setFont(f);
        nameLab->setStyleSheet("color:white; background:transparent;");
        nameLab->setAlignment(Qt::AlignCenter);
        v->addWidget(nameLab);
        setSelected(false);
    }
    void setSelected(bool on)
    {
        setStyleSheet(QString("background:#3a4a4d; border-radius:12px;"
                              " border:3px solid %1;")
                          .arg(on ? "#fda200" : "transparent"));
    }
protected:
    void mousePressEvent(QMouseEvent *e) override
    {
        if (e->button() == Qt::LeftButton) mOnClick();
        QWidget::mousePressEvent(e);
    }
private:
    std::function<void()> mOnClick;
};
} // namespace

DeckSelectWidget::DeckSelectWidget(const QFont &cnFont, QWidget *parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_StyledBackground, true);
    setStyleSheet("background:rgba(10,12,14,200);");   // 半透明压暗主菜单

    auto *outer = new QVBoxLayout(this);
    outer->setAlignment(Qt::AlignCenter);

    auto *panel = new QWidget(this);
    panel->setAttribute(Qt::WA_StyledBackground, true);
    panel->setStyleSheet("background:#4f6367; border-radius:16px;");
    panel->setFixedWidth(560);
    auto *pv = new QVBoxLayout(panel);
    pv->setContentsMargins(24, 20, 24, 20);
    pv->setSpacing(14);

    auto *title = new QLabel(QStringLiteral("选择牌组"), panel);
    QFont tf = cnFont; tf.setPixelSize(26); tf.setBold(true);
    title->setFont(tf);
    title->setStyleSheet("color:white; background:transparent;");
    title->setAlignment(Qt::AlignCenter);
    pv->addWidget(title);

    // —— 牌组选项行：经工厂取多态实例读名称/描述，新增牌组只需扩 GameDeckId ——
    const QPixmap back = CardItem::cardBackPixmap();
    const QVector<QPair<GameDeckId, QPixmap>> options = {
        { GameDeckId::Base,  back },
        { GameDeckId::Queue, hueShifted(back, 150) },   // 队列牌组卡背：色相偏移区分
    };
    auto *row = new QHBoxLayout();
    row->setSpacing(18);
    for (int i = 0; i < options.size(); ++i) {
        const auto deck = createGameDeck(options[i].first);
        auto *frame = new DeckOptionFrame(options[i].second, deck->name(), cnFont,
                                          [this, i]() { select(i); }, panel);
        mOptionFrames.append(frame);
        row->addWidget(frame);
    }
    pv->addLayout(row);

    mDescLabel = new QLabel(panel);
    QFont df = cnFont; df.setPixelSize(16);
    mDescLabel->setFont(df);
    mDescLabel->setStyleSheet("color:#d8e4e6; background:transparent;");
    mDescLabel->setWordWrap(true);
    mDescLabel->setAlignment(Qt::AlignCenter);
    mDescLabel->setMinimumHeight(64);
    pv->addWidget(mDescLabel);

    auto *btnRow = new QHBoxLayout();
    btnRow->setSpacing(14);
    auto makeBtn = [&](const QString &text, const QString &bg, const QString &dark) {
        auto *b = new QPushButton(text, panel);
        QFont f = cnFont; f.setPixelSize(20); f.setBold(true);
        b->setFont(f);
        b->setMinimumHeight(52);
        b->setCursor(Qt::PointingHandCursor);
        b->setStyleSheet(QString(
            "QPushButton { background:%1; color:white; border:none;"
            " border-radius:10px; border-bottom:5px solid %2; }"
            "QPushButton:pressed { border-bottom:2px solid %2; margin-top:3px; }")
                             .arg(bg, dark));
        btnRow->addWidget(b);
        return b;
    };
    auto *btnBack  = makeBtn(QStringLiteral("返回"),     "#fe5f55", "#c44840");
    auto *btnStart = makeBtn(QStringLiteral("开始游戏"), "#009dff", "#0077c2");
    connect(btnBack,  &QPushButton::clicked, this, [this]() { emit cancelled(); });
    connect(btnStart, &QPushButton::clicked, this,
            [this]() { emit startRequested(mSelected); });
    pv->addLayout(btnRow);

    outer->addWidget(panel);
    select(0);
}

void DeckSelectWidget::select(int idx)
{
    mSelected = static_cast<GameDeckId>(idx);
    for (int i = 0; i < mOptionFrames.size(); ++i)
        static_cast<DeckOptionFrame*>(mOptionFrames[i])->setSelected(i == idx);
    const auto deck = createGameDeck(mSelected);
    mDescLabel->setText(deck->description());
}

QPixmap DeckSelectWidget::hueShifted(const QPixmap &src, int dh)
{
    QImage img = src.toImage().convertToFormat(QImage::Format_ARGB32);
    for (int y = 0; y < img.height(); ++y) {
        QRgb *line = reinterpret_cast<QRgb*>(img.scanLine(y));
        for (int x = 0; x < img.width(); ++x) {
            const QColor c = QColor::fromRgba(line[x]);
            int h, s, v;
            c.getHsv(&h, &s, &v);
            if (h < 0) continue;   // 无色相（灰阶）像素不动
            QColor shifted;
            shifted.setHsv((h + dh) % 360, s, v, c.alpha());
            line[x] = shifted.rgba();
        }
    }
    return QPixmap::fromImage(img);
}
```

- [ ] **Step 4.4: CMakeLists.txt 注册**

`qt_add_executable` 列表中 `src/ui/deckviewwidget.h src/ui/deckviewwidget.cpp` 行之后插入：

```cmake
    src/ui/deckselectwidget.h src/ui/deckselectwidget.cpp
```

- [ ] **Step 4.5: MainWindow 接入流程**

`mainwindow.h`：include 区加 `class DeckSelectWidget;` 前置声明；成员区（`mMainMenuOverlay` 附近）加：

```cpp
    QPointer<DeckSelectWidget> mDeckSelectOverlay;
    GameDeckId mSelectedGameDeckId = GameDeckId::Base;  // 局内选项重开新局时复用
    void showDeckSelectOverlay();
```

（`mainwindow.h` 需 `#include "../game/gamedeck.h"`，放在现有 include 区。）

`mainwindow.cpp` btnPlay 的 connect（~4282）改为：

```cpp
    connect(btnPlay, &QPushButton::clicked, this, [this]() {
        showDeckSelectOverlay();   // 先选牌组，再开局（主菜单保持在底下）
    });
```

新增函数（建议放在 `showMainMenuOverlay()` 之后）：

```cpp
void MainWindow::showDeckSelectOverlay()
{
    if (mDeckSelectOverlay) {
        mDeckSelectOverlay->deleteLater();
        mDeckSelectOverlay = nullptr;
    }
    QWidget *host = centralWidget() ? centralWidget() : this;
    auto *w = new DeckSelectWidget(mCNFont, host);
    mDeckSelectOverlay = w;
    connect(w, &DeckSelectWidget::cancelled, this, [this]() {
        if (mDeckSelectOverlay) { mDeckSelectOverlay->deleteLater(); mDeckSelectOverlay = nullptr; }
    });
    connect(w, &DeckSelectWidget::startRequested, this, [this](GameDeckId id) {
        mSelectedGameDeckId = id;
        if (mDeckSelectOverlay) { mDeckSelectOverlay->deleteLater(); mDeckSelectOverlay = nullptr; }
        hideMainMenuOverlay();
        startNewRunFromOptions();
    });
    w->setGeometry(host->rect());
    w->raise();
    w->show();
}
```

`startNewRunFromOptions()`（~2196）`mGameState->startGame();` 之前加：

```cpp
    // 把所选游戏牌组注入模型（局内"新的一局"路径复用上次选择）。
    mGameState->setGameDeck(createGameDeck(mSelectedGameDeckId));
```

`mainwindow.cpp` include 区加 `#include "deckselectwidget.h"`。

`resizeEvent`（~6487，`mDeckViewWidget` 行之后）加：

```cpp
        if (mDeckSelectOverlay && centralWidget())
            mDeckSelectOverlay->setGeometry(centralWidget()->rect());
```

注意：resizeEvent 中这段在 `if (mPlayPage)` 块内，deck select 挂在 centralWidget 上也随其矩形同步即可。

- [ ] **Step 4.6: 构建验证**（同前，预期通过）

- [ ] **Step 4.7: Commit**

```powershell
git add src/card/carditem.h src/card/carditem.cpp src/ui/deckselectwidget.h src/ui/deckselectwidget.cpp src/ui/mainwindow.h src/ui/mainwindow.cpp CMakeLists.txt
git commit -m "队列牌组：DeckSelectWidget 选牌组界面与主菜单开局流程接入"
```

---

### Task 5: 手动 QA（无测试套件，运行核对）

**Files:** 无代码改动；运行 `build\Desktop_Qt_6_11_0_MinGW_64_bit-Debug\BalatroQt.exe`

- [ ] **Step 5.1: 选牌组流程**
  主菜单点"开始游戏"→ 出现选牌组层（两张卡背、队列牌组卡背颜色不同）；点"返回"回主菜单；再进，选队列牌组 → "开始游戏"进入盲注选择。

- [ ] **Step 5.2: 队列规则**
  局内：手牌不按点数排序（保持抽牌顺序）；"点数/花色"按钮置灰；第 7、8 张手牌压暗，点击只抖动不选中；前 6 张可自由勾选 ≤5 张；出牌/弃牌后新补的牌出现在最右侧；拖拽手牌松手弹回原位；手牌上方显示"队首 →"。

- [ ] **Step 5.3: +1/+1 补偿**
  队列牌组小盲开局显示 5 次出牌（4+1）、4 次弃牌（3+1）；商店侧栏"下一回合"预览同样含 +1/+1。

- [ ] **Step 5.4: 基础牌组回归**
  回主菜单 → 开始游戏 → 选基础牌组：排序按钮可用、手牌自动按点数排、无压暗无"队首"标记、4 出牌 3 弃牌。

- [ ] **Step 5.5: 蔚蓝铃铛抽查（可选，可用演示/调试手段触发）**
  队列牌组打到蔚蓝铃铛 Boss：被锁定选中的牌始终在前 6 张内。

- [ ] **Step 5.6: 全部通过后收尾 commit（如 QA 中有修补）**

---

## Self-Review 记录

- Spec 覆盖：玩法 6 条规则 → Task 2（补偿/校验/排序/铃铛）+ Task 3（窗口交互/视觉）；架构 → Task 1；UI 流程 → Task 4；验证 → Task 5。无缺口。
- 占位符扫描：所有步骤含完整代码/命令。
- 类型一致性：`GameDeckType/GameDeckId/createGameDeck/selectionWindow/allowHandSort/extraHands/extraDiscards` 各任务间签名一致；`cardBackPixmap` 定义（4.1）先于使用（4.3）。
