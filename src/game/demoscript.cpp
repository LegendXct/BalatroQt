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
        // 黑桃同花顺 ♠6 ♠7(玻璃) ♠8 ♠9 ♠10 + 3 张陪衬
        return { {R::Six,   S::Spades},  {R::Seven, S::Spades},
                 {R::Eight, S::Spades},  {R::Nine,  S::Spades},
                 {R::Ten,   S::Spades},  {R::Two,   S::Clubs},
                 {R::Three, S::Hearts},  {R::Four,  S::Hearts} };
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

static ShopOffer makeJokerOffer(JokerType t)
{
    ShopOffer o;
    o.kind = OfferKind::Joker;
    o.joker = t;
    o.cost = jokerBaseCost(t);
    return o;
}

void DemoScript::scriptedShopOffers(QVector<ShopOffer> &out, int slotCount)
{
    out.clear();
    if (sShopVisit == 1 && sShopRerolls == 0) {
        // 第一商店初始：蓝图 + 头脑风暴
        out.append(makeJokerOffer(JokerType::Blueprint));
        out.append(makeJokerOffer(JokerType::Brainstorm));
    } else if (sShopVisit == 1 && sShopRerolls >= 1) {
        // 第一商店重摇后：DNA + 特里布莱
        out.append(makeJokerOffer(JokerType::DNA));
        out.append(makeJokerOffer(JokerType::Triboulet));
    } else if (sShopVisit == 2) {
        // 第二商店：什么都不想卖（用户脚本里"展示设置不买东西"），随便放两张白板
        out.append(makeJokerOffer(JokerType::Joker));
        out.append(makeJokerOffer(JokerType::HalfJoker));
    } else {
        // 第 3 商店之后：路演到不了，给个普通小丑兜底，不再保证脚本
        out.append(makeJokerOffer(JokerType::Joker));
        out.append(makeJokerOffer(JokerType::HalfJoker));
    }
    // 多余槽位填普通小丑
    while (out.size() < slotCount) out.append(makeJokerOffer(JokerType::Joker));
}

void DemoScript::scriptedVoucherOffers(QVector<ShopOffer> &out)
{
    out.clear();
    if (sShopVisit == 1) {
        ShopOffer o;
        o.kind = OfferKind::Voucher;
        o.voucher = VoucherType::Grabber;
        o.cost = voucherData(VoucherType::Grabber).cost;
        out.append(o);
    }
    // 第二商店：不出 voucher（保持简洁）
}

void DemoScript::scriptedBoosterOffers(QVector<ShopOffer> &out)
{
    out.clear();
    auto makePack = [](PackKind k) {
        ShopOffer o;
        o.kind = OfferKind::Pack;
        o.pack = k;
        o.packSize = PackSize::Normal;
        o.packVariant = 0;
        o.cost = 4;   // 原版小包统一 $4
        return o;
    };
    if (sShopVisit == 1) {
        out.append(makePack(PackKind::Celestial));   // 含 Jupiter
        out.append(makePack(PackKind::Arcana));      // 含 Justice
    } else {
        // 第二商店：什么都不放，路演里只展示设置，不用包
        out.append(makePack(PackKind::Standard));
        out.append(makePack(PackKind::Standard));
    }
}

bool DemoScript::scriptedPackContent(PackKind kind, PackSize /*size*/, PackContent &out)
{
    auto fillBase = [&](int options, int choose) {
        out = PackContent{};
        out.kind = kind;
        out.size = PackSize::Normal;
        out.optionsToShow = options;
        out.choicesAllowed = choose;
    };

    if (kind == PackKind::Celestial) {
        fillBase(3, 1);
        // Jupiter（升级同花）放首位；后两位陪衬。
        out.consumables = {
            ConsumableType::Planet_Jupiter,
            ConsumableType::Planet_Mars,
            ConsumableType::Planet_Saturn,
        };
        return true;
    }
    if (kind == PackKind::Arcana) {
        fillBase(3, 1);
        // Justice（玻璃增强）放首位；后两位陪衬。
        out.consumables = {
            ConsumableType::Tarot_Justice,
            ConsumableType::Tarot_Empress,
            ConsumableType::Tarot_Magician,
        };
        return true;
    }
    return false;
}

BossEffect DemoScript::scriptedBoss(int ante)
{
    if (ante == 1) return BossEffect::ThePillar;
    return BossEffect::None;   // 路演到不了，让原 RNG 兜底
}
