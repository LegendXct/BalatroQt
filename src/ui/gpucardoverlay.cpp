#include "gpucardoverlay.h"

#include <QGraphicsScene>
#include <QGraphicsView>
#include <QGraphicsItem>
#include <QOpenGLShaderProgram>
#include <QSurfaceFormat>
#include <QVector2D>
#include <QVector>
#include <QImage>
#include <QPainter>
#include <algorithm>
#include <cmath>

#include "../card/carditem.h"
#include "../card/jokeritem.h"
#include "../card/consumableitem.h"
#include "../utils/shadereffects.h"

namespace {
static constexpr const char *kVertexShader = R"GLSL(
attribute highp vec2 position;
attribute highp vec2 texcoord;
varying highp vec2 v_tex;
void main()
{
    v_tex = texcoord;
    gl_Position = vec4(position, 0.0, 1.0);
}
)GLSL";

static constexpr const char *kFragmentShader = R"GLSL(
#ifdef GL_ES
precision highp float;
#endif

uniform sampler2D tex;
uniform highp float time;
uniform highp float intensity;
uniform int effect;
varying highp vec2 v_tex;

highp float hue(highp float s, highp float t, highp float h)
{
    highp float hs = mod(h, 1.0) * 6.0;
    if (hs < 1.0) return (t - s) * hs + s;
    if (hs < 3.0) return t;
    if (hs < 4.0) return (t - s) * (4.0 - hs) + s;
    return s;
}

highp vec4 RGB(highp vec4 c)
{
    if (c.g < 0.0001) return vec4(c.b, c.b, c.b, c.a);
    highp float t = (c.b < 0.5) ? c.g * c.b + c.b : -c.g * c.b + (c.g + c.b);
    highp float s = 2.0 * c.b - t;
    return vec4(hue(s, t, c.r + 1.0 / 3.0), hue(s, t, c.r), hue(s, t, c.r - 1.0 / 3.0), c.a);
}

highp vec4 HSL(highp vec4 c)
{
    highp float low = min(c.r, min(c.g, c.b));
    highp float high = max(c.r, max(c.g, c.b));
    highp float delta = high - low;
    highp float sum = high + low;
    highp vec4 hsl = vec4(0.0, 0.0, 0.5 * sum, c.a);
    if (abs(delta) < 0.000001) return hsl;
    hsl.g = (hsl.b < 0.5) ? delta / max(0.000001, sum) : delta / max(0.000001, 2.0 - sum);
    if (high == c.r) hsl.r = (c.g - c.b) / delta;
    else if (high == c.g) hsl.r = (c.b - c.r) / delta + 2.0;
    else hsl.r = (c.r - c.g) / delta + 4.0;
    hsl.r = mod(hsl.r / 6.0, 1.0);
    return hsl;
}

highp float shader_field(highp vec2 uv_scaled, highp float t)
{
    highp vec2 p1 = uv_scaled + vec2(50.0 * sin(-t / 143.6340), 50.0 * cos(-t / 99.4324));
    highp vec2 p2 = uv_scaled + vec2(50.0 * cos( t /  53.1532), 50.0 * cos( t / 61.4532));
    highp vec2 p3 = uv_scaled + vec2(50.0 * sin(-t /  87.53218), 50.0 * sin(-t / 49.0000));
    return (1.0 + (cos(length(p1) / 19.483) + sin(length(p2) / 33.155) * cos(p2.y / 15.73) +
                   cos(length(p3) / 27.193) * sin(p3.x / 21.92))) / 2.0;
}

void main()
{
    highp vec2 uv = v_tex;
    highp vec4 texel = texture2D(tex, uv);
    if (texel.a <= 0.0001) discard;

    highp float t = 400.0 + time * 0.28;
    highp float phase_x = time * 0.16;
    highp float phase_y = time * 0.11;

    if (effect == 1) {
        highp vec2 adjusted = uv - vec2(0.5);
        adjusted.x *= 142.0 / 190.0;
        highp float low = min(texel.r, min(texel.g, texel.b));
        highp float high = max(texel.r, max(texel.g, texel.b));
        highp float delta = min(high, max(0.5, 1.0 - low));
        highp float len90 = length(adjusted * 90.0);
        highp float len113 = length(adjusted * 113.1121);
        highp float fac = max(min(2.0 * sin((len90 + phase_x * 2.0) + 3.0 * (1.0 + 0.8 * cos(len113 - phase_x * 3.121))) - 1.0 - max(5.0 - len90, 0.0), 1.0), 0.0);
        highp vec2 rotater = vec2(cos(phase_y * 0.1221), sin(phase_y * 0.3512));
        highp float angle = dot(rotater, adjusted) / max(0.000001, length(rotater) * length(adjusted));
        highp float fac2 = max(min(5.0 * cos(phase_y * 0.3 + angle * 3.14 * (2.2 + 0.9 * sin(phase_y * 1.65 + 0.2 * phase_y))) - 4.0 - max(2.0 - length(adjusted * 20.0), 0.0), 1.0), 0.0);
        highp float fac3 = 0.3 * max(min(2.0 * sin(phase_x * 5.0 + uv.x * 3.0 + 3.0 * (1.0 + 0.5 * cos(phase_x * 7.0))) - 1.0, 1.0), -1.0);
        highp float fac4 = 0.3 * max(min(2.0 * sin(phase_x * 6.66 + uv.y * 3.8 + 3.0 * (1.0 + 0.5 * cos(phase_x * 3.414))) - 1.0, 1.0), -1.0);
        highp float maxfac = max(max(fac, max(fac2, max(fac3, max(fac4, 0.0)))) + 2.2 * (fac + fac2 + fac3 + fac4), 0.0);
        texel.r = texel.r - delta + delta * maxfac * 0.3;
        texel.g = texel.g - delta + delta * maxfac * 0.3;
        texel.b = texel.b + delta * maxfac * 1.9;
        texel.a = min(texel.a, 0.3 * texel.a + 0.9 * min(0.5, maxfac * 0.1));
        gl_FragColor = texel * intensity;
        return;
    }

    if (effect == 2) {
        highp vec4 hsl = HSL(texel * 0.5 + vec4(0.0, 0.0, 0.5, texel.a * 0.5));
        highp float st = phase_y * 7.221 + t;
        highp vec2 floored = vec2(floor(uv.x * 142.0) / 142.0, floor(uv.y * 190.0) / 190.0);
        highp float field = shader_field((floored - vec2(0.5)) * 250.0, st);
        highp float res = 0.5 + 0.5 * cos(phase_x * 2.612 + (field - 0.5) * 3.14);
        highp float low = min(texel.r, min(texel.g, texel.b));
        highp float high = max(texel.r, max(texel.g, texel.b));
        highp float delta = 0.2 + 0.3 * (high - low) + 0.1 * high;
        highp float gridsize = 0.79;
        highp float fac = 0.5 * max(max(max(0.0, 7.0 * abs(cos(uv.x * gridsize * 20.0)) - 6.0),
                                         max(0.0, 7.0 * cos(uv.y * gridsize * 45.0 + uv.x * gridsize * 20.0) - 6.0)),
                                    max(0.0, 7.0 * cos(uv.y * gridsize * 45.0 - uv.x * gridsize * 20.0) - 6.0));
        hsl.r += res + fac;
        hsl.g *= 1.3;
        hsl.b = hsl.b * 0.6 + 0.4;
        highp vec4 rgb = RGB(hsl) * vec4(0.9, 0.8, 1.2, texel.a);
        texel = texel * (1.0 - delta) + rgb * delta;
        if (texel.a < 0.7) texel.a /= 3.0;
        gl_FragColor = texel * intensity;
        return;
    }

    if (effect == 3) {
        highp float low = min(texel.r, min(texel.g, texel.b));
        highp float high = max(texel.r, max(texel.g, texel.b));
        highp float delta = high - low;
        highp float saturation_fac = 1.0 - max(0.0, 0.05 * (1.1 - delta));
        highp vec4 hsl = HSL(vec4(texel.r * saturation_fac, texel.g * saturation_fac, texel.b, texel.a));
        highp vec2 floored = vec2(floor(uv.x * 142.0) / 142.0, floor(uv.y * 190.0) / 190.0);
        highp float field = shader_field((floored - vec2(0.5)) * 50.0, phase_y * 2.221 + t);
        highp float res = 0.5 + 0.5 * cos(phase_x * 2.612 + (field - 0.5) * 3.14);
        hsl.r += res + phase_y * 0.04;
        hsl.g = min(0.6, hsl.g + 0.5);
        texel = RGB(hsl);
        if (texel.a < 0.7) texel.a /= 3.0;
        gl_FragColor = texel * intensity;
        return;
    }

    if (effect == 4) {
        highp vec4 hsl = HSL(texel);
        hsl.b = 1.0 - hsl.b;
        hsl.r = -hsl.r + 0.2;
        texel = RGB(hsl) + vec4(79.0 / 255.0, 99.0 / 255.0, 103.0 / 255.0, 0.0) * 0.8;
        texel.a = max(texel.a, 0.92);
        gl_FragColor = texel * intensity;
        return;
    }

    if (effect == 5) {
        highp float low = min(texel.r, min(texel.g, texel.b));
        highp float high = max(texel.r, max(texel.g, texel.b));
        highp float delta = max(0.0, high - low - 0.1);
        highp float fac  = 0.8 + 0.9 * sin(13.0 * uv.x + 5.32 * uv.y + phase_x * 12.0 + cos(phase_x * 5.3 + uv.y * 4.2 - uv.x * 4.0));
        highp float fac2 = 0.5 + 0.5 * sin(10.0 * uv.x + 2.32 * uv.y + phase_x * 5.0 - cos(phase_x * 2.3 + uv.x * 8.2));
        highp float fac3 = 0.5 + 0.5 * sin(12.0 * uv.x + 6.32 * uv.y + phase_x * 6.111 + sin(phase_x * 5.3 + uv.y * 3.2));
        highp float fac4 = 0.5 + 0.5 * sin(4.0 * uv.x + 2.32 * uv.y + phase_x * 8.111 + sin(phase_x * 1.3 + uv.y * 13.2));
        highp float fac5 = sin(0.5 * 16.0 * uv.x + 5.32 * uv.y + phase_x * 12.0 + cos(phase_x * 5.3 + uv.y * 4.2 - uv.x * 4.0));
        highp float maxfac = 0.6 * max(max(fac, max(fac2, max(fac3, 0.0))) + (fac + fac2 + fac3 * fac4), 0.0);
        texel.rgb = texel.rgb * 0.5 + vec3(0.4, 0.4, 0.8);
        texel.r = texel.r - delta + delta * maxfac * (0.7 + fac5 * 0.07) - 0.1;
        texel.g = texel.g - delta + delta * maxfac * (0.7 - fac5 * 0.17) - 0.1;
        texel.b = texel.b - delta + delta * maxfac * 0.7 - 0.1;
        texel.a = texel.a * (0.8 * max(min(1.0, max(0.0, 0.3 * max(low * 0.2, delta) + min(max(maxfac * 0.1, 0.0), 0.4))), 0.0) + 0.15 * maxfac * (0.1 + delta));
        gl_FragColor = texel * intensity;
        return;
    }

    gl_FragColor = texel;
}
)GLSL";

static int editionToEffect(Edition edition)
{
    switch (edition) {
    case Edition::Foil: return 1;
    case Edition::Holographic: return 2;
    case Edition::Polychrome: return 3;
    case Edition::Negative: return 4;
    case Edition::None: break;
    }
    return 0;
}
}

GpuCardOverlay::GpuCardOverlay(QGraphicsView *view, QGraphicsScene *scene, QWidget *parent)
    : QOpenGLWidget(parent), mView(view), mScene(scene)
{
    QSurfaceFormat fmt;
    fmt.setRenderableType(QSurfaceFormat::DefaultRenderableType);
    fmt.setAlphaBufferSize(8);
    fmt.setSwapBehavior(QSurfaceFormat::DoubleBuffer);
    fmt.setSamples(0);
    setFormat(fmt);

    setAutoFillBackground(false);
    setAttribute(Qt::WA_TranslucentBackground, true);
    setAttribute(Qt::WA_TransparentForMouseEvents, true);
    setAttribute(Qt::WA_AlwaysStackOnTop, true);
    setUpdateBehavior(QOpenGLWidget::PartialUpdate);

    mClock.start();
    mTimer.setTimerType(Qt::PreciseTimer);
    connect(&mTimer, &QTimer::timeout, this, [this]() { update(); });
    mTimer.start(16);
}

GpuCardOverlay::~GpuCardOverlay()
{
    makeCurrent();
    releaseTextures();
    mProgram.reset();
    doneCurrent();
}

void GpuCardOverlay::setViewAndScene(QGraphicsView *view, QGraphicsScene *scene)
{
    mView = view;
    mScene = scene;
    update();
}

void GpuCardOverlay::initializeGL()
{
    initializeOpenGLFunctions();
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    mProgram = std::make_unique<QOpenGLShaderProgram>();
    mProgramReady = mProgram->addShaderFromSourceCode(QOpenGLShader::Vertex, kVertexShader)
                 && mProgram->addShaderFromSourceCode(QOpenGLShader::Fragment, kFragmentShader)
                 && mProgram->link();
}

void GpuCardOverlay::resizeGL(int w, int h)
{
    const qreal dpr = devicePixelRatioF();
    glViewport(0, 0, std::max(1, int(std::round(w * dpr))), std::max(1, int(std::round(h * dpr))));
}

void GpuCardOverlay::paintGL()
{
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    if (!mProgramReady || !mProgram || !mView || !mScene) return;

    QVector<DrawCall> calls;
    collectDrawCalls(calls);
    if (calls.isEmpty()) return;

    std::sort(calls.begin(), calls.end(), [](const DrawCall &a, const DrawCall &b) {
        return a.z < b.z;
    });

    mProgram->bind();
    mProgram->setUniformValue("tex", 0);
    const float t = float(mClock.elapsed() / 1000.0);
    for (const DrawCall &call : calls) drawCall(call, t);
    mProgram->release();
}

void GpuCardOverlay::collectDrawCalls(QVector<DrawCall> &out) const
{
    if (!mScene || !mView) return;
    const QList<QGraphicsItem*> items = mScene->items();
    for (QGraphicsItem *raw : items) {
        if (!raw || !raw->isVisible() || raw->opacity() <= 0.001) continue;
        if (auto *card = dynamic_cast<CardItem*>(raw)) {
            const int fx = editionToEffect(card->gpuEdition());
            if (fx) appendItemQuad(out, card, card->gpuBasePixmap(), fx, 0.04, card->opacity());
            continue;
        }
        if (auto *joker = dynamic_cast<JokerItem*>(raw)) {
            const int fx = editionToEffect(joker->gpuEdition());
            if (fx) appendItemQuad(out, joker, joker->gpuBasePixmap(), fx, 0.04, joker->opacity());
            if (joker->gpuHasHologramFloating())
                appendItemQuad(out, joker, joker->gpuFloatingPixmap(), 5, 0.08, joker->opacity());
            continue;
        }
        if (auto *cons = dynamic_cast<ConsumableItem*>(raw)) {
            const int fx = editionToEffect(cons->gpuEdition());
            if (fx) appendItemQuad(out, cons, cons->gpuBasePixmap(), fx, 0.04, cons->opacity());
            continue;
        }
    }
}

void GpuCardOverlay::appendItemQuad(QVector<DrawCall> &out, const QGraphicsItem *item,
                                    const QPixmap &pixmap, int effect, qreal zBias, qreal opacity) const
{
    if (!item || pixmap.isNull() || effect == 0 || !mView) return;
    QPolygonF scenePoly = item->mapToScene(item->boundingRect());
    if (scenePoly.size() < 4) return;
    QPolygon viewPoly = mView->mapFromScene(scenePoly);
    if (viewPoly.size() < 4) return;

    QRect viewportRect(QPoint(0, 0), size());
    if (!viewportRect.adjusted(-220, -220, 220, 220).intersects(viewPoly.boundingRect())) return;

    DrawCall call;
    call.pixmap = pixmap;
    call.p0 = viewPoly.at(0);
    call.p1 = viewPoly.at(1);
    call.p2 = viewPoly.at(2);
    call.p3 = viewPoly.at(3);
    call.effect = effect;
    call.z = item->zValue() + zBias;
    call.opacity = opacity;
    out.append(call);
}

unsigned int GpuCardOverlay::textureForPixmap(const QPixmap &pixmap)
{
    const qint64 key = pixmap.cacheKey();
    auto it = mTextures.constFind(key);
    if (it != mTextures.constEnd()) return it.value();

    QImage img = pixmap.toImage().convertToFormat(QImage::Format_RGBA8888);
    unsigned int texId = 0;
    glGenTextures(1, &texId);
    glBindTexture(GL_TEXTURE_2D, texId);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, img.width(), img.height(), 0, GL_RGBA, GL_UNSIGNED_BYTE, img.constBits());
    mTextures.insert(key, texId);
    return texId;
}

void GpuCardOverlay::releaseTextures()
{
    for (auto it = mTextures.begin(); it != mTextures.end(); ++it) {
        unsigned int id = it.value();
        if (id) glDeleteTextures(1, &id);
    }
    mTextures.clear();
}

void GpuCardOverlay::drawCall(const DrawCall &call, float time)
{
    const unsigned int texture = textureForPixmap(call.pixmap);
    if (!texture) return;

    const qreal dpr = devicePixelRatioF();
    const float w = std::max(1.0f, float(width() * dpr));
    const float h = std::max(1.0f, float(height() * dpr));
    auto ndc = [&](const QPointF &p) -> QVector2D {
        const float x = float(p.x() * dpr);
        const float y = float(p.y() * dpr);
        return QVector2D((x / w) * 2.0f - 1.0f, 1.0f - (y / h) * 2.0f);
    };

    const QVector2D a = ndc(call.p0);
    const QVector2D b = ndc(call.p1);
    const QVector2D c = ndc(call.p3);
    const QVector2D d = ndc(call.p2);

    const GLfloat verts[] = {
        a.x(), a.y(), 0.0f, 0.0f,
        b.x(), b.y(), 1.0f, 0.0f,
        c.x(), c.y(), 0.0f, 1.0f,
        d.x(), d.y(), 1.0f, 1.0f
    };

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture);
    mProgram->setUniformValue("time", time);
    mProgram->setUniformValue("intensity", GLfloat(std::max<qreal>(0.0, std::min<qreal>(1.0, call.opacity))));
    mProgram->setUniformValue("effect", call.effect);

    const int posLoc = mProgram->attributeLocation("position");
    const int uvLoc = mProgram->attributeLocation("texcoord");
    mProgram->enableAttributeArray(posLoc);
    mProgram->enableAttributeArray(uvLoc);
    mProgram->setAttributeArray(posLoc, GL_FLOAT, verts, 2, 4 * sizeof(GLfloat));
    mProgram->setAttributeArray(uvLoc, GL_FLOAT, verts + 2, 2, 4 * sizeof(GLfloat));
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    mProgram->disableAttributeArray(posLoc);
    mProgram->disableAttributeArray(uvLoc);
}
