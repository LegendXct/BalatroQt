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

class Shop {
public:
    Shop() = default;
    void roll();              // 重 roll 上下两区
    void rerollShopOnly();    // reroll 按钮:只重 roll 商品区

    // 商品区(joker/tarot/planet)
    const QVector<ShopOffer>& shopOffers() const { return mShopOffers; }
    bool canBuyShop(int idx, int gold) const;
    ShopOffer takeShopOffer(int idx);

    // booster 区(pack)
    const QVector<ShopOffer>& boosterOffers() const { return mBoosterOffers; }
    QVector<ShopOffer>& boosterOffersMutable() { return mBoosterOffers; }
    bool canBuyBooster(int idx, int gold) const;
    ShopOffer takeBoosterOffer(int idx);

    int  rerollCost() const { return mRerollCost; }
    void onReroll();
    void resetForNewBlind();

private:
    QVector<ShopOffer> mShopOffers;
    QVector<ShopOffer> mBoosterOffers;
    int mRerollCost = 5;

    static ShopOffer randomShopOffer();
    static ShopOffer randomBoosterOffer();
    static JokerType randomJokerType();
    static int costFor(JokerType t);
};

#endif
