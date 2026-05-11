#include "shop.h"
#include <QRandomGenerator>

void Shop::roll() {
    mShopOffers.clear();
    mBoosterOffers.clear();
    for (int i = 0; i < 2; ++i) mShopOffers.append(randomShopOffer());
    for (int i = 0; i < 2; ++i) mBoosterOffers.append(randomBoosterOffer());
}

void Shop::rerollShopOnly() {
    mShopOffers.clear();
    for (int i = 0; i < 2; ++i) mShopOffers.append(randomShopOffer());
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

void Shop::onReroll()         { if (mRerollCost < 10) ++mRerollCost; }
void Shop::resetForNewBlind() { mRerollCost = 5; }

ShopOffer Shop::randomShopOffer() {
    int roll = QRandomGenerator::global()->bounded(100);
    ShopOffer o;
    if (roll < 56) {
        o.kind = OfferKind::Joker;
        o.joker = randomJokerType();
        o.cost = costFor(o.joker);
    } else if (roll < 78) {
        o.kind = OfferKind::Tarot;
        o.consumable = randomTarotType();
        o.cost = 3;
    } else {
        o.kind = OfferKind::Planet;
        o.consumable = randomPlanetType();
        o.cost = 3;
    }
    return o;
}

ShopOffer Shop::randomBoosterOffer() {
    // 按原版权重 normal 4 类:Arcana=10/Celestial=10/Standard=10/Buffoon=6
    int r = QRandomGenerator::global()->bounded(36);
    ShopOffer o;
    o.kind = OfferKind::Pack;
    o.cost = 4;
    if      (r < 10) o.pack = PackKind::Arcana;
    else if (r < 20) o.pack = PackKind::Celestial;
    else if (r < 30) o.pack = PackKind::Standard;
    else             o.pack = PackKind::Buffoon;
    return o;
}

// randomJokerType 和 costFor 保持原样,从旧代码拷过来

JokerType Shop::randomJokerType() {
    constexpr JokerType pool[] = {
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
    int n = int(sizeof(pool) / sizeof(pool[0]));
    return pool[QRandomGenerator::global()->bounded(n)];
}

int Shop::costFor(JokerType t) {
    switch (t) {
    case JokerType::Joker:           return 2;
    case JokerType::JollyJoker:
    case JokerType::SlyJoker:        return 3;
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
    case JokerType::Scholar:         return 4;
    case JokerType::GreedyJoker:
    case JokerType::LustyJoker:
    case JokerType::WrathfulJoker:
    case JokerType::GluttonousJoker:
    case JokerType::HalfJoker:
    case JokerType::Banner:
    case JokerType::MysticSummit:
    case JokerType::RaisedFist:      return 5;
    case JokerType::GoldenJoker:
    case JokerType::Bull:            return 6;
    case JokerType::Bootstraps:      return 7;
    case JokerType::Fibonacci:       return 8;
    }
    return 4;
}
