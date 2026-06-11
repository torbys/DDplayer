#ifndef FILTERSETTINGS_H
#define FILTERSETTINGS_H

#include <QWidget>
#include <QButtonGroup>
#include <QComboBox>

namespace Ui {
class FilterSettings;
}

class FilterSettings : public QWidget
{
    Q_OBJECT

public:
    explicit FilterSettings(QWidget *parent = nullptr);
    ~FilterSettings();

signals:
    void filterParamsChanged(float brightness, float contrast, float saturation, float blur);
    void filterTypeChanged(int type);

    // AI 超分信号
    void srEnabledChanged(bool enabled);
    void srModelChanged(int modelIndex);

private slots:
    void on_lightSlider_valueChanged(int value);
    void on_contrastSlider_valueChanged(int value);
    void on_saturationSlider_valueChanged(int value);
    void on_blurSlider_valueChanged(int value);
    void onFilterGroupClicked(QAbstractButton *button);

    // 超分相关槽
    void on_srCheckBox_toggled(bool checked);
    void on_srModelCombo_currentIndexChanged(int index);

private:
    void initSliders();
    void initCheckBoxGroup();
    void initSuperResolution();  // 初始化超分 UI
    void loadSettings();         // 从 QSettings 恢复状态
    void saveSettings();         // 保存状态到 QSettings

    Ui::FilterSettings *ui;
    QButtonGroup *filterGroup_;

    static constexpr int FILTER_NONE = 0;
    static constexpr int FILTER_FILM = 1;
    static constexpr int FILTER_BLUR = 2;
    static constexpr int FILTER_CARTOON = 3;
    static constexpr int FILTER_BW = 4;
    static constexpr int FILTER_SEPIA = 5;
    static constexpr int FILTER_EDGE = 6;
    static constexpr int FILTER_SHARPEN = 7;
    static constexpr int FILTER_OIL = 8;
    static constexpr int FILTER_WATERCOLOR = 9;
};

#endif // FILTERSETTINGS_H
