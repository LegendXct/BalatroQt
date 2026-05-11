#include "gamestate.h"
#include <QSet>
#include <QRandomGenerator>

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
    HandLevel &lv = mHandLevels[t];
    auto d = handLevelDelta(t);
    for (int i = 0; i < times; ++i) {
        lv.level++;
        lv.chipsBonus += d.first;
        lv.multBonus  += d.second;
    }
    emit handLevelsChanged();
}

static auto rankComp = [](const CardData &a, const CardData &b) {
    if (a.rank != b.rank) return static_cast<int>(a.rank) > static_cast<int>(b.rank);
    return static_cast<int>(a.suit) < static_cast<int>(b.suit);
};

static auto suitComp = [](const CardData &a, const CardData &b) {
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

void GameState::dealCards() {
    while (mHand.size() < Constants::HAND_SIZE && !mDeck.isEmpty())
        mHand.append(mDeck.draw());
    if (mSortMode == HandSortMode::ByRank)
        std::sort(mHand.begin(), mHand.end(), rankComp);
    else
        std::sort(mHand.begin(), mHand.end(), suitComp);
    emit handChanged();
}

void GameState::playCards(const QVector<int> &indices) {
    if (indices.isEmpty() || indices.size() > Constants::MAX_PLAY) return;
    if (mHandsLeft <= 0) return;

    // 取出 played
    QVector<CardData> played;
    for (int i : indices) played.append(mHand[i]);

    // 评分
    HandResult result = HandEvaluator::evaluate(played);

    // 牌型等级加成
    HandLevel &lv = mHandLevels[result.type];
    result.chips += lv.chipsBonus;
    result.mult  += lv.multBonus;
    result.level  = lv.level;
    result.xmult  = 1.0;

    result.baseChips = result.chips;    // ← 缓存:基础 + 等级,没加 enhancement/joker
    result.baseMult  = result.mult;

    // 找 scoringCards 在 played 中的下标（按字段全匹配，第一次未占用）
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
    std::sort(scoringPlayedIdx.begin(), scoringPlayedIdx.end());

    // Glass 破碎标记（按 played 下标）
    QVector<bool> shattered(played.size(), false);

    // 逐张计分牌评分（Red Seal 重触发整段）
    for (int playedIdx : scoringPlayedIdx) {
        const CardData &card = played[playedIdx];
        if (card.isDebuffed) continue;

        int triggers = (card.seal == Seal::Red) ? 2 : 1;
        for (int t = 0; t < triggers; ++t)
            scoreCard(card, result, playedIdx);

        // Glass 破碎只判一次（不因 Red Seal 翻倍）
        if (card.enhancement == Enhancement::Glass
            && QRandomGenerator::global()->bounded(4) == 0) {
            shattered[playedIdx] = true;
        }
    }

    // 持有的 Steel 牌（在手牌但未参与计分）→ xmult ×1.5
    QSet<int> scoringHandIdx;
    for (int p : scoringPlayedIdx) scoringHandIdx.insert(indices[p]);
    for (int i = 0; i < mHand.size(); ++i) {
        if (scoringHandIdx.contains(i)) continue;
        const CardData &c = mHand[i];
        if (c.isDebuffed) continue;
        if (c.enhancement == Enhancement::Steel) {
            result.xmult *= 1.5;
            result.events.append({ ScoreEventKind::SteelXMult, -1, i, -1, 0, 1.5 });
        }
    }

    {
        int chipsBefore = result.chips, multBefore = result.mult;
        double xmultBefore = result.xmult;

        TriggerContext ctx{ result, *this, mHand, result.scoringCards, nullptr };
        for (int ji = 0; ji < mJokers.size(); ++ji) {
            const Joker &j = mJokers[ji];
            if (j.isDebuffed) continue;
            if (j.timing != TriggerTiming::Passive
                && j.timing != TriggerTiming::OnPlayedHand) continue;
            j.effect(ctx);

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

    // 最终得分
    int gained = static_cast<int>(result.chips * result.mult * result.xmult);

    mLastResult = result;
    emit handPlayed();

    mScore += gained;
    mHandsLeft--;

    lv.played++;

    // 移除手牌：破碎的不放回 deck
    QVector<QPair<int,int>> idxPairs;   // {handIdx, playedIdx}
    for (int p = 0; p < indices.size(); ++p)
        idxPairs.append({ indices[p], p });
    std::sort(idxPairs.begin(), idxPairs.end(),
              [](const auto &a, const auto &b){ return a.first > b.first; });

    for (const auto &pr : idxPairs) {
        if (!shattered[pr.second]) mDeck.discard(mHand[pr.first]);
        mHand.removeAt(pr.first);
    }

    applyBossPostPlay();
    dealCards();
    applyBossDebuffs();

    emit scoreChanged();
    emit goldChanged();

    // 通关
    if (mScore >= mTargetScore) {
        // Gold 增强：手牌中每张 +$3
        for (const CardData &c : mHand)
            if (c.enhancement == Enhancement::Gold && !c.isDebuffed) addGold(3);

        // Blue Seal：手牌中每张 → 生成对应行星
        for (const CardData &c : mHand) {
            if (c.seal == Seal::Blue && !c.isDebuffed && canAddConsumable())
                addConsumable(planetTypeFor(result.type));
        }

        // ── 之后照原来的通关流程 ──
        int blindReward = 0;
        switch (mBlindType) {
        case BlindType::Small: blindReward = 3; break;
        case BlindType::Big:   blindReward = 4; break;
        case BlindType::Boss:  blindReward = 5; break;
        }
        int handBonus = mHandsLeft * Constants::HAND_GOLD;
        mGold += blindReward + handBonus;

        // OnRoundEnd 小丑（黄金小丑 +$4 等）
        HandResult dummy{};
        TriggerContext rctx{ dummy, *this, mHand, mHand, nullptr };
        for (const Joker &j : mJokers) {
            if (!j.isDebuffed && j.timing == TriggerTiming::OnRoundEnd)
                j.effect(rctx);
        }

        int interest = qMin(mGold / 5, Constants::INTEREST_MAX);
        mGold += interest;

        mPhase = GamePhase::Shop;
        mShop.roll();
        if (mFirstShop) {
            auto &b = mShop.boosterOffersMutable();
            if (b.size() >= 1) {
                ShopOffer &o = b[0];
                o.kind = OfferKind::Pack;
                o.pack = PackKind::Buffoon;
                o.cost = 4;
                o.sold = false;
            }
            mFirstShop = false;
        }
        emit goldChanged();
        emit roundWon(blindReward, handBonus, interest);
        return;
    }

    checkGameOver();
}

void GameState::discardCards(const QVector<int> &indices)
{
    if (indices.isEmpty()) return;
    if (mDiscardLeft <= 0) return;

    QVector<CardData> discarded;
    for (int i : indices) discarded.append(mHand[i]);

    // OnDiscard 小丑触发（当前没有这种类型的小丑,留着挂钩用）
    HandResult dummy{};
    TriggerContext ctx{ dummy, *this, mHand, discarded, nullptr };
    for (const Joker &j : mJokers) {
        if (j.isDebuffed) continue;
        if (j.timing == TriggerTiming::OnDiscard) j.effect(ctx);
    }

    // Purple Seal：被弃的每张 → 生成随机塔罗
    for (const CardData &c : discarded) {
        if (c.seal == Seal::Purple && !c.isDebuffed && canAddConsumable())
            addConsumable(randomTarotType());
    }

    QVector<int> sorted = indices;
    std::sort(sorted.begin(), sorted.end(), std::greater<int>());
    for (int i : sorted) {
        mDeck.discard(mHand[i]);
        mHand.removeAt(i);
    }

    mDiscardLeft--;
    dealCards();
    applyBossDebuffs();

    emit handChanged();
}

int GameState::calcTargetScore() const {
    const int baseScores[] = { 0, 300, 800, 2000, 5000, 11000, 20000, 35000, 50000 };
    double mult = 1.0;
    switch (mBlindType) {
    case BlindType::Small: mult = Constants::SMALL_BLIND_MULT; break;
    case BlindType::Big:   mult = Constants::BIG_BLIND_MULT;   break;
    case BlindType::Boss:  mult = Constants::BOSS_BLIND_MULT;  break;
    }
    int target = static_cast<int>(baseScores[mAnte] * mult);
    if (mBossEffect == BossEffect::TheWall) target *= 2;   // ← 关键
    return target;
}

int GameState::jokerSlots() const {
    int extra = 0;
    for (const CardData &c : mHand)
        if (c.edition == Edition::Negative) extra++;
    return Constants::MAX_JOKER_SLOTS + extra;
}

void GameState::applyCardEnhancements(HandResult &result) {
    for (const CardData &c : result.scoringCards) {
        if (c.isDebuffed) continue;

        // 先加筹码
        result.chips += c.chipValue();

        switch (c.enhancement) {
        case Enhancement::Bonus: result.chips += 30; break;
        case Enhancement::Mult: result.mult  += 4; break;
        case Enhancement::Glass: result.mult  *= 2; break;
        case Enhancement::Stone: result.chips += 50; break;
        default: break;
        }

        switch (c.edition) {
        case Edition::Foil: result.chips += 50; break;
        case Edition::Holographic: result.mult += 10; break;
        case Edition::Polychrome: result.mult = static_cast<int>(result.mult * 1.5); break;
        default: break;
        }
    }
}

void GameState::applyJokerEffects(HandResult &result) {
    TriggerContext ctx{
        result, *this, mHand, result.scoringCards, nullptr
    };

    for (const Joker &j : mJokers) {
        if (j.isDebuffed) continue;

        if (j.timing == TriggerTiming::Passive ||
            j.timing == TriggerTiming::OnPlayedHand) {
            j.effect(ctx);
        }

        if (j.timing == TriggerTiming::OnScoringCard) {
            for (const CardData &card : result.scoringCards) {
                ctx.currentCard = &card;
                j.effect(ctx);
            }
            ctx.currentCard = nullptr;
        }
    }
}

void GameState::checkGameOver() {
    if (mPhase != GamePhase::Blind) return;       // ← 新增：不在打盲注阶段就不判断
    if (mHandsLeft <= 0 && mScore < mTargetScore) {
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
    int interest = qMin(mGold / 5, Constants::INTEREST_MAX);
    return base + handBonus + interest;
}

void GameState::rerollShop() {
    if (mPhase != GamePhase::Shop) return;
    int cost = mShop.rerollCost();
    if (mGold < cost) return;

    mGold -= cost;
    mShop.onReroll();
    mShop.rerollShopOnly();   // ← 只 reroll 商品区,不动 booster

    emit goldChanged();
    emit shopChanged();
}

void GameState::applyBossDebuffs() {
    for (CardData &c : mHand) {
        c.isDebuffed = false;   // 清掉旧标记
        if (mBossEffect == BossEffect::TheClub  && c.suit == Suit::Clubs)
            c.isDebuffed = true;
        if (mBossEffect == BossEffect::ThePlant
            && (c.rank == Rank::Jack || c.rank == Rank::Queen || c.rank == Rank::King))
            c.isDebuffed = true;
    }
}

void GameState::applyBossPostPlay() {
    if (mBossEffect == BossEffect::TheHook && mHand.size() >= 2) {
        // 随机弃 2 张
        for (int i = 0; i < 2 && !mHand.isEmpty(); ++i) {
            int idx = QRandomGenerator::global()->bounded(mHand.size());
            mDeck.discard(mHand[idx]);
            mHand.removeAt(idx);
        }
    }
}

bool GameState::addConsumable(ConsumableType t) {
    if (!canAddConsumable()) return false;
    mConsumables.append(createConsumable(t));
    emit consumablesChanged();
    return true;
}

bool GameState::useConsumable(int idx, const QVector<int> &selectedHandIdx) {
    if (mPhase == GamePhase::GameOver) return false;
    if (idx < 0 || idx >= mConsumables.size()) return false;

    const Consumable &c = mConsumables[idx];
    if (c.needsSelection > 0 &&
        selectedHandIdx.size() < c.needsSelection) return false;

    QVector<int> sel = selectedHandIdx;
    std::sort(sel.begin(), sel.end());
    if (c.maxSelection > 0 && sel.size() > c.maxSelection)
        sel.resize(c.maxSelection);

    UseContext ctx{ *this, sel };
    c.effect(ctx);

    mConsumables.removeAt(idx);
    emit consumablesChanged();
    return true;
}

bool GameState::sellConsumable(int idx) {
    if (idx < 0 || idx >= mConsumables.size()) return false;
    int v = mConsumables[idx].sellValue;
    mConsumables.removeAt(idx);
    mGold += v;
    emit consumablesChanged();
    emit goldChanged();
    return true;
}

bool GameState::buyShopOffer(int idx) {
    if (mPhase != GamePhase::Shop) return false;
    if (!mShop.canBuyShop(idx, mGold)) return false;
    const ShopOffer &o = mShop.shopOffers()[idx];

    if (o.kind == OfferKind::Joker) {
        if (!canAddJoker()) return false;
        ShopOffer t = mShop.takeShopOffer(idx);
        mGold -= t.cost;
        mJokers.append(createJoker(t.joker));
        emit jokersChanged();
    } else {
        if (!canAddConsumable()) return false;
        ShopOffer t = mShop.takeShopOffer(idx);
        mGold -= t.cost;
        mConsumables.append(createConsumable(t.consumable));
        emit consumablesChanged();
    }
    emit goldChanged();
    emit shopChanged();
    return true;
}

void GameState::scoreCard(const CardData &card, HandResult &result, int playedIdx)
{
    // 1) chip:Stone 不加点数 chip
    if (card.enhancement != Enhancement::Stone) {
        int v = card.chipValue();
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
    case Enhancement::Lucky:
        if (QRandomGenerator::global()->bounded(5) == 0) {
            result.mult += 20;
            result.events.append({ ScoreEventKind::EnhancementMult, playedIdx, -1, -1, 20, 1.0 });
        }
        if (QRandomGenerator::global()->bounded(15) == 0) addGold(20);
        break;
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
    int chipsBefore = result.chips, multBefore = result.mult;
    double xmultBefore = result.xmult;

    for (int ji = 0; ji < mJokers.size(); ++ji) {
        const Joker &j = mJokers[ji];
        if (j.isDebuffed) continue;
        if (j.timing != TriggerTiming::OnScoringCard) continue;

        TriggerContext ctx{ result, *this, mHand, result.scoringCards, &card };
        j.effect(ctx);

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
    if (card.seal == Seal::Gold) addGold(3);
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
    if (!mShop.canBuyBooster(idx, mGold)) return false;
    const ShopOffer &o = mShop.boosterOffers()[idx];
    if (o.kind != OfferKind::Pack) return false;

    ShopOffer t = mShop.takeBoosterOffer(idx);
    mGold -= t.cost;
    out = generatePackContent(t.pack);

    emit goldChanged();
    emit shopChanged();
    return true;
}

void GameState::applyPackChoice(const PackContent &pack, int chosenIdx)
{
    if (chosenIdx < 0) return;

    switch (pack.kind) {
    case PackKind::Standard:
        if (chosenIdx >= pack.standardCards.size()) return;
        mDeck.addCard(pack.standardCards[chosenIdx]);
        break;
    case PackKind::Arcana:
    case PackKind::Celestial:
        if (chosenIdx >= pack.consumables.size()) return;
        if (!canAddConsumable()) return;
        mConsumables.append(createConsumable(pack.consumables[chosenIdx]));
        emit consumablesChanged();
        break;
    case PackKind::Buffoon:
        if (chosenIdx >= pack.jokers.size()) return;
        if (!canAddJoker()) return;
        mJokers.append(createJoker(pack.jokers[chosenIdx]));
        emit jokersChanged();
        break;
    }
}

void GameState::startGame()
{
    mGold = Constants::INITIAL_GOLD;
    mAnte = 1;
    mScore = 0;
    mJokers.clear();
    mShop = Shop();
    mHandLevels.clear();
    mConsumables.clear();
    mBlindIdx = 0;
    mPendingBossEffect = BossEffect::None;
    mSortMode = HandSortMode::ByRank;

    for (int i = 0; i < 3; ++i)
        mBlindStates[i] = (i == 0) ? BlindState::Current : BlindState::Upcoming;

    mFirstShop = true;
    enterBlindSelect();

    emit jokersChanged();
    emit consumablesChanged();
}

void GameState::enterBlindSelect() {
    mPhase = GamePhase::BlindSelect;
    mBlindType = static_cast<BlindType>(mBlindIdx);
    if (mPendingBossEffect == BossEffect::None)   // ← 去掉 mBlindIdx == 2 条件
        mPendingBossEffect = randomBossEffect();
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
    mHandsLeft = Constants::INITIAL_HANDS;
    mDiscardLeft = Constants::INITIAL_DISCARDS;

    if (type == BlindType::Boss) {
        mBossEffect = mPendingBossEffect;     // 用预先确定的
        if (mBossEffect == BossEffect::TheNeedle) mHandsLeft = 1;
    } else {
        mBossEffect = BossEffect::None;
    }

    mTargetScore = calcTargetScore();
    mPhase = GamePhase::Blind;

    mDeck.reset();
    mHand.clear();
    dealCards();
    applyBossDebuffs();

    emit handChanged();
    emit blindStarted();
}

void GameState::nextBlind()
{
    mBlindStates[mBlindIdx] = BlindState::Defeated;
    mBlindIdx++;
    if (mBlindIdx > 2) {
        mBlindIdx = 0;
        mAnte++;
        mPendingBossEffect = BossEffect::None;
        if (mAnte > 8) {
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

void GameState::skipCurrentBlind()
{
    if (mPhase != GamePhase::BlindSelect) return;
    if (mBlindIdx >= 2) return;

    mBlindStates[mBlindIdx] = BlindState::Skipped;
    mBlindIdx++;
    mBlindStates[mBlindIdx] = BlindState::Current;

    mJustSkipped = true;        // ← 标记"这次进入是因跳过"
    enterBlindSelect();          // 同步发 blindSelectEntered → onBlindSelectEntered 会读 justSkipped()
    mJustSkipped = false;        // ← 信号处理完后立即复位
}

void GameState::leaveShop()
{
    if (mPhase != GamePhase::Shop) return;
    mShop.resetForNewBlind();
    nextBlind();
}

HandResult GameState::previewSelection(const QVector<int> &indices) const
{
    QVector<CardData> selected;
    for (int i : indices) {
        if (i >= 0 && i < mHand.size())
            selected.append(mHand[i]);
    }
    HandResult r = HandEvaluator::preview(selected);
    // 加上牌型升级加成(这部分玩家应该看到)
    auto it = mHandLevels.find(r.type);
    if (it != mHandLevels.end()) {
        r.chips += it->chipsBonus;
        r.mult  += it->multBonus;
        r.level  = it->level;
    }
    return r;
}
