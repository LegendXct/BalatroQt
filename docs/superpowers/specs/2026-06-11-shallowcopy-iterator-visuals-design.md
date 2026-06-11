# 浅拷贝/迭代器视觉表现 + 程设卡面增强修复 设计

日期：2026-06-11
状态：已确认（用户选定：浅拷贝=共享地址角标；卡面修复=半透明叠化）

## 背景

`72d45f1 程设特色` 引入了两张程设扩展塔罗：

- **浅拷贝**：选 2 张手牌，左牌变为右牌副本并登记 `ShallowLink`，此后任一侧
  状态变化经 `GameState::syncShallowLinks()` 镜像到另一侧（任一侧被摧毁则解除）。
- **迭代器**（增强）：带该增强的牌每次打出后点数 +1（K→A→2），在
  `finalizePlayedHand()` 中生效。

四个问题：

1. 链接的两张牌在 UI 上**没有任何视觉联系**，玩家无从知道哪两张共享状态。
2. 程设牌组（DeckSkin::ChengShe）的 J/Q/K/A 整卡人像**盖住了增强底图**：
   `paintFront` 先画 Enhancers 增强底、再画卡面格子；默认图集人像四周透明、
   增强色可透出，程设整卡设计图不透明，背景式增强（奖励/倍率/万能/幸运/
   玻璃/钢铁/黄金）全部不可见。石头牌不画卡面，幸免。
3. 迭代器点数 +1 发生在数据层，打出的牌飞走前 UI 没有任何"点数变了"的演出。
4. 使用浅拷贝时没有动画：门卫 `usesOriginalTarotFlip()` 名单没列
   `Tarot_ShallowCopy`（也没列 `Tarot_Iterator`），走了无动画直刷路径。

## 设计

### 1. 浅拷贝共享地址角标

程设隐喻：两张牌是指向同一块内存的两个指针，牌面上印同一个"地址"。

- **模型暴露**：`GameState` 新增只读访问器
  `QVector<QPair<int,int>> shallowLinkPairs() const`（返回 uid 对），
  不暴露私有 `ShallowLink` 结构。
- **地址文案**：`0x` + 两侧较小 uid 的大写十六进制（如 `0x4D`）。
  uid 进程内唯一且链接期间不变 → 同对两侧恒同值、不同对天然不同。
- **CardItem 渲染**：新增 `setLinkTag(const QString&)`；`paint()` 在缓存
  pixmap 绘制完成后叠画角标（**不进缓存 key**——角标是少量文本，直接画开销
  可忽略，避免缓存条目膨胀）。样式：牌面下缘中央深色圆角小底板
  （rgba(20,24,28,210)）+ 白色像素字（m6x11plus），高约 22/190 卡高。
- **挂接时机**：`refreshHand()` 末尾统一对照 `shallowLinkPairs()` 给两侧
  CardItem 设置角标、其余清空——链接解除（一侧被摧毁）后自然消失。
  打出区 CardItem 在创建时同样对照设置（打出的牌仍可能在链接中）。
  牌堆查看（DeckViewWidget）如复用 CardItem 则顺手设置，非必做。

### 2. 程设卡面增强：~~半透明叠化~~ → 增强边框叠加（2026-06-11 实测修订）

> **修订**：半透明叠化实装后用户反馈双层都显得太透。实测 Enhancers.png：
> 玻璃以外的增强格子中心 alpha=255 全不透明，"增强整张叠在人像上"会盖死人像。
> 最终方案：人像在下 100% 不透明；增强层叠在人像之上、同样 100% 不透明，
> 但中心开圆角窗形成"增强边框"（玻璃贴图自带半透明，整张直接叠加不开窗）；
> 被边框盖住的点数/花色角标从原版图集原位回贴到最上层。
> API：`DeckSkin::enhancementOverArt()` 判定 + `DeckSkin::drawEnhancementOverArt()`
> 绘制，取代原 `faceOpacity()`；五处合成路径同步。以下原方案描述仅留档。

### 2'.（已废弃的原方案）半透明叠化

- `paintFront` 画卡面格子处：当 **DeckSkin 当前格子被整卡替换**
  （`DeckSkin::current()==ChengShe && rank>=Jack`，提供
  `DeckSkin::isFullArtCell(Rank)` 之类的判定助手）**且增强为背景式**
  （Bonus/Mult/Wild/Lucky/Glass/Steel/Gold）时，
  以 `bp.setOpacity(0.70)` 绘制人像，画完恢复 1.0。
- 效果：倍率牌人像泛红、钢铁牌泛金属灰、玻璃牌整卡半透明，
  八种增强统一生效；无增强时人像保持全幅不透明，观感不变。
- 缓存 key 已含 enhancement + DeckSkin::generation，**无需改 key**
  （透明度由 key 字段决定，确定性的）。
- 该修复在 QPainter 合成层，与版本（Foil/Holo/...）shader 正交：
  shader 作用于合成后的 body，路径不变。

### 3. 迭代器计分翻面动画

- **插入点**：`animateScoreTotalThenFinalize` 的 finished 回调内，总分滚动
  结束之后、`animatePlayedCardsToDiscardThen` 之前。
- **目标牌**：`mPlayedCards` 中 `enhancement==Iterator` 且不在
  `mShatteredPlayedIndices` 的项。无目标 → 流程与现状完全一致。
- **演出**（复用塔罗双翻面节奏，150ms 间隔错峰）：
  1. 每张目标牌 `flip()` 翻向背面（240ms，card1 音效）；
  2. 翻面中点后对该 CardItem `setCardData(rank = 下一点数)`——仅改 UI
     显示副本，数据层的真正 +1 仍由 `finalizePlayedHand()` 完成；
  3. 再 `flip()` 翻回正面（tarot2 音效），新点数"翻"出来；
  4. 全部翻完 + 短缓冲后再调 `animatePlayedCardsToDiscardThen`。
- **点数递推共享**：`iterNextRank` 目前是 gamestate.cpp 内部自由函数，
  迁为 `carddata.h` 的 inline 自由函数，模型与 UI 共用一份 K→A→2 规则。
- **计时**：用 `scheduleGame`（尊重倍速/暂停），不用裸 QTimer。

### 4. 浅拷贝/迭代器塔罗翻面动画

- `usesOriginalTarotFlip()` 的 switch 增加 `Tarot_ShallowCopy` 与
  `Tarot_Iterator` 两个 case。
- 现有序列自动生效：选中的牌翻背 → `useConsumable` 改数据 →
  `refreshHand`（`setCardData` 保留 faceUp=false）→ 翻回。
  浅拷贝两张牌都在 `sel` 中，两张都翻；翻回时左牌已是右牌副本，
  且 refreshHand 已给两侧挂上地址角标（与设计 1 自然衔接）。

## 验证（无测试套件，构建 + 手动 QA）

1. 用浅拷贝：两张牌翻面，翻回后两张一致、同时出现相同 `0x??` 角标；
   再对其中一张用变化类塔罗，另一张同步且角标保留；玻璃链接牌碎裂后
   另一侧角标消失。
2. 程设牌组下给 J/Q/K/A 上倍率/玻璃/钢铁/黄金/奖励增强：人像分别
   泛红/半透明/金属灰/金色/泛蓝；2~10 与默认牌组观感不变。
3. 打出迭代器牌：总分计完后该牌原地翻面、点数 +1，再飞向弃牌堆；
   下回合摸到该牌时点数与演出一致；多张迭代器同打错峰翻面。
4. 用迭代器塔罗给手牌上增强：有翻面动画。
5. 回归：无链接/无迭代器时计分链时序与现状一致；倍速设置下动画跟随。
