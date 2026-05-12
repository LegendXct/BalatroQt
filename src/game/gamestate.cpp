#include "gamestate.h"
#include <QSet>
#include <QRandomGenerator>
#include <algorithm>
#include <functional>

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


int GameState::deckTotal() const
{
    return mDeck.totalKnown() + mHand.size();
}

QVector<CardData> GameState::remainingDeckCards() const
{
    return mDeck.drawPile();
}

QVector<CardData> GameState::fullDeckCards() const
{
    QVector<CardData> out = mDeck.allKnownCards();
    for (CardData c : mHand) {
        c.isDebuffed = false;
        c.faceUp = true;
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

bool GameState::hasJokerDuplicateBypass() const
{
    // 原版 Showman/马戏团长允许重复小丑。当前项目还没有实现 Showman，
    // 所以这里先恒为 false；以后加 JokerType::Showman 时，在这里返回 true。
    return false;
}

void GameState::syncShopJokerRules()
{
    mShop.setOwnedJokers(ownedJokerTypes(), hasJokerDuplicateBypass());
}

void GameState::dealCards() {
    while (mHand.size() < handSize() && !mDeck.isEmpty())
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

        int interest = qMin(mGold / 5, mInterestCap / 5);
        mGold += interest;

        // 注意：这里不要立刻清空最后一手牌。原版是在结算界面仍然能看到最后一手，
        // 点“下一回合”时再播放收牌动画并把牌归还牌组。
        mPhase = GamePhase::Shop;
        syncShopJokerRules();
        mShop.roll();
        if (mFirstShop) {
            auto &b = mShop.boosterOffersMutable();
            if (b.size() >= 1) {
                ShopOffer &o = b[0];
                o.kind = OfferKind::Pack;
                o.pack = PackKind::Buffoon;
                o.packSize = PackSize::Normal;
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
    int interest = qMin(mGold / 5, mInterestCap / 5);
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
    sel.erase(std::unique(sel.begin(), sel.end()), sel.end());
    if (c.maxSelection > 0 && sel.size() > c.maxSelection)
        return false;

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

bool GameState::sellJoker(int idx) {
    if (idx < 0 || idx >= mJokers.size()) return false;
    int v = qMax(1, mJokers[idx].sellValue);
    mJokers.removeAt(idx);
    mGold += v;
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
    if (!mShop.canBuyShop(idx, mGold)) return false;
    const ShopOffer &o = mShop.shopOffers()[idx];

    if (o.kind == OfferKind::Joker) {
        if (!canAddJoker()) return false;
        ShopOffer t = mShop.takeShopOffer(idx);
        mGold -= t.cost;
        mJokers.append(createJoker(t.joker));
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
    } else {
        return false;
    }

    emit goldChanged();
    emit shopChanged();
    return true;
}

bool GameState::buyVoucherOffer(int idx) {
    if (mPhase != GamePhase::Shop) return false;
    if (!mShop.canBuyVoucher(idx, mGold)) return false;

    ShopOffer t = mShop.takeVoucherOffer(idx);
    mGold -= t.cost;
    mRedeemedVouchers.append(t.voucher);
    mShop.setRedeemedVouchers(mRedeemedVouchers);
    applyVoucher(t.voucher);
    syncShopJokerRules();
    mShop.refreshCurrentOfferCosts();
    mShop.ensureShopOfferCount();

    emit goldChanged();
    emit shopChanged();
    emit consumablesChanged();
    return true;
}

bool GameState::hasVoucher(VoucherType t) const {
    return mRedeemedVouchers.contains(t);
}

void GameState::applyVoucher(VoucherType t) {
    switch (t) {
    case VoucherType::Overstock:
        mShop.changeShopSlots(1);
        mShop.rerollShopOnly();
        break;
    case VoucherType::ClearanceSale:
        mShop.setDiscountPercent(25);
        break;
    case VoucherType::RerollSurplus:
        mShop.setRerollDiscount(2);
        break;
    case VoucherType::TarotMerchant:
        mShop.setTarotRate(9.6);
        break;
    case VoucherType::PlanetMerchant:
        mShop.setPlanetRate(9.6);
        break;
    case VoucherType::MagicTrick:
        // 原版 v_magic_trick: playing_card_rate = 4
        mShop.setPlayingCardRate(4.0);
        break;
    case VoucherType::CrystalBall:
        mExtraConsumableSlots += 1;
        break;
    case VoucherType::Grabber:
        mExtraHandsPerRound += 1;
        break;
    case VoucherType::Wasteful:
        mExtraDiscardsPerRound += 1;
        break;
    case VoucherType::SeedMoney:
        mInterestCap = 50;
        break;
    case VoucherType::PaintBrush:
        mExtraHandSize += 1;
        break;
    case VoucherType::Hieroglyph:
        mAnte = qMax(1, mAnte - 1);
        mExtraHandsPerRound -= 1;
        break;
    case VoucherType::Hone:
    case VoucherType::Telescope:
    case VoucherType::Blank:
    case VoucherType::DirectorsCut:
        // 这些需要和版本概率 / Boss 重掷 / 包偏向进一步联动，先登记为已购买。
        break;
    }
}

int GameState::consumableSlots() const {
    return Constants::MAX_CONSUMABLE_SLOTS + mExtraConsumableSlots;
}

int GameState::handSize() const {
    return Constants::HAND_SIZE + mExtraHandSize;
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
    out = generatePackContent(t.pack, t.packSize, false, hasVoucher(VoucherType::Telescope),
                              ConsumableType::Planet_Pluto,
                              ownedJokerTypes(), hasJokerDuplicateBypass());

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

    switch (type) {
    case ConsumableType::Tarot_Empress:    enhance(Enhancement::Mult, 2); break;
    case ConsumableType::Tarot_Hierophant: enhance(Enhancement::Bonus, 2); break;
    case ConsumableType::Tarot_Chariot:    enhance(Enhancement::Steel, 1); break;
    case ConsumableType::Tarot_Lovers:     enhance(Enhancement::Wild, 1); break;
    case ConsumableType::Tarot_Tower:      enhance(Enhancement::Stone, 1); break;
    case ConsumableType::Tarot_Hermit: {
        int gain = qMin(state.gold(), 20);
        state.addGold(gain);
        break;
    }

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

    case ConsumableType::Spectral_Talisman: seal(Seal::Gold, 1); break;
    case ConsumableType::Spectral_Aura:     randomEdition(1); break;
    case ConsumableType::Spectral_DejaVu:   seal(Seal::Red, 1); break;
    case ConsumableType::Spectral_Trance:   seal(Seal::Blue, 1); break;
    case ConsumableType::Spectral_Medium:   seal(Seal::Purple, 1); break;
    case ConsumableType::Spectral_Immolate: {
        QVector<int> use = normalizedSelection(sel, packHand.size(), 5);
        std::sort(use.begin(), use.end(), std::greater<int>());
        int destroyed = 0;
        for (int idx : use) {
            if (idx < 0 || idx >= packHand.size()) continue;
            packHand.removeAt(idx);
            ++destroyed;
        }
        if (destroyed > 0) state.addGold(20);
        break;
    }
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
        return true;

    case PackKind::Arcana:
    case PackKind::Celestial:
    case PackKind::Spectral:
        if (chosenIdx >= pack.consumables.size()) return false;
        return applyConsumableTypeToPackHand(*this, pack.consumables[chosenIdx],
                                             selectedPackHandIdx, packHand);

    case PackKind::Buffoon:
        if (chosenIdx >= pack.jokers.size()) return false;
        if (!canAddJoker()) return false;
        mJokers.append(createJoker(pack.jokers[chosenIdx]));
        emit jokersChanged();
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

    bool ok = applyConsumableTypeToPackHand(*this, c.type, sel, packHand);
    if (!ok) return false;

    mConsumables.removeAt(consumableIdx);
    emit consumablesChanged();
    return true;
}

void GameState::startGame()
{
    mGold = Constants::INITIAL_GOLD;
    mAnte = 1;
    mScore = 0;
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
    mBossEffect = BossEffect::None;  // 已激活 Boss 效果只存在于 Boss 对局内，不能泄漏到下个小盲注。
    mBlindType = static_cast<BlindType>(mBlindIdx);
    if (mPendingBossEffect == BossEffect::None)
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
    mHandsLeft = qMax(1, Constants::INITIAL_HANDS + mExtraHandsPerRound);
    mDiscardLeft = qMax(0, Constants::INITIAL_DISCARDS + mExtraDiscardsPerRound);

    if (type == BlindType::Boss) {
        mBossEffect = mPendingBossEffect;     // 用预先确定的
        if (mBossEffect == BossEffect::TheNeedle) mHandsLeft = 1;
    } else {
        mBossEffect = BossEffect::None;
    }

    mTargetScore = calcTargetScore();
    mPhase = GamePhase::Blind;

    // 原版每个盲注开始都会用当前完整牌组重新洗牌；上一盲注结束时手里没打出的牌也要回到牌组。
    mDeck.returnCards(mHand);
    mHand.clear();
    mDeck.reset();
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
    syncShopJokerRules();
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
