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
    // 显示尺寸:原图是 @2x 226×114。原本压到 200 让侧边栏 SHOP 招牌略显小气，
    // 拉大到 280 让它撑满更多侧边栏宽度，跟其它组件视觉权重一致。
    if (mFrameW == 0) return QSize(280, 140);
    int targetW = 280;
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
