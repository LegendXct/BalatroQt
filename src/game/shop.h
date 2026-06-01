#ifndef SHOP_H
#define SHOP_H

#include <QVector>
#include <QString>
#include <QPoint>
#include <QtGlobal>
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
    OverstockPlus,
    ClearanceSale,
    Liquidation,
    Hone,
    GlowUp,
    RerollSurplus,
    RerollGlut,
    CrystalBall,
    OmenGlobe,
    Telescope,
    Observatory,
    Grabber,
    NachoTong,
    Wasteful,
    Recyclomancy,
    TarotMerchant,
    TarotTycoon,
    PlanetMerchant,
    PlanetTycoon,
    SeedMoney,
    MoneyTree,
    Blank,
    Antimatter,
    MagicTrick,
    Illusion,
    Hieroglyph,
    Petroglyph,
    DirectorsCut,
    Retcon,
    PaintBrush,
    Palette,
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
    int packVariant = 0;
    VoucherType voucher = VoucherType::Overstock;
    Edition jokerEdition = Edition::None;
    int cost = 0;
    bool sold = false;
    bool freeByTag = false;
};

VoucherData voucherData(VoucherType t);
QVector<VoucherType> baseVoucherPool();
VoucherType upgradedVoucherFor(VoucherType t);
VoucherType prerequisiteVoucherFor(VoucherType t);

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
    bool moveShopOffer(int from, int to);

    // 下半区左：voucher
    const QVector<ShopOffer>& voucherOffers() const { return mVoucherOffers; }
    bool canBuyVoucher(int idx, int gold) const;
    ShopOffer takeVoucherOffer(int idx);

    // 下半区右：booster pack
    const QVector<ShopOffer>& boosterOffers() const { return mBoosterOffers; }
    QVector<ShopOffer>& boosterOffersMutable() { return mBoosterOffers; }
    QVector<ShopOffer>& voucherOffersMutable() { return mVoucherOffers; }
    void setBoosterOfferPack(int idx, PackKind kind, PackSize size, bool freeByTag = false);
    bool canBuyBooster(int idx, int gold) const;
    ShopOffer takeBoosterOffer(int idx);
    bool moveBoosterOffer(int from, int to);

    int  rerollCost() const { return mRerollCost; }
    void onReroll();
    void resetForNewBlind();

    // 优惠券会修改这些商店参数
    void changeShopSlots(int delta);
    void setTarotRate(double v) { mRates.tarot = v; }
    void setPlanetRate(double v) { mRates.planet = v; }
    void setPlayingCardRate(double v) { mRates.playingCard = v; }
    void setPlayingCardsEnhanced(bool v) { mPlayingCardsEnhanced = v; }
    void setNextShopFree(bool v) {
        mNextShopFree = v;
        if (v) {
            for (ShopOffer &o : mShopOffers) {
                if (!o.sold) { o.cost = 0; o.freeByTag = true; }
            }
            for (ShopOffer &o : mBoosterOffers) {
                if (!o.sold) { o.cost = 0; o.freeByTag = true; }
            }
        }
        refreshCurrentOfferCosts();
    }
    bool nextShopFree() const { return mNextShopFree; }
    void setSpectralRate(double v) { mRates.spectral = v; }
    void setDiscountPercent(int v) { mDiscountPercent = v; }
    void setJokerEditionRate(double rate) { mJokerEditionRate = qMax(0.0, rate); }
    void addPendingEditionJoker(Edition e);
    void addPendingRarityJoker(JokerRarity rarity);
    bool canCreateRarityJoker(JokerRarity rarity) const;
    void appendVoucherOffer();
    void setRerollDiscount(int v) {
        mRerollDiscount = v;
        if (!mNextShopRerollStartsFree)
            mRerollCost = qMax(0, 5 - mRerollDiscount);
    }
    void setNextShopRerollStartsFree() { mNextShopRerollStartsFree = true; mRerollCost = 0; }
    void setRedeemedVouchers(const QVector<VoucherType> &v) { mRedeemedVouchers = v; }
    void setOwnedJokers(const QVector<JokerType> &owned, bool allowDuplicates);
    void setOwnedConsumables(const QVector<ConsumableType> &owned) { mOwnedConsumables = owned; }
    void setGrosMichelExtinct(bool v) { mGrosMichelExtinct = v; }
    JokerType randomJokerForTag(const QVector<JokerType> &alreadyRolled = {}) const { return randomJokerType(alreadyRolled); }

    int shopSlots() const { return mShopSlots; }
    // 每个 Ante 第一次进入商店刷新 1 张优惠券；之后同 Ante 保留该券/售罄状态。
    void setAllowVoucherThisShop(bool v) { mAllowVoucherThisShop = v; }
    bool allowVoucherThisShop() const { return mAllowVoucherThisShop; }
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
    bool mAllowVoucherThisShop = false;
    ShopRates mRates;
    QVector<VoucherType> mRedeemedVouchers;
    QVector<JokerType> mOwnedJokers;
    QVector<ConsumableType> mOwnedConsumables;
    bool mAllowJokerDuplicates = false;
    bool mPlayingCardsEnhanced = false;
    bool mNextShopFree = false;
    bool mNextShopRerollStartsFree = false;
    double mJokerEditionRate = 1.0;
    QVector<Edition> mPendingEditionJokers;
    QVector<JokerRarity> mPendingRarityJokers;
    bool mGrosMichelExtinct = false;

    ShopOffer makeRarityJokerOffer(JokerRarity rarity, const QVector<ShopOffer> &alreadyRolled = {}) const;
    ShopOffer makeEditionJokerOffer(Edition e, const QVector<ShopOffer> &alreadyRolled = {}) const;
    ShopOffer randomShopOffer(const QVector<ShopOffer> &alreadyRolled = {}) const;
    ShopOffer randomVoucherOffer() const;
    ShopOffer randomBoosterOffer(const QVector<ShopOffer> &alreadyRolled = {}) const;
    CardData randomPlayingCard(bool enhancedPossible, const QVector<ShopOffer> &alreadyRolled = {}) const;

    static QVector<JokerType> jokerPool();
    JokerType randomJokerType(const QVector<JokerType> &alreadyRolled = {}) const;
    JokerType randomJokerTypeByRarity(JokerRarity rarity, const QVector<JokerType> &alreadyRolled = {}) const;
    Edition randomJokerEdition() const;
    int costFor(JokerType t, Edition e = Edition::None) const;
    int rawCostFor(const ShopOffer &o) const;
    int applyDiscount(int rawCost) const;
    bool duplicatesOffer(const ShopOffer &candidate, const QVector<ShopOffer> &existing) const;
    int choosePackVariant(PackKind kind, PackSize size, const QVector<ShopOffer> &existing) const;
};

#endif
