#ifndef GPUCARDOVERLAY_H
#define GPUCARDOVERLAY_H

#include <QOpenGLWidget>
#include <QOpenGLFunctions>
#include <QOpenGLShaderProgram>
#include <QElapsedTimer>
#include <QTimer>
#include <QHash>
#include <QPointer>
#include <QPixmap>
#include <memory>

class QGraphicsScene;
class QGraphicsView;
class QGraphicsItem;

class GpuCardOverlay : public QOpenGLWidget, protected QOpenGLFunctions
{
    Q_OBJECT
public:
    explicit GpuCardOverlay(QGraphicsView *view, QGraphicsScene *scene, QWidget *parent = nullptr);
    ~GpuCardOverlay() override;

    void setViewAndScene(QGraphicsView *view, QGraphicsScene *scene);

protected:
    void initializeGL() override;
    void resizeGL(int w, int h) override;
    void paintGL() override;

private:
    enum EffectKind {
        EffectFoil = 1,
        EffectHolo = 2,
        EffectPolychrome = 3,
        EffectNegative = 4,
        EffectHologram = 5
    };

    struct DrawCall {
        QPixmap pixmap;
        QPointF p0;
        QPointF p1;
        QPointF p2;
        QPointF p3;
        int effect = 0;
        qreal z = 0.0;
        qreal opacity = 1.0;
    };

    void collectDrawCalls(QVector<DrawCall> &out) const;
    void appendItemQuad(QVector<DrawCall> &out, const QGraphicsItem *item,
                        const QPixmap &pixmap, int effect, qreal zBias = 0.0,
                        qreal opacity = 1.0) const;
    unsigned int textureForPixmap(const QPixmap &pixmap);
    void releaseTextures();
    void drawCall(const DrawCall &call, float time);

    QPointer<QGraphicsView> mView;
    QPointer<QGraphicsScene> mScene;
    QTimer mTimer;
    QElapsedTimer mClock;
    std::unique_ptr<QOpenGLShaderProgram> mProgram;
    bool mProgramReady = false;
    QHash<qint64, unsigned int> mTextures;
};

#endif // GPUCARDOVERLAY_H
