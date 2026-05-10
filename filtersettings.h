#ifndef FILTERSETTINGS_H
#define FILTERSETTINGS_H

#include <QWidget>
#include <QButtonGroup>

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

private slots:
    void on_lightSlider_valueChanged(int value);
    void on_contrastSlider_valueChanged(int value);
    void on_saturationSlider_valueChanged(int value);
    void on_blurSlider_valueChanged(int value);
    void onFilterGroupClicked(QAbstractButton *button);

private:
    void initSliders();
    void initCheckBoxGroup();

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
