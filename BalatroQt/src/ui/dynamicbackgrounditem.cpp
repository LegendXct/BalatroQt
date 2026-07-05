#include "dynamicbackgrounditem.h"

#include <QSurfaceFormat>
#include <QVector2D>
#include <QVector4D>
#include <algorithm>
#include <cmath>

namespace {
static int clamp255(double v)
{
    return int(std::max(0.0, std::min(255.0, std::round(v))));
}

static QColor rgb(int r, int g, int b, int a = 255)
{
    return QColor(clamp255(r), clamp255(g), clamp255(b), clamp255(a));
}

static QColor scaleColour(const QColor &c, double k)
{
    return rgb(c.red() * k, c.green() * k, c.blue() * k, c.alpha());
}

static QColor mixColour(const QColor &a, const QColor &b, double t)
{
    t = std::max(0.0, std::min(1.0, t));
    return rgb(a.red() + (b.red() - a.red()) * t,
               a.green() + (b.green() - a.green()) * t,
               a.blue() + (b.blue() - a.blue()) * t,
               a.alpha() + (b.alpha() - a.alpha()) * t);
}

static constexpr const char *kVertexShader = R"GLSL(
attribute vec2 position;
varying vec2 v_pos;
void main()
{
    v_pos = position * 0.5 + 0.5;
    gl_Position = vec4(position, 0.0, 1.0);
}
)GLSL";

static constexpr const char *kFragmentShader = R"GLSL(
#ifdef GL_ES
precision highp float;
#endif

uniform highp float time;
uniform highp float spin_time;
uniform highp vec4 colour_1;
uniform highp vec4 colour_2;
uniform highp vec4 colour_3;
uniform highp float contrast;
uniform highp float spin_amount;
uniform highp vec2 screen_size;
varying highp vec2 v_pos;

#define PIXEL_SIZE_FAC 700.0
#define SPIN_EASE 0.5

void main()
{
    highp vec2 screen_coords = v_pos * screen_size;
    highp float screen_len = length(screen_size);
    highp float pixel_size = screen_len / PIXEL_SIZE_FAC;
    highp vec2 uv = (floor(screen_coords.xy * (1.0 / pixel_size)) * pixel_size - 0.5 * screen_size.xy) / screen_len - vec2(0.12, 0.0);
    highp float uv_len = length(uv);

    highp float speed = (spin_time * SPIN_EASE * 0.2) + 302.2;
    highp float new_pixel_angle = atan(uv.y, uv.x) + speed - SPIN_EASE * 20.0 * (spin_amount * uv_len + (1.0 - spin_amount));
    highp vec2 mid = (screen_size.xy / screen_len) / 2.0;
    uv = (vec2((uv_len * cos(new_pixel_angle) + mid.x), (uv_len * sin(new_pixel_angle) + mid.y)) - mid);

    uv *= 30.0;
    speed = time * 2.0;
    highp vec2 uv2 = vec2(uv.x + uv.y);

    for (int i = 0; i < 5; i++) {
        uv2 += sin(max(uv.x, uv.y)) + uv;
        uv += 0.5 * vec2(cos(5.1123314 + 0.353 * uv2.y + speed * 0.131121), sin(uv2.x - 0.113 * speed));
        uv -= 1.0 * cos(uv.x + uv.y) - 1.0 * sin(uv.x * 0.711 - uv.y);
    }

    highp float contrast_mod = (0.25 * contrast + 0.5 * spin_amount + 1.2);
    highp float paint_res = min(2.0, max(0.0, length(uv) * 0.035 * contrast_mod));
    highp float c1p = max(0.0, 1.0 - contrast_mod * abs(1.0 - paint_res));
    highp float c2p = max(0.0, 1.0 - contrast_mod * abs(paint_res));
    highp float c3p = 1.0 - min(1.0, c1p + c2p);

    highp vec4 ret_col = (0.3 / max(contrast, 0.001)) * colour_1
        + (1.0 - 0.3 / max(contrast, 0.001)) * (colour_1 * c1p + colour_2 * c2p + vec4(c3p * colour_3.rgb, c3p * colour_1.a));

    gl_FragColor = ret_col;
}
)GLSL";
} // namespace

DynamicBackgroundItem::DynamicBackgroundItem(QWidget *parent)
    : QOpenGLWidget(parent)
{
    QSurfaceFormat fmt;
    fmt.setRenderableType(QSurfaceFormat::DefaultRenderableType);
    fmt.setSwapBehavior(QSurfaceFormat::DoubleBuffer);
    fmt.setSamples(0);
    setFormat(fmt);

    setAutoFillBackground(false);
    setAttribute(Qt::WA_NoSystemBackground, true);
    setUpdateBehavior(QOpenGLWidget::PartialUpdate);

    updateTargets();
    mCurrentA = mTargetA;
    mCurrentB = mTargetB;
    mCurrentC = mTargetC;
    mCurrentContrast = mTargetContrast;
    mCurrentSpin = mTargetSpin;
    mVisualsReady = true;

    mClock.start();
    mLastTick = 0.0;
    mTimer.setTimerType(Qt::PreciseTimer);
    connect(&mTimer, &QTimer::timeout, this, [this]() {
        const double now = mClock.elapsed() / 1000.0;
        const double dt = std::max(0.001, std::min(0.080, now - mLastTick));
        mLastTick = now;
        mTime = now;
        mSpinTime += dt * mCurrentSpin;
        easeVisuals(dt);
        update();
    });
    mTimer.start(16);
}

DynamicBackgroundItem::~DynamicBackgroundItem()
{
    makeCurrent();
    mProgram.reset();
    doneCurrent();
}

void DynamicBackgroundItem::setSceneSize(qreal w, qreal h)
{
    const qreal nw = qMax<qreal>(1, w);
    const qreal nh = qMax<qreal>(1, h);
    if (qFuzzyCompare(nw, mW) && qFuzzyCompare(nh, mH)) return;
    mW = nw;
    mH = nh;
    update();
}

void DynamicBackgroundItem::setMood(Mood mood)
{
    if (mMood == mood) return;
    mMood = mood;
    updateTargets();
    update();
}

void DynamicBackgroundItem::initializeGL()
{
    initializeOpenGLFunctions();
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_BLEND);

    mProgram = std::make_unique<QOpenGLShaderProgram>();
    mProgramReady = mProgram->addShaderFromSourceCode(QOpenGLShader::Vertex, kVertexShader)
                 && mProgram->addShaderFromSourceCode(QOpenGLShader::Fragment, kFragmentShader)
                 && mProgram->link();
}

void DynamicBackgroundItem::resizeGL(int w, int h)
{
    mW = qMax(1, w);
    mH = qMax(1, h);
    const qreal dpr = devicePixelRatioF();
    glViewport(0, 0, qMax(1, int(std::round(mW * dpr))), qMax(1, int(std::round(mH * dpr))));
}

void DynamicBackgroundItem::paintGL()
{
    glClearColor(mCurrentA.redF(), mCurrentA.greenF(), mCurrentA.blueF(), 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    if (!mProgramReady || !mProgram) return;

    mProgram->bind();
    mProgram->setUniformValue("time", GLfloat(400.0 + mTime));
    mProgram->setUniformValue("spin_time", GLfloat(mSpinTime));
    mProgram->setUniformValue("contrast", GLfloat(mCurrentContrast));
    mProgram->setUniformValue("spin_amount", GLfloat(mCurrentSpin));
    const qreal dpr = devicePixelRatioF();
    mProgram->setUniformValue("screen_size", QVector2D(GLfloat(std::max<qreal>(1, mW * dpr)),
                                                       GLfloat(std::max<qreal>(1, mH * dpr))));
    sendColourUniform("colour_1", mCurrentA);
    sendColourUniform("colour_2", mCurrentB);
    sendColourUniform("colour_3", mCurrentC);

    static const GLfloat verts[] = {
        -1.0f, -1.0f,
         1.0f, -1.0f,
        -1.0f,  1.0f,
         1.0f,  1.0f
    };
    const int loc = mProgram->attributeLocation("position");
    mProgram->enableAttributeArray(loc);
    mProgram->setAttributeArray(loc, GL_FLOAT, verts, 2);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    mProgram->disableAttributeArray(loc);
    mProgram->release();
}

void DynamicBackgroundItem::sendColourUniform(const char *name, const QColor &c)
{
    if (!mProgram) return;
    mProgram->setUniformValue(name, QVector4D(GLfloat(c.redF()), GLfloat(c.greenF()), GLfloat(c.blueF()), GLfloat(c.alphaF())));
}

QColor DynamicBackgroundItem::baseA() const
{
    const QColor black = rgb(0x37, 0x42, 0x44);
    const QColor red = rgb(0xFE, 0x5F, 0x55);
    const QColor blindSmall = rgb(0x50, 0x84, 0x6e);
    const QColor boss = rgb(0xb4, 0x44, 0x30);
    switch (mMood) {
    case Mood::Tarot:       return scaleColour(black, 0.80);
    case Mood::Spectral:    return scaleColour(black, 0.80);
    case Mood::Standard:    return red;
    case Mood::Buffoon:     return black;
    case Mood::Celestial:   return scaleColour(black, 0.90);
    case Mood::Shop:        return scaleColour(blindSmall, 0.90);
    case Mood::BlindSelect: return scaleColour(blindSmall, 0.90);
    case Mood::Boss:        return boss;
    case Mood::Default:     return scaleColour(blindSmall, 0.90);
    }
    return scaleColour(blindSmall, 0.90);
}

QColor DynamicBackgroundItem::baseB() const
{
    const QColor black = rgb(0x37, 0x42, 0x44);
    const QColor blindSmall = rgb(0x50, 0x84, 0x6e);
    const QColor bossNew = rgb(0x6d, 0x55, 0x51);
    switch (mMood) {
    case Mood::Tarot:       return scaleColour(rgb(0x88, 0x67, 0xa5), 1.30);
    case Mood::Spectral:    return scaleColour(rgb(0x45, 0x84, 0xfa), 1.30);
    case Mood::Standard:    return scaleColour(rgb(0x2c, 0x35, 0x36), 1.30);
    case Mood::Buffoon:     return scaleColour(rgb(0xff, 0x9a, 0x00), 1.30);
    case Mood::Celestial:   return scaleColour(black, 1.30);
    case Mood::Shop:        return scaleColour(blindSmall, 1.30);
    case Mood::BlindSelect: return scaleColour(blindSmall, 1.30);
    case Mood::Boss:        return scaleColour(bossNew, 1.30);
    case Mood::Default:     return scaleColour(blindSmall, 1.30);
    }
    return scaleColour(blindSmall, 1.30);
}

QColor DynamicBackgroundItem::accent() const
{
    const QColor black = rgb(0x37, 0x42, 0x44);
    const QColor blindSmall = rgb(0x50, 0x84, 0x6e);
    const QColor bossNew = rgb(0x6d, 0x55, 0x51);
    switch (mMood) {
    case Mood::Tarot:       return scaleColour(rgb(0x88, 0x67, 0xa5), 0.40);
    case Mood::Spectral:    return scaleColour(rgb(0x45, 0x84, 0xfa), 0.40);
    case Mood::Standard:    return scaleColour(rgb(0x2c, 0x35, 0x36), 0.40);
    case Mood::Buffoon:     return scaleColour(rgb(0xff, 0x9a, 0x00), 0.40);
    case Mood::Celestial:   return scaleColour(black, 0.70);
    case Mood::Shop:        return scaleColour(blindSmall, 0.70);
    case Mood::BlindSelect: return scaleColour(blindSmall, 0.70);
    case Mood::Boss:        return scaleColour(bossNew, 0.40);
    case Mood::Default:     return scaleColour(blindSmall, 0.70);
    }
    return scaleColour(blindSmall, 0.70);
}

double DynamicBackgroundItem::targetContrast() const
{
    switch (mMood) {
    case Mood::Tarot:       return 1.5;
    case Mood::Spectral:    return 2.0;
    case Mood::Standard:    return 3.0;
    case Mood::Buffoon:     return 2.0;
    case Mood::Celestial:   return 3.0;
    case Mood::Shop:        return 1.0;
    case Mood::BlindSelect: return 1.0;
    case Mood::Boss:        return 2.0;
    case Mood::Default:     return 1.0;
    }
    return 1.0;
}

double DynamicBackgroundItem::targetSpin() const
{
    switch (mMood) {
    case Mood::Tarot:       return 0.055;
    case Mood::Spectral:    return 0.085;
    case Mood::Standard:    return 0.030;
    case Mood::Buffoon:     return 0.050;
    case Mood::Celestial:   return 0.065;
    case Mood::Shop:        return 0.025;
    case Mood::BlindSelect: return 0.012;
    case Mood::Boss:        return 0.024;
    case Mood::Default:     return 0.018;
    }
    return 0.018;
}

void DynamicBackgroundItem::updateTargets()
{
    mTargetA = baseA();
    mTargetB = baseB();
    mTargetC = accent();
    mTargetContrast = targetContrast();
    mTargetSpin = targetSpin();

    if (!mVisualsReady) {
        mCurrentA = mTargetA;
        mCurrentB = mTargetB;
        mCurrentC = mTargetC;
        mCurrentContrast = mTargetContrast;
        mCurrentSpin = mTargetSpin;
        mVisualsReady = true;
    }
}

void DynamicBackgroundItem::easeVisuals(double dt)
{
    const double a = 1.0 - std::exp(-dt * 5.5);
    mCurrentA = mixColour(mCurrentA, mTargetA, a);
    mCurrentB = mixColour(mCurrentB, mTargetB, a);
    mCurrentC = mixColour(mCurrentC, mTargetC, a);
    mCurrentContrast += (mTargetContrast - mCurrentContrast) * a;
    mCurrentSpin += (mTargetSpin - mCurrentSpin) * a;
}
