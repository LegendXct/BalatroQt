#include "shop.h"
#include "demoscript.h"
#include <QRandomGenerator>
#include <QtGlobal>
#include <cmath>
#include <algorithm>

static int rawJokerCostForType(JokerType t)
{
    int raw = 4;
    switch (t) {
    case JokerType::Joker:           raw = 2; break;
    case JokerType::JollyJoker:
    case JokerType::SlyJoker:        raw = 3; break;
    case JokerType::ZanyJoker:
    case JokerType::MadJoker:
    case JokerType::CrazyJoker:
    case JokerType::DrollJoker:
    case JokerType::WilyJoker:
    case JokerType::CleverJoker:
    case JokerType::DeviousJoker:
    case JokerType::CraftyJoker:
    case JokerType::Misprint:
    case JokerType::ToDoList:
    case JokerType::EvenSteven:
    case JokerType::OddTodd:
    case JokerType::Scholar:         raw = 4; break;
    case JokerType::GreedyJoker:
    case JokerType::LustyJoker:
    case JokerType::WrathfulJoker:
    case JokerType::GluttonousJoker:
    case JokerType::HalfJoker:
    case JokerType::Banner:
    case JokerType::MysticSummit:
    case JokerType::RaisedFist:      raw = 5; break;
    case JokerType::GoldenJoker:
    case JokerType::Bull:            raw = 6; break;
    case JokerType::Bootstraps:      raw = 7; break;
    case JokerType::Fibonacci:       raw = 8; break;
    case JokerType::AbstractJoker:   raw = 4; break;
    case JokerType::Supernova:       raw = 5; break;
    case JokerType::GrosMichel:      raw = 5; break;
    case JokerType::Cavendish:       raw = 4; break;
    case JokerType::IceCream:        raw = 5; break;
    case JokerType::Stuntman:        raw = 7; break;
    case JokerType::TheDuo:          raw = 8; break;
    case JokerType::TheTrio:         raw = 8; break;
    case JokerType::TheFamily:       raw = 8; break;
    case JokerType::TheOrder:        raw = 8; break;
    case JokerType::TheTribe:        raw = 8; break;
    case JokerType::Blackboard:      raw = 6; break;
    case JokerType::ScaryFace:       raw = 4; break;
    case JokerType::SmileyFace:      raw = 4; break;
    case JokerType::WalkieTalkie:    raw = 4; break;
    case JokerType::Arrowhead:       raw = 7; break;
    case JokerType::OnyxAgate:       raw = 7; break;
    case JokerType::RoughGem:        raw = 7; break;
    case JokerType::Bloodstone:      raw = 7; break;
    case JokerType::ShootTheMoon:    raw = 5; break;
    case JokerType::Baron:           raw = 8; break;
    case JokerType::FlowerPot:       raw = 6; break;
    case JokerType::Acrobat:         raw = 6; break;
    case JokerType::Swashbuckler:    raw = 4; break;
    case JokerType::Ramen:           raw = 6; break;
    case JokerType::DriversLicense:  raw = 7; break;
    case JokerType::Mime:            raw = 5; break;
    case JokerType::DNA:             raw = 8; break;
    case JokerType::Blueprint:
    case JokerType::Brainstorm:      raw = 10; break;
    case JokerType::Caino:
    case JokerType::Triboulet:
    case JokerType::Yorick:
    case JokerType::Chicot:
    case JokerType::Perkeo:          raw = 20; break;
    case JokerType::JokerStencil:    raw = 8;  break;
    case JokerType::SteelJoker:      raw = 7;  break;
    case JokerType::StoneJoker:      raw = 6;  break;
    case JokerType::BlueJoker:       raw = 5;  break;
    case JokerType::Erosion:         raw = 6;  break;
    case JokerType::BusinessCard:    raw = 4;  break;
    case JokerType::FacelessJoker:   raw = 4;  break;
    case JokerType::Cloud9:          raw = 7;  break;
    case JokerType::GoldenTicket:    raw = 5;  break;
    case JokerType::SeeingDouble:    raw = 6;  break;
    case JokerType::SquareJoker:     raw = 4;  break;
    case JokerType::Runner:          raw = 5;  break;
    case JokerType::Castle:          raw = 6;  break;
    case JokerType::GreenJoker:      raw = 4;  break;
    case JokerType::Obelisk:         raw = 8;  break;
    case JokerType::RideTheBus:      raw = 6;  break;
    case JokerType::SpareTrousers:   raw = 6;  break;
    case JokerType::WeeJoker:        raw = 8;  break;
    case JokerType::HitTheRoad:      raw = 8;  break;
    case JokerType::GlassJoker:      raw = 6;  break;
    case JokerType::LuckyCat:        raw = 6;  break;
    case JokerType::Popcorn:         raw = 5;  break;
    case JokerType::Juggler:         raw = 4;  break;
    case JokerType::Drunkard:        raw = 4;  break;
    case JokerType::MerryAndy:       raw = 7;  break;
    case JokerType::Troubadour:      raw = 6;  break;
    case JokerType::DelayedGratification: raw = 4; break;
    case JokerType::ToTheMoon:       raw = 5;  break;
    case JokerType::ReservedParking: raw = 6;  break;
    case JokerType::MailInRebate:    raw = 4;  break;
    case JokerType::AncientJoker:    raw = 8;  break;
    case JokerType::TheIdol:         raw = 6;  break;
    case JokerType::SpaceJoker:      raw = 5;  break;
    case JokerType::Hack:            raw = 6;  break;
    case JokerType::RiffRaff:        raw = 6;  break;
    case JokerType::MarbleJoker:     raw = 6;  break;
    case JokerType::Burglar:         raw = 6;  break;
    case JokerType::Cartomancer:     raw = 6;  break;
    case JokerType::Certificate:     raw = 6;  break;
    case JokerType::Madness:         raw = 7;  break;
    case JokerType::EightBall:       raw = 5;  break;
    case JokerType::Seance:          raw = 6;  break;
    case JokerType::Vagabond:        raw = 8;  break;
    case JokerType::Superposition:   raw = 4;  break;
    case JokerType::FlashCard:       raw = 5;  break;
    case JokerType::Throwback:       raw = 6;  break;
    case JokerType::Campfire:        raw = 9;  break;
    case JokerType::FortuneTeller:   raw = 6;  break;
    case JokerType::LoyaltyCard:     raw = 5;  break;
    case JokerType::Egg:             raw = 4;  break;
    case JokerType::Rocket:          raw = 6;  break;
    case JokerType::Satellite:       raw = 6;  break;
    case JokerType::GiftCard:        raw = 6;  break;
    case JokerType::Shortcut:        raw = 7;  break;
    case JokerType::SmearedJoker:    raw = 7;  break;
    case JokerType::Splash:          raw = 3;  break;
    case JokerType::Showman:         raw = 5;  break;
    case JokerType::Dusk:            raw = 5;  break;
    case JokerType::CeremonialDagger:raw = 6;  break;
    case JokerType::TurtleBean:      raw = 6;  break;
    case JokerType::Seltzer:         raw = 6;  break;
    case JokerType::BurntJoker:      raw = 8;  break;
    case JokerType::ChaosTheClown:   raw = 4;  break;
    case JokerType::Pareidolia:      raw = 5;  break;
    case JokerType::Hallucination:   raw = 4;  break;
    case JokerType::Luchador:        raw = 5;  break;
    case JokerType::InvisibleJoker:  raw = 8;  break;
    case JokerType::CreditCard:      raw = 1;  break;
    case JokerType::MrBones:         raw = 5;  break;
    case JokerType::DietCola:        raw = 6;  break;
    case JokerType::FourFingers:     raw = 7;  break;
    case JokerType::OopsAllSixes:    raw = 4;  break;
    case JokerType::SixthSense:      raw = 6;  break;
    case JokerType::RedCard:         raw = 5;  break;
    case JokerType::BaseballCard:    raw = 8;  break;
    case JokerType::TradingCard:     raw = 6;  break;
    case JokerType::Matador:         raw = 7;  break;
    case JokerType::Astronomer:      raw = 8;  break;
    }
    return raw;
}


VoucherData voucherData(VoucherType t) {
    VoucherData v;
    v.type = t;
    v.cost = 10;
    switch (t) {
    case VoucherType::Overstock:
        v.name = "库存过剩"; v.description = "商店上半区永久多 1 个商品槽"; v.spritePos = {0, 0}; break;
    case VoucherType::OverstockPlus:
        v.name = "库存过剩+"; v.description = "商店上半区再多 1 个商品槽"; v.spritePos = {0, 1}; break;
    case VoucherType::ClearanceSale:
        v.name = "清仓特卖"; v.description = "商店商品永久 25% 折扣"; v.spritePos = {3, 0}; break;
    case VoucherType::Liquidation:
        v.name = "大甩卖"; v.description = "商店商品永久 50% 折扣"; v.spritePos = {3, 1}; break;
    case VoucherType::Hone:
        v.name = "磨练"; v.description = "提高版本牌出现概率"; v.spritePos = {4, 0}; break;
    case VoucherType::GlowUp:
        v.name = "焕彩"; v.description = "进一步提高版本牌出现概率"; v.spritePos = {4, 1}; break;
    case VoucherType::RerollSurplus:
        v.name = "重抽盈余"; v.description = "每次商店重抽便宜 $2"; v.spritePos = {0, 2}; break;
    case VoucherType::RerollGlut:
        v.name = "重抽过剩"; v.description = "重抽费用再便宜 $2"; v.spritePos = {0, 3}; break;
    case VoucherType::CrystalBall:
        v.name = "水晶球"; v.description = "消耗牌槽位 +1"; v.spritePos = {2, 2}; break;
    case VoucherType::OmenGlobe:
        v.name = "预兆球"; v.description = "奥秘包中可能出现幻灵牌"; v.spritePos = {2, 3}; break;
    case VoucherType::Telescope:
        v.name = "望远镜"; v.description = "天体包第一张更偏向常用牌型"; v.spritePos = {3, 2}; break;
    case VoucherType::Observatory:
        v.name = "天文台"; v.description = "持有星球牌时会增强对应牌型（后续细化）"; v.spritePos = {3, 3}; break;
    case VoucherType::Grabber:
        v.name = "抓钩"; v.description = "每回合出牌次数 +1"; v.spritePos = {5, 0}; break;
    case VoucherType::NachoTong:
        v.name = "玉米片钳"; v.description = "每回合出牌次数再 +1"; v.spritePos = {5, 1}; break;
    case VoucherType::Wasteful:
        v.name = "浪费"; v.description = "每回合弃牌次数 +1"; v.spritePos = {6, 0}; break;
    case VoucherType::Recyclomancy:
        v.name = "回收术"; v.description = "每回合弃牌次数再 +1"; v.spritePos = {6, 1}; break;
    case VoucherType::TarotMerchant:
        v.name = "塔罗商人"; v.description = "商店塔罗权重从 4 提高到 9.6"; v.spritePos = {1, 0}; break;
    case VoucherType::TarotTycoon:
        v.name = "塔罗大亨"; v.description = "商店塔罗权重提高到 32"; v.spritePos = {1, 1}; break;
    case VoucherType::PlanetMerchant:
        v.name = "星球商人"; v.description = "商店星球权重从 4 提高到 9.6"; v.spritePos = {2, 0}; break;
    case VoucherType::PlanetTycoon:
        v.name = "星球大亨"; v.description = "商店星球权重提高到 32"; v.spritePos = {2, 1}; break;
    case VoucherType::SeedMoney:
        v.name = "种子基金"; v.description = "利息上限提高到 $50"; v.spritePos = {1, 2}; break;
    case VoucherType::MoneyTree:
        v.name = "摇钱树"; v.description = "利息上限提高到 $100"; v.spritePos = {1, 3}; break;
    case VoucherType::Blank:
        v.name = "空白"; v.description = "什么都不做，但可解锁反物质"; v.spritePos = {7, 0}; break;
    case VoucherType::Antimatter:
        v.name = "反物质"; v.description = "小丑槽位 +1"; v.spritePos = {7, 1}; break;
    case VoucherType::MagicTrick:
        v.name = "魔术把戏"; v.description = "商店上半区可刷普通游戏牌，权重为 4"; v.spritePos = {4, 2}; break;
    case VoucherType::Illusion:
        v.name = "幻觉"; v.description = "商店游戏牌可能带增强、版本或印章"; v.spritePos = {4, 3}; break;
    case VoucherType::Hieroglyph:
        v.name = "象形文字"; v.description = "底注 -1，每回合出牌次数 -1"; v.spritePos = {5, 2}; break;
    case VoucherType::Petroglyph:
        v.name = "岩画"; v.description = "底注再 -1，每回合出牌次数再 -1"; v.spritePos = {5, 3}; break;
    case VoucherType::DirectorsCut:
        v.name = "导演剪辑版"; v.description = "允许花钱重掷 Boss 盲注"; v.spritePos = {6, 2}; break;
    case VoucherType::Retcon:
        v.name = "追溯修改"; v.description = "可多次重掷 Boss 盲注"; v.spritePos = {6, 3}; break;
    case VoucherType::PaintBrush:
        v.name = "画笔"; v.description = "手牌上限 +1"; v.spritePos = {7, 2}; break;
    case VoucherType::Palette:
        v.name = "调色板"; v.description = "手牌上限再 +1"; v.spritePos = {7, 3}; break;
    }
    return v;
}

VoucherType upgradedVoucherFor(VoucherType t)
{
    switch (t) {
    case VoucherType::Overstock:      return VoucherType::OverstockPlus;
    case VoucherType::ClearanceSale:  return VoucherType::Liquidation;
    case VoucherType::Hone:           return VoucherType::GlowUp;
    case VoucherType::RerollSurplus:  return VoucherType::RerollGlut;
    case VoucherType::CrystalBall:    return VoucherType::OmenGlobe;
    case VoucherType::Telescope:      return VoucherType::Observatory;
    case VoucherType::Grabber:        return VoucherType::NachoTong;
    case VoucherType::Wasteful:       return VoucherType::Recyclomancy;
    case VoucherType::TarotMerchant:  return VoucherType::TarotTycoon;
    case VoucherType::PlanetMerchant: return VoucherType::PlanetTycoon;
    case VoucherType::SeedMoney:      return VoucherType::MoneyTree;
    case VoucherType::Blank:          return VoucherType::Antimatter;
    case VoucherType::MagicTrick:     return VoucherType::Illusion;
    case VoucherType::Hieroglyph:     return VoucherType::Petroglyph;
    case VoucherType::DirectorsCut:   return VoucherType::Retcon;
    case VoucherType::PaintBrush:     return VoucherType::Palette;
    default:                          return t;
    }
}

VoucherType prerequisiteVoucherFor(VoucherType t)
{
    switch (t) {
    case VoucherType::OverstockPlus:  return VoucherType::Overstock;
    case VoucherType::Liquidation:    return VoucherType::ClearanceSale;
    case VoucherType::GlowUp:         return VoucherType::Hone;
    case VoucherType::RerollGlut:     return VoucherType::RerollSurplus;
    case VoucherType::OmenGlobe:      return VoucherType::CrystalBall;
    case VoucherType::Observatory:    return VoucherType::Telescope;
    case VoucherType::NachoTong:      return VoucherType::Grabber;
    case VoucherType::Recyclomancy:   return VoucherType::Wasteful;
    case VoucherType::TarotTycoon:    return VoucherType::TarotMerchant;
    case VoucherType::PlanetTycoon:   return VoucherType::PlanetMerchant;
    case VoucherType::MoneyTree:      return VoucherType::SeedMoney;
    case VoucherType::Antimatter:     return VoucherType::Blank;
    case VoucherType::Illusion:       return VoucherType::MagicTrick;
    case VoucherType::Petroglyph:     return VoucherType::Hieroglyph;
    case VoucherType::Retcon:         return VoucherType::DirectorsCut;
    case VoucherType::Palette:        return VoucherType::PaintBrush;
    default:                          return t;
    }
}

QVector<VoucherType> baseVoucherPool() {
    return {
        VoucherType::Overstock,
        VoucherType::ClearanceSale,
        VoucherType::Hone,
        VoucherType::RerollSurplus,
        VoucherType::CrystalBall,
        VoucherType::Telescope,
        VoucherType::Grabber,
        VoucherType::Wasteful,
        VoucherType::TarotMerchant,
        VoucherType::PlanetMerchant,
        VoucherType::SeedMoney,
        VoucherType::Blank,
        VoucherType::MagicTrick,
        VoucherType::Hieroglyph,
        VoucherType::DirectorsCut,
        VoucherType::PaintBrush,
        VoucherType::OverstockPlus,
        VoucherType::Liquidation,
        VoucherType::GlowUp,
        VoucherType::RerollGlut,
        VoucherType::OmenGlobe,
        VoucherType::Observatory,
        VoucherType::NachoTong,
        VoucherType::Recyclomancy,
        VoucherType::TarotTycoon,
        VoucherType::PlanetTycoon,
        VoucherType::MoneyTree,
        VoucherType::Antimatter,
        VoucherType::Illusion,
        VoucherType::Petroglyph,
        VoucherType::Retcon,
        VoucherType::Palette,
    };
}

void Shop::roll() {
    const bool couponed = mNextShopFree;
    rerollShopOnly();

    if (DemoScript::active()) {
        // 演示模式：把 voucher 与 booster 槽完全交给脚本（不走 mAllowVoucherThisShop 与
        // randomBoosterOffer），保证每次商店出现固定内容。
        DemoScript::scriptedVoucherOffers(mVoucherOffers);
        DemoScript::scriptedBoosterOffers(mBoosterOffers);
        if (couponed) {
            for (ShopOffer &o : mShopOffers)    { if (!o.sold) { o.cost = 0; o.freeByTag = true; } }
            for (ShopOffer &o : mBoosterOffers) { if (!o.sold) { o.cost = 0; o.freeByTag = true; } }
        }
        mNextShopFree = false;
        return;
    }

    if (mAllowVoucherThisShop) {
        mVoucherOffers.clear();
        mVoucherOffers.append(randomVoucherOffer());
    }

    mBoosterOffers.clear();
    for (int i = 0; i < 2; ++i) mBoosterOffers.append(randomBoosterOffer(mBoosterOffers));

    if (couponed) {
        for (ShopOffer &o : mShopOffers) {
            if (o.sold) continue;
            o.cost = 0;
            o.freeByTag = true;
        }
        for (ShopOffer &o : mBoosterOffers) {
            if (o.sold) continue;
            o.cost = 0;
            o.freeByTag = true;
        }
    }

    // 原版 Coupon Tag：触发的"免费"只作用于商店首次出现的小丑 + 礼包，
    // 不影响优惠券，重摇后新出的商品也不再免费。这里在首次 roll 完后立刻 clear，
    // 后续 rerollShopOnly() 走正常价格。
    mNextShopFree = false;
}

void Shop::rerollShopOnly() {
    if (DemoScript::active()) {
        // 演示模式：完全用脚本接管主货架（不参与稀有/版本 pending、不走 randomShopOffer）。
        // 待处理的 pending edition/rarity 直接丢弃——脚本里不会用到，避免污染下一次重摇。
        mPendingRarityJokers.clear();
        mPendingEditionJokers.clear();
        DemoScript::scriptedShopOffers(mShopOffers, mShopSlots);
        return;
    }
    mShopOffers.clear();

    while (!mPendingRarityJokers.isEmpty() && mShopOffers.size() < mShopSlots) {
        const JokerRarity rarity = mPendingRarityJokers.takeFirst();
        if (!canCreateRarityJoker(rarity)) continue;

        ShopOffer offer = makeRarityJokerOffer(rarity, mShopOffers);
        if (!mPendingEditionJokers.isEmpty()) {
            offer.jokerEdition = mPendingEditionJokers.takeFirst();
            offer.cost = 0;
            offer.freeByTag = true;
        }
        mShopOffers.append(offer);
    }

    // 原版 Foil/Holographic/Polychrome/Negative Tag：下个商店生成一张对应版本小丑，价格为 $0。
    while (!mPendingEditionJokers.isEmpty() && mShopOffers.size() < mShopSlots) {
        Edition e = mPendingEditionJokers.takeFirst();
        mShopOffers.append(makeEditionJokerOffer(e, mShopOffers));
    }

    for (int i = mShopOffers.size(); i < mShopSlots; ++i)
        mShopOffers.append(randomShopOffer(mShopOffers));
}

void Shop::ensureShopOfferCount()
{
    while (mShopOffers.size() < mShopSlots) {
        // 演示模式：Overstock 买完扩槽 1，新槽由 demoscript 指定（演示里是木星）。
        ShopOffer demoExtra;
        if (DemoScript::active() && DemoScript::scriptedExtraShopOffer(demoExtra))
            mShopOffers.append(demoExtra);
        else
            mShopOffers.append(randomShopOffer(mShopOffers));
    }
    while (mShopOffers.size() > mShopSlots)
        mShopOffers.removeLast();
    refreshCurrentOfferCosts();
}

void Shop::refreshCurrentOfferCosts()
{
    auto refreshOne = [this](ShopOffer &o, bool couponApplies) {
        if (o.sold) return;
        // 原版 Coupon Tag 只让 shop_jokers 与 shop_booster 免费，
        // 不让 shop_vouchers 免费；Voucher Tag 只是额外增加一张券。
        if (o.freeByTag) { o.cost = 0; return; }
        if (couponApplies && mNextShopFree) { o.cost = 0; return; }
        if (o.kind == OfferKind::Voucher) {
            o.cost = voucherData(o.voucher).cost; // 优惠券固定 $10，不吃 Coupon Tag/清仓折扣
        } else {
            o.cost = applyDiscount(rawCostFor(o));
        }
    };
    for (ShopOffer &o : mShopOffers) refreshOne(o, true);
    for (ShopOffer &o : mBoosterOffers) refreshOne(o, true);
    for (ShopOffer &o : mVoucherOffers) refreshOne(o, false);
}

bool Shop::canBuyShop(int idx, int gold) const {
    if (idx < 0 || idx >= mShopOffers.size()) return false;
    const ShopOffer &o = mShopOffers[idx];
    return !o.sold && gold >= o.cost;
}

ShopOffer Shop::takeShopOffer(int idx) {
    ShopOffer o = mShopOffers[idx];
    mShopOffers[idx].sold = true;
    return o;
}

bool Shop::moveShopOffer(int from, int to)
{
    if (from < 0 || from >= mShopOffers.size() || to < 0 || to >= mShopOffers.size()) return false;
    if (from == to) return true;
    mShopOffers.move(from, to);
    return true;
}

bool Shop::canBuyVoucher(int idx, int gold) const {
    if (idx < 0 || idx >= mVoucherOffers.size()) return false;
    const ShopOffer &o = mVoucherOffers[idx];
    return !o.sold && gold >= o.cost;
}

ShopOffer Shop::takeVoucherOffer(int idx) {
    ShopOffer o = mVoucherOffers[idx];
    mVoucherOffers[idx].sold = true;
    return o;
}

bool Shop::canBuyBooster(int idx, int gold) const {
    if (idx < 0 || idx >= mBoosterOffers.size()) return false;
    const ShopOffer &o = mBoosterOffers[idx];
    return !o.sold && gold >= o.cost;
}

ShopOffer Shop::takeBoosterOffer(int idx) {
    ShopOffer o = mBoosterOffers[idx];
    mBoosterOffers[idx].sold = true;
    return o;
}

void Shop::setBoosterOfferPack(int idx, PackKind kind, PackSize size, bool freeByTag)
{
    if (idx < 0 || idx >= mBoosterOffers.size()) return;

    QVector<ShopOffer> existing = mBoosterOffers;
    existing.removeAt(idx);

    ShopOffer &o = mBoosterOffers[idx];
    const bool keepFree = freeByTag || o.freeByTag || o.cost == 0;
    o.kind = OfferKind::Pack;
    o.pack = kind;
    o.packSize = size;
    o.packVariant = choosePackVariant(kind, size, existing);
    o.sold = false;
    o.freeByTag = keepFree;
    o.cost = keepFree ? 0 : applyDiscount(rawCostFor(o));
}

bool Shop::moveBoosterOffer(int from, int to)
{
    if (from < 0 || from >= mBoosterOffers.size() || to < 0 || to >= mBoosterOffers.size()) return false;
    if (from == to) return true;
    mBoosterOffers.move(from, to);
    return true;
}

void Shop::onReroll() {
    // 原版 common_events.lua:2266-2268：每次重摇 cost_increase++，
    // 当前价 = base + cost_increase——没有 $10 上限。之前误加 cap 让玩家觉得"只到 $10"。
    ++mRerollCost;
}

void Shop::resetForNewBlind() {
    // base_reroll_cost = 5；D6/RerollSurplus 等优惠券通过 mRerollDiscount 减免基底。
    mRerollCost = qMax(0, 5 - mRerollDiscount);
    mNextShopFree = false;
    mNextShopRerollStartsFree = false;
}

void Shop::changeShopSlots(int delta) {
    mShopSlots = qMax(1, mShopSlots + delta);
    if (delta > 0) {
        QVector<ShopOffer> kept;
        kept.reserve(mShopOffers.size());
        for (const ShopOffer &offer : mShopOffers) {
            if (!offer.sold) kept.append(offer);
        }
        mShopOffers = kept;
    }
}

void Shop::setOwnedJokers(const QVector<JokerType> &owned, bool allowDuplicates)
{
    mOwnedJokers = owned;
    mAllowJokerDuplicates = allowDuplicates;
}

void Shop::addPendingEditionJoker(Edition e)
{
    if (e == Edition::None) return;
    mPendingEditionJokers.append(e);
}

void Shop::addPendingRarityJoker(JokerRarity rarity)
{
    mPendingRarityJokers.append(rarity);
}

bool Shop::canCreateRarityJoker(JokerRarity rarity) const
{
    QVector<JokerType> rolled;
    for (const ShopOffer &existing : mShopOffers)
        if (existing.kind == OfferKind::Joker) rolled.append(existing.joker);

    for (JokerType t : jokerPool()) {
        if (jokerRarity(t) != rarity) continue;
        if (t == JokerType::GrosMichel && mGrosMichelExtinct) continue;
        if (t == JokerType::Cavendish && !mGrosMichelExtinct) continue;
        if (!mAllowJokerDuplicates && (mOwnedJokers.contains(t) || rolled.contains(t)))
            continue;
        return true;
    }
    return false;
}

void Shop::appendVoucherOffer()
{
    for (int attempt = 0; attempt < 40; ++attempt) {
        ShopOffer offer = randomVoucherOffer();
        if (!duplicatesOffer(offer, mVoucherOffers)) {
            mVoucherOffers.append(offer);
            return;
        }
    }
    mVoucherOffers.append(randomVoucherOffer());
}

int Shop::applyDiscount(int rawCost) const {
    // 原版 Card:set_cost(): floor((base_cost + extra_cost + 0.5) * (100-discount)/100)，最低 $1。
    if (rawCost <= 0) return 0;
    if (mDiscountPercent <= 0) return qMax(1, rawCost);
    return qMax(1, int(std::floor((rawCost + 0.5) * (100 - mDiscountPercent) / 100.0)));
}

int Shop::rawCostFor(const ShopOffer &o) const
{
    switch (o.kind) {
    case OfferKind::Joker: {
        int raw = rawJokerCostForType(o.joker);
        switch (o.jokerEdition) {
        case Edition::Foil:        raw += 2; break;
        case Edition::Holographic: raw += 3; break;
        case Edition::Polychrome:  raw += 5; break;
        case Edition::Negative:    raw += 5; break;
        default: break;
        }
        return raw;
    }
    case OfferKind::Tarot:       return 3;
    case OfferKind::Planet:      return mOwnedJokers.contains(JokerType::Astronomer) ? 0 : 3;
    case OfferKind::Spectral:    return 4;
    case OfferKind::PlayingCard: return 3;
    case OfferKind::Voucher:     return voucherData(o.voucher).cost;
    case OfferKind::Pack:
        if (o.pack == PackKind::Celestial && mOwnedJokers.contains(JokerType::Astronomer))
            return 0;
        if (o.packSize == PackSize::Normal) return 4;
        if (o.packSize == PackSize::Jumbo)  return 6;
        return 8;
    }
    return o.cost;
}

bool Shop::duplicatesOffer(const ShopOffer &candidate, const QVector<ShopOffer> &existing) const
{
    // 原版普通商店不会刷出你当前消耗槽里已经持有的 Tarot/Planet/Spectral，
    // 逻辑和小丑去重一样；Showman 只影响 Joker，不影响消耗牌。
    if (candidate.kind == OfferKind::Tarot ||
        candidate.kind == OfferKind::Planet ||
        candidate.kind == OfferKind::Spectral) {
        if (mOwnedConsumables.contains(candidate.consumable))
            return true;
    }

    for (const ShopOffer &o : existing) {
        if (o.sold) continue;
        if (candidate.kind != o.kind) continue;
        switch (candidate.kind) {
        case OfferKind::Joker:
            if (candidate.joker == o.joker) return true;
            break;
        case OfferKind::Tarot:
        case OfferKind::Planet:
        case OfferKind::Spectral:
            if (candidate.consumable == o.consumable) return true;
            break;
        case OfferKind::PlayingCard:
            if (candidate.playingCard.rank == o.playingCard.rank &&
                candidate.playingCard.suit == o.playingCard.suit)
                return true;
            break;
        case OfferKind::Pack:
            if (candidate.pack == o.pack &&
                candidate.packSize == o.packSize &&
                candidate.packVariant == o.packVariant)
                return true;
            break;
        case OfferKind::Voucher:
            if (candidate.voucher == o.voucher) return true;
            break;
        }
    }
    return false;
}

ShopOffer Shop::makeEditionJokerOffer(Edition e, const QVector<ShopOffer> &alreadyRolled) const
{
    QVector<JokerType> rolled;
    for (const ShopOffer &existing : alreadyRolled)
        if (existing.kind == OfferKind::Joker) rolled.append(existing.joker);

    ShopOffer o;
    o.kind = OfferKind::Joker;
    o.joker = randomJokerType(rolled);
    o.jokerEdition = e;
    o.cost = 0;
    o.freeByTag = true;
    return o;
}

ShopOffer Shop::makeRarityJokerOffer(JokerRarity rarity, const QVector<ShopOffer> &alreadyRolled) const
{
    QVector<JokerType> rolled;
    for (const ShopOffer &existing : alreadyRolled)
        if (existing.kind == OfferKind::Joker) rolled.append(existing.joker);

    ShopOffer o;
    o.kind = OfferKind::Joker;
    o.joker = randomJokerTypeByRarity(rarity, rolled);
    o.jokerEdition = Edition::None;
    o.cost = 0;
    o.freeByTag = true;
    return o;
}

Edition Shop::randomJokerEdition() const
{
    // Project rule: Negative edition has a 5% shop roll chance.
    double p = QRandomGenerator::global()->generateDouble();
    if (p > 1.0 - 0.05) return Edition::Negative;
    if (p > 1.0 - 0.006 * mJokerEditionRate) return Edition::Polychrome;
    if (p > 1.0 - 0.02 * mJokerEditionRate) return Edition::Holographic;
    if (p > 1.0 - 0.04 * mJokerEditionRate) return Edition::Foil;
    return Edition::None;
}

ShopOffer Shop::randomShopOffer(const QVector<ShopOffer> &alreadyRolled) const {
    ShopOffer o;
    double total = mRates.joker + mRates.tarot + mRates.planet + mRates.playingCard + mRates.spectral;
    if (total <= 0.0) total = 1.0;

    // 原版如果没有 Showman/马戏团长，中心不会重复。这里最多尝试 40 次，避免极端池子过小卡住。
    for (int attempt = 0; attempt < 40; ++attempt) {
        double roll = QRandomGenerator::global()->generateDouble() * total;
        double acc = 0.0;

        acc += mRates.joker;
        if (roll <= acc) {
            o.kind = OfferKind::Joker;
            QVector<JokerType> rolled;
            for (const ShopOffer &e : alreadyRolled)
                if (e.kind == OfferKind::Joker) rolled.append(e.joker);
            o.joker = randomJokerType(rolled);
            o.jokerEdition = randomJokerEdition();
            o.cost = mNextShopFree ? 0 : costFor(o.joker, o.jokerEdition);
            if (!duplicatesOffer(o, alreadyRolled)) return o;
            continue;
        }

        acc += mRates.tarot;
        if (roll <= acc) {
            o.kind = OfferKind::Tarot;
            o.consumable = randomTarotType();
            o.cost = mNextShopFree ? 0 : applyDiscount(3);
            if (!duplicatesOffer(o, alreadyRolled)) return o;
            continue;
        }

        acc += mRates.planet;
        if (roll <= acc) {
            o.kind = OfferKind::Planet;
            o.consumable = randomPlanetType();
            o.cost = mNextShopFree ? 0 : applyDiscount(rawCostFor(o));
            if (!duplicatesOffer(o, alreadyRolled)) return o;
            continue;
        }

        acc += mRates.playingCard;
        if (roll <= acc) {
            o.kind = OfferKind::PlayingCard;
            o.playingCard = randomPlayingCard(mPlayingCardsEnhanced, alreadyRolled);
            o.cost = mNextShopFree ? 0 : applyDiscount(3);
            if (!duplicatesOffer(o, alreadyRolled)) return o;
            continue;
        }

        o.kind = OfferKind::Spectral;
        o.consumable = randomSpectralType();
        o.cost = mNextShopFree ? 0 : applyDiscount(4);
        if (!duplicatesOffer(o, alreadyRolled)) return o;
    }

    // 兜底：如果某一类池子被抽空，就允许最后一次结果返回，保证商店不会崩。
    QVector<ShopOffer> fallbackOffers;
    auto appendConsumableFallback = [&](OfferKind kind, ConsumableType first, ConsumableType last, int baseCost) {
        QVector<ConsumableType> choices;
        for (int v = int(first); v <= int(last); ++v) {
            ShopOffer candidate;
            candidate.kind = kind;
            candidate.consumable = static_cast<ConsumableType>(v);
            if (!duplicatesOffer(candidate, alreadyRolled))
                choices.append(candidate.consumable);
        }
        if (choices.isEmpty()) return;
        ShopOffer fallback;
        fallback.kind = kind;
        fallback.consumable = choices[QRandomGenerator::global()->bounded(choices.size())];
        fallback.cost = mNextShopFree ? 0 : applyDiscount(rawCostFor(fallback));
        fallbackOffers.append(fallback);
    };
    if (mRates.tarot > 0.0) {
        appendConsumableFallback(OfferKind::Tarot,
                                 ConsumableType::Tarot_Fool,
                                 ConsumableType::Tarot_World,
                                 3);
    }
    if (mRates.planet > 0.0) {
        appendConsumableFallback(OfferKind::Planet,
                                 ConsumableType::Planet_Pluto,
                                 ConsumableType::Planet_Eris,
                                 3);
    }
    if (mRates.spectral > 0.0) {
        // randomSpectralType() excludes Soul/Black Hole; keep this fallback on that same pool.
        appendConsumableFallback(OfferKind::Spectral,
                                 ConsumableType::Spectral_Familiar,
                                 ConsumableType::Spectral_Cryptid,
                                 4);
    }
    if (!fallbackOffers.isEmpty()) {
        return fallbackOffers[QRandomGenerator::global()->bounded(fallbackOffers.size())];
    }

    if (o.cost <= 0) {
        o.kind = OfferKind::Tarot;
        o.consumable = randomTarotType();
        o.cost = mNextShopFree ? 0 : applyDiscount(3);
    }
    return o;
}

ShopOffer Shop::randomVoucherOffer() const {
    QVector<VoucherType> pool;
    for (VoucherType v : baseVoucherPool()) {
        if (mRedeemedVouchers.contains(v)) continue;
        VoucherType prereq = prerequisiteVoucherFor(v);
        if (prereq != v && !mRedeemedVouchers.contains(prereq)) continue;
        pool.append(v);
    }
    if (pool.isEmpty()) pool.append(VoucherType::Blank);

    ShopOffer o;
    o.kind = OfferKind::Voucher;
    o.voucher = pool[QRandomGenerator::global()->bounded(pool.size())];
    // Coupon Tag 不影响优惠券价格；Voucher Tag 会额外生成免费/指定券时由 GameState 单独设置。
    o.cost = voucherData(o.voucher).cost;
    return o;
}

ShopOffer Shop::randomBoosterOffer(const QVector<ShopOffer> &alreadyRolled) const {
    struct Candidate {
        PackKind kind;
        PackSize size;
        int variant;
        int weight;   // 原版权重 ×100
        int cost;
    };

    const QVector<Candidate> pool = {
        {PackKind::Arcana, PackSize::Normal, 0, 100, 4}, {PackKind::Arcana, PackSize::Normal, 1, 100, 4},
        {PackKind::Arcana, PackSize::Normal, 2, 100, 4}, {PackKind::Arcana, PackSize::Normal, 3, 100, 4},
        {PackKind::Arcana, PackSize::Jumbo, 0, 100, 6},  {PackKind::Arcana, PackSize::Jumbo, 1, 100, 6},
        {PackKind::Arcana, PackSize::Mega, 0, 25, 8},    {PackKind::Arcana, PackSize::Mega, 1, 25, 8},

        {PackKind::Celestial, PackSize::Normal, 0, 100, 4}, {PackKind::Celestial, PackSize::Normal, 1, 100, 4},
        {PackKind::Celestial, PackSize::Normal, 2, 100, 4}, {PackKind::Celestial, PackSize::Normal, 3, 100, 4},
        {PackKind::Celestial, PackSize::Jumbo, 0, 100, 6},  {PackKind::Celestial, PackSize::Jumbo, 1, 100, 6},
        {PackKind::Celestial, PackSize::Mega, 0, 25, 8},    {PackKind::Celestial, PackSize::Mega, 1, 25, 8},

        {PackKind::Spectral, PackSize::Normal, 0, 30, 4}, {PackKind::Spectral, PackSize::Normal, 1, 30, 4},
        {PackKind::Spectral, PackSize::Jumbo, 0, 30, 6},
        {PackKind::Spectral, PackSize::Mega, 0, 7, 8},

        {PackKind::Standard, PackSize::Normal, 0, 100, 4}, {PackKind::Standard, PackSize::Normal, 1, 100, 4},
        {PackKind::Standard, PackSize::Normal, 2, 100, 4}, {PackKind::Standard, PackSize::Normal, 3, 100, 4},
        {PackKind::Standard, PackSize::Jumbo, 0, 100, 6},  {PackKind::Standard, PackSize::Jumbo, 1, 100, 6},
        {PackKind::Standard, PackSize::Mega, 0, 25, 8},    {PackKind::Standard, PackSize::Mega, 1, 25, 8},

        {PackKind::Buffoon, PackSize::Normal, 0, 60, 4}, {PackKind::Buffoon, PackSize::Normal, 1, 60, 4},
        {PackKind::Buffoon, PackSize::Jumbo, 0, 60, 6},
        {PackKind::Buffoon, PackSize::Mega, 0, 15, 8},
    };

    for (int attempt = 0; attempt < 40; ++attempt) {
        int total = 0;
        for (const auto &c : pool) total += c.weight;
        int r = QRandomGenerator::global()->bounded(total);
        int acc = 0;
        Candidate chosen = pool.front();
        for (const auto &c : pool) {
            acc += c.weight;
            if (r < acc) { chosen = c; break; }
        }

        ShopOffer o;
        o.kind = OfferKind::Pack;
        o.pack = chosen.kind;
        o.packSize = chosen.size;
        o.packVariant = chosen.variant;
        o.cost = mNextShopFree ? 0 : applyDiscount(rawCostFor(o));
        if (!duplicatesOffer(o, alreadyRolled)) return o;
    }

    ShopOffer o;
    o.kind = OfferKind::Pack;
    o.pack = PackKind::Arcana;
    o.packSize = PackSize::Normal;
    o.packVariant = choosePackVariant(o.pack, o.packSize, alreadyRolled);
    o.cost = mNextShopFree ? 0 : applyDiscount(rawCostFor(o));
    return o;
}

int Shop::choosePackVariant(PackKind kind, PackSize size, const QVector<ShopOffer> &existing) const
{
    QVector<int> variants;
    const int count = qMax(1, packSpriteVariantCount(kind, size));
    for (int i = 0; i < count; ++i) variants.append(i);

    for (const ShopOffer &o : existing) {
        if (o.sold || o.kind != OfferKind::Pack) continue;
        if (o.pack != kind || o.packSize != size) continue;
        variants.removeAll(o.packVariant);
    }

    if (variants.isEmpty()) {
        for (int i = 0; i < count; ++i) variants.append(i);
    }
    return variants[QRandomGenerator::global()->bounded(variants.size())];
}

CardData Shop::randomPlayingCard(bool enhancedPossible, const QVector<ShopOffer> &alreadyRolled) const {
    auto *rng = QRandomGenerator::global();
    CardData c;
    for (int attempt = 0; attempt < 30; ++attempt) {
        c = CardData{};
        c.suit = static_cast<Suit>(rng->bounded(4));
        c.rank = static_cast<Rank>(rng->bounded(13) + 2);

        if (enhancedPossible) {
            // Source Illusion shop-card generation: optional enhancement and optional edition.
            if (rng->generateDouble() > 0.6) {
                constexpr Enhancement pool[] = {
                    Enhancement::Bonus, Enhancement::Mult, Enhancement::Wild,
                    Enhancement::Glass, Enhancement::Steel, Enhancement::Lucky,
                    Enhancement::Gold,
                };
                c.enhancement = pool[rng->bounded(int(sizeof(pool)/sizeof(*pool)))];
            }
            if (rng->generateDouble() > 0.8) {
                double ep = rng->generateDouble();
                if (ep > 0.85) c.edition = Edition::Polychrome;
                else if (ep > 0.5) c.edition = Edition::Holographic;
                else c.edition = Edition::Foil;
            }
        }
        ShopOffer tmp; tmp.kind = OfferKind::PlayingCard; tmp.playingCard = c;
        if (!duplicatesOffer(tmp, alreadyRolled)) return c;
    }
    return c;
}

QVector<JokerType> Shop::jokerPool() {
    return {
        JokerType::Joker,
        JokerType::GreedyJoker,    JokerType::LustyJoker,
        JokerType::WrathfulJoker,  JokerType::GluttonousJoker,
        JokerType::HalfJoker,      JokerType::JollyJoker,
        JokerType::ZanyJoker,      JokerType::MadJoker,
        JokerType::CrazyJoker,     JokerType::DrollJoker,
        JokerType::GoldenJoker,    JokerType::ToDoList,
        JokerType::SlyJoker,       JokerType::WilyJoker,
        JokerType::CleverJoker,    JokerType::DeviousJoker,
        JokerType::CraftyJoker,    JokerType::Banner,
        JokerType::MysticSummit,   JokerType::Misprint,
        JokerType::RaisedFist,     JokerType::Fibonacci,
        JokerType::EvenSteven,     JokerType::OddTodd,
        JokerType::Scholar,        JokerType::Bull,
        JokerType::Bootstraps,      JokerType::AbstractJoker,
        JokerType::Supernova,       JokerType::GrosMichel,
        JokerType::Cavendish,       JokerType::IceCream,
        JokerType::Stuntman,        JokerType::TheDuo,
        JokerType::TheTrio,         JokerType::TheFamily,
        JokerType::TheOrder,        JokerType::TheTribe,
        JokerType::Blackboard,      JokerType::ScaryFace,
        JokerType::SmileyFace,      JokerType::WalkieTalkie,
        JokerType::Arrowhead,       JokerType::OnyxAgate,
        JokerType::RoughGem,        JokerType::Bloodstone,
        JokerType::ShootTheMoon,    JokerType::Baron,
        JokerType::FlowerPot,       JokerType::Acrobat,
        JokerType::Swashbuckler,    JokerType::Ramen,
        JokerType::DriversLicense,
        JokerType::Hiker, JokerType::CardSharp, JokerType::Hologram,
        JokerType::MidasMask, JokerType::Vampire, JokerType::Constellation,
        JokerType::Photograph, JokerType::HangingChad, JokerType::SockAndBuskin,
        JokerType::Blueprint, JokerType::Brainstorm, JokerType::DNA, JokerType::Mime,
        JokerType::JokerStencil, JokerType::SteelJoker, JokerType::StoneJoker,
        JokerType::BlueJoker, JokerType::Erosion, JokerType::BusinessCard,
        JokerType::FacelessJoker, JokerType::Cloud9, JokerType::GoldenTicket,
        JokerType::SeeingDouble, JokerType::SixthSense, JokerType::RedCard,
        JokerType::BaseballCard, JokerType::TradingCard, JokerType::Matador,
        JokerType::Astronomer,
        JokerType::SquareJoker, JokerType::Runner, JokerType::Castle,
        JokerType::GreenJoker, JokerType::Obelisk, JokerType::RideTheBus,
        JokerType::SpareTrousers, JokerType::WeeJoker, JokerType::HitTheRoad,
        JokerType::GlassJoker, JokerType::LuckyCat, JokerType::Popcorn,
        JokerType::Juggler, JokerType::Drunkard, JokerType::MerryAndy,
        JokerType::Troubadour, JokerType::DelayedGratification, JokerType::ToTheMoon,
        JokerType::ReservedParking, JokerType::MailInRebate, JokerType::AncientJoker,
        JokerType::TheIdol, JokerType::SpaceJoker, JokerType::Hack,
        JokerType::RiffRaff, JokerType::MarbleJoker, JokerType::Burglar,
        JokerType::Cartomancer, JokerType::Certificate, JokerType::Madness,
        JokerType::EightBall, JokerType::Seance, JokerType::Vagabond,
        JokerType::Superposition,
        JokerType::FlashCard, JokerType::Throwback, JokerType::Campfire,
        JokerType::FortuneTeller, JokerType::LoyaltyCard, JokerType::Egg,
        JokerType::Rocket, JokerType::Satellite, JokerType::GiftCard,
        JokerType::Shortcut, JokerType::SmearedJoker, JokerType::Splash,
        JokerType::Showman,
        JokerType::Dusk, JokerType::CeremonialDagger, JokerType::TurtleBean,
        JokerType::Seltzer, JokerType::BurntJoker, JokerType::ChaosTheClown,
        JokerType::Pareidolia, JokerType::Hallucination, JokerType::Luchador,
        JokerType::InvisibleJoker,
        JokerType::CreditCard, JokerType::MrBones, JokerType::DietCola,
        JokerType::FourFingers, JokerType::OopsAllSixes,
    };
}

JokerType Shop::randomJokerType(const QVector<JokerType> &alreadyRolled) const {
    QVector<JokerType> pool;
    for (JokerType t : jokerPool()) {
        if (t == JokerType::GrosMichel && mGrosMichelExtinct) continue;
        if (t == JokerType::Cavendish && !mGrosMichelExtinct) continue;
        if (!mAllowJokerDuplicates && (mOwnedJokers.contains(t) || alreadyRolled.contains(t)))
            continue;
        pool.append(t);
    }
    // 退化时仅放开“重复”过滤，仍保留 gros_michel_extinct 规则。
    if (pool.isEmpty()) {
        for (JokerType t : jokerPool()) {
            if (t == JokerType::GrosMichel && mGrosMichelExtinct) continue;
            if (t == JokerType::Cavendish && !mGrosMichelExtinct) continue;
            pool.append(t);
        }
    }
    if (pool.isEmpty()) pool = jokerPool();
    return pool[QRandomGenerator::global()->bounded(pool.size())];
}

JokerType Shop::randomJokerTypeByRarity(JokerRarity rarity, const QVector<JokerType> &alreadyRolled) const {
    QVector<JokerType> pool;
    for (JokerType t : jokerPool()) {
        if (jokerRarity(t) != rarity) continue;
        if (t == JokerType::GrosMichel && mGrosMichelExtinct) continue;
        if (t == JokerType::Cavendish && !mGrosMichelExtinct) continue;
        if (!mAllowJokerDuplicates && (mOwnedJokers.contains(t) || alreadyRolled.contains(t)))
            continue;
        pool.append(t);
    }
    if (pool.isEmpty()) {
        for (JokerType t : jokerPool()) {
            if (jokerRarity(t) != rarity) continue;
            if (t == JokerType::GrosMichel && mGrosMichelExtinct) continue;
            if (t == JokerType::Cavendish && !mGrosMichelExtinct) continue;
            pool.append(t);
        }
    }
    if (pool.isEmpty()) return randomJokerType(alreadyRolled);
    return pool[QRandomGenerator::global()->bounded(pool.size())];
}

int Shop::costFor(JokerType t, Edition e) const {
    int raw = rawJokerCostForType(t);
    switch (e) {
    case Edition::Foil:        raw += 2; break;
    case Edition::Holographic: raw += 3; break;
    case Edition::Polychrome:  raw += 5; break;
    case Edition::Negative:    raw += 5; break;
    default: break;
    }
    return applyDiscount(raw);
}
