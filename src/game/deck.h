#ifndef DECK_H
#define DECK_H

#include <QVector>
#include "../card/carddata.h"

class Deck
{
    QVector<CardData> mDrawPile;
    QVector<CardData> mDiscardPile;
    void buildDeck(); // 生成52张标准牌
    void shuffle(); // 洗牌
public:
    Deck();

    void reset(); // 过关时调用，合并两堆并洗牌
    CardData draw(); // 摸一张牌
    bool isEmpty() const;
    int remaining() const; // 剩余牌数
    void discard(const CardData &card); // 弃牌
    void addCard(const CardData &card);
};

#endif // DECK_H
