#include "demoscript.h"
#include "shop.h"
#include <algorithm>

bool DemoScript::sActive       = true;
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
        return { {R::Ten,   S::Hearts}, {R::Three, S::Diamonds},
                 {R::Nine,  S::Hearts}, {R::Eight, S::Hearts},
                 {R::Six,   S::Hearts}, {R::Two,   S::Spades},
                 {R::Queen, S::Spades}, {R::Four,  S::Clubs} };
    case 3:
        // 两张迭代器牌必须匹配增强状态，不能误取牌组里的天然红心 10。
        return { {R::Ace,   S::Hearts},      {R::Ten,   S::Hearts, true},
                 {R::Ten,   S::Hearts, true},{R::Eight, S::Hearts},
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
        // 在 [frontPos, end) 中找目标卡；Boss 的两个红心 10 还必须是迭代器增强。
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
        // 第一商店初始货架：两张课程设计小丑。
        out.append(makeJokerOffer(JokerType::OperatorOverload));
        out.append(makeJokerOffer(JokerType::ClassTemplate));
    } else if (sShopVisit == 1) {
        // 第一次及后续重掷：固定刷出两张课程设计塔罗，各 $3。
        out.append(makeConsumableOffer(OfferKind::Tarot, ConsumableType::Tarot_Iterator));
        out.append(makeConsumableOffer(OfferKind::Tarot, ConsumableType::Tarot_ShallowCopy));
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
    // 脚本可以有意提供超过基础槽位的固定商品（第二商店的木星 + 两张陪衬小丑）。
    // 这里只补不足，不裁掉已经写入的剧本商品。
    while (out.size() < slotCount) {
        ShopOffer extra;
        if (scriptedExtraShopOffer(extra)) out.append(extra);
        else out.append(makeJokerOffer(JokerType::Joker));
    }
}

bool DemoScript::scriptedExtraShopOffer(ShopOffer &out)
{
    // V1 兼容钩子：若外部仍通过 Overstock 扩充第一商店，补一张木星。
    if (sShopVisit != 1) return false;
    out = ShopOffer{};
    out.kind = OfferKind::Planet;
    out.consumable = ConsumableType::Planet_Jupiter;
    out.cost = 3;   // 原版行星价 $3
    return true;
}

JokerType DemoScript::scriptedLegendaryJoker()
{
    // Soul 塔罗在演示模式下固定开出特里布莱（人头牌 ×2 倍率）。
    return JokerType::Triboulet;
}


void DemoScript::scriptedVoucherOffers(QVector<ShopOffer> &out)
{
    // V2 不固定优惠券；第一商店必须通过一次 $5 重掷进入双程设塔罗货架。
    out.clear();
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
        // 超级塔罗包固定包含灵魂；旁边放一个不参与剧本的普通游戏卡包。
        out.append(makePack(PackKind::Arcana,   PackSize::Mega,   6));
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
        // 第一商店超级塔罗包：灵魂置于首位；两张课程设计塔罗改由主货架重掷刷出。
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
    if (kind == PackKind::Arcana) {
        // V1 兼容钩子：若其它演示入口生成普通塔罗包，仍固定提供灵魂并开出特里布莱。
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
    // 第一 Ante 固定为墙壁：只提高目标分数，不干扰固定手牌与扩展牌演示。
    if (ante == 1) return BossEffect::TheWall;
    return BossEffect::None;
}
