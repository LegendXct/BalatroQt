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
    void setData(int blindChipRow, double targetScore, int blindReward,
                 int handsLeft, int handBonus,
                 int interest, int extraBonus = 0, int totalPayout = -1);
    void showFromBottom(const QRect &finalGeometry);
    void hideToBottom(std::function<void()> after = nullptr);
    // 让面板水平居中时要避开右下角牌堆按钮的宽度（dp 像素）。
    void setRightReserve(int px) { mRightReserve = px; relayoutPanel(); }
protected:
    void resizeEvent(QResizeEvent *e) override;
signals:
    void nextClicked();
private:
    QFont mCNFont, mPixelFont;
    QPushButton *mCashOutBtn;
    class AnimatedBlindChip *mBlindChip;
    QLabel *mTargetLbl;
    QLabel *mBlindRewardSym;
    QLabel *mHandsNumLbl, *mHandsRewardSym;
    QLabel *mInterestNumLbl, *mInterestRewardSym;
    QWidget *mExtraRow = nullptr;
    QLabel *mExtraNumLbl = nullptr;
    QLabel *mExtraRewardSym = nullptr;
    QWidget *mPanel = nullptr;
    int mRightReserve = 0;        // 右侧"牌堆"区域要避开的像素宽度
    void buildUi();
    void relayoutPanel();
};

#endif
