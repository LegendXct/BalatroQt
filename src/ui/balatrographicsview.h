#ifndef BALATROGRAPHICSVIEW_H
#define BALATROGRAPHICSVIEW_H

#include <QColor>
#include <QElapsedTimer>
#include <QGraphicsView>
#include <QOpenGLFunctions>
#include <QOpenGLShaderProgram>
#include <QSize>
#include <QTimer>
#include <memory>

#include "../game/bossblind.h"

class BalatroGraphicsView : public QGraphicsView, protected QOpenGLFunctions
{
    Q_OBJECT
public:
    enum class Mood {
        Default,
        Shop,
        Tarot,
        Spectral,
        Celestial,
        Buffoon,
        Standard,
        BlindSelect,
        Boss,
    };

    explicit BalatroGraphicsView(QGraphicsScene *scene, QWidget *parent = nullptr);
    ~BalatroGraphicsView() override;

    void setMood(Mood mood);
    void setBossEffect(BossEffect effect);
    void setPaused(bool paused);

protected:
    void drawBackground(QPainter *painter, const QRectF &rect) override;
    void drawForeground(QPainter *painter, const QRectF &rect) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    bool ensureProgram();
    bool ensureCrtProgram();
    void renderBackgroundNative();
    void renderCrtNative();
    void sendColourUniform(const char *name, const QColor &c);
    void updateTargets();
    void easeVisuals(double dt);

    QColor baseA() const;
    QColor baseB() const;
    QColor accent() const;
    double targetContrast() const;
    double targetSpin() const;

    Mood mMood = Mood::Default;
    BossEffect mBossEffect = BossEffect::None;
    QTimer mTimer;
    QElapsedTimer mClock;
    double mLastTick = 0.0;
    double mTime = 0.0;
    double mSpinTime = 0.0;
    std::unique_ptr<QOpenGLShaderProgram> mProgram;
    std::unique_ptr<QOpenGLShaderProgram> mCrtProgram;
    QSize mSceneTextureSize;
    unsigned int mSceneTexture = 0;
    bool mFunctionsReady = false;
    bool mProgramReady = false;
    bool mCrtProgramReady = false;

    QColor mCurrentA;
    QColor mCurrentB;
    QColor mCurrentC;
    QColor mTargetA;
    QColor mTargetB;
    QColor mTargetC;
    double mCurrentContrast = 1.0;
    double mTargetContrast = 1.0;
    double mCurrentSpin = 0.0;
    double mTargetSpin = 0.0;
    bool mVisualsReady = false;
};

#endif // BALATROGRAPHICSVIEW_H
