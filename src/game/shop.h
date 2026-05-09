#ifndef SHOP_H
#define SHOP_H

#include <QVector>
#include "../card/joker.h"

struct ShopOffer {
    JokerType type;
    int cost = 0;
    bool sold = false;
};

class Shop
{
public:
    Shop() = default;

    void roll();                                  // 生成 2 个新 offer
    bool canBuy(int idx, int gold) const;
    ShopOffer takeOffer(int idx);                 // 标记已售并返回数据

    int rerollCost() const { return mRerollCost; }
    void onReroll();

    void resetForNewBlind();                      // 进新盲注前重置 reroll 价

    const QVector<ShopOffer> &offers() const { return mOffers; }

private:
    QVector<ShopOffer> mOffers;
    int mRerollCost = 5;

    static JokerType randomJokerType();
    static int costFor(JokerType t);
};

#endif
