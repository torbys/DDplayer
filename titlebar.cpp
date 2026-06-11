#include "titlebar.h"
#include "ui_titlebar.h"
#include <QMouseEvent>


TitleBar::TitleBar(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::TitleBar)
{
    ui->setupUi(this);
}

void TitleBar::initTitle(QString name)
{
    ui->VideoTitle->setText(name);
}

TitleBar::~TitleBar()
{
    delete ui;
}

// 按钮点击槽函数（假设按钮名称为 MinBtn, MaxBtn, CloseBtn）
void TitleBar::on_MinBtn_clicked()
{
    emit SigMinimize();
}

void TitleBar::on_MaxBtn_clicked()
{
    emit SigMaximize();
}

void TitleBar::on_CloseBtn_clicked()
{
    emit SigClose();
}

// ========== 拖动实现 ==========
void TitleBar::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        isDragging = true;

        // 关键修正：记录鼠标和窗口的初始屏幕坐标
        dragStartMousePos = event->globalPos();

        // 使用 window()->pos() 获取顶层窗口（MainWindow）的屏幕坐标
        // 或者使用 parentWidget()->mapToGlobal(QPoint(0,0)) 如果 parent 就是 MainWindow
        dragStartWindowPos = parentWidget()->window()->pos();

        event->accept();
    } else {
        QWidget::mousePressEvent(event);
    }
}

void TitleBar::mouseMoveEvent(QMouseEvent *event)
{
    if (isDragging && (event->buttons() & Qt::LeftButton)) {
        // 计算鼠标移动了多少像素
        QPoint delta = event->globalPos() - dragStartMousePos;

        // 新位置 = 窗口原位置 + 鼠标移动量
        QPoint newPos = dragStartWindowPos + delta;

        emit SigDragMove(newPos);
        event->accept();
    } else {
        QWidget::mouseMoveEvent(event);
    }
}

void TitleBar::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        isDragging = false;
        event->accept();
    } else {
        QWidget::mouseReleaseEvent(event);
    }
}

// 双击标题栏最大化/还原
void TitleBar::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        emit SigMaximize();
        event->accept();
    } else {
        QWidget::mouseDoubleClickEvent(event);
    }
}
