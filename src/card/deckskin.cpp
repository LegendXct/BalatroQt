#include "deckskin.h"

#include <QPainter>
#include <QPainterPath>

DeckSkin::Id DeckSkin::sCurrent = DeckSkin::Default;
int DeckSkin::sGeneration = 0;

namespace {
constexpr int CW = 142, CH = 190;   // 8BitDeck 每格尺寸（与 CardItem::SRC_W/H 一致）

// 角标（点数字母 + 小花色）在原版格子内的位置。对图集逐像素实测：四种花色、
// 所有点数的角标都落在 x12..27 / y14..49。程设牌组直接裁原版角标原位贴回，
// 保证字体、大小、位置与其他卡牌完全一致（设计图自带角标已在素材预处理时抹除）。
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
    case ChengShe: return QStringLiteral("程设牌组");
    case Default:  break;
    }
    return QStringLiteral("默认牌组");
}

const QPixmap &DeckSkin::baseSheet()
{
    static QPixmap base(QStringLiteral(":/textures/images/8BitDeck.png"));
    return base;
}

const QPixmap &DeckSkin::deckSheet()
{
    static QPixmap chengshe;
    if (sCurrent == ChengShe) {
        if (chengshe.isNull()) chengshe = buildChengSheSheet(baseSheet());
        if (!chengshe.isNull()) return chengshe;
    }
    return baseSheet();
}

QPixmap DeckSkin::buildChengSheSheet(const QPixmap &base)
{
    if (base.isNull()) return QPixmap();
    QPixmap sheet = base.copy();

    // 白色底片（Enhancers 第0行第1格）的 alpha 就是卡牌的圆角剪影：
    // 整卡设计图按它裁圆角后盖在白底上，轮廓与其它牌完全一致。
    QPixmap silhouette;
    {
        QPixmap enh(QStringLiteral(":/textures/images/Enhancers.png"));
        if (enh.isNull()) return QPixmap();
        silhouette = enh.copy(1 * CW, 0, CW, CH);
    }

    // 行 0=♥(h) 1=♣(c) 2=♦(d) 3=♠(s)，列 9..12 = J,Q,K,A（列号 = 点数-2），
    // 与 CardItem::deckSrcRect 的映射保持一致。素材是 16 张对齐过的整卡设计图
    // （deckskin_cs_<花色><点数>.png，284×380，已裁去底色与阴影、卡体统一居中）。
    const char suitLetters[4] = { 'h', 'c', 'd', 's' };
    const char rankLetters[4] = { 'J', 'Q', 'K', 'A' };

    QPainter p(&sheet);
    for (int row = 0; row < 4; ++row) {
        for (int i = 0; i < 4; ++i) {
            const QString res = QStringLiteral(":/textures/images/deckskin_cs_%1%2.png")
                                    .arg(QLatin1Char(suitLetters[row]))
                                    .arg(QLatin1Char(rankLetters[i]));
            const QPixmap card(res);
            if (card.isNull()) continue;

            // 整卡缩到格子大小，再用白底剪影裁出圆角。
            QPixmap cell(CW, CH);
            cell.fill(Qt::transparent);
            {
                QPainter cp(&cell);
                cp.setRenderHint(QPainter::SmoothPixmapTransform, true);
                cp.drawPixmap(QRect(0, 0, CW, CH), card);
                cp.setCompositionMode(QPainter::CompositionMode_DestinationIn);
                cp.drawPixmap(QRect(0, 0, CW, CH), silhouette);
            }

            const int cx = (9 + i) * CW, cy = row * CH;
            p.setCompositionMode(QPainter::CompositionMode_Clear);
            p.fillRect(QRect(cx, cy, CW, CH), Qt::transparent);
            p.setCompositionMode(QPainter::CompositionMode_SourceOver);
            p.drawPixmap(cx, cy, cell);

            // 原版 8-bit 角标原位贴回：左上 (12,14)，右下旋转 180° 镜像。
            const QPixmap corner = base.copy(cx + kCornerSrc.x(), cy + kCornerSrc.y(),
                                             kCornerSrc.width(), kCornerSrc.height());
            p.drawPixmap(cx + kCornerSrc.x(), cy + kCornerSrc.y(), corner);
            p.save();
            p.translate(cx + CW, cy + CH);
            p.rotate(180);
            p.drawPixmap(kCornerSrc.x(), kCornerSrc.y(), corner);
            p.restore();
        }
    }
    return sheet;
}

bool DeckSkin::enhancementOverArt(Rank rank, Enhancement enh)
{
    const bool fullArt = sCurrent == ChengShe && rank >= Rank::Jack;
    const bool bgEnhance = enh != Enhancement::None
                        && enh != Enhancement::Stone      // 石头不画卡面
                        && enh != Enhancement::Iterator;  // 迭代器有自己的装饰框
    return fullArt && bgEnhance;
}

void DeckSkin::drawEnhancementOverArt(QPainter *p,
                                      const QPixmap &enhSheet, const QRect &enhSrc,
                                      Rank rank, Suit suit, Enhancement enh)
{
    if (enhSheet.isNull() || enhSrc.isNull()) return;

    // 增强层：玻璃贴图自带半透明，整张直接叠加；其余增强不透明，
    // 中心开圆角窗只留"增强边框"，人像从窗口完整透出（双层都不再加额外透明度）。
    QPixmap overlay(CW, CH);
    overlay.fill(Qt::transparent);
    {
        QPainter op(&overlay);
        op.drawPixmap(QRect(0, 0, CW, CH), enhSheet, enhSrc);
        if (enh != Enhancement::Glass) {
            op.setCompositionMode(QPainter::CompositionMode_DestinationOut);
            QPainterPath window;
            window.addRoundedRect(QRectF(20, 24, CW - 40, CH - 48), 12, 12);
            op.fillPath(window, Qt::black);
        }
    }
    p->drawPixmap(QRect(0, 0, CW, CH), overlay);

    // 角标（点数字母+小花色）原位回贴：增强边框盖住了角标区域，从原版图集把
    // 透明底的 glyph 重新印到最上层——与默认牌"卡面在增强之上"的可读性一致。
    const int col = static_cast<int>(rank) - 2;
    int row = 0;
    switch (suit) {
    case Suit::Hearts:   row = 0; break;
    case Suit::Clubs:    row = 1; break;
    case Suit::Diamonds: row = 2; break;
    case Suit::Spades:   row = 3; break;
    }
    const QPixmap corner = baseSheet().copy(col * CW + kCornerSrc.x(),
                                            row * CH + kCornerSrc.y(),
                                            kCornerSrc.width(), kCornerSrc.height());
    p->drawPixmap(kCornerSrc.x(), kCornerSrc.y(), corner);
    p->save();
    p->translate(CW, CH);
    p->rotate(180);
    p->drawPixmap(kCornerSrc.x(), kCornerSrc.y(), corner);
    p->restore();
}
