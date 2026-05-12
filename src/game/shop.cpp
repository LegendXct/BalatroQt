#include "shop.h"
#include <QRandomGenerator>
#include <QtGlobal>
#include <cmath>
#include <algorithm>

static int rawJokerCostForType(JokerType t)
{
    int raw = 4;
    switch (t) {
    case JokerType::Joker:           raw = 2; break;
    case JokerType::JollyJoker:
    case JokerType::SlyJoker:        raw = 3; break;
    case JokerType::ZanyJoker:
    case JokerType::MadJoker:
    case JokerType::CrazyJoker:
    case JokerType::DrollJoker:
    case JokerType::WilyJoker:
    case JokerType::CleverJoker:
    case JokerType::DeviousJoker:
    case JokerType::CraftyJoker:
    case JokerType::Misprint:
    case JokerType::ToDoList:
    case JokerType::EvenSteven:
    case JokerType::OddTodd:
    case JokerType::Scholar:         raw = 4; break;
    case JokerType::GreedyJoker:
    case JokerType::LustyJoker:
    case JokerType::WrathfulJoker:
    case JokerType::GluttonousJoker:
    case JokerType::HalfJoker:
    case JokerType::Banner:
    case JokerType::MysticSummit:
    case JokerType::RaisedFist:      raw = 5; break;
    case JokerType::GoldenJoker:
    case JokerType::Bull:            raw = 6; break;
    case JokerType::Bootstraps:      raw = 7; break;
    case JokerType::Fibonacci:       raw = 8; break;
    }
    return raw;
}


VoucherData voucherData(VoucherType t) {
    VoucherData v;
    v.type = t;
    v.cost = 10;
    switch (t) {
    case VoucherType::Overstock:
        v.name = "库存过剩"; v.description = "商店上半区永久多 1 个商品槽"; v.spritePos = {0, 0}; break;
    case VoucherType::ClearanceSale:
        v.name = "清仓特卖"; v.description = "商店商品永久 25% 折扣"; v.spritePos = {3, 0}; break;
    case VoucherType::Hone:
        v.name = "磨练"; v.description = "提高版本牌出现概率（后续接入）"; v.spritePos = {4, 0}; break;
    case VoucherType::RerollSurplus:
        v.name = "重抽盈余"; v.description = "每次商店重抽便宜 $2"; v.spritePos = {0, 2}; break;
    case VoucherType::CrystalBall:
        v.name = "水晶球"; v.description = "消耗牌槽位 +1"; v.spritePos = {2, 2}; break;
    case VoucherType::Telescope:
        v.name = "望远镜"; v.description = "天体包第一张更偏向常用牌型（后续接入）"; v.spritePos = {3, 2}; break;
    case VoucherType::Grabber:
        v.name = "抓钩"; v.description = "每回合出牌次数 +1"; v.spritePos = {5, 0}; break;
    case VoucherType::Wasteful:
        v.name = "浪费"; v.description = "每回合弃牌次数 +1"; v.spritePos = {6, 0}; break;
    case VoucherType::TarotMerchant:
        v.name = "塔罗商人"; v.description = "商店塔罗权重从 4 提高到 9.6"; v.spritePos = {1, 0}; break;
    case VoucherType::PlanetMerchant:
        v.name = "星球商人"; v.description = "商店星球权重从 4 提高到 9.6"; v.spritePos = {2, 0}; break;
    case VoucherType::SeedMoney:
        v.name = "种子基金"; v.description = "利息上限提高到 $50"; v.spritePos = {1, 2}; break;
    case VoucherType::Blank:
        v.name = "空白"; v.description = "什么都不做，但可以为后续升级券铺路"; v.spritePos = {7, 0}; break;
    case VoucherType::MagicTrick:
        v.name = "魔术把戏"; v.description = "商店上半区可以刷出普通游戏牌，权重为 4"; v.spritePos = {4, 2}; break;
    case VoucherType::Hieroglyph:
        v.name = "象形文字"; v.description = "底注 -1，每回合出牌次数 -1"; v.spritePos = {5, 2}; break;
    case VoucherType::DirectorsCut:
        v.name = "导演剪辑版"; v.description = "允许花钱重掷 Boss 盲注（后续接入）"; v.spritePos = {6, 2}; break;
    case VoucherType::PaintBrush:
        v.name = "画笔"; v.description = "手牌上限 +1"; v.spritePos = {7, 2}; break;
    }
    return v;
}

QVector<VoucherType> baseVoucherPool() {
    return {
        VoucherType::Overstock,
        VoucherType::ClearanceSale,
        VoucherType::Hone,
        VoucherType::RerollSurplus,
        VoucherType::CrystalBall,
        VoucherType::Telescope,
        VoucherType::Grabber,
        VoucherType::Wasteful,
        VoucherType::TarotMerchant,
        VoucherType::PlanetMerchant,
        VoucherType::SeedMoney,
        VoucherType::Blank,
        VoucherType::MagicTrick,
        VoucherType::Hieroglyph,
        VoucherType::DirectorsCut,
        VoucherType::PaintBrush,
    };
}

void Shop::roll() {
    rerollShopOnly();

    mVoucherOffers.clear();
    mVoucherOffers.append(randomVoucherOffer());

    mBoosterOffers.clear();
    for (int i = 0; i < 2; ++i) mBoosterOffers.append(randomBoosterOffer(mBoosterOffers));
}

void Shop::rerollShopOnly() {
    mShopOffers.clear();
    for (int i = 0; i < mShopSlots; ++i) mShopOffers.append(randomShopOffer(mShopOffers));
}

void Shop::ensureShopOfferCount()
{
    while (mShopOffers.size() < mShopSlots)
        mShopOffers.append(randomShopOffer(mShopOffers));
    while (mShopOffers.size() > mShopSlots)
        mShopOffers.removeLast();
}

void Shop::refreshCurrentOfferCosts()
{
    auto refreshOne = [this](ShopOffer &o) {
        if (o.sold) return;
        if (o.kind == OfferKind::Voucher) {
            o.cost = voucherData(o.voucher).cost; // 原版券固定 $10，不吃清仓折扣
        } else {
            o.cost = applyDiscount(rawCostFor(o));
        }
    };
    for (ShopOffer &o : mShopOffers) refreshOne(o);
    for (ShopOffer &o : mBoosterOffers) refreshOne(o);
    for (ShopOffer &o : mVoucherOffers) refreshOne(o);
}

bool Shop::canBuyShop(int idx, int gold) const {
    if (idx < 0 || idx >= mShopOffers.size()) return false;
    const ShopOffer &o = mShopOffers[idx];
    return !o.sold && gold >= o.cost;
}

ShopOffer Shop::takeShopOffer(int idx) {
    ShopOffer o = mShopOffers[idx];
    mShopOffers[idx].sold = true;
    return o;
}

bool Shop::canBuyVoucher(int idx, int gold) const {
    if (idx < 0 || idx >= mVoucherOffers.size()) return false;
    const ShopOffer &o = mVoucherOffers[idx];
    return !o.sold && gold >= o.cost;
}

ShopOffer Shop::takeVoucherOffer(int idx) {
    ShopOffer o = mVoucherOffers[idx];
    mVoucherOffers[idx].sold = true;
    return o;
}

bool Shop::canBuyBooster(int idx, int gold) const {
    if (idx < 0 || idx >= mBoosterOffers.size()) return false;
    const ShopOffer &o = mBoosterOffers[idx];
    return !o.sold && gold >= o.cost;
}

ShopOffer Shop::takeBoosterOffer(int idx) {
    ShopOffer o = mBoosterOffers[idx];
    mBoosterOffers[idx].sold = true;
    return o;
}

void Shop::onReroll() {
    if (mRerollCost < 10) ++mRerollCost;
}

void Shop::resetForNewBlind() {
    mRerollCost = qMax(0, 5 - mRerollDiscount);
}

void Shop::changeShopSlots(int delta) {
    mShopSlots = qMax(1, mShopSlots + delta);
}

void Shop::setOwnedJokers(const QVector<JokerType> &owned, bool allowDuplicates)
{
    mOwnedJokers = owned;
    mAllowJokerDuplicates = allowDuplicates;
}

int Shop::applyDiscount(int rawCost) const {
    if (mDiscountPercent <= 0) return rawCost;
    return qMax(0, int(std::floor(rawCost * (100 - mDiscountPercent) / 100.0)));
}

int Shop::rawCostFor(const ShopOffer &o) const
{
    switch (o.kind) {
    case OfferKind::Joker:       return rawJokerCostForType(o.joker);
    case OfferKind::Tarot:       return 3;
    case OfferKind::Planet:      return 3;
    case OfferKind::Spectral:    return 4;
    case OfferKind::PlayingCard: return 3;
    case OfferKind::Voucher:     return voucherData(o.voucher).cost;
    case OfferKind::Pack:
        if (o.packSize == PackSize::Normal) return 4;
        if (o.packSize == PackSize::Jumbo)  return 6;
        return 8;
    }
    return o.cost;
}

bool Shop::duplicatesOffer(const ShopOffer &candidate, const QVector<ShopOffer> &existing) const
{
    for (const ShopOffer &o : existing) {
        if (o.sold) continue;
        if (candidate.kind != o.kind) continue;
        switch (candidate.kind) {
        case OfferKind::Joker:
            if (candidate.joker == o.joker) return true;
            break;
        case OfferKind::Tarot:
        case OfferKind::Planet:
        case OfferKind::Spectral:
            if (candidate.consumable == o.consumable) return true;
            break;
        case OfferKind::PlayingCard:
            if (candidate.playingCard.rank == o.playingCard.rank &&
                candidate.playingCard.suit == o.playingCard.suit)
                return true;
            break;
        case OfferKind::Pack:
            if (candidate.pack == o.pack && candidate.packSize == o.packSize) return true;
            break;
        case OfferKind::Voucher:
            if (candidate.voucher == o.voucher) return true;
            break;
        }
    }
    return false;
}

ShopOffer Shop::randomShopOffer(const QVector<ShopOffer> &alreadyRolled) const {
    ShopOffer o;
    double total = mRates.joker + mRates.tarot + mRates.planet + mRates.playingCard + mRates.spectral;
    if (total <= 0.0) total = 1.0;

    // 原版如果没有 Showman/马戏团长，中心不会重复。这里最多尝试 40 次，避免极端池子过小卡住。
    for (int attempt = 0; attempt < 40; ++attempt) {
        double roll = QRandomGenerator::global()->generateDouble() * total;
        double acc = 0.0;

        acc += mRates.joker;
        if (roll <= acc) {
            o.kind = OfferKind::Joker;
            QVector<JokerType> rolled;
            for (const ShopOffer &e : alreadyRolled)
                if (e.kind == OfferKind::Joker) rolled.append(e.joker);
            o.joker = randomJokerType(rolled);
            o.cost = costFor(o.joker);
            if (!duplicatesOffer(o, alreadyRolled)) return o;
            continue;
        }

        acc += mRates.tarot;
        if (roll <= acc) {
            o.kind = OfferKind::Tarot;
            o.consumable = randomTarotType();
            o.cost = applyDiscount(3);
            if (!duplicatesOffer(o, alreadyRolled)) return o;
            continue;
        }

        acc += mRates.planet;
        if (roll <= acc) {
            o.kind = OfferKind::Planet;
            o.consumable = randomPlanetType();
            o.cost = applyDiscount(3);
            if (!duplicatesOffer(o, alreadyRolled)) return o;
            continue;
        }

        acc += mRates.playingCard;
        if (roll <= acc) {
            o.kind = OfferKind::PlayingCard;
            o.playingCard = randomPlayingCard(false, alreadyRolled);
            o.cost = applyDiscount(3);
            if (!duplicatesOffer(o, alreadyRolled)) return o;
            continue;
        }

        o.kind = OfferKind::Spectral;
        o.consumable = randomSpectralType();
        o.cost = applyDiscount(4);
        if (!duplicatesOffer(o, alreadyRolled)) return o;
    }

    // 兜底：如果某一类池子被抽空，就允许最后一次结果返回，保证商店不会崩。
    if (o.cost <= 0) {
        o.kind = OfferKind::Tarot;
        o.consumable = randomTarotType();
        o.cost = applyDiscount(3);
    }
    return o;
}

ShopOffer Shop::randomVoucherOffer() const {
    QVector<VoucherType> pool;
    for (VoucherType v : baseVoucherPool()) {
        if (!mRedeemedVouchers.contains(v)) pool.append(v);
    }
    if (pool.isEmpty()) pool.append(VoucherType::Blank);

    ShopOffer o;
    o.kind = OfferKind::Voucher;
    o.voucher = pool[QRandomGenerator::global()->bounded(pool.size())];
    o.cost = voucherData(o.voucher).cost;
    return o;
}

ShopOffer Shop::randomBoosterOffer(const QVector<ShopOffer> &alreadyRolled) const {
    struct Candidate {
        PackKind kind;
        PackSize size;
        int weight;   // 原版权重 ×100
        int cost;
    };

    const QVector<Candidate> pool = {
        {PackKind::Arcana, PackSize::Normal, 100, 4}, {PackKind::Arcana, PackSize::Normal, 100, 4},
        {PackKind::Arcana, PackSize::Normal, 100, 4}, {PackKind::Arcana, PackSize::Normal, 100, 4},
        {PackKind::Arcana, PackSize::Jumbo, 100, 6},  {PackKind::Arcana, PackSize::Jumbo, 100, 6},
        {PackKind::Arcana, PackSize::Mega, 25, 8},    {PackKind::Arcana, PackSize::Mega, 25, 8},

        {PackKind::Celestial, PackSize::Normal, 100, 4}, {PackKind::Celestial, PackSize::Normal, 100, 4},
        {PackKind::Celestial, PackSize::Normal, 100, 4}, {PackKind::Celestial, PackSize::Normal, 100, 4},
        {PackKind::Celestial, PackSize::Jumbo, 100, 6},  {PackKind::Celestial, PackSize::Jumbo, 100, 6},
        {PackKind::Celestial, PackSize::Mega, 25, 8},    {PackKind::Celestial, PackSize::Mega, 25, 8},

        {PackKind::Spectral, PackSize::Normal, 30, 4}, {PackKind::Spectral, PackSize::Normal, 30, 4},
        {PackKind::Spectral, PackSize::Jumbo, 30, 6},
        {PackKind::Spectral, PackSize::Mega, 7, 8},

        {PackKind::Standard, PackSize::Normal, 100, 4}, {PackKind::Standard, PackSize::Normal, 100, 4},
        {PackKind::Standard, PackSize::Normal, 100, 4}, {PackKind::Standard, PackSize::Normal, 100, 4},
        {PackKind::Standard, PackSize::Jumbo, 100, 6},  {PackKind::Standard, PackSize::Jumbo, 100, 6},
        {PackKind::Standard, PackSize::Mega, 25, 8},    {PackKind::Standard, PackSize::Mega, 25, 8},

        {PackKind::Buffoon, PackSize::Normal, 60, 4}, {PackKind::Buffoon, PackSize::Normal, 60, 4},
        {PackKind::Buffoon, PackSize::Jumbo, 60, 6},
        {PackKind::Buffoon, PackSize::Mega, 15, 8},
    };

    for (int attempt = 0; attempt < 40; ++attempt) {
        int total = 0;
        for (const auto &c : pool) total += c.weight;
        int r = QRandomGenerator::global()->bounded(total);
        int acc = 0;
        Candidate chosen = pool.front();
        for (const auto &c : pool) {
            acc += c.weight;
            if (r < acc) { chosen = c; break; }
        }

        ShopOffer o;
        o.kind = OfferKind::Pack;
        o.pack = chosen.kind;
        o.packSize = chosen.size;
        o.cost = applyDiscount(chosen.cost);
        if (!duplicatesOffer(o, alreadyRolled)) return o;
    }

    ShopOffer o;
    o.kind = OfferKind::Pack;
    o.pack = PackKind::Arcana;
    o.packSize = PackSize::Normal;
    o.cost = applyDiscount(4);
    return o;
}

CardData Shop::randomPlayingCard(bool enhancedPossible, const QVector<ShopOffer> &alreadyRolled) const {
    auto *rng = QRandomGenerator::global();
    CardData c;
    for (int attempt = 0; attempt < 30; ++attempt) {
        c = CardData{};
        c.suit = static_cast<Suit>(rng->bounded(4));
        c.rank = static_cast<Rank>(rng->bounded(13) + 2);

        if (enhancedPossible) {
            constexpr Enhancement pool[] = {
                Enhancement::Bonus, Enhancement::Mult, Enhancement::Wild,
                Enhancement::Glass, Enhancement::Steel, Enhancement::Lucky,
                Enhancement::Gold,
            };
            c.enhancement = pool[rng->bounded(int(sizeof(pool)/sizeof(*pool)))];
        }
        ShopOffer tmp; tmp.kind = OfferKind::PlayingCard; tmp.playingCard = c;
        if (!duplicatesOffer(tmp, alreadyRolled)) return c;
    }
    return c;
}

QVector<JokerType> Shop::jokerPool() {
    return {
        JokerType::Joker,
        JokerType::GreedyJoker,    JokerType::LustyJoker,
        JokerType::WrathfulJoker,  JokerType::GluttonousJoker,
        JokerType::HalfJoker,      JokerType::JollyJoker,
        JokerType::ZanyJoker,      JokerType::MadJoker,
        JokerType::CrazyJoker,     JokerType::DrollJoker,
        JokerType::GoldenJoker,    JokerType::ToDoList,
        JokerType::SlyJoker,       JokerType::WilyJoker,
        JokerType::CleverJoker,    JokerType::DeviousJoker,
        JokerType::CraftyJoker,    JokerType::Banner,
        JokerType::MysticSummit,   JokerType::Misprint,
        JokerType::RaisedFist,     JokerType::Fibonacci,
        JokerType::EvenSteven,     JokerType::OddTodd,
        JokerType::Scholar,        JokerType::Bull,
        JokerType::Bootstraps,
    };
}

JokerType Shop::randomJokerType(const QVector<JokerType> &alreadyRolled) const {
    QVector<JokerType> pool;
    for (JokerType t : jokerPool()) {
        if (!mAllowJokerDuplicates && (mOwnedJokers.contains(t) || alreadyRolled.contains(t)))
            continue;
        pool.append(t);
    }
    if (pool.isEmpty()) pool = jokerPool();
    return pool[QRandomGenerator::global()->bounded(pool.size())];
}

int Shop::costFor(JokerType t) const {
    return applyDiscount(rawJokerCostForType(t));
}
