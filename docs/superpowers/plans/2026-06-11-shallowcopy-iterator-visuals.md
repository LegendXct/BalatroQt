# 浅拷贝/迭代器视觉表现 + 程设卡面增强修复 实现计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 浅拷贝链接牌共享地址角标、程设整卡人像与增强底图半透明叠化、迭代器计分后翻面演出、浅拷贝/迭代器塔罗接入翻面动画。

**Architecture:** 模型层只新增一个只读访问器（`shallowLinkPairs`）和一个共享 inline 函数（`iterNextRank` 迁入 carddata.h）；视觉全部在 UI/card 层完成。卡面不透明度规则收敛进 `DeckSkin::faceOpacity`，五处卡面合成路径统一调用。角标画在 CardItem 缓存图层之上，不进缓存 key。

**Tech Stack:** Qt6 Widgets / QGraphicsScene，无测试套件——每个任务以构建通过为验证，最后统一手动 QA。

**构建命令**（仓库根目录）：

```powershell
$env:PATH = 'D:\Qt\6.11.0\mingw_64\bin;D:\Qt\Tools\mingw1310_64\bin;D:\Qt\Tools\Ninja;D:\Qt\Tools\CMake_64\bin;' + $env:PATH
cmake --build build/Desktop_Qt_6_11_0_MinGW_64_bit-Debug
```

预期输出末尾无 error；链接失败先确认没有残留的 BalatroQt.exe 进程占用输出文件（`Get-Process BalatroQt -ErrorAction SilentlyContinue | Stop-Process -Force`）。

**注意：另一会话可能在同一工作区工作。每个任务提交前先 `git status` 确认只暂存本任务的文件；改文件前若上次读取已久远，重读目标区域。**

---

### Task 1: 共享基础——iterNextRank 迁入 carddata.h + GameState::shallowLinkPairs()

**Files:**
- Modify: `src/card/carddata.h`（Rank 枚举之后，约第 26 行）
- Modify: `src/game/gamestate.cpp:11-17`（删除本地 iterNextRank）
- Modify: `src/game/gamestate.h:177-179`（registerShallowLink/syncShallowLinks 旁加访问器）

- [ ] **Step 1: carddata.h 添加 inline iterNextRank**

在 `enum class Rank { ... };`（第 26 行 `};`）之后插入：

```cpp
// 迭代器增强（程设扩展）的点数递推：每次打出后 +1，K→A→2 回绕
// （区别于"力量"塔罗的 nextRank：那个 A 封顶）。
// 模型(finalizePlayedHand)与 UI(计分后的翻面演出)共用，规则只此一份。
inline Rank iterNextRank(Rank r)
{
    return (r == Rank::Ace) ? Rank::Two : static_cast<Rank>(static_cast<int>(r) + 1);
}
```

- [ ] **Step 2: gamestate.cpp 删除本地定义**

把：

```cpp
namespace {
// 迭代器增强：每次打出后点数 +1，K→A→2 回绕（区别于"力量"塔罗的 nextRank：那个 A 封顶）。
Rank iterNextRank(Rank r)
{
    return (r == Rank::Ace) ? Rank::Two : static_cast<Rank>(static_cast<int>(r) + 1);
}

// 类模板：牌型 → 构成张数（高牌1 对子2 三条3 两对/四条4 其余5）。
```

替换为：

```cpp
namespace {
// 类模板：牌型 → 构成张数（高牌1 对子2 三条3 两对/四条4 其余5）。
```

（iterNextRank 已在 carddata.h，gamestate.cpp 经 include 链可见，1237 行的调用无需改。）

- [ ] **Step 3: gamestate.h 添加 shallowLinkPairs()**

文件顶部确认有 `#include <QPair>`（没有则在现有 Qt include 旁加上）。然后把：

```cpp
    // ── 浅拷贝塔罗：两张牌共享状态（点数/花色/增强/版本/蜡封/debuff/永久加筹） ──
    void registerShallowLink(int uidA, int uidB);
    void syncShallowLinks();
```

替换为：

```cpp
    // ── 浅拷贝塔罗：两张牌共享状态（点数/花色/增强/版本/蜡封/debuff/永久加筹） ──
    void registerShallowLink(int uidA, int uidB);
    void syncShallowLinks();
    // UI 用：当前所有链接对（uidA, uidB），给两侧牌面画共享地址角标。
    QVector<QPair<int,int>> shallowLinkPairs() const {
        QVector<QPair<int,int>> out;
        out.reserve(mShallowLinks.size());
        for (const auto &l : mShallowLinks) out.append({l.uidA, l.uidB});
        return out;
    }
```

（类内 inline 成员函数体按"类完整"规则编译，可引用后面才声明的私有 `mShallowLinks`。）

- [ ] **Step 4: 构建**

运行上方构建命令。预期：成功，无新 warning（gamestate.cpp ~2348 行附近的反斜杠注释 warning 是历史遗留，忽略）。

- [ ] **Step 5: 提交**

```powershell
git status   # 确认只有这 3 个文件变更
git add src/card/carddata.h src/game/gamestate.cpp src/game/gamestate.h
git commit -m "重构：iterNextRank 迁入 carddata.h 共用；GameState 暴露浅拷贝链接对"
```

---

### Task 2: 程设卡面增强半透明叠化（修复增强效果被整卡人像盖住）

**Files:**
- Modify: `src/card/deckskin.h`（加 include + 静态助手声明）
- Modify: `src/card/deckskin.cpp`（助手实现）
- Modify: `src/card/carditem.cpp:252-255`（paintFront 卡面绘制）
- Modify: `src/ui/deckviewwidget.cpp:548-558`（renderCard）
- Modify: `src/ui/packopenwidget.cpp:1011-1021`（renderPlayingCard）
- Modify: `src/ui/shopwidget.cpp:1685-1695`（playingCardPixmap）
- Modify: `src/ui/mainwindow.cpp:2826-2829`（collectionPlayingCardPixmap）

- [ ] **Step 1: deckskin.h 声明 faceOpacity**

`#include <QString>` 之后加 `#include "carddata.h"`。在 `static int generation() { return sGeneration; }` 之后插入：

```cpp
    // 背景式增强（奖励/倍率/万能/幸运/玻璃/钢铁/黄金）画在卡面之下；程设皮肤的
    // J/Q/K/A 整卡人像不透明、会盖死增强底色，此时人像以 70% 不透明度叠化让底色
    // 透出（倍率泛红/钢铁泛灰/玻璃整卡半透明）。所有卡面合成路径统一经这里取值。
    static qreal faceOpacity(Rank rank, Enhancement enh);
```

- [ ] **Step 2: deckskin.cpp 实现**

文件末尾追加：

```cpp
qreal DeckSkin::faceOpacity(Rank rank, Enhancement enh)
{
    const bool fullArt = sCurrent == ChengShe && rank >= Rank::Jack;
    const bool bgEnhance = enh != Enhancement::None
                        && enh != Enhancement::Stone      // 石头不画卡面
                        && enh != Enhancement::Iterator;  // 迭代器底图是白底，无需叠化
    return (fullArt && bgEnhance) ? 0.70 : 1.0;
}
```

- [ ] **Step 3: carditem.cpp paintFront 接入**

把：

```cpp
            QRect enh = enhanceSrcRect();
            if (!enh.isNull()) bp.drawPixmap(cacheRect, *sEnhSheet, enh);
            if (mData.enhancement != Enhancement::Stone)
                bp.drawPixmap(cacheRect, DeckSkin::deckSheet(), deckSrcRect());
```

替换为：

```cpp
            QRect enh = enhanceSrcRect();
            if (!enh.isNull()) bp.drawPixmap(cacheRect, *sEnhSheet, enh);
            if (mData.enhancement != Enhancement::Stone) {
                // 程设整卡人像会盖死底下的背景式增强，按需半透明叠化。
                const qreal fo = DeckSkin::faceOpacity(mData.rank, mData.enhancement);
                if (fo < 1.0) bp.setOpacity(fo);
                bp.drawPixmap(cacheRect, DeckSkin::deckSheet(), deckSrcRect());
                if (fo < 1.0) bp.setOpacity(1.0);
            }
```

（缓存 key 已含 enhancement + DeckSkin::generation，透明度由这两者决定，key 不用改。）

- [ ] **Step 4: deckviewwidget.cpp renderCard 接入**

把（约 548-558 行）：

```cpp
    if (c.enhancement != Enhancement::Stone && !deckSheet.isNull()) {
        int col = static_cast<int>(c.rank) - 2;
        int row = 0;
        switch (c.suit) {
        case Suit::Hearts:   row = 0; break;
        case Suit::Clubs:    row = 1; break;
        case Suit::Diamonds: row = 2; break;
        case Suit::Spades:   row = 3; break;
        }
        p.drawPixmap(QRect(0, 0, W, H), deckSheet, QRect(col*W, row*H, W, H));
    }
```

替换为：

```cpp
    if (c.enhancement != Enhancement::Stone && !deckSheet.isNull()) {
        int col = static_cast<int>(c.rank) - 2;
        int row = 0;
        switch (c.suit) {
        case Suit::Hearts:   row = 0; break;
        case Suit::Clubs:    row = 1; break;
        case Suit::Diamonds: row = 2; break;
        case Suit::Spades:   row = 3; break;
        }
        // 程设整卡人像 × 背景式增强：半透明叠化（与 CardItem::paintFront 一致）。
        const qreal fo = DeckSkin::faceOpacity(c.rank, c.enhancement);
        if (fo < 1.0) p.setOpacity(fo);
        p.drawPixmap(QRect(0, 0, W, H), deckSheet, QRect(col*W, row*H, W, H));
        if (fo < 1.0) p.setOpacity(1.0);
    }
```

- [ ] **Step 5: packopenwidget.cpp renderPlayingCard 接入**

把（约 1011-1021 行）：

```cpp
    if (c.enhancement != Enhancement::Stone && !deckSheet.isNull()) {
        int col = static_cast<int>(c.rank) - 2;
        int row = 0;
        switch (c.suit) {
        case Suit::Hearts:   row = 0; break;
        case Suit::Clubs:    row = 1; break;
        case Suit::Diamonds: row = 2; break;
        case Suit::Spades:   row = 3; break;
        }
        p.drawPixmap(QRect(0, 0, W, H), deckSheet, QRect(col*W, row*H, W, H));
    }
```

替换为：

```cpp
    if (c.enhancement != Enhancement::Stone && !deckSheet.isNull()) {
        int col = static_cast<int>(c.rank) - 2;
        int row = 0;
        switch (c.suit) {
        case Suit::Hearts:   row = 0; break;
        case Suit::Clubs:    row = 1; break;
        case Suit::Diamonds: row = 2; break;
        case Suit::Spades:   row = 3; break;
        }
        // 程设整卡人像 × 背景式增强：半透明叠化（与 CardItem::paintFront 一致）。
        const qreal fo = DeckSkin::faceOpacity(c.rank, c.enhancement);
        if (fo < 1.0) p.setOpacity(fo);
        p.drawPixmap(QRect(0, 0, W, H), deckSheet, QRect(col*W, row*H, W, H));
        if (fo < 1.0) p.setOpacity(1.0);
    }
```

- [ ] **Step 6: shopwidget.cpp playingCardPixmap 接入**

把（约 1685-1695 行，注意此文件 switch 各 case 与上面缩进略不同，按原样保留）：

```cpp
    if (c.enhancement != Enhancement::Stone && !deckSheet.isNull()) {
        int col = static_cast<int>(c.rank) - 2;
        int row = 0;
        switch (c.suit) {
        case Suit::Hearts: row = 0; break;
        case Suit::Clubs:  row = 1; break;
        case Suit::Diamonds: row = 2; break;
        case Suit::Spades: row = 3; break;
        }
        p.drawPixmap(QRect(0, 0, W, H), deckSheet, QRect(col*W, row*H, W, H));
    }
```

替换为：

```cpp
    if (c.enhancement != Enhancement::Stone && !deckSheet.isNull()) {
        int col = static_cast<int>(c.rank) - 2;
        int row = 0;
        switch (c.suit) {
        case Suit::Hearts: row = 0; break;
        case Suit::Clubs:  row = 1; break;
        case Suit::Diamonds: row = 2; break;
        case Suit::Spades: row = 3; break;
        }
        // 程设整卡人像 × 背景式增强：半透明叠化（与 CardItem::paintFront 一致）。
        const qreal fo = DeckSkin::faceOpacity(c.rank, c.enhancement);
        if (fo < 1.0) p.setOpacity(fo);
        p.drawPixmap(QRect(0, 0, W, H), deckSheet, QRect(col*W, row*H, W, H));
        if (fo < 1.0) p.setOpacity(1.0);
    }
```

- [ ] **Step 7: mainwindow.cpp collectionPlayingCardPixmap 接入**

把（约 2826-2829 行）：

```cpp
        if (enhancement != Enhancement::Stone) {
            p.drawPixmap(QRect(0, 0, CardItem::SRC_W, CardItem::SRC_H),
                         deck, QRect(12 * CardItem::SRC_W, 0, CardItem::SRC_W, CardItem::SRC_H));
        }
```

替换为：

```cpp
        if (enhancement != Enhancement::Stone) {
            // 图鉴样例固定用第 12 列的 A——程设皮肤下也是整卡人像，需同样叠化。
            const qreal fo = DeckSkin::faceOpacity(Rank::Ace, enhancement);
            if (fo < 1.0) p.setOpacity(fo);
            p.drawPixmap(QRect(0, 0, CardItem::SRC_W, CardItem::SRC_H),
                         deck, QRect(12 * CardItem::SRC_W, 0, CardItem::SRC_W, CardItem::SRC_H));
            if (fo < 1.0) p.setOpacity(1.0);
        }
```

- [ ] **Step 8: 构建**

运行构建命令。预期成功。

- [ ] **Step 9: 提交**

```powershell
git status   # 确认只有这 7 个文件
git add src/card/deckskin.h src/card/deckskin.cpp src/card/carditem.cpp src/ui/deckviewwidget.cpp src/ui/packopenwidget.cpp src/ui/shopwidget.cpp src/ui/mainwindow.cpp
git commit -m "修复：程设整卡人像盖住背景式增强——五处卡面合成统一半透明叠化(DeckSkin::faceOpacity)"
```

---

### Task 3: 浅拷贝/迭代器塔罗接入原版翻面动画

**Files:**
- Modify: `src/ui/mainwindow.cpp:150-170`（usesOriginalTarotFlip）

- [ ] **Step 1: 名单补两个 case**

把：

```cpp
    case ConsumableType::Tarot_World:
        return true;
```

替换为：

```cpp
    case ConsumableType::Tarot_World:
    // 程设扩展塔罗：迭代器(上增强)/浅拷贝(左牌变右牌副本)也走"翻背→改→翻回"。
    case ConsumableType::Tarot_Iterator:
    case ConsumableType::Tarot_ShallowCopy:
        return true;
```

- [ ] **Step 2: 构建**

运行构建命令。预期成功。

- [ ] **Step 3: 提交**

```powershell
git add src/ui/mainwindow.cpp
git commit -m "浅拷贝/迭代器塔罗接入原版翻面动画(usesOriginalTarotFlip 名单)"
```

---

### Task 4: 浅拷贝共享地址角标

**Files:**
- Modify: `src/card/carditem.h`（setLinkTag/setLinkTagFont 声明 + 成员）
- Modify: `src/card/carditem.cpp`（实现 + paint 接入）
- Modify: `src/ui/mainwindow.cpp`（ctor 注入字体 + refreshHand 挂角标）

- [ ] **Step 1: carditem.h 声明**

public 段（`void setScoringLifted(bool lifted);` 之后）加：

```cpp
    // 浅拷贝链接角标：两张链接牌共享的"内存地址"文案（空 = 无链接，不绘制）。
    void setLinkTag(const QString &tag);
    static void setLinkTagFont(const QFont &f);   // MainWindow loadFonts 后注入像素字体
```

private 段（`QString` 成员处，如 `CardData mData;` 之后）加：

```cpp
    QString mLinkTag;
```

private 函数区（`void paintBack(QPainter *painter);` 之后）加：

```cpp
    void paintLinkTag(QPainter *painter);
    static QFont sLinkTagFont;
```

文件顶部确认 `#include <QFont>`（QPropertyAnimation/QGraphicsObject 链一般已带，缺则补）。

- [ ] **Step 2: carditem.cpp 实现**

静态成员定义（`QPixmap *CardItem::sJokerSheet = nullptr;` 之后）：

```cpp
QFont CardItem::sLinkTagFont;
```

实现（放在 `paintBack` 之后）：

```cpp
void CardItem::setLinkTagFont(const QFont &f)
{
    sLinkTagFont = f;
    sLinkTagFont.setPixelSize(17);
}

void CardItem::setLinkTag(const QString &tag)
{
    if (mLinkTag == tag) return;
    mLinkTag = tag;
    update();
}

// 浅拷贝共享地址角标：牌面下缘中央一块深色小铭牌，链接两侧文案相同，
// 隐喻"两个指针指向同一块内存"。画在缓存图层之上，不进 paintFront 的缓存 key。
void CardItem::paintLinkTag(QPainter *p)
{
    p->setFont(sLinkTagFont);
    const QFontMetrics fm(sLinkTagFont);
    const qreal w = fm.horizontalAdvance(mLinkTag) + 14;
    const qreal h = 20;
    const QRectF plate((WIDTH - w) / 2.0, HEIGHT - h - 7, w, h);
    p->setRenderHint(QPainter::Antialiasing, true);
    p->setPen(Qt::NoPen);
    p->setBrush(QColor(20, 24, 28, 210));
    p->drawRoundedRect(plate, 5, 5);
    p->setPen(QColor(154, 232, 255));
    p->drawText(plate, Qt::AlignCenter, mLinkTag);
}
```

paint() 接入——把：

```cpp
    if (mData.faceUp) paintFront(painter);
    else paintBack(painter);
```

替换为：

```cpp
    if (mData.faceUp) {
        paintFront(painter);
        if (!mLinkTag.isEmpty()) paintLinkTag(painter);
    } else {
        paintBack(painter);
    }
```

- [ ] **Step 3: mainwindow.cpp ctor 注入字体**

`loadFonts();`（约 902 行）之后加：

```cpp
    CardItem::setLinkTagFont(mPixelFont);   // 浅拷贝地址角标用像素字体
```

- [ ] **Step 4: refreshHand 末尾挂角标**

在 `refreshHand()` 末尾、`layoutHandCards();` 调用（约 4949 行）之前插入：

```cpp
    // 浅拷贝链接：两侧牌面挂同一"共享地址"角标（隐喻两个指针指向同一块数据）；
    // 链接解除（一侧被摧毁/新开一局）后 uid 查不到，角标自动清空。
    QHash<int, QString> linkTags;
    for (const auto &pr : mGameState->shallowLinkPairs()) {
        const QString addr = QStringLiteral("0x%1")
            .arg(QString::number(qMin(pr.first, pr.second), 16).toUpper());
        linkTags.insert(pr.first, addr);
        linkTags.insert(pr.second, addr);
    }
    for (auto *c : mHandCards) c->setLinkTag(linkTags.value(c->cardData().uid));
    for (auto *c : mPlayedCards)
        if (c) c->setLinkTag(linkTags.value(c->cardData().uid));
```

（`QHash` 在 mainwindow.cpp 已有使用；`linkTags.value(uid)` 查不到返回空串 = 清角标。）

- [ ] **Step 5: 构建**

运行构建命令。预期成功。

- [ ] **Step 6: 提交**

```powershell
git status
git add src/card/carditem.h src/card/carditem.cpp src/ui/mainwindow.cpp
git commit -m "浅拷贝视觉联系：链接两侧牌面共享 0x?? 地址角标（refreshHand 统一挂/清）"
```

---

### Task 5: 迭代器计分后翻面演出

**Files:**
- Modify: `src/ui/mainwindow.cpp:9127-9150`（animateScoreTotalThenFinalize 的 finished 回调）

- [ ] **Step 1: 重构 finished 回调，插入翻面序列**

把（9127 行起）：

```cpp
    connect(anim, &QVariantAnimation::finished, this, [this, after]() {
        setLabelScaledText(mLblScore, formatScoreNumber(after), uiPx(38));
        updateScoreProgressBar(after, true);
        // 火焰目标在 900ms 后归零(spring ease 自然熄灭)
        scheduleGame(900, [this]() { resetScoreFlame(); });

        animatePlayedCardsToDiscardThen([this]() {
```

替换为：

```cpp
    connect(anim, &QVariantAnimation::finished, this, [this, after]() {
        setLabelScaledText(mLblScore, formatScoreNumber(after), uiPx(38));
        updateScoreProgressBar(after, true);
        // 火焰目标在 900ms 后归零(spring ease 自然熄灭)
        scheduleGame(900, [this]() { resetScoreFlame(); });

        auto flyOut = [this]() {
        animatePlayedCardsToDiscardThen([this]() {
```

并把该 `animatePlayedCardsToDiscardThen(...)` 调用的收尾（原 9149 行）：

```cpp
        });
    });
    anim->start(QAbstractAnimation::DeleteWhenStopped);
```

替换为：

```cpp
        });
        };

        // 迭代器增强：总分计完、牌飞向弃牌堆之前，把"点数 +1"翻给玩家看。
        // 数据层的真正 +1 在 finalizePlayedHand（flyOut 内）执行，这里只改显示副本。
        // 节拍用固定时长 singleShot（与塔罗翻面流程一致）：flip() 本身 240ms 固定，
        // 若走 scheduleGame 的倍速缩放，高倍速下会跑到翻面中点前头。
        QVector<int> iterIdx;
        for (int i = 0; i < mPlayedCards.size(); ++i) {
            CardItem *c = mPlayedCards[i];
            if (c && c->cardData().enhancement == Enhancement::Iterator
                && !mShatteredPlayedIndices.contains(i))
                iterIdx.append(i);
        }
        if (iterIdx.isEmpty()) { flyOut(); return; }

        for (int k = 0; k < iterIdx.size(); ++k) {
            const int idx = iterIdx[k];
            const int t0 = 150 * (k + 1);
            QTimer::singleShot(t0, this, [this, idx]() {          // 翻向背面
                if (idx < 0 || idx >= mPlayedCards.size()) return;
                CardItem *c = mPlayedCards[idx];
                if (!c) return;
                c->flip();
                AudioManager::instance()->play(QStringLiteral("card1"), 1.0, 1.0);
            });
            QTimer::singleShot(t0 + 130, this, [this, idx]() {    // 翻面中点已过，背面期间改显示点数
                if (idx < 0 || idx >= mPlayedCards.size()) return;
                CardItem *c = mPlayedCards[idx];
                if (!c) return;
                CardData d = c->cardData();
                d.rank = iterNextRank(d.rank);
                c->setCardData(d);   // setCardData 保留 faceUp=false，不打断翻面
            });
            QTimer::singleShot(t0 + 320, this, [this, idx]() {    // 翻回正面，新点数亮出来
                if (idx < 0 || idx >= mPlayedCards.size()) return;
                CardItem *c = mPlayedCards[idx];
                if (!c) return;
                c->flip();
                AudioManager::instance()->play(QStringLiteral("tarot2"), 1.0, 0.6);
            });
        }
        QTimer::singleShot(150 * iterIdx.size() + 700, this, flyOut);
    });
    anim->start(QAbstractAnimation::DeleteWhenStopped);
```

注意：`flyOut` 内的 `animatePlayedCardsToDiscardThen([this]() { ... })` 原有 lambda 体（finalizePlayedHand、mScoringInProgress=false、mHandLevelAnimating 清理、phase==Blind 的布局恢复）**逐字保留**，只是外面套了一层 `auto flyOut = [this]() { ... };`。`flyOut` 内层缩进可保持原样（diff 最小化），也可整体加一级缩进，按执行时观感选一种。

时序：第 k 张 t0=150(k+1) 开始翻背（240ms），t0+130 在背面期间改点数，t0+320 翻回（240ms），t0+560 完成；全部完成后 150n+700 触发 flyOut，牌带着新点数飞向弃牌堆。无迭代器牌时 `flyOut()` 立即执行，与现状路径完全一致。

- [ ] **Step 2: 构建**

运行构建命令。预期成功。

- [ ] **Step 3: 提交**

```powershell
git add src/ui/mainwindow.cpp
git commit -m "迭代器计分演出：总分计完后打出的迭代器牌翻面点数+1，再飞向弃牌堆"
```

---

### Task 6: 手动 QA

- [ ] **Step 1: 启动游戏**

```powershell
Start-Process build\Desktop_Qt_6_11_0_MinGW_64_bit-Debug\BalatroQt.exe
```

- [ ] **Step 2: 按清单验证（用户执行）**

1. **浅拷贝**：对两张牌使用浅拷贝 → 两张牌翻面、翻回后一致，同时出现相同 `0x??` 角标；对其中一张再用变化类塔罗（如倍率/换花色）→ 另一张同步、角标保留；玻璃链接牌碎裂 → 另一侧角标消失；打出链接中的一张 → 打出区该牌角标仍在。
2. **程设卡面增强**：定制牌组切到程设牌组，给 J/Q/K/A 上倍率/玻璃/钢铁/黄金/奖励 → 人像分别泛红/整卡半透明/金属灰/金色/泛蓝；牌堆查看、商店购卡、开标准包、收藏图鉴四处同样生效；2~10 的牌观感不变；默认牌组观感不变。
3. **迭代器计分**：打出迭代器牌 → 总分计完后该牌原地翻面、点数 +1、飞向弃牌堆；下次摸到点数与演出一致；多张迭代器同打错峰翻面；K 翻成 A、A 翻成 2。
4. **迭代器塔罗**：使用迭代器塔罗给手牌上增强 → 有翻面动画。
5. **回归**：不含迭代器/链接的普通计分时序与现状一致；倍速（设置里调高速度）下计分链正常、迭代器翻面节拍仍完整；暂停菜单打开/关闭不破坏动画。

发现问题 → systematic-debugging 定位后修复，回到对应任务补提交。
