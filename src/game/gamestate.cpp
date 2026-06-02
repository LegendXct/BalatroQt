#include "gamestate.h"
#include "demoscript.h"
#include <QSet>
#include <QRandomGenerator>
#include <limits>
#include <cmath>
#include <algorithm>
#include <functional>
#include <cmath>

GameState::GameState(QObject *parent)
    : QObject{parent}
{
    startGame();
}

QPair<int, int> GameState::handLevelDelta(HandType t) {
    switch (t) {
    case HandType::HighCard:      return { 10, 1 };
    case HandType::Pair:          return { 15, 1 };
    case HandType::TwoPair:       return { 20, 1 };
    case HandType::ThreeOfAKind:  return { 20, 2 };
    case HandType::Straight:      return { 30, 3 };
    case HandType::Flush:         return { 15, 2 };
    case HandType::FullHouse:     return { 25, 2 };
    case HandType::FourOfAKind:   return { 30, 3 };
    case HandType::StraightFlush: return { 40, 4 };
    case HandType::RoyalFlush:    return { 40, 4 };
    case HandType::FiveOfAKind:   return { 35, 3 };
    case HandType::FlushHouse:    return { 40, 4 };
    case HandType::FlushFive:     return { 50, 3 };
    }
    return { 0, 0 };
}

void GameState::levelUpHand(HandType t, int times) {
    if (mDryRun) return;   // 最佳出牌提示模拟期间不得真正升级牌型
    for (Joker &j : mJokers) {
        if (!j.isDebuffed && j.type == JokerType::Constellation) j.counter += times;
    }
    HandLevel &lv = mHandLevels[t];
    auto d = handLevelDelta(t);
    for (int i = 0; i < times; ++i) {
        lv.level++;
        lv.chipsBonus += d.first;
        lv.multBonus  += d.second;
    }
    emit handLevelsChanged();
}


static bool consumableNeedsFreeJokerSlot(ConsumableType type)
{
    return type == ConsumableType::Tarot_Judgement
        || type == ConsumableType::Spectral_Wraith
        || type == ConsumableType::Spectral_Soul;
}

static int editionExtraCost(Edition e)
{
    switch (e) {
    case Edition::Foil:        return 2;
    case Edition::Holographic: return 3;
    case Edition::Polychrome:  return 5;
    case Edition::Negative:    return 5;
    default:                   return 0;
    }
}

static int discountedCost(int rawCost, int discountPercent)
{
    if (discountPercent <= 0) return qMax(1, rawCost);
    return qMax(1, int(std::floor((rawCost + 0.5) * (100 - discountPercent) / 100.0)));
}

static bool telescopeHandVisible(HandType t, int played)
{
    switch (t) {
    case HandType::HighCard:
    case HandType::Pair:
    case HandType::TwoPair:
    case HandType::ThreeOfAKind:
    case HandType::Straight:
    case HandType::Flush:
    case HandType::FullHouse:
    case HandType::FourOfAKind:
    case HandType::StraightFlush:
        return true;
    default:
        return played > 0;
    }
}

static bool planetMatchesHand(ConsumableType type, HandType hand)
{
    switch (type) {
    case ConsumableType::Planet_Pluto:   return hand == HandType::HighCard;
    case ConsumableType::Planet_Mercury: return hand == HandType::Pair;
    case ConsumableType::Planet_Uranus:  return hand == HandType::TwoPair;
    case ConsumableType::Planet_Venus:   return hand == HandType::ThreeOfAKind;
    case ConsumableType::Planet_Saturn:  return hand == HandType::Straight;
    case ConsumableType::Planet_Jupiter: return hand == HandType::Flush;
    case ConsumableType::Planet_Earth:   return hand == HandType::FullHouse;
    case ConsumableType::Planet_Mars:    return hand == HandType::FourOfAKind;
    case ConsumableType::Planet_Neptune: return hand == HandType::StraightFlush || hand == HandType::RoyalFlush;
    case ConsumableType::Planet_PlanetX: return hand == HandType::FiveOfAKind;
    case ConsumableType::Planet_Ceres:   return hand == HandType::FlushHouse;
    case ConsumableType::Planet_Eris:    return hand == HandType::FlushFive;
    default: return false;
    }
}

static auto rankComp = [](const CardData &a, const CardData &b) {
    const bool aStone = a.enhancement == Enhancement::Stone;
    const bool bStone = b.enhancement == Enhancement::Stone;
    // 原版石头牌没有点数/花色；无论按点数还是按花色理牌，都应该沉到最后。
    if (aStone != bStone) return !aStone;
    if (a.rank != b.rank) return static_cast<int>(a.rank) > static_cast<int>(b.rank);
    return static_cast<int>(a.suit) < static_cast<int>(b.suit);
};

static bool jokerUsesCardRankOrSuit(JokerType type)
{
    switch (type) {
    case JokerType::GreedyJoker:
    case JokerType::LustyJoker:
    case JokerType::WrathfulJoker:
    case JokerType::GluttonousJoker:
    case JokerType::Fibonacci:
    case JokerType::EvenSteven:
    case JokerType::OddTodd:
    case JokerType::Scholar:
    case JokerType::ScaryFace:
    case JokerType::SmileyFace:
    case JokerType::WalkieTalkie:
    case JokerType::Arrowhead:
    case JokerType::OnyxAgate:
    case JokerType::RoughGem:
    case JokerType::Bloodstone:
    case JokerType::Photograph:
    case JokerType::Triboulet:
    case JokerType::AncientJoker:
    case JokerType::TheIdol:
        return true;
    default:
        return false;
    }
}

static bool bossDebuffsCard(BossEffect effect, const CardData &c)
{
    // 石头牌没有花色和点数，不能被花色/人头类 Boss 盲注按旧底牌属性限制。
    if (c.enhancement == Enhancement::Stone) return false;
    if (effect == BossEffect::TheClub  && c.suit == Suit::Clubs)    return true;
    if (effect == BossEffect::TheGoad  && c.suit == Suit::Spades)   return true;
    if (effect == BossEffect::TheHead  && c.suit == Suit::Hearts)   return true;
    if (effect == BossEffect::TheWindow&& c.suit == Suit::Diamonds) return true;
    if (effect == BossEffect::ThePlant &&
        (c.rank == Rank::Jack || c.rank == Rank::Queen || c.rank == Rank::King)) return true;
    return false;
}

// 原版 Blueprint / Brainstorm 不只是复制计分阶段，也会复制 Perkeo、DNA 等
// calculate_joker 风格效果。这里先把“最终被复制到的真实小丑下标”解析出来，
// 这样计分、离开商店、提示 UI 都能复用同一套规则。
static int resolveCopiedJokerIndex(const QVector<Joker> &jokers, int idx)
{
    QSet<int> seen;
    int cur = idx;
    for (int guard = 0; guard < jokers.size() + 4; ++guard) {
        if (cur < 0 || cur >= jokers.size()) return -1;
        if (seen.contains(cur)) return -1;
        seen.insert(cur);

        const Joker &j = jokers[cur];
        if (j.type == JokerType::Blueprint) {
            cur = cur + 1;
            continue;
        }
        if (j.type == JokerType::Brainstorm) {
            cur = 0;
            continue;
        }
        return cur;
    }
    return -1;
}

static const Joker *resolveCopiedJoker(const QVector<Joker> &jokers, int idx)
{
    int resolved = resolveCopiedJokerIndex(jokers, idx);
    if (resolved < 0 || resolved >= jokers.size()) return nullptr;
    return &jokers[resolved];
}


static int countResolvedJokersOfType(const QVector<Joker> &jokers, JokerType type)
{
    int count = 0;
    for (int i = 0; i < jokers.size(); ++i) {
        if (jokers[i].isDebuffed) continue;
        const Joker *resolved = resolveCopiedJoker(jokers, i);
        if (resolved && !resolved->isDebuffed && resolved->type == type) ++count;
    }
    return count;
}

static bool canRecordForFool(ConsumableType t)
{
    return t != ConsumableType::Tarot_Fool && kindOf(t) != ConsumableKind::Spectral;
}

static void applyResolvedJokerEffect(const Joker &j, TriggerContext &ctx)
{
    if (j.type == JokerType::IceCream) {
        ctx.result.chips += qMax(0, j.counter);
        return;
    }
    if (j.type == JokerType::Hologram) {
        ctx.result.xmult *= (1.0 + 0.25 * qMax(0, j.counter));
        return;
    }
    if (j.type == JokerType::Constellation) {
        ctx.result.xmult *= (1.0 + 0.1 * qMax(0, j.counter));
        return;
    }
    if (j.type == JokerType::Vampire) {
        ctx.result.xmult *= (1.0 + 0.1 * qMax(0, j.counter));
        return;
    }
    // Batch 2：计数器型小丑
    switch (j.type) {
    case JokerType::SquareJoker:
    case JokerType::Runner:
    case JokerType::Castle:
    case JokerType::WeeJoker:
        ctx.result.chips += qMax(0, j.counter);                       return;
    case JokerType::GreenJoker:
    case JokerType::SpareTrousers:
    case JokerType::RideTheBus:
    case JokerType::Popcorn:
        ctx.result.mult += qMax(0, j.counter);                        return;
    case JokerType::Obelisk:
        ctx.result.xmult *= (1.0 + 0.2 * qMax(0, j.counter));         return;
    case JokerType::HitTheRoad:
        ctx.result.xmult *= (1.0 + 0.5 * qMax(0, j.counter));         return;
    case JokerType::GlassJoker:
        ctx.result.xmult *= (1.0 + 0.75 * qMax(0, j.counter));        return;
    case JokerType::LuckyCat:
        ctx.result.xmult *= (1.0 + 0.25 * qMax(0, j.counter));        return;
    case JokerType::Ramen:
        ctx.result.xmult *= qMax(1.0, j.counter / 100.0);            return;
    case JokerType::Madness:
        ctx.result.xmult *= (1.0 + qMax(0, j.counter) / 100.0);      return;
    case JokerType::FlashCard:
        ctx.result.mult += 2 * qMax(0, j.counter);                   return;
    case JokerType::FortuneTeller:
        ctx.result.mult += qMax(0, j.counter);                       return;
    case JokerType::Throwback:
    case JokerType::Campfire:
        ctx.result.xmult *= (1.0 + 0.25 * qMax(0, j.counter));       return;
    case JokerType::LoyaltyCard:
        if (j.counter > 0 && j.counter % 6 == 0) ctx.result.xmult *= 4.0;
        return;
    case JokerType::CeremonialDagger:
        ctx.result.mult += qMax(0, j.counter);                       return;
    default: break;
    }
    j.effect(ctx);
}

static auto suitComp = [](const CardData &a, const CardData &b) {
    const bool aStone = a.enhancement == Enhancement::Stone;
    const bool bStone = b.enhancement == Enhancement::Stone;
    if (aStone != bStone) return !aStone;
    if (a.suit != b.suit) return static_cast<int>(a.suit) < static_cast<int>(b.suit);
    return static_cast<int>(a.rank) > static_cast<int>(b.rank);
};

void GameState::sortHandByRank() {
    mSortMode = HandSortMode::ByRank;
    std::sort(mHand.begin(), mHand.end(), rankComp);
    emit handChanged();
}

void GameState::sortHandBySuit() {
    mSortMode = HandSortMode::BySuit;
    std::sort(mHand.begin(), mHand.end(), suitComp);
    emit handChanged();
}


int GameState::deckTotal() const
{
    return mDeck.totalKnown() + mHand.size();
}

QVector<CardData> GameState::remainingDeckCards() const
{
    QVector<CardData> out = mDeck.drawPile();
    bool bossDisabled = hasJokerType(JokerType::Chicot);
    for (CardData &c : out) {
        c.faceUp = true;
        c.isDebuffed = (mPhase == GamePhase::Blind) && !bossDisabled && bossDebuffsCard(mBossEffect, c);
    }
    return out;
}

QVector<CardData> GameState::fullDeckCards() const
{
    QVector<CardData> out = mDeck.allKnownCards();
    bool bossDisabled = hasJokerType(JokerType::Chicot);
    for (CardData &c : out) {
        c.faceUp = true;
        c.isDebuffed = (mPhase == GamePhase::Blind) && !bossDisabled && bossDebuffsCard(mBossEffect, c);
    }
    for (CardData c : mHand) {
        c.faceUp = true;
        c.isDebuffed = (mPhase == GamePhase::Blind) && !bossDisabled && bossDebuffsCard(mBossEffect, c);
        out.append(c);
    }
    return out;
}

QVector<JokerType> GameState::ownedJokerTypes() const
{
    QVector<JokerType> out;
    for (const Joker &j : mJokers) out.append(j.type);
    return out;
}

QVector<ConsumableType> GameState::ownedConsumableTypes() const
{
    QVector<ConsumableType> out;
    for (const Consumable &c : mConsumables) out.append(c.type);
    return out;
}

bool GameState::hasJokerDuplicateBypass() const
{
    // 马戏团长(Showman)：允许小丑/塔罗/星球/幻灵牌在商店与卡包中重复出现。
    return hasJokerType(JokerType::Showman);
}

HandMods GameState::currentHandMods() const
{
    HandMods m;
    for (const Joker &j : mJokers) {
        if (j.isDebuffed) continue;
        switch (j.type) {
        case JokerType::SmearedJoker: m.smeared = true;  break;
        case JokerType::Shortcut:     m.shortcut = true; break;
        case JokerType::Splash:       m.splash = true;   break;
        case JokerType::FourFingers:  m.fourFingers = true; break;
        default: break;
        }
    }
    return m;
}

void GameState::syncShopJokerRules()
{
    mShop.setOwnedJokers(ownedJokerTypes(), hasJokerDuplicateBypass());
    mShop.setOwnedConsumables(ownedConsumableTypes());
    mShop.setGrosMichelExtinct(mGrosMichelExtinct);
}

void GameState::updateOwnedSellValues()
{
    const int discount = mShop.discountPercent();
    for (Joker &j : mJokers) {
        const int rawCost = jokerBaseCost(j.type) + editionExtraCost(j.edition);
        const int currentCost = discountedCost(rawCost, discount);
        j.sellValue = qMax(1, currentCost / 2) + qMax(0, j.extraSellValue);
    }
}

void GameState::dealCards(DrawContext ctx) {
    const bool chicot = hasJokerType(JokerType::Chicot);
    // 巨蛇(The Serpent)：出牌/弃牌后无视手牌上限，固定补 3 张。
    const bool serpent = !chicot && mBossEffect == BossEffect::TheSerpent
                         && ctx != DrawContext::BlindStart;
    const int toDraw = serpent ? 3 : (handSize() - mHand.size());

    for (int i = 0; i < toDraw && !mDeck.isEmpty(); ++i) {
        CardData c = mDeck.draw();
        if (!chicot) {
            bool faceDown = false;
            // 轮子(The Wheel)：1/7 概率背面朝下。
            if (mBossEffect == BossEffect::TheWheel
                && QRandomGenerator::global()->bounded(7) == 0) faceDown = true;
            // 标记(The Mark)：人头牌背面朝下。
            if (mBossEffect == BossEffect::TheMark
                && (c.rank == Rank::Jack || c.rank == Rank::Queen || c.rank == Rank::King))
                faceDown = true;
            // 鱼(The Fish)：每次出牌后补的牌背面朝下。
            if (mBossEffect == BossEffect::TheFish && ctx == DrawContext::AfterPlay)
                faceDown = true;
            if (faceDown) c.faceUp = false;
        }
        mHand.append(c);
    }
    if (mSortMode == HandSortMode::ByRank)
        std::sort(mHand.begin(), mHand.end(), rankComp);
    else if (mSortMode == HandSortMode::BySuit)
        std::sort(mHand.begin(), mHand.end(), suitComp);
    refreshCeruleanForced();
    if (ctx != DrawContext::AfterDiscard)
        applyCrimsonHeartDebuffForNextHand();
    emit handChanged();
}

void GameState::applyCrimsonHeartDebuffForNextHand()
{
    bool changed = false;
    if (mCrimsonHeartDisabled >= 0 && mCrimsonHeartDisabled < mJokers.size()
        && mJokers[mCrimsonHeartDisabled].isDebuffed) {
        mJokers[mCrimsonHeartDisabled].isDebuffed = false;
        changed = true;
    }
    mCrimsonHeartDisabled = -1;

    if (mPhase != GamePhase::Blind
        || mBlindType != BlindType::Boss
        || mBossEffect != BossEffect::CrimsonHeart
        || hasJokerType(JokerType::Chicot)
        || mJokers.isEmpty()) {
        if (changed) emit jokersChanged();
        return;
    }

    QVector<int> candidates;
    candidates.reserve(mJokers.size());
    for (int i = 0; i < mJokers.size(); ++i) {
        if (mJokers.size() > 1 && i == mLastCrimsonHeartDisabled) continue;
        candidates.append(i);
    }
    if (candidates.isEmpty()) {
        for (int i = 0; i < mJokers.size(); ++i) candidates.append(i);
    }

    const int pick = candidates[QRandomGenerator::global()->bounded(candidates.size())];
    mCrimsonHeartDisabled = pick;
    mLastCrimsonHeartDisabled = pick;
    mJokers[pick].isDebuffed = true;
    emit jokersChanged();
}

void GameState::playCards(const QVector<int> &indices) {
    if (indices.isEmpty() || indices.size() > Constants::MAX_PLAY) return;
    if (mHandsLeft <= 0) return;
    if (mAwaitingScoreFinalize) return;

    QVector<int> sorted = indices;
    std::sort(sorted.begin(), sorted.end());
    for (int i : sorted) {
        if (i < 0 || i >= mHand.size()) return;
    }

    // 取出 played，评分动画期间暂时不补牌；补牌要等所有计分动画和收牌动画结束后再做。
    QVector<CardData> played;
    for (int i : sorted) {
        // 背面朝下的牌（房屋/轮子/鱼/标记）一旦被打出就翻回正面，正常参与计分与展示。
        mHand[i].faceUp = true;
        played.append(mHand[i]);
        // 支柱(The Pillar)：记录本 Ante 打出过的牌，进入下一盘时禁用它们。
        mCardsPlayedThisAnte.insert(mHand[i].uid);
    }

    // DNA：只有本盲注第一次出牌且只打 1 张牌时生效。
    // 多个 DNA / 蓝图 / 头脑风暴复制 DNA 时，本次出牌可以创建多张复制牌。
    const bool firstHandOfBlind = (mHandsLeft == mBlindStartingHands);
    mDNAEligibleThisPlay = (!mDNAUsedThisBlind && firstHandOfBlind && sorted.size() == 1);
    if (firstHandOfBlind) mDNAUsedThisBlind = true;
    mDNACopiesCreatedThisPlay = 0;
    mPendingDNACopies.clear();

    HandResult result = HandEvaluator::evaluate(played, currentHandMods());
    if (bossBlocksPlayedHand(result, played.size())) {
        // 原版会阻止非法出牌并给提示；当前 Qt 版先保证不会卡死：
        // 这手按 0 分处理并正常进入收牌/补牌流程。
        result.name = "Not Allowed";
        result.chips = 0;
        result.mult = 0;
        result.xmult = 1.0;
        result.baseChips = 0;
        result.baseMult = 0;
        result.events.clear();
        result.events.append({ ScoreEventKind::NotAllowed, -1, -1, -1, 0, 1.0 });
        mPendingHandScore = 0;
        mPendingPlayedIndices = sorted;
        mPendingShattered = QVector<bool>(played.size(), false);
        mPendingDNACopies.clear();
        mAwaitingScoreFinalize = true;
        mLastResult = result;
        emit handPlayed();
        return;
    }

    HandLevel &lv = mHandLevels[result.type];
    if (!hasJokerType(JokerType::Chicot) && mBossEffect == BossEffect::TheArm && lv.level > 1) {
        auto d = handLevelDelta(result.type);
        lv.level = qMax(1, lv.level - 1);
        lv.chipsBonus = qMax(0, lv.chipsBonus - d.first);
        lv.multBonus  = qMax(0, lv.multBonus  - d.second);
        emit handLevelsChanged();
    }
    result.chips += lv.chipsBonus;
    result.mult  += lv.multBonus;
    result.level  = lv.level;
    result.xmult  = 1.0;

    if (!hasJokerType(JokerType::Chicot) && mBossEffect == BossEffect::TheFlint) {
        result.chips = std::max(0.0, std::floor(result.chips * 0.5 + 0.5));
        result.mult  = std::max(1.0, std::floor(result.mult  * 0.5 + 0.5));
    }

    result.baseChips = result.chips;
    result.baseMult  = result.mult;

    QVector<int> scoringPlayedIdx;
    QSet<int> used;
    for (const CardData &sc : result.scoringCards) {
        for (int k = 0; k < played.size(); ++k) {
            if (used.contains(k)) continue;
            const CardData &p = played[k];
            if (p.rank == sc.rank && p.suit == sc.suit
                && p.enhancement == sc.enhancement
                && p.edition == sc.edition
                && p.seal == sc.seal) {
                scoringPlayedIdx.append(k);
                used.insert(k);
                break;
            }
        }
    }
    // 原版石头牌没有点数/花色，不参与牌型判定；但只要被打出，它自己总是计分 +50。
    for (int k = 0; k < played.size(); ++k) {
        if (played[k].enhancement != Enhancement::Stone) continue;
        if (!scoringPlayedIdx.contains(k)) scoringPlayedIdx.append(k);
        result.scoringCards.append(played[k]);
    }
    std::sort(scoringPlayedIdx.begin(), scoringPlayedIdx.end());

    // DNA 属于原版 context.before：本盲注第一次且只出 1 张时，
    // 先把复制牌永久加入手牌最右侧，再进入本手的计分流程。
    // 这样如果复制出来的是钢铁牌，它会作为“留在手牌中的牌”参与本次计分。
    if (mDNAEligibleThisPlay) {
        if (played.size() == 1 && result.scoringCards.size() == 1) {
            const int dnaCopies = countResolvedJokersOfType(mJokers, JokerType::DNA);
            for (int i = 0; i < dnaCopies; ++i) {
                CardData copy = played.first();
                copy.assignNewUid();
                copy.faceUp = true;
                copy.isDebuffed = false;
                mHand.append(copy);
                ++mDNACopiesCreatedThisPlay;
            }
        }
        // 后面的 OnPlayedHand 小丑循环仍会经过 DNA 本体；提前关掉资格，
        // 避免同一张 DNA 在 joker loop 里再复制一次。
        mDNAEligibleThisPlay = false;
    }

    QVector<bool> shattered(played.size(), false);

    const int sockRetriggers = countResolvedJokersOfType(mJokers, JokerType::SockAndBuskin);
    const int chadRetriggers = 2 * countResolvedJokersOfType(mJokers, JokerType::HangingChad);
    const int hikerTriggers  = countResolvedJokersOfType(mJokers, JokerType::Hiker);
    const int midasTriggers  = countResolvedJokersOfType(mJokers, JokerType::MidasMask);
    const int vampireTriggers = countResolvedJokersOfType(mJokers, JokerType::Vampire);
    const int hackTriggers   = countResolvedJokersOfType(mJokers, JokerType::Hack);
    const int duskTriggers   = (mHandsLeft == 1)
                               ? countResolvedJokersOfType(mJokers, JokerType::Dusk) : 0;
    int seltzerTriggers = 0;
    for (const Joker &j : mJokers)
        if (!j.isDebuffed && j.type == JokerType::Seltzer && j.counter > 0) ++seltzerTriggers;

    bool firstScoringCard = true;
    bool faceCardScoredYet = false;   // 照片：标记本手是否已出现过计分人头牌
    for (int playedIdx : scoringPlayedIdx) {
        CardData card = played[playedIdx];
        if (card.isDebuffed) { firstScoringCard = false; continue; }

        const int redSealReps = (card.seal == Seal::Red) ? 1 : 0;
        int triggers = 1 + redSealReps;
        const bool hasRankAndSuit = card.enhancement != Enhancement::Stone;
        const bool isFace = hasRankAndSuit && isFaceCard(card);   // 含幻想性错觉
        const bool firstFaceCard = isFace && !faceCardScoredYet;
        if (isFace) triggers += sockRetriggers;
        if (firstScoringCard) triggers += chadRetriggers;
        // Batch 3：黑客——重新触发 2/3/4/5
        if (hasRankAndSuit) {
            int rv = static_cast<int>(card.rank);
            if (rv >= 2 && rv <= 5) triggers += hackTriggers;
        }
        // Batch 7：黄昏（最后一手）/ 苏打水 重新触发所有计分牌
        triggers += duskTriggers + seltzerTriggers;

        int globalIdx = (playedIdx >= 0 && playedIdx < sorted.size()) ? sorted[playedIdx] : -1;

        if (hikerTriggers > 0 && globalIdx >= 0 && globalIdx < mHand.size()) {
            // 原版 Hiker：每张计分牌本体永久 +5 筹码；蓝图/头脑风暴复制时额外叠加。
            mHand[globalIdx].permanentBonusChips += 5 * hikerTriggers;
            card.permanentBonusChips = mHand[globalIdx].permanentBonusChips;
        }

        // Batch 2：小小小丑——每张计分的 2 给自身永久 +8 筹码
        if (hasRankAndSuit && card.rank == Rank::Two) {
            for (Joker &wj : mJokers)
                if (!wj.isDebuffed && wj.type == JokerType::WeeJoker) wj.counter += 8 * triggers;
        }

        for (int t = 0; t < triggers; ++t) {
            if (t > 0 && t <= redSealReps) {
                // 原版红色蜡封在真正重复计算前先显示“再触发”。
                // Sock/Chad 等小丑带来的重复不伪装成红蜡封，只保留各自的计分事件。
                result.events.append({ ScoreEventKind::RedSealRetrigger, playedIdx, -1, -1, 0, 1.0 });
            }
            scoreCard(card, result, playedIdx, firstFaceCard);
        }
        if (isFace) faceCardScoredYet = true;

        if (midasTriggers > 0 && isFace && globalIdx >= 0 && globalIdx < mHand.size()) {
            mHand[globalIdx].enhancement = Enhancement::Gold;
        }

        if (vampireTriggers > 0 && globalIdx >= 0 && globalIdx < mHand.size()
            && mHand[globalIdx].enhancement != Enhancement::None) {
            mHand[globalIdx].enhancement = Enhancement::None;
            for (Joker &vj : mJokers) {
                if (!vj.isDebuffed && vj.type == JokerType::Vampire) vj.counter += vampireTriggers;
            }
        }

        if (card.enhancement == Enhancement::Glass
            && QRandomGenerator::global()->bounded(probDenom(4)) == 0) {
            shattered[playedIdx] = true;
            // 破碎发生在这张玻璃牌完成所有计分/重触发之后，和原版 shatter 队列一致。
            result.events.append({ ScoreEventKind::GlassShatter, playedIdx, -1, -1, 0, 1.0 });
            // Batch 2：玻璃小丑——每张玻璃牌破碎 +X0.75 倍率
            for (Joker &gj : mJokers)
                if (!gj.isDebuffed && gj.type == JokerType::GlassJoker) gj.counter += 1;
        }
        firstScoringCard = false;
    }

    QSet<int> playedHandIdx;
    for (int p : sorted) playedHandIdx.insert(p);
    QVector<CardData> heldHand;
    QVector<int> heldGlobalIdx;
    for (int i = 0; i < mHand.size(); ++i) {
        if (playedHandIdx.contains(i)) continue;
        heldHand.append(mHand[i]);
        heldGlobalIdx.append(i);
    }

    const int mimeRetriggers = countResolvedJokersOfType(mJokers, JokerType::Mime);
    const int baronTriggers = countResolvedJokersOfType(mJokers, JokerType::Baron);
    const int shootMoonTriggers = countResolvedJokersOfType(mJokers, JokerType::ShootTheMoon);
    const int reservedParkingTriggers = countResolvedJokersOfType(mJokers, JokerType::ReservedParking);
    for (int hi = 0; hi < heldHand.size(); ++hi) {
        const CardData &c = heldHand[hi];
        int heldVisualIdx = hi; // UI 中 mHandCards 已经移除了打出的牌，heldHand 的顺序就是手牌显示顺序
        if (c.isDebuffed) continue;

        // Batch 3：预留车位——手牌中每张人头牌 1/2 概率 +$1
        if (reservedParkingTriggers > 0 && isFaceCard(c)) {
            for (int rp = 0; rp < reservedParkingTriggers; ++rp)
                if (QRandomGenerator::global()->bounded(probDenom(2)) == 0) addGold(1);
        }

        // 原版手牌阶段先计算这张手牌是否真的有效果，然后红蜡封和 Mime 才能追加 repetition。
        // 因此红钢 K + 男爵 + 蓝图/头脑风暴/哑剧时，总次数 = 1 + 红蜡封次数 + 解析后的 Mime 次数；
        // 每一次 repetition 都重新执行钢铁、男爵、Shoot the Moon 等全部 hand-card 效果。
        const bool hasHeldEffect = (c.enhancement == Enhancement::Steel)
                                || (c.rank == Rank::King && baronTriggers > 0)
                                || (c.rank == Rank::Queen && shootMoonTriggers > 0);
        const int redSealReps = (hasHeldEffect && c.seal == Seal::Red) ? 1 : 0;
        int retriggers = 1 + redSealReps + (hasHeldEffect ? mimeRetriggers : 0);

        for (int r = 0; r < retriggers; ++r) {
            if (r > 0 && r <= redSealReps)
                result.events.append({ ScoreEventKind::RedSealRetrigger, -1, heldVisualIdx, -1, 0, 1.0 });

            if (c.enhancement == Enhancement::Steel) {
                result.xmult *= 1.5;
                result.events.append({ ScoreEventKind::SteelXMult, -1, heldVisualIdx, -1, 0, 1.5 });
            }
            if (c.rank == Rank::King) {
                for (int b = 0; b < baronTriggers; ++b) {
                    result.xmult *= 1.5;
                    result.events.append({ ScoreEventKind::SteelXMult, -1, heldVisualIdx, -1, 0, 1.5 });
                }
            }
            if (c.rank == Rank::Queen) {
                for (int sm = 0; sm < shootMoonTriggers; ++sm) {
                    result.mult += 13;
                    result.events.append({ ScoreEventKind::EnhancementMult, -1, heldVisualIdx, -1, 13, 1.0 });
                }
            }
        }
    }

    // Batch 2：在小丑计分前更新「打出手牌」型计数器（对应原版 context.before）
    {
        bool faceScored = false;
        for (const CardData &c : result.scoringCards)
            if (!c.isDebuffed && isFaceCard(c)) { faceScored = true; break; }
        int mostPlayed = 0;
        for (auto it = mHandLevels.constBegin(); it != mHandLevels.constEnd(); ++it)
            mostPlayed = qMax(mostPlayed, it.value().played);
        const bool isMostPlayed = (mostPlayed > 0 && mHandLevels[result.type].played >= mostPlayed);
        const HandType t = result.type;
        const bool isStraight = (t == HandType::Straight || t == HandType::StraightFlush
                                 || t == HandType::RoyalFlush);
        const bool hasTwoPair = (t == HandType::TwoPair || t == HandType::FullHouse
                                 || t == HandType::FlushHouse);
        for (Joker &j : mJokers) {
            if (j.isDebuffed) continue;
            switch (j.type) {
            case JokerType::SquareJoker:   if (played.size() == 4) j.counter += 4;  break;
            case JokerType::Runner:        if (isStraight) j.counter += 15;          break;
            case JokerType::GreenJoker:    j.counter += 1;                           break;
            case JokerType::SpareTrousers: if (hasTwoPair) j.counter += 2;           break;
            case JokerType::RideTheBus:    j.counter = faceScored ? 0 : j.counter + 1; break;
            case JokerType::Obelisk:       j.counter = isMostPlayed ? 0 : j.counter + 1; break;
            case JokerType::LoyaltyCard:   j.counter += 1; break;
            default: break;
            }
        }
    }

    {
        double chipsBefore = result.chips, multBefore = result.mult;
        double xmultBefore = result.xmult;

        TriggerContext ctx{ result, *this, heldHand, result.scoringCards, nullptr };
        ctx.playedCards = &played;
        for (int ji = 0; ji < mJokers.size(); ++ji) {
            const Joker &j = mJokers[ji];
            if (j.isDebuffed) continue;

            const Joker *effectJoker = resolveCopiedJoker(mJokers, ji);

            if (effectJoker && !effectJoker->isDebuffed &&
                (effectJoker->timing == TriggerTiming::Passive ||
                 effectJoker->timing == TriggerTiming::OnPlayedHand)) {
                ctx.self = &mJokers[ji];
                applyResolvedJokerEffect(*effectJoker, ctx);
            }

            switch (j.edition) {
            case Edition::Foil:        result.chips += 50; break;
            case Edition::Holographic: result.mult  += 10; break;
            case Edition::Polychrome:  result.xmult *= 1.5; break;
            default: break;
            }

            if (result.chips != chipsBefore) {
                result.events.append({ ScoreEventKind::JokerChip, -1, -1, ji,
                                      result.chips - chipsBefore, 1.0 });
                chipsBefore = result.chips;
            }
            if (result.mult != multBefore) {
                result.events.append({ ScoreEventKind::JokerMult, -1, -1, ji,
                                      result.mult - multBefore, 1.0 });
                multBefore = result.mult;
            }
            if (qAbs(result.xmult - xmultBefore) > 1e-6) {
                result.events.append({ ScoreEventKind::JokerXMult, -1, -1, ji,
                                      0, result.xmult / xmultBefore });
                xmultBefore = result.xmult;
            }
        }
    }

    // 只缓存本手最终分。真正加到回合总分，要等 UI 播完逐张牌/小丑动画之后。
    if (hasVoucher(VoucherType::Observatory)) {
        for (const Consumable &c : mConsumables) {
            if (c.kind == ConsumableKind::Planet && planetMatchesHand(c.type, result.type)) {
                result.xmult *= 1.5;
                result.events.append({ ScoreEventKind::JokerXMult, -1, -1, -1, 0, 1.5 });
            }
        }
    }

    mPendingHandScore = result.chips * result.mult * result.xmult;
    if (!std::isfinite(mPendingHandScore)) mPendingHandScore = std::numeric_limits<double>::infinity();
    mPendingPlayedIndices = sorted;
    mPendingShattered = shattered;
    mAwaitingScoreFinalize = true;
    mLastResult = result;
    emit handPlayed();
}

void GameState::finalizePlayedHand()
{
    if (!mAwaitingScoreFinalize) return;
    mAwaitingScoreFinalize = false;

    // 绯红之心(Crimson Heart)：恢复本手被禁用的小丑。
    bool crimsonRestored = false;
    if (mCrimsonHeartDisabled >= 0 && mCrimsonHeartDisabled < mJokers.size()) {
        mJokers[mCrimsonHeartDisabled].isDebuffed = false;
        crimsonRestored = true;
    }
    mCrimsonHeartDisabled = -1;

    const int playedCount = mPendingPlayedIndices.size();

    const HandResult result = mLastResult;
    mScore += mPendingHandScore;
    mHandsLeft--;

    // 公牛(The Ox)：打出当前最常用牌型时，金币归零。
    if (mBossEffect == BossEffect::TheOx && !hasJokerType(JokerType::Chicot)) {
        int mostPlayed = 0;
        for (auto it = mHandLevels.constBegin(); it != mHandLevels.constEnd(); ++it)
            mostPlayed = qMax(mostPlayed, it.value().played);
        if (mostPlayed > 0 && mHandLevels[mLastResult.type].played >= mostPlayed)
            mGold = 0;
    }
    mHandLevels[mLastResult.type].played++;
    mHandTypesPlayedThisRound.insert(static_cast<int>(mLastResult.type));   // 锋利卡牌
    // Handy Tag 用：累加本局打出的总手数（不区分牌型，跨回合累计）。
    mTotalHandsPlayedThisRun++;
    // Batch 7：苏打水每出一手 -1，耗尽后销毁
    for (Joker &j : mJokers)
        if (j.type == JokerType::Seltzer) j.counter -= 1;
    cleanupDepletedJokers();

    QVector<QPair<int,int>> idxPairs;
    for (int pidx = 0; pidx < mPendingPlayedIndices.size(); ++pidx)
        idxPairs.append({ mPendingPlayedIndices[pidx], pidx });
    std::sort(idxPairs.begin(), idxPairs.end(),
              [](const auto &a, const auto &b){ return a.first > b.first; });

    for (const auto &pr : idxPairs) {
        if (pr.first < 0 || pr.first >= mHand.size()) continue;
        bool broken = pr.second >= 0 && pr.second < mPendingShattered.size()
                      && mPendingShattered[pr.second];
        if (broken) {
            notifyPlayingCardDestroyed(mHand[pr.first]);
        } else {
            mDeck.discard(mHand[pr.first]);
        }
        mHand.removeAt(pr.first);
    }

    // DNA 复制牌已经在 playCards() 的 context.before 阶段加入 mHand，
    // 这里仅清理旧路径的临时缓存，避免遗留状态影响下一手。
    mPendingDNACopies.clear();

    mPendingPlayedIndices.clear();
    mPendingShattered.clear();
    mPendingHandScore = 0;

    if (mDNACopiesCreatedThisPlay > 0) {
        mDNAUsedThisBlind = true;
        mDNAEligibleThisPlay = false;
        mDNACopiesCreatedThisPlay = 0;
    }

    decayEndOfHandJokers();

    // 牙齿(The Tooth)：每打出 1 张牌失去 $1。
    if (mBossEffect == BossEffect::TheTooth && !hasJokerType(JokerType::Chicot))
        mGold = qMax(0, mGold - playedCount);
    // 房屋(The House)：第一手出完后，剩余手牌全部翻回正面。
    if (mBossEffect == BossEffect::TheHouse)
        for (CardData &c : mHand) c.faceUp = true;

    applyBossPostPlay();
    applyBossDebuffs();

    emit scoreChanged();
    emit goldChanged();
    if (crimsonRestored) emit jokersChanged();   // 恢复绯红之心禁用的小丑后刷新显示

    if (mScore >= mTargetScore) {
        finishWinningRound();
        return;
    }

    if (mHandsLeft <= 0) {
        checkGameOver();
        return;
    }

    // 只有未过关且还能继续出牌时，才在计分动画和收牌动画结束后补新牌。
    dealCards(DrawContext::AfterPlay);
    applyBossDebuffs();
    emit handChanged();
}

void GameState::finishWinningRound()
{
    const HandResult result = mLastResult;
    const int goldBeforeCashout = mGold;
    mPendingRoundPayout = 0;
    mSuppressGoldSignal = true;

    // 原版 end_of_round 阶段也走“先看这张手牌有没有效果，再由红蜡封/Mime 追加 repetition”。
    // 这里使用解析后的 Mime 数量，Blueprint / Brainstorm 指向 Mime 时也会生效。
    // UI 事件只负责动画提示；金币先进入待提现缓存，点击“提现”后再真正加到账户。
    const int mimeRetriggers = countResolvedJokersOfType(mJokers, JokerType::Mime);
    QVector<ScoreEvent> endRoundEvents;

    for (int i = 0; i < mHand.size(); ++i) {
        const CardData &c = mHand[i];
        if (c.isDebuffed) continue;
        const bool hasEndEffect = (c.enhancement == Enhancement::Gold) || (c.seal == Seal::Blue);
        if (!hasEndEffect) continue;

        const int redSealReps = (c.seal == Seal::Red) ? 1 : 0;
        const int reps = 1 + mimeRetriggers + redSealReps;
        for (int r = 0; r < reps; ++r) {
            if (r > 0 && r <= redSealReps)
                endRoundEvents.append({ ScoreEventKind::RedSealRetrigger, -1, i, -1, 0, 1.0 });

            if (c.enhancement == Enhancement::Gold) {
                endRoundEvents.append({ ScoreEventKind::DollarGain, -1, i, -1, 3, 1.0 });
            }
            if (c.seal == Seal::Blue && canAddConsumable()) {
                addConsumable(planetTypeFor(result.type));
                endRoundEvents.append({ ScoreEventKind::BlueSealPlanet, -1, i, -1, 0, 1.0 });
            }
        }
    }

    int endRoundGoldGain = 0;
    for (const ScoreEvent &ev : endRoundEvents)
        if (ev.kind == ScoreEventKind::DollarGain) endRoundGoldGain += int(std::round(ev.intValue));
    if (endRoundGoldGain > 0) mGold += endRoundGoldGain;

    if (!endRoundEvents.isEmpty())
        emit endRoundCardTriggered(endRoundEvents);

    int blindReward = 0;
    switch (mBlindType) {
    case BlindType::Small: blindReward = 3; break;
    case BlindType::Big:   blindReward = 4; break;
    case BlindType::Boss:  blindReward = 5; break;
    }
    // Garbage Tag 用：累加本局未用过的弃牌数（含本回合剩余）。
    mUnusedDiscardsThisRun += qMax(0, mDiscardLeft);
    int handBonus = mHandsLeft * Constants::HAND_GOLD;
    mGold += blindReward + handBonus;
    if (mBlindType == BlindType::Boss && mPendingInvestmentBonus > 0) {
        mGold += mPendingInvestmentBonus;
        mPendingInvestmentBonus = 0;
    }
    mOneRoundHandSizeBonus = 0;

    HandResult dummy{};
    TriggerContext rctx{ dummy, *this, mHand, mHand, nullptr };
    for (const Joker &j : mJokers) {
        if (!j.isDebuffed && j.timing == TriggerTiming::OnRoundEnd)
            j.effect(rctx);
    }

    // Batch 2：爆米花每回合结束 -4 倍率
    for (Joker &j : mJokers)
        if (j.type == JokerType::Popcorn) j.counter = qMax(0, j.counter - 4);

    // Batch 5：回合结算经济型小丑
    {
        const bool bossDefeated = (mBlindType == BlindType::Boss);
        for (Joker &j : mJokers) {
            if (j.isDebuffed) continue;
            switch (j.type) {
            case JokerType::Egg:       j.extraSellValue += 3; break;
            case JokerType::Rocket:
                addGold(qMax(0, j.counter));
                if (bossDefeated) j.counter += 2;
                break;
            case JokerType::Satellite:
                addGold(static_cast<int>(mPlanetsUsedThisRun.size()));
                break;
            case JokerType::Campfire:
                if (bossDefeated) j.counter = 0;
                break;
            case JokerType::TurtleBean:
                j.counter -= 1;
                break;
            case JokerType::InvisibleJoker:
                j.counter += 1;
                break;
            default: break;
            }
        }
        int gifts = 0;
        for (const Joker &j : mJokers)
            if (!j.isDebuffed && j.type == JokerType::GiftCard) ++gifts;
        if (gifts > 0) {
            for (Joker &j : mJokers) j.extraSellValue += gifts;
            for (Consumable &c : mConsumables) c.sellValue += gifts;
        }
        updateOwnedSellValues();
    }

    processEndOfRoundJokerExtinctions();
    cleanupDepletedJokers();   // Batch 7：移除爆米花/海龟豆等耗尽的小丑

    int interest = qMin(qMax(0, mGold) / 5, mInterestCap / 5);
    // Batch 3：飞向月球——每 $5 额外 $1 利息
    if (hasJokerType(JokerType::ToTheMoon)) interest += mGold / 5;
    mGold += interest;

    // 结算窗口弹出前不直接改变左侧金币。
    // 上面仍按原有顺序临时计算所有回合末金币变化，保证利息、金牌、
    // 投资标签/小丑等逻辑和原流程一致；随后把差额存入待提现缓存。
    mPendingRoundPayout = mGold - goldBeforeCashout;
    mGold = goldBeforeCashout;
    mSuppressGoldSignal = false;

    mPhase = GamePhase::Shop;
    // 演示模式：进商店 → shopVisit++（在 mShop.roll 调用之前，让 Shop hook 读到正确编号）
    if (DemoScript::active()) DemoScript::onEnterShop();
    mChaosFreeRerollUsed = false;   // Batch 7：进商店重置混沌小丑免费重摇
    syncShopJokerRules();
    // 原版：每个 Ante 只有击败 Boss 后的商店出现优惠券(基础 1 张)。
    // Voucher Tag(含 Double Tag 复制)的语义是 "在下个商店额外多出 1 张优惠券",
    // 所以 Boss 商店遇到 N 张缓存 Voucher Tag 应出 1+N 张;非 Boss 商店则出 N 张。
    const int extraVouchersFromTag = mTagVoucherPendingShops;
    mTagVoucherPendingShops = 0;
    mTagVoucherNextShop = false;
    const bool refreshAnteVoucher = (mVoucherRolledAnte != mAnte);
    mShop.setAllowVoucherThisShop(refreshAnteVoucher);

    // Coupon Tag：每张缓存对应一次"下个商店初始价免费"。
    if (mTagCouponPendingShops > 0) {
        mShop.setNextShopFree(true);
        --mTagCouponPendingShops;
    } else {
        mShop.setNextShopFree(false);
    }

    // D6 Tag：每张缓存对应一次"下个商店重摇折扣"。
    if (mTagD6PendingShops > 0) {
        mShop.setRerollDiscount(5);
        --mTagD6PendingShops;
    } else {
        mShop.setRerollDiscount(0);
    }

    mShop.roll();
    if (refreshAnteVoucher) mVoucherRolledAnte = mAnte;
    for (int i = 0; i < extraVouchersFromTag; ++i) mShop.appendVoucherOffer();
    if (mFirstShop) {
        auto &b = mShop.boosterOffersMutable();
        if (b.size() >= 1) {
            mShop.setBoosterOfferPack(0, PackKind::Buffoon, PackSize::Normal);
            // 保留 Coupon Tag 触发的 cost=0;否则按原版固定 $4 小丑包。
        }
        // 第一商店硬塞 Buffoon Normal 后,如果旁边那张随机礼包恰好也是 Buffoon Normal,
        // 就出现两张同封面——按原版规则避免重复封面。
        if (b.size() >= 2 && b[0].kind == OfferKind::Pack && b[1].kind == OfferKind::Pack &&
            b[0].pack == b[1].pack && b[0].packSize == b[1].packSize &&
            b[0].packVariant == b[1].packVariant) {
            // 用一个不同尺寸的同类型,或换成 Arcana Normal 作为兜底——保持卡包性质,但封面不同。
            ShopOffer &o2 = b[1];
            o2.pack = PackKind::Arcana;
            o2.packSize = PackSize::Normal;
            // cost 维持原本随机礼包的价格(Arcana Normal = $4)。
            const bool keepFree = (o2.cost == 0);
            o2.cost = keepFree ? 0 : 4;
        }
        mFirstShop = false;
    }
    applyTagEffectsToShop();
    emit goldChanged();
    emit roundWon(blindReward, handBonus, interest);
}

bool GameState::claimRoundPayout()
{
    if (mPendingRoundPayout == 0) return false;
    mGold += mPendingRoundPayout;
    mPendingRoundPayout = 0;
    emit goldChanged();
    return true;
}


void GameState::discardCards(const QVector<int> &indices)
{
    if (indices.isEmpty()) return;
    if (mDiscardLeft <= 0) return;

    QVector<CardData> discarded;
    for (int i : indices) discarded.append(mHand[i]);

    notifyDiscardedCardsForYorick(discarded.size());

    // OnDiscard 小丑触发（当前没有这种类型的小丑,留着挂钩用）
    HandResult dummy{};
    TriggerContext ctx{ dummy, *this, mHand, discarded, nullptr };
    for (const Joker &j : mJokers) {
        if (j.isDebuffed) continue;
        if (j.timing == TriggerTiming::OnDiscard) j.effect(ctx);
    }

    // Batch 2：弃牌型计数器小丑
    for (Joker &j : mJokers) {
        if (j.isDebuffed) continue;
        switch (j.type) {
        case JokerType::GreenJoker:
            j.counter = qMax(0, j.counter - 1);
            break;
        case JokerType::HitTheRoad:
            for (const CardData &c : discarded)
                if (c.enhancement != Enhancement::Stone && c.rank == Rank::Jack) j.counter += 1;
            break;
        case JokerType::Castle:
            for (const CardData &c : discarded)
                if (c.enhancement != Enhancement::Stone && c.suit == mCastleSuit) j.counter += 3;
            break;
        case JokerType::Ramen:
            j.counter = qMax(100, j.counter - discarded.size());   // ×倍率最低降到 ×1.00
            break;
        default: break;
        }
    }

    // Batch 7：焦痕小丑——每回合第一次弃牌升级该牌型
    const int burntCopies = countResolvedJokersOfType(mJokers, JokerType::BurntJoker);
    if (mFirstDiscardThisRound && burntCopies > 0 && !discarded.isEmpty()) {
        HandResult dr = HandEvaluator::evaluate(discarded, currentHandMods());
        levelUpHand(dr.type, burntCopies);
    }
    mFirstDiscardThisRound = false;
    cleanupDepletedJokers();   // 拉面降到 ×1.0 时销毁

    // Purple Seal：被弃的每张 → 生成随机塔罗
    for (const CardData &c : discarded) {
        if (c.seal == Seal::Purple && !c.isDebuffed && canAddConsumable())
            addConsumable(randomTarotType());
    }

    QVector<int> sorted = indices;
    std::sort(sorted.begin(), sorted.end(), std::greater<int>());
    for (int i : sorted) {
        mHand[i].faceUp = true;   // 背面朝下的牌被弃掉时翻回正面
        mDeck.discard(mHand[i]);
        mHand.removeAt(i);
    }

    mDiscardLeft--;
    dealCards(DrawContext::AfterDiscard);
    applyBossDebuffs();

    emit handChanged();
}

double GameState::calcTargetScore() const {
    const int baseScores[] = { 0, 300, 800, 2000, 5000, 11000, 20000, 35000, 50000 };
    double base;
    if (mAnte <= 8) {
        base = double(baseScores[qBound(0, mAnte, 8)]);
    } else {
        // 无尽模式 ante 9+ 的指数级膨胀，沿用原版 get_blind_amount 公式。
        const double k = 0.75, A = 50000.0, B = 1.6;
        const double C = mAnte - 8;
        const double D = 1.0 + 0.2 * (mAnte - 8);
        double amt = std::floor(A * std::pow(B + std::pow(k * C, D), C));
        if (std::isfinite(amt) && amt > 0.0) {
            double mag = std::pow(10.0, std::floor(std::log10(amt)) - 1.0);
            if (mag >= 1.0) amt -= std::fmod(amt, mag);   // 取整到有效数字
        }
        base = std::isfinite(amt) ? amt : 1e300;
    }
    double mult = 1.0;
    switch (mBlindType) {
    case BlindType::Small: mult = Constants::SMALL_BLIND_MULT; break;
    case BlindType::Big:   mult = Constants::BIG_BLIND_MULT;   break;
    case BlindType::Boss:  mult = Constants::BOSS_BLIND_MULT;  break;
    }
    double target = base * mult;
    if (!hasJokerType(JokerType::Chicot)) {
        if (mBossEffect == BossEffect::TheWall)         target *= 2.0;  // 围墙 mult 4 = 2×2
        if (mBossEffect == BossEffect::VioletVessel)    target *= 3.0;  // 紫罗兰之器 mult 6 = 2×3
        if (mBossEffect == BossEffect::TheNeedle)       target *= 0.5;  // 针 mult 1 = 2×0.5（每回合只 1 手，分数线减半）
    }
    return target;
}

int GameState::jokerSlots() const {
    int extra = mExtraJokerSlots;
    for (const Joker &j : mJokers)
        if (j.edition == Edition::Negative) extra++;
    return Constants::MAX_JOKER_SLOTS + extra;
}

bool GameState::canAddJokerWithEdition(Edition edition) const {
    // 原版负片小丑自身会给 +1 joker slot，因此即使当前显示为 5/5，
    // 购买负片小丑后也会立即变成 6/6，不应该被满槽判断拦住。
    if (edition == Edition::Negative) return true;
    return canAddJoker();
}

void GameState::checkGameOver() {
    if (mPhase != GamePhase::Blind) return;       // ← 新增：不在打盲注阶段就不判断
    if (mHandsLeft <= 0 && mScore < mTargetScore) {
        // 骨头先生：得分达到要求的 25% 时免于失败，本牌随后消失。
        if (hasJokerType(JokerType::MrBones) && mScore * 4.0 >= mTargetScore) {
            for (int i = 0; i < mJokers.size(); ++i)
                if (mJokers[i].type == JokerType::MrBones) { mJokers.removeAt(i); break; }
            syncShopJokerRules();
            emit jokersChanged();
            finishWinningRound();
            return;
        }
        mPhase = GamePhase::GameOver;
        emit gameOver(false);
    }
}

int GameState::roundReward() const {
    int base = 0;
    switch (mBlindType) {
    case BlindType::Small: base = 3; break;
    case BlindType::Big: base = 4; break;
    case BlindType::Boss: base = 5; break;
    }
    int handBonus = mHandsLeft * Constants::HAND_GOLD;
    int interest = qMin(qMax(0, mGold) / 5, mInterestCap / 5);
    if (hasJokerType(JokerType::ToTheMoon)) interest += mGold / 5;
    return base + handBonus + interest;
}

void GameState::rerollShop() {
    if (mPhase != GamePhase::Shop) return;
    int cost = mShop.rerollCost();
    // Batch 7：混沌小丑——每次进商店首次重摇免费
    const bool freeReroll = (!mChaosFreeRerollUsed && hasJokerType(JokerType::ChaosTheClown));
    if (freeReroll) cost = 0;
    if (spendableGold() < cost) return;
    if (freeReroll) mChaosFreeRerollUsed = true;

    mGold -= cost;
    syncShopJokerRules();
    // 演示模式：rerollCount++（在 rerollShopOnly 之前，让 Shop hook 看到正确的重摇次数）
    if (DemoScript::active()) DemoScript::onShopReroll();
    mShop.onReroll();
    mShop.rerollShopOnly();   // ← 只 reroll 商品区,不动 booster

    // Batch 5：闪卡——每次重摇 +2 倍率
    for (Joker &j : mJokers)
        if (!j.isDebuffed && j.type == JokerType::FlashCard) j.counter += 1;

    emit goldChanged();
    emit shopChanged();
}

void GameState::applyBossDebuffs() {
    bool bossDisabled = hasJokerType(JokerType::Chicot);
    for (CardData &c : mHand) {
        bool d = !bossDisabled && bossDebuffsCard(mBossEffect, c);
        // 支柱(The Pillar)：本 Ante 之前打出过的牌被禁用。
        if (!bossDisabled && mBossEffect == BossEffect::ThePillar
            && mCardsPlayedThisAnte.contains(c.uid))
            d = true;
        // 翠绿之叶(Verdant Leaf)：卖出 1 张小丑前，所有牌都被禁用。
        if (!bossDisabled && mBossEffect == BossEffect::VerdantLeaf && mVerdantLeafActive)
            d = true;
        c.isDebuffed = d;
    }
}

void GameState::applyBossPostPlay() {
    if (!hasJokerType(JokerType::Chicot) && mBossEffect == BossEffect::TheHook && mHand.size() >= 2) {
        // 随机弃 2 张
        for (int i = 0; i < 2 && !mHand.isEmpty(); ++i) {
            int idx = QRandomGenerator::global()->bounded(mHand.size());
            mDeck.discard(mHand[idx]);
            mHand.removeAt(idx);
        }
    }
}

bool GameState::addConsumable(ConsumableType t) {
    if (mDryRun) return false;          // 出牌提示模拟期间不得真正造牌
    if (!canAddConsumable()) return false;
    mConsumables.append(createConsumable(t));
    emit consumablesChanged();
    return true;
}

bool GameState::addFoolCopyConsumable() {
    // 原版 The Fool：只复制上一张使用过的塔罗/星球牌；没有记录、上一张是愚者、或上一张是幻灵牌时不可使用。
    if (!mHasLastUsedConsumable || !canRecordForFool(mLastUsedConsumable)) return false;
    if (!canAddConsumable()) return false;
    mConsumables.append(createConsumable(mLastUsedConsumable));
    emit consumablesChanged();
    return true;
}

bool GameState::canUseFool() const
{
    return mHasLastUsedConsumable && canRecordForFool(mLastUsedConsumable);
}

bool GameState::useConsumable(int idx, const QVector<int> &selectedHandIdx) {
    if (mPhase == GamePhase::GameOver) return false;
    if (idx < 0 || idx >= mConsumables.size()) return false;

    Consumable c = mConsumables[idx];
    if (c.needsSelection > 0 &&
        selectedHandIdx.size() < c.needsSelection) return false;

    QVector<int> sel = selectedHandIdx;
    std::sort(sel.begin(), sel.end());
    sel.erase(std::unique(sel.begin(), sel.end()), sel.end());
    if (c.maxSelection > 0 && sel.size() > c.maxSelection)
        return false;

    if (c.type == ConsumableType::Tarot_Fool && !mHasLastUsedConsumable) return false;
    if (c.type == ConsumableType::Tarot_Fool && !canRecordForFool(mLastUsedConsumable)) return false;
    if (consumableNeedsFreeJokerSlot(c.type) && !canAddJoker()) return false;

    // 先把被使用的消耗牌移出槽位，再执行效果。否则皇帝/女祭司在 2 槽已占 1 槽时只会生成 1 张。
    mConsumables.removeAt(idx);

    UseContext ctx{ *this, sel };
    c.effect(ctx);

    // Batch 5：算命师统计塔罗使用、卫星统计行星种类
    ConsumableKind usedKind = kindOf(c.type);
    if (usedKind == ConsumableKind::Tarot) {
        for (Joker &j : mJokers)
            if (!j.isDebuffed && j.type == JokerType::FortuneTeller) j.counter += 1;
    } else if (usedKind == ConsumableKind::Planet) {
        mPlanetsUsedThisRun.insert(static_cast<int>(c.type));
    } else if (c.type == ConsumableType::Spectral_BlackHole) {
        mPlanetsUsedThisRun.insert(static_cast<int>(c.type));
        for (Joker &j : mJokers)
            if (!j.isDebuffed && j.type == JokerType::Constellation) j.counter += 1;
    }

    if (canRecordForFool(c.type)) {
        mLastUsedConsumable = c.type;
        mHasLastUsedConsumable = true;
    }

    emit consumablesChanged();
    return true;
}

bool GameState::sellConsumable(int idx) {
    if (idx < 0 || idx >= mConsumables.size()) return false;
    int v = mConsumables[idx].sellValue;
    mConsumables.removeAt(idx);
    mGold += v;
    // Batch 5：篝火——每卖出 1 张牌 +X0.25
    for (Joker &j : mJokers)
        if (!j.isDebuffed && j.type == JokerType::Campfire) j.counter += 1;
    emit consumablesChanged();
    emit goldChanged();
    return true;
}

bool GameState::sellJoker(int idx) {
    if (idx < 0 || idx >= mJokers.size()) return false;
    const JokerType soldType = mJokers[idx].type;
    const int soldCounter = mJokers[idx].counter;
    int v = qMax(1, mJokers[idx].sellValue);
    mJokers.removeAt(idx);
    mGold += v;
    // Batch 5：篝火——每卖出 1 张牌 +X0.25
    for (Joker &j : mJokers)
        if (!j.isDebuffed && j.type == JokerType::Campfire) j.counter += 1;
    // Batch 8：摔跤手——出售时禁用当前 Boss
    if (soldType == JokerType::Luchador && mBossEffect != BossEffect::None) {
        mBossEffect = BossEffect::None;
        applyBossDebuffs();
        emit handChanged();
    }
    // Batch 8：隐形小丑——经过 2 回合后出售，复制 1 张随机小丑
    if (soldType == JokerType::InvisibleJoker && soldCounter >= 2
        && !mJokers.isEmpty() && canAddJoker()) {
        Joker copy = mJokers[QRandomGenerator::global()->bounded(mJokers.size())];
        mJokers.append(copy);
    }
    // Batch 9：无糖可乐——出售时获得 1 个双倍标签
    if (soldType == JokerType::DietCola)
        mActiveTags.append(TagType::Double);
    // 翠绿之叶(Verdant Leaf)：卖出任意小丑即解除全牌禁用。
    if (mVerdantLeafActive && mBossEffect == BossEffect::VerdantLeaf) {
        mVerdantLeafActive = false;
        applyBossDebuffs();
        emit handChanged();
    }
    // 绯红之心：被禁用小丑的下标可能因移除而失效。
    if (mCrimsonHeartDisabled >= mJokers.size()) mCrimsonHeartDisabled = -1;
    syncShopJokerRules();
    if (mPhase == GamePhase::Shop) {
        mShop.refreshCurrentOfferCosts();
        mShop.ensureShopOfferCount();
    }
    emit jokersChanged();
    emit goldChanged();
    emit shopChanged();
    return true;
}

bool GameState::moveJoker(int from, int to) {
    if (from < 0 || from >= mJokers.size() || to < 0 || to >= mJokers.size()) return false;
    if (from == to) return true;
    mJokers.move(from, to);
    emit jokersChanged();
    return true;
}

bool GameState::moveConsumable(int from, int to) {
    if (from < 0 || from >= mConsumables.size() || to < 0 || to >= mConsumables.size()) return false;
    if (from == to) return true;
    mConsumables.move(from, to);
    emit consumablesChanged();
    return true;
}

bool GameState::moveHandCard(int from, int to) {
    if (from < 0 || from >= mHand.size() || to < 0 || to >= mHand.size()) return false;
    if (from == to) return true;
    mHand.move(from, to);
    // 手动移动只影响当前手牌展示；之后补牌仍按玩家上次选择的“点数/花色”自动理牌。
    emit handChanged();
    return true;
}

bool GameState::moveShopOffer(int from, int to)
{
    if (mPhase != GamePhase::Shop) return false;
    if (!mShop.moveShopOffer(from, to)) return false;
    emit shopChanged();
    return true;
}

bool GameState::moveBoosterOffer(int from, int to)
{
    if (mPhase != GamePhase::Shop) return false;
    if (!mShop.moveBoosterOffer(from, to)) return false;
    emit shopChanged();
    return true;
}

void GameState::collectRoundCardsToDeck() {
    // RoundEnd → Shop 时调用：把最后留在手上的牌、以及出牌区已弃进弃牌堆的牌
    // 统一合回摸牌堆并洗牌，保证商店开包/下一盲注看到的是完整当前牌组。
    if (!mHand.isEmpty()) {
        mDeck.returnCards(mHand);
        mHand.clear();
    }
    mDeck.reset();
    emit handChanged();
}

bool GameState::buyShopOffer(int idx) {
    if (mPhase != GamePhase::Shop) return false;
    if (!mShop.canBuyShop(idx, spendableGold())) return false;
    const ShopOffer &o = mShop.shopOffers()[idx];

    if (o.kind == OfferKind::Joker) {
        // 负片小丑自带 +1 小丑槽，原版允许在槽位满时买入负片。
        // 不要用 canAddJoker() 单独判断，否则 5/5 时会把负片小丑误拦截。
        if (!canAddJokerWithEdition(o.jokerEdition)) return false;
        ShopOffer t = mShop.takeShopOffer(idx);
        mGold -= t.cost;
        Joker j = createJoker(t.joker);
        j.edition = t.jokerEdition;
        if (j.type == JokerType::Throwback)
            j.counter = mTotalSkipsThisRun;
        mJokers.append(j);
        updateOwnedSellValues();
        syncShopJokerRules();
        emit jokersChanged();
    } else if (o.kind == OfferKind::Tarot ||
               o.kind == OfferKind::Planet ||
               o.kind == OfferKind::Spectral) {
        if (!canAddConsumable()) return false;
        ShopOffer t = mShop.takeShopOffer(idx);
        mGold -= t.cost;
        mConsumables.append(createConsumable(t.consumable));
        emit consumablesChanged();
    } else if (o.kind == OfferKind::PlayingCard) {
        ShopOffer t = mShop.takeShopOffer(idx);
        mGold -= t.cost;
        mDeck.addCard(t.playingCard);
        for (Joker &j : mJokers) if (!j.isDebuffed && j.type == JokerType::Hologram) ++j.counter;
    } else {
        return false;
    }

    emit goldChanged();
    emit shopChanged();
    return true;
}

bool GameState::canBuyAndUseShopConsumable(int idx) const
{
    if (mPhase != GamePhase::Shop) return false;
    if (idx < 0 || idx >= mShop.shopOffers().size()) return false;
    const ShopOffer &o = mShop.shopOffers()[idx];
    if (o.sold) return false;
    if (o.kind != OfferKind::Tarot && o.kind != OfferKind::Planet && o.kind != OfferKind::Spectral)
        return false;
    if (spendableGold() < o.cost) return false;
    Consumable c = createConsumable(o.consumable);
    // 需要选牌的消耗品（如 Magician/Empress/Sun/Moon 等）在商店没有手牌可选，禁用直接使用。
    if (c.needsSelection > 0) return false;
    if (c.type == ConsumableType::Tarot_Fool && !canUseFool()) return false;
    if (consumableNeedsFreeJokerSlot(c.type) && !canAddJoker()) return false;
    return true;
}

bool GameState::buyAndUseShopConsumable(int idx, const QVector<int> &selectedHandIdx)
{
    if (mPhase != GamePhase::Shop) return false;
    if (idx < 0 || idx >= mShop.shopOffers().size()) return false;
    const ShopOffer &o = mShop.shopOffers()[idx];
    if (o.sold) return false;
    if (o.kind != OfferKind::Tarot && o.kind != OfferKind::Planet && o.kind != OfferKind::Spectral)
        return false;
    if (spendableGold() < o.cost) return false;

    // 先临时把消耗牌"塞进"消耗槽（即使槽满也能买并立即使用，原版 BUY AND USE 行为）。
    ShopOffer t = mShop.takeShopOffer(idx);
    mGold -= t.cost;
    mConsumables.append(createConsumable(t.consumable));
    int useIdx = mConsumables.size() - 1;

    emit goldChanged();
    emit shopChanged();

    if (!useConsumable(useIdx, selectedHandIdx)) {
        // 失败回滚：把刚加进去的牌移除、金币退回。
        if (useIdx >= 0 && useIdx < mConsumables.size())
            mConsumables.removeAt(useIdx);
        mGold += t.cost;
        emit consumablesChanged();
        emit goldChanged();
        return false;
    }
    return true;
}

bool GameState::buyVoucherOffer(int idx) {
    if (mPhase != GamePhase::Shop) return false;
    if (!mShop.canBuyVoucher(idx, spendableGold())) return false;

    ShopOffer t = mShop.takeVoucherOffer(idx);
    mGold -= t.cost;
    mRedeemedVouchers.append(t.voucher);
    mShop.setRedeemedVouchers(mRedeemedVouchers);
    applyVoucher(t.voucher);
    syncShopJokerRules();
    mShop.refreshCurrentOfferCosts();

    emit goldChanged();
    emit shopChanged();
    emit jokersChanged();
    emit consumablesChanged();
    return true;
}

bool GameState::hasVoucher(VoucherType t) const {
    return mRedeemedVouchers.contains(t);
}

bool GameState::telescopePlanetForPack(ConsumableType &out) const
{
    const QVector<HandType> order = {
        HandType::HighCard, HandType::Pair, HandType::TwoPair,
        HandType::ThreeOfAKind, HandType::Straight, HandType::Flush,
        HandType::FullHouse, HandType::FourOfAKind, HandType::StraightFlush,
        HandType::RoyalFlush, HandType::FiveOfAKind, HandType::FlushHouse,
        HandType::FlushFive,
    };

    bool found = false;
    HandType best = HandType::HighCard;
    int bestPlayed = 0;
    for (HandType t : order) {
        auto it = mHandLevels.constFind(t);
        const int played = (it == mHandLevels.constEnd()) ? 0 : it.value().played;
        if (!telescopeHandVisible(t, played)) continue;
        if (played > bestPlayed) {
            found = true;
            best = t;
            bestPlayed = played;
        }
    }
    if (!found || bestPlayed <= 0) return false;
    out = planetTypeFor(best);
    return true;
}

void GameState::applyVoucher(VoucherType t) {
    switch (t) {
    case VoucherType::Overstock:
    case VoucherType::OverstockPlus:
        mShop.changeShopSlots(1);
        mShop.ensureShopOfferCount();
        break;
    case VoucherType::ClearanceSale:
        mShop.setDiscountPercent(25);
        mShop.refreshCurrentOfferCosts();
        updateOwnedSellValues();
        break;
    case VoucherType::Liquidation:
        mShop.setDiscountPercent(50);
        mShop.refreshCurrentOfferCosts();
        updateOwnedSellValues();
        break;
    case VoucherType::RerollSurplus:
        mShop.setRerollDiscount(2);
        break;
    case VoucherType::RerollGlut:
        mShop.setRerollDiscount(4);
        break;
    case VoucherType::TarotMerchant:
        mShop.setTarotRate(9.6);
        break;
    case VoucherType::TarotTycoon:
        mShop.setTarotRate(32.0);
        break;
    case VoucherType::PlanetMerchant:
        mShop.setPlanetRate(9.6);
        break;
    case VoucherType::PlanetTycoon:
        mShop.setPlanetRate(32.0);
        break;
    case VoucherType::MagicTrick:
        // 原版 v_magic_trick: playing_card_rate = 4
        mShop.setPlayingCardRate(4.0);
        break;
    case VoucherType::Illusion:
        mShop.setPlayingCardRate(4.0);
        mShop.setPlayingCardsEnhanced(true);
        break;
    case VoucherType::CrystalBall:
        mExtraConsumableSlots += 1;
        break;
    case VoucherType::OmenGlobe:
        // generatePackContent 会在奥秘包内按概率把塔罗替换成幻灵。
        break;
    case VoucherType::Grabber:
    case VoucherType::NachoTong:
        mExtraHandsPerRound += 1;
        break;
    case VoucherType::Wasteful:
    case VoucherType::Recyclomancy:
        mExtraDiscardsPerRound += 1;
        break;
    case VoucherType::SeedMoney:
        mInterestCap = 50;
        break;
    case VoucherType::MoneyTree:
        mInterestCap = 100;
        break;
    case VoucherType::PaintBrush:
    case VoucherType::Palette:
        mExtraHandSize += 1;
        break;
    case VoucherType::Antimatter:
        mExtraJokerSlots += 1;
        break;
    case VoucherType::Hieroglyph:
        // 原版 card.lua:1957-1965: ease_ante(-1) + hands -=1
        mAnte = qMax(1, mAnte - 1);
        mExtraHandsPerRound -= 1;
        break;
    case VoucherType::Petroglyph:
        // 原版 card.lua:1957/1966-1969: ease_ante(-1) + discards -=1（注意是 discards 不是 hands）
        mAnte = qMax(1, mAnte - 1);
        mExtraDiscardsPerRound -= 1;
        break;
    case VoucherType::Hone:
        mShop.setJokerEditionRate(2.0);
        break;
    case VoucherType::GlowUp:
        mShop.setJokerEditionRate(4.0);
        break;
    case VoucherType::Telescope:
    case VoucherType::Observatory:
    case VoucherType::Blank:
    case VoucherType::DirectorsCut:
    case VoucherType::Retcon:
        // 与版本概率 / Boss 重掷 / 天文台持有星球牌联动的细节后续继续细化。
        break;
    }
}

int GameState::consumableSlots() const {
    int negativeSlots = 0;
    for (const Consumable &c : mConsumables) if (c.negative) ++negativeSlots;
    return Constants::MAX_CONSUMABLE_SLOTS + mExtraConsumableSlots + negativeSlots;
}

int GameState::handSize() const {
    int size = Constants::HAND_SIZE + mExtraHandSize + mOneRoundHandSizeBonus;
    for (const Joker &j : mJokers) {
        if (j.isDebuffed) continue;
        switch (j.type) {
        case JokerType::Stuntman:  size -= 2; break;
        case JokerType::Juggler:   size += 1; break;
        case JokerType::MerryAndy: size -= 1; break;
        case JokerType::Troubadour:size += 2; break;
        case JokerType::TurtleBean:size += qMax(0, j.counter); break;
        default: break;
        }
    }
    if (mPhase == GamePhase::Blind && mBlindType == BlindType::Boss &&
        mBossEffect == BossEffect::TheManacle && !hasJokerType(JokerType::Chicot)) size -= 1;
    return qMax(1, size);
}


bool GameState::dnaCanTriggerThisPlay() const
{
    return mPhase == GamePhase::Blind && mDNAEligibleThisPlay;
}

void GameState::createDNACopy(const CardData &card)
{
    if (mDryRun) return;
    if (!dnaCanTriggerThisPlay()) return;
    CardData copy = card;
    copy.assignNewUid();              // 这是新复制出的实体牌，不能沿用原牌 uid
    copy.faceUp = true;
    copy.isDebuffed = false;
    // 兼容旧的 joker effect 路径：正常实战中 DNA 已经在 playCards() 的
    // context.before 阶段提前处理；若有其它调用路径走到这里，仍暂存到
    // finalizePlayedHand() 前清理，避免计分中途 emit handChanged 产生幽灵牌。
    mPendingDNACopies.append(copy);
    ++mDNACopiesCreatedThisPlay;
}

void GameState::decayEndOfHandJokers()
{
    bool changed = false;
    for (Joker &j : mJokers) {
        if (j.type == JokerType::IceCream && j.counter > 0) {
            j.counter = qMax(0, j.counter - 5);
            changed = true;
        }
    }
    if (changed) emit jokersChanged();
}

// ── 最佳出牌提示：无副作用计分模拟器 ─────────────────────────────
// 镜像 playCards() 的计分段（约 319–544 行），但只写入局部 HandResult，
// 不改 mHand/mJokers/mHandLevels；随机效果按期望值估算；mDryRun 抑制
// 小丑效果里的 addGold/createDNACopy 等状态改写。
double GameState::simulatePlayScore(const QVector<int> &orderedIndices)
{
    if (orderedIndices.isEmpty()) return 0.0;

    QVector<CardData> played;
    QSet<int> playedSet;
    for (int i : orderedIndices) {
        if (i < 0 || i >= mHand.size()) return 0.0;
        played.append(mHand[i]);
        playedSet.insert(i);
    }

    HandResult result = HandEvaluator::evaluate(played, currentHandMods());

    // 牌型等级加成（只读 mHandLevels）
    int lvLevel = 1, lvChips = 0, lvMult = 0;
    auto itLv = mHandLevels.constFind(result.type);
    if (itLv != mHandLevels.constEnd()) {
        lvLevel = itLv->level; lvChips = itLv->chipsBonus; lvMult = itLv->multBonus;
    }
    // The Arm：用降一级后的加成估算（不真正修改等级）
    if (!hasJokerType(JokerType::Chicot) && mBossEffect == BossEffect::TheArm && lvLevel > 1) {
        auto d = handLevelDelta(result.type);
        lvLevel = qMax(1, lvLevel - 1);
        lvChips = qMax(0, lvChips - d.first);
        lvMult  = qMax(0, lvMult  - d.second);
    }
    result.chips += lvChips;
    result.mult  += lvMult;
    result.level  = lvLevel;
    result.xmult  = 1.0;

    if (!hasJokerType(JokerType::Chicot) && mBossEffect == BossEffect::TheFlint) {
        result.chips = std::max(0.0, std::floor(result.chips * 0.5 + 0.5));
        result.mult  = std::max(1.0, std::floor(result.mult  * 0.5 + 0.5));
    }

    // 找出 played 中参与计分的下标（与 playCards 相同的匹配逻辑）
    QVector<int> scoringPlayedIdx;
    QSet<int> used;
    for (const CardData &sc : result.scoringCards) {
        for (int k = 0; k < played.size(); ++k) {
            if (used.contains(k)) continue;
            const CardData &p = played[k];
            if (p.rank == sc.rank && p.suit == sc.suit
                && p.enhancement == sc.enhancement
                && p.edition == sc.edition && p.seal == sc.seal) {
                scoringPlayedIdx.append(k); used.insert(k); break;
            }
        }
    }
    for (int k = 0; k < played.size(); ++k) {
        if (played[k].enhancement != Enhancement::Stone) continue;
        if (!scoringPlayedIdx.contains(k)) scoringPlayedIdx.append(k);
        result.scoringCards.append(played[k]);
    }
    std::sort(scoringPlayedIdx.begin(), scoringPlayedIdx.end());

    const int sockRetriggers = countResolvedJokersOfType(mJokers, JokerType::SockAndBuskin);
    const int chadRetriggers = 2 * countResolvedJokersOfType(mJokers, JokerType::HangingChad);
    const int hikerTriggers  = countResolvedJokersOfType(mJokers, JokerType::Hiker);
    const int hackTriggers   = countResolvedJokersOfType(mJokers, JokerType::Hack);
    const int duskTriggers   = (mHandsLeft == 1)
                               ? countResolvedJokersOfType(mJokers, JokerType::Dusk) : 0;
    int seltzerTriggers = 0;
    for (const Joker &j : mJokers)
        if (!j.isDebuffed && j.type == JokerType::Seltzer && j.counter > 0) ++seltzerTriggers;

    // 单张计分牌的算分（镜像 scoreCard，去掉事件/RNG，随机效果取期望值）
    auto simScoreCard = [this, &result](const CardData &card, bool firstFaceCard) {
        if (card.enhancement != Enhancement::Stone) {
            int v = card.chipValue() + qMax(0, card.permanentBonusChips);
            if (v > 0) result.chips += v;
        }
        switch (card.enhancement) {
        case Enhancement::Bonus: result.chips += 30;   break;
        case Enhancement::Mult:  result.mult  += 4;    break;
        case Enhancement::Glass: result.xmult *= 2.0;  break;
        case Enhancement::Stone: result.chips += 50;   break;
        case Enhancement::Lucky: result.mult  += 4.0;  break;   // 期望值：20 × 1/5
        default: break;
        }
        switch (card.edition) {
        case Edition::Foil:        result.chips += 50;  break;
        case Edition::Holographic: result.mult  += 10;  break;
        case Edition::Polychrome:  result.xmult *= 1.5; break;
        default: break;
        }
        for (int ji = 0; ji < mJokers.size(); ++ji) {
            if (mJokers[ji].isDebuffed) continue;
            const Joker *ej = resolveCopiedJoker(mJokers, ji);
            if (!ej || ej->isDebuffed || ej->timing != TriggerTiming::OnScoringCard) continue;
            if (card.enhancement == Enhancement::Stone && jokerUsesCardRankOrSuit(ej->type)) continue;
            if (ej->type == JokerType::Bloodstone) {            // 随机 → 期望值
                if (!card.isDebuffed && card.suit == Suit::Hearts) result.xmult *= 1.25;
                continue;
            }
            TriggerContext ctx{ result, *this, mHand, result.scoringCards, &card };
            ctx.isFirstFaceCard = firstFaceCard;
            ctx.self = &mJokers[ji];
            applyResolvedJokerEffect(*ej, ctx);
        }
    };

    mDryRun = true;

    bool firstScoringCard = true;
    bool faceCardScoredYet = false;
    for (int playedIdx : scoringPlayedIdx) {
        CardData card = played[playedIdx];
        if (card.isDebuffed) { firstScoringCard = false; continue; }

        const int redSealReps = (card.seal == Seal::Red) ? 1 : 0;
        int triggers = 1 + redSealReps;
        const bool hasRankAndSuit = card.enhancement != Enhancement::Stone;
        const bool isFace = hasRankAndSuit && isFaceCard(card);   // 含幻想性错觉
        const bool firstFaceCard = isFace && !faceCardScoredYet;
        if (isFace) triggers += sockRetriggers;
        if (firstScoringCard) triggers += chadRetriggers;
        if (hasRankAndSuit) {
            int rv = static_cast<int>(card.rank);
            if (rv >= 2 && rv <= 5) triggers += hackTriggers;
        }
        triggers += duskTriggers + seltzerTriggers;

        if (hikerTriggers > 0) card.permanentBonusChips += 5 * hikerTriggers;

        for (int t = 0; t < triggers; ++t) simScoreCard(card, firstFaceCard);
        if (isFace) faceCardScoredYet = true;
        firstScoringCard = false;
    }

    // 留手牌效果（Steel / Baron / ShootTheMoon + Mime / 红蜡封重触发）
    QVector<CardData> heldHand;
    for (int i = 0; i < mHand.size(); ++i)
        if (!playedSet.contains(i)) heldHand.append(mHand[i]);

    const int mimeRetriggers     = countResolvedJokersOfType(mJokers, JokerType::Mime);
    const int baronTriggers      = countResolvedJokersOfType(mJokers, JokerType::Baron);
    const int shootMoonTriggers  = countResolvedJokersOfType(mJokers, JokerType::ShootTheMoon);
    for (const CardData &c : heldHand) {
        if (c.isDebuffed) continue;
        const bool hasHeldEffect = (c.enhancement == Enhancement::Steel)
            || (c.rank == Rank::King  && baronTriggers > 0)
            || (c.rank == Rank::Queen && shootMoonTriggers > 0);
        const int redSealReps = (hasHeldEffect && c.seal == Seal::Red) ? 1 : 0;
        int retriggers = 1 + redSealReps + (hasHeldEffect ? mimeRetriggers : 0);
        for (int r = 0; r < retriggers; ++r) {
            if (c.enhancement == Enhancement::Steel) result.xmult *= 1.5;
            if (c.rank == Rank::King)
                for (int b = 0; b < baronTriggers; ++b) result.xmult *= 1.5;
            if (c.rank == Rank::Queen)
                for (int sm = 0; sm < shootMoonTriggers; ++sm) result.mult += 13;
        }
    }

    // OnPlayedHand / Passive 小丑遍历 + 小丑版本
    {
        TriggerContext ctx{ result, *this, heldHand, result.scoringCards, nullptr };
        ctx.playedCards = &played;
        for (int ji = 0; ji < mJokers.size(); ++ji) {
            const Joker &j = mJokers[ji];
            if (j.isDebuffed) continue;
            const Joker *ej = resolveCopiedJoker(mJokers, ji);
            if (ej && !ej->isDebuffed &&
                (ej->timing == TriggerTiming::Passive ||
                 ej->timing == TriggerTiming::OnPlayedHand)) {
                if (ej->type == JokerType::Misprint) result.mult += 11.5;  // 期望值：0~23 均值
                else { ctx.self = &mJokers[ji]; applyResolvedJokerEffect(*ej, ctx); }
            }
            switch (j.edition) {
            case Edition::Foil:        result.chips += 50;  break;
            case Edition::Holographic: result.mult  += 10;  break;
            case Edition::Polychrome:  result.xmult *= 1.5; break;
            default: break;
            }
        }
    }

    if (hasVoucher(VoucherType::Observatory)) {
        for (const Consumable &c : mConsumables) {
            if (c.kind == ConsumableKind::Planet && planetMatchesHand(c.type, result.type))
                result.xmult *= 1.5;
        }
    }

    mDryRun = false;

    double score = result.chips * result.mult * result.xmult;
    if (!std::isfinite(score)) score = std::numeric_limits<double>::infinity();
    return score;
}

QVector<int> GameState::findBestPlay()
{
    const int n = qMin(mHand.size(), 16);   // 防御性上限，正常手牌远小于此
    if (n == 0) return {};
    const int maxI = qMin(5, n);
    int minI = 1;
    // 灵媒(The Psychic)：必须出 5 张
    if (mBossEffect == BossEffect::ThePsychic && !hasJokerType(JokerType::Chicot))
        minI = maxI;
    int forcedIdx = -1;
    if (mBossEffect == BossEffect::CeruleanBell && !hasJokerType(JokerType::Chicot) && mCeruleanForcedUid > 0) {
        for (int i = 0; i < n; ++i) {
            if (mHand[i].uid == mCeruleanForcedUid) {
                forcedIdx = i;
                break;
            }
        }
    }

    QVector<int> best;
    double bestScore = -1.0;

    for (quint32 mask = 1; mask < (1u << n); ++mask) {
        if (forcedIdx >= 0 && !(mask & (1u << forcedIdx))) continue;
        int pc = 0;
        for (int b = 0; b < n; ++b) if (mask & (1u << b)) ++pc;
        if (pc < minI || pc > maxI) continue;

        QVector<int> combo;                 // 手牌下标
        QVector<CardData> comboCards;
        for (int b = 0; b < n; ++b)
            if (mask & (1u << b)) { combo.append(b); comboCards.append(mHand[b]); }

        // 牌型与"哪些牌计分"只取决于集合，先评估一次
        HandResult hr = HandEvaluator::evaluate(comboCards, currentHandMods());
        QVector<int> scoringPos;            // combo 内位置
        QSet<int> usedPos;
        for (const CardData &sc : hr.scoringCards) {
            for (int k = 0; k < comboCards.size(); ++k) {
                if (usedPos.contains(k)) continue;
                const CardData &p = comboCards[k];
                if (p.rank == sc.rank && p.suit == sc.suit
                    && p.enhancement == sc.enhancement
                    && p.edition == sc.edition && p.seal == sc.seal) {
                    scoringPos.append(k); usedPos.insert(k); break;
                }
            }
        }
        for (int k = 0; k < comboCards.size(); ++k)
            if (comboCards[k].enhancement == Enhancement::Stone && !usedPos.contains(k)) {
                scoringPos.append(k); usedPos.insert(k);
            }
        QVector<int> nonScoringPos;
        for (int k = 0; k < comboCards.size(); ++k)
            if (!usedPos.contains(k)) nonScoringPos.append(k);

        // 只对计分牌做全排列（非计分牌顺序不影响分数）
        std::sort(scoringPos.begin(), scoringPos.end());
        QVector<int> perm = scoringPos;
        do {
            QVector<int> ordered;
            for (int p : perm)          ordered.append(combo[p]);
            for (int p : nonScoringPos) ordered.append(combo[p]);
            double s = simulatePlayScore(ordered);
            if (s > bestScore) { bestScore = s; best = ordered; }
        } while (std::next_permutation(perm.begin(), perm.end()));
    }
    return best;
}

void GameState::bringHandCardsToFront(const QVector<int> &indices)
{
    if (indices.isEmpty()) return;
    QSet<int> sel(indices.begin(), indices.end());
    QVector<CardData> front, rest;
    for (int i : indices)
        if (i >= 0 && i < mHand.size()) front.append(mHand[i]);
    for (int i = 0; i < mHand.size(); ++i)
        if (!sel.contains(i)) rest.append(mHand[i]);
    mHand = front + rest;
    // 最佳出牌只是临时把推荐牌移到最前方便选择，不应覆盖玩家原先的
    // 点数/花色理牌偏好；本手出完补牌后仍按原模式自动排序。
    emit handChanged();
}

void GameState::reorderHandByUids(const QVector<int> &uidOrder)
{
    if (uidOrder.isEmpty()) return;
    QHash<int, CardData> byUid;
    for (const CardData &c : mHand) byUid.insert(c.uid, c);
    QVector<CardData> rebuilt;
    rebuilt.reserve(mHand.size());
    QSet<int> placed;
    for (int uid : uidOrder) {
        auto it = byUid.find(uid);
        if (it != byUid.end()) {
            rebuilt.append(it.value());
            placed.insert(uid);
        }
    }
    // 快照里没有的新牌（理论上"最佳出牌"瞬间不该有补牌，但保险起见保留尾部）。
    for (const CardData &c : mHand) {
        if (!placed.contains(c.uid)) rebuilt.append(c);
    }
    if (rebuilt.size() != mHand.size()) return;   // 数据不一致就放弃
    mHand = rebuilt;
    emit handChanged();
}

void GameState::scoreCard(const CardData &card, HandResult &result, int playedIdx, bool firstFaceCard)
{
    // 1) chip:Stone 不加点数 chip
    if (card.enhancement != Enhancement::Stone) {
        int v = card.chipValue() + qMax(0, card.permanentBonusChips);
        if (v > 0) {
            result.chips += v;
            result.events.append({ ScoreEventKind::ScoringCardChip, playedIdx, -1, -1, v, 1.0 });
        }
    }

    // 2) enhancement
    switch (card.enhancement) {
    case Enhancement::Bonus:
        result.chips += 30;
        result.events.append({ ScoreEventKind::ScoringCardChip, playedIdx, -1, -1, 30, 1.0 });
        break;
    case Enhancement::Mult:
        result.mult += 4;
        result.events.append({ ScoreEventKind::EnhancementMult, playedIdx, -1, -1, 4, 1.0 });
        break;
    case Enhancement::Glass:
        result.xmult *= 2.0;
        result.events.append({ ScoreEventKind::EnhancementXMult, playedIdx, -1, -1, 0, 2.0 });
        break;
    case Enhancement::Stone:
        result.chips += 50;
        result.events.append({ ScoreEventKind::ScoringCardChip, playedIdx, -1, -1, 50, 1.0 });
        break;
    case Enhancement::Lucky: {
        bool luckyTriggered = false;
        if (QRandomGenerator::global()->bounded(probDenom(5)) == 0) {
            result.mult += 20;
            result.events.append({ ScoreEventKind::EnhancementMult, playedIdx, -1, -1, 20, 1.0 });
            luckyTriggered = true;
        }
        if (QRandomGenerator::global()->bounded(probDenom(15)) == 0) {
            addGold(20);
            result.events.append({ ScoreEventKind::DollarGain, playedIdx, -1, -1, 20, 1.0 });
            luckyTriggered = true;
        }
        // Batch 2：幸运猫——每次幸运牌触发 +X0.25 倍率
        if (luckyTriggered)
            for (Joker &lj : mJokers)
                if (!lj.isDebuffed && lj.type == JokerType::LuckyCat) lj.counter += 1;
        break;
    }
    default: break;
    }

    // 3) edition
    switch (card.edition) {
    case Edition::Foil:
        result.chips += 50;
        result.events.append({ ScoreEventKind::EditionChip, playedIdx, -1, -1, 50, 1.0 });
        break;
    case Edition::Holographic:
        result.mult += 10;
        result.events.append({ ScoreEventKind::EditionMult, playedIdx, -1, -1, 10, 1.0 });
        break;
    case Edition::Polychrome:
        result.xmult *= 1.5;
        result.events.append({ ScoreEventKind::EditionXMult, playedIdx, -1, -1, 0, 1.5 });
        break;
    default: break;
    }

    // 4) OnScoringCard 小丑:之前直接调 effect,现在需要在 ctx 里带 idx 让 effect 写事件
    // 但 effect 是 lambda 修改 ctx.result,没法知道是哪个 joker。
    // 简化做法:在 effect 调用前后比较 chips/mult 差值。
    double chipsBefore = result.chips, multBefore = result.mult;
    double xmultBefore = result.xmult;

    for (int ji = 0; ji < mJokers.size(); ++ji) {
        const Joker &j = mJokers[ji];
        if (j.isDebuffed) continue;

        const Joker *effectJoker = resolveCopiedJoker(mJokers, ji);

        if (!effectJoker || effectJoker->isDebuffed || effectJoker->timing != TriggerTiming::OnScoringCard) continue;
        if (card.enhancement == Enhancement::Stone && jokerUsesCardRankOrSuit(effectJoker->type)) continue;

        TriggerContext ctx{ result, *this, mHand, result.scoringCards, &card };
        ctx.isFirstFaceCard = firstFaceCard;
        ctx.self = &mJokers[ji];
        applyResolvedJokerEffect(*effectJoker, ctx);

        if (result.chips != chipsBefore) {
            result.events.append({ ScoreEventKind::JokerChip, playedIdx, -1, ji,
                                  result.chips - chipsBefore, 1.0 });
            chipsBefore = result.chips;
        }
        if (result.mult != multBefore) {
            result.events.append({ ScoreEventKind::JokerMult, playedIdx, -1, ji,
                                  result.mult - multBefore, 1.0 });
            multBefore = result.mult;
        }
        if (qAbs(result.xmult - xmultBefore) > 1e-6) {
            result.events.append({ ScoreEventKind::JokerXMult, playedIdx, -1, ji,
                                  0, result.xmult / xmultBefore });
            xmultBefore = result.xmult;
        }
    }

    // 5) Gold Seal +$3
    if (card.seal == Seal::Gold) {
        addGold(3);
        result.events.append({ ScoreEventKind::DollarGain, playedIdx, -1, -1, 3, 1.0 });
    }
}

ConsumableType GameState::planetTypeFor(HandType t)
{
    switch (t) {
    case HandType::HighCard:      return ConsumableType::Planet_Pluto;
    case HandType::Pair:          return ConsumableType::Planet_Mercury;
    case HandType::TwoPair:       return ConsumableType::Planet_Uranus;
    case HandType::ThreeOfAKind:  return ConsumableType::Planet_Venus;
    case HandType::Straight:      return ConsumableType::Planet_Saturn;
    case HandType::Flush:         return ConsumableType::Planet_Jupiter;
    case HandType::FullHouse:     return ConsumableType::Planet_Earth;
    case HandType::FourOfAKind:   return ConsumableType::Planet_Mars;
    case HandType::StraightFlush:
    case HandType::RoyalFlush:    return ConsumableType::Planet_Neptune;
    case HandType::FiveOfAKind:   return ConsumableType::Planet_PlanetX;
    case HandType::FlushHouse:    return ConsumableType::Planet_Ceres;
    case HandType::FlushFive:     return ConsumableType::Planet_Eris;
    }
    return ConsumableType::Planet_Pluto;
}

bool GameState::buyPack(int idx, PackContent &out)
{
    if (mPhase != GamePhase::Shop) return false;
    if (!mShop.canBuyBooster(idx, spendableGold())) return false;
    const ShopOffer &o = mShop.boosterOffers()[idx];
    if (o.kind != OfferKind::Pack) return false;

    ShopOffer t = mShop.takeBoosterOffer(idx);
    mGold -= t.cost;
    ConsumableType telescopePlanet = ConsumableType::Planet_Pluto;
    const bool telescopeActive = hasVoucher(VoucherType::Telescope) && telescopePlanetForPack(telescopePlanet);
    out = generatePackContent(t.pack, t.packSize, hasVoucher(VoucherType::OmenGlobe), telescopeActive,
                              telescopePlanet,
                              ownedJokerTypes(), hasJokerDuplicateBypass(), mGrosMichelExtinct);
    out.spriteVariant = t.packVariant;

    // Batch 8：幻觉——打开卡包时 1/2 概率创建塔罗牌
    for (const Joker &j : mJokers)
        if (!j.isDebuffed && j.type == JokerType::Hallucination
            && QRandomGenerator::global()->bounded(2) == 0)
            addConsumable(randomTarotType());

    emit goldChanged();
    emit shopChanged();
    return true;
}


QVector<CardData> GameState::drawPackHand()
{
    QVector<CardData> out;
    int n = handSize();
    for (int i = 0; i < n; ++i) {
        if (mDeck.isEmpty()) mDeck.reset();
        if (mDeck.isEmpty()) break;
        out.append(mDeck.draw());
    }
    // 塔罗 / 幻灵包打开时，临时手牌也要遵循玩家当前的理牌方式——
    // 否则随机抽出的顺序看起来"乱"。Manual 模式不动顺序（玩家自己拖过的当前手牌的次序与抽出的随机序无关，
    // 这里就当 Manual 是"按抽到的随机顺序"也合理）。
    if (mSortMode == HandSortMode::ByRank)
        std::sort(out.begin(), out.end(), rankComp);
    else if (mSortMode == HandSortMode::BySuit)
        std::sort(out.begin(), out.end(), suitComp);
    emit scoreChanged();
    return out;
}

void GameState::returnPackHand(const QVector<CardData> &packHand)
{
    // 开包界面的临时手牌来自牌组。没被摧毁的牌要放回牌组；
    // 被 Immolate 等效果删除的牌不会出现在 packHand 里，也就不会放回。
    for (const CardData &c : packHand) {
        mDeck.addCard(c);
    }
    emit scoreChanged();
}

static QVector<int> normalizedSelection(const QVector<int> &selected, int size, int maxSelection)
{
    QVector<int> sel = selected;
    std::sort(sel.begin(), sel.end());
    sel.erase(std::unique(sel.begin(), sel.end()), sel.end());
    sel.erase(std::remove_if(sel.begin(), sel.end(), [size](int idx) {
        return idx < 0 || idx >= size;
    }), sel.end());
    return sel;
}

static bool applyConsumableTypeToPackHand(GameState &state,
                                          ConsumableType type,
                                          const QVector<int> &selectedPackHandIdx,
                                          QVector<CardData> &packHand)
{
    Consumable c = createConsumable(type);
    QVector<int> sel = normalizedSelection(selectedPackHandIdx, packHand.size(), c.maxSelection);
    if (sel.size() < c.needsSelection) return false;
    if (c.maxSelection > 0 && sel.size() > c.maxSelection) return false;
    if (consumableNeedsFreeJokerSlot(type) && !state.canAddJoker()) return false;

    auto enhance = [&](Enhancement e, int maxN) {
        QVector<int> use = normalizedSelection(sel, packHand.size(), maxN);
        int applied = 0;
        for (int idx : use) { if (applied++ >= maxN) break; packHand[idx].enhancement = e; }
    };
    auto seal = [&](Seal s, int maxN) {
        QVector<int> use = normalizedSelection(sel, packHand.size(), maxN);
        int applied = 0;
        for (int idx : use) { if (applied++ >= maxN) break; packHand[idx].seal = s; }
    };
    auto randomEdition = [&](int maxN) {
        QVector<int> use = normalizedSelection(sel, packHand.size(), maxN);
        int applied = 0;
        for (int idx : use) {
            if (applied++ >= maxN) break;
            int r = QRandomGenerator::global()->bounded(3);
            if      (r == 0) packHand[idx].edition = Edition::Foil;
            else if (r == 1) packHand[idx].edition = Edition::Holographic;
            else             packHand[idx].edition = Edition::Polychrome;
        }
    };
    auto destroyRandomPackCards = [&](int count, int goldGain) {
        QVector<int> idx;
        for (int i = 0; i < packHand.size(); ++i) idx.append(i);
        for (int i = idx.size() - 1; i > 0; --i) {
            int j = QRandomGenerator::global()->bounded(i + 1);
            idx.swapItemsAt(i, j);
        }
        int n = qMin(count, idx.size());
        idx = idx.mid(0, n);
        std::sort(idx.begin(), idx.end(), std::greater<int>());
        for (int i : idx) {
            if (i < 0 || i >= packHand.size()) continue;
            state.notifyPlayingCardDestroyed(packHand[i]);
            packHand.removeAt(i);
        }
        if (n > 0) state.addGold(goldGain);
    };
    auto copySelectedPackCardTwice = [&]() {
        if (sel.size() != 1) return false;
        int idx = sel.first();
        if (idx < 0 || idx >= packHand.size()) return false;
        CardData c = packHand[idx];
        packHand.append(c);
        packHand.append(c);
        return true;
    };
    auto setPackSuit = [&]() {
        Suit ss = static_cast<Suit>(QRandomGenerator::global()->bounded(4));
        for (CardData &c : packHand) if (c.enhancement != Enhancement::Stone) c.suit = ss;
    };
    auto setPackRank = [&]() {
        Rank rr = static_cast<Rank>(QRandomGenerator::global()->bounded(13) + 2);
        for (CardData &c : packHand) if (c.enhancement != Enhancement::Stone) c.rank = rr;
        state.addPermanentHandSizeBonus(-1);
    };

    auto rankUpPack = [&](int maxN) {
        QVector<int> use = normalizedSelection(sel, packHand.size(), maxN);
        for (int idx : use) {
            if (idx < 0 || idx >= packHand.size()) continue;
            if (packHand[idx].enhancement != Enhancement::Stone) {
                if (packHand[idx].rank != Rank::Ace)
                    packHand[idx].rank = static_cast<Rank>(static_cast<int>(packHand[idx].rank) + 1);
            }
        }
    };
    auto suitPack = [&](Suit ss, int maxN) {
        QVector<int> use = normalizedSelection(sel, packHand.size(), maxN);
        for (int idx : use) {
            if (idx < 0 || idx >= packHand.size()) continue;
            if (packHand[idx].enhancement != Enhancement::Stone) packHand[idx].suit = ss;
        }
    };
    auto deathPack = [&]() -> bool {
        if (sel.size() != 2) return false;
        int a = sel[0], b = sel[1];
        if (a < 0 || a >= packHand.size() || b < 0 || b >= packHand.size() || a == b) return false;
        packHand[a] = packHand[b];
        return true;
    };

    switch (type) {
    case ConsumableType::Tarot_Fool:       return state.addFoolCopyConsumable();
    case ConsumableType::Tarot_Magician:   enhance(Enhancement::Lucky, 2); break;
    case ConsumableType::Tarot_HighPriestess:
        for (int i = 0; i < 2; ++i) { if (!state.addConsumable(randomPlanetType())) break; }
        break;
    case ConsumableType::Tarot_Empress:    enhance(Enhancement::Mult, 2); break;
    case ConsumableType::Tarot_Emperor:
        for (int i = 0; i < 2; ++i) { if (!state.addConsumable(randomTarotType())) break; }
        break;
    case ConsumableType::Tarot_Hierophant: enhance(Enhancement::Bonus, 2); break;
    case ConsumableType::Tarot_Chariot:    enhance(Enhancement::Steel, 1); break;
    case ConsumableType::Tarot_Lovers:     enhance(Enhancement::Wild, 1); break;
    case ConsumableType::Tarot_Justice:    enhance(Enhancement::Glass, 1); break;
    case ConsumableType::Tarot_Tower:      enhance(Enhancement::Stone, 1); break;
    case ConsumableType::Tarot_HangedMan: {
        QVector<int> use = normalizedSelection(sel, packHand.size(), 2);
        std::sort(use.begin(), use.end(), std::greater<int>());
        for (int idx : use) {
            if (idx < 0 || idx >= packHand.size()) continue;
            state.notifyPlayingCardDestroyed(packHand[idx]);
            packHand.removeAt(idx);
        }
        break;
    }
    case ConsumableType::Tarot_Hermit: {
        int gain = qMin(state.gold(), 20);
        state.addGold(gain);
        break;
    }
    case ConsumableType::Tarot_Wheel:
        if (QRandomGenerator::global()->bounded(4) == 0) {
            constexpr Edition editions[] = { Edition::Foil, Edition::Holographic, Edition::Polychrome };
            state.setRandomEditionlessJoker(editions[QRandomGenerator::global()->bounded(3)], false, false);
        }
        break;
    case ConsumableType::Tarot_Strength:   rankUpPack(2); break;
    case ConsumableType::Tarot_Death:      return deathPack();
    case ConsumableType::Tarot_Temperance: {
        int total = 0;
        for (const Joker &j : state.jokers()) total += qMax(1, j.sellValue);
        state.addGold(qMin(total, 50));
        break;
    }
    case ConsumableType::Tarot_Devil:      enhance(Enhancement::Gold, 1); break;
    case ConsumableType::Tarot_Star:       suitPack(Suit::Diamonds, 3); break;
    case ConsumableType::Tarot_Moon:       suitPack(Suit::Clubs, 3); break;
    case ConsumableType::Tarot_Sun:        suitPack(Suit::Hearts, 3); break;
    case ConsumableType::Tarot_Judgement:  state.addRandomRareJoker(); break;
    case ConsumableType::Tarot_World:      suitPack(Suit::Spades, 3); break;

    case ConsumableType::Planet_Pluto:     state.levelUpHand(HandType::HighCard); break;
    case ConsumableType::Planet_Mercury:   state.levelUpHand(HandType::Pair); break;
    case ConsumableType::Planet_Uranus:    state.levelUpHand(HandType::TwoPair); break;
    case ConsumableType::Planet_Venus:     state.levelUpHand(HandType::ThreeOfAKind); break;
    case ConsumableType::Planet_Saturn:    state.levelUpHand(HandType::Straight); break;
    case ConsumableType::Planet_Jupiter:   state.levelUpHand(HandType::Flush); break;
    case ConsumableType::Planet_Earth:     state.levelUpHand(HandType::FullHouse); break;
    case ConsumableType::Planet_Mars:      state.levelUpHand(HandType::FourOfAKind); break;
    case ConsumableType::Planet_Neptune:   state.levelUpHand(HandType::StraightFlush); break;
    case ConsumableType::Planet_PlanetX:   state.levelUpHand(HandType::FiveOfAKind); break;
    case ConsumableType::Planet_Ceres:     state.levelUpHand(HandType::FlushHouse); break;
    case ConsumableType::Planet_Eris:      state.levelUpHand(HandType::FlushFive); break;

    case ConsumableType::Spectral_Familiar: {
        if (!packHand.isEmpty()) {
            int victim = QRandomGenerator::global()->bounded(packHand.size());
            state.notifyPlayingCardDestroyed(packHand[victim]);
            packHand.removeAt(victim);
        }
        for (int i = 0; i < 3; ++i) {
            CardData c; c.suit = static_cast<Suit>(QRandomGenerator::global()->bounded(4));
            constexpr Rank faces[] = { Rank::Jack, Rank::Queen, Rank::King };
            c.rank = faces[QRandomGenerator::global()->bounded(3)];
            c.enhancement = Enhancement::Bonus;
            packHand.append(c);
        }
        break;
    }
    case ConsumableType::Spectral_Grim: {
        if (!packHand.isEmpty()) {
            int victim = QRandomGenerator::global()->bounded(packHand.size());
            state.notifyPlayingCardDestroyed(packHand[victim]);
            packHand.removeAt(victim);
        }
        for (int i = 0; i < 2; ++i) {
            CardData c; c.suit = static_cast<Suit>(QRandomGenerator::global()->bounded(4));
            c.rank = Rank::Ace; c.enhancement = Enhancement::Bonus;
            packHand.append(c);
        }
        break;
    }
    case ConsumableType::Spectral_Incantation: {
        if (!packHand.isEmpty()) {
            int victim = QRandomGenerator::global()->bounded(packHand.size());
            state.notifyPlayingCardDestroyed(packHand[victim]);
            packHand.removeAt(victim);
        }
        for (int i = 0; i < 4; ++i) {
            CardData c; c.suit = static_cast<Suit>(QRandomGenerator::global()->bounded(4));
            c.rank = static_cast<Rank>(QRandomGenerator::global()->bounded(9) + 2);
            c.enhancement = Enhancement::Bonus;
            packHand.append(c);
        }
        break;
    }
    case ConsumableType::Spectral_Talisman: seal(Seal::Gold, 1); break;
    case ConsumableType::Spectral_Aura:     randomEdition(1); break;
    case ConsumableType::Spectral_Wraith:
        if (state.addRandomRareJoker()) state.addGold(-state.gold());
        break;
    case ConsumableType::Spectral_Sigil:    setPackSuit(); break;
    case ConsumableType::Spectral_Ouija:    setPackRank(); break;
    case ConsumableType::Spectral_Ectoplasm: state.setRandomEditionlessJoker(Edition::Negative, false, true); break;
    case ConsumableType::Spectral_DejaVu:   seal(Seal::Red, 1); break;
    case ConsumableType::Spectral_Hex:      state.setRandomEditionlessJoker(Edition::Polychrome, true, false); break;
    case ConsumableType::Spectral_Trance:   seal(Seal::Blue, 1); break;
    case ConsumableType::Spectral_Medium:   seal(Seal::Purple, 1); break;
    case ConsumableType::Spectral_Cryptid:
        return copySelectedPackCardTwice();
    case ConsumableType::Spectral_Ankh:
        state.duplicateRandomJokerAndDestroyOthers();
        break;
    case ConsumableType::Spectral_Immolate:
        destroyRandomPackCards(5, 20);
        break;
    case ConsumableType::Spectral_Soul:
        return state.addRandomLegendaryJoker();
    case ConsumableType::Spectral_BlackHole:
        state.levelUpAllHands(1);
        break;
    }
    return true;
}

bool GameState::applyPackChoice(const PackContent &pack, int chosenIdx,
                                const QVector<int> &selectedPackHandIdx,
                                QVector<CardData> &packHand)
{
    if (chosenIdx < 0) return false;

    switch (pack.kind) {
    case PackKind::Standard:
        if (chosenIdx >= pack.standardCards.size()) return false;
        mDeck.addCard(pack.standardCards[chosenIdx]);
        for (Joker &j : mJokers) if (!j.isDebuffed && j.type == JokerType::Hologram) ++j.counter;
        return true;

    case PackKind::Arcana:
    case PackKind::Celestial:
    case PackKind::Spectral: {
        if (chosenIdx >= pack.consumables.size()) return false;
        ConsumableType usedType = pack.consumables[chosenIdx];
        bool ok = applyConsumableTypeToPackHand(*this, usedType,
                                                selectedPackHandIdx, packHand);
        if (ok && usedType == ConsumableType::Spectral_BlackHole) {
            mPlanetsUsedThisRun.insert(static_cast<int>(usedType));
            for (Joker &j : mJokers)
                if (!j.isDebuffed && j.type == JokerType::Constellation) j.counter += 1;
        }
        if (ok && canRecordForFool(usedType)) {
            mLastUsedConsumable = usedType;
            mHasLastUsedConsumable = true;
        }
        return ok;
    }

    case PackKind::Buffoon:
        if (chosenIdx >= pack.jokers.size()) return false;
        if (!canAddJoker()) return false;
        mJokers.append(createJoker(pack.jokers[chosenIdx]));
        updateOwnedSellValues();
        syncShopJokerRules();
        emit jokersChanged();
        emit shopChanged();
        return true;
    }
    return false;
}

bool GameState::useConsumableOnPackHand(int consumableIdx,
                                        const QVector<int> &selectedPackHandIdx,
                                        QVector<CardData> &packHand)
{
    if (consumableIdx < 0 || consumableIdx >= mConsumables.size()) return false;
    Consumable c = mConsumables[consumableIdx];
    QVector<int> sel = normalizedSelection(selectedPackHandIdx, packHand.size(), c.maxSelection);
    if (sel.size() < c.needsSelection) return false;
    if (c.maxSelection > 0 && sel.size() > c.maxSelection) return false;

    if (c.type == ConsumableType::Tarot_Fool && !mHasLastUsedConsumable) return false;
    if (c.type == ConsumableType::Tarot_Fool && !canRecordForFool(mLastUsedConsumable)) return false;
    if (consumableNeedsFreeJokerSlot(c.type) && !canAddJoker()) return false;

    // 先腾出槽位，皇帝/女祭司才能在原有消耗牌槽位里补满两张。
    mConsumables.removeAt(consumableIdx);

    bool ok = applyConsumableTypeToPackHand(*this, c.type, sel, packHand);
    if (!ok) {
        mConsumables.insert(consumableIdx, c);
        emit consumablesChanged();
        return false;
    }

    if (c.type == ConsumableType::Spectral_BlackHole) {
        mPlanetsUsedThisRun.insert(static_cast<int>(c.type));
        for (Joker &j : mJokers)
            if (!j.isDebuffed && j.type == JokerType::Constellation) j.counter += 1;
    }

    if (canRecordForFool(c.type)) {
        mLastUsedConsumable = c.type;
        mHasLastUsedConsumable = true;
    }

    emit consumablesChanged();
    return true;
}

void GameState::startGame()
{
    // 演示模式：在所有 RNG 命中点之前把脚本计数器归零，保证本局完整走脚本一遍。
    if (DemoScript::active()) DemoScript::onStartGame();

    mDeck = Deck();
    mHand.clear();
    mAwaitingScoreFinalize = false;
    mPendingHandScore = 0;
    mPendingPlayedIndices.clear();
    mPendingShattered.clear();
    mGold = Constants::INITIAL_GOLD;
    mPendingRoundPayout = 0;
    mSuppressGoldSignal = false;
    mAnte = 1;
    mScore = 0;
    // 必须在 enterBlindSelect / refreshCounters 之前给 hands/discards 重置一个合法初值，
    // 否则上一局的尾值（含 Burglar/Manacle 等可能减为 0/-1 的状态）会被 UI 短暂显示。
    mHandsLeft = Constants::INITIAL_HANDS;
    mDiscardLeft = Constants::INITIAL_DISCARDS;
    mBlindStartingHands = mHandsLeft;
    mBlindStartingDiscards = mDiscardLeft;
    mTargetScore = 0;
    mJokers.clear();
    mShop = Shop();
    syncShopJokerRules();
    mHandLevels.clear();
    mConsumables.clear();
    mRedeemedVouchers.clear();
    mExtraConsumableSlots = 0;
    mExtraHandsPerRound = 0;
    mExtraDiscardsPerRound = 0;
    mExtraHandSize = 0;
    mInterestCap = Constants::INTEREST_MAX;
    mExtraJokerSlots = 0;
    mOneRoundHandSizeBonus = 0;
    mPendingInvestmentBonus = 0;
    mTagVoucherNextShop = false;
    mTagVoucherPendingShops = 0;
    mTagCouponPendingShops = 0;
    mTagD6PendingShops = 0;
    mHasTagFreePack = false;
    mActiveTags.clear();
    mLastSkippedTag = TagType::Skip;
    mLastConsumedDoubleTags = 0;
    mPendingDoubleTags = 0;
    mTotalSkipsThisRun = 0;
    mTotalHandsPlayedThisRun = 0;
    mUnusedDiscardsThisRun = 0;
    mBlindIdx = 0;
    mPendingBossEffect = BossEffect::None;
    mBossRerollsUsedThisAnte = 0;
    mBossMouthHasHand = false;
    mBossEyePlayedHands.clear();
    mSortMode = HandSortMode::ByRank;
    mCainoXMult = 1.0;
    mYorickXMult = 1.0;
    mYorickDiscardsRemaining = 23;
    mHasLastUsedConsumable = false;
    mLastUsedConsumable = ConsumableType::Tarot_Fool;
    mGrosMichelExtinct = false;
    mEndlessMode = false;
    mCardsPlayedThisAnte.clear();
    mCrimsonHeartDisabled = -1;
    mLastCrimsonHeartDisabled = -1;
    mVerdantLeafActive = false;
    mCeruleanForcedUid = -1;

    for (int i = 0; i < 3; ++i)
        mBlindStates[i] = (i == 0) ? BlindState::Current : BlindState::Upcoming;

    mFirstShop = true;
    mVoucherRolledAnte = 0;
    prepareBlindTags();
    enterBlindSelect();

    emit jokersChanged();
    emit consumablesChanged();
}

void GameState::prepareBlindTags()
{
    mBlindTags[0] = randomTagForAnte(mAnte);
    do {
        mBlindTags[1] = randomTagForAnte(mAnte);
    } while (mBlindTags[1] == mBlindTags[0]);
}

TagType GameState::blindTag(int idx) const
{
    if (idx == 0 || idx == 1) return mBlindTags[idx];
    return TagType::Skip;
}

int GameState::projectedTagReward(TagType type) const
{
    switch (type) {
    case TagType::Skip:
        return qMax(1, mTotalSkipsThisRun + 1) * 5;
    case TagType::Handy:
        return qMax(0, mTotalHandsPlayedThisRun);
    case TagType::Garbage:
        return qMax(0, mUnusedDiscardsThisRun);
    case TagType::Economy:
        return qMin(40, qMax(0, mGold));
    default:
        return 0;
    }
}

QString GameState::tagDescriptionFor(TagType type) const
{
    const TagData td = tagData(type);
    const QString money = QStringLiteral("<span style=\"color:#eac058;font-weight:bold\">$%1</span>");
    switch (type) {
    case TagType::Skip:
        return QStringLiteral("%1<br/>本赛局每跳过一次盲注，获得 %2<br/>"
                              "<span style=\"color:#9aa4a9\">将获得 %3</span>")
            .arg(td.name, money.arg(5), money.arg(projectedTagReward(type)));
    case TagType::Handy:
        return QStringLiteral("%1<br/>本赛局每打出一次手牌，获得 %2<br/>"
                              "<span style=\"color:#9aa4a9\">将获得 %3</span>")
            .arg(td.name, money.arg(1), money.arg(projectedTagReward(type)));
    case TagType::Garbage:
        return QStringLiteral("%1<br/>本赛局每个未使用的弃牌次数，获得 %2<br/>"
                              "<span style=\"color:#9aa4a9\">将获得 %3</span>")
            .arg(td.name, money.arg(1), money.arg(projectedTagReward(type)));
    case TagType::Economy:
        return QStringLiteral("%1<br/>将当前金钱翻倍，最多获得 %2<br/>"
                              "<span style=\"color:#9aa4a9\">将获得 %3</span>")
            .arg(td.name, money.arg(40), money.arg(projectedTagReward(type)));
    case TagType::D6:
        return QStringLiteral("%1<br/>下一次商店重掷起价为 %2").arg(td.name, money.arg(0));
    case TagType::Voucher:
        return QStringLiteral("%1<br/>下个商店额外添加 1 张优惠券<br/>"
                              "<span style=\"color:#9aa4a9\">优惠券本身不免费</span>").arg(td.name);
    case TagType::Rare:
        return QStringLiteral("%1<br/>下个商店强制生成一张免费的稀有小丑<br/>"
                              "<span style=\"color:#9aa4a9\">若没有可用稀有小丑则 NOPE</span>").arg(td.name);
    case TagType::Uncommon:
        return QStringLiteral("%1<br/>下个商店强制生成一张免费的罕见小丑").arg(td.name);
    default:
        return QStringLiteral("%1<br/>%2").arg(td.name, td.description);
    }
}

void GameState::enterBlindSelect() {
    mPhase = GamePhase::BlindSelect;
    mBossEffect = BossEffect::None;  // 已激活 Boss 效果只存在于 Boss 对局内，不能泄漏到下个小盲注。
    mBlindType = static_cast<BlindType>(mBlindIdx);
    if (mPendingBossEffect == BossEffect::None)
        mPendingBossEffect = randomBossEffect(mAnte);
    mTargetScore = calcTargetScore();
    emit blindSelectEntered();
}

void GameState::selectCurrentBlind()
{
    if (mPhase != GamePhase::BlindSelect) return;
    startBlind(mBlindType);
}

void GameState::startBlind(BlindType type)
{
    mBlindType = type;
    mScore = 0;
    mPendingRoundPayout = 0;
    mSuppressGoldSignal = false;
    // Batch 3：吟游诗人每回合 -1 出牌
    int handsJokerDelta = 0;
    for (const Joker &j : mJokers)
        if (!j.isDebuffed && j.type == JokerType::Troubadour) handsJokerDelta -= 1;
    mHandsLeft = qMax(1, Constants::INITIAL_HANDS + mExtraHandsPerRound + handsJokerDelta);
    mBlindStartingHands = mHandsLeft;
    mHandTypesPlayedThisRound.clear();   // 锋利卡牌：新回合清空已打牌型
    // Batch 2：上路每回合清零；城堡每回合随机选一个计数花色
    mCastleSuit = static_cast<Suit>(QRandomGenerator::global()->bounded(4));
    // Batch 3：邮件回扣 / 远古小丑 / 偶像每回合随机指定点数·花色
    mMailRank   = static_cast<Rank>(2 + QRandomGenerator::global()->bounded(13));
    mAncientSuit = static_cast<Suit>(QRandomGenerator::global()->bounded(4));
    mIdolRank   = static_cast<Rank>(2 + QRandomGenerator::global()->bounded(13));
    mIdolSuit   = static_cast<Suit>(QRandomGenerator::global()->bounded(4));
    for (Joker &j : mJokers)
        if (j.type == JokerType::HitTheRoad) j.counter = 0;
    mFirstDiscardThisRound = true;   // 焦痕小丑：新回合重置首次弃牌标记
    mDNAUsedThisBlind = false;
    mDNAEligibleThisPlay = false;
    mDNACopiesCreatedThisPlay = 0;
    // Batch 3：酒鬼 +1 弃牌、欢乐安迪 +3 弃牌
    int discardJokerDelta = 0;
    for (const Joker &j : mJokers) {
        if (j.isDebuffed) continue;
        if (j.type == JokerType::Drunkard)  discardJokerDelta += 1;
        if (j.type == JokerType::MerryAndy) discardJokerDelta += 3;
    }
    mDiscardLeft = qMax(0, Constants::INITIAL_DISCARDS + mExtraDiscardsPerRound + discardJokerDelta);
    mBlindStartingDiscards = mDiscardLeft;   // 延迟满足：判断本回合是否用过弃牌

    mCrimsonHeartDisabled = -1;
    mLastCrimsonHeartDisabled = -1;
    mVerdantLeafActive = false;
    mCeruleanForcedUid = -1;

    if (type == BlindType::Boss) {
        mBossEffect = mPendingBossEffect;     // 用预先确定的
        if (!hasJokerType(JokerType::Chicot)) {
            if (mBossEffect == BossEffect::TheNeedle) { mHandsLeft = 1; mBlindStartingHands = 1; }
            if (mBossEffect == BossEffect::TheWater)  mDiscardLeft = 0;
            // 翠绿之叶：卖出小丑前全牌禁用。
            if (mBossEffect == BossEffect::VerdantLeaf) mVerdantLeafActive = true;
            // 琥珀橡果：所有小丑翻面并打乱顺序（顺序会影响蓝图等小丑）。
            if (mBossEffect == BossEffect::AmberAcorn && mJokers.size() > 1) {
                for (int i = mJokers.size() - 1; i > 0; --i) {
                    int j = QRandomGenerator::global()->bounded(i + 1);
                    mJokers.swapItemsAt(i, j);
                }
                emit jokersChanged();
            }
        }
        mBossMouthHasHand = false;
        mBossEyePlayedHands.clear();
    } else {
        mBossEffect = BossEffect::None;
    }

    mTargetScore = calcTargetScore();
    mPhase = GamePhase::Blind;

    // 原版每个盲注开始都会用当前完整牌组重新洗牌；上一盲注结束时手里没打出的牌也要回到牌组。
    mDeck.returnCards(mHand);
    mHand.clear();
    // 演示模式：在 reset 之前 ++blindEntered，让 Deck::reset 里的 reorderDeckForNextBlind
    // 读到正确的盲注编号（1=小盲, 2=大盲, 3=Boss）。
    if (DemoScript::active()) DemoScript::onEnterBlind();
    mDeck.reset();
    triggerBlindSelectJokers(type);   // Batch 4：乌合之众/弹珠/窃贼/占卜师/证书/疯狂
    dealCards();

    // 房屋(The House)：开局第一手牌全部背面朝下。
    if (mBossEffect == BossEffect::TheHouse && !hasJokerType(JokerType::Chicot))
        for (CardData &c : mHand) c.faceUp = false;
    // 蔚蓝铃铛(Cerulean Bell)：随机锁定一张手牌为强制选中。
    if (mBossEffect == BossEffect::CeruleanBell && !hasJokerType(JokerType::Chicot))
        refreshCeruleanForced();

    applyBossDebuffs();

    emit handChanged();
    emit blindStarted();
}

void GameState::triggerBlindSelectJokers(BlindType type)
{
    // 普通(rarity 1)小丑池，供乌合之众创建
    static const QVector<JokerType> commonPool = {
        JokerType::Joker, JokerType::GreedyJoker, JokerType::LustyJoker,
        JokerType::WrathfulJoker, JokerType::GluttonousJoker, JokerType::JollyJoker,
        JokerType::ZanyJoker, JokerType::MadJoker, JokerType::CrazyJoker,
        JokerType::DrollJoker, JokerType::SlyJoker, JokerType::WilyJoker,
        JokerType::CleverJoker, JokerType::DeviousJoker, JokerType::CraftyJoker,
        JokerType::HalfJoker, JokerType::Banner, JokerType::MysticSummit,
        JokerType::RaisedFist, JokerType::Misprint, JokerType::ScaryFace,
        JokerType::SmileyFace, JokerType::EvenSteven, JokerType::OddTodd,
        JokerType::Scholar, JokerType::BlueJoker, JokerType::GreenJoker,
    };
    bool jokersDirty = false;

    // mJokers 会在循环中被修改，先对当前小丑类型做快照
    QVector<JokerType> present;
    for (const Joker &j : mJokers)
        if (!j.isDebuffed) present.append(j.type);

    for (JokerType jt : present) {
        switch (jt) {
        case JokerType::RiffRaff:
            for (int k = 0; k < 2 && canAddJoker(); ++k) {
                mJokers.append(createJoker(
                    commonPool[QRandomGenerator::global()->bounded(commonPool.size())]));
                jokersDirty = true;
            }
            break;
        case JokerType::MarbleJoker: {
            CardData c;
            c.suit = Suit::Spades; c.rank = Rank::Ace;
            c.enhancement = Enhancement::Stone;
            c.assignNewUid();
            mDeck.addCard(c);
            break;
        }
        case JokerType::Burglar:
            mHandsLeft += 3;
            mBlindStartingHands += 3;
            mDiscardLeft = 0;
            break;
        case JokerType::Cartomancer:
            addConsumable(randomTarotType());
            break;
        case JokerType::Certificate: {
            CardData c;
            c.suit = static_cast<Suit>(QRandomGenerator::global()->bounded(4));
            c.rank = static_cast<Rank>(2 + QRandomGenerator::global()->bounded(13));
            c.seal = static_cast<Seal>(1 + QRandomGenerator::global()->bounded(4));
            c.assignNewUid();
            mDeck.addCard(c);
            break;
        }
        default: break;
        }
    }

    // 疯狂：仅小/大盲注触发，+X0.5 并摧毁一张随机的非疯狂小丑
    if (type != BlindType::Boss) {
        bool hasMadness = false;
        for (Joker &j : mJokers)
            if (!j.isDebuffed && j.type == JokerType::Madness) {
                j.counter += 50;
                hasMadness = true;
            }
        if (hasMadness) {
            QVector<int> victims;
            for (int i = 0; i < mJokers.size(); ++i)
                if (mJokers[i].type != JokerType::Madness) victims.append(i);
            if (!victims.isEmpty()) {
                mJokers.removeAt(victims[QRandomGenerator::global()->bounded(victims.size())]);
                jokersDirty = true;
            }
        }
    }

    // 祭祀匕首：摧毁右侧小丑，永久获得其出售价值 ×2 的倍率
    for (int i = 0; i < mJokers.size(); ++i) {
        if (mJokers[i].isDebuffed || mJokers[i].type != JokerType::CeremonialDagger) continue;
        if (i + 1 < mJokers.size()) {
            mJokers[i].counter += 2 * qMax(0, mJokers[i + 1].sellValue);
            mJokers.removeAt(i + 1);
            jokersDirty = true;
        }
    }

    if (jokersDirty) { updateOwnedSellValues(); syncShopJokerRules(); emit jokersChanged(); }
}

void GameState::cleanupDepletedJokers()
{
    bool changed = false;
    for (int i = mJokers.size() - 1; i >= 0; --i) {
        const Joker &j = mJokers[i];
        bool destroy = (j.type == JokerType::Popcorn    && j.counter <= 0)
                    || (j.type == JokerType::Ramen      && j.counter <= 100)
                    || (j.type == JokerType::Seltzer    && j.counter <= 0)
                    || (j.type == JokerType::TurtleBean && j.counter <= 0);
        if (destroy) { mJokers.removeAt(i); changed = true; }
    }
    if (changed) { syncShopJokerRules(); emit jokersChanged(); emit shopChanged(); }
}

void GameState::refreshCeruleanForced()
{
    if (mBossEffect != BossEffect::CeruleanBell || hasJokerType(JokerType::Chicot)) {
        mCeruleanForcedUid = -1;
        return;
    }
    // 已锁定的牌仍在手牌里就保持不变，否则重新随机挑一张。
    for (const CardData &c : mHand)
        if (c.uid == mCeruleanForcedUid) return;
    if (mHand.isEmpty()) { mCeruleanForcedUid = -1; return; }
    mCeruleanForcedUid = mHand[QRandomGenerator::global()->bounded(mHand.size())].uid;
}

void GameState::nextBlind()
{
    mBlindStates[mBlindIdx] = BlindState::Defeated;
    mBlindIdx++;
    if (mBlindIdx > 2) {
        mBlindIdx = 0;
        mAnte++;
        mPendingBossEffect = BossEffect::None;
        mBossRerollsUsedThisAnte = 0;
        mCardsPlayedThisAnte.clear();   // 支柱：换 Ante 清空已打出记录
        prepareBlindTags();
        if (mAnte > 8 && !mEndlessMode) {
            // 通关 ante 8：弹出胜利结算；玩家可选择继续无尽模式。
            mPhase = GamePhase::GameOver;
            emit gameOver(true);
            return;
        }
    }
    // 不论是新 ante 还是同 ante 内推进,都重算所有状态
    for (int i = 0; i < 3; ++i) {
        if (i < mBlindIdx)       mBlindStates[i] = BlindState::Defeated;
        else if (i == mBlindIdx) mBlindStates[i] = BlindState::Current;
        else                     mBlindStates[i] = BlindState::Upcoming;
    }
    enterBlindSelect();
}

void GameState::continueEndless()
{
    // 仅在通关 ante 8（已进入胜利结算）后可调用：开启无尽模式并继续。
    if (mPhase != GamePhase::GameOver) return;
    if (mAnte <= 8) return;
    mEndlessMode = true;
    mBlindIdx = 0;
    for (int i = 0; i < 3; ++i)
        mBlindStates[i] = (i == 0) ? BlindState::Current : BlindState::Upcoming;
    enterBlindSelect();
}

void GameState::skipCurrentBlind()
{
    if (mPhase != GamePhase::BlindSelect) return;
    if (mBlindIdx >= 2) return;

    TagType gained = mBlindTags[mBlindIdx];

    // 必须在 applySkippedTag 之前 ++ skip 计数：原版 Skip Tag 公式
    // `G.GAME.skips * $5` 中的 G.GAME.skips 是包含本次跳过的，第一次跳过给 $5。
    mTotalSkipsThisRun++;

    // 原版 add_tag：先让已有 Double Tag 复制新获得的非 Double Tag，之后再加入原始 tag。
    int doublesToFire = (gained != TagType::Double) ? mPendingDoubleTags : 0;
    mLastSkippedTag = gained;
    mLastConsumedDoubleTags = doublesToFire;

    if (doublesToFire > 0) {
        mPendingDoubleTags = 0;
        for (int i = 0; i < doublesToFire; ++i) {
            mActiveTags.append(gained);
            // 与原版一致：复制出来的副本随后按它自己的 type 触发副作用（叠加：
            // immediate 类立刻发钱、shop 类增加缓存计数 N+1、edition 类多压一张到队列）。
            applySkippedTag(gained);
        }
    }

    mActiveTags.append(gained);
    applySkippedTag(gained);

    mBlindStates[mBlindIdx] = BlindState::Skipped;
    mBlindIdx++;
    mBlindStates[mBlindIdx] = BlindState::Current;

    // Batch 5：复古——每跳过 1 个盲注 +X0.25
    for (Joker &j : mJokers)
        if (!j.isDebuffed && j.type == JokerType::Throwback) j.counter += 1;

    mJustSkipped = true;        // ← 标记"这次进入是因跳过"
    enterBlindSelect();          // 同步发 blindSelectEntered → onBlindSelectEntered 会读 justSkipped()
    mJustSkipped = false;        // ← 信号处理完后立即复位
}


void GameState::applySkippedTag(TagType t, int recursionDepth)
{
    switch (t) {
    case TagType::Skip:
        // 原版 tag.lua:154 — ease_dollars((G.GAME.skips or 0)*skip_bonus($5))
        // 跳过次数 N（含本次）× $5；mTotalSkipsThisRun 在 skipCurrentBlind 已 ++ 含本次。
        addGold(qMax(1, mTotalSkipsThisRun) * 5);
        break;
    case TagType::Economy:
        // 原版 tag.lua:184 — ease_dollars(math.min(max=40, math.max(0, G.GAME.dollars)))
        // 即"金钱翻倍最多 +40"。加入 qMax(0, mGold) 防止负债时给负数。
        addGold(qMin(40, qMax(0, mGold)));
        break;
    case TagType::Handy:
        // 原版 tag.lua:172 — ease_dollars((G.GAME.hands_played or 0)*dollars_per_hand($1))
        addGold(mTotalHandsPlayedThisRun);
        break;
    case TagType::Garbage:
        // 原版 tag.lua:163 — ease_dollars((G.GAME.unused_discards or 0)*dollars_per_discard($1))
        addGold(mUnusedDiscardsThisRun);
        break;
    case TagType::Investment:
        mPendingInvestmentBonus += 25;
        break;
    case TagType::Voucher:
        // 计数式：Double Tag 可让同时缓存多张优惠券标签，下面 N 个商店各出一张。
        ++mTagVoucherPendingShops;
        mTagVoucherNextShop = true;
        break;
    case TagType::Coupon:
        // 计数式：缓存 N 次"下个商店初始价免费"。每进一次商店消耗一份。
        ++mTagCouponPendingShops;
        break;
    case TagType::Boss:
        mPendingBossEffect = randomBossEffect(mAnte);
        break;
    case TagType::Standard:
    case TagType::Charm:
    case TagType::Meteor:
    case TagType::Buffoon:
    case TagType::Ethereal:
        // 原版这些标签在 blind choice 立刻打开对应免费包，不等到商店。
        // MainWindow 会根据 lastSkippedTag 直接生成并打开包。
        break;
    case TagType::Juggle:
        mOneRoundHandSizeBonus += 3;
        break;
    case TagType::D6:
        // 原版 D6 Tag 只对下个商店第一次重摇免费。计数式：缓存 N 个商店各享受一次。
        ++mTagD6PendingShops;
        break;
    case TagType::Orbital: {
        HandType types[] = { HandType::HighCard, HandType::Pair, HandType::TwoPair,
                             HandType::ThreeOfAKind, HandType::Straight, HandType::Flush,
                             HandType::FullHouse, HandType::FourOfAKind, HandType::StraightFlush };
        levelUpHand(types[QRandomGenerator::global()->bounded(int(sizeof(types)/sizeof(*types)))], 3);
        break;
    }
    case TagType::Double:
        // 原版：Double Tag 不立刻产生新标签，而是"下一次获得任何非 Double 标签时复制它"。
        // 这里只增加挂起计数，复制由 skipCurrentBlind() 里的非 Double 分支处理。
        ++mPendingDoubleTags;
        break;
    case TagType::Negative:
        mShop.addPendingEditionJoker(Edition::Negative);
        break;
    case TagType::Foil:
        mShop.addPendingEditionJoker(Edition::Foil);
        break;
    case TagType::Holographic:
        mShop.addPendingEditionJoker(Edition::Holographic);
        break;
    case TagType::Polychrome:
        mShop.addPendingEditionJoker(Edition::Polychrome);
        break;
    case TagType::TopUp: {
        syncShopJokerRules();
        int created = 0;
        while (created < 2 && canAddJoker()) {
            QVector<JokerType> already = ownedJokerTypes();
            Joker j = createJoker(mShop.randomJokerForTag(already));
            mJokers.append(j);
            updateOwnedSellValues();
            ++created;
            syncShopJokerRules();
        }
        if (created > 0) {
            emit jokersChanged();
            emit shopChanged();
        }
        break;
    }
    case TagType::Uncommon:
        mShop.addPendingRarityJoker(JokerRarity::Uncommon);
        break;
    case TagType::Rare:
        mShop.addPendingRarityJoker(JokerRarity::Rare);
        break;
    }
}

void GameState::applyTagEffectsToShop()
{
    // Voucher Tag 的“出券”已由 Shop::roll() 通过 mAllowVoucherThisShop 处理，
    // 这里不再二次替换券槽——避免双重生成 / 错乱。
    // 仅保留 free-pack 标签（Standard/Charm/Meteor/Buffoon/Ethereal）的礼包置换。

    if (mHasTagFreePack && !mShop.boosterOffersMutable().isEmpty()) {
        mShop.setBoosterOfferPack(0, mTagFreePackKind, PackSize::Normal, true);
        mHasTagFreePack = false;
    }
    mShop.refreshCurrentOfferCosts();
}

void GameState::leaveShop()
{
    if (mPhase != GamePhase::Shop) return;
    triggerPerkeoLeavingShop();
    mShop.resetForNewBlind();
    syncShopJokerRules();
    nextBlind();
}

bool GameState::hasJokerType(JokerType t) const
{
    for (const Joker &j : mJokers) if (j.type == t) return true;
    return false;
}

bool GameState::bossBlocksPlayedHand(const HandResult &result, int playedCount)
{
    if (mBlindType != BlindType::Boss || hasJokerType(JokerType::Chicot)) return false;
    if (mBossEffect == BossEffect::ThePsychic && playedCount < 5) return true;
    if (mBossEffect == BossEffect::TheMouth) {
        if (mBossMouthHasHand && mBossMouthOnlyHand != result.type) return true;
        mBossMouthOnlyHand = result.type;
        mBossMouthHasHand = true;
    }
    if (mBossEffect == BossEffect::TheEye) {
        if (mBossEyePlayedHands.contains(result.type)) return true;
        mBossEyePlayedHands.insert(result.type);
    }
    return false;
}

bool GameState::canRerollBoss() const
{
    if (mPhase != GamePhase::BlindSelect || mBlindIdx > 2) return false;
    if (!hasVoucher(VoucherType::DirectorsCut) && !hasVoucher(VoucherType::Retcon)) return false;
    if (spendableGold() < 10) return false;
    if (hasVoucher(VoucherType::Retcon)) return true;
    return mBossRerollsUsedThisAnte == 0;
}

bool GameState::rerollBoss()
{
    if (!canRerollBoss()) return false;
    mGold -= 10;
    ++mBossRerollsUsedThisAnte;
    BossEffect old = mPendingBossEffect;
    for (int i = 0; i < 12; ++i) {
        mPendingBossEffect = randomBossEffect(mAnte);
        if (mPendingBossEffect != old) break;
    }
    mTargetScore = calcTargetScore();
    emit goldChanged();
    emit blindSelectEntered();
    return true;
}


static bool isFaceRankForLegendary(const CardData &card)
{
    return card.rank == Rank::Jack || card.rank == Rank::Queen || card.rank == Rank::King;
}

void GameState::notifyPlayingCardDestroyed(const CardData &card)
{
    // 原版 Caino：每摧毁 1 张人头牌，获得 X1 倍率；初始为 X1。
    if (hasJokerType(JokerType::Caino) && isFaceCard(card)) {
        mCainoXMult += 1.0;
        emit jokersChanged();
    }
}

void GameState::notifyDiscardedCardsForYorick(int count)
{
    // 原版 Yorick：每弃掉 23 张牌，获得 X1 倍率；初始为 X1。
    // UI 需要实时显示“还需要弃 [N/23]”，所以只要计数变化就发 jokersChanged。
    if (!hasJokerType(JokerType::Yorick) || count <= 0) return;
    int remaining = count;
    bool changed = false;
    while (remaining > 0) {
        if (remaining >= mYorickDiscardsRemaining) {
            remaining -= mYorickDiscardsRemaining;
            mYorickXMult += 1.0;
            mYorickDiscardsRemaining = 23;
            changed = true;
        } else {
            mYorickDiscardsRemaining -= remaining;
            remaining = 0;
            changed = true;
        }
    }
    if (changed) emit jokersChanged();
}


int GameState::jokerDynamicCounter(JokerType t) const
{
    for (const Joker &j : mJokers) if (j.type == t) return j.counter;
    return 0;
}

void GameState::processEndOfRoundJokerExtinctions()
{
    bool changed = false;
    for (int i = mJokers.size() - 1; i >= 0; --i) {
        Joker &j = mJokers[i];
        if (j.type == JokerType::GrosMichel) {
            if (QRandomGenerator::global()->bounded(6) == 0) {
                mJokers.removeAt(i);
                mGrosMichelExtinct = true;
                changed = true;
            }
        } else if (j.type == JokerType::Cavendish) {
            if (QRandomGenerator::global()->bounded(1000) == 0) {
                mJokers.removeAt(i);
                changed = true;
            }
        }
    }
    if (changed) {
        syncShopJokerRules();
        emit jokersChanged();
        emit shopChanged();
    }
}

void GameState::triggerPerkeoLeavingShop()
{
    // 原版 Perkeo 在离开商店时触发；Blueprint / Brainstorm 如果最终复制到 Perkeo，
    // 也应当额外触发一次。所以“蓝图 + 佩克欧”会复制 2 张负片消耗牌。
    if (mConsumables.isEmpty()) return;

    int triggerCount = 0;
    for (int ji = 0; ji < mJokers.size(); ++ji) {
        const Joker &j = mJokers[ji];
        if (j.isDebuffed) continue;
        const Joker *resolved = resolveCopiedJoker(mJokers, ji);
        if (resolved && !resolved->isDebuffed && resolved->type == JokerType::Perkeo)
            ++triggerCount;
    }
    if (triggerCount <= 0) return;

    for (int n = 0; n < triggerCount; ++n) {
        if (mConsumables.isEmpty()) break;
        Consumable copy = mConsumables[QRandomGenerator::global()->bounded(mConsumables.size())];
        copy.negative = true;
        copy.sellValue = qMax(copy.sellValue, 1);
        mConsumables.append(copy);       // 负片消耗牌自身提供 +1 消耗牌槽，因此允许满槽追加。
    }
    emit consumablesChanged();
}


bool GameState::addRandomRareJoker()
{
    if (!canAddJoker()) return false;
    QVector<JokerType> rare = {
        JokerType::Blueprint, JokerType::Brainstorm, JokerType::DNA,
        JokerType::Mime, JokerType::Baron, JokerType::Bloodstone,
        JokerType::DriversLicense, JokerType::Hologram, JokerType::Vampire,
        JokerType::Constellation, JokerType::MidasMask, JokerType::CardSharp,
        JokerType::SockAndBuskin, JokerType::Photograph, JokerType::TheDuo, JokerType::TheTrio,
        JokerType::TheFamily, JokerType::TheOrder, JokerType::TheTribe,
        JokerType::Obelisk, JokerType::Vagabond, JokerType::Campfire, JokerType::AncientJoker
    };
    if (!hasJokerDuplicateBypass()) {
        QVector<JokerType> owned = ownedJokerTypes();
        rare.erase(std::remove_if(rare.begin(), rare.end(), [&owned](JokerType t){ return owned.contains(t); }), rare.end());
    }
    if (rare.isEmpty()) return false;
    Joker j = createJoker(rare[QRandomGenerator::global()->bounded(rare.size())]);
    mJokers.append(j);
    updateOwnedSellValues();
    syncShopJokerRules();
    emit jokersChanged();
    emit shopChanged();
    return true;
}

bool GameState::duplicateRandomJokerAndDestroyOthers()
{
    if (mJokers.isEmpty()) return false;
    int keep = QRandomGenerator::global()->bounded(mJokers.size());
    Joker copy = mJokers[keep];
    mJokers.clear();
    mJokers.append(copy);
    if (canAddJoker()) mJokers.append(copy);
    syncShopJokerRules();
    emit jokersChanged();
    emit shopChanged();
    return true;
}

bool GameState::setRandomEditionlessJoker(Edition e, bool destroyOthers, bool reduceHandSize)
{
    QVector<int> candidates;
    for (int i = 0; i < mJokers.size(); ++i) {
        if (mJokers[i].edition == Edition::None) candidates.append(i);
    }
    if (candidates.isEmpty()) return false;
    int chosen = candidates[QRandomGenerator::global()->bounded(candidates.size())];
    Joker selected = mJokers[chosen];
    selected.edition = e;
    if (destroyOthers) {
        mJokers.clear();
        mJokers.append(selected);
    } else {
        mJokers[chosen] = selected;
    }
    updateOwnedSellValues();
    if (reduceHandSize) addPermanentHandSizeBonus(-1);
    syncShopJokerRules();
    emit jokersChanged();
    emit shopChanged();
    return true;
}

void GameState::addPermanentHandSizeBonus(int delta)
{
    mExtraHandSize += delta;
    if (mExtraHandSize < -6) mExtraHandSize = -6;
    emit handChanged();
}

void GameState::immolateRandomHandCards(int destroyCount, int goldGain)
{
    if (mHand.isEmpty()) return;
    QVector<int> idx;
    for (int i = 0; i < mHand.size(); ++i) idx.append(i);
    for (int i = idx.size() - 1; i > 0; --i) {
        int j = QRandomGenerator::global()->bounded(i + 1);
        idx.swapItemsAt(i, j);
    }
    int n = qMin(destroyCount, idx.size());
    idx = idx.mid(0, n);
    std::sort(idx.begin(), idx.end(), std::greater<int>());
    for (int i : idx) {
        if (i < 0 || i >= mHand.size()) continue;
        notifyPlayingCardDestroyed(mHand[i]);
        mHand.removeAt(i);
    }
    if (n > 0) addGold(goldGain);
    emit handChanged();
}

bool GameState::addRandomLegendaryJoker()
{
    if (!canAddJoker()) return false;
    QVector<JokerType> legends = {
        JokerType::Caino, JokerType::Triboulet, JokerType::Yorick,
        JokerType::Chicot, JokerType::Perkeo
    };
    if (!hasJokerDuplicateBypass()) {
        legends.erase(std::remove_if(legends.begin(), legends.end(), [this](JokerType t){
            return ownedJokerTypes().contains(t);
        }), legends.end());
    }
    if (legends.isEmpty()) legends = { JokerType::Caino, JokerType::Triboulet, JokerType::Yorick, JokerType::Chicot, JokerType::Perkeo };
    Joker j = createJoker(legends[QRandomGenerator::global()->bounded(legends.size())]);
    mJokers.append(j);
    updateOwnedSellValues();
    syncShopJokerRules();
    emit jokersChanged();
    return true;
}

void GameState::levelUpAllHands(int times)
{
    if (mDryRun) return;
    const HandType all[] = {
        HandType::HighCard, HandType::Pair, HandType::TwoPair, HandType::ThreeOfAKind,
        HandType::Straight, HandType::Flush, HandType::FullHouse, HandType::FourOfAKind,
        HandType::StraightFlush, HandType::RoyalFlush, HandType::FiveOfAKind,
        HandType::FlushHouse, HandType::FlushFive
    };
    // 直接改 mHandLevels 而非走 13 次 levelUpHand+emit；
    // 这样 onHandLevelsChanged 只触发一次，能识别"多手同时升级"走 All Hands 演出。
    for (HandType t : all) {
        HandLevel &lv = mHandLevels[t];
        auto d = handLevelDelta(t);
        for (int i = 0; i < times; ++i) {
            lv.level++;
            lv.chipsBonus += d.first;
            lv.multBonus  += d.second;
        }
    }
    // Black Hole counts as one Planet card use for Constellation; callers record that once.
    emit handLevelsChanged();
}

HandResult GameState::previewSelection(const QVector<int> &indices) const
{
    QVector<CardData> selected;
    for (int i : indices) {
        if (i >= 0 && i < mHand.size())
            selected.append(mHand[i]);
    }
    HandResult r = HandEvaluator::preview(selected, currentHandMods());
    // 加上牌型升级加成(这部分玩家应该看到)
    auto it = mHandLevels.find(r.type);
    if (it != mHandLevels.end()) {
        r.chips += it->chipsBonus;
        r.mult  += it->multBonus;
        r.level  = it->level;
    }
    return r;
}
