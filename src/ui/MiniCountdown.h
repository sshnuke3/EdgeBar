#ifndef MINICOUNTDOWN_H
#define MINICOUNTDOWN_H

#include <QWidget>
#include <QPainter>

/**
 * @brief 番茄钟迷你倒计时浮窗
 *
 * 面板隐藏时，在屏幕边缘显示一个 24px 宽的迷你倒计时数字。
 * 点击浮窗触发主面板滑入。
 */
class MiniCountdown : public QWidget
{
    Q_OBJECT
public:
    explicit MiniCountdown(QWidget *parent = nullptr);

    /// 设置倒计时秒数
    void setRemaining(int seconds);

    /// 设置是否为休息模式（绿色/红色）
    void setBreakMode(bool isBreak);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;

signals:
    void clicked();

private:
    int  m_remaining = 0;
    bool m_isBreak = false;
};

#endif // MINICOUNTDOWN_H
