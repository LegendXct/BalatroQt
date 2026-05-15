#include "gpueffectsrenderer.h"

#include <QCoreApplication>
#include <QGuiApplication>
#include <QHash>
#include <QImage>
#include <QOffscreenSurface>
#include <QOpenGLContext>
#include <QOpenGLFramebufferObject>
#include <QOpenGLFunctions>
#include <QOpenGLShaderProgram>
#include <QSurfaceFormat>
#include <QThread>
#include <QVector>
#include <QVector2D>
#include <QtMath>
#include <memory>

namespace BalatroShaders {
namespace {

enum class GpuKind {
    Foil = 0,
    Holo = 1,
    Polychrome = 2,
    Negative = 3,
    Hologram = 4,
    Voucher = 5,
    Debuff = 6,
    GoldSeal = 7,
    Booster = 8,
    Played = 9,
    NegativeShine = 10
};

static QString kindName(GpuKind kind)
{
    switch (kind) {
    case GpuKind::Foil: return QStringLiteral("foil");
    case GpuKind::Holo: return QStringLiteral("holo");
    case GpuKind::Polychrome: return QStringLiteral("polychrome");
    case GpuKind::Negative: return QStringLiteral("negative");
    case GpuKind::Hologram: return QStringLiteral("hologram");
    case GpuKind::Voucher: return QStringLiteral("voucher");
    case GpuKind::Debuff: return QStringLiteral("debuff");
    case GpuKind::GoldSeal: return QStringLiteral("goldseal");
    case GpuKind::Booster: return QStringLiteral("booster");
    case GpuKind::Played: return QStringLiteral("played");
    case GpuKind::NegativeShine: return QStringLiteral("negative_shine");
    }
    return QStringLiteral("unknown");
}

static bool nameToKind(const QString &name, GpuKind &kind)
{
    if (name == QLatin1String("foil")) { kind = GpuKind::Foil; return true; }
    if (name == QLatin1String("holo")) { kind = GpuKind::Holo; return true; }
    if (name == QLatin1String("polychrome")) { kind = GpuKind::Polychrome; return true; }
    if (name == QLatin1String("negative")) { kind = GpuKind::Negative; return true; }
    if (name == QLatin1String("negative_shine")) { kind = GpuKind::NegativeShine; return true; }
    if (name == QLatin1String("hologram")) { kind = GpuKind::Hologram; return true; }
    if (name == QLatin1String("voucher")) { kind = GpuKind::Voucher; return true; }
    if (name == QLatin1String("debuff")) { kind = GpuKind::Debuff; return true; }
    if (name == QLatin1String("goldseal")) { kind = GpuKind::GoldSeal; return true; }
    if (name == QLatin1String("gold_seal")) { kind = GpuKind::GoldSeal; return true; }
    if (name == QLatin1String("booster")) { kind = GpuKind::Booster; return true; }
    if (name == QLatin1String("played")) { kind = GpuKind::Played; return true; }
    return false;
}

static QString vertexSource()
{
    return QStringLiteral(R"GLSL(
#ifdef GL_ES
precision highp float;
#endif
attribute vec2 a_pos;
attribute vec2 a_uv;
varying vec2 v_uv;
void main() {
    v_uv = a_uv;
    gl_Position = vec4(a_pos, 0.0, 1.0);
}
)GLSL");
}

static QString fragmentSource()
{
    return QStringLiteral(R"GLSL(
#ifdef GL_ES
precision highp float;
#endif

uniform sampler2D u_texture;
uniform int u_kind;
uniform vec2 u_size;
uniform vec2 u_phase;
uniform float u_seed_time;
uniform float u_intensity;
uniform bool u_overlay;
varying vec2 v_uv;

float saturate(float x) { return clamp(x, 0.0, 1.0); }
vec4 saturate4(vec4 x) { return clamp(x, 0.0, 1.0); }

vec4 over_straight(vec4 bottom, vec4 top) {
    float a = top.a + bottom.a * (1.0 - top.a);
    if (a <= 0.00001) return vec4(0.0);
    return vec4((top.rgb * top.a + bottom.rgb * bottom.a * (1.0 - top.a)) / a, a);
}

float hue(float s, float t, float h) {
    float hs = mod(h, 1.0) * 6.0;
    if (hs < 1.0) return (t - s) * hs + s;
    if (hs < 3.0) return t;
    if (hs < 4.0) return (t - s) * (4.0 - hs) + s;
    return s;
}

vec4 RGB(vec4 c) {
    if (c.y < 0.0001) return vec4(vec3(c.z), c.a);
    float t = (c.z < 0.5) ? c.y * c.z + c.z : -c.y * c.z + (c.y + c.z);
    float s = 2.0 * c.z - t;
    return vec4(hue(s, t, c.x + 1.0 / 3.0), hue(s, t, c.x), hue(s, t, c.x - 1.0 / 3.0), c.w);
}

vec4 HSL(vec4 c) {
    float low = min(c.r, min(c.g, c.b));
    float high = max(c.r, max(c.g, c.b));
    float delta = high - low;
    float sum = high + low;
    vec4 hsl = vec4(0.0, 0.0, 0.5 * sum, c.a);
    if (abs(delta) <= 0.0000001) return hsl;
    hsl.y = (hsl.z < 0.5) ? delta / max(sum, 0.000001) : delta / max(2.0 - sum, 0.000001);
    if (high == c.r) hsl.x = (c.g - c.b) / delta;
    else if (high == c.g) hsl.x = (c.b - c.r) / delta + 2.0;
    else hsl.x = (c.r - c.g) / delta + 4.0;
    hsl.x = mod(hsl.x / 6.0, 1.0);
    return hsl;
}

float shader_field(vec2 uv_scaled_centered, float t) {
    vec2 field_part1 = uv_scaled_centered + 50.0 * vec2(sin(-t / 143.6340), cos(-t / 99.4324));
    vec2 field_part2 = uv_scaled_centered + 50.0 * vec2(cos( t / 53.1532),  cos( t / 61.4532));
    vec2 field_part3 = uv_scaled_centered + 50.0 * vec2(sin(-t / 87.53218), sin(-t / 49.0000));
    return (1.0 + (cos(length(field_part1) / 19.483) + sin(length(field_part2) / 33.155) * cos(field_part2.y / 15.73) + cos(length(field_part3) / 27.193) * sin(field_part3.x / 21.92))) / 2.0;
}

vec4 effect_foil(vec4 tex, vec2 uv) {
    vec2 adjusted_uv = uv - vec2(0.5);
    adjusted_uv.x = adjusted_uv.x * u_size.x / max(u_size.y, 0.0001);
    float low = min(tex.r, min(tex.g, tex.b));
    float high = max(tex.r, max(tex.g, tex.b));
    float delta = min(high, max(0.5, 1.0 - low));
    float fac = max(min(2.0 * sin((length(90.0 * adjusted_uv) + u_phase.x * 2.0) + 3.0 * (1.0 + 0.8 * cos(length(113.1121 * adjusted_uv) - u_phase.x * 3.121))) - 1.0 - max(5.0 - length(90.0 * adjusted_uv), 0.0), 1.0), 0.0);
    vec2 rotater = vec2(cos(u_phase.x * 0.1221), sin(u_phase.x * 0.3512));
    float denom = max(length(rotater) * length(adjusted_uv), 0.000001);
    float angle = dot(rotater, adjusted_uv) / denom;
    float fac2 = max(min(5.0 * cos(u_phase.y * 0.3 + angle * 3.14 * (2.2 + 0.9 * sin(u_phase.x * 1.65 + 0.2 * u_phase.y))) - 4.0 - max(2.0 - length(20.0 * adjusted_uv), 0.0), 1.0), 0.0);
    float fac3 = 0.3 * max(min(2.0 * sin(u_phase.x * 5.0 + uv.x * 3.0 + 3.0 * (1.0 + 0.5 * cos(u_phase.x * 7.0))) - 1.0, 1.0), -1.0);
    float fac4 = 0.3 * max(min(2.0 * sin(u_phase.x * 6.66 + uv.y * 3.8 + 3.0 * (1.0 + 0.5 * cos(u_phase.x * 3.414))) - 1.0, 1.0), -1.0);
    float maxfac = max(max(fac, max(fac2, max(fac3, max(fac4, 0.0)))) + 2.2 * (fac + fac2 + fac3 + fac4), 0.0);
    tex.r = tex.r - delta + delta * maxfac * 0.3;
    tex.g = tex.g - delta + delta * maxfac * 0.3;
    tex.b = tex.b + delta * maxfac * 1.9;
    tex.a = min(tex.a, 0.3 * tex.a + 0.9 * min(0.5, maxfac * 0.1));
    return tex;
}

vec4 effect_holo(vec4 tex, vec2 uv) {
    vec4 hsl = HSL(0.5 * tex + 0.5 * vec4(0.0, 0.0, 1.0, tex.a));
    float t = u_phase.y * 7.221 + u_seed_time;
    vec2 floored_uv = floor(uv * u_size) / u_size;
    vec2 uv_scaled_centered = (floored_uv - 0.5) * 250.0;
    float field = shader_field(uv_scaled_centered, t);
    float res = 0.5 + 0.5 * cos(u_phase.x * 2.612 + (field - 0.5) * 3.14);
    float low = min(tex.r, min(tex.g, tex.b));
    float high = max(tex.r, max(tex.g, tex.b));
    float delta = 0.2 + 0.3 * (high - low) + 0.1 * high;
    float gridsize = 0.79;
    float fac = 0.5 * max(max(max(0.0, 7.0 * abs(cos(uv.x * gridsize * 20.0)) - 6.0), max(0.0, 7.0 * cos(uv.y * gridsize * 45.0 + uv.x * gridsize * 20.0) - 6.0)), max(0.0, 7.0 * cos(uv.y * gridsize * 45.0 - uv.x * gridsize * 20.0) - 6.0));
    hsl.x = hsl.x + res + fac;
    hsl.y = hsl.y * 1.3;
    hsl.z = hsl.z * 0.6 + 0.4;
    vec4 rgb = RGB(hsl) * vec4(0.9, 0.8, 1.2, tex.a);
    tex = tex * (1.0 - delta) + rgb * delta;
    if (tex.a < 0.7) tex.a = tex.a / 3.0;
    return tex;
}

vec4 effect_polychrome(vec4 tex, vec2 uv) {
    float low = min(tex.r, min(tex.g, tex.b));
    float high = max(tex.r, max(tex.g, tex.b));
    float delta = high - low;
    float saturation_fac = 1.0 - max(0.0, 0.05 * (1.1 - delta));
    vec4 hsl = HSL(vec4(tex.r * saturation_fac, tex.g * saturation_fac, tex.b, tex.a));
    float t = u_phase.y * 2.221 + u_seed_time;
    vec2 floored_uv = floor(uv * u_size) / u_size;
    vec2 uv_scaled_centered = (floored_uv - 0.5) * 50.0;
    float field = shader_field(uv_scaled_centered, t);
    float res = 0.5 + 0.5 * cos(u_phase.x * 2.612 + (field - 0.5) * 3.14);
    hsl.x = hsl.x + res + u_phase.y * 0.04;
    hsl.y = min(0.6, hsl.y + 0.5);
    tex.rgb = RGB(hsl).rgb;
    if (tex.a < 0.7) tex.a = tex.a / 3.0;
    return tex;
}

vec4 effect_negative(vec4 tex) {
    vec4 sat = HSL(tex);
    sat.z = 1.0 - sat.z;
    sat.x = -sat.x + 0.2;
    tex = RGB(sat) + 0.8 * vec4(79.0 / 255.0, 99.0 / 255.0, 103.0 / 255.0, 0.0);
    if (tex.a < 0.7) tex.a = tex.a / 3.0;
    return tex;
}

vec4 effect_negative_shine(vec4 tex, vec2 uv) {
    float low = min(tex.r, min(tex.g, tex.b));
    float high = max(tex.r, max(tex.g, tex.b));
    float delta = high - low - 0.1;
    float fac  = 0.8 + 0.9 * sin(11.0 * uv.x + 4.32 * uv.y + u_phase.x * 12.0 + cos(u_phase.x * 5.3 + uv.y * 4.2 - uv.x * 4.0));
    float fac2 = 0.5 + 0.5 * sin(8.0 * uv.x + 2.32 * uv.y + u_phase.x * 5.0 - cos(u_phase.x * 2.3 + uv.x * 8.2));
    float fac3 = 0.5 + 0.5 * sin(10.0 * uv.x + 5.32 * uv.y + u_phase.x * 6.111 + sin(u_phase.x * 5.3 + uv.y * 3.2));
    float fac4 = 0.5 + 0.5 * sin(3.0 * uv.x + 2.32 * uv.y + u_phase.x * 8.111 + sin(u_phase.x * 1.3 + uv.y * 11.2));
    float fac5 = sin(0.9 * 16.0 * uv.x + 5.32 * uv.y + u_phase.x * 12.0 + cos(u_phase.x * 5.3 + uv.y * 4.2 - uv.x * 4.0));
    float maxfac = 0.7 * max(max(fac, max(fac2, max(fac3, 0.0))) + (fac + fac2 + fac3 * fac4), 0.0);
    tex.rgb = tex.rgb * 0.5 + vec3(0.4, 0.4, 0.8);
    tex.r = tex.r - delta + delta * maxfac * (0.7 + fac5 * 0.27) - 0.1;
    tex.g = tex.g - delta + delta * maxfac * (0.7 - fac5 * 0.27) - 0.1;
    tex.b = tex.b - delta + delta * maxfac * 0.7 - 0.1;
    tex.a = tex.a * (0.5 * max(min(1.0, max(0.0, 0.3 * max(low * 0.2, delta) + min(max(maxfac * 0.1, 0.0), 0.4))), 0.0) + 0.15 * maxfac * (0.1 + delta));
    return tex;
}

vec4 effect_voucher_like(vec4 tex, vec2 uv, bool booster) {
    float low = min(tex.r, min(tex.g, tex.b));
    float high = max(tex.r, max(tex.g, tex.b));
    float delta = booster ? max(high - low, low * 0.7) : (high - low);
    float fac  = 0.8 + 0.9 * sin(13.0 * uv.x + 5.32 * uv.y + u_phase.x * 12.0 + cos(u_phase.x * 5.3 + uv.y * 4.2 - uv.x * 4.0));
    float fac2 = 0.5 + 0.5 * sin(10.0 * uv.x + 2.32 * uv.y + u_phase.x * 5.0 - cos(u_phase.x * 2.3 + uv.x * 8.2));
    float fac3 = 0.5 + 0.5 * sin(12.0 * uv.x + 6.32 * uv.y + u_phase.x * 6.111 + sin(u_phase.x * 5.3 + uv.y * 3.2));
    float fac4 = 0.5 + 0.5 * sin(4.0 * uv.x + 2.32 * uv.y + u_phase.x * 8.111 + sin(u_phase.x * 1.3 + uv.y * 13.2));
    float fac5 = sin(0.5 * 16.0 * uv.x + 5.32 * uv.y + u_phase.x * 12.0 + cos(u_phase.x * 5.3 + uv.y * 4.2 - uv.x * 4.0));
    float maxfac = 0.6 * max(max(fac, max(fac2, max(fac3, 0.0))) + (fac + fac2 + fac3 * fac4), 0.0);
    tex.rgb = tex.rgb * 0.5 + vec3(0.4, 0.4, 0.8);
    tex.r = tex.r - delta + delta * maxfac * (0.7 + fac5 * 0.07) - 0.1;
    tex.g = tex.g - delta + delta * maxfac * (0.7 - fac5 * 0.17) - 0.1;
    tex.b = tex.b - delta + delta * maxfac * 0.7 - 0.1;
    tex.a = tex.a * (0.8 * max(min(1.0, max(0.0, 0.3 * max(low * 0.2, delta) + min(max(maxfac * 0.1, 0.0), 0.4))), 0.0) + 0.15 * maxfac * (0.1 + delta));
    return tex;
}

vec4 effect_debuff(vec4 tex, vec2 uv) {
    float origA = tex.a;
    vec4 sat = HSL(tex * 0.8 + 0.2 * vec4(1.0, 0.0, 0.0, tex.a));
    sat.y = 0.5;
    float width = 0.1;
    bool cross = false;
    if ((uv.x + uv.y > 1.0 - width && uv.x + uv.y < 1.0 + width) ||
        ((1.0 - uv.x) + uv.y > 1.0 - width && (1.0 - uv.x) + uv.y < 1.0 + width)) {
        cross = true;
    }
    vec4 outc;
    if (cross) {
        sat.x = 1.0;
        sat.y = 0.7;
        sat.z = 0.8 * sat.z;
        outc = mix(tex, RGB(sat), 0.78);
    } else {
        sat.y *= 0.5;
        sat.z *= 0.7;
        outc = mix(tex, RGB(sat), 0.62);
    }
    outc.a = origA;
    return outc;
}

vec4 effect_hologram(vec4 tex, vec2 uv) {
    float glow = 0.0;
    float count = 0.0;
    float glow_dist = 0.0015;
    for (int i = -4; i <= 4; ++i) {
        for (int j = -4; j <= 4; ++j) {
            float a = texture2D(u_texture, v_uv + glow_dist * vec2(float(i), float(j))).a;
            if (a < 0.9) { count += 1.0; glow += a; }
        }
    }
    glow = (count > 0.0) ? glow / (0.7 * count) : 0.0;
    float timefac = u_phase.y;
    float offset_l = -10.0 * (-0.5 + sin(timefac * 0.512 + v_uv.y * 14.0) + sin(-timefac * 0.8233 + v_uv.y * 11.532) + sin(timefac * 0.333 + v_uv.y * 13.3) + sin(-timefac * 0.1112331 + v_uv.y * 4.044343));
    float offset_r = -10.0 * (-0.5 + sin(timefac * 0.6924 + v_uv.y * 19.0) + sin(-timefac * 0.9661 + v_uv.y * 21.532) + sin(timefac * 0.4423 + v_uv.y * 30.3) + sin(-timefac * 0.13321312 + v_uv.y * 3.011));
    if (offset_r >= 1.5 || offset_r <= 0.0) offset_r = 0.0;
    if (offset_l >= 1.5 || offset_l <= 0.0) offset_l = 0.0;
    vec2 tc = v_uv;
    tc.x = tc.x + 0.002 * (-offset_l + offset_r);
    tex = texture2D(u_texture, tc);
    if (tex.a > 0.999) tex = vec4(0.0);
    if (tex.a < 0.001) tex.rgb = vec3(0.0, 1.0, 1.0);
    if (uv.x > 0.95 || uv.x < 0.05 || uv.y > 0.95 || uv.y < 0.05) return vec4(0.0);
    float light_strength = 0.4 * (0.3 * sin(2.0 * u_phase.y) + 0.6 + 0.3 * sin(u_phase.x * 3.0) + 0.9);
    if (tex.a < 0.001) return tex + vec4(0.0, 1.0, 0.5, 0.6) * light_strength * (1.0 + abs(offset_l) + abs(offset_r)) * glow;
    return tex + vec4(0.0, 0.3, 0.2, 0.3) * light_strength * (1.0 + abs(offset_l) + abs(offset_r)) * glow;
}

vec4 effect_played(vec4 tex, vec2 uv) {
    float high = max(tex.r, max(tex.g, tex.b));
    float delta = high * 0.5;
    float fac = 0.3 + sin((uv.x * 450.0 + sin(u_phase.x * 6.0) * 180.0) - 700.0 * u_phase.x) - sin((uv.x * 190.0 + uv.y * 30.0) + 1080.3 * u_phase.x);
    tex.r = max(tex.r, (1.0 - tex.r) * delta * fac + tex.r);
    tex.g = max(tex.g, (1.0 - tex.g) * delta * fac + tex.g);
    tex.b = max(tex.b, (1.0 - tex.b) * delta * fac + tex.b);
    return tex;
}

void main() {
    vec2 uv = v_uv;
    vec4 base = texture2D(u_texture, uv);
    if (base.a <= 0.00001 && u_kind != 4) { gl_FragColor = vec4(0.0); return; }
    vec4 fx = base;

    if (u_kind == 0) fx = effect_foil(base, uv);
    else if (u_kind == 1) fx = effect_holo(base, uv);
    else if (u_kind == 2) fx = effect_polychrome(base, uv);
    else if (u_kind == 3) {
        vec4 neg = effect_negative(base);
        vec4 shine = effect_negative_shine(base, uv);
        fx = over_straight(neg, shine);
        gl_FragColor = saturate4(fx * vec4(1.0, 1.0, 1.0, u_intensity));
        return;
    }
    else if (u_kind == 4) fx = effect_hologram(base, uv);
    else if (u_kind == 5) fx = effect_voucher_like(base, uv, false);
    else if (u_kind == 6) fx = effect_debuff(base, uv);
    else if (u_kind == 7) fx = effect_voucher_like(base, uv, false);
    else if (u_kind == 8) fx = effect_voucher_like(base, uv, true);
    else if (u_kind == 9) fx = effect_played(base, uv);
    else if (u_kind == 10) fx = effect_negative_shine(base, uv);

    fx.a *= u_intensity;
    vec4 out_col = u_overlay ? over_straight(base, fx) : fx;
    gl_FragColor = saturate4(out_col);
}
)GLSL");
}

class Renderer
{
public:
    bool ensure();
    QPixmap render(const QPixmap &base, GpuKind kind, double intensity, bool overlayOnBase);
    void clearCache() { m_cache.clear(); m_order.clear(); }

private:
    bool m_failed = false;
    std::unique_ptr<QOffscreenSurface> m_surface;
    std::unique_ptr<QOpenGLContext> m_context;
    std::unique_ptr<QOpenGLShaderProgram> m_program;
    QHash<QString, QPixmap> m_cache;
    QStringList m_order;
};

bool Renderer::ensure()
{
    if (m_failed) return false;
    if (m_context && m_program) return true;
    if (!QGuiApplication::instance()) { m_failed = true; return false; }
    if (QThread::currentThread() != QGuiApplication::instance()->thread()) { m_failed = true; return false; }

    QSurfaceFormat fmt;
    fmt.setRenderableType(QSurfaceFormat::OpenGL);
    fmt.setVersion(2, 0);
    fmt.setProfile(QSurfaceFormat::NoProfile);
    fmt.setDepthBufferSize(0);
    fmt.setStencilBufferSize(0);
    fmt.setAlphaBufferSize(8);

    m_surface.reset(new QOffscreenSurface());
    m_surface->setFormat(fmt);
    m_surface->create();
    if (!m_surface->isValid()) { m_failed = true; return false; }

    m_context.reset(new QOpenGLContext());
    m_context->setFormat(fmt);
    if (!m_context->create()) { m_failed = true; return false; }
    if (!m_context->makeCurrent(m_surface.get())) { m_failed = true; return false; }

    m_program.reset(new QOpenGLShaderProgram());
    if (!m_program->addShaderFromSourceCode(QOpenGLShader::Vertex, vertexSource()) ||
        !m_program->addShaderFromSourceCode(QOpenGLShader::Fragment, fragmentSource())) {
        qWarning("GpuEffectsRenderer: shader compile failed: %s", qPrintable(m_program->log()));
        m_failed = true;
        return false;
    }
    m_program->bindAttributeLocation("a_pos", 0);
    m_program->bindAttributeLocation("a_uv", 1);
    if (!m_program->link()) {
        qWarning("GpuEffectsRenderer: shader link failed: %s", qPrintable(m_program->log()));
        m_failed = true;
        return false;
    }
    m_context->doneCurrent();
    return true;
}

QPixmap Renderer::render(const QPixmap &base, GpuKind kind, double intensity, bool overlayOnBase)
{
    if (base.isNull()) return QPixmap();
    if (!ensure()) return QPixmap();

    // 版本效果按原版公式取 REAL/28，但这里不再让每张牌持续 update。
    // 使用稳定相位后，商店和小丑槽一致，清晰度不变，也不会因为小丑多而掉帧。
    const quint64 baseKey = base.cacheKey();
    const float seedTime = std::fmod(123.33412f * float((baseKey % 1000000ULL) + 1ULL) / 1.14212f, 3000.0f);
    const float phaseX = std::fmod(seedTime / 28.0f, 3000.0f);
    const float phaseY = 0.0f;

    const QString key = QString::number(baseKey) + QLatin1Char('|')
                      + kindName(kind) + QLatin1Char('|')
                      + QString::number(int(std::round(intensity * 100.0))) + QLatin1Char('|')
                      + QString::number(overlayOnBase ? 1 : 0) + QLatin1Char('|')
                      + QString::number(base.width()) + QLatin1Char('x') + QString::number(base.height());
    auto it = m_cache.constFind(key);
    if (it != m_cache.constEnd()) return it.value();

    if (!m_context->makeCurrent(m_surface.get())) return QPixmap();
    QOpenGLFunctions *gl = m_context->functions();
    if (!gl) { m_context->doneCurrent(); return QPixmap(); }

    const QSize size(base.width(), base.height());
    QOpenGLFramebufferObject fbo(size);
    if (!fbo.isValid()) { m_context->doneCurrent(); return QPixmap(); }

    QImage src = base.toImage().convertToFormat(QImage::Format_RGBA8888);

    GLuint tex = 0;
    gl->glGenTextures(1, &tex);
    gl->glBindTexture(GL_TEXTURE_2D, tex);
    gl->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    gl->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    gl->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    gl->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    gl->glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    gl->glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, src.width(), src.height(), 0, GL_RGBA, GL_UNSIGNED_BYTE, src.constBits());

    fbo.bind();
    gl->glViewport(0, 0, size.width(), size.height());
    gl->glDisable(GL_DEPTH_TEST);
    gl->glDisable(GL_CULL_FACE);
    gl->glDisable(GL_BLEND);
    gl->glClearColor(0.f, 0.f, 0.f, 0.f);
    gl->glClear(GL_COLOR_BUFFER_BIT);

    m_program->bind();
    m_program->setUniformValue("u_texture", 0);
    m_program->setUniformValue("u_kind", int(kind));
    m_program->setUniformValue("u_size", QVector2D(float(size.width()), float(size.height())));
    m_program->setUniformValue("u_phase", QVector2D(phaseX, phaseY));
    m_program->setUniformValue("u_seed_time", seedTime);
    m_program->setUniformValue("u_intensity", float(intensity));
    m_program->setUniformValue("u_overlay", overlayOnBase);

    const GLfloat verts[] = {
        -1.f, -1.f,   0.f, 1.f,
         1.f, -1.f,   1.f, 1.f,
        -1.f,  1.f,   0.f, 0.f,
         1.f,  1.f,   1.f, 0.f
    };
    m_program->enableAttributeArray(0);
    m_program->enableAttributeArray(1);
    m_program->setAttributeArray(0, GL_FLOAT, verts, 2, 4 * sizeof(GLfloat));
    m_program->setAttributeArray(1, GL_FLOAT, verts + 2, 2, 4 * sizeof(GLfloat));
    gl->glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    m_program->disableAttributeArray(0);
    m_program->disableAttributeArray(1);
    m_program->release();

    gl->glBindTexture(GL_TEXTURE_2D, 0);
    gl->glDeleteTextures(1, &tex);
    fbo.release();

    QImage out = fbo.toImage().convertToFormat(QImage::Format_ARGB32);
    QPixmap result = QPixmap::fromImage(out);
    m_cache.insert(key, result);
    m_order.append(key);
    while (m_order.size() > 256) m_cache.remove(m_order.takeFirst());
    m_context->doneCurrent();
    return result;
}

static Renderer &renderer()
{
    static Renderer r;
    return r;
}

} // namespace

QPixmap renderShaderPixmapGpu(const QPixmap &base, const QString &shaderName, double intensity, bool overlayOnBase, bool *ok)
{
    if (ok) *ok = false;
    GpuKind kind;
    if (!nameToKind(shaderName, kind)) return QPixmap();
    QPixmap result = renderer().render(base, kind, intensity, overlayOnBase);
    const bool good = !result.isNull();
    if (ok) *ok = good;
    return result;
}

QPixmap renderEditionPixmapGpu(const QPixmap &base, Edition edition, double intensity, bool *ok)
{
    if (ok) *ok = false;
    QString shader;
    bool overlay = true;
    switch (edition) {
    case Edition::Foil: shader = QStringLiteral("foil"); break;
    case Edition::Holographic: shader = QStringLiteral("holo"); break;
    case Edition::Polychrome: shader = QStringLiteral("polychrome"); break;
    case Edition::Negative: shader = QStringLiteral("negative"); overlay = false; break;
    default: return base;
    }
    return renderShaderPixmapGpu(base, shader, intensity, overlay, ok);
}

void clearGpuEffectCache()
{
    renderer().clearCache();
}

} // namespace BalatroShaders
