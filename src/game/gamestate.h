#ifndef GAMESTATE_H
#define GAMESTATE_H

#include <QObject>
#include <QVector>
#include "deck.h"
#include "handevaluator.h"
#include "../card/carddata.h"
#include "../card/joker.h"
#include "../utils/constants.h"

enum class BlindType {
    Small,
    Big,
    Boss
};

enum class GamePhase {
    Blind,
    Shop,
    GameOver
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
    void enterShop();
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
signals:
    void handChanged();
    void scoreChanged();
    void goldChanged();
    void gameOver(bool won);
    void handPlayed();
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

    void dealCards(); // 补牌到满
    int calcTargetScore() const;
    void applyCardEnhancements(HandResult &result);
    void applyJokerEffects(HandResult &result);
    void checkGameOver();
};

#endif // GAMESTATE_H
