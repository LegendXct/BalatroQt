#ifndef BALATRO_SHADER_EFFECTS_H
#define BALATRO_SHADER_EFFECTS_H

#include <QColor>
#include <QPixmap>
#include <QPainter>
#include <QRectF>
#include <QSizeF>
#include <QPointF>
#include <QImage>
#include "../card/carddata.h"

namespace BalatroShaders {

// 这些函数不是直接加载 GLSL（Qt Widgets 项目没有 Love2D shader 管线），
// 而是把你们找到的原版 .fs 里的 time/sin/uv/colour_1/2/3 等公式转成 QPainter 可画的层。
// 目标是：同一套效果用于普通牌、小丑、商店商品、补充包、优惠券和动态背景。

double shaderTime();

QImage makeBackgroundImage(const QSizeF &logicalSize,
                           const QColor &colour1,
                           const QColor &colour2,
                           const QColor &colour3,
                           double contrast = 3.0,
                           double spinAmount = 0.08,
                           double spinTimeScale = 1.0,
                           double time = -1.0);

void paintBackground(QPainter *p,
                     const QRectF &rect,
                     const QColor &colour1,
                     const QColor &colour2,
                     const QColor &colour3,
                     double contrast = 3.0,
                     double spinAmount = 0.08,
                     double spinTimeScale = 1.0,
                     double time = -1.0);

void paintCrtOverlay(QPainter *p, const QRectF &rect, double opacity = 0.18);

void paintEdition(QPainter *p, const QRectF &rect, Edition edition, double intensity = 1.0);
void paintNegativeShine(QPainter *p, const QRectF &rect, double intensity = 1.0);
void paintBoosterShader(QPainter *p, const QRectF &rect, double intensity = 1.0);
void paintVoucherShader(QPainter *p, const QRectF &rect, double intensity = 1.0);
void paintHologramShader(QPainter *p, const QRectF &rect, double intensity = 1.0);
void paintDissolveGlow(QPainter *p, const QRectF &rect,
                       const QColor &burn1 = QColor(255, 255, 255, 80),
                       const QColor &burn2 = QColor(120, 180, 255, 50),
                       double intensity = 1.0);
void paintDebuff(QPainter *p, const QRectF &rect, double intensity = 1.0);
void paintFlame(QPainter *p, const QRectF &rect, double intensity = 1.0);
void paintGoldSealGlow(QPainter *p, const QRectF &rect, double intensity = 1.0);

// The Soul 的 shared_soul 前景层：原版 game.lua 把 shared_soul 直接放在 ASSET_ATLAS["centers"]
// (即 Enhancers.png) 的 {x=0,y=1} 格子里——是一块完整的白水晶贴图，不是从塔罗中抠像素。
// 这里只要按 card.lua: 4503-4509 的 scale_mod / rotate_mod / dissolve glow 浮动叠层即可。
void paintSoulCrystal(QPainter *p, const QRectF &rect, const QPixmap &enhancersSheet);

// 给 shop 里平面卡包做接近 booster.fs 的立体贴图；返回的 pixmap 本体留边很小，不会再被 QIcon 压小。
QPixmap makeBooster3DPixmap(const QPixmap &base);

} // namespace BalatroShaders

#endif
