#include "animatedblindchip.h"
#include <QTimer>
#include <QShowEvent>
#include <QHideEvent>
#include <QVariantAnimation>
#include <QPointer>
#include "../utils/shadereffects.h"

QPixmap *AnimatedBlindChip::sSheet = nullptr;
int AnimatedBlindChip::sFrameCount = 21;   // 原版 frames = 21
int AnimatedBlindChip::sFrameSize  = 68;   // 2x 资源：每帧 68×68 (1x 是 34×34)

AnimatedBlindChip::AnimatedBlindChip(QWidget *parent)
    : QLabel(parent)
{
    setAlignment(Qt::AlignCenter);
    setStyleSheet("background:transparent;");
    setAttribute(Qt::WA_TranslucentBackground, true);
    setScaledContents(false);

    mTimer = new QTimer(this);
    mTimer->setInterval(mIntervalMs);
    connect(mTimer, &QTimer::timeout, this, [this]() {
        if (!sSheet || sSheet->isNull() || sFrameCount <= 0) return;
        mFrame = (mFrame + 1) % sFrameCount;
        refreshCurrentFrame();
    });

    ensureSheetLoaded();
    refreshCurrentFrame();
}

void AnimatedBlindChip::ensureSheetLoaded()
{
    if (sSheet && !sSheet->isNull()) return;
    if (!sSheet) sSheet = new QPixmap();
    sSheet->load(":/textures/images/BlindChips.png");
    if (sSheet->isNull()) return;
    // 极端意外尺寸保护：若资源未加载到预期 1428×2108，仍以 68px 帧大小作为兜底。
    if (sSheet->height() > 0 && sSheet->width() > 0) {
        sFrameSize  = 68;
        sFrameCount = qMax(1, sSheet->width() / sFrameSize);
    }
}

void AnimatedBlindChip::setBlindRow(int row)
{
    if (row < 0) row = 0;
    if (mRow == row) return;
    mRow = row;
    mFrame = 0;
    refreshCurrentFrame();
}

void AnimatedBlindChip::setDisplaySize(int sidePx)
{
    sidePx = qMax(8, sidePx);
    if (mDisplaySide == sidePx) return;
    mDisplaySide = sidePx;
    setFixedSize(mDisplaySide, mDisplaySide);
    refreshCurrentFrame();
}

void AnimatedBlindChip::setFrameIntervalMs(int ms)
{
    mIntervalMs = qBound(16, ms, 1000);
    if (mTimer) mTimer->setInterval(mIntervalMs);
}

void AnimatedBlindChip::refreshCurrentFrame()
{
    if (!sSheet || sSheet->isNull()) return;
    const int rows = qMax(1, sSheet->height() / sFrameSize);
    const int useRow = qBound(0, mRow, rows - 1);
    const int useFrame = qBound(0, mFrame, sFrameCount - 1);
    QPixmap pix = sSheet->copy(useFrame * sFrameSize, useRow * sFrameSize,
                               sFrameSize, sFrameSize);
    setPixmap(pix.scaled(mDisplaySide, mDisplaySide,
                         Qt::KeepAspectRatio,
                         Qt::SmoothTransformation));
}

void AnimatedBlindChip::startDissolve(int durationMs)
{
    if (mDissolving) return;
    if (!sSheet || sSheet->isNull()) return;
    mDissolving = true;
    // 暂停帧动画，固定在当前帧上做溶解。
    if (mTimer && mTimer->isActive()) mTimer->stop();

    // 取当前帧渲染基底，溶解过程用 renderDissolvePixmap 重绘 QLabel pixmap。
    const int rows = qMax(1, sSheet->height() / sFrameSize);
    const int useRow = qBound(0, mRow, rows - 1);
    const int useFrame = qBound(0, mFrame, sFrameCount - 1);
    QPixmap base = sSheet->copy(useFrame * sFrameSize, useRow * sFrameSize,
                                 sFrameSize, sFrameSize)
                         .scaled(mDisplaySide, mDisplaySide,
                                 Qt::KeepAspectRatio, Qt::SmoothTransformation);

    auto *anim = new QVariantAnimation(this);
    anim->setDuration(qMax(120, durationMs));
    anim->setStartValue(0.0);
    anim->setEndValue(1.0);
    anim->setEasingCurve(QEasingCurve::InCubic);
    QPointer<AnimatedBlindChip> guard(this);
    connect(anim, &QVariantAnimation::valueChanged, this,
            [guard, base](const QVariant &v) {
                if (!guard) return;
                const double t = qBound(0.0, v.toDouble(), 1.0);
                QPixmap pix = BalatroShaders::renderDissolvePixmap(
                                  base, t,
                                  QColor(255, 230, 130, 200),
                                  QColor(255, 110, 70, 160),
                                  1.0);
                if (!pix.isNull()) guard->setPixmap(pix);
            });
    connect(anim, &QVariantAnimation::finished, this, [guard]() {
        if (!guard) return;
        guard->mDissolving = false;
        // 只清掉 pixmap，不 hide()：保留 QLabel 在布局中的占位，
        // 否则同 HBox 中的"至少得分"会因 chip 消失而突然居中。
        guard->clear();
    });
    anim->start(QAbstractAnimation::DeleteWhenStopped);
}

void AnimatedBlindChip::showEvent(QShowEvent *e)
{
    QLabel::showEvent(e);
    if (mTimer && !mTimer->isActive()) mTimer->start();
}

void AnimatedBlindChip::hideEvent(QHideEvent *e)
{
    QLabel::hideEvent(e);
    if (mTimer && mTimer->isActive()) mTimer->stop();
}
