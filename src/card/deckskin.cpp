#include "deckskin.h"

#include <QImage>
#include <QPainter>
#include <QTransform>

DeckSkin::Id DeckSkin::sCurrent = DeckSkin::Default;
int DeckSkin::sGeneration = 0;

namespace {
constexpr int CW = 142, CH = 190;   // 8BitDeck 每格尺寸（与 CardItem::SRC_W/H 一致）

// 角标（点数字母 + 小花色）在格子内的位置。对图集逐像素实测：四种花色、J/Q/K/A
// 的角标都落在 x12..27 / y14..49（字母 y14..29，花色 y34..49），而 J/Q/K 的中央
// 装饰框最左从 x30 起——这个裁剪框拿到的是干净的角标，不会带进边框像素。
const QRect kCornerSrc(12, 14, 16, 36);
}

void DeckSkin::setCurrent(Id id)
{
    if (sCurrent == id) return;
    sCurrent = id;
    ++sGeneration;
}

QString DeckSkin::name(Id id)
{
    switch (id) {
    case ChengShe: return QStringLiteral("程设专用牌组");
    case Default:  break;
    }
    return QStringLiteral("默认牌组");
}

const QPixmap &DeckSkin::deckSheet()
{
    static QPixmap base(QStringLiteral(":/textures/images/8BitDeck.png"));
    static QPixmap chengshe;
    if (sCurrent == ChengShe) {
        if (chengshe.isNull()) chengshe = buildChengSheSheet(base);
        if (!chengshe.isNull()) return chengshe;
    }
    return base;
}

QPixmap DeckSkin::buildChengSheSheet(const QPixmap &base)
{
    if (base.isNull()) return QPixmap();
    QPixmap sheet = base.copy();

    // 列 9..12 = J,Q,K,A（列号 = 点数-2）；行 0=♥ 1=♣ 2=♦ 3=♠，
    // 与 CardItem::deckSrcRect 的映射保持一致。
    const struct { int col; const char *res; } faces[] = {
        { 9,  ":/textures/images/deckskin_cs_J.png" },   // J = 贾川民
        { 10, ":/textures/images/deckskin_cs_Q.png" },   // Q = 刘家瑛
        { 11, ":/textures/images/deckskin_cs_K.png" },   // K = 郭炜
        { 12, ":/textures/images/deckskin_cs_A.png" },   // A = 杨帅
    };

    QPainter p(&sheet);
    for (const auto &f : faces) {
        const QPixmap portrait(QString::fromLatin1(f.res));
        if (portrait.isNull()) continue;
        // 人像素材是纯色浅灰底的方图：取角落像素当相框内的填充色，方图等宽嵌入后
        // 上下留出的空隙与素材自身底色无缝衔接。
        const QColor bg = portrait.toImage().pixelColor(4, 4);

        for (int row = 0; row < 4; ++row) {
            const int cx = f.col * CW, cy = row * CH;

            // 1) 角标先从“原格子”裁出来（像素风字母+花色原样保留），再清空整格重画。
            const QPixmap corner = base.copy(cx + kCornerSrc.x(), cy + kCornerSrc.y(),
                                             kCornerSrc.width(), kCornerSrc.height());
            p.setCompositionMode(QPainter::CompositionMode_Clear);
            p.fillRect(QRect(cx, cy, CW, CH), Qt::transparent);
            p.setCompositionMode(QPainter::CompositionMode_SourceOver);

            // 2) 左上角标 + 右下旋转 180° 的镜像角标（经典双向牌面布局）。
            p.drawPixmap(cx + kCornerSrc.x(), cy + kCornerSrc.y(), corner);
            p.save();
            p.translate(cx + CW, cy + CH);
            p.rotate(180);
            p.drawPixmap(kCornerSrc.x(), kCornerSrc.y(), corner);
            p.restore();

            // 3) 中央相框：位置对齐原版人头牌装饰框（x30..111 / y14..175），
            //    3px 深色描边 + 素材底色填充。
            const QRect frame(cx + 30, cy + 14, 82, 162);
            p.fillRect(frame, QColor(0x3c, 0x43, 0x68));
            const QRect inner = frame.adjusted(3, 3, -3, -3);
            p.fillRect(inner, bg);

            // 4) 人像等宽缩放放进相框，略偏上（人物头部居中观感更好）。
            const int side = inner.width();
            const QPixmap scaled = portrait.scaled(side, side, Qt::KeepAspectRatio,
                                                   Qt::SmoothTransformation);
            p.drawPixmap(inner.x(), inner.y() + (inner.height() - side) * 2 / 5, scaled);
        }
    }
    return sheet;
}
