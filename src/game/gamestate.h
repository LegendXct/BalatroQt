#ifndef GAMESTATE_H
#define GAMESTATE_H

#include <QObject>
#include <QVector>
#include <QHash>
#include "deck.h"
#include "handevaluator.h"
#include "../card/carddata.h"
#include "../card/joker.h"
#include "../utils/constants.h"
#include "shop.h"
#include "bossblind.h"
#include "../card/consumable.h"

struct HandLevel {
    int level = 1;
    int played = 0;
    int chipsBonus = 0;
    int multBonus = 0;
};

enum class BlindType {
    Small,
    Big,
    Boss
};

enum class BlindState {
    Upcoming,
    Current,
    Defeated,
    Skipped,
};

enum class GamePhase {
    BlindSelect,
    Blind,
    Shop,
    GameOver
};

enum class HandSortMode {
    ByRank,
    BySuit
};

class GameState : public QObject
{
    Q_OBJECT
public:
    explicit GameState(QObject *parent = nullptr);
    void startGame();
    void startBlind(BlindType type);
    void playCards(const QVector<int> &indices);
    void discardCards(const QVector<int> &indices);
    void nextBlind();
    const QVector<CardData> &hand() const {return mHand;}
    const QVector<Joker> &jokers() const {return mJokers;}
    int gold() const {return mGold;}
    int handsLeft() const {return mHandsLeft;}
    int discardLeft() const {return mDiscardLeft;}
    int score() const {return mScore;}
    int targetScore() const {return mTargetScore;}
    int ante() const {return mAnte;}
    int jokerSlots() const;
    BlindType blindType() const {return mBlindType;}
    GamePhase phase() const {return mPhase;}
    void addGold(int amount) {mGold += amount; emit goldChanged();}
    const HandResult &lastResult() const{return mLastResult;}
    void sortHandByRank();
    void sortHandBySuit();
    HandSortMode sortMode() const { return mSortMode; }
    int deckRemaining() const {return mDeck.remaining();}
    int deckTotal() const;
    QVector<CardData> remainingDeckCards() const;
    QVector<CardData> fullDeckCards() const;
    BlindState blindState(int idx) const { return mBlindStates[idx]; }
    void skipCurrentBlind();

    // 商店相关
    Shop &shop() { return mShop; }
    void rerollShop();
    void leaveShop();                // 离开商店 → 进入下一盲注
    bool canAddJoker() const { return mJokers.size() < jokerSlots(); }
    bool buyVoucherOffer(int offerIdx);
    bool hasVoucher(VoucherType t) const;

    const QHash<HandType, HandLevel> &handLevels() const { return mHandLevels; }
    void levelUpHand(HandType t, int times = 1);   // 行星牌用

    BossEffect bossEffect() const { return mBossEffect; }
    BossInfo currentBossInfo() const { return bossInfo(mBossEffect); }

    QVector<CardData> &handMutable() { return mHand; }
    void notifyHandChanged() { emit handChanged(); }

    // 消耗品接口
    const QVector<Consumable> &consumables() const { return mConsumables; }
    int consumableSlots() const;
    int handSize() const;
    bool canAddConsumable() const { return mConsumables.size() < consumableSlots(); }
    bool addConsumable(ConsumableType t);
    bool useConsumable(int idx, const QVector<int> &selectedHandIdx);
    bool sellConsumable(int idx);
    bool sellJoker(int idx);
    bool moveJoker(int from, int to);
    void collectRoundCardsToDeck();

    // 商店买入：扩展为支持 3 种 offer
    bool buyShopOffer(int offerIdx);     // ← 替代旧的 buyJoker
    bool buyPack(int offerIdx, PackContent &outContent);
    QVector<CardData> drawPackHand();
    void returnPackHand(const QVector<CardData> &packHand);
    bool applyPackChoice(const PackContent &pack, int chosenIdx,
                         const QVector<int> &selectedPackHandIdx,
                         QVector<CardData> &packHand);
    bool useConsumableOnPackHand(int consumableIdx,
                                 const QVector<int> &selectedPackHandIdx,
                                 QVector<CardData> &packHand);

    int blindIdx() const { return mBlindIdx; }       // 当前 ante 内打到第几个 (0/1/2)
    BossEffect pendingBossEffect() const { return mPendingBossEffect; }
    void selectCurrentBlind();                       // 玩家从 BlindSelect 点"选择"
    bool justSkipped() const { return mJustSkipped; }

    HandResult previewSelection(const QVector<int> &indices) const;
signals:
    void handChanged();
    void scoreChanged();
    void goldChanged();
    void roundWon(int blindReward, int handBonus, int interest);
    void gameOver(bool won);
    void handPlayed();
    void jokersChanged();
    void shopChanged();
    void handLevelsChanged();
    void consumablesChanged();
    void blindSelectEntered();                       // 切到选择页
    void blindStarted();                             // 切到对局页

private:
    Deck mDeck;
    QVector<CardData> mHand;
    QVector<Joker> mJokers;
    int mGold;
    int mScore;
    int mTargetScore;
    int mHandsLeft;
    int mDiscardLeft;
    int mAnte;
    BlindType mBlindType;
    GamePhase mPhase;
    HandResult mLastResult;
    Shop mShop;
    QHash<HandType, HandLevel> mHandLevels;
    BlindState mBlindStates[3] = {
        BlindState::Current, BlindState::Upcoming, BlindState::Upcoming
    };

    bool mJustSkipped = false;

    int mBlindIdx = 0;
    BossEffect mPendingBossEffect = BossEffect::None;

    HandSortMode mSortMode = HandSortMode::ByRank;   // 默认按点数
    void enterBlindSelect();

    void dealCards(); // 补牌到满
    int calcTargetScore() const;
    void applyCardEnhancements(HandResult &result);
    void applyJokerEffects(HandResult &result);
    void checkGameOver();
    int roundReward() const;
    static QPair<int, int> handLevelDelta(HandType t);   // {chipsΔ, multΔ}

    BossEffect mBossEffect = BossEffect::None;
    void applyBossDebuffs();   // 给手牌打 debuff 标记
    void applyBossPostPlay();  // 出牌后 The Hook 等

    bool mFirstShop = true;

    QVector<Consumable> mConsumables;
    QVector<VoucherType> mRedeemedVouchers;

    int mExtraConsumableSlots = 0;
    int mExtraHandsPerRound = 0;
    int mExtraDiscardsPerRound = 0;
    int mExtraHandSize = 0;
    int mInterestCap = Constants::INTEREST_MAX;

    void applyVoucher(VoucherType t);
    QVector<JokerType> ownedJokerTypes() const;
    bool hasJokerDuplicateBypass() const;
    void syncShopJokerRules();
    void scoreCard(const CardData &card, HandResult &result, int playedIdx);
    static ConsumableType planetTypeFor(HandType t);
};

#endif // GAMESTATE_H
