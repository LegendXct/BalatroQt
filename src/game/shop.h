#ifndef SHOP_H
#define SHOP_H

#include <QVector>
#include <QString>
#include <QPoint>
#include "../card/joker.h"
#include "../card/consumable.h"
#include "../card/carddata.h"
#include "boosterpack.h"

enum class OfferKind {
    Joker,
    Tarot,
    Planet,
    Spectral,
    PlayingCard,
    Pack,
    Voucher,
};

enum class VoucherType {
    Overstock,
    ClearanceSale,
    Hone,
    RerollSurplus,
    CrystalBall,
    Telescope,
    Grabber,
    Wasteful,
    TarotMerchant,
    PlanetMerchant,
    SeedMoney,
    Blank,
    MagicTrick,
    Hieroglyph,
    DirectorsCut,
    PaintBrush,
};

struct VoucherData {
    VoucherType type = VoucherType::Overstock;
    QString name;
    QString description;
    QPoint spritePos = {0, 0};
    int cost = 10;
};

struct ShopRates {
    double joker = 20.0;
    double tarot = 4.0;
    double planet = 4.0;
    double playingCard = 0.0;
    double spectral = 0.0;
};

struct ShopOffer {
    OfferKind kind = OfferKind::Joker;
    JokerType joker = JokerType::Joker;
    ConsumableType consumable = ConsumableType::Planet_Pluto;
    CardData playingCard;
    PackKind pack = PackKind::Standard;
    PackSize packSize = PackSize::Normal;
    VoucherType voucher = VoucherType::Overstock;
    int cost = 0;
    bool sold = false;
};

VoucherData voucherData(VoucherType t);
QVector<VoucherType> baseVoucherPool();

class Shop {
public:
    Shop() = default;
    void roll();              // 重 roll 上下两区
    void rerollShopOnly();    // reroll 按钮：只重 roll 商品区
    void ensureShopOfferCount();      // 优惠券立即改变槽位时，补足/裁剪当前商品区
    void refreshCurrentOfferCosts();  // 折扣类优惠券立即刷新当前商店价格

    // 上半区：joker/tarot/planet/playing card/spectral
    const QVector<ShopOffer>& shopOffers() const { return mShopOffers; }
    bool canBuyShop(int idx, int gold) const;
    ShopOffer takeShopOffer(int idx);

    // 下半区左：voucher
    const QVector<ShopOffer>& voucherOffers() const { return mVoucherOffers; }
    bool canBuyVoucher(int idx, int gold) const;
    ShopOffer takeVoucherOffer(int idx);

    // 下半区右：booster pack
    const QVector<ShopOffer>& boosterOffers() const { return mBoosterOffers; }
    QVector<ShopOffer>& boosterOffersMutable() { return mBoosterOffers; }
    bool canBuyBooster(int idx, int gold) const;
    ShopOffer takeBoosterOffer(int idx);

    int  rerollCost() const { return mRerollCost; }
    void onReroll();
    void resetForNewBlind();

    // 优惠券会修改这些商店参数
    void changeShopSlots(int delta);
    void setTarotRate(double v) { mRates.tarot = v; }
    void setPlanetRate(double v) { mRates.planet = v; }
    void setPlayingCardRate(double v) { mRates.playingCard = v; }
    void setSpectralRate(double v) { mRates.spectral = v; }
    void setDiscountPercent(int v) { mDiscountPercent = v; }
    void setRerollDiscount(int v) { mRerollDiscount = v; resetForNewBlind(); }
    void setRedeemedVouchers(const QVector<VoucherType> &v) { mRedeemedVouchers = v; }
    void setOwnedJokers(const QVector<JokerType> &owned, bool allowDuplicates);

    int shopSlots() const { return mShopSlots; }
    const ShopRates &rates() const { return mRates; }
    int discountPercent() const { return mDiscountPercent; }

private:
    QVector<ShopOffer> mShopOffers;
    QVector<ShopOffer> mVoucherOffers;
    QVector<ShopOffer> mBoosterOffers;

    int mRerollCost = 5;
    int mRerollDiscount = 0;
    int mShopSlots = 2;
    int mDiscountPercent = 0;
    ShopRates mRates;
    QVector<VoucherType> mRedeemedVouchers;
    QVector<JokerType> mOwnedJokers;
    bool mAllowJokerDuplicates = false;

    ShopOffer randomShopOffer(const QVector<ShopOffer> &alreadyRolled = {}) const;
    ShopOffer randomVoucherOffer() const;
    ShopOffer randomBoosterOffer(const QVector<ShopOffer> &alreadyRolled = {}) const;
    CardData randomPlayingCard(bool enhancedPossible, const QVector<ShopOffer> &alreadyRolled = {}) const;

    static QVector<JokerType> jokerPool();
    JokerType randomJokerType(const QVector<JokerType> &alreadyRolled = {}) const;
    int costFor(JokerType t) const;
    int rawCostFor(const ShopOffer &o) const;
    int applyDiscount(int rawCost) const;
    bool duplicatesOffer(const ShopOffer &candidate, const QVector<ShopOffer> &existing) const;
};

#endif
