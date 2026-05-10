#ifndef TITLEBAR_H
#define TITLEBAR_H

#include <QWidget>
#include <QString>

namespace Ui {
class TitleBar;
}

class TitleBar : public QWidget
{
    Q_OBJECT

public:
    explicit TitleBar(QWidget *parent = nullptr);
    void initTitle(QString name);
    ~TitleBar();

signals:
    // 窗口控制信号
    void SigMinimize();      // 最小化
    void SigMaximize();      // 最大化/还原切换
    void SigClose();         // 关闭

    // 拖动相关（可选，也可直接在MainWindow处理）
    void SigDragStart(const QPoint &pos);
    void SigDragMove(const QPoint &newPos);  // 发送新的窗口全局坐标
    void SigDragEnd();

private slots:
    void on_MinBtn_clicked();
    void on_MaxBtn_clicked();
    void on_CloseBtn_clicked();

private:
    Ui::TitleBar *ui;

    // 用于拖动
    bool dragging = false;
    QPoint dragStartPos;

    QPoint dragStartMousePos;    // 鼠标按下时的全局坐标
    QPoint dragStartWindowPos;   // 窗口按下时的全局坐标（关键修正）

    bool isDragging;

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override; // 双击最大化
};

#endif // TITLEBAR_H
