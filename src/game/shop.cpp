#include "shop.h"
#include <QRandomGenerator>

void Shop::roll() {
    mOffers.clear();
    for (int i = 0; i < 2; ++i) {
        ShopOffer o;
        o.type = randomJokerType();
        o.cost = costFor(o.type);
        mOffers.append(o);
    }
}

bool Shop::canBuy(int idx, int gold) const {
    if (idx < 0 || idx >= mOffers.size()) return false;
    const ShopOffer &o = mOffers[idx];
    return !o.sold && gold >= o.cost;
}

ShopOffer Shop::takeOffer(int idx) {
    ShopOffer o = mOffers[idx];
    mOffers[idx].sold = true;
    return o;
}

void Shop::onReroll() {
    if (mRerollCost < 10) ++mRerollCost;
}

void Shop::resetForNewBlind() {
    mRerollCost = 5;
}

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
