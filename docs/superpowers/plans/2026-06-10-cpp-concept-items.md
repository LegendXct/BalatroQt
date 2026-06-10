# C++ 概念主题道具（运算符重载/类模板/迭代器/浅拷贝）实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 新增 2 张小丑牌（运算符重载、类模板）与 2 张塔罗牌（迭代器、浅拷贝），含专属卡面 UI 与完整玩法逻辑。

**Architecture:** 全部走现有扩展点——`JokerType`/`ConsumableType` 枚举 + `createJoker`/`createConsumable` 工厂 + `TriggerContext`/`UseContext` 效果回调；计分改动以 `ScoreEvent` 事件流为唯一载体（运算符重载=事件流后处理变换；类模板=OnPlayedHand xmult 事件）；卡面 UI 沿用程设牌组的"素材规格化 + 剪影遮罩"管线。游戏逻辑层（`src/game`、`src/card`）不依赖 UI。

**Tech Stack:** Qt6 / C++17 / CMake+Ninja。无测试框架——按项目约定，构建通过 + 临时 dump 校验 + 手动验收清单为验证手段。

**素材（已齐全，位于 `D:\leo\PKU\courses\professional\sem2\cxsjsx\Qt\图片素材`）：**
- `运算符重载-小丑.png`（浅灰底整卡设计）
- `类模板-小丑牌.png`（浅灰底整卡设计）
- `迭代器-塔罗牌.png`（黑底整卡设计）
- `浅拷贝-塔罗牌.png`（黑底整卡设计）

**已确认的设计决策（执行前需用户确认，见计划末尾"确认点"）：**
- D1 运算符重载：基础 chips/mult 不交换；事件流中**所有**筹码/倍率贡献互换（含卡牌筹码→倍率、×倍率→×筹码）。
- D2 类模板：实例化的那一手**立即**吃倍率（用户确认），之后命中同牌型持续生效；Boss 击败后重置为未实例化。
- D3 迭代器：作为新 `Enhancement::Iterator`（与现有增强互斥，替换式），塔罗选 1~2 张；点数+1 在每次**打出**后生效（无论是否计分），K→A→2 回绕。
- D4 浅拷贝：**完全镜像**（用户确认）——点数/花色/增强/版本/蜡封/debuff 全部同步；任一侧被摧毁则链接解除。

---

## 文件结构

| 文件 | 操作 | 职责 |
|---|---|---|
| `resources/images/joker_cs_overload.png` 等 4 个 | 新建 | 规格化后的卡面素材（284×380） |
| `CMakeLists.txt` / `resources/sounds/resources.qrc` | 修改 | 资源登记 |
| `src/card/joker.h/.cpp` | 修改 | 2 个新 JokerType + 工厂 + 稀有度/价格/蓝图兼容 |
| `src/card/jokeritem.cpp(.h)` | 修改 | 自定义卡面分支 `customCardPixmap()` |
| `src/card/consumable.h/.cpp` | 修改 | 2 个新 ConsumableType + 效果 + 塔罗池 |
| `src/card/consumableitem.cpp` | 修改 | 塔罗自定义卡面分支 |
| `src/card/carddata.h` | 修改 | `Enhancement::Iterator` |
| `src/card/carditem.cpp(.h)` | 修改 | Iterator 角徽绘制（静态 helper 供各渲染点复用） |
| `src/game/handevaluator.h` | 修改 | 新 `ScoreEventKind::ChipsXBoost` |
| `src/game/gamestate.h/.cpp` | 修改 | 交换后处理、类模板实例化/重置、迭代器点数+1、浅拷贝链接表+同步 |
| `src/game/shop.cpp` | 修改 | `jokerPool()` 加 2 个新小丑 |
| `src/ui/mainwindow.cpp` | 修改 | `playScoreEvent` 新事件分支、收藏列表/计数 |
| `src/ui/deckviewwidget.cpp` `shopwidget.cpp` `packopenwidget.cpp` | 修改 | Iterator 渲染 case |
| `src/ui/cardtooltipformat.*` | 修改 | Iterator/浅拷贝提示文案 |

---

## Task 0: 素材规格化（离线预处理）

**Files:** 新建 `resources/images/{joker_cs_overload,joker_cs_template,tarot_cs_iterator,tarot_cs_shallow}.png`

- [ ] **Step 0.1 探测边界**：复用程设牌组管线（见 git log 中 deckskin 素材处理）。对每张图先用 PowerShell 沿中线打印亮度剖面（参考 `deckskin` 工作流），确定卡体裁剪阈值：
  - 两张小丑图：浅灰底（约 #ECECEC），卡体白边更亮 → 从外向内"持续亮度 ≥246"为卡缘；若底色与卡缘难分（如黑桃情形），整图即卡体。
  - 两张塔罗图：纯黑底 → "持续亮度 ≥40"为卡缘。
- [ ] **Step 0.2 裁剪+规格化**：裁出卡体，等比缩放居中放入 284×380 画布（即图集格 142×190 的 2 倍），空隙用卡面边缘色填充；保存为上述 4 个资源名。
- [ ] **Step 0.3 目检**：Read 工具查看 4 张输出图，确认无底色残留、卡体居中。

## Task 1: 资源登记

**Files:** Modify `CMakeLists.txt`（`qt_add_resources(BalatroQt "textures")` 块）、`resources/sounds/resources.qrc`（`/textures` 段）

- [ ] **Step 1.1** 两处各追加 4 行新 png（路径格式照抄相邻 `deckskin_cs_*` 行）。
- [ ] **Step 1.2** `cmake --build build/Desktop_Qt_6_11_0_MinGW_64_bit-Debug` 通过。

## Task 2: 新小丑类型注册

**Files:** Modify `src/card/joker.h:68`（枚举末尾）、`src/card/joker.cpp`（createJoker / jokerRarity / jokerBaseCost / jokerBlueprintCompatible）、`src/game/shop.cpp`（`Shop::jokerPool()`）、`src/card/jokeritem.cpp`（spritePos 默认分支防御）

- [ ] **Step 2.1** `joker.h` 枚举末尾追加：
```cpp
    // 程设扩展：C++ 概念小丑
    OperatorOverload,   // 运算符重载：筹码/倍率贡献互换
    ClassTemplate,      // 类模板：首个牌型实例化，此后该牌型 ×对应张数
```
- [ ] **Step 2.2** `createJoker` 加 2 个 case（文案用项目的 `{C:...}` 着色标记，参考 Tarot_Death 的描述写法）：
```cpp
    case JokerType::OperatorOverload:
        j.name = "运算符重载";
        j.description = "计分时所有 {C:chips}筹码{} 与 {C:mult}倍率{}\n"
                        "的增减与倍乘 {C:attention}互换\n"
                        "{C:inactive}swap(mult, chips);";
        j.timing = TriggerTiming::Passive;
        j.effect = [](TriggerContext &) {};   // 交换在 GameState 事件流后处理统一执行
        break;
    case JokerType::ClassTemplate:
        j.name = "类模板";
        j.description = "{C:attention}template<?>{} 未实例化\n"
                        "本底注打出的第一种牌型将其实例化\n"
                        "此后该牌型 {X:mult,C:white}×对应张数{}\n"
                        "{C:inactive}Boss 击败后重置";
        j.timing = TriggerTiming::OnPlayedHand;
        j.effect = [](TriggerContext &) {};   // 实例化/倍率在 GameState 出牌管线专门处理
        break;
```
- [ ] **Step 2.3** `jokerRarity`：两者 `Rare`；`jokerBaseCost`：8；`jokerBlueprintCompatible`：两者返回 `false`（全局被动/带状态，蓝图复制无意义）。
- [ ] **Step 2.4** `Shop::jokerPool()`（shop.cpp:544 附近的池来源）追加两个类型；`JokerItem::spritePos` 给两个新类型返回 `{0,0}`（实际渲染走 Task 3 的自定义贴图分支，此处仅防御）。
- [ ] **Step 2.5** 构建通过（必有未处理 switch 警告则按编译器提示补全遗漏 case）。

## Task 3: 自定义卡面渲染（小丑 + 塔罗）

**Files:** Modify `src/card/jokeritem.h/.cpp`、`src/card/consumableitem.cpp`、`src/ui/mainwindow.cpp`（collectionJokerPixmap）、`src/ui/shopwidget.cpp`（小丑 offer 渲染处）

- [ ] **Step 3.1** `JokerItem` 增加静态函数：
```cpp
// 程设扩展小丑的专属整卡贴图；非扩展类型返回空 pixmap。
// 用图集 (0,0) 格（基础小丑）的 alpha 作圆角剪影，轮廓与图集小丑一致。
static QPixmap customCardPixmap(JokerType type);
```
实现：`OperatorOverload → ":/textures/images/joker_cs_overload.png"`、`ClassTemplate → ":/textures/images/joker_cs_template.png"`；加载→缩放 142×190→`CompositionMode_DestinationIn` 叠图集 (0,0) 格 alpha→静态缓存（`static QHash<int,QPixmap>`）。
- [ ] **Step 3.2** 接入渲染点：grep `Jokers.png` 与 `spritePos(` 找全部采样处（已知：jokeritem.cpp paint、shopwidget offerPixmap、mainwindow.cpp `collectionJokerPixmap`、packopenwidget 若有）。每处在画图集格之前加 `if (!JokerItem::customCardPixmap(type).isNull()) { 画自定义图; } else { 原逻辑 }`。
- [ ] **Step 3.3** `ConsumableItem::renderPixmap`（packopenwidget.cpp:976 证实是统一入口；先确认 consumableitem.cpp 内部 paint 也走它）顶部加分支：`Tarot_Iterator → tarot_cs_iterator.png`、`Tarot_ShallowCopy → tarot_cs_shallow.png`，同法用 Tarots.png (0,0) 格 alpha 遮罩。（本步可先写 if-分支引用 Task 7 的枚举名，与 Task 7 同一次提交编译。）
- [ ] **Step 3.4** 构建 + 临时验证：main.cpp 临时加 `--dump-customcards`（参考 deckskin 验证流程）把 4 张合成结果存 PNG，Read 目检后撤掉。

## Task 4: 运算符重载——计分交换

**Files:** Modify `src/game/handevaluator.h:26`（ScoreEventKind）、`src/game/gamestate.cpp`（事件后处理 + 两处 `chips * mult * xmult` 之前）、`src/ui/mainwindow.cpp`（playScoreEvent）

- [ ] **Step 4.1** `handevaluator.h` ScoreEventKind 追加：
```cpp
    ChipsXBoost,        // 运算符重载交换出的 ×筹码（蓝色 ×N 打在筹码计数上）
```
- [ ] **Step 4.2** gamestate.cpp 新增静态函数（放 applyResolvedJokerEffect 附近）：
```cpp
// 运算符重载：交换事件流里所有筹码/倍率贡献的方向（D1：基础值不交换），
// 然后从 baseChips/baseMult 出发按 UI 同款回放规则重算 chips/mult/xmult。
static ScoreEventKind overloadSwappedKind(ScoreEventKind k)
{
    switch (k) {
    case ScoreEventKind::ScoringCardChip:  return ScoreEventKind::EnhancementMult;
    case ScoreEventKind::EditionChip:      return ScoreEventKind::EditionMult;
    case ScoreEventKind::JokerChip:        return ScoreEventKind::JokerMult;
    case ScoreEventKind::EnhancementMult:  return ScoreEventKind::ScoringCardChip;
    case ScoreEventKind::EditionMult:      return ScoreEventKind::EditionChip;
    case ScoreEventKind::JokerMult:        return ScoreEventKind::JokerChip;
    case ScoreEventKind::EnhancementXMult:
    case ScoreEventKind::EditionXMult:
    case ScoreEventKind::SteelXMult:
    case ScoreEventKind::JokerXMult:       return ScoreEventKind::ChipsXBoost;
    default:                               return k;  // DollarGain/Retrigger/动画类不动
    }
}
```
- [ ] **Step 4.3** 实现 `void GameState::applyOperatorOverload(HandResult &result)`：持有未被禁用的 OperatorOverload 才执行。**先读 mainwindow `playScoreEvent` 确认 UI 对每种事件的数值回放规则**（chips += intValue / mult += intValue / mult-计数吸收 xmult 的具体方式），然后镜像实现：把每个事件 kind 换成 `overloadSwappedKind`，从基础值起重放，`ChipsXBoost` 为 `chips *= xmultValue`，最终写回 `result.chips/mult/xmult`（被吸收进 chips 的 x 项置回 1）。
- [ ] **Step 4.4** 在 gamestate.cpp 两处终算前调用（行锚点：`mPendingHandScore = result.chips * result.mult * result.xmult`（≈1003，确认其所在函数是否为预览；预览也要交换以保证所见即所得）与 ≈2216 的 `double score = ...` 之前）。
- [ ] **Step 4.5** UI：mainwindow `playScoreEvent` 加 `ChipsXBoost` case——复用 xmult 事件的演出样式，但目标计数为筹码：`mDisplayedChips *= ev.xmultValue`，飘字 "×N" 用筹码蓝（对照现有 JokerXMult case 的写法改色/改目标）。
- [ ] **Step 4.6** 构建 + 手动验收：买"运算符重载"，打一手含 Mult 增强牌+任意加筹码小丑的牌——确认 +4 倍率变成 +4 筹码、+30 筹码变成 +30 倍率、Glass ×2 打在筹码上，结算总分 = 屏幕动画终值。

## Task 5: 类模板——实例化与重置

**Files:** Modify `src/game/gamestate.cpp`（OnPlayedHand 管线 + Boss 击败路径）、`src/card/joker.cpp`（辅助函数）

- [ ] **Step 5.1** joker.cpp（或 gamestate.cpp 匿名空间）加映射：
```cpp
// 类模板：牌型 → 构成张数（D2 表）：高牌1 对子2 三条3 两对/四条4 其余5
static int templateHandCards(HandType t)
{
    switch (t) {
    case HandType::HighCard:      return 1;
    case HandType::Pair:          return 2;
    case HandType::ThreeOfAKind:  return 3;
    case HandType::TwoPair:
    case HandType::FourOfAKind:   return 4;
    default:                      return 5;
    }
}
```
- [ ] **Step 5.2** gamestate.cpp 出牌计分管线中、OnPlayedHand 小丑遍历同一阶段（锚点 ≈2186 的循环之后、终算之前），加专门段（需要 `Joker &` 可变访问，模式同 Popcorn ≈1189）：
```cpp
// 类模板：counter==0 为未实例化；首手实例化为当前牌型（当手不吃倍率，D2），
// 此后命中实例化牌型 → ×构成张数，事件用 JokerXMult 让 UI 走现成 xmult 演出。
for (int ji = 0; ji < mJokers.size(); ++ji) {
    Joker &j = mJokers[ji];
    if (j.type != JokerType::ClassTemplate || j.isDebuffed) continue;
    if (j.counter == 0) {
        j.counter = int(result.type) + 1;
        j.description = QString("{C:attention}template<%1>{} 已实例化\n"
                                "该牌型 {X:mult,C:white}×%2{}\n"
                                "{C:inactive}Boss 击败后重置")
                            .arg(HandEvaluator::handTypeName(result.type))
                            .arg(templateHandCards(result.type));
        emit jokersChanged();
    } else if (j.counter == int(result.type) + 1) {
        const double x = templateHandCards(result.type);
        result.xmult *= x;
        result.events.append({ ScoreEventKind::JokerXMult, -1, -1, ji, 0.0, x });
    }
}
```
注意预览路径（≈950 的同型循环）同样补这段的"只读版"（命中才乘，不实例化、不发事件），否则预览分数与实算不一致。
- [ ] **Step 5.3** Boss 击败重置：grep gamestate.cpp 中 ante 递增/boss 击败分支（`mAnte++` 或 bossDefeated 相关），追加：
```cpp
for (Joker &j : mJokers)
    if (j.type == JokerType::ClassTemplate) {
        j.counter = 0;
        j.description = createJoker(JokerType::ClassTemplate).description;
    }
emit jokersChanged();
```
- [ ] **Step 5.4** 构建 + 手动验收：买"类模板"→打对子（无倍率，描述变 template&lt;对子&gt;）→再打对子（×2 演出）→打同花（无加成）→过 Boss → 描述复位、重新可实例化。

## Task 6: Enhancement::Iterator（增强 + 卡面角徽 + 点数+1）

**Files:** Modify `src/card/carddata.h`（枚举）、`src/card/carditem.h/.cpp`、`src/ui/deckviewwidget.cpp:530`、`src/ui/shopwidget.cpp:1664`、`src/ui/packopenwidget.cpp:991`、`src/ui/cardtooltipformat.*`、`src/game/gamestate.cpp`（出牌后点数+1）、`src/game/handevaluator.cpp`（确认增强 switch 默认安全）

- [ ] **Step 6.1** `carddata.h` Enhancement 枚举**末尾**追加 `Iterator`（勿插中间——多处 switch/UI 依赖既有值次序的稳健性更高）。
- [x] **Step 6.2**（已按用户补充素材调整）用户提供了专属蒙版素材 `迭代器-卡牌增强效果.png`：
预处理为 `resources/images/enh_cs_iterator.png`（裁剪卡体→284×380 规格化→低饱和白/灰/黑像素转透明，只留彩色装饰框）。
`CardItem::drawIteratorOverlay(QPainter*, QRectF)` 绘制该蒙版；`paintFront` 在牌面之后、版本 shader 之前叠加
（缓存 key 已含 enhancement，自动失效正确）。deckview/shop/packopen/收藏增强页复用同一函数。
- [ ] **Step 6.3** 三个 UI 合成路径（deckviewwidget.cpp:530 / shopwidget.cpp:1664 / packopenwidget.cpp:991 的增强 switch）各加 `case Enhancement::Iterator: eCol = 1; eRow = 0; break;`（白底），并在牌面绘制后补 `CardItem::drawIteratorBadge(...)`。收藏-增强页（mainwindow `collectionEnhancerRect`）同样补 case + 在列表里展示新增强（文案见 6.4）。
- [ ] **Step 6.4** `cardtooltipformat` 增强说明加：`迭代器：每次打出后点数 +1（K→A，A→2）`。
- [ ] **Step 6.5** gamestate.cpp 出牌管线：计分结束、打出牌移入弃牌堆**之前**（grep playHand 中把 played 牌从 mHand 移除/进弃牌堆的位置），对每张 `enhancement == Enhancement::Iterator` 的打出牌执行：
```cpp
// 迭代器：每次打出后点数 +1，K→A→2 回绕（与"力量"塔罗的 nextRank 不同，那个 A 封顶）。
static Rank iterNextRank(Rank r)
{
    return (r == Rank::Ace) ? Rank::Two : static_cast<Rank>(int(r) + 1);
}
```
注意必须改到**随牌流转的那份 CardData**（进弃牌堆的对象），保 uid 不变；handevaluator.cpp 里增强相关 switch 确认 Iterator 走 default（无计分效果）。
- [ ] **Step 6.6** 构建 + 手动验收：用 Task 7 的塔罗给 K 打上迭代器→出牌→弃牌堆查看（或下轮抽回）确认变 A 且角徽还在→再出牌变 2。

## Task 7: 迭代器塔罗牌

**Files:** Modify `src/card/consumable.h:60`（枚举末尾）、`src/card/consumable.cpp`（kindOf / createConsumable / randomTarotType）、`src/ui/mainwindow.cpp`（collectionConsumableOrder 塔罗列表 + 计数）

- [ ] **Step 7.1** `consumable.h` 枚举**最末尾**（Spectral_BlackHole 之后）追加——不要插进塔罗段中间，塔罗/星球/幻灵的图集格映射按枚举序号偏移，插中间会移位：
```cpp
    // ── 程设扩展（追加在枚举末尾，避免挪动 Tarots.png 图集映射） ──
    Tarot_Iterator,      // 选≤2张：迭代器增强（每次打出点数+1，K→A→2）
    Tarot_ShallowCopy,   // 选2张：左牌浅拷贝右牌，两牌共享状态（增强/版本/蜡封/debuff）
```
- [ ] **Step 7.2** `kindOf` 两个新 case 返回 `ConsumableKind::Tarot`；`createConsumable` 加：
```cpp
    case ConsumableType::Tarot_Iterator:
        c.name = "迭代器";
        c.description = "至多 {C:attention}2{} 张选定手牌\n"
                        "获得 {C:attention}迭代器{} 增强\n"
                        "每次打出后点数 {C:attention}+1{}（K→A→2）";
        c.needsSelection = 1; c.maxSelection = 2;
        c.effect = [](UseContext &ctx) { enhanceSelected(ctx, Enhancement::Iterator, 2); };
        break;
```
- [ ] **Step 7.3** `randomTarotType` 池追加 `Tarot_Iterator`、`Tarot_ShallowCopy`（与 Task 8 同次提交）。
- [ ] **Step 7.4** mainwindow `collectionConsumableOrder` 的 Tarot 列表追加两项；收藏页塔罗计数文案（grep "22"/塔罗 计数标签）改为 24。
- [ ] **Step 7.5** 构建 + 手动验收：商店刷出/秘术包开出"迭代器"（卡面为新素材）→ 使用并选 2 张牌 → 卡面出现「++」角徽。

## Task 8: 浅拷贝塔罗牌（共享状态链接）

**Files:** Modify `src/card/consumable.cpp`（效果）、`src/game/gamestate.h/.cpp`（链接表 + 同步）、`src/ui/cardtooltipformat.*`（可选提示）

- [ ] **Step 8.1** consumable.cpp 加效果（紧挨 deathConvertSelected，模式一致）：
```cpp
// 浅拷贝：左牌变为右牌的副本（保 uid，同死神），并登记共享链接——
// 此后任一侧的 增强/版本/蜡封/debuff 变化同步到另一侧（GameState::syncShallowLinks）。
static void shallowCopySelected(UseContext &ctx)
{
    if (ctx.selectedHandIdx.size() != 2) return;
    auto &hand = ctx.state.handMutable();
    int a = ctx.selectedHandIdx[0], b = ctx.selectedHandIdx[1];
    if (a < 0 || a >= hand.size() || b < 0 || b >= hand.size() || a == b) return;
    const int aUid = hand[a].uid;
    hand[a] = hand[b];
    hand[a].uid = aUid;
    ctx.state.registerShallowLink(aUid, hand[b].uid);
    ctx.state.notifyHandChanged();
}
```
`createConsumable` 条目：名"浅拷贝"，描述 `选定 {C:attention}2{} 张：靠左浅拷贝靠右\n两张牌{C:attention}共享{}增强/版本/蜡封/debuff\n{C:inactive}（拖动可调位置）`，`needsSelection = 2; maxSelection = 2;`。
- [ ] **Step 8.2** gamestate.h 私有成员 + 接口：
```cpp
// 浅拷贝链接：两 uid 共享状态字段（增强/版本/蜡封/debuff）。snapshot 用于判定哪一侧发生了变化。
struct ShallowLink { int uidA = -1, uidB = -1;
                     Enhancement enhA, enhB; Edition edA, edB; Seal sealA, sealB;
                     bool dbA = false, dbB = false; };
QVector<ShallowLink> mShallowLinks;
public:
    void registerShallowLink(int uidA, int uidB);
    void syncShallowLinks();    // 任何卡牌状态可能变化的时机调用
```
- [ ] **Step 8.3** 实现 `syncShallowLinks`：
  1. 按 uid 在 手牌/牌堆/弃牌堆 中查卡（先 grep Deck 的可变访问接口；若无则给 Deck 加 `CardData *findByUid(int)`，手牌直接遍历 `mHand`）。任一侧找不到（玻璃碎/上吊人摧毁）→ 移除该链接。
  2. 对找到的两侧：与 snapshot 比对四个字段——发生变化的一侧为"写方"，把四字段拷给另一侧；双方都变（罕见，如群体 debuff）则以 A 侧为准。
  3. 刷新 snapshot。若有同步发生 → `emit handChanged(...)`（按 notifyHandChanged 现行签名）。
- [ ] **Step 8.4** 调用点：`notifyHandChanged()` 内部（发信号前）；Boss debuff 应用之后（grep `applyBossDebuffs` 调用处）；迭代器/力量塔罗点数+1 之后无需（D4：点数不同步）。`registerShallowLink` 记得初始化 snapshot 为当前两卡状态。
- [ ] **Step 8.5** 构建 + 手动验收：浅拷贝链接两张牌 → 用"皇后"给其中一张上 Mult 增强 → 另一张同步变化；进 Boss"被 debuff 的花色"两张同步禁用；打碎其中一张（玻璃）→ 另一张不再联动。

## Task 9: 收尾——计数、构建、总验收

- [ ] **Step 9.1** 收藏页"小丑 150/150"计数（mainwindow ≈2973 `addMain(0, 0, "小丑", "150 / 150")`）改 152/152；`collectionJokerOrder()` 末尾追加两个新小丑类型。
- [ ] **Step 9.2** 全量构建 + 启动跑一局 demo 流程（演示模式关），按 Task 4-8 验收清单逐项过。
- [ ] **Step 9.3** 提交（按功能分多次 commit：素材/小丑注册/渲染/运算符重载/类模板/迭代器/浅拷贝/收尾）。

---

## Self-Review 备注

- 规格覆盖：4 个道具 ↔ Task 4/5/6+7/8；UI 素材 ↔ Task 0/1/3；"手牌增强 UI" ↔ Task 6.2/6.3。
- 已知风险点（执行时先读后写）：`playScoreEvent` 的数值回放细节（Task 4.3 依赖）、Deck 的牌堆可变访问（Task 8.3）、boss 击败钩子位置（Task 5.3）。三处都给了 grep 锚点。
- 类型一致性：`ChipsXBoost`、`Enhancement::Iterator`、`Tarot_Iterator/Tarot_ShallowCopy`、`registerShallowLink/syncShallowLinks`、`templateHandCards/iterNextRank/overloadSwappedKind` 全文统一。
