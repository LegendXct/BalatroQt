#include "consumable.h"
#include "../game/gamestate.h"
#include "../game/handevaluator.h"
#include <QRandomGenerator>
#include <algorithm>

ConsumableKind kindOf(ConsumableType t) {
    if (static_cast<int>(t) < static_cast<int>(ConsumableType::Planet_Pluto))
        return ConsumableKind::Tarot;
    if (static_cast<int>(t) < static_cast<int>(ConsumableType::Spectral_Familiar))
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

static Enhancement randomNonStoneEnhancement()
{
    constexpr Enhancement pool[] = {
        Enhancement::Bonus, Enhancement::Mult, Enhancement::Wild,
        Enhancement::Glass, Enhancement::Steel, Enhancement::Gold,
        Enhancement::Lucky
    };
    return pool[QRandomGenerator::global()->bounded(int(sizeof(pool)/sizeof(*pool)))];
}

static CardData makeRandomCard(Rank rank, bool fixedRank)
{
    auto *rng = QRandomGenerator::global();
    CardData c;
    c.suit = static_cast<Suit>(rng->bounded(4));
    if (fixedRank) {
        c.rank = rank;
    } else {
        c.rank = static_cast<Rank>(rng->bounded(13) + 2);
    }
    c.enhancement = randomNonStoneEnhancement();
    return c;
}

static void destroyRandomAndCreateCards(UseContext &ctx, int createCount, const QVector<Rank> &rankPool)
{
    auto *rng = QRandomGenerator::global();
    auto &hand = ctx.state.handMutable();
    if (!hand.isEmpty()) {
        int victim = rng->bounded(hand.size());
        ctx.state.notifyPlayingCardDestroyed(hand[victim]);
        hand.removeAt(victim);
    }
    for (int i = 0; i < createCount; ++i) {
        Rank r = rankPool[rng->bounded(rankPool.size())];
        hand.append(makeRandomCard(r, true));
    }
    ctx.state.notifyHandChanged();
}

static void setAllHandSuit(UseContext &ctx)
{
    auto *rng = QRandomGenerator::global();
    auto &hand = ctx.state.handMutable();
    Suit s = static_cast<Suit>(rng->bounded(4));
    for (CardData &c : hand) {
        if (c.enhancement != Enhancement::Stone) c.suit = s;
    }
    ctx.state.notifyHandChanged();
}

static void setAllHandRank(UseContext &ctx)
{
    auto *rng = QRandomGenerator::global();
    auto &hand = ctx.state.handMutable();
    Rank r = static_cast<Rank>(rng->bounded(13) + 2);
    for (CardData &c : hand) {
        if (c.enhancement != Enhancement::Stone) c.rank = r;
    }
    ctx.state.addPermanentHandSizeBonus(-1);
    ctx.state.notifyHandChanged();
}

static void copySelectedCardTwice(UseContext &ctx)
{
    if (ctx.selectedHandIdx.size() != 1) return;
    auto &hand = ctx.state.handMutable();
    int idx = ctx.selectedHandIdx.first();
    if (idx < 0 || idx >= hand.size()) return;
    CardData c = hand[idx];
    hand.append(c);
    hand.append(c);
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
        ctx.state.notifyPlayingCardDestroyed(hand[idx]);
        hand.removeAt(idx);      // 幻灵牌 Immolate 是“摧毁”，不进弃牌堆
        destroyed++;
    }
    if (destroyed > 0) ctx.state.addGold(goldGain);
    ctx.state.notifyHandChanged();
}

static void planetLevelUp(UseContext &ctx, HandType t) {
    ctx.state.levelUpHand(t, 1);
}

static Rank nextRank(Rank r)
{
    if (r == Rank::Ace) return Rank::Ace;
    return static_cast<Rank>(static_cast<int>(r) + 1);
}

static void rankUpSelected(UseContext &ctx, int maxN)
{
    auto &hand = ctx.state.handMutable();
    int applied = 0;
    for (int idx : ctx.selectedHandIdx) {
        if (applied >= maxN) break;
        if (idx < 0 || idx >= hand.size()) continue;
        if (hand[idx].enhancement != Enhancement::Stone) hand[idx].rank = nextRank(hand[idx].rank);
        ++applied;
    }
    ctx.state.notifyHandChanged();
}

static void suitSelected(UseContext &ctx, Suit suit, int maxN)
{
    auto &hand = ctx.state.handMutable();
    int applied = 0;
    for (int idx : ctx.selectedHandIdx) {
        if (applied >= maxN) break;
        if (idx < 0 || idx >= hand.size()) continue;
        if (hand[idx].enhancement != Enhancement::Stone) hand[idx].suit = suit;
        ++applied;
    }
    ctx.state.notifyHandChanged();
}

static void deathConvertSelected(UseContext &ctx)
{
    if (ctx.selectedHandIdx.size() != 2) return;
    auto &hand = ctx.state.handMutable();
    int a = ctx.selectedHandIdx[0];
    int b = ctx.selectedHandIdx[1];
    if (a < 0 || a >= hand.size() || b < 0 || b >= hand.size() || a == b) return;
    // 原版 Death：选 2 张，左侧牌变成右侧牌。当前选择向量按位置升序，
    // 用较左的一张复制较右的一张，保留 UI 上“左变右”的直觉。
    hand[a] = hand[b];
    ctx.state.notifyHandChanged();
}

static void temperanceGain(UseContext &ctx)
{
    int total = 0;
    for (const Joker &j : ctx.state.jokers()) total += qMax(1, j.sellValue);
    ctx.state.addGold(qMin(total, 50));
}

static void wheelOfFortune(UseContext &ctx)
{
    if (QRandomGenerator::global()->bounded(4) != 0) return;
    constexpr Edition editions[] = { Edition::Foil, Edition::Holographic, Edition::Polychrome };
    Edition e = editions[QRandomGenerator::global()->bounded(3)];
    ctx.state.setRandomEditionlessJoker(e, false, false);
}

static void addPlanets(UseContext &ctx, int count)
{
    for (int i = 0; i < count; ++i) {
        if (!ctx.state.addConsumable(randomPlanetType())) break;
    }
}

static void addTarots(UseContext &ctx, int count)
{
    for (int i = 0; i < count; ++i) {
        if (!ctx.state.addConsumable(randomTarotType())) break;
    }
}

Consumable createConsumable(ConsumableType type) {
    Consumable c;
    c.type = type;
    c.kind = kindOf(type);
    c.sellValue = (c.kind == ConsumableKind::Spectral) ? 2 : 1;

    switch (type) {
    case ConsumableType::Tarot_Fool:
        c.name = "愚者"; c.description = "生成上一张使用过的塔罗/星球牌（不包括愚者和幻灵牌）";
        c.effect = [](UseContext &ctx) { ctx.state.addFoolCopyConsumable(); };
        break;
    case ConsumableType::Tarot_Magician:
        c.name = "魔术师"; c.description = "把最多 2 张选中手牌变为 Lucky 增强";
        c.needsSelection = 1; c.maxSelection = 2;
        c.effect = [](UseContext &ctx) { enhanceSelected(ctx, Enhancement::Lucky, 2); };
        break;
    case ConsumableType::Tarot_HighPriestess:
        c.name = "女祭司"; c.description = "生成最多 2 张随机星球牌";
        c.effect = [](UseContext &ctx) { addPlanets(ctx, 2); };
        break;
    case ConsumableType::Tarot_Empress:
        c.name = "皇后";   c.description = "把最多 2 张选中手牌变为 Mult 增强";
        c.needsSelection = 1; c.maxSelection = 2;
        c.effect = [](UseContext &ctx) { enhanceSelected(ctx, Enhancement::Mult, 2); };
        break;
    case ConsumableType::Tarot_Emperor:
        c.name = "皇帝"; c.description = "生成最多 2 张随机塔罗牌";
        c.effect = [](UseContext &ctx) { addTarots(ctx, 2); };
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
    case ConsumableType::Tarot_Justice:
        c.name = "正义"; c.description = "把 1 张选中手牌变为 Glass 增强";
        c.needsSelection = 1; c.maxSelection = 1;
        c.effect = [](UseContext &ctx) { enhanceSelected(ctx, Enhancement::Glass, 1); };
        break;
    case ConsumableType::Tarot_Tower:
        c.name = "塔";     c.description = "把 1 张选中手牌变为 Stone 增强";
        c.needsSelection = 1; c.maxSelection = 1;
        c.effect = [](UseContext &ctx) { enhanceSelected(ctx, Enhancement::Stone, 1); };
        break;
    case ConsumableType::Tarot_HangedMan:
        c.name = "倒吊人"; c.description = "摧毁最多 2 张选中手牌";
        c.needsSelection = 1; c.maxSelection = 2;
        c.effect = [](UseContext &ctx) { destroySelectedGainGold(ctx, 2, 0); };
        break;
    case ConsumableType::Tarot_Hermit:
        c.name = "隐者";   c.description = "金币翻倍（最多 +20）";
        c.effect = [](UseContext &ctx) {
            int gain = qMin(ctx.state.gold(), 20);
            ctx.state.addGold(gain);
        };
        break;
    case ConsumableType::Tarot_Wheel:
        c.name = "命运之轮"; c.description = "1/4 概率给随机无版本小丑添加闪箔/镭射/多彩";
        c.effect = [](UseContext &ctx) { wheelOfFortune(ctx); };
        break;
    case ConsumableType::Tarot_Strength:
        c.name = "力量"; c.description = "最多 2 张选中手牌点数 +1";
        c.needsSelection = 1; c.maxSelection = 2;
        c.effect = [](UseContext &ctx) { rankUpSelected(ctx, 2); };
        break;
    case ConsumableType::Tarot_Death:
        c.name = "死神"; c.description = "选 2 张牌：左侧牌复制右侧牌";
        c.needsSelection = 2; c.maxSelection = 2;
        c.effect = [](UseContext &ctx) { deathConvertSelected(ctx); };
        break;
    case ConsumableType::Tarot_Temperance:
        c.name = "节制"; c.description = "获得当前小丑总售价的金币，最多 +$50";
        c.effect = [](UseContext &ctx) { temperanceGain(ctx); };
        break;
    case ConsumableType::Tarot_Devil:
        c.name = "恶魔"; c.description = "把 1 张选中手牌变为 Gold 增强";
        c.needsSelection = 1; c.maxSelection = 1;
        c.effect = [](UseContext &ctx) { enhanceSelected(ctx, Enhancement::Gold, 1); };
        break;
    case ConsumableType::Tarot_Star:
        c.name = "星星"; c.description = "把最多 3 张选中手牌变为方块";
        c.needsSelection = 1; c.maxSelection = 3;
        c.effect = [](UseContext &ctx) { suitSelected(ctx, Suit::Diamonds, 3); };
        break;
    case ConsumableType::Tarot_Moon:
        c.name = "月亮"; c.description = "把最多 3 张选中手牌变为梅花";
        c.needsSelection = 1; c.maxSelection = 3;
        c.effect = [](UseContext &ctx) { suitSelected(ctx, Suit::Clubs, 3); };
        break;
    case ConsumableType::Tarot_Sun:
        c.name = "太阳"; c.description = "把最多 3 张选中手牌变为红心";
        c.needsSelection = 1; c.maxSelection = 3;
        c.effect = [](UseContext &ctx) { suitSelected(ctx, Suit::Hearts, 3); };
        break;
    case ConsumableType::Tarot_Judgement:
        c.name = "审判"; c.description = "生成 1 张随机小丑";
        c.effect = [](UseContext &ctx) { ctx.state.addRandomRareJoker(); };
        break;
    case ConsumableType::Tarot_World:
        c.name = "世界"; c.description = "把最多 3 张选中手牌变为黑桃";
        c.needsSelection = 1; c.maxSelection = 3;
        c.effect = [](UseContext &ctx) { suitSelected(ctx, Suit::Spades, 3); };
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

    case ConsumableType::Spectral_Familiar:
        c.name = "熟悉感"; c.description = "摧毁 1 张随机手牌，生成 3 张随机增强人头牌";
        c.effect = [](UseContext &ctx) {
            destroyRandomAndCreateCards(ctx, 3, {Rank::Jack, Rank::Queen, Rank::King});
        };
        break;
    case ConsumableType::Spectral_Grim:
        c.name = "阴森"; c.description = "摧毁 1 张随机手牌，生成 2 张随机增强 A";
        c.effect = [](UseContext &ctx) {
            destroyRandomAndCreateCards(ctx, 2, {Rank::Ace});
        };
        break;
    case ConsumableType::Spectral_Incantation:
        c.name = "咒语"; c.description = "摧毁 1 张随机手牌，生成 4 张随机增强数字牌";
        c.effect = [](UseContext &ctx) {
            destroyRandomAndCreateCards(ctx, 4, {Rank::Two, Rank::Three, Rank::Four, Rank::Five, Rank::Six, Rank::Seven, Rank::Eight, Rank::Nine, Rank::Ten});
        };
        break;
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
    case ConsumableType::Spectral_Wraith:
        c.name = "幽灵"; c.description = "生成 1 张稀有小丑，金币归零";
        c.effect = [](UseContext &ctx) {
            if (ctx.state.addRandomRareJoker()) ctx.state.addGold(-ctx.state.gold());
        };
        break;
    case ConsumableType::Spectral_Sigil:
        c.name = "符印"; c.description = "把所有手牌变成同一随机花色";
        c.effect = [](UseContext &ctx) { setAllHandSuit(ctx); };
        break;
    case ConsumableType::Spectral_Ouija:
        c.name = "通灵板"; c.description = "把所有手牌变成同一随机点数，手牌上限 -1";
        c.effect = [](UseContext &ctx) { setAllHandRank(ctx); };
        break;
    case ConsumableType::Spectral_Ectoplasm:
        c.name = "灵质"; c.description = "随机小丑变成负片，手牌上限 -1";
        c.effect = [](UseContext &ctx) { ctx.state.setRandomEditionlessJoker(Edition::Negative, false, true); };
        break;
    case ConsumableType::Spectral_Immolate:
        c.name = "火祭"; c.description = "随机摧毁 5 张手牌，不足 5 张则全毁，获得 $20";
        c.needsSelection = 0; c.maxSelection = 0;
        c.effect = [](UseContext &ctx) { ctx.state.immolateRandomHandCards(5, 20); };
        break;
    case ConsumableType::Spectral_Ankh:
        c.name = "十字章"; c.description = "复制 1 张随机小丑，并摧毁其他小丑";
        c.effect = [](UseContext &ctx) { ctx.state.duplicateRandomJokerAndDestroyOthers(); };
        break;
    case ConsumableType::Spectral_DejaVu:
        c.name = "既视感"; c.description = "把 1 张选中手牌加上红色印章";
        c.needsSelection = 1; c.maxSelection = 1;
        c.effect = [](UseContext &ctx) { sealSelected(ctx, Seal::Red, 1); };
        break;
    case ConsumableType::Spectral_Hex:
        c.name = "妖法"; c.description = "随机小丑变成多彩，并摧毁其他小丑";
        c.effect = [](UseContext &ctx) { ctx.state.setRandomEditionlessJoker(Edition::Polychrome, true, false); };
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
    case ConsumableType::Spectral_Cryptid:
        c.name = "隐秘"; c.description = "复制 1 张选中手牌 2 次";
        c.needsSelection = 1; c.maxSelection = 1;
        c.effect = [](UseContext &ctx) { copySelectedCardTwice(ctx); };
        break;
    case ConsumableType::Spectral_Soul:
        c.name = "灵魂"; c.description = "生成 1 张传奇小丑（只能由灵魂牌获得）";
        c.needsSelection = 0; c.maxSelection = 0;
        c.effect = [](UseContext &ctx) { ctx.state.addRandomLegendaryJoker(); };
        break;
    case ConsumableType::Spectral_BlackHole:
        c.name = "黑洞"; c.description = "所有牌型等级 +1";
        c.needsSelection = 0; c.maxSelection = 0;
        c.effect = [](UseContext &ctx) { ctx.state.levelUpAllHands(1); };
        break;
    }
    return c;
}

ConsumableType randomTarotType() {
    constexpr ConsumableType pool[] = {
        ConsumableType::Tarot_Fool, ConsumableType::Tarot_Magician,
        ConsumableType::Tarot_HighPriestess, ConsumableType::Tarot_Empress,
        ConsumableType::Tarot_Emperor, ConsumableType::Tarot_Hierophant,
        ConsumableType::Tarot_Lovers, ConsumableType::Tarot_Chariot,
        ConsumableType::Tarot_Justice, ConsumableType::Tarot_Hermit,
        ConsumableType::Tarot_Wheel, ConsumableType::Tarot_Strength,
        ConsumableType::Tarot_HangedMan, ConsumableType::Tarot_Death,
        ConsumableType::Tarot_Temperance, ConsumableType::Tarot_Devil,
        ConsumableType::Tarot_Tower, ConsumableType::Tarot_Star,
        ConsumableType::Tarot_Moon, ConsumableType::Tarot_Sun,
        ConsumableType::Tarot_Judgement, ConsumableType::Tarot_World,
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
        ConsumableType::Spectral_Familiar, ConsumableType::Spectral_Grim,
        ConsumableType::Spectral_Incantation, ConsumableType::Spectral_Talisman,
        ConsumableType::Spectral_Aura, ConsumableType::Spectral_Wraith,
        ConsumableType::Spectral_Sigil, ConsumableType::Spectral_Ouija,
        ConsumableType::Spectral_Ectoplasm, ConsumableType::Spectral_Immolate,
        ConsumableType::Spectral_Ankh, ConsumableType::Spectral_DejaVu,
        ConsumableType::Spectral_Hex, ConsumableType::Spectral_Trance,
        ConsumableType::Spectral_Medium, ConsumableType::Spectral_Cryptid,
    };
    int n = int(sizeof(pool)/sizeof(pool[0]));
    return pool[QRandomGenerator::global()->bounded(n)];
}

