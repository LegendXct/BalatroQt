#include "tag.h"
#include <QRandomGenerator>

TagData tagData(TagType type)
{
    TagData t;
    t.type = type;
    switch (type) {
    case TagType::Uncommon:
        t.key = "tag_uncommon"; t.name = "罕见标签"; t.description = "下个商店尝试生成一个罕见小丑"; t.spritePos = {0, 0}; break;
    case TagType::Rare:
        t.key = "tag_rare"; t.name = "稀有标签"; t.description = "下个商店尝试生成一个稀有小丑"; t.spritePos = {1, 0}; break;
    case TagType::Negative:
        t.key = "tag_negative"; t.name = "负片标签"; t.description = "下个商店小丑有机会带负片版本"; t.spritePos = {2, 0}; t.minAnte = 2; break;
    case TagType::Foil:
        t.key = "tag_foil"; t.name = "闪箔标签"; t.description = "下个商店小丑有机会带闪箔版本"; t.spritePos = {3, 0}; break;
    case TagType::Holographic:
        t.key = "tag_holo"; t.name = "镭射标签"; t.description = "下个商店小丑有机会带镭射版本"; t.spritePos = {0, 1}; break;
    case TagType::Polychrome:
        t.key = "tag_polychrome"; t.name = "多彩标签"; t.description = "下个商店小丑有机会带多彩版本"; t.spritePos = {1, 1}; break;
    case TagType::Investment:
        t.key = "tag_investment"; t.name = "投资标签"; t.description = "击败 Boss 盲注结算时获得 $25"; t.spritePos = {2, 1}; break;
    case TagType::Voucher:
        t.key = "tag_voucher"; t.name = "优惠券标签"; t.description = "下个商店出现一张优惠券"; t.spritePos = {3, 1}; break;
    case TagType::Boss:
        t.key = "tag_boss"; t.name = "Boss 标签"; t.description = "重新随机本底注的 Boss 盲注"; t.spritePos = {0, 2}; break;
    case TagType::Standard:
        t.key = "tag_standard"; t.name = "标准标签"; t.description = "下个商店获得一个免费标准包"; t.spritePos = {1, 2}; t.minAnte = 2; break;
    case TagType::Charm:
        t.key = "tag_charm"; t.name = "护符标签"; t.description = "下个商店获得一个免费奥秘包"; t.spritePos = {2, 2}; break;
    case TagType::Meteor:
        t.key = "tag_meteor"; t.name = "流星标签"; t.description = "下个商店获得一个免费天体包"; t.spritePos = {3, 2}; t.minAnte = 2; break;
    case TagType::Buffoon:
        t.key = "tag_buffoon"; t.name = "小丑标签"; t.description = "下个商店获得一个免费小丑包"; t.spritePos = {4, 2}; t.minAnte = 2; break;
    case TagType::Handy:
        t.key = "tag_handy"; t.name = "灵巧标签"; t.description = "立即获得少量金钱"; t.spritePos = {1, 3}; t.minAnte = 2; break;
    case TagType::Garbage:
        t.key = "tag_garbage"; t.name = "垃圾标签"; t.description = "立即获得少量金钱"; t.spritePos = {2, 3}; t.minAnte = 2; break;
    case TagType::Ethereal:
        t.key = "tag_ethereal"; t.name = "虚幻标签"; t.description = "下个商店获得一个免费幻灵包"; t.spritePos = {3, 3}; t.minAnte = 2; break;
    case TagType::Coupon:
        t.key = "tag_coupon"; t.name = "优惠标签"; t.description = "下个商店初始商品和补充包免费，不影响优惠券"; t.spritePos = {4, 0}; break;
    case TagType::Double:
        t.key = "tag_double"; t.name = "双倍标签"; t.description = "下一次选定的标签会额外获得一个复制品，双倍标签除外"; t.spritePos = {5, 0}; break;
    case TagType::Juggle:
        t.key = "tag_juggle"; t.name = "杂耍标签"; t.description = "下个盲注手牌上限 +3"; t.spritePos = {5, 1}; break;
    case TagType::D6:
        t.key = "tag_d_six"; t.name = "D6 标签"; t.description = "下个商店第一次重掷免费"; t.spritePos = {5, 3}; break;
    case TagType::TopUp:
        t.key = "tag_top_up"; t.name = "充值标签"; t.description = "若有空位，下个商店补充小丑"; t.spritePos = {4, 1}; t.minAnte = 2; break;
    case TagType::Skip:
        t.key = "tag_skip"; t.name = "跳过标签"; t.description = "立即获得 $5"; t.spritePos = {0, 3}; break;
    case TagType::Orbital:
        t.key = "tag_orbital"; t.name = "轨道标签"; t.description = "随机牌型等级 +3"; t.spritePos = {5, 2}; t.minAnte = 2; break;
    case TagType::Economy:
        t.key = "tag_economy"; t.name = "经济标签"; t.description = "金钱翻倍，最多获得 $40"; t.spritePos = {4, 3}; break;
    }
    return t;
}

QVector<TagType> baseTagPoolForAnte(int ante)
{
    QVector<TagType> all = {
        TagType::Uncommon, TagType::Rare, TagType::Negative, TagType::Foil, TagType::Holographic,
        TagType::Polychrome, TagType::Investment, TagType::Voucher, TagType::Boss,
        TagType::Standard, TagType::Charm, TagType::Meteor, TagType::Buffoon,
        TagType::Handy, TagType::Garbage, TagType::Ethereal, TagType::Coupon,
        TagType::Double, TagType::Juggle, TagType::D6, TagType::TopUp,
        TagType::Skip, TagType::Orbital, TagType::Economy,
    };
    QVector<TagType> pool;
    for (TagType tt : all) {
        if (tagData(tt).minAnte <= ante) pool.append(tt);
    }
    if (pool.isEmpty()) pool.append(TagType::Skip);
    return pool;
}

TagType randomTagForAnte(int ante)
{
    QVector<TagType> pool = baseTagPoolForAnte(ante);
    return pool[QRandomGenerator::global()->bounded(pool.size())];
}
