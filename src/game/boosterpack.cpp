#include "boosterpack.h"
#include <QRandomGenerator>
#include <algorithm>

QString packDisplayName(PackKind k) {
    return packDisplayName(k, PackSize::Normal);
}

QString packDisplayName(PackKind k, PackSize s) {
    QString prefix;
    if (s == PackSize::Jumbo) prefix = "超级";
    if (s == PackSize::Mega)  prefix = "巨型";

    switch (k) {
    case PackKind::Standard:  return prefix + "标准包";
    case PackKind::Arcana:    return prefix + "奥秘包";
    case PackKind::Celestial: return prefix + "天体包";
    case PackKind::Buffoon:   return prefix + "小丑包";
    case PackKind::Spectral:  return prefix + "幻灵包";
    }
    return "";
}

QPoint packSpritePos(PackKind k, PackSize s) {
    // 坐标来自原版 booster atlas：x 是同类变体列，y 是行。
    switch (k) {
    case PackKind::Arcana:
        if (s == PackSize::Normal) return {0, 0};
        if (s == PackSize::Jumbo)  return {0, 2};
        return {2, 2};
    case PackKind::Celestial:
        if (s == PackSize::Normal) return {0, 1};
        if (s == PackSize::Jumbo)  return {0, 3};
        return {2, 3};
    case PackKind::Spectral:
        if (s == PackSize::Normal) return {0, 4};
        if (s == PackSize::Jumbo)  return {2, 4};
        return {3, 4};
    case PackKind::Standard:
        if (s == PackSize::Normal) return {0, 6};
        if (s == PackSize::Jumbo)  return {0, 7};
        return {2, 7};
    case PackKind::Buffoon:
        if (s == PackSize::Normal) return {0, 8};
        if (s == PackSize::Jumbo)  return {2, 8};
        return {3, 8};
    }
    return {0, 0};
}

static int optionsFor(PackKind k, PackSize s) {
    if (k == PackKind::Buffoon) {
        if (s == PackSize::Normal) return 2;
        return 4;
    }
    if (k == PackKind::Spectral) {
        if (s == PackSize::Normal) return 2;
        return 4;
    }
    if (s == PackSize::Normal) return 3;
    return 5;
}

static int choicesFor(PackSize s) {
    return (s == PackSize::Mega) ? 2 : 1;
}

static bool sameBaseCard(const CardData &a, const CardData &b)
{
    return a.rank == b.rank && a.suit == b.suit;
}

static CardData randomStandardCard(const QVector<CardData> &existing) {
    auto *rng = QRandomGenerator::global();
    CardData c;
    for (int attempt = 0; attempt < 40; ++attempt) {
        c = CardData{};
        c.suit = static_cast<Suit>(rng->bounded(4));
        c.rank = static_cast<Rank>(rng->bounded(13) + 2);   // 2..14

        // 原版 Standard Pack：大致 40% Enhanced / 60% Base。
        if (rng->bounded(100) < 40) {
            constexpr Enhancement pool[] = {
                Enhancement::Bonus, Enhancement::Mult, Enhancement::Wild,
                Enhancement::Glass, Enhancement::Steel, Enhancement::Lucky,
                Enhancement::Gold,
            };
            c.enhancement = pool[rng->bounded(int(sizeof(pool)/sizeof(*pool)))];
        }

        if (rng->bounded(100) < 10) {
            constexpr Edition pool[] = {
                Edition::Foil, Edition::Holographic, Edition::Polychrome,
            };
            c.edition = pool[rng->bounded(int(sizeof(pool)/sizeof(*pool)))];
        }
        if (rng->bounded(100) < 20) {
            constexpr Seal pool[] = {
                Seal::Gold, Seal::Red, Seal::Blue, Seal::Purple,
            };
            c.seal = pool[rng->bounded(int(sizeof(pool)/sizeof(*pool)))];
        }

        bool dup = false;
        for (const CardData &e : existing) {
            if (sameBaseCard(c, e)) { dup = true; break; }
        }
        if (!dup) return c;
    }
    return c;
}

static QVector<JokerType> allBuffoonJokers() {
    return {
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
}

static JokerType randomBuffoonJoker(const QVector<JokerType> &owned,
                                    const QVector<JokerType> &alreadyRolled,
                                    bool allowDuplicates) {
    QVector<JokerType> pool;
    for (JokerType t : allBuffoonJokers()) {
        if (!allowDuplicates && (owned.contains(t) || alreadyRolled.contains(t))) continue;
        pool.append(t);
    }
    if (pool.isEmpty()) pool = allBuffoonJokers();
    return pool[QRandomGenerator::global()->bounded(pool.size())];
}

template <typename T>
static T randomUniqueFromPool(const QVector<T> &pool, QVector<T> &already)
{
    QVector<T> candidates;
    for (const T &v : pool) {
        if (!already.contains(v)) candidates.append(v);
    }
    if (candidates.isEmpty()) candidates = pool;
    T chosen = candidates[QRandomGenerator::global()->bounded(candidates.size())];
    already.append(chosen);
    return chosen;
}

static ConsumableType randomUniqueTarot(QVector<ConsumableType> &already)
{
    QVector<ConsumableType> pool = {
        ConsumableType::Tarot_Empress, ConsumableType::Tarot_Hierophant,
        ConsumableType::Tarot_Chariot, ConsumableType::Tarot_Lovers,
        ConsumableType::Tarot_Hermit,  ConsumableType::Tarot_Tower,
    };
    return randomUniqueFromPool(pool, already);
}

static ConsumableType randomUniquePlanet(QVector<ConsumableType> &already)
{
    QVector<ConsumableType> pool = {
        ConsumableType::Planet_Pluto,   ConsumableType::Planet_Mercury,
        ConsumableType::Planet_Uranus,  ConsumableType::Planet_Venus,
        ConsumableType::Planet_Saturn,  ConsumableType::Planet_Jupiter,
        ConsumableType::Planet_Earth,   ConsumableType::Planet_Mars,
        ConsumableType::Planet_Neptune, ConsumableType::Planet_PlanetX,
        ConsumableType::Planet_Ceres,   ConsumableType::Planet_Eris,
    };
    return randomUniqueFromPool(pool, already);
}

static ConsumableType randomUniqueSpectral(QVector<ConsumableType> &already)
{
    QVector<ConsumableType> pool = {
        ConsumableType::Spectral_Talisman, ConsumableType::Spectral_Aura,
        ConsumableType::Spectral_Immolate, ConsumableType::Spectral_DejaVu,
        ConsumableType::Spectral_Trance,   ConsumableType::Spectral_Medium,
    };
    return randomUniqueFromPool(pool, already);
}

PackContent generatePackContent(PackKind k, PackSize s, bool omenGlobe,
                                bool telescope, ConsumableType telescopePlanet,
                                const QVector<JokerType> &ownedJokers,
                                bool allowDuplicateJokers) {
    Q_UNUSED(telescope);
    Q_UNUSED(telescopePlanet);

    PackContent pc;
    pc.kind = k;
    pc.size = s;
    pc.optionsToShow = optionsFor(k, s);
    pc.choicesAllowed = choicesFor(s);

    QVector<ConsumableType> usedConsumables;
    QVector<JokerType> usedJokers;

    switch (k) {
    case PackKind::Standard:
        for (int i = 0; i < pc.optionsToShow; ++i)
            pc.standardCards.append(randomStandardCard(pc.standardCards));
        break;
    case PackKind::Arcana:
        for (int i = 0; i < pc.optionsToShow; ++i) {
            if (omenGlobe && QRandomGenerator::global()->bounded(100) < 20)
                pc.consumables.append(randomUniqueSpectral(usedConsumables));
            else
                pc.consumables.append(randomUniqueTarot(usedConsumables));
        }
        break;
    case PackKind::Celestial:
        for (int i = 0; i < pc.optionsToShow; ++i)
            pc.consumables.append(randomUniquePlanet(usedConsumables));
        break;
    case PackKind::Spectral:
        for (int i = 0; i < pc.optionsToShow; ++i)
            pc.consumables.append(randomUniqueSpectral(usedConsumables));
        break;
    case PackKind::Buffoon:
        for (int i = 0; i < pc.optionsToShow; ++i) {
            JokerType j = randomBuffoonJoker(ownedJokers, usedJokers, allowDuplicateJokers);
            pc.jokers.append(j);
            usedJokers.append(j);
        }
        break;
    }
    return pc;
}
