#include "shadereffects.h"

#include <QDateTime>
#include <QPainterPath>
#include <QLinearGradient>
#include <QRadialGradient>
#include <QImage>
#include <QPixmap>
#include <QtMath>
#include <cmath>
#include <algorithm>
#include <QHash>
#include <QStringList>
#include <QString>

namespace BalatroShaders {
namespace {
constexpr double PI = 3.14159265358979323846;

static double clamp01(double v) { return std::max(0.0, std::min(1.0, v)); }
static int clamp255(double v) { return int(std::max(0.0, std::min(255.0, std::round(v)))); }
static double mix(double a, double b, double t) { return a + (b - a) * t; }
static double fract(double v) { return v - std::floor(v); }

static QColor mixColor(const QColor &a, const QColor &b, double t, int alphaOverride = -1)
{
    t = clamp01(t);
    QColor c(clamp255(mix(a.red(),   b.red(),   t)),
             clamp255(mix(a.green(), b.green(), t)),
             clamp255(mix(a.blue(),  b.blue(),  t)),
             clamp255(mix(a.alpha(), b.alpha(), t)));
    if (alphaOverride >= 0) c.setAlpha(alphaOverride);
    return c;
}

static QColor hslColor(double h, double s, double l, int alpha)
{
    QColor c;
    c.setHslF(fract(h), clamp01(s), clamp01(l), clamp01(alpha / 255.0));
    return c;
}

static void roundedClip(QPainter *p, const QRectF &r, qreal radius = 10)
{
    QPainterPath path;
    path.addRoundedRect(r.adjusted(2, 2, -2, -2), radius, radius);
    p->setClipPath(path);
}

static double shaderField(double u, double v, double time, double scale)
{
    // 原版 foil/holo/polychrome 使用同一组有机噪声场：3 个缓慢漂移的 length/sin/cos 叠加。
    const double sx = (u - 0.5) * scale;
    const double sy = (v - 0.5) * scale;
    const double p1x = sx + 50.0 * std::sin(-time / 143.6340);
    const double p1y = sy + 50.0 * std::cos(-time / 99.4324);
    const double p2x = sx + 50.0 * std::cos( time / 53.1532);
    const double p2y = sy + 50.0 * std::cos( time / 61.4532);
    const double p3x = sx + 50.0 * std::sin(-time / 87.53218);
    const double p3y = sy + 50.0 * std::sin(-time / 49.0000);

    const double field = (1.0 + (
        std::cos(std::hypot(p1x, p1y) / 19.483) +
        std::sin(std::hypot(p2x, p2y) / 33.155) * std::cos(p2y / 15.73) +
        std::cos(std::hypot(p3x, p3y) / 27.193) * std::sin(p3x / 21.92)
    )) / 2.0;
    return clamp01(field);
}

static QColor weightedBackgroundColour(const QColor &c1, const QColor &c2, const QColor &c3,
                                        double c1p, double c2p, double c3p, double contrast)
{
    // background.fs 第 49 行：不是普通 mix，而是 colour_1 的 bias + 三色权重叠加。
    const double safeContrast = std::max(0.05, contrast);
    const double bias = clamp01(0.3 / safeContrast);
    const double rest = 1.0 - bias;
    const double r = bias * c1.red()   + rest * (c1.red()   * c1p + c2.red()   * c2p + c3.red()   * c3p);
    const double g = bias * c1.green() + rest * (c1.green() * c1p + c2.green() * c2p + c3.green() * c3p);
    const double b = bias * c1.blue()  + rest * (c1.blue()  * c1p + c2.blue()  * c2p + c3.blue()  * c3p);
    return QColor(clamp255(r), clamp255(g), clamp255(b), 255);
}

static QImage makeEditionOverlay(const QSize &wanted, Edition edition, double intensity)
{
    const int w = std::max(24, wanted.width());
    const int h = std::max(32, wanted.height());

    // 牌面 edition shader 也不能在同一帧被多张牌反复 CPU 重算。
    // 量化到 24fps 后缓存，同一帧同尺寸直接复用，视觉仍然连续但输入延迟低很多。
    const int frame = int(shaderTime() * 24.0);
    const int intensityKey = int(std::round(intensity * 100.0));
    const QString key = QString::number(int(edition)) + QLatin1Char('|')
                      + QString::number(w) + QLatin1Char('x') + QString::number(h)
                      + QLatin1Char('|') + QString::number(intensityKey)
                      + QLatin1Char('|') + QString::number(frame);
    static QHash<QString, QImage> cache;
    static QStringList order;
    auto it = cache.constFind(key);
    if (it != cache.constEnd()) return it.value();

    QImage img(w, h, QImage::Format_ARGB32);
    img.fill(Qt::transparent);

    const double t = frame / 24.0;
    const double aspect = double(w) / std::max(1, h);

    for (int y = 0; y < h; ++y) {
        QRgb *line = reinterpret_cast<QRgb *>(img.scanLine(y));
        const double v = (y + 0.5) / h;
        for (int x = 0; x < w; ++x) {
            const double u = (x + 0.5) / w;
            QColor out(0, 0, 0, 0);

            if (edition == Edition::Polychrome) {
                // polychrome.fs：50 倍缩放的有机 field 推动 HSL hue，而不是简单 RGB 彩带。
                const double field = shaderField(u, v, t * 2.221, 50.0);
                const double res = 0.5 + 0.5 * std::cos(t * 2.612 + (field - 0.5) * PI);
                const double rim = 0.18 + 0.82 * std::pow(field, 0.72);
                const int alpha = clamp255((70.0 + 85.0 * rim) * intensity);
                out = hslColor(res + t * 0.04 + 0.06 * u, 0.92, 0.54 + 0.13 * field, alpha);
            } else if (edition == Edition::Holographic) {
                // holo.fs：更细密的 250 field + 三向网格 fac，让镭射像原版那样“晶格走色”。
                const double field = shaderField(u, v, t * 7.221, 250.0);
                const double res = 0.5 + 0.5 * std::cos(t * 2.612 + (field - 0.5) * PI);
                const double gridsize = 0.79;
                const double facA = std::max(0.0, 7.0 * std::abs(std::cos(u * gridsize * 20.0)) - 6.0);
                const double facB = std::max(0.0, 7.0 * std::cos(v * gridsize * 45.0 + u * gridsize * 20.0) - 6.0);
                const double facC = std::max(0.0, 7.0 * std::cos(v * gridsize * 45.0 - u * gridsize * 20.0) - 6.0);
                const double fac = 0.5 * std::max(facA, std::max(facB, facC));
                const int alpha = clamp255((34.0 + 78.0 * clamp01(res + fac)) * intensity);
                out = hslColor(0.57 + res + fac + 0.04 * std::sin(t + u * 6.0), 0.82, 0.66 + 0.16 * field, alpha);
            } else if (edition == Edition::Foil) {
                // foil.fs：径向银蓝高光 + 斜向角度高光 + 两组细波纹。
                double ax = (u - 0.5) * aspect;
                double ay = (v - 0.5);
                const double len90 = std::hypot(90.0 * ax, 90.0 * ay);
                const double len113 = std::hypot(113.1121 * ax, 113.1121 * ay);
                const double fac = clamp01(2.0 * std::sin((len90 + t * 2.0) + 3.0 * (1.0 + 0.8 * std::cos(len113 - t * 3.121))) - 1.0 - std::max(5.0 - len90, 0.0));
                const double rx = std::cos(t * 0.1221), ry = std::sin(t * 0.3512);
                const double denom = std::max(0.0001, std::hypot(rx, ry) * std::hypot(ax, ay));
                const double angle = (rx * ax + ry * ay) / denom;
                const double fac2 = clamp01(5.0 * std::cos(t * 0.3 + angle * PI * (2.2 + 0.9 * std::sin(t * 1.65 + 0.2 * t))) - 4.0 - std::max(2.0 - std::hypot(20.0 * ax, 20.0 * ay), 0.0));
                const double fac3 = 0.3 * std::max(-1.0, std::min(1.0, 2.0 * std::sin(t * 5.0 + u * 3.0 + 3.0 * (1.0 + 0.5 * std::cos(t * 7.0))) - 1.0));
                const double fac4 = 0.3 * std::max(-1.0, std::min(1.0, 2.0 * std::sin(t * 6.66 + v * 3.8 + 3.0 * (1.0 + 0.5 * std::cos(t * 3.414))) - 1.0));
                const double maxfac = std::max(std::max(fac, std::max(fac2, std::max(fac3, std::max(fac4, 0.0)))) + 2.2 * (fac + fac2 + fac3 + fac4), 0.0);
                const int alpha = clamp255((28.0 + 118.0 * clamp01(maxfac * 0.32)) * intensity);
                out = QColor(180 + clamp255(58.0 * clamp01(maxfac)),
                             210 + clamp255(38.0 * clamp01(maxfac)),
                             255,
                             alpha);
            } else if (edition == Edition::Negative) {
                // negative_shine.fs：多组 sin fac 叠出紫蓝反相高光。
                const double fac  = 0.8 + 0.9 * std::sin(11.0 * u + 4.32 * v + t * 12.0 + std::cos(t * 5.3 + v * 4.2 - u * 4.0));
                const double fac2 = 0.5 + 0.5 * std::sin(8.0 * u + 2.32 * v + t * 5.0 - std::cos(t * 2.3 + u * 8.2));
                const double fac3 = 0.5 + 0.5 * std::sin(10.0 * u + 5.32 * v + t * 6.111 + std::sin(t * 5.3 + v * 3.2));
                const double fac4 = 0.5 + 0.5 * std::sin(3.0 * u + 2.32 * v + t * 8.111 + std::sin(t * 1.3 + v * 11.2));
                const double fac5 = std::sin(14.4 * u + 5.32 * v + t * 12.0 + std::cos(t * 5.3 + v * 4.2 - u * 4.0));
                const double maxfac = 0.7 * std::max(std::max(fac, std::max(fac2, std::max(fac3, 0.0))) + (fac + fac2 + fac3 * fac4), 0.0);
                const int alpha = clamp255((30.0 + 78.0 * clamp01(maxfac * 0.28)) * intensity);
                out = QColor(clamp255(110 + 95 * clamp01(maxfac * (0.7 + fac5 * 0.27))),
                             clamp255(55 + 105 * clamp01(maxfac * (0.7 - fac5 * 0.27))),
                             clamp255(170 + 80 * clamp01(maxfac * 0.7)),
                             alpha);
            }

            line[x] = out.rgba();
        }
    }
    cache.insert(key, img);
    order.append(key);
    while (order.size() > 72) {
        cache.remove(order.takeFirst());
    }
    return img;
}

static void paintMovingStripe(QPainter *p, const QRectF &r, const QColor &c, double speed, double widthFac = 0.34)
{
    const double t = shaderTime();
    const qreal stripeX = r.left() - r.width() * 0.8 + std::fmod(t * speed, 1.0) * r.width() * 2.4;
    QLinearGradient stripe(QPointF(stripeX, r.top()), QPointF(stripeX + r.width() * widthFac, r.bottom()));
    QColor z = c; z.setAlpha(0);
    stripe.setColorAt(0.00, z);
    stripe.setColorAt(0.45, c);
    stripe.setColorAt(0.55, c.lighter(160));
    stripe.setColorAt(1.00, z);
    p->fillRect(r, stripe);
}

static QPixmap makeSoulForeground(const QPixmap &tarotSheet)
{
    // 只从 Tarots.png 的 Spectral_Soul {x=2,y=2} 提取中心白水晶/旋涡。
    // 这样不会使用错误的 Enhancers 白底层，比例也跟原版灵魂牌一致。
    constexpr int W = 142, H = 190;
    if (tarotSheet.isNull()) return QPixmap();
    QImage src = tarotSheet.copy(2 * W, 2 * H, W, H).toImage().convertToFormat(QImage::Format_ARGB32);
    QImage out(W, H, QImage::Format_ARGB32);
    out.fill(Qt::transparent);

    for (int y = 0; y < H; ++y) {
        for (int x = 0; x < W; ++x) {
            const QColor c = QColor::fromRgba(src.pixel(x, y));
            if (c.alpha() < 10) continue;

            // 中心层范围；避开外框、牌名、顶部文字和底部边框。
            if (x < 22 || x > 120 || y < 27 || y > 150) continue;

            const int maxc = std::max({c.red(), c.green(), c.blue()});
            const int minc = std::min({c.red(), c.green(), c.blue()});
            const int lum = int(0.299 * c.red() + 0.587 * c.green() + 0.114 * c.blue());
            const bool whiteCrystal = lum > 118 && (maxc - minc) < 90;
            const bool blueSoulLine = c.blue() > 115 && c.blue() > c.red() + 12 && c.green() > 70;
            const bool yellowSpark = c.red() > 155 && c.green() > 125 && c.blue() < 135;

            if (whiteCrystal || blueSoulLine || yellowSpark) {
                QColor o = c;
                int alpha = c.alpha();
                // 中心越接近，越完整；边缘渐隐，避免把整张牌抠出来。
                const double dx = (x - W * 0.5) / (W * 0.43);
                const double dy = (y - H * 0.48) / (H * 0.47);
                const double radial = clamp01(1.22 - std::sqrt(dx * dx + dy * dy));
                alpha = int(alpha * clamp01(0.20 + radial * 1.15));
                o.setAlpha(alpha);
                out.setPixelColor(x, y, o);
            }
        }
    }
    return QPixmap::fromImage(out);
}
} // namespace

double shaderTime()
{
    return QDateTime::currentMSecsSinceEpoch() / 1000.0;
}

QImage makeBackgroundImage(const QSizeF &logicalSize, const QColor &c1, const QColor &c2, const QColor &c3,
                           double contrast, double spinAmount, double spinTimeScale, double time)
{
    if (logicalSize.width() <= 1 || logicalSize.height() <= 1) return QImage();
    if (time < 0.0) time = shaderTime();
    const double spinTime = time * spinTimeScale;

    // 原版是 GPU shader，Qt Widgets 这里必须用 CPU 生成一张连续场图。
    // 关键是 60fps 下不要在 paint() 里重复算整屏：DynamicBackgroundItem 会缓存此帧，paint 只负责拉伸绘制。
    // Qt Widgets 这里是 CPU 近似，不是原版 GPU shader。长边控制在 240，
    // 再由 SmoothPixmapTransform 放大，能保持平滑，同时避免开局拖垮主线程。
    const int maxSide = 240;
    const double maxLogicalSide = std::max(logicalSize.width(), logicalSize.height());
    const double scale = std::min(1.0, maxSide / std::max(1.0, maxLogicalSide));
    const int iw = std::max(128, int(std::ceil(logicalSize.width() * scale)));
    const int ih = std::max(72,  int(std::ceil(logicalSize.height() * scale)));
    QImage img(iw, ih, QImage::Format_RGB32);

    const double w = logicalSize.width();
    const double h = logicalSize.height();
    const double cx = w * 0.5;
    const double cy = h * 0.5;
    const double norm = std::hypot(w, h);
    const double contrastMod = 0.25 * contrast + 0.5 * spinAmount + 1.2;

    for (int y = 0; y < ih; ++y) {
        QRgb *line = reinterpret_cast<QRgb *>(img.scanLine(y));
        const double sy = (y + 0.5) * h / ih;
        for (int x = 0; x < iw; ++x) {
            const double sx = (x + 0.5) * w / iw;

            double ux = ((sx - cx) / norm) - 0.12;
            double uy = ((sy - cy) / norm);
            const double uvLen = std::hypot(ux, uy);

            double speed = (spinTime * 0.5 * 0.2) + 302.2;
            const double angle = std::atan2(uy, ux) + speed - 0.5 * 20.0 * (spinAmount * uvLen + (1.0 - spinAmount));
            ux = uvLen * std::cos(angle);
            uy = uvLen * std::sin(angle);

            ux *= 30.0;
            uy *= 30.0;
            double u2x = ux + uy;
            double u2y = ux + uy;
            speed = time * 2.0;
            for (int i = 0; i < 5; ++i) {
                const double mx = std::max(ux, uy);
                u2x += std::sin(mx) + ux;
                u2y += std::sin(mx) + uy;
                ux += 0.5 * std::cos(5.1123314 + 0.353 * u2y + speed * 0.131121);
                uy += 0.5 * std::sin(u2x - 0.113 * speed);
                const double v = std::cos(ux + uy) - std::sin(ux * 0.711 - uy);
                ux -= v;
                uy -= v;
            }

            const double paint = std::min(2.0, std::max(0.0, std::hypot(ux, uy) * 0.035 * contrastMod));
            const double c1p = std::max(0.0, 1.0 - contrastMod * std::abs(1.0 - paint));
            const double c2p = std::max(0.0, 1.0 - contrastMod * std::abs(paint));
            const double c3p = 1.0 - std::min(1.0, c1p + c2p);
            const QColor col = weightedBackgroundColour(c1, c2, c3, c1p, c2p, c3p, contrast);
            line[x] = qRgb(col.red(), col.green(), col.blue());
        }
    }
    return img;
}

void paintBackground(QPainter *p, const QRectF &rect, const QColor &c1, const QColor &c2, const QColor &c3,
                     double contrast, double spinAmount, double spinTimeScale, double time)
{
    if (!p || rect.width() <= 1 || rect.height() <= 1) return;
    const QImage img = makeBackgroundImage(rect.size(), c1, c2, c3, contrast, spinAmount, spinTimeScale, time);
    if (img.isNull()) return;

    p->save();
    p->setRenderHint(QPainter::SmoothPixmapTransform, true);
    p->setPen(Qt::NoPen);
    p->drawImage(rect, img, QRectF(img.rect()));
    // 原版整体带非常轻的 CRT / 暗角感，但不能盖成脏块。
    paintCrtOverlay(p, rect, 0.07);
    p->restore();
}

void paintCrtOverlay(QPainter *p, const QRectF &rect, double opacity)
{
    if (!p) return;
    p->save();
    p->setPen(Qt::NoPen);
    for (int y = int(rect.top()); y < rect.bottom(); y += 4) {
        QColor line(0, 0, 0, int(70 * opacity));
        p->fillRect(QRectF(rect.left(), y, rect.width(), 1.0), line);
    }
    QRadialGradient vignette(rect.center(), std::max(rect.width(), rect.height()) * 0.68);
    vignette.setColorAt(0.0, QColor(0, 0, 0, 0));
    vignette.setColorAt(0.74, QColor(0, 0, 0, int(42 * opacity)));
    vignette.setColorAt(1.0, QColor(0, 0, 0, int(155 * opacity)));
    p->fillRect(rect, vignette);
    p->restore();
}

void paintEdition(QPainter *p, const QRectF &r, Edition edition, double intensity)
{
    if (!p || edition == Edition::None) return;
    const double t = shaderTime();

    p->save();
    p->setRenderHint(QPainter::Antialiasing, true);
    p->setRenderHint(QPainter::SmoothPixmapTransform, true);
    roundedClip(p, r, 10);

    const QSize overlaySize(std::max(48, int(std::round(r.width()))),
                            std::max(64, int(std::round(r.height()))));
    const QImage overlay = makeEditionOverlay(overlaySize, edition, intensity);

    if (edition == Edition::Negative) {
        // negative.fs 先反相/HSL 翻转，再加灰蓝底；这里用 Difference + Multiply + negative_shine 场近似。
        p->setCompositionMode(QPainter::CompositionMode_Difference);
        p->fillRect(r, QColor(235, 238, 245, clamp255(160 * intensity)));
        p->setCompositionMode(QPainter::CompositionMode_Multiply);
        p->fillRect(r, QColor(70, 48, 96, clamp255(105 * intensity)));
        p->setCompositionMode(QPainter::CompositionMode_Screen);
        p->drawImage(r, overlay, QRectF(overlay.rect()));
        paintNegativeShine(p, r, intensity * 0.95);
    } else if (edition == Edition::Foil) {
        p->setCompositionMode(QPainter::CompositionMode_Screen);
        p->drawImage(r, overlay, QRectF(overlay.rect()));
        paintMovingStripe(p, r, QColor(255, 255, 255, int(58 * intensity)), 0.58, 0.27);
    } else if (edition == Edition::Holographic) {
        p->setCompositionMode(QPainter::CompositionMode_Screen);
        p->drawImage(r, overlay, QRectF(overlay.rect()));
        // 镭射的晶格线应很细，不再画大块彩色条纹。
        p->setPen(QPen(QColor(230, 250, 255, int(34 * intensity)), 0.8));
        for (int i = -4; i < 14; ++i) {
            const qreal x = r.left() + i * 13 + std::fmod(t * 28, 13.0);
            p->drawLine(QPointF(x, r.top()), QPointF(x + r.width() * 0.52, r.bottom()));
        }
    } else if (edition == Edition::Polychrome) {
        p->setCompositionMode(QPainter::CompositionMode_Screen);
        p->drawImage(r, overlay, QRectF(overlay.rect()));
        p->setCompositionMode(QPainter::CompositionMode_SourceOver);
        QLinearGradient gloss(r.topLeft(), r.bottomRight());
        QColor z(255,255,255,0);
        gloss.setColorAt(0.0, z);
        gloss.setColorAt(0.48 + 0.16 * std::sin(t * 0.9), QColor(255,255,255,int(24 * intensity)));
        gloss.setColorAt(1.0, z);
        p->fillRect(r, gloss);
    }

    // 原 shader 不会出现粗荧光边框；只留很轻的边缘辉光，避免“贴纸边框感”。
    p->setClipping(false);
    p->setCompositionMode(QPainter::CompositionMode_SourceOver);
    QColor edge;
    switch (edition) {
    case Edition::Foil:        edge = QColor(205, 238, 255, 115); break;
    case Edition::Holographic: edge = QColor(255, 180, 245, 100); break;
    case Edition::Polychrome:  edge = QColor::fromHsv(int(std::fmod(t * 70, 360.0)), 150, 255, 105); break;
    case Edition::Negative:    edge = QColor(205, 145, 255, 130); break;
    default: break;
    }
    p->setPen(QPen(edge, edition == Edition::Negative ? 1.7 : 1.35));
    p->setBrush(Qt::NoBrush);
    p->drawRoundedRect(r.adjusted(2, 2, -2, -2), 10, 10);
    p->restore();
}

void paintNegativeShine(QPainter *p, const QRectF &r, double intensity)
{
    if (!p) return;
    const double t = shaderTime();
    p->save();
    p->setCompositionMode(QPainter::CompositionMode_Screen);

    const QImage shine = makeEditionOverlay(QSize(std::max(48, int(r.width())), std::max(64, int(r.height()))),
                                            Edition::Negative, intensity * 0.72);
    p->drawImage(r, shine, QRectF(shine.rect()));

    QRadialGradient rg(QPointF(r.center().x() + std::sin(t * 1.1) * r.width() * 0.25,
                              r.center().y() + std::cos(t * 0.9) * r.height() * 0.18),
                       r.width() * 0.85);
    rg.setColorAt(0.0, QColor(255, 105, 255, int(42 * intensity)));
    rg.setColorAt(0.42, QColor(45, 235, 255, int(30 * intensity)));
    rg.setColorAt(1.0, QColor(0, 0, 0, 0));
    p->fillRect(r, rg);
    p->restore();
}

void paintBoosterShader(QPainter *p, const QRectF &r, double intensity)
{
    if (!p) return;
    const double t = shaderTime();
    p->save();
    p->setRenderHint(QPainter::Antialiasing, true);
    roundedClip(p, r, 12);
    p->setCompositionMode(QPainter::CompositionMode_Screen);
    for (int i = 0; i < 6; ++i) {
        const double freq = 7.0 + i * 1.9;
        const qreal y = r.top() + std::fmod(t * (24 + i * 3) + i * 29.0, r.height());
        QLinearGradient g(QPointF(r.left(), y - 30), QPointF(r.right(), y + 30));
        QColor a(90, 90, 230, 0), b(185, 215, 255, int((28 + i * 4) * intensity));
        if (std::sin(t * 1.7 + freq) > 0) b = QColor(255, 205, 115, int((18 + i * 4) * intensity));
        g.setColorAt(0.0, a); g.setColorAt(0.5, b); g.setColorAt(1.0, a);
        p->fillRect(r, g);
    }
    paintMovingStripe(p, r, QColor(255, 255, 255, int(80 * intensity)), 0.40, 0.22);
    p->restore();
}

void paintVoucherShader(QPainter *p, const QRectF &r, double intensity)
{
    if (!p) return;
    const double t = shaderTime();
    p->save();
    roundedClip(p, r, 10);
    p->setCompositionMode(QPainter::CompositionMode_Screen);
    QLinearGradient g(r.topLeft(), r.bottomRight());
    g.setColorAt(0.00, QColor(90, 245, 255, int(42 * intensity)));
    g.setColorAt(0.45 + 0.12 * std::sin(t), QColor(255, 255, 255, int(56 * intensity)));
    g.setColorAt(1.00, QColor(255, 180, 80, int(35 * intensity)));
    p->fillRect(r, g);
    paintMovingStripe(p, r, QColor(255, 255, 255, int(90 * intensity)), 0.35, 0.25);
    p->restore();
}

void paintHologramShader(QPainter *p, const QRectF &r, double intensity)
{
    if (!p) return;
    p->save();
    roundedClip(p, r, 10);
    p->setCompositionMode(QPainter::CompositionMode_Screen);
    const QImage overlay = makeEditionOverlay(QSize(std::max(48, int(r.width())), std::max(64, int(r.height()))),
                                              Edition::Holographic, intensity);
    p->drawImage(r, overlay, QRectF(overlay.rect()));
    p->restore();
}

void paintDissolveGlow(QPainter *p, const QRectF &r, const QColor &burn1, const QColor &burn2, double intensity)
{
    if (!p) return;
    const double t = shaderTime();
    p->save();
    p->setCompositionMode(QPainter::CompositionMode_Screen);
    for (int i = 0; i < 3; ++i) {
        QRadialGradient rg(QPointF(r.center().x() + std::sin(t * 1.3 + i) * r.width() * 0.12,
                                  r.center().y() + std::cos(t * 1.1 + i) * r.height() * 0.10),
                           r.width() * (0.45 + i * 0.12));
        QColor a = (i % 2) ? burn1 : burn2;
        a.setAlpha(int(a.alpha() * intensity));
        rg.setColorAt(0.0, a);
        a.setAlpha(0);
        rg.setColorAt(1.0, a);
        p->fillRect(r, rg);
    }
    p->restore();
}

void paintDebuff(QPainter *p, const QRectF &r, double intensity)
{
    if (!p) return;
    // debuff.fs：降饱和 + 红叉线。这里叠一层半透明黑灰和动态红叉。
    const double t = shaderTime();
    p->save();
    roundedClip(p, r, 10);
    p->setCompositionMode(QPainter::CompositionMode_SourceOver);
    p->fillRect(r, QColor(0, 0, 0, int(105 * intensity)));
    p->setCompositionMode(QPainter::CompositionMode_Screen);
    p->fillRect(r, QColor(160, 160, 170, int(35 * intensity)));
    p->setCompositionMode(QPainter::CompositionMode_SourceOver);
    QColor red(255, 70 + int(30 * std::sin(t * 5.0)), 70, 230);
    p->setPen(QPen(red, 4.2, Qt::SolidLine, Qt::RoundCap));
    p->drawLine(r.topLeft() + QPointF(10, 10), r.bottomRight() - QPointF(10, 10));
    p->drawLine(QPointF(r.right() - 10, r.top() + 10), QPointF(r.left() + 10, r.bottom() - 10));
    p->restore();
}

void paintFlame(QPainter *p, const QRectF &r, double intensity)
{
    if (!p) return;
    const double t = shaderTime();
    p->save();
    p->setRenderHint(QPainter::Antialiasing, true);
    p->setCompositionMode(QPainter::CompositionMode_Screen);
    for (int i = 0; i < 12; ++i) {
        qreal x = r.left() + r.width() * (i + 0.5) / 12.0;
        qreal h = r.height() * (0.35 + 0.25 * std::sin(t * 5.0 + i * 0.9));
        QPainterPath flame;
        flame.moveTo(x, r.bottom());
        flame.cubicTo(x - 12, r.bottom() - h * 0.45, x - 4, r.bottom() - h, x + std::sin(t * 3 + i) * 6, r.bottom() - h * 1.12);
        flame.cubicTo(x + 14, r.bottom() - h * 0.55, x + 8, r.bottom() - h * 0.2, x, r.bottom());
        QLinearGradient g(QPointF(x, r.bottom()), QPointF(x, r.bottom() - h));
        g.setColorAt(0.0, QColor(255, 50, 30, int(115 * intensity)));
        g.setColorAt(0.45, QColor(255, 155, 35, int(95 * intensity)));
        g.setColorAt(1.0, QColor(255, 255, 135, int(55 * intensity)));
        p->fillPath(flame, g);
    }
    p->restore();
}

void paintGoldSealGlow(QPainter *p, const QRectF &r, double intensity)
{
    if (!p) return;
    const double t = shaderTime();
    p->save();
    p->setCompositionMode(QPainter::CompositionMode_Screen);
    QRadialGradient rg(QPointF(r.left() + r.width() * 0.75, r.top() + r.height() * 0.18), r.width() * 0.38);
    rg.setColorAt(0.0, QColor(255, 255, 210, int((75 + 35 * std::sin(t * 3.2)) * intensity)));
    rg.setColorAt(0.52, QColor(255, 190, 60, int(45 * intensity)));
    rg.setColorAt(1.0, QColor(255, 190, 60, 0));
    p->fillRect(r, rg);
    p->restore();
}

void paintSoulCrystal(QPainter *p, const QRectF &rect, const QPixmap &tarotSheet)
{
    if (!p) return;
    static QPixmap cached;
    static quint64 key = 0;
    quint64 currentKey = tarotSheet.cacheKey();
    if (cached.isNull() || key != currentKey) {
        cached = makeSoulForeground(tarotSheet);
        key = currentKey;
    }

    const double t = shaderTime();
    const double frac = t - std::floor(t);
    const double scaleMod = 0.05 + 0.05 * std::sin(1.8 * t) + 0.07 * std::sin(frac * PI * 14.0) * std::pow(1.0 - frac, 3.0);
    const double rotateMod = 0.10 * std::sin(1.219 * t) + 0.07 * std::sin(t * PI * 5.0) * std::pow(1.0 - frac, 2.0);

    p->save();
    p->setRenderHint(QPainter::SmoothPixmapTransform, true);
    p->setRenderHint(QPainter::Antialiasing, true);

    // 原版 shared_soul 也是整卡尺寸 sprite，但只在中心有像素。必须整卡对齐，不能局部缩小。
    const QPointF c = rect.center();
    p->translate(c);
    p->rotate(rotateMod * 180.0 / PI);
    p->scale(1.0 + scaleMod, 1.0 + scaleMod);
    p->translate(-c);

    paintDissolveGlow(p, rect, QColor(255,255,255,92), QColor(120,180,255,72), 1.2);
    if (!cached.isNull()) {
        p->setOpacity(0.98);
        p->drawPixmap(rect, cached, QRectF(0, 0, cached.width(), cached.height()));
        p->setOpacity(1.0);
        p->setCompositionMode(QPainter::CompositionMode_Screen);
        paintMovingStripe(p, rect.adjusted(12, 20, -12, -22), QColor(255,255,255,95), 0.55, 0.30);
    }
    p->restore();
}

QPixmap makeBooster3DPixmap(const QPixmap &base)
{
    if (base.isNull()) return QPixmap();
    // 几乎不留透明边；靠按钮本身控制大小，避免“卡包变小”。
    QPixmap pix(base.width() + 10, base.height() + 14);
    pix.fill(Qt::transparent);
    QPainter p(&pix);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::SmoothPixmapTransform, true);

    QRectF r(4, 2, base.width(), base.height());
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(0,0,0,130));
    p.drawRoundedRect(r.translated(5, 9), 11, 11);

    // 厚度层：右/下侧边。
    QPainterPath side;
    side.addRoundedRect(r.translated(4, 6), 10, 10);
    QLinearGradient sg(r.topRight(), r.bottomRight() + QPointF(6, 7));
    sg.setColorAt(0.0, QColor(255,255,255,52));
    sg.setColorAt(0.5, QColor(55,55,65,72));
    sg.setColorAt(1.0, QColor(0,0,0,110));
    p.fillPath(side, sg);

    p.save();
    p.translate(r.center());
    p.rotate(-1.7);
    QRectF target(-base.width()/2.0, -base.height()/2.0, base.width(), base.height());
    p.drawPixmap(target, base, base.rect());
    paintBoosterShader(&p, target, 0.9);
    p.restore();

    return pix;
}

} // namespace BalatroShaders
