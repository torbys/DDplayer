#include "filtersettings.h"
#include "ui_filtersettings.h"
#include <QCheckBox>
#include <QLabel>
#include <QHBoxLayout>
#include <QSettings>
#include "superresolution.h"

static constexpr const char* SETTINGS_GROUP = "FilterSettings";
static constexpr const char* SETTINGS_SR_ENABLED = "srEnabled";
static constexpr const char* SETTINGS_SR_MODEL = "srModel";

FilterSettings::FilterSettings(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::FilterSettings)
{
    ui->setupUi(this);
    this->setWindowTitle("滤镜设置");

    initSliders();
    initCheckBoxGroup();
    initSuperResolution();
    loadSettings();
}

FilterSettings::~FilterSettings()
{
    saveSettings();
    delete ui;
}

void FilterSettings::loadSettings()
{
    QSettings settings("DDplayer", "FilterSettings");
    settings.beginGroup(SETTINGS_GROUP);

    // 恢复超分启用状态
    bool srEnabled = settings.value(SETTINGS_SR_ENABLED, false).toBool();
    int srModel = settings.value(SETTINGS_SR_MODEL, 0).toInt();
    settings.endGroup();

    // 应用到 UI
    QCheckBox *srCB = findChild<QCheckBox*>("srCheckBox");
    QComboBox *srCombo = findChild<QComboBox*>("srModelCombo");

    if (srCB) {
        srCB->setChecked(srEnabled);
    }
    if (srCombo) {
        srCombo->setCurrentIndex(qBound(0, srModel, srCombo->count() - 1));
        srCombo->setEnabled(srEnabled);
    }

    // 发送信号让 VideoGLWidget 应用状态
    emit srEnabledChanged(srEnabled);
    if (srEnabled && srModel >= 0) {
        emit srModelChanged(srModel);
    }
}

void FilterSettings::saveSettings()
{
    QSettings settings("DDplayer", "FilterSettings");
    settings.beginGroup(SETTINGS_GROUP);

    QCheckBox *srCB = findChild<QCheckBox*>("srCheckBox");
    QComboBox *srCombo = findChild<QComboBox*>("srModelCombo");

    if (srCB) {
        settings.setValue(SETTINGS_SR_ENABLED, srCB->isChecked());
    }
    if (srCombo) {
        settings.setValue(SETTINGS_SR_MODEL, srCombo->currentIndex());
    }

    settings.endGroup();
    settings.sync();
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
    filterGroup_->setExclusive(false);  // 非互斥，允许全部取消选中

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

// ============================================================
// AI 超分 UI 初始化
// ============================================================

void FilterSettings::initSuperResolution()
{
    // 在滤镜选择区域（widget_2）的布局末尾添加超分控制区
    QWidget *srWidget = new QWidget(this);
    QHBoxLayout *srLayout = new QHBoxLayout(srWidget);
    srLayout->setContentsMargins(4, 8, 4, 4);

    QLabel *srLabel = new QLabel("AI 超分:", srWidget);
    QFont labelFont = srLabel->font();
    labelFont.setBold(true);
    srLabel->setFont(labelFont);

    QCheckBox *srCheckBox = new QCheckBox("启用", srWidget);
    srCheckBox->setObjectName("srCheckBox");
    srCheckBox->setToolTip("开启 AI 超分辨率，将视频画质提升至 2x 分辨率\n需要 NVIDIA GPU 支持 (CUDA)");

    QLabel *modelLabel = new QLabel("模型:", srWidget);

    QComboBox *modelCombo = new QComboBox(srWidget);
    modelCombo->setObjectName("srModelCombo");
    modelCombo->setMinimumWidth(180);
    // 填充可用模型列表
    for (int i = 0; i < MODEL_COUNT; i++) {
        modelCombo->addItem(QString::fromStdString(AVAILABLE_MODELS[i].name));
    }
    modelCombo->setCurrentIndex(0);  // 默认 HFA2kCompact
    modelCombo->setEnabled(false);   // 默认禁用，只有启用超分后才可选

    QLabel *infoLabel = new QLabel("", srWidget);
    infoLabel->setObjectName("srInfoLabel");
    infoLabel->setStyleSheet("color: #888888; font-size: 11px;");
    infoLabel->setText("(GPU 推理)");

    srLayout->addWidget(srLabel);
    srLayout->addWidget(srCheckBox);
    srLayout->addSpacing(16);
    srLayout->addWidget(modelLabel);
    srLayout->addWidget(modelCombo);
    srLayout->addWidget(infoLabel);
    srLayout->addStretch();

    // 插入到 widget_2 的 gridLayout 中（滤镜选择区域下方）
    QGridLayout *grid = qobject_cast<QGridLayout*>(ui->widget_2->layout());
    if (grid) {
        int row = grid->rowCount();
        grid->addWidget(srWidget, row, 0, 1, 3);
    }

    // 连接信号
    connect(srCheckBox, &QCheckBox::toggled, this, &FilterSettings::on_srCheckBox_toggled);
    connect(modelCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &FilterSettings::on_srModelCombo_currentIndexChanged);

    // 保存指针供槽函数使用（通过 findChild 获取）
    modelCombo->setProperty("srModelCombo", true);
}

void FilterSettings::on_srCheckBox_toggled(bool checked)
{
    qDebug() << "SR: 超分" << (checked ? "已启用" : "已禁用");

    // 启用时解锁模型选择框，禁用时锁定
    QComboBox *combo = findChild<QComboBox*>("srModelCombo");
    if (combo) {
        combo->setEnabled(checked);
    }

    emit srEnabledChanged(checked);
}

void FilterSettings::on_srModelCombo_currentIndexChanged(int index)
{
    if (index < 0) return;

    qDebug() << "SR: 切换模型到索引" << index;
    emit srModelChanged(index);
}

// ============================================================
// Slider 槽函数
// ============================================================

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
    QCheckBox *cb = qobject_cast<QCheckBox*>(button);
    if (!cb) return;

    if (!cb->isChecked()) {
        // 如果点击后变为未选中（用户取消选择），发送无滤镜
        qDebug() << "滤镜: 已关闭";
        emit filterTypeChanged(FILTER_NONE);
        return;
    }

    // 选中当前项时，取消其他所有项（手动实现互斥）
    for (QAbstractButton *btn : filterGroup_->buttons()) {
        if (btn != button) {
            QCheckBox *other = qobject_cast<QCheckBox*>(btn);
            if (other) {
                other->setChecked(false);
            }
        }
    }

    qDebug() << "滤镜切换到:" << type;
    emit filterTypeChanged(type);
}
