#include "consumable.h"
#include "../game/gamestate.h"
#include "../game/handevaluator.h"
#include <QRandomGenerator>

ConsumableKind kindOf(ConsumableType t) {
    if (static_cast<int>(t) < static_cast<int>(ConsumableType::Planet_Pluto))
        return ConsumableKind::Tarot;
    if (static_cast<int>(t) < static_cast<int>(ConsumableType::Spectral_Talisman))
        return ConsumableKind::Planet;
    return ConsumableKind::Spectral;
}

// 把 ctx.selectedHandIdx 指到的手牌应用 enhancement，最多 maxN 张
static void enhanceSelected(UseContext &ctx, Enhancement e, int maxN) {
    auto &hand = ctx.state.handMutable();
    int applied = 0;
    for (int idx : ctx.selectedHandIdx) {
        if (applied >= maxN) break;
        if (idx < 0 || idx >= hand.size()) continue;
        hand[idx].enhancement = e;
        applied++;
    }
    ctx.state.notifyHandChanged();
}

static void sealSelected(UseContext &ctx, Seal s, int maxN) {
    auto &hand = ctx.state.handMutable();
    int applied = 0;
    for (int idx : ctx.selectedHandIdx) {
        if (applied >= maxN) break;
        if (idx < 0 || idx >= hand.size()) continue;
        hand[idx].seal = s;
        applied++;
    }
    ctx.state.notifyHandChanged();
}

static void randomEditionSelected(UseContext &ctx, int maxN) {
    auto &hand = ctx.state.handMutable();
    int applied = 0;
    for (int idx : ctx.selectedHandIdx) {
        if (applied >= maxN) break;
        if (idx < 0 || idx >= hand.size()) continue;
        int r = QRandomGenerator::global()->bounded(3);
        if      (r == 0) hand[idx].edition = Edition::Foil;
        else if (r == 1) hand[idx].edition = Edition::Holographic;
        else             hand[idx].edition = Edition::Polychrome;
        applied++;
    }
    ctx.state.notifyHandChanged();
}

static void destroySelectedGainGold(UseContext &ctx, int maxN, int goldGain) {
    auto &hand = ctx.state.handMutable();
    QVector<int> sel = ctx.selectedHandIdx;
    std::sort(sel.begin(), sel.end(), std::greater<int>());

    int destroyed = 0;
    for (int idx : sel) {
        if (destroyed >= maxN) break;
        if (idx < 0 || idx >= hand.size()) continue;
        hand.removeAt(idx);      // 幻灵牌 Immolate 是“摧毁”，不进弃牌堆
        destroyed++;
    }
    if (destroyed > 0) ctx.state.addGold(goldGain);
    ctx.state.notifyHandChanged();
}

static void planetLevelUp(UseContext &ctx, HandType t) {
    ctx.state.levelUpHand(t, 1);
}

Consumable createConsumable(ConsumableType type) {
    Consumable c;
    c.type = type;
    c.kind = kindOf(type);
    c.sellValue = (c.kind == ConsumableKind::Spectral) ? 2 : 1;

    switch (type) {
    case ConsumableType::Tarot_Empress:
        c.name = "皇后";   c.description = "把最多 2 张选中手牌变为 Mult 增强";
        c.needsSelection = 1; c.maxSelection = 2;
        c.effect = [](UseContext &ctx) { enhanceSelected(ctx, Enhancement::Mult, 2); };
        break;
    case ConsumableType::Tarot_Hierophant:
        c.name = "教皇";   c.description = "把最多 2 张选中手牌变为 Bonus 增强";
        c.needsSelection = 1; c.maxSelection = 2;
        c.effect = [](UseContext &ctx) { enhanceSelected(ctx, Enhancement::Bonus, 2); };
        break;
    case ConsumableType::Tarot_Chariot:
        c.name = "战车";   c.description = "把 1 张选中手牌变为 Steel 增强";
        c.needsSelection = 1; c.maxSelection = 1;
        c.effect = [](UseContext &ctx) { enhanceSelected(ctx, Enhancement::Steel, 1); };
        break;
    case ConsumableType::Tarot_Lovers:
        c.name = "恋人";   c.description = "把 1 张选中手牌变为 Wild 增强";
        c.needsSelection = 1; c.maxSelection = 1;
        c.effect = [](UseContext &ctx) { enhanceSelected(ctx, Enhancement::Wild, 1); };
        break;
    case ConsumableType::Tarot_Tower:
        c.name = "塔";     c.description = "把 1 张选中手牌变为 Stone 增强";
        c.needsSelection = 1; c.maxSelection = 1;
        c.effect = [](UseContext &ctx) { enhanceSelected(ctx, Enhancement::Stone, 1); };
        break;
    case ConsumableType::Tarot_Hermit:
        c.name = "隐者";   c.description = "金币翻倍（最多 +20）";
        c.effect = [](UseContext &ctx) {
            int gain = qMin(ctx.state.gold(), 20);
            ctx.state.addGold(gain);
        };
        break;

    case ConsumableType::Planet_Pluto:
        c.name = "冥王星";   c.description = "升级【高牌】";
        c.effect = [](UseContext &ctx) { planetLevelUp(ctx, HandType::HighCard); }; break;
    case ConsumableType::Planet_Mercury:
        c.name = "水星";     c.description = "升级【对子】";
        c.effect = [](UseContext &ctx) { planetLevelUp(ctx, HandType::Pair); }; break;
    case ConsumableType::Planet_Uranus:
        c.name = "天王星";   c.description = "升级【两对】";
        c.effect = [](UseContext &ctx) { planetLevelUp(ctx, HandType::TwoPair); }; break;
    case ConsumableType::Planet_Venus:
        c.name = "金星";     c.description = "升级【三条】";
        c.effect = [](UseContext &ctx) { planetLevelUp(ctx, HandType::ThreeOfAKind); }; break;
    case ConsumableType::Planet_Saturn:
        c.name = "土星";     c.description = "升级【顺子】";
        c.effect = [](UseContext &ctx) { planetLevelUp(ctx, HandType::Straight); }; break;
    case ConsumableType::Planet_Jupiter:
        c.name = "木星";     c.description = "升级【同花】";
        c.effect = [](UseContext &ctx) { planetLevelUp(ctx, HandType::Flush); }; break;
    case ConsumableType::Planet_Earth:
        c.name = "地球";     c.description = "升级【葫芦】";
        c.effect = [](UseContext &ctx) { planetLevelUp(ctx, HandType::FullHouse); }; break;
    case ConsumableType::Planet_Mars:
        c.name = "火星";     c.description = "升级【四条】";
        c.effect = [](UseContext &ctx) { planetLevelUp(ctx, HandType::FourOfAKind); }; break;
    case ConsumableType::Planet_Neptune:
        c.name = "海王星";   c.description = "升级【同花顺】";
        c.effect = [](UseContext &ctx) { planetLevelUp(ctx, HandType::StraightFlush); }; break;
    case ConsumableType::Planet_PlanetX:
        c.name = "X 行星";   c.description = "升级【五条】";
        c.effect = [](UseContext &ctx) { planetLevelUp(ctx, HandType::FiveOfAKind); }; break;
    case ConsumableType::Planet_Ceres:
        c.name = "谷神星";   c.description = "升级【同花葫芦】";
        c.effect = [](UseContext &ctx) { planetLevelUp(ctx, HandType::FlushHouse); }; break;
    case ConsumableType::Planet_Eris:
        c.name = "厄里斯";   c.description = "升级【同花五条】";
        c.effect = [](UseContext &ctx) { planetLevelUp(ctx, HandType::FlushFive); }; break;

    case ConsumableType::Spectral_Talisman:
        c.name = "护身符"; c.description = "把 1 张选中手牌加上金色印章";
        c.needsSelection = 1; c.maxSelection = 1;
        c.effect = [](UseContext &ctx) { sealSelected(ctx, Seal::Gold, 1); };
        break;
    case ConsumableType::Spectral_Aura:
        c.name = "光环"; c.description = "把 1 张选中手牌变为随机版本";
        c.needsSelection = 1; c.maxSelection = 1;
        c.effect = [](UseContext &ctx) { randomEditionSelected(ctx, 1); };
        break;
    case ConsumableType::Spectral_Immolate:
        c.name = "火祭"; c.description = "摧毁最多 5 张选中手牌，获得 $20";
        c.needsSelection = 1; c.maxSelection = 5;
        c.effect = [](UseContext &ctx) { destroySelectedGainGold(ctx, 5, 20); };
        break;
    case ConsumableType::Spectral_DejaVu:
        c.name = "既视感"; c.description = "把 1 张选中手牌加上红色印章";
        c.needsSelection = 1; c.maxSelection = 1;
        c.effect = [](UseContext &ctx) { sealSelected(ctx, Seal::Red, 1); };
        break;
    case ConsumableType::Spectral_Trance:
        c.name = "恍惚"; c.description = "把 1 张选中手牌加上蓝色印章";
        c.needsSelection = 1; c.maxSelection = 1;
        c.effect = [](UseContext &ctx) { sealSelected(ctx, Seal::Blue, 1); };
        break;
    case ConsumableType::Spectral_Medium:
        c.name = "灵媒"; c.description = "把 1 张选中手牌加上紫色印章";
        c.needsSelection = 1; c.maxSelection = 1;
        c.effect = [](UseContext &ctx) { sealSelected(ctx, Seal::Purple, 1); };
        break;
    }
    return c;
}

ConsumableType randomTarotType() {
    constexpr ConsumableType pool[] = {
        ConsumableType::Tarot_Empress,    ConsumableType::Tarot_Hierophant,
        ConsumableType::Tarot_Chariot,    ConsumableType::Tarot_Lovers,
        ConsumableType::Tarot_Hermit,     ConsumableType::Tarot_Tower,
    };
    int n = int(sizeof(pool)/sizeof(pool[0]));
    return pool[QRandomGenerator::global()->bounded(n)];
}

ConsumableType randomPlanetType() {
    constexpr ConsumableType pool[] = {
        ConsumableType::Planet_Pluto,   ConsumableType::Planet_Mercury,
        ConsumableType::Planet_Uranus,  ConsumableType::Planet_Venus,
        ConsumableType::Planet_Saturn,  ConsumableType::Planet_Jupiter,
        ConsumableType::Planet_Earth,   ConsumableType::Planet_Mars,
        ConsumableType::Planet_Neptune, ConsumableType::Planet_PlanetX,
        ConsumableType::Planet_Ceres,   ConsumableType::Planet_Eris,
    };
    int n = int(sizeof(pool)/sizeof(pool[0]));
    return pool[QRandomGenerator::global()->bounded(n)];
}

ConsumableType randomSpectralType() {
    constexpr ConsumableType pool[] = {
        ConsumableType::Spectral_Talisman,
        ConsumableType::Spectral_Aura,
        ConsumableType::Spectral_Immolate,
        ConsumableType::Spectral_DejaVu,
        ConsumableType::Spectral_Trance,
        ConsumableType::Spectral_Medium,
    };
    int n = int(sizeof(pool)/sizeof(pool[0]));
    return pool[QRandomGenerator::global()->bounded(n)];
}
