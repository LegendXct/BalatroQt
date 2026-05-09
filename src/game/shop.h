#ifndef SHOP_H
#define SHOP_H

#include <QVector>
#include "../card/joker.h"
#include "../card/consumable.h"
#include "boosterpack.h"

enum class OfferKind { Joker, Tarot, Planet, Pack };

struct ShopOffer {
    OfferKind kind = OfferKind::Joker;
    JokerType joker = JokerType::Joker;
    ConsumableType consumable = ConsumableType::Planet_Pluto;
    PackKind pack = PackKind::Standard;
    int cost = 0;
    bool sold = false;
};
class Shop
{
public:
    Shop() = default;
    void roll();
    bool canBuy(int idx, int gold) const;
    ShopOffer takeOffer(int idx);
    int rerollCost() const { return mRerollCost; }
    void onReroll();
    void resetForNewBlind();
    const QVector<ShopOffer> &offers() const { return mOffers; }

private:
    QVector<ShopOffer> mOffers;
    int mRerollCost = 5;

    static ShopOffer randomOffer();
    static JokerType randomJokerType();
    static int costFor(JokerType t);
};

#endif
