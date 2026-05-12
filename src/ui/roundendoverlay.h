#ifndef ROUNDENDOVERLAY_H
#define ROUNDENDOVERLAY_H

#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QFont>
#include <QRect>
#include <functional>

class RoundEndOverlay : public QWidget
{
    Q_OBJECT
public:
    RoundEndOverlay(const QFont &cnFont, const QFont &pixelFont, QWidget *parent = nullptr);
    void setData(int blindChipRow, int targetScore, int blindReward,
                 int handsLeft, int handBonus,
                 int interest);
    void showFromBottom(const QRect &finalGeometry);
    void hideToBottom(std::function<void()> after = nullptr);
signals:
    void nextClicked();
private:
    QFont mCNFont, mPixelFont;
    QPushButton *mCashOutBtn;
    QLabel *mBlindChip;
    QLabel *mTargetLbl;
    QLabel *mBlindRewardSym;
    QLabel *mHandsNumLbl, *mHandsRewardSym;
    QLabel *mInterestNumLbl, *mInterestRewardSym;
    void buildUi();
};

#endif
