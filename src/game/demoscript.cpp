#include "demoscript.h"
#include "shop.h"
#include <algorithm>

bool DemoScript::sActive       = false;
int  DemoScript::sShopVisit    = 0;
int  DemoScript::sShopRerolls  = 0;
int  DemoScript::sBlindEntered = 0;

void DemoScript::setActive(bool on)
{
    sActive = on;
    // 任何时候切到 on/off 都把计数器归零——避免上一局残留的 shopVisit 把新局打乱。
    sShopVisit = 0;
    sShopRerolls = 0;
    sBlindEntered = 0;
}

void DemoScript::onStartGame()
{
    sShopVisit = 0;
    sShopRerolls = 0;
    sBlindEntered = 0;
}

void DemoScript::onEnterShop()
{
    ++sShopVisit;
    sShopRerolls = 0;
}

void DemoScript::onShopReroll() { ++sShopRerolls; }
void DemoScript::onEnterBlind() { ++sBlindEntered; }

struct ScriptedCardSpec {
    Rank rank;
    Suit suit;
    bool iterator = false;
};

// 每个盲注的固定抽牌顺序。小盲的 13 张覆盖起手及两次补牌，后两关固定起手 8 张。
static QVector<ScriptedCardSpec> scriptedDrawsForBlind(int blindNo)
{
    using R = Rank; using S = Suit;
    switch (blindNo) {
    case 1:
        return { {R::Three, S::Clubs},    {R::Two,   S::Spades},
                 {R::Five,  S::Spades},   {R::Five,  S::Diamonds},
                 {R::Six,   S::Hearts},   {R::Seven, S::Clubs},
                 {R::Eight, S::Diamonds}, {R::Nine,  S::Spades},
                 {R::Jack,  S::Diamonds}, {R::Jack,  S::Hearts},
                 {R::King,  S::Clubs},    {R::Four,  S::Spades},
                 {R::Two,   S::Hearts} };
    case 2:
        return { {R::Queen, S::Hearts}, {R::Three, S::Diamonds},
                 {R::Jack,  S::Hearts}, {R::Eight, S::Hearts},
                 {R::Six,   S::Hearts}, {R::Two,   S::Spades},
                 {R::Queen, S::Spades}, {R::Four,  S::Clubs} };
    case 3:
        // 两张迭代器牌必须匹配增强状态，不能误取牌组里的天然红心 Q。
        return { {R::Ace,   S::Hearts},        {R::Queen, S::Hearts, true},
                 {R::Queen, S::Hearts, true},  {R::Eight, S::Hearts},
                 {R::Five,  S::Hearts},      {R::Three, S::Spades},
                 {R::Seven, S::Clubs},       {R::Two,   S::Diamonds} };
    default: return {};
    }
}

void DemoScript::reorderDeckForNextBlind(QVector<CardData> &pile)
{
    // startBlind() 先调用 onEnterBlind()，再调用 Deck::reset()，因此这里的计数就是当前盲注。
    const auto want = scriptedDrawsForBlind(sBlindEntered);
    if (want.isEmpty()) return;

    int frontPos = 0;
    for (const auto &w : want) {
        // 在 [frontPos, end) 中找目标卡；Boss 的两个红心 Q 还必须是迭代器增强。
        auto it = std::find_if(pile.begin() + frontPos, pile.end(),
                               [&w](const CardData &c) {
                                   return c.rank == w.rank && c.suit == w.suit
                                       && (!w.iterator || c.enhancement == Enhancement::Iterator);
                               });
        if (it == pile.end()) continue;
        if (std::distance(pile.begin() + frontPos, it) > 0)
            std::iter_swap(pile.begin() + frontPos, it);
        ++frontPos;
    }
}

static ShopOffer makeJokerOffer(JokerType t, Edition e = Edition::None)
{
    ShopOffer o;
    o.kind = OfferKind::Joker;
    o.joker = t;
    o.jokerEdition = e;
    o.cost = jokerBaseCost(t);
    return o;
}

static ShopOffer makeConsumableOffer(OfferKind kind, ConsumableType type, int cost = 3)
{
    ShopOffer o;
    o.kind = kind;
    o.consumable = type;
    o.cost = cost;
    return o;
}

void DemoScript::scriptedShopOffers(QVector<ShopOffer> &out, int slotCount)
{
    out.clear();
    if (sShopVisit == 1 && sShopRerolls == 0) {
        // 第一商店初始货架：函数重载小丑 + 迭代器塔罗。
        out.append(makeJokerOffer(JokerType::OperatorOverload));
        out.append(makeConsumableOffer(OfferKind::Tarot, ConsumableType::Tarot_Iterator));
    } else if (sShopVisit == 1) {
        // 重掷后：类模板小丑 + 浅拷贝塔罗；扩容后的第三格放普通陪衬小丑。
        out.append(makeJokerOffer(JokerType::ClassTemplate));
        out.append(makeConsumableOffer(OfferKind::Tarot, ConsumableType::Tarot_ShallowCopy));
        if (slotCount >= 3) out.append(makeJokerOffer(JokerType::JollyJoker));
    } else if (sShopVisit == 2) {
        // 第二商店：木星 + 两张普通陪衬小丑，剧本只购买木星。
        out.append(makeConsumableOffer(OfferKind::Planet, ConsumableType::Planet_Jupiter));
        out.append(makeJokerOffer(JokerType::JollyJoker));
        out.append(makeJokerOffer(JokerType::Misprint));
    } else if (sShopVisit == 3) {
        // Boss 后的商店只展示三张标准比例、视觉差异明显的普通小丑。
        out.append(makeJokerOffer(JokerType::Joker));        // 经典 +4 倍率
        out.append(makeJokerOffer(JokerType::Bull));         // 每金币 +2 筹码
        out.append(makeJokerOffer(JokerType::Fibonacci));    // A/2/3/5/8 +8 倍率
    } else {
        out.append(makeJokerOffer(JokerType::GreedyJoker));
        out.append(makeJokerOffer(JokerType::LustyJoker));
        out.append(makeJokerOffer(JokerType::WrathfulJoker));
    }
    // 第二商店固定写入木星 + 两张陪衬小丑；其它阶段按实际槽位补足。
    while (out.size() < slotCount) {
        ShopOffer extra;
        if (scriptedExtraShopOffer(extra)) out.append(extra);
        else out.append(makeJokerOffer(JokerType::Joker));
    }
}

bool DemoScript::scriptedExtraShopOffer(ShopOffer &out)
{
    // 第一商店购买扩容后新增第三格，固定补一张闪箔蓝图。
    if (sShopVisit != 1 || sShopRerolls != 0) return false;
    out = makeJokerOffer(JokerType::Blueprint, Edition::Foil);
    return true;
}

JokerType DemoScript::scriptedLegendaryJoker()
{
    // Soul 塔罗在演示模式下固定开出特里布莱（人头牌 ×2 倍率）。
    return JokerType::Triboulet;
}


void DemoScript::scriptedVoucherOffers(QVector<ShopOffer> &out)
{
    out.clear();
    if (sShopVisit == 1) {
        ShopOffer o;
        o.kind = OfferKind::Voucher;
        o.voucher = VoucherType::Overstock;
        o.cost = voucherData(VoucherType::Overstock).cost;
        out.append(o);
    }
}

void DemoScript::scriptedBoosterOffers(QVector<ShopOffer> &out)
{
    out.clear();
    auto makePack = [](PackKind k, PackSize sz, int cost) {
        ShopOffer o;
        o.kind = OfferKind::Pack;
        o.pack = k;
        o.packSize = sz;
        o.packVariant = 0;
        o.cost = cost;
        return o;
    };
    if (sShopVisit == 1) {
        // 普通塔罗包固定包含灵魂；普通游戏卡包展示指定增强/蜡封/版本组合。
        out.append(makePack(PackKind::Arcana,   PackSize::Normal, 4));
        out.append(makePack(PackKind::Standard, PackSize::Normal, 4));
    } else if (sShopVisit == 2) {
        // 第二商店不再出现塔罗包，避免和第一商店的固定路线冲突。
        out.append(makePack(PackKind::Standard, PackSize::Normal, 4));
        out.append(makePack(PackKind::Buffoon,  PackSize::Normal, 4));
    } else if (sShopVisit == 3) {
        // 最后一次商店：一个标准包 + 一个小丑包做收尾，演示牌堆扩充和最后捡漏的可能性。
        out.append(makePack(PackKind::Standard, PackSize::Normal, 4));
        out.append(makePack(PackKind::Buffoon,  PackSize::Normal, 4));
    } else {
        out.append(makePack(PackKind::Standard, PackSize::Normal, 4));
        out.append(makePack(PackKind::Standard, PackSize::Normal, 4));
    }
}

bool DemoScript::scriptedPackContent(PackKind kind, PackSize size, PackContent &out)
{
    auto fillBase = [&](int options, int choose) {
        out = PackContent{};
        out.kind = kind;
        out.size = size;
        out.optionsToShow = options;
        out.choicesAllowed = choose;
    };

    if (kind == PackKind::Buffoon && size == PackSize::Mega) {
        // 超级小丑包：4 选 1。多彩抽象小丑 + 3 张陪衬。
        // 抽象小丑：每张小丑牌 +3 倍率——6 张满槽时给 +18 倍率。
        fillBase(4, 1);
        out.jokers = {
            JokerType::AbstractJoker,  // 演示重点：多彩
            JokerType::JollyJoker,
            JokerType::Banner,
            JokerType::EvenSteven,
        };
        out.jokerEditions = {
            Edition::Polychrome,       // 抽象小丑挂多彩——包内 hover 和拿走后都带 shader
            Edition::None,
            Edition::None,
            Edition::None,
        };
        return true;
    }
    if (sShopVisit == 1 && kind == PackKind::Standard && size == PackSize::Normal) {
        // 普通游戏卡包：红蜡封普通 K、钢铁蓝蜡封 10、镭射倍率 9。
        fillBase(3, 1);
        auto mk = [](Rank r, Suit s, Enhancement e, Edition ed, Seal sl) {
            CardData c; c.rank = r; c.suit = s; c.enhancement = e; c.edition = ed; c.seal = sl;
            c.assignNewUid();
            return c;
        };
        using R = Rank; using S = Suit; using E = Enhancement; using ED = Edition; using SL = Seal;
        out.standardCards = {
            mk(R::King, S::Spades, E::None,  ED::None,        SL::Red),
            mk(R::Ten,  S::Hearts, E::Steel, ED::None,        SL::Blue),
            mk(R::Nine, S::Clubs,  E::Mult,  ED::Holographic, SL::None),
        };
        return true;
    }
    if (kind == PackKind::Standard && size == PackSize::Mega) {
        // 超级游戏卡包：5 选 2。展示蜡封 + 增强组合，覆盖红/蓝/金/紫四种蜡封 + 幸运/钢铁/万能三种增强。
        fillBase(5, 2);
        auto mk = [](Rank r, Suit s, Enhancement e, Seal sl) {
            CardData c; c.rank = r; c.suit = s; c.enhancement = e; c.seal = sl;
            c.assignNewUid();
            return c;
        };
        using R = Rank; using S = Suit; using E = Enhancement; using SL = Seal;
        out.standardCards = {
            mk(R::King,  S::Spades,   E::Lucky, SL::Red),    // 红蜡封幸运 ♠K
            mk(R::Eight, S::Diamonds, E::Steel, SL::Blue),   // 蓝蜡封钢铁 ♦8
            mk(R::Six,   S::Hearts,   E::None,  SL::None),   // 中间：普通 ♥6
            mk(R::Nine,  S::Clubs,    E::None,  SL::Gold),   // 金蜡封 ♣9
            mk(R::Queen, S::Hearts,   E::Wild,  SL::Purple), // 紫蜡封万能 ♥Q
        };
        return true;
    }
    if (kind == PackKind::Arcana && size == PackSize::Mega) {
        // V1 兼容：其它入口若生成超级塔罗包，仍固定把灵魂放在首位。
        fillBase(5, 2);
        out.consumables = {
            ConsumableType::Spectral_Soul,
            ConsumableType::Tarot_Empress,
            ConsumableType::Tarot_Hierophant,
            ConsumableType::Tarot_Magician,
            ConsumableType::Tarot_Strength,
        };
        return true;
    }
    if (sShopVisit == 1 && kind == PackKind::Arcana) {
        // 第一商店普通塔罗包：灵魂 + 两张不重复的陪衬塔罗，3 选 1。
        fillBase(3, 1);
        out.consumables = {
            ConsumableType::Spectral_Soul,
            ConsumableType::Tarot_Magician,
            ConsumableType::Tarot_Empress,
        };
        return true;
    }
    return false;
}

BossEffect DemoScript::scriptedBoss(int ante)
{
    // 第一 Ante 固定为车轮；演示局的具体背面牌由 GameState::dealCards 确定。
    if (ante == 1) return BossEffect::TheWheel;
    return BossEffect::None;
}
