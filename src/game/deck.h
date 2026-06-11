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
    static CardData clearTransientFlags(CardData card);
public:
    Deck();

    void reset(); // 新盲注开始时：合并弃牌堆到摸牌堆并洗牌
    CardData draw(); // 摸一张牌
    bool isEmpty() const;
    int remaining() const; // 摸牌堆剩余牌数
    int totalKnown() const; // 摸牌堆 + 弃牌堆
    void discard(const CardData &card); // 弃牌
    void addCard(const CardData &card);
    void returnCards(const QVector<CardData> &cards); // 把当前手牌/临时手牌归还进牌组

    const QVector<CardData> &drawPile() const { return mDrawPile; }
    const QVector<CardData> &discardPile() const { return mDiscardPile; }
    QVector<CardData> allKnownCards() const;
    // 浅拷贝塔罗的状态同步：按 uid 在摸牌/弃牌堆中找牌（可变指针；找不到返回 nullptr）。
    CardData *findByUid(int uid);
    bool removeByUid(int uid, CardData *removed = nullptr);
};

#endif // DECK_H
