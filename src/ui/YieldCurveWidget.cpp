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

    root->addWidget(buildScenarioPanel());

    auto* splitter = new QSplitter(Qt::Horizontal, this);
    splitter->addWidget(buildChartTabs());
    splitter->addWidget(buildDataTable());
    splitter->setStretchFactor(0, 3);
    splitter->setStretchFactor(1, 1);
    root->addWidget(splitter, 1);

    connect(m_worker, &AsyncWorker::yieldCurveFinished,
            this, &YieldCurveWidget::onYieldCurveFinished);
    connect(m_worker, &AsyncWorker::nelsonSiegelFinished,
            this, &YieldCurveWidget::onNelsonSiegelFinished);

    // 初始化 scenario series
    m_scenSpot1 = m_scenSpot2 = m_scenSpot3 = nullptr;
    m_scenCount = 0;

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
    // ── Fit Nelson-Siegel 按鈕 ────────────────────────────────────────────────
        m_nsBtn = new QPushButton("Fit Nelson-Siegel", box);
    m_nsBtn->setMinimumHeight(32);
    m_nsBtn->setMinimumWidth(140);
    m_nsBtn->setEnabled(false);   // Bootstrap 完成後才啟用
    m_nsBtn->setStyleSheet(
        "QPushButton{border:1px solid #7c3aed;color:#c4b5fd;"
        "border-radius:6px;padding:0 10px;}"
        "QPushButton:hover{background:#1e1b4b;}"
        "QPushButton:disabled{color:palette(mid);border-color:palette(mid);}");
    topRow->addWidget(m_nsBtn);
    connect(m_nsBtn, &QPushButton::clicked,
        this, &YieldCurveWidget::onFitNelsonSiegel);

    m_nsParamLbl = new QLabel("", box);
    m_nsParamLbl->setStyleSheet("font-size:10px;color:rgba(196,181,253,0.7);");
    topRow->addWidget(m_nsParamLbl);
    // ─────────────────────────────────────────────────────────────────────────

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
    m_nsSeries     = new QLineSeries; m_nsSeries->setName("Nelson-Siegel fit");
    QPen pns(QColor("#f0abfc")); pns.setWidth(2); pns.setStyle(Qt::DotLine);
    m_nsSeries->setPen(pns);
    QPen ps(QColor("#3b82f6")); ps.setWidth(2); m_spotSeries->setPen(ps);
    QPen pz(QColor("#10b981")); pz.setWidth(2); m_zeroSeries->setPen(pz);
    QPen pf(QColor("#f97316")); pf.setWidth(1); pf.setStyle(Qt::DashLine); pf.setColor(QColor(249,115,22,80));
    m_fwdSeries->setPen(pf);
    QPen ps2(QColor("#fb923c")); ps2.setWidth(2); m_fwdSmoothed->setPen(ps2);

    m_spotChart->addSeries(m_spotSeries);
    m_spotChart->addSeries(m_zeroSeries);
    m_spotChart->addSeries(m_fwdSeries);
    m_spotChart->addSeries(m_fwdSmoothed);
    m_spotChart->addSeries(m_nsSeries);

    auto* axX = makeAxis("Maturity (years)", "%.1f", 7);
    auto* axY = makeAxis("Rate (%)",         "%.2f", 7);
    m_spotChart->addAxis(axX, Qt::AlignBottom);
    m_spotChart->addAxis(axY, Qt::AlignLeft);
    for (auto* s : {m_spotSeries, m_zeroSeries, m_fwdSeries, m_fwdSmoothed, m_nsSeries}) {
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
    if (m_applyShiftBtn) m_applyShiftBtn->setEnabled(true);
    if (m_nsBtn)        m_nsBtn->setEnabled(true);

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

// =============================================================================
// buildScenarioPanel — 情境分析（平移 / 扭轉 / 蝶形）
// =============================================================================
QWidget* YieldCurveWidget::buildScenarioPanel()
{
    auto* box  = new QGroupBox("Scenario analysis", this);
    auto* grid = new QGridLayout(box);
    grid->setHorizontalSpacing(10); grid->setVerticalSpacing(6);
    grid->setContentsMargins(12,10,12,10);

    auto makeSpin = [&](const QString& lbl, double v, int row, int col) -> QDoubleSpinBox* {
        grid->addWidget(new QLabel(lbl, box), row, col*2);
        auto* s = new QDoubleSpinBox(box);
        s->setRange(-500, 500); s->setValue(v);
        s->setSingleStep(5); s->setDecimals(0); s->setSuffix(" bps");
        s->setMinimumWidth(110);
        grid->addWidget(s, row, col*2+1);
        return s;
    };

    // Row 0：平移
    grid->addWidget(new QLabel("<b>Parallel shift</b>", box), 0, 0, 1, 2);
    m_parallelShift = makeSpin("All tenors:", 25, 1, 0);

    // Row 2：扭轉
    grid->addWidget(new QLabel("<b>Twist (steepen/flatten)</b>", box), 2, 0, 1, 4);
    m_twistShort = makeSpin("Short end:",  50, 3, 0);
    m_twistLong  = makeSpin("Long end:",  -25, 3, 1);

    // Row 4：蝶形
    grid->addWidget(new QLabel("<b>Butterfly</b>", box), 4, 0, 1, 4);
    m_butterflyMid = makeSpin("Mid-curve:",  30, 5, 0);

    grid->setColumnStretch(4, 1);

    m_applyShiftBtn = new QPushButton("Add scenario curve", box);
    m_applyShiftBtn->setMinimumHeight(32); m_applyShiftBtn->setMinimumWidth(160);
    m_applyShiftBtn->setStyleSheet(
        "QPushButton{background:#7c3aed;color:white;border-radius:6px;font-weight:500;}"
        "QPushButton:hover{background:#6d28d9;}"
        "QPushButton:disabled{background:#475569;}");
    m_applyShiftBtn->setEnabled(false);  // 需先計算基準曲線
    grid->addWidget(m_applyShiftBtn, 5, 3);
    connect(m_applyShiftBtn, &QPushButton::clicked,
            this, &YieldCurveWidget::onApplyShift);

    auto* clearBtn = new QPushButton("Clear scenarios", box);
    clearBtn->setMinimumHeight(32);
    clearBtn->setStyleSheet(
        "QPushButton{border:0.5px solid palette(mid);border-radius:6px;padding:0 10px;}"
        "QPushButton:hover{background:palette(midlight);}");
    connect(clearBtn, &QPushButton::clicked, [this]{
        for (auto* s : {m_scenSpot1, m_scenSpot2, m_scenSpot3}) {
            if (s) s->clear();
        }
        m_scenCount = 0;
        emit statusMessage("Scenario curves cleared.");
    });
    grid->addWidget(clearBtn, 5, 4);

    auto* hint = new QLabel(
        "Shifts are in basis points (1 bps = 0.01%).  Max 3 scenario curves.", box);
    hint->setStyleSheet("font-size:10px;color:rgba(150,150,150,0.6);");
    grid->addWidget(hint, 6, 0, 1, 5);

    return box;
}

// ─────────────────────────────────────────────────────────────────────────────
void YieldCurveWidget::onApplyShift()
{
    if (!m_lastResult.success || m_lastResult.maturitiesYears.isEmpty()) return;
    if (m_scenCount >= 3) {
        emit statusMessage("Maximum 3 scenario curves. Click 'Clear scenarios' first.");
        return;
    }

    double parallel = m_parallelShift->value() / 10000.0;
    double twistS   = m_twistShort->value()    / 10000.0;
    double twistL   = m_twistLong->value()     / 10000.0;
    double butterfly= m_butterflyMid->value()  / 10000.0;

    // 建立 shifted tenor rates
    QVector<TenorRate> shiftedTenors;
    double maxT = m_lastResult.maturitiesYears.last();
    for (int i = 0; i < m_tenors.size(); ++i) {
        double T        = m_tenors[i].months / 12.0;
        double baseRate = m_tenors[i].spin->value();

        // 平移
        double shift = parallel;

        // 扭轉：線性從 short end → long end
        double tRatio = maxT > 0 ? T / maxT : 0;
        shift += twistS * (1.0 - tRatio) + twistL * tRatio;

        // 蝶形：頂點在曲線中間，拋物線形狀
        // 中端最大，兩端最小
        double midT = maxT / 2.0;
        double bShape = 1.0 - std::pow((T - midT) / midT, 2.0);
        shift += butterfly * qMax(0.0, bShape);

        double newRate = qMax(0.001, baseRate + shift);
        shiftedTenors.append({m_tenors[i].months, newRate});
    }

    // 同步計算（資料量小，直接在 UI thread 跑）
    YieldCurveResult sc = AsyncWorker::staticComputeYieldCurve(shiftedTenors,
                              AppSettings::instance().riskFreeRate());
    if (!sc.success) {
        emit statusMessage("Scenario error: " + sc.errorMsg);
        return;
    }

    // 選顏色
    static const QColor scenColors[] = {
        QColor("#a855f7"),   // 紫
        QColor("#f97316"),   // 橘
        QColor("#22c55e"),   // 綠
    };
    QString name = QString("Scenario %1 (+%2bps P / %3/%4 T / %5 B)")
                   .arg(m_scenCount+1)
                   .arg(m_parallelShift->value(),0,'f',0)
                   .arg(m_twistShort->value(),0,'f',0)
                   .arg(m_twistLong->value(),0,'f',0)
                   .arg(m_butterflyMid->value(),0,'f',0);

    addScenarioSeries(sc, name, scenColors[m_scenCount]);
    m_scenCount++;
    emit statusMessage(name + " added.");
}

// ─────────────────────────────────────────────────────────────────────────────
void YieldCurveWidget::addScenarioSeries(const YieldCurveResult& r,
                                          const QString& name,
                                          const QColor& color)
{
    QLineSeries** target = nullptr;
    if      (m_scenCount == 0) target = &m_scenSpot1;
    else if (m_scenCount == 1) target = &m_scenSpot2;
    else if (m_scenCount == 2) target = &m_scenSpot3;
    else return;

    if (*target == nullptr) {
        *target = new QLineSeries;
        QPen p(color); p.setWidth(2); p.setStyle(Qt::DashDotLine);
        (*target)->setPen(p);
        m_spotChart->addSeries(*target);
        auto axX = m_spotChart->axes(Qt::Horizontal);
        auto axY = m_spotChart->axes(Qt::Vertical);
        if (!axX.isEmpty()) (*target)->attachAxis(axX.first());
        if (!axY.isEmpty()) (*target)->attachAxis(axY.first());
    }

    (*target)->setName(name);
    QVector<QPointF> pts;
    for (int i = 0; i < r.maturitiesYears.size(); ++i)
        pts.append({r.maturitiesYears[i], r.spotRates[i]});
    (*target)->replace(pts);
}

// =============================================================================
// Nelson-Siegel 擬合
// =============================================================================
void YieldCurveWidget::onFitNelsonSiegel()
{
    QVector<TenorRate> tenors;
    for (const auto& t : m_tenors)
        tenors.append({t.months, t.spin->value()});

    m_nsBtn->setEnabled(false);
    m_nsBtn->setText("Fitting...");
    m_worker->submitNelsonSiegel(tenors);
    emit statusMessage("Fitting Nelson-Siegel model...");
}

void YieldCurveWidget::onNelsonSiegelFinished(const NelsonSiegelResult& result)
{
    m_nsBtn->setEnabled(true);
    m_nsBtn->setText("Fit Nelson-Siegel");

    if (!result.success) {
        emit statusMessage("NS fit error: " + result.errorMsg);
        return;
    }

    // 更新疊加曲線
    QVector<QPointF> pts;
    for (int i = 0; i < result.maturitiesYears.size(); ++i)
        pts.append({result.maturitiesYears[i], result.nsRates[i]});
    m_nsSeries->replace(pts);

    // 顯示擬合參數
    m_nsParamLbl->setText(
        QString("NS: β₀=%1  β₁=%2  β₂=%3  τ=%4  RMSE=%5bps")
        .arg(result.beta0*100, 0,'f',2)
        .arg(result.beta1*100, 0,'f',2)
        .arg(result.beta2*100, 0,'f',2)
        .arg(result.tau,       0,'f',2)
        .arg(result.fitError*100, 0,'f',1));

    emit statusMessage(
        QString("Nelson-Siegel fit: β₀=%1%% β₁=%2%% β₂=%3%% τ=%4 RMSE=%5bps")
        .arg(result.beta0*100, 0,'f',2)
        .arg(result.beta1*100, 0,'f',2)
        .arg(result.beta2*100, 0,'f',2)
        .arg(result.tau,       0,'f',2)
        .arg(result.fitError*100, 0,'f',1));
}
