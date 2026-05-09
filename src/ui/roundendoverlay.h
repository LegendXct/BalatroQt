#ifndef ROUNDENDOVERLAY_H
#define ROUNDENDOVERLAY_H

#include <QWidget>
#include <QLabel>
#include <QPushButton>

class RoundEndOverlay : public QWidget
{
    Q_OBJECT
public:
    explicit RoundEndOverlay(const QFont &cnFont, const QFont &pixelFont, QWidget *parent = nullptr);
    void setData(const QString &blindName, int score, int target, int blindReward, int handBonus, int interest);
signals:
    void nextClicked();
protected:
    void resizeEvent(QResizeEvent *event) override;

private:
    QFont mCNFont;
    QFont mPixelFont;

    QWidget    *mPanel          = nullptr;  // 中央面板
    QLabel     *mLblTitle       = nullptr;  // "胜利！"
    QLabel     *mLblBlind       = nullptr;  // "击败 小盲注"
    QLabel     *mLblScoreLine   = nullptr;
    QLabel     *mLblTargetLine  = nullptr;
    QLabel     *mLblBlindReward = nullptr;
    QLabel     *mLblHandBonus   = nullptr;
    QLabel     *mLblInterest    = nullptr;
    QLabel     *mLblTotal       = nullptr;
    QPushButton *mBtnNext       = nullptr;

    void buildUi();
    void layoutPanel();
};

#endif // ROUNDENDOVERLAY_H
