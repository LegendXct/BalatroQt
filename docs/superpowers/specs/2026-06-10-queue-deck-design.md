# 队列牌组（Queue Deck）设计文档

日期：2026-06-10
状态：已与用户确认

## 目标

为课程作业引入"程设元素"：新增**游戏牌组**体系（区别于纯 UI 换肤的 `DeckSkin`），
首批两个牌组——基础牌组、队列牌组。牌组在开始游戏前选择，影响游戏核心逻辑。
代码层面用 **继承 + 多态 + 纯虚函数 + 智能指针**（对标程设课第 04–06 讲、第 10 讲）实现牌组体系。

## 玩法规则（队列牌组）

1. **队列序**：手牌严格按抽牌顺序排列，最左 = 队首（最早抽到），补牌从右侧入队尾。
   全程禁用按点数/花色排序（按钮禁用），禁用手牌拖拽重排。
2. **队首窗口**：只有队列最前 **6** 张可被选中（窗口内自由勾选任意子集，出牌/弃牌上限仍为 5 张）。
   第 7 张起的手牌处于"排队中"状态：不可选中，视觉上压暗（约 0.6 不透明度），点击播放拒绝抖动。
3. **补偿**：每回合 **+1 出牌次数、+1 弃牌次数**（对标原版 `back.lua` 红牌组 +1 弃牌 / 蓝牌组 +1 出牌的强度）。
4. **补牌语义**：所有进入手牌的新牌（开局发牌、出/弃后补牌、DNA 复制、巨蛇固定补 3 等）一律 append 入队尾。
5. **Boss 兼容**：
   - 蔚蓝铃铛（强制锁定一张选中）：锁定牌改为只在**窗口内**（前 6 张）随机挑选，规则不冲突。
   - 巨蛇/鱼/轮子/标记等补牌、翻面类效果照常。
   - 灵媒（必须打 5 张）等出牌张数限制照常——窗口 6 ≥ 5，永远可满足。
6. 基础牌组 = 现状行为，零改动。

## 架构

### 新增 `src/game/gamedeck.h` / `gamedeck.cpp`

```cpp
// 游戏牌组体系：抽象基类 + 派生类（继承/多态/纯虚函数）。
class GameDeckType {
public:
    virtual ~GameDeckType() = default;                  // 虚析构
    virtual QString name() const = 0;                   // 纯虚
    virtual QString description() const = 0;            // 纯虚（选牌组界面文案）
    virtual int  extraHands()    const { return 0; }    // 每回合出牌次数修正
    virtual int  extraDiscards() const { return 0; }    // 每回合弃牌次数修正
    virtual int  selectionWindow() const;               // 可选牌窗口，默认 INT_MAX（不限）
    virtual bool allowHandSort() const { return true; } // 是否允许排序/重排
};

class BaseGameDeck  : public GameDeckType { /* 基础牌组：全默认 */ };
class QueueGameDeck : public GameDeckType {
    // name="队列牌组"; extraHands/extraDiscards=+1; selectionWindow=6; allowHandSort=false
};
```

窗口大小、补偿值定义在 `src/utils/constants.h`（`QUEUE_DECK_WINDOW=6` 等），不硬编码。

### `GameState` 改动

- 持有 `std::unique_ptr<GameDeckType> mGameDeck`（默认 `BaseGameDeck`），
  提供 `setGameDeck(std::unique_ptr<GameDeckType>)`（仅开局前调用）与 `const GameDeckType &gameDeck() const`。
- `startGame()`：若 `!allowHandSort()`，`mSortMode` 置 `Manual`（替代现有无条件 `ByRank`）。
- `startBlind()`：`mHandsLeft` / `mDiscardLeft` 算式各加 `mGameDeck->extraHands()/extraDiscards()`。
- 商店侧栏预览 `extraHandsPerRoundPreview()` / `extraDiscardsPerRoundPreview()` 同步加上牌组修正。
- `playCards()` / `discardCards()`：模型层校验所有 indices < `selectionWindow()`，违规则拒绝（防御性，UI 已拦）。
- `sortHandByRank()/BySuit()`：`!allowHandSort()` 时直接 return。
- `refreshCeruleanForced()`：锁定目标限定在窗口内随机。

### UI 改动

- **选牌组界面**：新建 `src/ui/deckselectwidget.h/.cpp`（overlay 模式，遵循"不用原生 QDialog"的项目规矩）。
  主菜单"开始游戏"→ 显示牌组选择层：两张牌组卡背并排（选中高亮描边），下方名称 + 效果描述，
  底部"开始游戏"/"返回"。队列牌组卡背由现有卡背做色相偏移生成，不引入新贴图。
- **`MainWindow`**：
  - `startNewRunFromOptions()` 前插入牌组选择流程，把所选牌组 set 进 `GameState`。
  - `onCardClicked()`：队列牌组下，index ≥ 窗口的牌不可选（拒绝抖动）。
  - 排序按钮：队列牌组局内禁用（置灰）。
  - 手牌拖拽重排：队列牌组下禁用（拖动松手弹回原位）。
  - 窗口分界视觉：窗口外手牌 `setOpacity(≈0.6)`；手牌区加一个仅队列牌组显示的小"队首"标记。
- **`CMakeLists.txt`**：`gamedeck.h/.cpp`、`deckselectwidget.h/.cpp` 共 4 个新文件加入 `qt_add_executable` 列表。

## 数据流

主菜单 → 开始游戏 → DeckSelectWidget（选牌组）→ MainWindow 把 `unique_ptr<GameDeckType>` 注入 GameState
→ `startGame()`（锁排序模式）→ `startBlind()`（+1/+1）→ 局内：UI 按 `gameDeck()` 查询窗口/排序权限做交互限制，
模型层在 play/discard 入口二次校验。

## 错误处理

- 模型层窗口校验失败：`qWarning` + 忽略本次操作（不崩溃、不部分执行）。
- 牌组未设置：`GameState` 构造时默认 `BaseGameDeck`，任何路径下 `mGameDeck` 非空。

## 验证

- 项目无测试套件/CI，验证 = `cmake --build` 通过 + 手动开一局队列牌组逐条核对规则
  （窗口锁定、+1/+1、排序禁用、弃牌推进队列、蔚蓝铃铛锁定在窗口内）。
- 基础牌组回归：开一局确认行为与改动前一致（排序按钮可用、无窗口限制）。

## 非目标（YAGNI）

- 不做存档/牌组解锁条件；
- 不改 `DeckSkin`（换肤与游戏牌组正交，两者可同时生效）。

---

# 追加设计（2026-06-10 第二批，已与用户确认）

## A. Bug 修复：队首标记进商店后未消失

`layoutHandCards()` 在 `n == 0` 时提前 return，没有隐藏 `mQueueHeadLabel`——
回合结束手牌收回牌组后标记残留在牌桌上。修复：`n == 0` 分支里先
`mQueueHeadLabel->setVisible(false)` 再 return。

## B. 栈牌组（Stack Deck）

与队列牌组一致的部分：+1 出牌/+1 弃牌、禁排序/拖拽重排（`allowHandSort=false`，
手牌锁定抽牌序）、卡背色相偏移区分。

差异：**不限选牌窗口**（全手牌可选），唯一约束是**最新到手的"栈顶"牌
（`mHand` 末位）必须被选中**才能出牌/弃牌——交互仿蔚蓝铃铛：栈顶牌自动强制
选中、点击不可取消。

### 实现

- `GameDeckType` 新增虚函数：
  - `bool mustIncludeNewest() const { return false; }` —— 栈牌组返回 true；
  - `QString handMarkerText() const { return {}; }` —— 队列返回"队首 →"、栈返回"← 栈顶"、基础返回空（不显示）；
  - `bool handMarkerAtTail() const { return false; }` —— 栈牌组 true（标记挂手牌行尾）。
- `StackGameDeck` 派生类 + `GameDeckId::Stack` + 工厂 case。
- `GameState`：
  - `playCards`/`discardCards`：`mustIncludeNewest()` 时校验 indices 含 `mHand.size()-1`，否则拒绝；
  - `findBestPlay`：`mustIncludeNewest()` 时栈顶作为强制包含项（与蔚蓝铃铛 forcedIdx 同机制）。
- `MainWindow`：
  - `refreshHand`：栈牌组下自动强制选中栈顶（仿蔚蓝铃铛块，限 Blind 阶段且非计分中）；
  - `onCardClicked`：栈顶牌不可取消选中（与 isForced 同分支）；
  - `layoutHandCards` 标记块泛化：文案/位置由 `handMarkerText()`/`handMarkerAtTail()` 决定。
- `DeckSelectWidget`：第三个选项（色相偏移取另一角度），面板宽度 560→640 容纳三张卡背。
- 蔚蓝铃铛与栈顶强制可共存（两张强制牌 ≤ 5 上限）；灵媒（必须 5 张）可满足。
