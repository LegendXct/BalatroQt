#ifndef TAG_H
#define TAG_H

#include <QString>
#include <QPoint>
#include <QVector>

// 原版 G.P_TAGS 的子集/完整键名映射。
// pos 对应 resources/images/tags.png 的 68x68 切片坐标。
enum class TagType {
    Uncommon,
    Rare,
    Negative,
    Foil,
    Holographic,
    Polychrome,
    Investment,
    Voucher,
    Boss,
    Standard,
    Charm,
    Meteor,
    Buffoon,
    Handy,
    Garbage,
    Ethereal,
    Coupon,
    Double,
    Juggle,
    D6,
    TopUp,
    Skip,
    Orbital,
    Economy,
};

struct TagData {
    TagType type = TagType::Skip;
    QString key;
    QString name;
    QString description;
    QPoint spritePos = {0, 3};
    int minAnte = 1;
};

TagData tagData(TagType type);
QVector<TagType> baseTagPoolForAnte(int ante);
TagType randomTagForAnte(int ante);

#endif // TAG_H
