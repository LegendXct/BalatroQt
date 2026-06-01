#include "boosterpack.h"
#include <QRandomGenerator>
#include <QtGlobal>
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
    case PackKind::Arcana:    return prefix + "塔罗包";
    case PackKind::Celestial: return prefix + "天体包";
    case PackKind::Buffoon:   return prefix + "小丑包";
    case PackKind::Spectral:  return prefix + "幻灵包";
    }
    return "";
}

int packSpriteVariantCount(PackKind k, PackSize s)
{
    switch (k) {
    case PackKind::Arcana:
    case PackKind::Celestial:
    case PackKind::Standard:
        return (s == PackSize::Normal) ? 4 : 2;
    case PackKind::Spectral:
        return (s == PackSize::Normal) ? 2 : 1;
    case PackKind::Buffoon:
        return (s == PackSize::Normal) ? 2 : 1;
    }
    return 1;
}

QPoint packSpritePos(PackKind k, PackSize s, int variant) {
    const int count = qMax(1, packSpriteVariantCount(k, s));
    const int v = ((variant % count) + count) % count;
    // 坐标来自原版 booster atlas：x 是同类变体列，y 是行。
    switch (k) {
    case PackKind::Arcana:
        if (s == PackSize::Normal) return {v, 0};
        if (s == PackSize::Jumbo)  return {v, 2};
        return {2 + v, 2};
    case PackKind::Celestial:
        if (s == PackSize::Normal) return {v, 1};
        if (s == PackSize::Jumbo)  return {v, 3};
        return {2 + v, 3};
    case PackKind::Spectral:
        if (s == PackSize::Normal) return {v, 4};
        if (s == PackSize::Jumbo)  return {2, 4};
        return {3, 4};
    case PackKind::Standard:
        if (s == PackSize::Normal) return {v, 6};
        if (s == PackSize::Jumbo)  return {v, 7};
        return {2 + v, 7};
    case PackKind::Buffoon:
        if (s == PackSize::Normal) return {v, 8};
        if (s == PackSize::Jumbo)  return {2, 8};
        return {3, 8};
    }
    return {0, 0};
}

QPoint packSpritePos(PackKind k, PackSize s) {
    return packSpritePos(k, s, 0);
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
        JokerType::AbstractJoker,   JokerType::Supernova,
        JokerType::GrosMichel,      JokerType::Cavendish,
        JokerType::IceCream,        JokerType::Stuntman,
        JokerType::TheDuo,          JokerType::TheTrio,
        JokerType::TheFamily,       JokerType::TheOrder,
        JokerType::TheTribe,        JokerType::Blackboard,
        JokerType::ScaryFace,       JokerType::SmileyFace,
        JokerType::WalkieTalkie,    JokerType::Arrowhead,
        JokerType::OnyxAgate,       JokerType::RoughGem,
        JokerType::Bloodstone,      JokerType::ShootTheMoon,
        JokerType::Baron,           JokerType::FlowerPot,
        JokerType::Acrobat,         JokerType::Swashbuckler,
        JokerType::Ramen,           JokerType::DriversLicense,
        JokerType::Hiker,           JokerType::CardSharp,
        JokerType::Hologram,        JokerType::MidasMask,
        JokerType::Vampire,         JokerType::Constellation,
        JokerType::Photograph,      JokerType::HangingChad,
        JokerType::SockAndBuskin,
        JokerType::Blueprint,       JokerType::Brainstorm,
        JokerType::DNA,             JokerType::Mime,
        // Batch 1
        JokerType::JokerStencil,    JokerType::SteelJoker,
        JokerType::StoneJoker,      JokerType::BlueJoker,
        JokerType::Erosion,         JokerType::BusinessCard,
        JokerType::FacelessJoker,   JokerType::Cloud9,
        JokerType::GoldenTicket,    JokerType::SeeingDouble,
        // Batch 2
        JokerType::SquareJoker,     JokerType::Runner,
        JokerType::Castle,          JokerType::GreenJoker,
        JokerType::Obelisk,         JokerType::RideTheBus,
        JokerType::SpareTrousers,   JokerType::WeeJoker,
        JokerType::HitTheRoad,      JokerType::GlassJoker,
        JokerType::LuckyCat,        JokerType::Popcorn,
        // Batch 3
        JokerType::Juggler,         JokerType::Drunkard,
        JokerType::MerryAndy,       JokerType::Troubadour,
        JokerType::DelayedGratification, JokerType::ToTheMoon,
        JokerType::ReservedParking, JokerType::MailInRebate,
        JokerType::AncientJoker,    JokerType::TheIdol,
        JokerType::SpaceJoker,      JokerType::Hack,
        // Batch 4
        JokerType::RiffRaff,        JokerType::MarbleJoker,
        JokerType::Burglar,         JokerType::Cartomancer,
        JokerType::Certificate,     JokerType::Madness,
        JokerType::EightBall,       JokerType::Seance,
        JokerType::Vagabond,        JokerType::Superposition,
        // Batch 5
        JokerType::FlashCard,       JokerType::Throwback,
        JokerType::Campfire,        JokerType::FortuneTeller,
        JokerType::LoyaltyCard,     JokerType::Egg,
        JokerType::Rocket,          JokerType::Satellite,
        JokerType::GiftCard,
        // Batch 6
        JokerType::Shortcut,        JokerType::SmearedJoker,
        JokerType::Splash,          JokerType::Showman,
        // Batch 7
        JokerType::Dusk,            JokerType::CeremonialDagger,
        JokerType::TurtleBean,      JokerType::Seltzer,
        JokerType::BurntJoker,      JokerType::ChaosTheClown,
        // Batch 8
        JokerType::Pareidolia,      JokerType::Hallucination,
        JokerType::Luchador,        JokerType::InvisibleJoker,
        // Batch 9
        JokerType::CreditCard,      JokerType::MrBones,
        JokerType::DietCola,        JokerType::FourFingers,
        JokerType::OopsAllSixes,
    };
}

static JokerType randomBuffoonJoker(const QVector<JokerType> &owned,
                                    const QVector<JokerType> &alreadyRolled,
                                    bool allowDuplicates,
                                    bool grosMichelExtinct) {
    QVector<JokerType> pool;
    for (JokerType t : allBuffoonJokers()) {
        // 原版 no_pool_flag/yes_pool_flag：
        //   - 大麦克：灭绝后不再出现
        //   - 卡文迪什：仅在大麦克灭绝后出现
        if (t == JokerType::GrosMichel && grosMichelExtinct) continue;
        if (t == JokerType::Cavendish && !grosMichelExtinct) continue;
        if (!allowDuplicates && (owned.contains(t) || alreadyRolled.contains(t))) continue;
        pool.append(t);
    }
    // 退化时仅放开“重复”过滤，仍保留 gros_michel_extinct 规则，防止把卡文迪什/大麦克同框刷出。
    if (pool.isEmpty()) {
        for (JokerType t : allBuffoonJokers()) {
            if (t == JokerType::GrosMichel && grosMichelExtinct) continue;
            if (t == JokerType::Cavendish && !grosMichelExtinct) continue;
            pool.append(t);
        }
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
        ConsumableType::Tarot_Hermit,  ConsumableType::Tarot_HangedMan,
        ConsumableType::Tarot_Tower,
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
        ConsumableType::Spectral_Familiar, ConsumableType::Spectral_Grim,
        ConsumableType::Spectral_Incantation, ConsumableType::Spectral_Talisman,
        ConsumableType::Spectral_Aura, ConsumableType::Spectral_Wraith,
        ConsumableType::Spectral_Sigil, ConsumableType::Spectral_Ouija,
        ConsumableType::Spectral_Ectoplasm, ConsumableType::Spectral_Immolate,
        ConsumableType::Spectral_Ankh, ConsumableType::Spectral_DejaVu,
        ConsumableType::Spectral_Hex, ConsumableType::Spectral_Trance,
        ConsumableType::Spectral_Medium, ConsumableType::Spectral_Cryptid,
    };
    return randomUniqueFromPool(pool, already);
}

static ConsumableType randomPackConsumableWithSpecials(PackKind kind, QVector<ConsumableType> &already, bool omenGlobe)
{
    auto *rng = QRandomGenerator::global();
    // Project rule: The Soul / Black Hole each roll at 5% in eligible packs.
    if ((kind == PackKind::Arcana || kind == PackKind::Spectral) && !already.contains(ConsumableType::Spectral_Soul)) {
        if (rng->generateDouble() < 0.05) { already.append(ConsumableType::Spectral_Soul); return ConsumableType::Spectral_Soul; }
    }
    if ((kind == PackKind::Celestial || kind == PackKind::Spectral) && !already.contains(ConsumableType::Spectral_BlackHole)) {
        if (rng->generateDouble() < 0.05) { already.append(ConsumableType::Spectral_BlackHole); return ConsumableType::Spectral_BlackHole; }
    }

    if (kind == PackKind::Arcana) {
        if (omenGlobe && rng->bounded(100) < 20) return randomUniqueSpectral(already);
        return randomUniqueTarot(already);
    }
    if (kind == PackKind::Celestial) return randomUniquePlanet(already);
    return randomUniqueSpectral(already);
}

PackContent generatePackContent(PackKind k, PackSize s, bool omenGlobe,
                                bool telescope, ConsumableType telescopePlanet,
                                const QVector<JokerType> &ownedJokers,
                                bool allowDuplicateJokers,
                                bool grosMichelExtinct) {
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
    case PackKind::Celestial:
    case PackKind::Spectral:
        for (int i = 0; i < pc.optionsToShow; ++i) {
            if (k == PackKind::Celestial && telescope && i == 0 &&
                kindOf(telescopePlanet) == ConsumableKind::Planet) {
                pc.consumables.append(telescopePlanet);
                usedConsumables.append(telescopePlanet);
            } else {
                pc.consumables.append(randomPackConsumableWithSpecials(k, usedConsumables, omenGlobe));
            }
        }
        break;
    case PackKind::Buffoon:
        for (int i = 0; i < pc.optionsToShow; ++i) {
            JokerType j = randomBuffoonJoker(ownedJokers, usedJokers, allowDuplicateJokers, grosMichelExtinct);
            pc.jokers.append(j);
            usedJokers.append(j);
        }
        break;
    }
    return pc;
}
