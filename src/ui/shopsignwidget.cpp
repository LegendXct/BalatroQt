#include "shopsignwidget.h"
#include <QPainter>

ShopSignWidget::ShopSignWidget(QWidget *parent)
    : QWidget(parent)
{
    mSheet = QPixmap(":/textures/images/ShopSignAnimation.png");
    if (!mSheet.isNull()) {
        // 4 帧横排:单帧宽度 = 总宽 / 4
        mFrameW = mSheet.width() / mFrameCount;
        mFrameH = mSheet.height();
    }

    // 10 FPS = 每 100ms 切下一帧
    connect(&mTimer, &QTimer::timeout, this, [this]() {
        mCurrent = (mCurrent + 1) % mFrameCount;
        update();
    });
    mTimer.start(100);

    setAttribute(Qt::WA_TranslucentBackground, true);
}

ShopSignWidget::~ShopSignWidget() = default;

QSize ShopSignWidget::sizeHint() const
{
    // 显示尺寸:原图是 @2x 226×114,缩到 226×114(直接 1:1)或更大都行
    // 为侧边栏适配,目标宽度 200(留 padding)→ 高度按比例
    if (mFrameW == 0) return QSize(200, 100);
    int targetW = 200;
    int targetH = targetW * mFrameH / mFrameW;
    return QSize(targetW, targetH);
}

void ShopSignWidget::paintEvent(QPaintEvent *)
{
    if (mSheet.isNull() || mFrameW == 0) return;
    QPainter p(this);
    p.setRenderHint(QPainter::SmoothPixmapTransform, false);  // 像素风,不平滑

    QRect src(mCurrent * mFrameW, 0, mFrameW, mFrameH);
    QRect dst(0, 0, width(), height());
    p.drawPixmap(dst, mSheet, src);
}
