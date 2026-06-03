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

// ── 每个盲注的开局 8 张手牌（{rank, suit} 对，顺序无关——只要求出现在前 8 张里）──
//   1 = 小盲, 2 = 大盲, 3 = Boss(支柱)
static QVector<QPair<Rank, Suit>> scriptedHandForBlind(int blindNo)
{
    using R = Rank; using S = Suit;
    switch (blindNo) {
    case 1:
        // 一对 ♠5 ♦5 + 顺子 ♥6 ♣7 ♦8 ♦9 ♣10 + 弃牌靶子 ♦2
        // 注意：顺子里 8 用 ♦8 而非 ♠8——避免和 Boss 同花顺的 ♠8 是同一张卡，否则
        //   Pillar(mCardsPlayedThisAnte) 会把 Boss 手牌里的 ♠8 标禁用。
        return { {R::Five,  S::Spades},  {R::Five,  S::Diamonds},
                 {R::Six,   S::Hearts},  {R::Seven, S::Clubs},
                 {R::Eight, S::Diamonds},{R::Nine,  S::Diamonds},
                 {R::Ten,   S::Clubs},   {R::Two,   S::Diamonds} };
    case 2:
        // 红心同花 ♥9 ♥J ♥Q ♥K ♥A + 对子 ♣4 ♦4 + 留给 Boss 的 ♠7（Justice→玻璃）
        return { {R::Nine,  S::Hearts},  {R::Jack,  S::Hearts},
                 {R::Queen, S::Hearts},  {R::King,  S::Hearts},
                 {R::Ace,   S::Hearts},  {R::Four,  S::Clubs},
                 {R::Four,  S::Diamonds},{R::Seven, S::Spades} };
    case 3:
        // 黑桃同花顺 ♠6 ♠7 ♠8 ♠9 ♠10 + 3 张人头牌（被 The Mark 翻成背面，纯演示用，不打出）
        return { {R::Six,   S::Spades},  {R::Seven, S::Spades},
                 {R::Eight, S::Spades},  {R::Nine,  S::Spades},
                 {R::Ten,   S::Spades},  {R::Jack,  S::Spades},
                 {R::Queen, S::Diamonds},{R::King,  S::Clubs} };
    default: return {};
    }
}

void DemoScript::reorderDeckForNextBlind(QVector<CardData> &pile)
{
    // sBlindEntered 表示"已经进过几个盲注"，下一个盲注是 sBlindEntered + 1。
    // 但 onEnterBlind() 是在 startBlind 里 reset() 之前调用还是之后，依赖于 hook 时机。
    // 我们在 reset() 里看到的 sBlindEntered 应该已经被本盲注的 onEnterBlind() 累加过——
    // 所以当前要演的盲注就是 sBlindEntered（不再 +1）。
    const auto want = scriptedHandForBlind(sBlindEntered);
    if (want.isEmpty()) return;

    int frontPos = 0;
    for (const auto &w : want) {
        // 在 [frontPos, end) 范围内找匹配点数+花色的卡——已交换到前面的不再动。
        auto it = std::find_if(pile.begin() + frontPos, pile.end(),
                               [&w](const CardData &c) {
                                   return c.rank == w.first && c.suit == w.second;
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

void DemoScript::scriptedShopOffers(QVector<ShopOffer> &out, int slotCount)
{
    out.clear();
    if (sShopVisit == 1 && sShopRerolls == 0) {
        // 第一商店初始：蓝图(闪箔) + 头脑风暴(镭射)
        out.append(makeJokerOffer(JokerType::Blueprint,  Edition::Foil));
        out.append(makeJokerOffer(JokerType::Brainstorm, Edition::Holographic));
    } else if (sShopVisit == 1 && sShopRerolls >= 1) {
        // 第一商店重摇后：负片的 悬挂乍得(未断选票) + 普通的 纸牌占卜师(塔罗术士)
        // 负片 = +1 小丑槽位，让最终 6 张小丑（蓝图/头脑风暴/乍得/占卜师/公牛/特里布莱）能塞下默认 5 槽。
        out.append(makeJokerOffer(JokerType::HangingChad, Edition::Negative));
        out.append(makeJokerOffer(JokerType::Cartomancer, Edition::None));
    } else if (sShopVisit == 2) {
        // 第二商店（大盲后）：脚本只展示设置不买，3 槽用 3 张不同小丑避免视觉重复。
        out.append(makeJokerOffer(JokerType::HalfJoker));
        out.append(makeJokerOffer(JokerType::JollyJoker));
        out.append(makeJokerOffer(JokerType::Misprint));
    } else if (sShopVisit == 3) {
        // 第三商店（Boss 后，Ante 2 小盲前）：和第二商店换一组小丑，否则两次商店看着假。
        out.append(makeJokerOffer(JokerType::Banner));
        out.append(makeJokerOffer(JokerType::AbstractJoker));
        out.append(makeJokerOffer(JokerType::EvenSteven));
    } else {
        out.append(makeJokerOffer(JokerType::GreedyJoker));
        out.append(makeJokerOffer(JokerType::LustyJoker));
        out.append(makeJokerOffer(JokerType::WrathfulJoker));
    }
    // Overstock 买完会扩槽到 3，第一商店的新槽用 scriptedExtraShopOffer 填（木星）；
    // 第二商店及之后已经按 3 槽给齐了，这里只是兜底。
    while (out.size() < slotCount) {
        ShopOffer extra;
        if (scriptedExtraShopOffer(extra)) out.append(extra);
        else out.append(makeJokerOffer(JokerType::Joker));
    }
}

bool DemoScript::scriptedExtraShopOffer(ShopOffer &out)
{
    // 第一商店买 Overstock 之后扩槽：补一张木星行星牌。其它访问/重摇都返回 false。
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
        // 超级小丑包($6) + 普通塔罗包($4)
        out.append(makePack(PackKind::Buffoon, PackSize::Mega,   6));
        out.append(makePack(PackKind::Arcana,  PackSize::Normal, 4));
    } else if (sShopVisit == 2) {
        // 大盲后第二商店：超级塔罗包($6) 给 Boss 段准备道具（皇后/正义 5 选 2）；
        // 另一个槽位放标准包做陪衬。
        out.append(makePack(PackKind::Arcana,   PackSize::Mega,   6));
        out.append(makePack(PackKind::Standard, PackSize::Normal, 4));
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
        // 超级小丑包：4 选 1。多彩侠盗 + 3 张陪衬小丑。
        // 侠盗 = 其他所有小丑出售价值合计加入倍率——和现有 5 张小丑配合在 Boss 一击爆分。
        fillBase(4, 1);
        out.jokers = {
            JokerType::Swashbuckler,   // 演示重点：多彩
            JokerType::JollyJoker,
            JokerType::Banner,
            JokerType::EvenSteven,
        };
        out.jokerEditions = {
            Edition::Polychrome,       // 侠盗挂多彩——包内 hover 和拿走后都带 shader
            Edition::None,
            Edition::None,
            Edition::None,
        };
        return true;
    }
    if (kind == PackKind::Arcana && size == PackSize::Mega) {
        // 超级塔罗包：5 选 2。必含皇后(+Mult 增强) 和正义(+Glass 增强)，其余 3 张陪衬。
        // 玩家在大盲注后的第二商店看到，给 Boss 段的"打玻璃 + 打倍率"准备道具。
        fillBase(5, 2);
        out.consumables = {
            ConsumableType::Tarot_Empress,    // 选 ≤2 张：Mult 增强
            ConsumableType::Tarot_Justice,    // 选 1 张：玻璃增强
            ConsumableType::Tarot_Hierophant, // 陪衬：Bonus 增强
            ConsumableType::Tarot_Magician,   // 陪衬：Lucky 增强
            ConsumableType::Tarot_Strength,   // 陪衬：点数 +1
        };
        return true;
    }
    if (kind == PackKind::Arcana) {
        // 普通塔罗包（第一商店用）：3 选 1。第一张固定灵魂(Spectral_Soul)——使用后强制开特里布莱。
        // 注意：原版灵魂是从塔罗包里以 5% 概率混入，我们这里直接塞首位。
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
    // 第一 Ante 固定为符号(TheMark)：所有人头牌背面朝下发出——视觉冲击 + 可讲解。
    if (ante == 1) return BossEffect::TheMark;
    return BossEffect::None;
}
