#include "YieldCurveWidget.h"
#include "../infra/AppSettings.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QDoubleSpinBox>
#include <QPushButton>
#include <QLabel>
#include <QTabWidget>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QHeaderView>
#include <QComboBox>
#include <QSplitter>
#include <QFileDialog>
#include <QTextStream>
#include <QMessageBox>

#include <QtCharts/QChart>
#include <QtCharts/QChartView>
#include <QtCharts/QLineSeries>
#include <QtCharts/QValueAxis>
#include <QtCharts/QLegend>

// ── 預設利率場景 ──────────────────────────────────────────────────────────────
struct Preset {
    QString name;
    QVector<double> rates;  // 對應 3M/6M/1Y/2Y/5Y/10Y/30Y
};

static const QVector<Preset> kPresets = {
    { "Current (~Apr 2025)",  {0.0527,0.0512,0.0489,0.0463,0.0432,0.0436,0.0473} },
    { "Flat 5%",              {0.0500,0.0500,0.0500,0.0500,0.0500,0.0500,0.0500} },
    { "Normal (upward)",      {0.0300,0.0330,0.0370,0.0420,0.0470,0.0490,0.0510} },
    { "Inverted (2022-style)",{0.0540,0.0535,0.0510,0.0470,0.0400,0.0370,0.0360} },
    { "Custom",               {} },
};

// ── 共用圖表建構 ──────────────────────────────────────────────────────────────
static QChart* makeChart(bool showLegend = true)
{
    auto* c = new QChart;
    c->legend()->setVisible(showLegend);
    c->legend()->setAlignment(Qt::AlignBottom);
    c->legend()->setLabelColor(QColor(180,180,180));
    c->setMargins(QMargins(4,4,4,4));
    c->setBackgroundVisible(false);
    return c;
}

static QValueAxis* makeAxis(const QString& title, const QString& fmt = "%.2f",
                             int ticks = 7)
{
    auto* a = new QValueAxis;
    a->setTitleText(title);
    a->setLabelFormat(fmt);
    a->setTickCount(ticks);
    a->setLabelsColor(QColor(180,180,180));
    a->setTitleBrush(QColor(180,180,180));
    QPen gp(QColor(55,55,55)); gp.setWidth(1);
    a->setGridLinePen(gp);
    return a;
}

// ═════════════════════════════════════════════════════════════════════════════
YieldCurveWidget::YieldCurveWidget(AsyncWorker* worker, QWidget* parent)
    : QWidget(parent), m_worker(worker)
{
    // 定義 7 個標準 tenor
    m_tenors = {
        {3,   "3M",  nullptr},
        {6,   "6M",  nullptr},
        {12,  "1Y",  nullptr},
        {24,  "2Y",  nullptr},
        {60,  "5Y",  nullptr},
        {120, "10Y", nullptr},
        {360, "30Y", nullptr},
    };

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(12,12,12,12);
    root->setSpacing(10);

    root->addWidget(buildInputPanel());

    auto* splitter = new QSplitter(Qt::Horizontal, this);
    splitter->addWidget(buildChartTabs());
    splitter->addWidget(buildDataTable());
    splitter->setStretchFactor(0, 3);
    splitter->setStretchFactor(1, 1);
    root->addWidget(splitter, 1);

    connect(m_worker, &AsyncWorker::yieldCurveFinished,
            this, &YieldCurveWidget::onYieldCurveFinished);

    // 套用預設場景並立即計算
    onPresetChanged(0);
    onCalculate();
}

// ─────────────────────────────────────────────────────────────────────────────
QWidget* YieldCurveWidget::buildInputPanel()
{
    auto* box  = new QGroupBox("Treasury Rates Input", this);
    auto* vlay = new QVBoxLayout(box);

    // 預設選單
    auto* topRow = new QHBoxLayout;
    topRow->addWidget(new QLabel("Preset:", box));
    m_presetCombo = new QComboBox(box);
    for (const auto& p : kPresets) m_presetCombo->addItem(p.name);
    m_presetCombo->setMinimumWidth(220);
    topRow->addWidget(m_presetCombo);
    topRow->addStretch();

    m_exportBtn = new QPushButton("Export CSV", box);
    m_exportBtn->setMinimumHeight(32); m_exportBtn->setMinimumWidth(110);
    m_exportBtn->setEnabled(false);  // 計算前不可用
    m_exportBtn->setStyleSheet(
        "QPushButton{border:0.5px solid palette(mid);border-radius:6px;padding:0 12px;}"
        "QPushButton:hover{background:palette(midlight);}"
        "QPushButton:disabled{color:palette(mid);}");
    topRow->addWidget(m_exportBtn);
    connect(m_exportBtn, &QPushButton::clicked, this, &YieldCurveWidget::onExportCsv);

    m_calcBtn = new QPushButton("Bootstrap curve", box);
    m_calcBtn->setMinimumHeight(32); m_calcBtn->setMinimumWidth(140);
    m_calcBtn->setStyleSheet(
        "QPushButton{background:#2563eb;color:white;border-radius:6px;font-weight:500;}"
        "QPushButton:hover{background:#1d4ed8;}"
        "QPushButton:disabled{background:#475569;}");
    topRow->addWidget(m_calcBtn);
    vlay->addLayout(topRow);

    connect(m_presetCombo, &QComboBox::currentIndexChanged,
            this, &YieldCurveWidget::onPresetChanged);
    connect(m_calcBtn, &QPushButton::clicked, this, &YieldCurveWidget::onCalculate);

    // Tenor rate spinboxes
    auto* grid = new QGridLayout;
    grid->setHorizontalSpacing(16); grid->setVerticalSpacing(4);

    for (int i = 0; i < m_tenors.size(); ++i) {
        auto& t = m_tenors[i];
        grid->addWidget(new QLabel(t.label, box), 0, i, Qt::AlignCenter);
        t.spin = new QDoubleSpinBox(box);
        t.spin->setRange(0.001, 0.30);
        t.spin->setSingleStep(0.001);
        t.spin->setDecimals(4);
        t.spin->setSuffix("%");    // 顯示用，實際 value 是小數
        t.spin->setMinimumWidth(90);
        // 連接 valueChanged 時標記為 Custom
        connect(t.spin, &QDoubleSpinBox::editingFinished, [this]{
            m_presetCombo->setCurrentIndex(kPresets.size()-1); // Custom
        });
        grid->addWidget(t.spin, 1, i);
    }
    vlay->addLayout(grid);

    // 說明：spinbox 顯示 % 但內部存 decimal
    auto* note = new QLabel("Rates shown as decimal (e.g. 0.0500 = 5.00%)", box);
    note->setStyleSheet("font-size:10px;color:rgba(150,150,150,0.6);");
    vlay->addWidget(note);

    return box;
}

// ─────────────────────────────────────────────────────────────────────────────
QWidget* YieldCurveWidget::buildChartTabs()
{
    m_tabWidget = new QTabWidget(this);
    m_tabWidget->addTab(buildSpotForwardTab(), "Spot / Forward");
    m_tabWidget->addTab(buildDiscountTab(),    "Discount factor");
    return m_tabWidget;
}

QWidget* YieldCurveWidget::buildSpotForwardTab()
{
    auto* w   = new QWidget;
    auto* lay = new QVBoxLayout(w);
    lay->setContentsMargins(6,6,6,6);

    m_spotChart   = makeChart(true);
    m_spotSeries  = new QLineSeries; m_spotSeries->setName("Spot rate");
    m_zeroSeries  = new QLineSeries; m_zeroSeries->setName("Zero rate");
    m_fwdSeries    = new QLineSeries; m_fwdSeries->setName("1M forward (raw)");
    m_fwdSmoothed  = new QLineSeries; m_fwdSmoothed->setName("Forward (3M avg)");
    QPen ps(QColor("#3b82f6")); ps.setWidth(2); m_spotSeries->setPen(ps);
    QPen pz(QColor("#10b981")); pz.setWidth(2); m_zeroSeries->setPen(pz);
    QPen pf(QColor("#f97316")); pf.setWidth(1); pf.setStyle(Qt::DashLine); pf.setColor(QColor(249,115,22,80));
    m_fwdSeries->setPen(pf);
    QPen ps2(QColor("#fb923c")); ps2.setWidth(2); m_fwdSmoothed->setPen(ps2);

    m_spotChart->addSeries(m_spotSeries);
    m_spotChart->addSeries(m_zeroSeries);
    m_spotChart->addSeries(m_fwdSeries);
    m_spotChart->addSeries(m_fwdSmoothed);

    auto* axX = makeAxis("Maturity (years)", "%.1f", 7);
    auto* axY = makeAxis("Rate (%)",         "%.2f", 7);
    m_spotChart->addAxis(axX, Qt::AlignBottom);
    m_spotChart->addAxis(axY, Qt::AlignLeft);
    for (auto* s : {m_spotSeries, m_zeroSeries, m_fwdSeries, m_fwdSmoothed}) {
        s->attachAxis(axX); s->attachAxis(axY);
    }

    m_spotView = new QChartView(m_spotChart, w);
    m_spotView->setRenderHint(QPainter::Antialiasing);
    m_spotView->setMinimumHeight(240);
    lay->addWidget(m_spotView, 1);
    return w;
}

QWidget* YieldCurveWidget::buildDiscountTab()
{
    auto* w   = new QWidget;
    auto* lay = new QVBoxLayout(w);
    lay->setContentsMargins(6,6,6,6);

    m_discChart  = makeChart(false);
    m_discSeries = new QLineSeries; m_discSeries->setName("Discount factor");
    QPen pd(QColor("#a855f7")); pd.setWidth(2); m_discSeries->setPen(pd);
    m_discChart->addSeries(m_discSeries);

    auto* axX = makeAxis("Maturity (years)", "%.1f", 7);
    auto* axY = makeAxis("Discount factor",  "%.3f", 7);
    m_discChart->addAxis(axX, Qt::AlignBottom);
    m_discChart->addAxis(axY, Qt::AlignLeft);
    m_discSeries->attachAxis(axX);
    m_discSeries->attachAxis(axY);

    m_discView = new QChartView(m_discChart, w);
    m_discView->setRenderHint(QPainter::Antialiasing);
    m_discView->setMinimumHeight(240);
    lay->addWidget(m_discView, 1);
    return w;
}

// ─────────────────────────────────────────────────────────────────────────────
QWidget* YieldCurveWidget::buildDataTable()
{
    auto* box = new QGroupBox("Key points", this);
    auto* lay = new QVBoxLayout(box);

    m_dataTable = new QTableWidget(0, 4, box);
    m_dataTable->setHorizontalHeaderLabels({"Maturity","Spot (%)","Zero (%)","Disc. factor"});
    m_dataTable->horizontalHeader()->setStretchLastSection(true);
    m_dataTable->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_dataTable->verticalHeader()->setVisible(false);
    m_dataTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_dataTable->setAlternatingRowColors(true);
    m_dataTable->setShowGrid(false);
    m_dataTable->setMaximumWidth(340);
    lay->addWidget(m_dataTable);
    return box;
}

// ─────────────────────────────────────────────────────────────────────────────
void YieldCurveWidget::onPresetChanged(int index)
{
    if (index >= kPresets.size() || kPresets[index].rates.isEmpty()) return;
    const auto& rates = kPresets[index].rates;
    for (int i = 0; i < qMin(rates.size(), m_tenors.size()); ++i)
        m_tenors[i].spin->setValue(rates[i]);
}

void YieldCurveWidget::onCalculate()
{
    QVector<TenorRate> tenors;
    for (const auto& t : m_tenors)
        tenors.append({t.months, t.spin->value()});

    m_calcBtn->setEnabled(false);
    m_calcBtn->setText("Bootstrapping...");
    m_worker->submitYieldCurve(tenors, AppSettings::instance().riskFreeRate());
    emit statusMessage("Bootstrapping yield curve...");
}

// ─────────────────────────────────────────────────────────────────────────────
void YieldCurveWidget::onYieldCurveFinished(const YieldCurveResult& result)
{
    m_calcBtn->setEnabled(true);
    m_calcBtn->setText("Bootstrap curve");
    m_lastResult = result;   // 儲存供 Export CSV 使用

    if (!result.success) {
        emit statusMessage("Yield curve error: " + result.errorMsg);
        return;
    }

    updateCharts(result);
    updateTable(result);

    // Status bar: 顯示關鍵點
    double maxT = result.maturitiesYears.last();
    double spotShort = result.spotRates.isEmpty() ? 0 : result.spotRates.first();
    double spotLong  = result.spotRates.isEmpty() ? 0 : result.spotRates.last();
    double slope     = spotLong - spotShort;
    QString slopeStr = slope > 0 ? "normal (upward sloping)"
                     : slope < -0.001 ? "inverted" : "flat";

    emit statusMessage(
        QString("Curve: %1  |  3M: %2%  |  %3Y: %4%  |  Shape: %5")
        .arg(m_presetCombo->currentText())
        .arg(spotShort*100, 0,'f',2)
        .arg(maxT, 0,'f',0)
        .arg(spotLong*100, 0,'f',2)
        .arg(slopeStr));
}

// ─────────────────────────────────────────────────────────────────────────────
void YieldCurveWidget::updateCharts(const YieldCurveResult& r)
{
    QVector<QPointF> spotPts, zeroPts, fwdPts, fwdSmPts, discPts;
    int n = r.maturitiesYears.size();
    spotPts.reserve(n); zeroPts.reserve(n);
    fwdPts.reserve(n);  fwdSmPts.reserve(n); discPts.reserve(n);

    for (int i = 0; i < n; ++i) {
        double T = r.maturitiesYears[i];
        spotPts.append({T, r.spotRates[i]});
        zeroPts.append({T, r.zeroRates[i]});
        fwdPts .append({T, r.forwardRates[i]});
        fwdSmPts.append({T, r.smoothedForwardRates[i]});
        discPts.append({T, r.discountFactors[i]});
    }

    m_spotSeries ->replace(spotPts);
    m_zeroSeries ->replace(zeroPts);
    m_fwdSeries  ->replace(fwdPts);
    m_fwdSmoothed->replace(fwdSmPts);
    m_discSeries ->replace(discPts);
    m_exportBtn->setEnabled(true);

    // 更新軸範圍
    double maxT = r.maturitiesYears.last();
    auto updateAxes = [](QChart* chart, double xMax,
                         double yMin, double yMax) {
        auto axX = chart->axes(Qt::Horizontal);
        auto axY = chart->axes(Qt::Vertical);
        if (!axX.isEmpty()) qobject_cast<QValueAxis*>(axX.first())->setRange(0, xMax);
        if (!axY.isEmpty()) {
            double mg = (yMax-yMin)*0.1;
            qobject_cast<QValueAxis*>(axY.first())->setRange(yMin-mg, yMax+mg);
        }
    };

    // Spot/Zero/Forward chart
    double yMin1 = 999, yMax1 = -999;
    for (const auto& v : r.spotRates+r.zeroRates+r.smoothedForwardRates) {
        yMin1 = qMin(yMin1, v); yMax1 = qMax(yMax1, v);
    }
    updateAxes(m_spotChart, maxT, yMin1, yMax1);

    // Discount chart
    double dfMin = *std::min_element(r.discountFactors.begin(), r.discountFactors.end());
    updateAxes(m_discChart, maxT, dfMin, 1.0);
}

// ─────────────────────────────────────────────────────────────────────────────
void YieldCurveWidget::updateTable(const YieldCurveResult& r)
{
    m_dataTable->clearContents();
    m_dataTable->setRowCount(0);

    // 只顯示 key tenors：每年一個點 + 月份短端
    QVector<int> keyMonths = {3, 6, 12, 24, 36, 60, 84, 120, 180, 240, 360};

    for (int km : keyMonths) {
        int idx = km - 1;  // index = months - 1
        if (idx >= r.maturitiesYears.size()) break;

        double T  = r.maturitiesYears[idx];
        int row   = m_dataTable->rowCount();
        m_dataTable->insertRow(row);

        QString tenor = km >= 12
            ? QString("%1Y").arg(km/12)
            : QString("%1M").arg(km);

        auto set = [&](int col, const QString& txt) {
            auto* it = new QTableWidgetItem(txt);
            it->setTextAlignment(Qt::AlignCenter | Qt::AlignVCenter);
            m_dataTable->setItem(row, col, it);
        };

        set(0, tenor);
        set(1, QString::number(r.spotRates[idx], 'f', 3));
        set(2, QString::number(r.zeroRates[idx], 'f', 3));
        set(3, QString::number(r.discountFactors[idx], 'f', 4));

        // 倒掛時把 spot 標紅（< 3M spot）
        if (idx > 0 && r.spotRates[idx] < r.spotRates[0]) {
            for (int c = 0; c < 4; ++c) {
                if (auto* it = m_dataTable->item(row, c))
                    it->setForeground(QColor("#ef4444"));
            }
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// onExportCsv  —  匯出完整曲線數據到 CSV
// ─────────────────────────────────────────────────────────────────────────────
void YieldCurveWidget::onExportCsv()
{
    if (!m_lastResult.success || m_lastResult.maturitiesYears.isEmpty()) {
        emit statusMessage("No curve data to export. Please bootstrap first.");
        return;
    }

    QString path = QFileDialog::getSaveFileName(
        this,
        "Export yield curve data",
        "yield_curve.csv",
        "CSV files (*.csv);;All files (*)");

    if (path.isEmpty()) return;

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, "Export failed",
                             "Cannot open file: " + path);
        return;
    }

    QTextStream out(&file);

    // Header
    out << "Maturity_Years,Spot_Rate_pct,Zero_Rate_pct,"
           "Forward_Raw_pct,Forward_Smoothed_pct,Discount_Factor\n";

    const int n = m_lastResult.maturitiesYears.size();
    for (int i = 0; i < n; ++i) {
        out << QString::number(m_lastResult.maturitiesYears[i],     'f', 4) << ","
            << QString::number(m_lastResult.spotRates[i],           'f', 6) << ","
            << QString::number(m_lastResult.zeroRates[i],           'f', 6) << ","
            << QString::number(m_lastResult.forwardRates[i],        'f', 6) << ","
            << QString::number(m_lastResult.smoothedForwardRates[i],'f', 6) << ","
            << QString::number(m_lastResult.discountFactors[i],     'f', 8) << "\n";
    }
    file.close();

    emit statusMessage(QString("Exported %1 data points to: %2").arg(n).arg(path));
}
