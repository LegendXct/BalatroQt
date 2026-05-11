#ifndef SHOPSIGNWIDGET_H
#define SHOPSIGNWIDGET_H

#include <QWidget>
#include <QPixmap>
#include <QTimer>

class ShopSignWidget : public QWidget
{
    Q_OBJECT
public:
    explicit ShopSignWidget(QWidget *parent = nullptr);
    ~ShopSignWidget();
    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent *e) override;

private:
    QPixmap mSheet;        // 整张 sprite sheet
    int     mFrameCount = 4;
    int     mFrameW = 0;
    int     mFrameH = 0;
    int     mCurrent = 0;
    QTimer  mTimer;
};

#endif
