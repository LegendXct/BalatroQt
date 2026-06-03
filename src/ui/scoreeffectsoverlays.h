#ifndef SCOREEFFECTSOVERLAYS_H
#define SCOREEFFECTSOVERLAYS_H

#include <QColor>
#include <QElapsedTimer>
#include <QOpenGLFunctions>
#include <QOpenGLShaderProgram>
#include <QOpenGLWidget>
#include <QPixmap>
#include <QTimer>
#include <QVector2D>
#include <QVector4D>
#include <QWidget>

// 计分火焰瓦片：普通 QWidget，paintEvent 里画 圆角底框(蓝/红) + 离屏渲染的火焰 QPixmap。
// 火焰之外透明，数字标签浮在本控件之上即可（数字在火焰上方）。比之前的半透明
// QOpenGLWidget 叠层方案可靠：不会盖不住下层、不会与分数框之间出现缝隙/边框。
class FlameTile : public QWidget
{
    Q_OBJECT
public:
    explicit FlameTile(QWidget *parent = nullptr);
    void setColours(const QColor &colour1, const QColor &colour2);  // 火焰主色 / 底部高光色
    void setBaseColour(const QColor &base);                         // 圆角底框颜色（蓝/红）
    void setBoxGeometry(int boxHeight, int radius);                 // 底框高度（数字区）+圆角
    void setFlameId(float id);
    void setAmount(float amount);   // 由 mainwindow 弹簧 tick 直接喂 real 值
    void stop();

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    void tick();

    QColor mBase = QColor(0, 157, 255);
    QColor mColour1 = QColor(0, 157, 255, 235);
    QColor mColour2 = QColor(255, 230, 120, 255);
    float mId = 13.37f;
    int mBoxH = 0;          // 底框高度；0 = 整个控件都是底框
    int mBoxRadius = 8;
    double mAmount = 0.0;
    double mTime = 0.0;
    qint64 mLastMs = 0;
    QPixmap mFlame;
    QTimer mTimer;
    QElapsedTimer mClock;
};

class FlashShaderOverlay : public QOpenGLWidget, protected QOpenGLFunctions
{
    Q_OBJECT
public:
    explicit FlashShaderOverlay(QWidget *parent = nullptr);
    void trigger(float midFlash = 0.65f, int durationMs = 360);

protected:
    void initializeGL() override;
    void resizeGL(int w, int h) override;
    void paintGL() override;

private:
    QOpenGLShaderProgram mProgram;
    QTimer mTimer;
    QElapsedTimer mClock;
    float mMidFlash = 0.0f;
    int mDurationMs = 0;
    bool mReady = false;
};

#endif // SCOREEFFECTSOVERLAYS_H
