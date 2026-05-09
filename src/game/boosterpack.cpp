#include "boosterpack.h"
#include <QRandomGenerator>

QString packDisplayName(PackKind k) {
    switch (k) {
    case PackKind::Standard:  return "标准包";
    case PackKind::Arcana:    return "奥秘包";
    case PackKind::Celestial: return "天体包";
    case PackKind::Buffoon:   return "小丑包";
    }
    return "";
}

static CardData randomStandardCard() {
    auto *rng = QRandomGenerator::global();
    CardData c;
    c.suit = static_cast<Suit>(rng->bounded(4));
    c.rank = static_cast<Rank>(rng->bounded(13) + 2);   // 2..14

    // 30% 加 enhancement
    if (rng->bounded(100) < 30) {
        constexpr Enhancement pool[] = {
            Enhancement::Bonus, Enhancement::Mult, Enhancement::Wild,
            Enhancement::Glass, Enhancement::Steel, Enhancement::Lucky,
            Enhancement::Gold,
        };
        c.enhancement = pool[rng->bounded(int(sizeof(pool)/sizeof(*pool)))];
    }
    // 10% 加 edition
    if (rng->bounded(100) < 10) {
        constexpr Edition pool[] = {
            Edition::Foil, Edition::Holographic, Edition::Polychrome,
        };
        c.edition = pool[rng->bounded(int(sizeof(pool)/sizeof(*pool)))];
    }
    // 5% 加 seal
    if (rng->bounded(100) < 5) {
        constexpr Seal pool[] = {
            Seal::Gold, Seal::Red, Seal::Blue, Seal::Purple,
        };
        c.seal = pool[rng->bounded(int(sizeof(pool)/sizeof(*pool)))];
    }
    return c;
}

static JokerType randomBuffoonJoker() {
    // 跟阶段 3.2 的 Shop::randomJokerType 同一份池子
    constexpr JokerType pool[] = {
        JokerType::Joker,           JokerType::GreedyJoker,
        JokerType::LustyJoker,      JokerType::WrathfulJoker,
        JokerType::GluttonousJoker, JokerType::HalfJoker,
        JokerType::JollyJoker,      JokerType::ZanyJoker,
        JokerType::MadJoker,        JokerType::CrazyJoker,
        JokerType::DrollJoker,      JokerType::GoldenJoker,
        JokerType::ToDoList,        JokerType::SlyJoker,
        JokerType::WilyJoker,       JokerType::CleverJoker,
        JokerType::DeviousJoker,    JokerType::CraftyJoker,
        JokerType::Banner,          JokerType::MysticSummit,
        JokerType::Misprint,        JokerType::RaisedFist,
        JokerType::Fibonacci,       JokerType::EvenSteven,
        JokerType::OddTodd,         JokerType::Scholar,
        JokerType::Bull,            JokerType::Bootstraps,
    };
    int n = int(sizeof(pool)/sizeof(*pool));
    return pool[QRandomGenerator::global()->bounded(n)];
}

PackContent generatePackContent(PackKind k) {
    PackContent pc; pc.kind = k;
    switch (k) {
    case PackKind::Standard:
        for (int i = 0; i < 3; ++i) pc.standardCards.append(randomStandardCard());
        break;
    case PackKind::Arcana:
        for (int i = 0; i < 3; ++i) pc.consumables.append(randomTarotType());
        break;
    case PackKind::Celestial:
        for (int i = 0; i < 3; ++i) pc.consumables.append(randomPlanetType());
        break;
    case PackKind::Buffoon:
        for (int i = 0; i < 2; ++i) pc.jokers.append(randomBuffoonJoker());
        break;
    }
    return pc;
}
