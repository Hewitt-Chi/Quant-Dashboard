#include "PricerWidget.h"
#include "../infra/AsyncWorker.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QComboBox>
#include <QLabel>
#include <QPushButton>
#include <QButtonGroup>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QHeaderView>
#include <QSplitter>
#include <QFrame>
#include <QDateTime>

#include <QtCharts/QChart>
#include <QtCharts/QChartView>
#include <QtCharts/QLineSeries>
#include <QtCharts/QValueAxis>

// ═════════════════════════════════════════════════════════════════════════════
// ResultCard
// ═════════════════════════════════════════════════════════════════════════════
ResultCard::ResultCard(const QString& label, QWidget* parent)
    : QWidget(parent)
{
    setMinimumWidth(90);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(12, 10, 12, 10);
    root->setSpacing(4);

    m_label = new QLabel(label, this);
    m_label->setStyleSheet("font-size:11px; color: rgba(180,180,180,0.8);");

    m_value = new QLabel("—", this);
    m_value->setStyleSheet("font-size:20px; font-weight:500;");
    m_value->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

    root->addWidget(m_label);
    root->addWidget(m_value);

    setStyleSheet(
        "ResultCard {"
        "  background: rgba(128,128,128,0.15);"   // 半透明灰，深淺色都能看到
        "  border: 1px solid rgba(128,128,128,0.3);"
        "  border-radius: 8px;"
        "}"
    );
}

void ResultCard::setValue(double v, int decimals)
{
    m_value->setText(QString::number(v, 'f', decimals));
}

void ResultCard::setHighlight(bool red)
{
    QString color = red ? "#c0392b" : "#2563eb";
    m_value->setStyleSheet(
        QString("font-size:20px; font-weight:500; color:%1;").arg(color));
}

void ResultCard::clear()
{
    m_value->setText("—");
    m_value->setStyleSheet("font-size:20px; font-weight:500;");
}

// ═════════════════════════════════════════════════════════════════════════════
// PricerWidget
// ═════════════════════════════════════════════════════════════════════════════
PricerWidget::PricerWidget(AsyncWorker* worker, QWidget* parent)
    : QWidget(parent)
    , m_worker(worker)
{
    // ── 主佈局：上下分割 ──────────────────────────────────────────────────────
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(12, 12, 12, 12);
    root->setSpacing(10);

    // 上半：輸入 + 結果卡
    root->addWidget(buildInputPanel());
    root->addWidget(buildResultCards());

    // 下半：圖表 + 歷史表格（用 QSplitter 讓使用者可拖動）
    auto* splitter = new QSplitter(Qt::Horizontal, this);
    splitter->addWidget(buildGreeksChart());
    splitter->addWidget(buildHistoryTable());
    splitter->setStretchFactor(0, 3);
    splitter->setStretchFactor(1, 2);
    root->addWidget(splitter, 1);

    // ── 連接 AsyncWorker signal ───────────────────────────────────────────────
    connect(m_worker, &AsyncWorker::pricingFinished,
            this,     &PricerWidget::onPricingFinished);
}

// ─────────────────────────────────────────────────────────────────────────────
// buildInputPanel  —  參數輸入區
// ─────────────────────────────────────────────────────────────────────────────
QWidget* PricerWidget::buildInputPanel()
{
    auto* box = new QGroupBox("Parameters", this);
    auto* grid = new QGridLayout(box);
    grid->setHorizontalSpacing(12);
    grid->setVerticalSpacing(8);

    // ── 輔助 lambda：建立帶 label 的 QDoubleSpinBox ───────────────────────────
    auto makeDouble = [&](const QString& lbl, double min, double max,
                          double val, double step, int dec,
                          int row, int col) -> QDoubleSpinBox*
    {
        grid->addWidget(new QLabel(lbl, box), row * 2, col);
        auto* sb = new QDoubleSpinBox(box);
        sb->setRange(min, max);
        sb->setValue(val);
        sb->setSingleStep(step);
        sb->setDecimals(dec);
        sb->setMinimumWidth(90);
        grid->addWidget(sb, row * 2 + 1, col);
        return sb;
    };

    // Row 0
    m_spot    = makeDouble("Spot price",   0.01, 99999, 540.0,  1.0,  2, 0, 0);
    m_strike  = makeDouble("Strike",       0.01, 99999, 560.0,  1.0,  2, 0, 1);
    m_vol     = makeDouble("Volatility σ", 0.001, 5.0,  0.20,  0.01,  3, 0, 2);
    m_rate    = makeDouble("Risk-free r",  0.0,   1.0,  0.05,  0.01,  3, 0, 3);
    m_div     = makeDouble("Dividend q",   0.0,   1.0,  0.00,  0.01,  3, 0, 4);

    // Row 1 — Maturity, Model, Call/Put, Calculate
    grid->addWidget(new QLabel("Maturity (days)", box), 2, 0);
    m_maturity = new QSpinBox(box);
    m_maturity->setRange(1, 3650);
    m_maturity->setValue(30);
    m_maturity->setMinimumWidth(90);
    grid->addWidget(m_maturity, 3, 0);

    grid->addWidget(new QLabel("Model", box), 2, 1);
    m_model = new QComboBox(box);
    m_model->addItem("Black-Scholes",  static_cast<int>(PricingRequest::Model::BlackScholes));
    m_model->addItem("Binomial (CRR)", static_cast<int>(PricingRequest::Model::Binomial));
    m_model->addItem("Heston",         static_cast<int>(PricingRequest::Model::Heston));
    grid->addWidget(m_model, 3, 1);
    connect(m_model, &QComboBox::currentIndexChanged,
        this, &PricerWidget::onModelChanged);

    // Call / Put 切換按鈕
    grid->addWidget(new QLabel("Option type", box), 2, 2);
    auto* typeRow = new QHBoxLayout;
    auto* callBtn = new QPushButton("Call", box);
    auto* putBtn  = new QPushButton("Put",  box);
    callBtn->setCheckable(true);
    putBtn->setCheckable(true);
    callBtn->setChecked(true);
    callBtn->setMinimumWidth(56);
    putBtn->setMinimumWidth(56);
    m_typeGroup = new QButtonGroup(this);
    m_typeGroup->addButton(callBtn, 0);
    m_typeGroup->addButton(putBtn,  1);
    m_typeGroup->setExclusive(true);
    typeRow->addWidget(callBtn);
    typeRow->addWidget(putBtn);
    typeRow->addStretch();
    auto* typeW = new QWidget(box);
    typeW->setLayout(typeRow);
    grid->addWidget(typeW, 3, 2);

    // Calculate button
    m_calcBtn = new QPushButton("Calculate", box);
    m_calcBtn->setMinimumHeight(36);
    m_calcBtn->setStyleSheet(
        "QPushButton { background:#2563eb; color:white; border-radius:6px; font-weight:500; }"
        "QPushButton:hover { background:#1d4ed8; }"
        "QPushButton:disabled { background:#94a3b8; }");
    // grid->addWidget(m_calcBtn, 3, 4);
    // 改成
    auto* btnRow = new QHBoxLayout;
    btnRow->addStretch();
    btnRow->addWidget(m_calcBtn);
    auto* btnW = new QWidget(box);
    btnW->setLayout(btnRow);
    grid->addWidget(btnW, 3, 2, 1, 3);  // 跨 col 2-4
    connect(m_calcBtn, &QPushButton::clicked, this, &PricerWidget::onCalculate);

    // ── Heston 額外參數面板（預設隱藏）────────────────────────────────────────
    m_hestonPanel = new QWidget(box);
    auto* hestonLayout = new QHBoxLayout(m_hestonPanel);
    hestonLayout->setContentsMargins(0, 0, 0, 0);
    hestonLayout->setSpacing(8);

    auto makeHeston = [&](const QString& lbl, double val) -> QDoubleSpinBox* {
        auto* l  = new QLabel(lbl, m_hestonPanel);
        auto* sb = new QDoubleSpinBox(m_hestonPanel);
        sb->setRange(-10.0, 10.0);
        sb->setValue(val);
        sb->setDecimals(3);
        sb->setSingleStep(0.01);
        sb->setMinimumWidth(70);
        hestonLayout->addWidget(l);
        hestonLayout->addWidget(sb);
        return sb;
    };
    hestonLayout->addWidget(new QLabel("Heston:", m_hestonPanel));
    m_v0     = makeHeston("v₀",    0.04);
    m_kappa  = makeHeston("κ",     1.50);
    m_thetaH = makeHeston("θ_H",   0.04);
    m_sigmaH = makeHeston("σ_v",   0.30);
    m_rhoH   = makeHeston("ρ_H",  -0.70);
    hestonLayout->addStretch();
    m_hestonPanel->setVisible(false);

    grid->addWidget(m_hestonPanel, 4, 0, 1, 5);

    // 讓最後一欄吃掉多餘空間，其他欄位靠左對齊
    grid->setColumnStretch(5, 1);

    return box;
}

// ─────────────────────────────────────────────────────────────────────────────
// buildResultCards  —  Price + Greeks 顯示列
// ─────────────────────────────────────────────────────────────────────────────
QWidget* PricerWidget::buildResultCards()
{
    auto* w = new QWidget(this);
    auto* row = new QHBoxLayout(w);
    row->setContentsMargins(0, 0, 0, 0);
    row->setSpacing(8);

    m_cardPrice = new ResultCard("Price",  w);
    m_cardDelta = new ResultCard("Delta",  w);
    m_cardGamma = new ResultCard("Gamma",  w);
    m_cardVega  = new ResultCard("Vega",   w);
    m_cardTheta = new ResultCard("Theta",  w);
    m_cardRho   = new ResultCard("Rho",    w);

    // Price 用藍色，Theta 用紅色（代表時間價值流失）
    m_cardPrice->setHighlight(false);
    m_cardTheta->setHighlight(true);

    for (auto* card : {m_cardPrice, m_cardDelta, m_cardGamma,
                       m_cardVega,  m_cardTheta, m_cardRho})
        row->addWidget(card, 1);

    return w;
}

// ─────────────────────────────────────────────────────────────────────────────
// buildGreeksChart  —  Delta vs Spot 折線圖
// ─────────────────────────────────────────────────────────────────────────────
QWidget* PricerWidget::buildGreeksChart()
{
    auto* box = new QGroupBox("Delta vs spot price", this);
    auto* lay = new QVBoxLayout(box);

    m_deltaSeries = new QLineSeries;
    m_deltaSeries->setName("Delta");
    QPen pen(QColor("#2563eb"));
    pen.setWidth(2);
    m_deltaSeries->setPen(pen);

    m_chart = new QChart;
    m_chart->addSeries(m_deltaSeries);
    m_chart->setTitle("");
    m_chart->legend()->hide();
    m_chart->setMargins(QMargins(4, 4, 4, 4));
    m_chart->setBackgroundVisible(false);

    auto* axisX = new QValueAxis;
    axisX->setTitleText("Spot price");
    axisX->setLabelFormat("%.0f");
    axisX->setTickCount(7);

    auto* axisY = new QValueAxis;
    axisY->setTitleText("Delta");
    axisY->setRange(0.0, 1.0);
    axisY->setLabelFormat("%.2f");
    axisY->setTickCount(6);

    m_chart->addAxis(axisX, Qt::AlignBottom);
    m_chart->addAxis(axisY, Qt::AlignLeft);
    m_deltaSeries->attachAxis(axisX);
    m_deltaSeries->attachAxis(axisY);

    m_chartView = new QChartView(m_chart, box);
    m_chartView->setRenderHint(QPainter::Antialiasing);
    m_chartView->setMinimumHeight(200);

    lay->addWidget(m_chartView);

    m_chart->setBackgroundBrush(Qt::transparent);
    m_chart->setPlotAreaBackgroundVisible(false);

    // 軸的顏色改成淺色，在深色背景才看得到
    QPen axisPen(QColor(160, 160, 160));
    axisX->setLinePen(axisPen);
    axisX->setLabelsBrush(QColor(200, 200, 200));
    axisX->setTitleBrush(QColor(200, 200, 200));
    axisY->setLinePen(axisPen);
    axisY->setLabelsBrush(QColor(200, 200, 200));
    axisY->setTitleBrush(QColor(200, 200, 200));
    return box;
}

// ─────────────────────────────────────────────────────────────────────────────
// buildHistoryTable  —  最近計算記錄
// ─────────────────────────────────────────────────────────────────────────────
QWidget* PricerWidget::buildHistoryTable()
{
    auto* box = new QGroupBox("Recent calculations", this);
    auto* lay = new QVBoxLayout(box);

    m_historyTable = new QTableWidget(0, 8, box);
    m_historyTable->setHorizontalHeaderLabels(
        {"Type","Strike","T(d)","σ","r","Price","Delta","Time"});
    m_historyTable->horizontalHeader()->setStretchLastSection(true);
    m_historyTable->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_historyTable->verticalHeader()->setVisible(false);
    m_historyTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_historyTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_historyTable->setAlternatingRowColors(true);
    m_historyTable->setShowGrid(false);
    m_historyTable->setMinimumWidth(280);

    lay->addWidget(m_historyTable);

    // 清除按鈕
    auto* clearBtn = new QPushButton("Clear history", box);
    connect(clearBtn, &QPushButton::clicked, m_historyTable, &QTableWidget::clearContents);
    connect(clearBtn, &QPushButton::clicked, [this]{ m_historyTable->setRowCount(0); });
    lay->addWidget(clearBtn);

    return box;
}

// ─────────────────────────────────────────────────────────────────────────────
// onCalculate  —  送出定價請求
// ─────────────────────────────────────────────────────────────────────────────
void PricerWidget::onCalculate()
{
    setLoading(true);
    m_lastRequest = currentRequest();
    m_worker->submitPricing(m_lastRequest);
    emit statusMessage("Calculating...");
}

// ─────────────────────────────────────────────────────────────────────────────
// onPricingFinished  —  接收計算結果
// ─────────────────────────────────────────────────────────────────────────────
void PricerWidget::onPricingFinished(const PricingResult& result)
{
    setLoading(false);

    if (!result.success) {
        emit statusMessage("Error: " + result.errorMsg);
        return;
    }

    m_lastResult = result;

    // ── 更新結果卡 ────────────────────────────────────────────────────────────
    m_cardPrice->setValue(result.price,  4);
    m_cardDelta->setValue(result.delta,  4);
    m_cardGamma->setValue(result.gamma,  6);
    m_cardVega ->setValue(result.vega,   4);
    m_cardTheta->setValue(result.theta,  4);
    m_cardRho  ->setValue(result.rho,    4);

    // ── 更新 Delta vs Spot 曲線 ───────────────────────────────────────────────
    updateDeltaChart();

    // ── 加入歷史記錄 ──────────────────────────────────────────────────────────
    addHistoryRow(m_lastRequest, result);

    emit statusMessage(
        QString("Price: %1 | Delta: %2 | Theta: %3")
        .arg(result.price,  0, 'f', 4)
        .arg(result.delta,  0, 'f', 4)
        .arg(result.theta,  0, 'f', 4));
}

// ─────────────────────────────────────────────────────────────────────────────
// updateDeltaChart  —  在背景計算不同 Spot 下的 Delta，然後更新圖表
// ─────────────────────────────────────────────────────────────────────────────
void PricerWidget::updateDeltaChart()
{
    // 以當前 Strike 為中心，繪製 ±30% 的 spot 範圍
    const double K       = m_lastRequest.strike;
    const double spotMin = K * 0.70;
    const double spotMax = K * 1.30;
    const int    steps   = 60;

    QVector<QPointF> points;
    points.reserve(steps + 1);

    // 在 UI 執行緒直接計算（步數少，速度快）
    for (int i = 0; i <= steps; ++i) {
        double s = spotMin + (spotMax - spotMin) * i / steps;
        PricingRequest req = m_lastRequest;
        req.spot = s;
        PricingResult r = AsyncWorker::staticComputePricing(req);
        if (r.success)
            points.append({s, r.delta});
    }

    m_deltaSeries->replace(points);

    auto* axisX = qobject_cast<QValueAxis*>(m_chart->axes(Qt::Horizontal).first());
    if (axisX) axisX->setRange(spotMin, spotMax);

    // Y 軸根據 option type 調整
    auto* axisY = qobject_cast<QValueAxis*>(m_chart->axes(Qt::Vertical).first());
    if (axisY) {
        bool isCall = (m_lastRequest.optionType == PricingRequest::OptionType::Call);
        axisY->setRange(isCall ? 0.0 : -1.0, isCall ? 1.0 : 0.0);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// addHistoryRow
// ─────────────────────────────────────────────────────────────────────────────
void PricerWidget::addHistoryRow(const PricingRequest& req,
                                 const PricingResult& res)
{
    int row = 0;
    m_historyTable->insertRow(row);   // 插在最上方

    auto setCell = [&](int col, const QString& txt, Qt::Alignment align = Qt::AlignRight) {
        auto* item = new QTableWidgetItem(txt);
        item->setTextAlignment(align | Qt::AlignVCenter);
        m_historyTable->setItem(row, col, item);
    };

    bool isCall = (req.optionType == PricingRequest::OptionType::Call);
    setCell(0, isCall ? "Call" : "Put", Qt::AlignCenter);
    setCell(1, QString::number(req.strike,     'f', 2));
    setCell(2, QString::number(static_cast<int>(req.maturityYears * 365)));
    setCell(3, QString::number(req.volatility, 'f', 3));
    setCell(4, QString::number(req.riskFree,   'f', 3));
    setCell(5, QString::number(res.price,      'f', 4));
    setCell(6, QString::number(res.delta,      'f', 4));
    setCell(7, QDateTime::currentDateTime().toString("hh:mm:ss"), Qt::AlignCenter);

    // Call = 藍色，Put = 紅色 badge
    QColor bg = isCall ? QColor("#dbeafe") : QColor("#fee2e2");
    QColor fg = isCall ? QColor("#1e3a8a") : QColor("#7f1d1d");
    if (auto* item = m_historyTable->item(row, 0)) {
        item->setBackground(bg);
        item->setForeground(fg);
    }

    // 只保留最近 50 筆
    while (m_historyTable->rowCount() > 50)
        m_historyTable->removeRow(m_historyTable->rowCount() - 1);
}

// ─────────────────────────────────────────────────────────────────────────────
// onModelChanged  —  切換 Heston 面板顯示
// ─────────────────────────────────────────────────────────────────────────────
void PricerWidget::onModelChanged(int index)
{
    bool isHeston = (index == 2);
    m_hestonPanel->setVisible(isHeston);
}

// ─────────────────────────────────────────────────────────────────────────────
// currentRequest  —  從 UI 組出 PricingRequest
// ─────────────────────────────────────────────────────────────────────────────
PricingRequest PricerWidget::currentRequest() const
{
    PricingRequest req;
    req.spot          = m_spot->value();
    req.strike        = m_strike->value();
    req.volatility    = m_vol->value();
    req.riskFree      = m_rate->value();
    req.dividendYield = m_div->value();
    req.maturityYears = m_maturity->value() / 365.0;
    req.model         = static_cast<PricingRequest::Model>(
                            m_model->currentData().toInt());
    req.optionType    = (m_typeGroup->checkedId() == 0)
                        ? PricingRequest::OptionType::Call
                        : PricingRequest::OptionType::Put;

    if (req.model == PricingRequest::Model::Heston) {
        req.v0      = m_v0->value();
        req.kappa   = m_kappa->value();
        req.theta_h = m_thetaH->value();
        req.sigma   = m_sigmaH->value();
        req.rho_h   = m_rhoH->value();
    }
    return req;
}

// ─────────────────────────────────────────────────────────────────────────────
// setLoading
// ─────────────────────────────────────────────────────────────────────────────
void PricerWidget::setLoading(bool on)
{
    m_calcBtn->setEnabled(!on);
    m_calcBtn->setText(on ? "Calculating..." : "Calculate");
    if (on) {
        for (auto* c : {m_cardPrice, m_cardDelta, m_cardGamma,
                        m_cardVega,  m_cardTheta, m_cardRho})
            c->clear();
    }
}
