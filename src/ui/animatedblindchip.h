#ifndef ANIMATEDBLINDCHIP_H
#define ANIMATEDBLINDCHIP_H

#include <QLabel>
#include <QPixmap>

// 复现原版 game.lua:1011 的 animation_atli.blind_chips：
// 把 resources/images/BlindChips.png 当作 21 帧×31 行 (68×68 每帧) 的精灵表，
// 每个盲注对应一行，按 ~12.5 fps 循环播放所有列。
class AnimatedBlindChip : public QLabel
{
    Q_OBJECT
public:
    explicit AnimatedBlindChip(QWidget *parent = nullptr);
    void setBlindRow(int row);              // 设置使用 BlindChips.png 的哪一行
    void setDisplaySize(int sidePx);        // 显示尺寸（保持等比，单位像素）
    void setFrameIntervalMs(int ms);
    int  blindRow() const { return mRow; }
    // 击败盲注后播放破碎/溶解动画，结束后隐藏自己。
    void startDissolve(int durationMs = 700);

protected:
    void showEvent(QShowEvent *e) override;
    void hideEvent(QHideEvent *e) override;

private:
    static QPixmap *sSheet;                 // 全局共享 sheet，避免每个 chip 都重复加载
    static int      sFrameCount;            // 21
    static int      sFrameSize;             // 68
    int             mRow = 0;
    int             mFrame = 0;
    int             mDisplaySide = 50;
    int             mIntervalMs = 80;
    class QTimer   *mTimer = nullptr;

    void refreshCurrentFrame();
    void ensureSheetLoaded();
    bool mDissolving = false;
};

#endif // ANIMATEDBLINDCHIP_H
