#include "PricerWidget.h"
#include "../infra/AsyncWorker.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
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
#include <QTabWidget>
#include <QDateTime>
#include <QMouseEvent>
#include <QGraphicsScene>
#include <QGraphicsLineItem>
#include <QGraphicsSimpleTextItem>
#include <QGraphicsRectItem>
#include <cmath>

#include <QtCharts/QChart>
#include <QtCharts/QChartView>
#include <QtCharts/QLineSeries>
#include <QtCharts/QValueAxis>
#include <QtCharts/QLegend>

// ── Greek 顏色定義 ────────────────────────────────────────────────────────────
struct GreekStyle {
    QString name;
    QString hex;
    int     decimals;
    QString unit;
};
static const GreekStyle kGreeks[] = {
    { "Delta", "#3b82f6", 4, "" },   // 藍
    { "Gamma", "#a855f7", 6, "" },   // 紫
    { "Vega",  "#10b981", 2, "" },   // 綠
    { "Theta", "#ef4444", 2, "" },   // 紅
};

// ── 建立共用圖表物件 ──────────────────────────────────────────────────────────
static QChart* makeChart()
{
    auto* c = new QChart;
    c->legend()->setVisible(true);
    c->legend()->setAlignment(Qt::AlignBottom);
    c->legend()->setLabelColor(QColor(180, 180, 180));
    c->setMargins(QMargins(4, 4, 4, 4));
    c->setBackgroundVisible(false);
    c->setPlotAreaBackgroundVisible(false);
    return c;
}

static QValueAxis* makeAxis(const QString& title,
                             const QString& fmt = "%.3g",
                             int ticks = 6)
{
    auto* a = new QValueAxis;
    a->setTitleText(title);
    a->setLabelFormat(fmt);
    a->setTickCount(ticks);
    a->setLabelsColor(QColor(180, 180, 180));
    a->setTitleBrush(QColor(180, 180, 180));
    QPen gp(QColor(55, 55, 55)); gp.setWidth(1);
    a->setGridLinePen(gp);
    QPen ap(QColor(90, 90, 90)); ap.setWidth(1);
    a->setLinePen(ap);
    return a;
}

// ─────────────────────────────────────────────────────────────────────────────
// autoDecimals  —  根據數值範圍自動決定小數位
// ─────────────────────────────────────────────────────────────────────────────
int PricerWidget::autoDecimals(double rangeMin, double rangeMax)
{
    double range = std::abs(rangeMax - rangeMin);
    if (range == 0.0) return 4;
    int mag = static_cast<int>(std::floor(std::log10(range)));
    int dec = std::max(0, 2 - mag);
    return std::min(dec, 6);
}

// ═════════════════════════════════════════════════════════════════════════════
// CrosshairChartView
// ═════════════════════════════════════════════════════════════════════════════
CrosshairChartView::CrosshairChartView(QChart* chart, QWidget* parent)
    : QChartView(chart, parent)
{
    setRenderHint(QPainter::Antialiasing);
    setMouseTracking(true);
    setMinimumHeight(220);

    // 建立十字線（加到 scene，初始隱藏）
    QPen linePen(QColor(200, 200, 200, 120));
    linePen.setStyle(Qt::DashLine);
    linePen.setWidth(1);

    m_vLine = scene()->addLine(0,0,0,0, linePen);
    m_hLine = scene()->addLine(0,0,0,0, linePen);
    m_vLine->setVisible(false);
    m_hLine->setVisible(false);

    // Tooltip 文字
    m_tooltip = scene()->addSimpleText("");
    m_tooltip->setBrush(QColor(220, 220, 220));
    m_tooltip->setFont(QFont("Arial", 9));
    m_tooltip->setVisible(false);
    m_tooltip->setZValue(10);
}

void CrosshairChartView::setYFormat(int decimals, const QString& unit)
{
    m_decimals = decimals;
    m_unit     = unit;
}

void CrosshairChartView::mouseMoveEvent(QMouseEvent* event)
{
    QChartView::mouseMoveEvent(event);
    updateCrosshair(event->pos());
}

void CrosshairChartView::leaveEvent(QEvent* event)
{
    QChartView::leaveEvent(event);
    hideCrosshair();
}

void CrosshairChartView::updateCrosshair(const QPointF& pos)
{
    // 把 widget 座標轉成 chart 座標（數值）
    QPointF chartPt = chart()->mapToValue(
        chart()->mapFromScene(mapToScene(pos.toPoint())));

    // 確認在 plot area 範圍內
    QRectF plotArea = chart()->plotArea();
    QPointF scenePos = mapToScene(pos.toPoint());
    if (!plotArea.contains(chart()->mapFromScene(scenePos))) {
        hideCrosshair();
        return;
    }

    // 計算 scene 座標（用於畫線）
    QPointF sp = chart()->mapFromScene(scenePos);
    QPointF sceneFromChart = mapToScene(
        chart()->mapToPosition(chartPt).toPoint());

    m_vLine->setLine(scenePos.x(), plotArea.top(),
                     scenePos.x(), plotArea.bottom());
    m_hLine->setLine(plotArea.left(),  scenePos.y(),
                     plotArea.right(), scenePos.y());
    m_vLine->setVisible(true);
    m_hLine->setVisible(true);

    // Tooltip 文字
    auto* axX = qobject_cast<QValueAxis*>(chart()->axes(Qt::Horizontal).value(0));
    auto* axY = qobject_cast<QValueAxis*>(chart()->axes(Qt::Vertical).value(0));
    if (!axX || !axY) return;

    QString txt = QString("x: %1\ny: %2%3")
        .arg(chartPt.x(), 0, 'f', 2)
        .arg(chartPt.y(), 0, 'f', m_decimals)
        .arg(m_unit);

    m_tooltip->setText(txt);

    // Tooltip 位置：避免超出右邊界
    double tx = scenePos.x() + 12;
    double ty = scenePos.y() - 36;
    QRectF tbr = m_tooltip->boundingRect();
    if (tx + tbr.width() > plotArea.right())
        tx = scenePos.x() - tbr.width() - 8;
    if (ty < plotArea.top())
        ty = scenePos.y() + 8;
    m_tooltip->setPos(tx, ty);
    m_tooltip->setVisible(true);
}

void CrosshairChartView::hideCrosshair()
{
    m_vLine->setVisible(false);
    m_hLine->setVisible(false);
    m_tooltip->setVisible(false);
}

// ═════════════════════════════════════════════════════════════════════════════
// ResultCard
// ═════════════════════════════════════════════════════════════════════════════
ResultCard::ResultCard(const QString& label, QWidget* parent)
    : QWidget(parent)
{
    setMinimumWidth(90);
    auto* lay = new QVBoxLayout(this);
    lay->setContentsMargins(12,10,12,10);
    lay->setSpacing(4);

    m_label = new QLabel(label, this);
    m_label->setStyleSheet("font-size:11px;color:rgba(180,180,180,0.8);");

    m_value = new QLabel("—", this);
    m_value->setStyleSheet("font-size:20px;font-weight:500;");

    lay->addWidget(m_label);
    lay->addWidget(m_value);

    setStyleSheet("ResultCard{"
                  "background:rgba(128,128,128,0.15);"
                  "border:1px solid rgba(128,128,128,0.3);"
                  "border-radius:8px;}");
}

void ResultCard::setValue(double v, int d)
{
    m_value->setText(QString::number(v, 'f', d));
}

void ResultCard::setColor(const QString& hex)
{
    m_value->setStyleSheet(
        QString("font-size:20px;font-weight:500;color:%1;").arg(hex));
}

void ResultCard::clear()
{
    m_value->setText("—");
    m_value->setStyleSheet("font-size:20px;font-weight:500;");
}

// ═════════════════════════════════════════════════════════════════════════════
// PricerWidget 建構
// ═════════════════════════════════════════════════════════════════════════════
PricerWidget::PricerWidget(AsyncWorker* worker, QWidget* parent)
    : QWidget(parent), m_worker(worker)
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(12,12,12,12);
    root->setSpacing(10);

    root->addWidget(buildInputPanel());
    root->addWidget(buildResultCards());

    auto* splitter = new QSplitter(Qt::Horizontal, this);
    splitter->addWidget(buildChartTabs());
    splitter->addWidget(buildHistoryTable());
    splitter->setStretchFactor(0,3);
    splitter->setStretchFactor(1,2);
    root->addWidget(splitter, 1);

    connect(m_worker, &AsyncWorker::pricingFinished,
            this, &PricerWidget::onPricingFinished);
}

// ─────────────────────────────────────────────────────────────────────────────
// buildInputPanel
// ─────────────────────────────────────────────────────────────────────────────
QWidget* PricerWidget::buildInputPanel()
{
    auto* box  = new QGroupBox("Parameters", this);
    auto* grid = new QGridLayout(box);
    grid->setHorizontalSpacing(12);
    grid->setVerticalSpacing(6);

    auto dbl = [&](const QString& lbl, double mn, double mx,
                   double v, double step, int dec,
                   int row, int col) -> QDoubleSpinBox*
    {
        grid->addWidget(new QLabel(lbl, box), row*2, col);
        auto* sb = new QDoubleSpinBox(box);
        sb->setRange(mn,mx); sb->setValue(v);
        sb->setSingleStep(step); sb->setDecimals(dec);
        sb->setMinimumWidth(88);
        grid->addWidget(sb, row*2+1, col);
        return sb;
    };

    m_spot    = dbl("Spot price",   0.01,99999, 540.0, 1.0,  2, 0,0);
    m_strike  = dbl("Strike",       0.01,99999, 560.0, 1.0,  2, 0,1);
    m_vol     = dbl("Volatility σ", 0.001, 5.0,  0.20,0.01,  3, 0,2);
    m_rate    = dbl("Risk-free r",  0.0,   1.0,  0.05,0.005, 3, 0,3);
    m_div     = dbl("Dividend q",   0.0,   1.0,  0.00,0.005, 3, 0,4);

    grid->addWidget(new QLabel("Maturity (days)", box), 2,0);
    m_maturity = new QSpinBox(box);
    m_maturity->setRange(1,3650); m_maturity->setValue(30);
    m_maturity->setMinimumWidth(88);
    grid->addWidget(m_maturity, 3,0);

    grid->addWidget(new QLabel("Model", box), 2,1);
    m_model = new QComboBox(box);
    m_model->addItem("Black-Scholes",  0);
    m_model->addItem("Binomial (CRR)", 1);
    m_model->addItem("Heston",         2);
    grid->addWidget(m_model, 3,1);
    connect(m_model, &QComboBox::currentIndexChanged,
            this, &PricerWidget::onModelChanged);

    grid->addWidget(new QLabel("Option type", box), 2,2);
    auto* callBtn = new QPushButton("Call", box);
    auto* putBtn  = new QPushButton("Put",  box);
    callBtn->setCheckable(true); putBtn->setCheckable(true);
    callBtn->setChecked(true);
    m_typeGroup = new QButtonGroup(this);
    m_typeGroup->addButton(callBtn,0);
    m_typeGroup->addButton(putBtn,1);
    m_typeGroup->setExclusive(true);
    auto* typeRow = new QHBoxLayout;
    typeRow->addWidget(callBtn); typeRow->addWidget(putBtn); typeRow->addStretch();
    auto* typeW = new QWidget(box); typeW->setLayout(typeRow);
    grid->addWidget(typeW, 3,2);

    m_calcBtn = new QPushButton("Calculate", box);
    m_calcBtn->setMinimumHeight(36); m_calcBtn->setMinimumWidth(110);
    m_calcBtn->setStyleSheet(
        "QPushButton{background:#2563eb;color:white;border-radius:6px;font-weight:500;}"
        "QPushButton:hover{background:#1d4ed8;}"
        "QPushButton:disabled{background:#475569;}");
    auto* btnRow = new QHBoxLayout;
    btnRow->addStretch(); btnRow->addWidget(m_calcBtn);
    auto* btnW = new QWidget(box); btnW->setLayout(btnRow);
    grid->addWidget(btnW, 3,3,1,2);
    grid->setColumnStretch(5,1);
    connect(m_calcBtn, &QPushButton::clicked, this, &PricerWidget::onCalculate);

    // Heston panel
    m_hestonPanel = new QWidget(box);
    auto* hLay = new QHBoxLayout(m_hestonPanel);
    hLay->setContentsMargins(0,2,0,0); hLay->setSpacing(8);
    hLay->addWidget(new QLabel("Heston:", m_hestonPanel));
    auto hd = [&](const QString& l, double v) -> QDoubleSpinBox* {
        auto* lb = new QLabel(l, m_hestonPanel);
        auto* sb = new QDoubleSpinBox(m_hestonPanel);
        sb->setRange(-10,10); sb->setValue(v);
        sb->setDecimals(3); sb->setSingleStep(0.01); sb->setMinimumWidth(72);
        hLay->addWidget(lb); hLay->addWidget(sb); return sb;
    };
    m_v0=hd("v₀",0.04); m_kappa=hd("κ",1.5); m_thetaH=hd("θ_H",0.04);
    m_sigmaH=hd("σ_v",0.3); m_rhoH=hd("ρ_H",-0.7);
    hLay->addStretch();
    m_hestonPanel->setVisible(false);
    grid->addWidget(m_hestonPanel, 4,0,1,6);

    return box;
}

// ─────────────────────────────────────────────────────────────────────────────
// buildResultCards  —  每張卡用各自的顏色
// ─────────────────────────────────────────────────────────────────────────────
QWidget* PricerWidget::buildResultCards()
{
    auto* w   = new QWidget(this);
    auto* row = new QHBoxLayout(w);
    row->setContentsMargins(0,0,0,0); row->setSpacing(8);

    m_cardPrice = new ResultCard("Price", w);
    m_cardDelta = new ResultCard("Delta", w);
    m_cardGamma = new ResultCard("Gamma", w);
    m_cardVega  = new ResultCard("Vega",  w);
    m_cardTheta = new ResultCard("Theta", w);
    m_cardRho   = new ResultCard("Rho",   w);

    // 對應 kGreeks 的顏色
    m_cardPrice->setColor("#f59e0b");          // 金色（Price）
    m_cardDelta->setColor(kGreeks[0].hex);     // 藍
    m_cardGamma->setColor(kGreeks[1].hex);     // 紫
    m_cardVega ->setColor(kGreeks[2].hex);     // 綠
    m_cardTheta->setColor(kGreeks[3].hex);     // 紅
    m_cardRho  ->setColor("#64748b");          // 灰藍

    for (auto* c : {m_cardPrice,m_cardDelta,m_cardGamma,
                    m_cardVega, m_cardTheta,m_cardRho})
        row->addWidget(c,1);
    return w;
}

// ─────────────────────────────────────────────────────────────────────────────
// buildChartTabs
// ─────────────────────────────────────────────────────────────────────────────
QWidget* PricerWidget::buildChartTabs()
{
    m_tabWidget = new QTabWidget(this);
    m_tabWidget->addTab(buildGreekVsSpotTab(), "Greek vs Spot");
    m_tabWidget->addTab(buildVolSmileTab(),    "Vol Smile");
    m_tabWidget->addTab(buildModelCompareTab(),"BSM vs Heston");
    connect(m_tabWidget, &QTabWidget::currentChanged,
            this, &PricerWidget::onTabChanged);
    return m_tabWidget;
}

// ─────────────────────────────────────────────────────────────────────────────
// Tab 1 — Greek vs Spot
// ─────────────────────────────────────────────────────────────────────────────
QWidget* PricerWidget::buildGreekVsSpotTab()
{
    auto* w   = new QWidget;
    auto* lay = new QVBoxLayout(w);
    lay->setContentsMargins(8,8,8,8); lay->setSpacing(6);

    auto* topRow = new QHBoxLayout;
    topRow->addWidget(new QLabel("Show:", w));
    m_greekSelector = new QComboBox(w);
    for (const auto& g : kGreeks) m_greekSelector->addItem(g.name);
    topRow->addWidget(m_greekSelector);
    topRow->addStretch();

    // 說明標籤
    auto* hint = new QLabel("滑鼠移入圖表可看到十字準線與數值", w);
    hint->setStyleSheet("font-size:11px;color:rgba(150,150,150,0.7);");
    topRow->addWidget(hint);
    lay->addLayout(topRow);

    connect(m_greekSelector, &QComboBox::currentIndexChanged,
            this, &PricerWidget::onGreekSelectorChanged);

    m_greekChart  = makeChart();
    m_greekSeries = new QLineSeries;
    m_greekChart->addSeries(m_greekSeries);

    auto* axX = makeAxis("Spot price", "%.0f", 7);
    auto* axY = makeAxis("Value",      "%.4g", 6);
    m_greekChart->addAxis(axX, Qt::AlignBottom);
    m_greekChart->addAxis(axY, Qt::AlignLeft);
    m_greekSeries->attachAxis(axX);
    m_greekSeries->attachAxis(axY);

    m_greekView = new CrosshairChartView(m_greekChart, w);
    lay->addWidget(m_greekView, 1);
    return w;
}

// ─────────────────────────────────────────────────────────────────────────────
// Tab 2 — Vol Smile
// ─────────────────────────────────────────────────────────────────────────────
QWidget* PricerWidget::buildVolSmileTab()
{
    auto* w   = new QWidget;
    auto* lay = new QVBoxLayout(w);
    lay->setContentsMargins(8,8,8,8);

    auto* info = new QLabel(
        "Implied vol (%) vs Strike — BSM bisection.  "
        "Blue = Call  ·  Red = Put", w);
    info->setStyleSheet("font-size:11px;color:rgba(150,150,150,0.7);");
    lay->addWidget(info);

    m_smileChart   = makeChart();
    m_smileSeriesC = new QLineSeries; m_smileSeriesC->setName("Call IV");
    m_smileSeriesP = new QLineSeries; m_smileSeriesP->setName("Put IV");
    QPen pc(QColor("#3b82f6")); pc.setWidth(2); m_smileSeriesC->setPen(pc);
    QPen pp(QColor("#ef4444")); pp.setWidth(2); m_smileSeriesP->setPen(pp);
    m_smileChart->addSeries(m_smileSeriesC);
    m_smileChart->addSeries(m_smileSeriesP);

    auto* axX = makeAxis("Strike",         "%.0f", 9);
    auto* axY = makeAxis("Implied vol (%)","%.2f", 6);
    m_smileChart->addAxis(axX, Qt::AlignBottom);
    m_smileChart->addAxis(axY, Qt::AlignLeft);
    m_smileSeriesC->attachAxis(axX); m_smileSeriesC->attachAxis(axY);
    m_smileSeriesP->attachAxis(axX); m_smileSeriesP->attachAxis(axY);

    m_smileView = new CrosshairChartView(m_smileChart, w);
    m_smileView->setYFormat(2, "%");
    lay->addWidget(m_smileView, 1);
    return w;
}

// ─────────────────────────────────────────────────────────────────────────────
// Tab 3 — BSM vs Heston
// ─────────────────────────────────────────────────────────────────────────────
QWidget* PricerWidget::buildModelCompareTab()
{
    auto* w   = new QWidget;
    auto* lay = new QVBoxLayout(w);
    lay->setContentsMargins(8,8,8,8);

    auto* info = new QLabel(
        "Price vs Spot — Blue = BSM  ·  Orange = Heston", w);
    info->setStyleSheet("font-size:11px;color:rgba(150,150,150,0.7);");
    lay->addWidget(info);

    m_compareChart  = makeChart();
    m_compareBSM    = new QLineSeries; m_compareBSM->setName("BSM");
    m_compareHeston = new QLineSeries; m_compareHeston->setName("Heston");
    QPen pb(QColor("#3b82f6")); pb.setWidth(2); m_compareBSM->setPen(pb);
    QPen ph(QColor("#f97316")); ph.setWidth(2); m_compareHeston->setPen(ph);
    m_compareChart->addSeries(m_compareBSM);
    m_compareChart->addSeries(m_compareHeston);

    auto* axX = makeAxis("Spot price",   "%.0f", 7);
    auto* axY = makeAxis("Option price", "%.2f", 6);
    m_compareChart->addAxis(axX, Qt::AlignBottom);
    m_compareChart->addAxis(axY, Qt::AlignLeft);
    m_compareBSM->attachAxis(axX);    m_compareBSM->attachAxis(axY);
    m_compareHeston->attachAxis(axX); m_compareHeston->attachAxis(axY);

    m_compareView = new CrosshairChartView(m_compareChart, w);
    m_compareView->setYFormat(4);
    lay->addWidget(m_compareView, 1);
    return w;
}

// ─────────────────────────────────────────────────────────────────────────────
// buildHistoryTable
// ─────────────────────────────────────────────────────────────────────────────
QWidget* PricerWidget::buildHistoryTable()
{
    auto* box = new QGroupBox("Recent calculations", this);
    auto* lay = new QVBoxLayout(box);

    m_historyTable = new QTableWidget(0, 8, box);
    m_historyTable->setHorizontalHeaderLabels(
        {"Type","Strike","T(d)","σ","r","Price","Delta","Time"});
    m_historyTable->horizontalHeader()->setStretchLastSection(true);
    m_historyTable->horizontalHeader()
        ->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_historyTable->verticalHeader()->setVisible(false);
    m_historyTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_historyTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_historyTable->setAlternatingRowColors(true);
    m_historyTable->setShowGrid(false);
    m_historyTable->setMinimumWidth(260);
    lay->addWidget(m_historyTable);

    auto* clearBtn = new QPushButton("Clear history", box);
    connect(clearBtn, &QPushButton::clicked, [this]{
        m_historyTable->clearContents();
        m_historyTable->setRowCount(0);
    });
    lay->addWidget(clearBtn);
    return box;
}

// ─────────────────────────────────────────────────────────────────────────────
// Slots
// ─────────────────────────────────────────────────────────────────────────────
void PricerWidget::onCalculate()
{
    setLoading(true);
    m_lastRequest = currentRequest();
    m_worker->submitPricing(m_lastRequest);
    emit statusMessage("Calculating...");
}

void PricerWidget::onPricingFinished(const PricingResult& result)
{
    setLoading(false);
    if (!result.success) {
        emit statusMessage("Error: " + result.errorMsg);
        return;
    }
    m_lastResult = result;
    m_hasResult  = true;

    m_cardPrice->setValue(result.price,  4);
    m_cardDelta->setValue(result.delta,  4);
    m_cardGamma->setValue(result.gamma,  6);
    m_cardVega ->setValue(result.vega,   2);
    m_cardTheta->setValue(result.theta,  2);
    m_cardRho  ->setValue(result.rho,    4);

    onTabChanged(m_tabWidget->currentIndex());
    addHistoryRow(m_lastRequest, result);

    emit statusMessage(
        QString("Price: %1  |  Δ: %2  |  Γ: %3  |  V: %4  |  Θ: %5")
        .arg(result.price,0,'f',4)
        .arg(result.delta,0,'f',4)
        .arg(result.gamma,0,'f',6)
        .arg(result.vega, 0,'f',2)
        .arg(result.theta,0,'f',2));
}

void PricerWidget::onTabChanged(int index)
{
    if (!m_hasResult) return;
    switch (index) {
        case 0: updateGreekVsSpot();  break;
        case 1: updateVolSmile();     break;
        case 2: updateModelCompare(); break;
    }
}

void PricerWidget::onGreekSelectorChanged(int)
{
    if (m_hasResult) updateGreekVsSpot();
}

void PricerWidget::onModelChanged(int index)
{
    m_hestonPanel->setVisible(index == 2);
}

// ─────────────────────────────────────────────────────────────────────────────
// computeGreek
// ─────────────────────────────────────────────────────────────────────────────
double PricerWidget::computeGreek(int idx, double spot,
                                   const PricingRequest& base)
{
    PricingRequest r = base;
    r.spot  = spot;
    r.model = PricingRequest::Model::BlackScholes;
    auto res = AsyncWorker::staticComputePricing(r);
    if (!res.success) return 0.0;
    switch (idx) {
        case 0: return res.delta;
        case 1: return res.gamma;
        case 2: return res.vega;
        case 3: return res.theta;
    }
    return 0.0;
}

// ─────────────────────────────────────────────────────────────────────────────
// updateGreekVsSpot  —  動態精度 + 顏色
// ─────────────────────────────────────────────────────────────────────────────
void PricerWidget::updateGreekVsSpot()
{
    int    idx  = m_greekSelector->currentIndex();
    const  auto& gs = kGreeks[idx];
    double K    = m_lastRequest.strike;
    double sMin = K * 0.70, sMax = K * 1.30;
    const  int steps = 80;

    QVector<QPointF> pts;
    pts.reserve(steps+1);
    for (int i = 0; i <= steps; ++i) {
        double s = sMin + (sMax-sMin) * i / steps;
        pts.append({s, computeGreek(idx, s, m_lastRequest)});
    }

    // 顏色與名稱
    QPen pen(QColor(gs.hex)); pen.setWidth(2);
    m_greekSeries->setPen(pen);
    m_greekSeries->setName(gs.name);
    m_greekSeries->replace(pts);

    // X 軸
    auto axX = m_greekChart->axes(Qt::Horizontal);
    if (!axX.isEmpty())
        qobject_cast<QValueAxis*>(axX.first())->setRange(sMin, sMax);

    // Y 軸：動態精度
    double yMin = pts[0].y(), yMax = pts[0].y();
    for (const auto& p : pts) {
        yMin = qMin(yMin, p.y());
        yMax = qMax(yMax, p.y());
    }
    int dec = autoDecimals(yMin, yMax);
    QString fmt = QString("%.%1f").arg(dec);

    auto axY = m_greekChart->axes(Qt::Vertical);
    if (!axY.isEmpty()) {
        auto* ay = qobject_cast<QValueAxis*>(axY.first());
        double margin = (yMax-yMin) * 0.12;
        ay->setRange(yMin-margin, yMax+margin);
        ay->setLabelFormat(fmt);
    }

    // 更新 crosshair 精度
    m_greekView->setYFormat(dec, gs.unit);
}

// ─────────────────────────────────────────────────────────────────────────────
// updateVolSmile
// ─────────────────────────────────────────────────────────────────────────────
void PricerWidget::updateVolSmile()
{
    double spot = m_lastRequest.spot;
    double kMin = spot * 0.75, kMax = spot * 1.25;
    const  int steps = 16;

    QVector<QPointF> callPts, putPts;
    callPts.reserve(steps+1); putPts.reserve(steps+1);

    for (int i = 0; i <= steps; ++i) {
        double K = kMin + (kMax-kMin) * i / steps;
        PricingRequest req = m_lastRequest;
        req.strike = K;
        req.model  = PricingRequest::Model::BlackScholes;

        auto implVol = [&](PricingRequest::OptionType ot) -> double {
            req.optionType = ot;
            double mkt = AsyncWorker::staticComputePricing(req).price;
            double lo=1e-4, hi=5.0;
            for (int it=0; it<60; ++it) {
                double mid=(lo+hi)/2.0;
                PricingRequest rr=req; rr.volatility=mid;
                double p=AsyncWorker::staticComputePricing(rr).price;
                if(p>mkt) hi=mid; else lo=mid;
                if(hi-lo<1e-6) break;
            }
            return (lo+hi)/2.0*100.0;
        };

        callPts.append({K, implVol(PricingRequest::OptionType::Call)});
        putPts .append({K, implVol(PricingRequest::OptionType::Put)});
    }

    m_smileSeriesC->replace(callPts);
    m_smileSeriesP->replace(putPts);

    auto axX = m_smileChart->axes(Qt::Horizontal);
    if (!axX.isEmpty())
        qobject_cast<QValueAxis*>(axX.first())->setRange(kMin, kMax);

    double yMin=999, yMax=-999;
    for (const auto& p : callPts+putPts) {
        yMin=qMin(yMin,p.y()); yMax=qMax(yMax,p.y());
    }
    auto axY = m_smileChart->axes(Qt::Vertical);
    if (!axY.isEmpty())
        qobject_cast<QValueAxis*>(axY.first())
            ->setRange(yMin*0.95, yMax*1.05);
}

// ─────────────────────────────────────────────────────────────────────────────
// updateModelCompare
// ─────────────────────────────────────────────────────────────────────────────
void PricerWidget::updateModelCompare()
{
    double K=m_lastRequest.strike;
    double sMin=K*0.70, sMax=K*1.30;
    const  int steps=60;

    QVector<QPointF> bsmPts, hPts;
    bsmPts.reserve(steps+1); hPts.reserve(steps+1);

    for (int i=0; i<=steps; ++i) {
        double s = sMin+(sMax-sMin)*i/steps;

        PricingRequest br=m_lastRequest; br.spot=s;
        br.model=PricingRequest::Model::BlackScholes;
        auto br_= AsyncWorker::staticComputePricing(br);
        if(br_.success) bsmPts.append({s, br_.price});

        PricingRequest hr=m_lastRequest; hr.spot=s;
        hr.model=PricingRequest::Model::Heston;
        hr.v0=m_v0->value(); hr.kappa=m_kappa->value();
        hr.theta_h=m_thetaH->value(); hr.sigma=m_sigmaH->value();
        hr.rho_h=m_rhoH->value();
        auto hr_= AsyncWorker::staticComputePricing(hr);
        if(hr_.success) hPts.append({s, hr_.price});
    }

    m_compareBSM->replace(bsmPts);
    m_compareHeston->replace(hPts);

    auto axX = m_compareChart->axes(Qt::Horizontal);
    if (!axX.isEmpty())
        qobject_cast<QValueAxis*>(axX.first())->setRange(sMin, sMax);

    double yMin=999, yMax=-999;
    for (const auto& p : bsmPts+hPts) {
        yMin=qMin(yMin,p.y()); yMax=qMax(yMax,p.y());
    }
    double mg=(yMax-yMin)*0.1;
    auto axY = m_compareChart->axes(Qt::Vertical);
    if (!axY.isEmpty())
        qobject_cast<QValueAxis*>(axY.first())
            ->setRange(qMax(0.0,yMin-mg), yMax+mg);
}

// ─────────────────────────────────────────────────────────────────────────────
// addHistoryRow
// ─────────────────────────────────────────────────────────────────────────────
void PricerWidget::addHistoryRow(const PricingRequest& req,
                                  const PricingResult& res)
{
    int row=0;
    m_historyTable->insertRow(row);
    auto set=[&](int col, const QString& txt,
                 Qt::Alignment a=Qt::AlignRight){
        auto* it=new QTableWidgetItem(txt);
        it->setTextAlignment(a|Qt::AlignVCenter);
        m_historyTable->setItem(row,col,it);
    };
    bool isCall=(req.optionType==PricingRequest::OptionType::Call);
    set(0,isCall?"Call":"Put",Qt::AlignCenter);
    set(1,QString::number(req.strike,   'f',2));
    set(2,QString::number(static_cast<int>(req.maturityYears*365)));
    set(3,QString::number(req.volatility,'f',3));
    set(4,QString::number(req.riskFree,  'f',3));
    set(5,QString::number(res.price,     'f',4));
    set(6,QString::number(res.delta,     'f',4));
    set(7,QDateTime::currentDateTime().toString("hh:mm:ss"),Qt::AlignCenter);

    QColor bg=isCall?QColor("#1e3a5f"):QColor("#3f1515");
    QColor fg=isCall?QColor("#93c5fd"):QColor("#fca5a5");
    if(auto* it=m_historyTable->item(row,0)){
        it->setBackground(bg); it->setForeground(fg);
    }
    while(m_historyTable->rowCount()>50)
        m_historyTable->removeRow(m_historyTable->rowCount()-1);
}

// ─────────────────────────────────────────────────────────────────────────────
// currentRequest / setLoading
// ─────────────────────────────────────────────────────────────────────────────
PricingRequest PricerWidget::currentRequest() const
{
    PricingRequest req;
    req.spot          = m_spot->value();
    req.strike        = m_strike->value();
    req.volatility    = m_vol->value();
    req.riskFree      = m_rate->value();
    req.dividendYield = m_div->value();
    req.maturityYears = m_maturity->value()/365.0;
    req.model = static_cast<PricingRequest::Model>(m_model->currentIndex());
    req.optionType = (m_typeGroup->checkedId()==0)
                     ? PricingRequest::OptionType::Call
                     : PricingRequest::OptionType::Put;
    if (req.model==PricingRequest::Model::Heston) {
        req.v0=m_v0->value(); req.kappa=m_kappa->value();
        req.theta_h=m_thetaH->value(); req.sigma=m_sigmaH->value();
        req.rho_h=m_rhoH->value();
    }
    return req;
}

void PricerWidget::setLoading(bool on)
{
    m_calcBtn->setEnabled(!on);
    m_calcBtn->setText(on?"Calculating...":"Calculate");
    if(on)
        for(auto* c:{m_cardPrice,m_cardDelta,m_cardGamma,
                     m_cardVega, m_cardTheta,m_cardRho})
            c->clear();
}
