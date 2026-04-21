#include "VolSurfaceWidget.h"
#include "../infra/AppSettings.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QDoubleSpinBox>
#include <QPushButton>
#include <QLabel>
#include <QSplitter>
#include <QLinearGradient>

//using namespace QtDataVisualization;

VolSurfaceWidget::VolSurfaceWidget(AsyncWorker* worker, QWidget* parent)
    : QWidget(parent), m_worker(worker)
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(12,12,12,12);
    root->setSpacing(10);

    root->addWidget(buildParamsPanel());
    root->addWidget(build3DSurface(), 1);

    connect(m_worker, &AsyncWorker::volSurfaceFinished,
            this, &VolSurfaceWidget::onVolSurfaceFinished);
}

VolSurfaceWidget::~VolSurfaceWidget()
{
    // Q3DSurface 需要手動刪除 container
    delete m_surface;
}

// ─────────────────────────────────────────────────────────────────────────────
QWidget* VolSurfaceWidget::buildParamsPanel()
{
    auto* box = new QGroupBox("Parameters", this);
    auto* row = new QHBoxLayout(box);
    row->setSpacing(12);

    auto addSpin = [&](const QString& lbl, QDoubleSpinBox* spin) {
        row->addWidget(new QLabel(lbl, box));
        row->addWidget(spin);
    };

    m_spot = new QDoubleSpinBox(box);
    m_spot->setRange(1, 99999); m_spot->setValue(540); m_spot->setDecimals(2);
    m_spot->setMinimumWidth(88);
    addSpin("Spot:", m_spot);

    m_rf = new QDoubleSpinBox(box);
    m_rf->setRange(0,0.5); m_rf->setValue(0.05); m_rf->setDecimals(3);
    m_rf->setSingleStep(0.005); m_rf->setMinimumWidth(72);
    addSpin("Risk-free r:", m_rf);

    m_vol = new QDoubleSpinBox(box);
    m_vol->setRange(0.01,5); m_vol->setValue(0.20); m_vol->setDecimals(3);
    m_vol->setSingleStep(0.01); m_vol->setMinimumWidth(72);
    addSpin("ATM vol σ:", m_vol);

    row->addStretch();

    auto* hint = new QLabel(
        "Surface uses BSM skew model: IV(K,T) = σ - 0.10×(K/S-1) + 0.02×√T", box);
    hint->setStyleSheet("font-size:10px;color:rgba(150,150,150,0.6);");
    row->addWidget(hint);

    row->addStretch();

    m_calcBtn = new QPushButton("Generate surface", box);
    m_calcBtn->setMinimumHeight(34); m_calcBtn->setMinimumWidth(140);
    m_calcBtn->setStyleSheet(
        "QPushButton{background:#7c3aed;color:white;border-radius:6px;font-weight:500;}"
        "QPushButton:hover{background:#6d28d9;}"
        "QPushButton:disabled{background:#475569;}");
    connect(m_calcBtn, &QPushButton::clicked, this, &VolSurfaceWidget::onCalculate);
    row->addWidget(m_calcBtn);

    return box;
}

// ─────────────────────────────────────────────────────────────────────────────
QWidget* VolSurfaceWidget::build3DSurface()
{
    auto* box = new QGroupBox("Implied Volatility Surface (Strike × Maturity)", this);
    auto* lay = new QVBoxLayout(box);

    // 建立 Q3DSurface
    m_surface = new Q3DSurface;

    // 軸設定
    m_surface->axisX()->setTitle("Strike");
    m_surface->axisY()->setTitle("Implied Vol (%)");
    m_surface->axisZ()->setTitle("Maturity (years)");
    m_surface->axisX()->setTitleVisible(true);
    m_surface->axisY()->setTitleVisible(true);
    m_surface->axisZ()->setTitleVisible(true);
    m_surface->axisX()->setLabelFormat("%.0f");
    m_surface->axisY()->setLabelFormat("%.1f%%");
    m_surface->axisZ()->setLabelFormat("%.1fY");

    // 外觀
    m_surface->setHorizontalAspectRatio(1.5);
    m_surface->setShadowQuality(QAbstract3DGraph::ShadowQualityMedium);
  //  m_surface->scene()->activeCamera()->setCameraPreset(
    //    Q3DCamera::CameraPresetIsometricLeftHigh);
    m_surface->scene()->activeCamera()->setXRotation(30.0f);
    m_surface->scene()->activeCamera()->setYRotation(25.0f);
    m_surface->scene()->activeCamera()->setZoomLevel(110.0f);

    // 顏色映射（低 IV = 藍，高 IV = 紅）
    QLinearGradient gradient;
    gradient.setColorAt(0.0, QColor("#1e40af"));    // 深藍
    gradient.setColorAt(0.3, QColor("#06b6d4"));    // 青
    gradient.setColorAt(0.6, QColor("#22c55e"));    // 綠
    gradient.setColorAt(0.8, QColor("#f59e0b"));    // 橘
    gradient.setColorAt(1.0, QColor("#ef4444"));    // 紅

    // Series
    m_proxy  = new QSurfaceDataProxy;
    m_series = new QSurface3DSeries(m_proxy);
    m_series->setDrawMode(QSurface3DSeries::DrawSurface);
    m_series->setFlatShadingEnabled(false);
    m_series->setColorStyle(Q3DTheme::ColorStyleRangeGradient);
    m_series->setBaseGradient(gradient);
    m_series->setSingleHighlightGradient(gradient);
    m_surface->addSeries(m_series);

    // Theme（深色）
    QFont labelFont("Arial", 9);
    m_surface->activeTheme()->setType(Q3DTheme::ThemeStoneMoss);
    m_surface->activeTheme()->setBackgroundEnabled(false);
    m_surface->activeTheme()->setFont(labelFont);
    m_surface->activeTheme()->setLabelBackgroundEnabled(false);
    m_surface->activeTheme()->setLabelBorderEnabled(false);

    // 包裝成 QWidget
    m_container = QWidget::createWindowContainer(m_surface, box);
    m_container->setMinimumHeight(400);
    m_container->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    auto* hint = new QLabel(
        "滑鼠拖動 = 旋轉  ·  滾輪 = 縮放  ·  右鍵 = 平移", box);
    hint->setStyleSheet("font-size:10px;color:rgba(150,150,150,0.6);");
    hint->setAlignment(Qt::AlignCenter);

    lay->addWidget(m_container, 1);
    lay->addWidget(hint);
    return box;
}

// ─────────────────────────────────────────────────────────────────────────────
void VolSurfaceWidget::onCalculate()
{
    m_calcBtn->setEnabled(false);
    m_calcBtn->setText("Computing...");

    OptionChainRequest req;
    req.spot          = m_spot->value();
    req.riskFree      = m_rf->value();
    req.baseVol       = m_vol->value();
    req.dividendYield = 0.0;
    req.strikeMin     = 0.70;
    req.strikeMax     = 1.30;

    m_worker->submitVolSurface(req);
    emit statusMessage(
        QString("Computing vol surface: Spot=%1, σ=%2...")
        .arg(req.spot,0,'f',2).arg(req.baseVol,0,'f',3));
}

// ─────────────────────────────────────────────────────────────────────────────
void VolSurfaceWidget::onVolSurfaceFinished(const VolSurfaceResult& result)
{
    m_calcBtn->setEnabled(true);
    m_calcBtn->setText("Generate surface");

    if (!result.success) {
        emit statusMessage("Vol surface error: " + result.errorMsg);
        return;
    }

    const int nK = result.strikes.size();
    const int nT = result.maturities.size();

    // 建立 QSurfaceDataArray
    auto* dataArray = new QSurfaceDataArray;
    dataArray->reserve(nT);

    double volMin = 999, volMax = -999;
    for (double v : result.impliedVols) {
        volMin = qMin(volMin, v); volMax = qMax(volMax, v);
    }

    for (int ti = 0; ti < nT; ++ti) {
        auto* row = new QSurfaceDataRow(nK);
        for (int ki = 0; ki < nK; ++ki) {
            double K   = result.strikes[ki];
            double T   = result.maturities[ti];
            double vol = result.impliedVols[ti * nK + ki];
            (*row)[ki] = QSurfaceDataItem(QVector3D(
                static_cast<float>(K),
                static_cast<float>(vol),
                static_cast<float>(T)));
        }
        dataArray->append(row);
    }

    m_proxy->resetArray(dataArray);

    // 更新軸範圍
    m_surface->axisX()->setRange(
        static_cast<float>(result.strikes.first()),
        static_cast<float>(result.strikes.last()));
    m_surface->axisY()->setRange(
        static_cast<float>(volMin * 0.95f),
        static_cast<float>(volMax * 1.05f));
    m_surface->axisZ()->setRange(
        static_cast<float>(result.maturities.first()),
        static_cast<float>(result.maturities.last()));

    emit statusMessage(
        QString("Vol surface: %1 strikes × %2 maturities | IV range: %3% - %4%")
        .arg(nK).arg(nT)
        .arg(volMin,0,'f',2).arg(volMax,0,'f',2));
}
