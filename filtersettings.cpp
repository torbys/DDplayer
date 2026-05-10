#include "filtersettings.h"
#include "ui_filtersettings.h"
#include <QCheckBox>

FilterSettings::FilterSettings(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::FilterSettings)
{
    ui->setupUi(this);
    this->setWindowTitle("滤镜设置");

    initSliders();
    initCheckBoxGroup();
}

FilterSettings::~FilterSettings()
{
    delete ui;
}

void FilterSettings::initSliders()
{
    ui->lightSlider->setRange(-100, 100);
    ui->lightSlider->setValue(0);

    ui->contrastSlider->setRange(-100, 100);
    ui->contrastSlider->setValue(0);

    ui->saturationSlider->setRange(-100, 100);
    ui->saturationSlider->setValue(0);

    ui->blurSlider->setRange(0, 100);
    ui->blurSlider->setValue(0);

    connect(ui->lightSlider, &QSlider::valueChanged, this, &FilterSettings::on_lightSlider_valueChanged);
    connect(ui->contrastSlider, &QSlider::valueChanged, this, &FilterSettings::on_contrastSlider_valueChanged);
    connect(ui->saturationSlider, &QSlider::valueChanged, this, &FilterSettings::on_saturationSlider_valueChanged);
    connect(ui->blurSlider, &QSlider::valueChanged, this, &FilterSettings::on_blurSlider_valueChanged);
}

void FilterSettings::initCheckBoxGroup()
{
    filterGroup_ = new QButtonGroup(this);
    filterGroup_->setExclusive(true);

    filterGroup_->addButton(ui->filmCB, FILTER_FILM);
    filterGroup_->addButton(ui->blurCB, FILTER_BLUR);
    filterGroup_->addButton(ui->cartoonCB, FILTER_CARTOON);
    filterGroup_->addButton(ui->bwCB, FILTER_BW);
    filterGroup_->addButton(ui->sepiaCB, FILTER_SEPIA);
    filterGroup_->addButton(ui->edgeCB, FILTER_EDGE);
    filterGroup_->addButton(ui->sharpenCB, FILTER_SHARPEN);
    filterGroup_->addButton(ui->oilCB, FILTER_OIL);
    filterGroup_->addButton(ui->waterCB, FILTER_WATERCOLOR);

    connect(filterGroup_, &QButtonGroup::buttonClicked, this, &FilterSettings::onFilterGroupClicked);
}

void FilterSettings::on_lightSlider_valueChanged(int value)
{
    float fValue = value / 100.0f;
    ui->lightValue->setText(QString::number(fValue, 'f', 1));
    emit filterParamsChanged(fValue,
                             ui->contrastSlider->value() / 100.0f,
                             ui->saturationSlider->value() / 100.0f,
                             ui->blurSlider->value() / 100.0f);
}

void FilterSettings::on_contrastSlider_valueChanged(int value)
{
    float fValue = value / 100.0f;
    ui->contrastValue->setText(QString::number(fValue, 'f', 1));
    emit filterParamsChanged(ui->lightSlider->value() / 100.0f,
                             fValue,
                             ui->saturationSlider->value() / 100.0f,
                             ui->blurSlider->value() / 100.0f);
}

void FilterSettings::on_saturationSlider_valueChanged(int value)
{
    float fValue = value / 100.0f;
    ui->saturationValue->setText(QString::number(fValue, 'f', 1));
    emit filterParamsChanged(ui->lightSlider->value() / 100.0f,
                             ui->contrastSlider->value() / 100.0f,
                             fValue,
                             ui->blurSlider->value() / 100.0f);
}

void FilterSettings::on_blurSlider_valueChanged(int value)
{
    float fValue = value / 100.0f;
    ui->blurValue->setText(QString::number(fValue, 'f', 1));
    emit filterParamsChanged(ui->lightSlider->value() / 100.0f,
                             ui->contrastSlider->value() / 100.0f,
                             ui->saturationSlider->value() / 100.0f,
                             fValue);
}

void FilterSettings::onFilterGroupClicked(QAbstractButton *button)
{
    int type = filterGroup_->id(button);
    qDebug() << "滤镜切换到:" << type;
    emit filterTypeChanged(type);
}
