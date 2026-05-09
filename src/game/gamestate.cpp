#include "gamestate.h"

GameState::GameState(QObject *parent)
    : QObject{parent}
{
    startGame();
}

void GameState::startGame() {
    mGold = Constants::INITIAL_GOLD;
    mAnte = 1;
    mScore = 0;
    mPhase = GamePhase::Blind;
    mJokers.clear();
    mShop = Shop();
    startBlind(BlindType::Small);
    emit jokersChanged();
}

void GameState::startBlind(BlindType type) {
    mBlindType = type;
    mScore = 0;
    mHandsLeft = Constants::INITIAL_HANDS;
    mDiscardLeft = Constants::INITIAL_DISCARDS;
    mTargetScore = calcTargetScore();
    mPhase = GamePhase::Blind;

    mDeck.reset();
    mHand.clear();
    dealCards();
}

void GameState::dealCards() {
    while (mHand.size() < Constants::HAND_SIZE && !mDeck.isEmpty())
        mHand.append(mDeck.draw());
    emit handChanged();
}

void GameState::playCards(const QVector<int> &indices) {
    if (indices.isEmpty() || indices.size() > Constants::MAX_PLAY) return;
    if (mHandsLeft <= 0) return;

    QVector<CardData> played;
    for (int i : indices) played.append(mHand[i]);

    HandResult result = HandEvaluator::evaluate(played);

    applyCardEnhancements(result);
    applyJokerEffects(result);

    mLastResult = result;
    emit handPlayed();

    int gained = result.chips * result.mult;
    mScore += gained;
    mHandsLeft--;

    QVector<int> sorted = indices;
    std::sort(sorted.begin(), sorted.end(), std::greater<int>());
    for (int i : sorted) {
        mDeck.discard(mHand[i]);
        mHand.removeAt(i);
    }

    dealCards();
    emit scoreChanged();
    emit goldChanged();

    if (mScore >= mTargetScore) {
        // ── 通关：发奖、触发 OnRoundEnd 小丑、初始化商店 ──
        int blindReward = 0;
        switch (mBlindType) {
        case BlindType::Small: blindReward = 3; break;
        case BlindType::Big:   blindReward = 4; break;
        case BlindType::Boss:  blindReward = 5; break;
        }
        int handBonus = mHandsLeft * Constants::HAND_GOLD;

        mGold += blindReward + handBonus;

        // 触发 OnRoundEnd 类小丑（黄金小丑 +4 之类）
        HandResult dummy{};
        TriggerContext ctx{ dummy, *this, mHand, mHand, nullptr };
        for (const Joker &j : mJokers) {
            if (!j.isDebuffed && j.timing == TriggerTiming::OnRoundEnd)
                j.effect(ctx);
        }

        int interest = qMin(mGold / 5, Constants::INTEREST_MAX);
        mGold += interest;

        mPhase = GamePhase::Shop;
        mShop.roll();                                 // ← 阶段 2：初始化商店
        emit goldChanged();
        emit roundWon(blindReward, handBonus, interest);
        return;                                       // ← 关键：必须 return
    }

    checkGameOver();   // 失败检测，只在没通关时执行一次
}

void GameState::discardCards(const QVector<int> &indices) {
    if (indices.isEmpty()) return;
    if (mDiscardLeft <= 0) return;

    QVector<int> sorted = indices;
    std::sort(sorted.begin(), sorted.end(), std::greater<int>());
    for (int i : sorted) {
        mDeck.discard(mHand[i]);
        mHand.removeAt(i);
    }

    mDiscardLeft--;
    dealCards();

    emit handChanged();
}

void GameState::nextBlind() {
    switch (mBlindType) {
    case BlindType::Small:
        startBlind(BlindType::Big);   break;
    case BlindType::Big:
        startBlind(BlindType::Boss);  break;
    case BlindType::Boss:
        mAnte++;
        if (mAnte > 8) {
            mPhase = GamePhase::GameOver;
            emit gameOver(true);      // 通关
        } else {
            startBlind(BlindType::Small);
        }
        break;
    }
}

int GameState::calcTargetScore() const {
    // 基础目标分数随 ante 递增
    const int baseScores[] = {
        0, 300, 800, 2000, 5000,
        11000, 20000, 35000, 50000
    };
    double mult = 1.0;
    switch (mBlindType) {
    case BlindType::Small: mult = Constants::SMALL_BLIND_MULT; break;
    case BlindType::Big: mult = Constants::BIG_BLIND_MULT; break;
    case BlindType::Boss: mult = Constants::BOSS_BLIND_MULT; break;
    }
    return static_cast<int>(baseScores[mAnte] * mult);
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
void GameState::sortHandByRank() {
    std::sort(mHand.begin(), mHand.end(),
              [](const CardData &a, const CardData &b){
                  return static_cast<int>(a.rank) > static_cast<int>(b.rank);
              });
    emit handChanged();
}

void GameState::sortHandBySuit() {
    std::sort(mHand.begin(), mHand.end(),
              [](const CardData &a, const CardData &b){
                  if (a.suit != b.suit)
                      return static_cast<int>(a.suit) < static_cast<int>(b.suit);
                  return static_cast<int>(a.rank) > static_cast<int>(b.rank);
              });
    emit handChanged();
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

bool GameState::buyJoker(int idx) {
    if (mPhase != GamePhase::Shop) return false;
    if (!mShop.canBuy(idx, mGold)) return false;
    if (!canAddJoker()) return false;

    ShopOffer o = mShop.takeOffer(idx);
    mGold -= o.cost;
    mJokers.append(createJoker(o.type));

    emit goldChanged();
    emit jokersChanged();
    emit shopChanged();
    return true;
}

void GameState::rerollShop() {
    if (mPhase != GamePhase::Shop) return;
    int cost = mShop.rerollCost();
    if (mGold < cost) return;

    mGold -= cost;
    mShop.onReroll();
    mShop.roll();

    emit goldChanged();
    emit shopChanged();
}

void GameState::leaveShop() {
    if (mPhase != GamePhase::Shop) return;
    mShop.resetForNewBlind();
    nextBlind();   // 内部会发 handChanged
}
