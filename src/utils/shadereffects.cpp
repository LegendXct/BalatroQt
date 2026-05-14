#include "shadereffects.h"
#include "gpueffectsrenderer.h"

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
#include <QString>
#include <QStringList>

namespace BalatroShaders {
namespace {
constexpr double PI = 3.14159265358979323846;

enum class ShaderKind {
    Foil,
    Holo,
    Polychrome,
    Negative,
    NegativeShine,
    Booster,
    Voucher,
    Debuff,
    Played,
    Dissolve,
    Hologram,
    GoldSeal
};

constexpr double FX_CACHE_FPS = 15.0;

static bool kindUsesAnimatedFrame(ShaderKind kind, double dissolve)
{
    if (dissolve > 0.001) return true;
    switch (kind) {
    case ShaderKind::Dissolve:
    case ShaderKind::Hologram:
        return true;
    case ShaderKind::Foil:
    case ShaderKind::Holo:
    case ShaderKind::Polychrome:
    case ShaderKind::Negative:
    case ShaderKind::NegativeShine:
    case ShaderKind::Booster:
    case ShaderKind::Voucher:
    case ShaderKind::Debuff:
    case ShaderKind::Played:
    case ShaderKind::GoldSeal:
        return false;
    }
    return false;
}

struct V2 { double x = 0.0; double y = 0.0; };
struct V4 { double r = 0.0; double g = 0.0; double b = 0.0; double a = 0.0; };

static double clamp01(double v) { return std::max(0.0, std::min(1.0, v)); }
static double clamp(double v, double lo, double hi) { return std::max(lo, std::min(hi, v)); }
static int clamp255(double v) { return int(std::max(0.0, std::min(255.0, std::round(v)))); }
static double mix(double a, double b, double t) { return a + (b - a) * t; }
static double fract(double v) { return v - std::floor(v); }
static double modp(double x, double y) { return x - y * std::floor(x / y); }
static double len(V2 v) { return std::hypot(v.x, v.y); }
static V2 add(V2 a, V2 b) { return {a.x + b.x, a.y + b.y}; }
static V2 sub(V2 a, V2 b) { return {a.x - b.x, a.y - b.y}; }
static V2 mul(V2 a, double k) { return {a.x * k, a.y * k}; }
static V4 add(V4 a, V4 b) { return {a.r + b.r, a.g + b.g, a.b + b.b, a.a + b.a}; }
static V4 mul(V4 a, double k) { return {a.r * k, a.g * k, a.b * k, a.a * k}; }
static V4 mul(V4 a, V4 b) { return {a.r * b.r, a.g * b.g, a.b * b.b, a.a * b.a}; }
static V4 mix(V4 a, V4 b, double t) { return {mix(a.r,b.r,t), mix(a.g,b.g,t), mix(a.b,b.b,t), mix(a.a,b.a,t)}; }

static QColor rgb(int r, int g, int b, int a = 255)
{
    return QColor(clamp255(r), clamp255(g), clamp255(b), clamp255(a));
}

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

static V4 fromColor(const QColor &c)
{
    return {c.redF(), c.greenF(), c.blueF(), c.alphaF()};
}

static V4 fromPixel(QRgb px)
{
    return {qRed(px) / 255.0, qGreen(px) / 255.0, qBlue(px) / 255.0, qAlpha(px) / 255.0};
}

static QRgb toPixel(V4 c)
{
    return qRgba(clamp255(clamp01(c.r) * 255.0),
                 clamp255(clamp01(c.g) * 255.0),
                 clamp255(clamp01(c.b) * 255.0),
                 clamp255(clamp01(c.a) * 255.0));
}

static V4 over(V4 bottom, V4 top)
{
    const double a = top.a + bottom.a * (1.0 - top.a);
    if (a <= 0.00001) return {0,0,0,0};
    return {
        (top.r * top.a + bottom.r * bottom.a * (1.0 - top.a)) / a,
        (top.g * top.a + bottom.g * bottom.a * (1.0 - top.a)) / a,
        (top.b * top.a + bottom.b * bottom.a * (1.0 - top.a)) / a,
        a
    };
}

static double hue(double s, double t, double h)
{
    const double hs = modp(h, 1.0) * 6.0;
    if (hs < 1.0) return (t - s) * hs + s;
    if (hs < 3.0) return t;
    if (hs < 4.0) return (t - s) * (4.0 - hs) + s;
    return s;
}

static V4 RGB(V4 c)
{
    if (c.g < 0.0001) return {c.b, c.b, c.b, c.a};
    const double t = (c.b < 0.5) ? c.g * c.b + c.b : -c.g * c.b + (c.g + c.b);
    const double s = 2.0 * c.b - t;
    return {hue(s, t, c.r + 1.0 / 3.0), hue(s, t, c.r), hue(s, t, c.r - 1.0 / 3.0), c.a};
}

static V4 HSL(V4 c)
{
    const double low = std::min(c.r, std::min(c.g, c.b));
    const double high = std::max(c.r, std::max(c.g, c.b));
    const double delta = high - low;
    const double sum = high + low;
    V4 hsl{0.0, 0.0, 0.5 * sum, c.a};
    if (std::abs(delta) < 0.0000001) return hsl;
    hsl.g = (hsl.b < 0.5) ? delta / std::max(0.000001, sum) : delta / std::max(0.000001, 2.0 - sum);
    if (high == c.r) hsl.r = (c.g - c.b) / delta;
    else if (high == c.g) hsl.r = (c.b - c.r) / delta + 2.0;
    else hsl.r = (c.r - c.g) / delta + 4.0;
    hsl.r = modp(hsl.r / 6.0, 1.0);
    return hsl;
}

static double shaderField(V2 uvScaledCentered, double t)
{
    V2 p1 = add(uvScaledCentered, {50.0 * std::sin(-t / 143.6340), 50.0 * std::cos(-t / 99.4324)});
    V2 p2 = add(uvScaledCentered, {50.0 * std::cos( t / 53.1532), 50.0 * std::cos( t / 61.4532)});
    V2 p3 = add(uvScaledCentered, {50.0 * std::sin(-t / 87.53218), 50.0 * std::sin(-t / 49.0000)});
    return (1.0 + (std::cos(len(p1) / 19.483) + std::sin(len(p2) / 33.155) * std::cos(p2.y / 15.73) +
                   std::cos(len(p3) / 27.193) * std::sin(p3.x / 21.92))) / 2.0;
}

static void roundedClip(QPainter *p, const QRectF &r, qreal radius = 10)
{
    QPainterPath path;
    path.addRoundedRect(r.adjusted(2, 2, -2, -2), radius, radius);
    p->setClipPath(path);
}

static V4 dissolveMask(V4 tex, V2 uv, int w, int h, double dissolve, double seedTime,
                       bool shadow = false,
                       V4 burn1 = {1,1,1,0}, V4 burn2 = {1,1,1,0})
{
    if (dissolve < 0.001) {
        if (shadow) return {0, 0, 0, tex.a * 0.3};
        return tex;
    }

    const double adjusted = (dissolve * dissolve * (3.0 - 2.0 * dissolve)) * 1.02 - 0.01;
    const double t = seedTime * 10.0 + 2003.0;
    const double maxSide = std::max(w, h);
    V2 floored{std::floor(uv.x * w) / maxSide, std::floor(uv.y * h) / maxSide};
    V2 uvScaled = mul(sub(floored, {0.5, 0.5}), 2.3 * maxSide);
    const double field = shaderField(uvScaled, t);
    const double bx = 0.2, by = 0.8;
    double res = (0.5 + 0.5 * std::cos(adjusted / 82.612 + (field - 0.5) * 3.14));
    res -= (floored.x > by ? (floored.x - by) * (5.0 + 5.0 * dissolve) : 0.0) * dissolve;
    res -= (floored.y > by ? (floored.y - by) * (5.0 + 5.0 * dissolve) : 0.0) * dissolve;
    res -= (floored.x < bx ? (bx - floored.x) * (5.0 + 5.0 * dissolve) : 0.0) * dissolve;
    res -= (floored.y < bx ? (bx - floored.y) * (5.0 + 5.0 * dissolve) : 0.0) * dissolve;

    if (tex.a > 0.01 && burn1.a > 0.01 && !shadow &&
        res < adjusted + 0.8 * (0.5 - std::abs(adjusted - 0.5)) && res > adjusted) {
        if (res < adjusted + 0.5 * (0.5 - std::abs(adjusted - 0.5))) tex = burn1;
        else if (burn2.a > 0.01) tex = burn2;
    }
    if (res <= adjusted) tex.a = 0.0;
    if (shadow) return {0, 0, 0, tex.a * 0.3};
    return tex;
}

static V4 sampleNearest(const QImage &img, double u, double v)
{
    const int x = std::max(0, std::min(img.width() - 1, int(std::floor(u * img.width()))));
    const int y = std::max(0, std::min(img.height() - 1, int(std::floor(v * img.height()))));
    return fromPixel(img.pixel(x, y));
}

static V4 shaderPixel(const QImage &src, ShaderKind kind, int x, int y, double seedTime, double phaseX, double phaseY, double intensity,
                      double dissolve = 0.0, V4 burn1 = {1,1,1,0}, V4 burn2 = {1,1,1,0})
{
    const int w = std::max(1, src.width());
    const int h = std::max(1, src.height());
    const double u = (x + 0.5) / double(w);
    const double v = (y + 0.5) / double(h);
    V2 uv{u, v};
    V4 tex = fromPixel(src.pixel(x, y));
    if (tex.a <= 0.00001 && kind != ShaderKind::Hologram) return {0,0,0,0};

    switch (kind) {
    case ShaderKind::Foil: {
        V2 adjusted{uv.x - 0.5, uv.y - 0.5};
        adjusted.x *= double(w) / double(h);
        const double low = std::min(tex.r, std::min(tex.g, tex.b));
        const double high = std::max(tex.r, std::max(tex.g, tex.b));
        const double delta = std::min(high, std::max(0.5, 1.0 - low));
        const double len90 = len(mul(adjusted, 90.0));
        const double len113 = len(mul(adjusted, 113.1121));
        const double fac = std::max(std::min(2.0 * std::sin((len90 + phaseX * 2.0) + 3.0 * (1.0 + 0.8 * std::cos(len113 - phaseX * 3.121))) - 1.0 - std::max(5.0 - len90, 0.0), 1.0), 0.0);
        V2 rotater{std::cos(phaseY * 0.1221), std::sin(phaseY * 0.3512)};
        const double denom = std::max(0.000001, len(rotater) * len(adjusted));
        const double angle = (rotater.x * adjusted.x + rotater.y * adjusted.y) / denom;
        const double fac2 = std::max(std::min(5.0 * std::cos(phaseY * 0.3 + angle * 3.14 * (2.2 + 0.9 * std::sin(phaseY * 1.65 + 0.2 * phaseY))) - 4.0 - std::max(2.0 - len(mul(adjusted, 20.0)), 0.0), 1.0), 0.0);
        const double fac3 = 0.3 * std::max(std::min(2.0 * std::sin(phaseX * 5.0 + uv.x * 3.0 + 3.0 * (1.0 + 0.5 * std::cos(phaseX * 7.0))) - 1.0, 1.0), -1.0);
        const double fac4 = 0.3 * std::max(std::min(2.0 * std::sin(phaseX * 6.66 + uv.y * 3.8 + 3.0 * (1.0 + 0.5 * std::cos(phaseX * 3.414))) - 1.0, 1.0), -1.0);
        const double maxfac = std::max(std::max(fac, std::max(fac2, std::max(fac3, std::max(fac4, 0.0)))) + 2.2 * (fac + fac2 + fac3 + fac4), 0.0);
        tex.r = tex.r - delta + delta * maxfac * 0.3;
        tex.g = tex.g - delta + delta * maxfac * 0.3;
        tex.b = tex.b + delta * maxfac * 1.9;
        tex.a = std::min(tex.a, 0.3 * tex.a + 0.9 * std::min(0.5, maxfac * 0.1));
        tex.a *= intensity;
        return dissolveMask(tex, uv, w, h, dissolve, seedTime, false, burn1, burn2);
    }
    case ShaderKind::Holo: {
        V4 hsl = HSL(add(mul(tex, 0.5), {0,0,0.5,tex.a * 0.5}));
        const double t = phaseY * 7.221 + seedTime;
        V2 floored{std::floor(uv.x * w) / double(w), std::floor(uv.y * h) / double(h)};
        V2 uvScaled = mul(sub(floored, {0.5, 0.5}), 250.0);
        const double field = shaderField(uvScaled, t);
        const double res = 0.5 + 0.5 * std::cos(phaseX * 2.612 + (field - 0.5) * 3.14);
        const double low = std::min(tex.r, std::min(tex.g, tex.b));
        const double high = std::max(tex.r, std::max(tex.g, tex.b));
        const double delta = 0.2 + 0.3 * (high - low) + 0.1 * high;
        const double gridsize = 0.79;
        const double fac = 0.5 * std::max(std::max(std::max(0.0, 7.0 * std::abs(std::cos(uv.x * gridsize * 20.0)) - 6.0),
                                                   std::max(0.0, 7.0 * std::cos(uv.y * gridsize * 45.0 + uv.x * gridsize * 20.0) - 6.0)),
                                          std::max(0.0, 7.0 * std::cos(uv.y * gridsize * 45.0 - uv.x * gridsize * 20.0) - 6.0));
        hsl.r = hsl.r + res + fac;
        hsl.g = hsl.g * 1.3;
        hsl.b = hsl.b * 0.6 + 0.4;
        V4 rgb = mul(RGB(hsl), {0.9, 0.8, 1.2, tex.a});
        tex = add(mul(tex, 1.0 - delta), mul(rgb, delta));
        if (tex.a < 0.7) tex.a /= 3.0;
        tex.a *= intensity;
        return dissolveMask(tex, uv, w, h, dissolve, seedTime, false, burn1, burn2);
    }
    case ShaderKind::Polychrome: {
        const double low = std::min(tex.r, std::min(tex.g, tex.b));
        const double high = std::max(tex.r, std::max(tex.g, tex.b));
        const double delta = high - low;
        const double saturationFac = 1.0 - std::max(0.0, 0.05 * (1.1 - delta));
        V4 hsl = HSL({tex.r * saturationFac, tex.g * saturationFac, tex.b, tex.a});
        const double t = phaseY * 2.221 + seedTime;
        V2 floored{std::floor(uv.x * w) / double(w), std::floor(uv.y * h) / double(h)};
        V2 uvScaled = mul(sub(floored, {0.5, 0.5}), 50.0);
        const double field = shaderField(uvScaled, t);
        const double res = 0.5 + 0.5 * std::cos(phaseX * 2.612 + (field - 0.5) * 3.14);
        hsl.r = hsl.r + res + phaseY * 0.04;
        hsl.g = std::min(0.6, hsl.g + 0.5);
        V4 rgb = RGB(hsl);
        tex.r = rgb.r; tex.g = rgb.g; tex.b = rgb.b;
        if (tex.a < 0.7) tex.a /= 3.0;
        tex.a *= intensity;
        return dissolveMask(tex, uv, w, h, dissolve, seedTime, false, burn1, burn2);
    }
    case ShaderKind::Negative: {
        V4 sat = HSL(tex);
        sat.b = 1.0 - sat.b;
        sat.r = -sat.r + 0.2;
        tex = add(RGB(sat), mul({79.0/255.0, 99.0/255.0, 103.0/255.0, 0.0}, 0.8));
        if (tex.a < 0.7) tex.a /= 3.0;
        tex.a *= intensity;
        return dissolveMask(tex, uv, w, h, dissolve, seedTime, false, burn1, burn2);
    }
    case ShaderKind::NegativeShine: {
        const double low = std::min(tex.r, std::min(tex.g, tex.b));
        const double high = std::max(tex.r, std::max(tex.g, tex.b));
        const double delta = high - low - 0.1;
        const double fac  = 0.8 + 0.9 * std::sin(11.0 * uv.x + 4.32 * uv.y + phaseX * 12.0 + std::cos(phaseX * 5.3 + uv.y * 4.2 - uv.x * 4.0));
        const double fac2 = 0.5 + 0.5 * std::sin(8.0 * uv.x + 2.32 * uv.y + phaseX * 5.0 - std::cos(phaseX * 2.3 + uv.x * 8.2));
        const double fac3 = 0.5 + 0.5 * std::sin(10.0 * uv.x + 5.32 * uv.y + phaseX * 6.111 + std::sin(phaseX * 5.3 + uv.y * 3.2));
        const double fac4 = 0.5 + 0.5 * std::sin(3.0 * uv.x + 2.32 * uv.y + phaseX * 8.111 + std::sin(phaseX * 1.3 + uv.y * 11.2));
        const double fac5 = std::sin(0.9 * 16.0 * uv.x + 5.32 * uv.y + phaseX * 12.0 + std::cos(phaseX * 5.3 + uv.y * 4.2 - uv.x * 4.0));
        const double maxfac = 0.7 * std::max(std::max(fac, std::max(fac2, std::max(fac3, 0.0))) + (fac + fac2 + fac3 * fac4), 0.0);
        tex.r = tex.r * 0.5 + 0.4;
        tex.g = tex.g * 0.5 + 0.4;
        tex.b = tex.b * 0.5 + 0.8;
        tex.r = tex.r - delta + delta * maxfac * (0.7 + fac5 * 0.27) - 0.1;
        tex.g = tex.g - delta + delta * maxfac * (0.7 - fac5 * 0.27) - 0.1;
        tex.b = tex.b - delta + delta * maxfac * 0.7 - 0.1;
        tex.a = tex.a * (0.5 * std::max(std::min(1.0, std::max(0.0, 0.3 * std::max(low * 0.2, delta) + std::min(std::max(maxfac * 0.1, 0.0), 0.4))), 0.0) + 0.15 * maxfac * (0.1 + delta));
        tex.a *= intensity;
        return dissolveMask(tex, uv, w, h, dissolve, seedTime, false, burn1, burn2);
    }
    case ShaderKind::Booster:
    case ShaderKind::Voucher: {
        const bool booster = (kind == ShaderKind::Booster);
        const double low = std::min(tex.r, std::min(tex.g, tex.b));
        const double high = std::max(tex.r, std::max(tex.g, tex.b));
        const double delta = booster ? std::max(high - low, low * 0.7) : (high - low);
        const double fac  = 0.8 + 0.9 * std::sin(13.0 * uv.x + 5.32 * uv.y + phaseX * 12.0 + std::cos(phaseX * 5.3 + uv.y * 4.2 - uv.x * 4.0));
        const double fac2 = 0.5 + 0.5 * std::sin(10.0 * uv.x + 2.32 * uv.y + phaseX * 5.0 - std::cos(phaseX * 2.3 + uv.x * 8.2));
        const double fac3 = 0.5 + 0.5 * std::sin(12.0 * uv.x + 6.32 * uv.y + phaseX * 6.111 + std::sin(phaseX * 5.3 + uv.y * 3.2));
        const double fac4 = 0.5 + 0.5 * std::sin(4.0 * uv.x + 2.32 * uv.y + phaseX * 8.111 + std::sin(phaseX * 1.3 + uv.y * 13.2));
        const double fac5 = std::sin(0.5 * 16.0 * uv.x + 5.32 * uv.y + phaseX * 12.0 + std::cos(phaseX * 5.3 + uv.y * 4.2 - uv.x * 4.0));
        const double maxfac = 0.6 * std::max(std::max(fac, std::max(fac2, std::max(fac3, 0.0))) + (fac + fac2 + fac3 * fac4), 0.0);
        tex.r = tex.r * 0.5 + 0.4;
        tex.g = tex.g * 0.5 + 0.4;
        tex.b = tex.b * 0.5 + 0.8;
        tex.r = tex.r - delta + delta * maxfac * (0.7 + fac5 * 0.07) - 0.1;
        tex.g = tex.g - delta + delta * maxfac * (0.7 - fac5 * 0.17) - 0.1;
        tex.b = tex.b - delta + delta * maxfac * 0.7 - 0.1;
        tex.a = tex.a * (0.8 * std::max(std::min(1.0, std::max(0.0, 0.3 * std::max(low * 0.2, delta) + std::min(std::max(maxfac * 0.1, 0.0), 0.4))), 0.0) + 0.15 * maxfac * (0.1 + delta));
        tex.a *= intensity;
        return dissolveMask(tex, uv, w, h, dissolve, seedTime, false, burn1, burn2);
    }
    case ShaderKind::Debuff: {
        V4 sat = HSL(add(mul(tex, 0.8), {0.2, 0.0, 0.0, tex.a * 0.2}));
        sat.g = 0.5;
        const double width = 0.1;
        const bool cross = ((uv.x + uv.y > 1.0 - width && uv.x + uv.y < 1.0 + width) ||
                            ((1.0 - uv.x) + uv.y > 1.0 - width && (1.0 - uv.x) + uv.y < 1.0 + width));
        if (cross) {
            sat.r = 1.0;
            sat.g = 0.7;
            sat.b = 0.8 * sat.b;
        } else {
            sat.g *= 0.5;
            sat.b *= 0.7;
        }
        tex = RGB(sat);
        if (!cross) tex.a *= 0.3;
        tex.a *= intensity;
        return dissolveMask(tex, uv, w, h, dissolve, seedTime, false, burn1, burn2);
    }
    case ShaderKind::Played: {
        V4 sat = HSL(tex);
        sat.g = sat.g * 0.5;
        sat.b = sat.b * 0.8;
        tex = RGB(sat);
        tex.a *= 0.5 * intensity;
        return dissolveMask(tex, uv, w, h, dissolve, seedTime, false, burn1, burn2);
    }
    case ShaderKind::Dissolve: {
        if (dissolve > 0.01) {
            if (burn2.a > 0.01) tex = mix(tex, {burn2.r, burn2.g, burn2.b, tex.a}, 0.6 * dissolve);
            else if (burn1.a > 0.01) tex = mix(tex, {burn1.r, burn1.g, burn1.b, tex.a}, 0.6 * dissolve);
        }
        tex.a *= intensity;
        return dissolveMask(tex, uv, w, h, dissolve, seedTime, false, burn1, burn2);
    }
    case ShaderKind::Hologram: {
        double glow = 0.0;
        int actual = 0;
        const double glowDist = 0.0015;
        for (int i = -4; i <= 4; ++i) {
            for (int j = -4; j <= 4; ++j) {
                const double a = sampleNearest(src, u + glowDist * i, v + glowDist * j).a;
                if (a < 0.9) { actual += 1; glow += a; }
            }
        }
        if (actual > 0) glow /= 0.7 * actual;
        double offsetL = -10.0 * (-0.5 + std::sin(phaseY * 0.512 + v * 14.0) + std::sin(-phaseY * 0.8233 + v * 11.532) + std::sin(phaseY * 0.333 + v * 13.3) + std::sin(-phaseY * 0.1112331 + v * 4.044343));
        double offsetR = -10.0 * (-0.5 + std::sin(phaseY * 0.6924 + v * 19.0) + std::sin(-phaseY * 0.9661 + v * 21.532) + std::sin(phaseY * 0.4423 + v * 30.3) + std::sin(-phaseY * 0.13321312 + v * 3.011));
        if (offsetR >= 1.5 || offsetR <= 0.0) offsetR = 0.0;
        if (offsetL >= 1.5 || offsetL <= 0.0) offsetL = 0.0;
        const double shiftedU = u + 0.002 * (-offsetL + offsetR);
        tex = sampleNearest(src, shiftedU, v);
        if (tex.a > 0.999) tex = {0,0,0,0};
        if (tex.a < 0.001) { tex.r = 0.0; tex.g = 1.0; tex.b = 1.0; }
        if (uv.x > 0.95 || uv.x < 0.05 || uv.y > 0.95 || uv.y < 0.05) return {0,0,0,0};
        const double light = 0.4 * (0.3 * std::sin(2.0 * phaseY) + 0.6 + 0.3 * std::sin(phaseY * 3.0) + 0.9);
        V4 addCol = tex.a < 0.001 ? V4{0.0, 1.0, 0.5, 0.6} : V4{0.0, 0.3, 0.2, 0.3};
        V4 finalCol = add(tex, mul(addCol, light * (1.0 + std::abs(offsetL) + std::abs(offsetR)) * glow));
        finalCol.a *= intensity;
        return dissolveMask(finalCol, uv, w, h, dissolve, seedTime, false, burn1, burn2);
    }
    case ShaderKind::GoldSeal: {
        const double low = std::min(tex.r, std::min(tex.g, tex.b));
        Q_UNUSED(low);
        const double high = std::max(tex.r, std::max(tex.g, tex.b));
        const double delta = high * 0.5;
        const double fac = 0.3 + std::sin((u * 450.0 + std::sin(phaseX * 6.0) * 180.0) - 700.0 * phaseX)
                         - std::sin((u * 190.0 + v * 30.0) + 1080.3 * phaseX);
        tex.r = std::max(tex.r, (1.0 - tex.r) * delta * fac + tex.r);
        tex.g = std::max(tex.g, (1.0 - tex.g) * delta * fac + tex.g);
        tex.b = std::max(tex.b, (1.0 - tex.b) * delta * fac + tex.b);
        tex.a *= intensity;
        return tex;
    }
    }
    return tex;
}

static QPixmap processPixmap(const QPixmap &base, ShaderKind kind, double intensity = 1.0, bool overlayOnBase = true,
                             double dissolve = 0.0,
                             const QColor &burn1Color = QColor(255,255,255,0),
                             const QColor &burn2Color = QColor(255,255,255,0))
{
    if (base.isNull()) return QPixmap();
    const double realTime = shaderTime();
    const double phaseX = realTime / 28.0;
    const double phaseY = realTime;
    const quint64 baseKey = base.cacheKey();
    const double seedTime = std::fmod(123.33412 * double((baseKey % 1000000ULL) + 1ULL) / 1.14212, 3000.0);
    const int frame = kindUsesAnimatedFrame(kind, dissolve) ? int(realTime * FX_CACHE_FPS) : 0;
    const QString key = QString::number(baseKey) + QLatin1Char('|') + QString::number(int(kind)) + QLatin1Char('|')
                      + QString::number(frame) + QLatin1Char('|') + QString::number(int(std::round(intensity * 100.0))) + QLatin1Char('|')
                      + QString::number(int(std::round(dissolve * 1000.0))) + QLatin1Char('|') + QString::number(overlayOnBase ? 1 : 0);
    static QHash<QString, QPixmap> cache;
    static QStringList order;
    auto it = cache.constFind(key);
    if (it != cache.constEnd()) return it.value();

    const QImage src = base.toImage().convertToFormat(QImage::Format_ARGB32);
    QImage out(src.size(), QImage::Format_ARGB32);
    out.fill(Qt::transparent);
    const V4 burn1 = fromColor(burn1Color);
    const V4 burn2 = fromColor(burn2Color);

    for (int y = 0; y < src.height(); ++y) {
        QRgb *dstLine = reinterpret_cast<QRgb *>(out.scanLine(y));
        for (int x = 0; x < src.width(); ++x) {
            V4 px = shaderPixel(src, kind, x, y, seedTime, phaseX, phaseY, intensity, dissolve, burn1, burn2);
            if (overlayOnBase) px = over(fromPixel(src.pixel(x, y)), px);
            dstLine[x] = toPixel(px);
        }
    }

    QPixmap result = QPixmap::fromImage(out);
    cache.insert(key, result);
    order.append(key);
    while (order.size() > 180) cache.remove(order.takeFirst());
    return result;
}

static QPixmap makeFlatBase(const QSize &size, int alpha = 170)
{
    QPixmap pix(std::max(1, size.width()), std::max(1, size.height()));
    pix.fill(Qt::transparent);
    QPainter p(&pix);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setBrush(QColor(255,255,255, alpha));
    p.setPen(Qt::NoPen);
    p.drawRoundedRect(QRectF(1, 1, pix.width() - 2, pix.height() - 2), 10, 10);
    return pix;
}

static QColor weightedBackgroundColour(const QColor &c1, const QColor &c2, const QColor &c3,
                                        double c1p, double c2p, double c3p, double contrast)
{
    const double safeContrast = std::max(0.05, contrast);
    const double bias = clamp01(0.3 / safeContrast);
    const double rest = 1.0 - bias;
    const double r = bias * c1.red()   + rest * (c1.red()   * c1p + c2.red()   * c2p + c3.red()   * c3p);
    const double g = bias * c1.green() + rest * (c1.green() * c1p + c2.green() * c2p + c3.green() * c3p);
    const double b = bias * c1.blue()  + rest * (c1.blue()  * c1p + c2.blue()  * c2p + c3.blue()  * c3p);
    return QColor(clamp255(r), clamp255(g), clamp255(b), 255);
}

static QPixmap makeSoulForeground(const QPixmap &enhancersSheet)
{
    constexpr int W = 142, H = 190;
    if (enhancersSheet.isNull()) return QPixmap();
    return enhancersSheet.copy(0, H, W, H);
}

} // namespace

double shaderTime()
{
    static const qint64 startMs = QDateTime::currentMSecsSinceEpoch();
    return (QDateTime::currentMSecsSinceEpoch() - startMs) / 1000.0;
}

QPixmap renderEditionPixmap(const QPixmap &base, Edition edition, double intensity)
{
    if (edition == Edition::None || base.isNull()) return base;

    bool gpuOk = false;
    QPixmap gpu = renderEditionPixmapGpu(base, edition, intensity, &gpuOk);
    if (gpuOk && !gpu.isNull()) return gpu;

    switch (edition) {
    case Edition::Foil:        return processPixmap(base, ShaderKind::Foil, intensity, true);
    case Edition::Holographic: return processPixmap(base, ShaderKind::Holo, intensity, true);
    case Edition::Polychrome:  return processPixmap(base, ShaderKind::Polychrome, intensity, true);
    case Edition::Negative: {
        QPixmap neg = processPixmap(base, ShaderKind::Negative, intensity, false);
        QPixmap shine = processPixmap(base, ShaderKind::NegativeShine, intensity, false);
        QImage bottom = neg.toImage().convertToFormat(QImage::Format_ARGB32);
        QImage top = shine.toImage().convertToFormat(QImage::Format_ARGB32);
        for (int y = 0; y < bottom.height(); ++y) {
            QRgb *line = reinterpret_cast<QRgb *>(bottom.scanLine(y));
            for (int x = 0; x < bottom.width(); ++x) {
                line[x] = toPixel(over(fromPixel(line[x]), fromPixel(top.pixel(x, y))));
            }
        }
        return QPixmap::fromImage(bottom);
    }
    default: return base;
    }
}

QPixmap renderBoosterPixmap(const QPixmap &base, double intensity)
{
    bool ok = false;
    QPixmap gpu = renderShaderPixmapGpu(base, QStringLiteral("booster"), intensity, true, &ok);
    return (ok && !gpu.isNull()) ? gpu : processPixmap(base, ShaderKind::Booster, intensity, true);
}

QPixmap renderVoucherPixmap(const QPixmap &base, double intensity)
{
    bool ok = false;
    QPixmap gpu = renderShaderPixmapGpu(base, QStringLiteral("voucher"), intensity, true, &ok);
    return (ok && !gpu.isNull()) ? gpu : processPixmap(base, ShaderKind::Voucher, intensity, true);
}

QPixmap renderHologramPixmap(const QPixmap &base, double intensity)
{
    bool ok = false;
    QPixmap gpu = renderShaderPixmapGpu(base, QStringLiteral("hologram"), intensity, true, &ok);
    return (ok && !gpu.isNull()) ? gpu : processPixmap(base, ShaderKind::Hologram, intensity, true);
}

QPixmap renderDebuffedPixmap(const QPixmap &base, double intensity)
{
    bool ok = false;
    QPixmap gpu = renderShaderPixmapGpu(base, QStringLiteral("debuff"), intensity, true, &ok);
    return (ok && !gpu.isNull()) ? gpu : processPixmap(base, ShaderKind::Debuff, intensity, true);
}

QPixmap renderPlayedPixmap(const QPixmap &base, double intensity)
{
    bool ok = false;
    QPixmap gpu = renderShaderPixmapGpu(base, QStringLiteral("played"), intensity, true, &ok);
    return (ok && !gpu.isNull()) ? gpu : processPixmap(base, ShaderKind::Played, intensity, true);
}

QPixmap renderDissolvePixmap(const QPixmap &base, double dissolve, const QColor &burn1, const QColor &burn2, double intensity)
{
    return processPixmap(base, ShaderKind::Dissolve, intensity, false, dissolve, burn1, burn2);
}

QPixmap renderGoldSealPixmap(const QPixmap &base, double intensity)
{
    bool ok = false;
    QPixmap gpu = renderShaderPixmapGpu(base, QStringLiteral("goldseal"), intensity, false, &ok);
    return (ok && !gpu.isNull()) ? gpu : processPixmap(base, ShaderKind::GoldSeal, intensity, false);
}

QImage makeBackgroundImage(const QSizeF &logicalSize, const QColor &c1, const QColor &c2, const QColor &c3,
                           double contrast, double spinAmount, double spinTimeScale, double time)
{
    if (logicalSize.width() <= 1 || logicalSize.height() <= 1) return QImage();
    if (time < 0.0) time = shaderTime();
    const double spinTime = time * spinTimeScale;
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
    paintCrtOverlay(p, rect, 0.07);
    p->restore();
}

void paintCrtOverlay(QPainter *p, const QRectF &rect, double opacity)
{
    if (!p) return;
    p->save();
    p->setPen(Qt::NoPen);
    for (int y = int(rect.top()); y < rect.bottom(); y += 4) {
        p->fillRect(QRectF(rect.left(), y, rect.width(), 1.0), QColor(0, 0, 0, int(70 * opacity)));
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
    if (!p || edition == Edition::None || r.width() <= 0 || r.height() <= 0) return;
    QPixmap base = makeFlatBase(QSize(int(std::round(r.width())), int(std::round(r.height()))), edition == Edition::Negative ? 230 : 170);
    QPixmap fx = renderEditionPixmap(base, edition, intensity);
    p->save();
    p->setRenderHint(QPainter::SmoothPixmapTransform, true);
    roundedClip(p, r, 10);
    p->drawPixmap(r, fx, QRectF(0, 0, fx.width(), fx.height()));
    p->restore();
}

void paintNegativeShine(QPainter *p, const QRectF &r, double intensity)
{
    if (!p) return;
    QPixmap base = makeFlatBase(QSize(int(std::round(r.width())), int(std::round(r.height()))), 190);
    bool ok = false;
    QPixmap fx = renderShaderPixmapGpu(base, QStringLiteral("negative_shine"), intensity, false, &ok);
    if (!ok || fx.isNull()) fx = processPixmap(base, ShaderKind::NegativeShine, intensity, false);
    p->save();
    roundedClip(p, r, 10);
    p->drawPixmap(r, fx, QRectF(0, 0, fx.width(), fx.height()));
    p->restore();
}

void paintBoosterShader(QPainter *p, const QRectF &r, double intensity)
{
    if (!p) return;
    QPixmap base = makeFlatBase(QSize(int(std::round(r.width())), int(std::round(r.height()))), 155);
    bool ok = false;
    QPixmap fx = renderShaderPixmapGpu(base, QStringLiteral("booster"), intensity, false, &ok);
    if (!ok || fx.isNull()) fx = processPixmap(base, ShaderKind::Booster, intensity, false);
    p->save();
    roundedClip(p, r, 12);
    p->drawPixmap(r, fx, QRectF(0, 0, fx.width(), fx.height()));
    p->restore();
}

void paintVoucherShader(QPainter *p, const QRectF &r, double intensity)
{
    if (!p) return;
    QPixmap base = makeFlatBase(QSize(int(std::round(r.width())), int(std::round(r.height()))), 155);
    bool ok = false;
    QPixmap fx = renderShaderPixmapGpu(base, QStringLiteral("voucher"), intensity, false, &ok);
    if (!ok || fx.isNull()) fx = processPixmap(base, ShaderKind::Voucher, intensity, false);
    p->save();
    roundedClip(p, r, 10);
    p->drawPixmap(r, fx, QRectF(0, 0, fx.width(), fx.height()));
    p->restore();
}

void paintHologramShader(QPainter *p, const QRectF &r, double intensity)
{
    if (!p) return;
    QPixmap base = makeFlatBase(QSize(int(std::round(r.width())), int(std::round(r.height()))), 190);
    bool ok = false;
    QPixmap fx = renderShaderPixmapGpu(base, QStringLiteral("hologram"), intensity, false, &ok);
    if (!ok || fx.isNull()) fx = processPixmap(base, ShaderKind::Hologram, intensity, false);
    p->save();
    roundedClip(p, r, 10);
    p->drawPixmap(r, fx, QRectF(0, 0, fx.width(), fx.height()));
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
    QPixmap base = makeFlatBase(QSize(int(std::round(r.width())), int(std::round(r.height()))), 210);
    bool ok = false;
    QPixmap fx = renderShaderPixmapGpu(base, QStringLiteral("debuff"), intensity, false, &ok);
    if (!ok || fx.isNull()) fx = processPixmap(base, ShaderKind::Debuff, intensity, false);
    p->save();
    roundedClip(p, r, 10);
    p->drawPixmap(r, fx, QRectF(0, 0, fx.width(), fx.height()));
    p->restore();
}

void paintFlame(QPainter *p, const QRectF &r, double intensity)
{
    if (!p || intensity <= 0.01) return;
    const double t = shaderTime();
    p->save();
    p->setCompositionMode(QPainter::CompositionMode_Screen);
    p->setRenderHint(QPainter::Antialiasing, true);
    for (int i = 0; i < 18; ++i) {
        const double u = (i + 0.5) / 18.0 - 0.5;
        const double phase = t * 4.0 + i * 1.781;
        const double h = r.height() * (0.25 + 0.20 * intensity + 0.12 * std::sin(phase));
        const double x = r.center().x() + u * r.width() * 0.9;
        QPainterPath flame;
        flame.moveTo(x, r.bottom());
        flame.cubicTo(x - r.width() * 0.08, r.bottom() - h * 0.45,
                       x + std::sin(phase) * 12.0, r.bottom() - h * 0.92,
                       x + std::sin(phase * 0.7) * 7.0, r.bottom() - h * 1.15);
        flame.cubicTo(x + r.width() * 0.09, r.bottom() - h * 0.62,
                       x + r.width() * 0.05, r.bottom() - h * 0.18,
                       x, r.bottom());
        QLinearGradient g(QPointF(x, r.bottom()), QPointF(x, r.bottom() - h));
        g.setColorAt(0.0, QColor(255, 55, 25, int(125 * intensity)));
        g.setColorAt(0.45, QColor(255, 140, 28, int(110 * intensity)));
        g.setColorAt(1.0, QColor(255, 255, 155, int(65 * intensity)));
        p->fillPath(flame, g);
    }
    p->restore();
}

void paintGoldSealGlow(QPainter *p, const QRectF &r, double intensity)
{
    if (!p) return;
    QPixmap base = makeFlatBase(QSize(int(std::round(r.width())), int(std::round(r.height()))), 170);
    bool ok = false;
    QPixmap fx = renderShaderPixmapGpu(base, QStringLiteral("gold_seal"), intensity, false, &ok);
    if (!ok || fx.isNull()) fx = processPixmap(base, ShaderKind::GoldSeal, intensity, false);
    p->save();
    p->setCompositionMode(QPainter::CompositionMode_Screen);
    p->drawPixmap(r, fx, QRectF(0, 0, fx.width(), fx.height()));
    p->restore();
}

void paintSoulCrystal(QPainter *p, const QRectF &rect, const QPixmap &enhancersSheet)
{
    if (!p) return;
    static QPixmap cached;
    static quint64 key = 0;
    const quint64 currentKey = enhancersSheet.cacheKey();
    if (cached.isNull() || key != currentKey) {
        cached = makeSoulForeground(enhancersSheet);
        key = currentKey;
    }
    const double t = shaderTime();
    const double frac = t - std::floor(t);
    const double scaleMod = 0.05 + 0.05 * std::sin(1.8 * t)
                          + 0.07 * std::sin(frac * PI * 14.0) * std::pow(1.0 - frac, 3.0);
    const double rotateMod = 0.10 * std::sin(1.219 * t)
                           + 0.07 * std::sin(t * PI * 5.0) * std::pow(1.0 - frac, 2.0);
    p->save();
    p->setRenderHint(QPainter::SmoothPixmapTransform, true);
    p->setRenderHint(QPainter::Antialiasing, true);
    const QPointF c = rect.center();
    p->translate(c);
    p->rotate(rotateMod * 180.0 / PI);
    p->scale(1.0 + scaleMod, 1.0 + scaleMod);
    p->translate(-c);
    if (!cached.isNull()) {
        // 原版 The Soul 画两次 shared_soul dissolve；这里不再额外叠径向白雾，
        // 避免中心和边缘出现原版没有的白色模糊光圈。
        p->setOpacity(0.6);
        p->drawPixmap(rect, cached, QRectF(0, 0, cached.width(), cached.height()));
        p->setOpacity(1.0);
        p->drawPixmap(rect, cached, QRectF(0, 0, cached.width(), cached.height()));
    }
    p->restore();
}

QPixmap makeBooster3DPixmap(const QPixmap &base)
{
    if (base.isNull()) return QPixmap();
    QPixmap shaded = renderBoosterPixmap(base, 0.95);
    QPixmap pix(base.width() + 10, base.height() + 14);
    pix.fill(Qt::transparent);
    QPainter p(&pix);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::SmoothPixmapTransform, true);
    QRectF r(4, 2, base.width(), base.height());
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(0,0,0,130));
    p.drawRoundedRect(r.translated(5, 9), 11, 11);
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
    p.drawPixmap(target, shaded, shaded.rect());
    p.restore();
    return pix;
}

} // namespace BalatroShaders
