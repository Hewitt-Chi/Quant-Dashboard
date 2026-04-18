#include "BacktestWidget.h"
#include "../infra/AppSettings.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QPushButton>
#include <QLabel>
#include <QProgressBar>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QHeaderView>
#include <QSplitter>
#include <QDateTime>
#include <cmath>

#include <QtCharts/QChart>
#include <QtCharts/QChartView>
#include <QtCharts/QLineSeries>
#include <QtCharts/QValueAxis>
#include <QtCharts/QLegend>

// ═════════════════════════════════════════════════════════════════════════════
// MetricCard
// ═════════════════════════════════════════════════════════════════════════════
MetricCard::MetricCard(const QString& label, QWidget* parent)
    : QWidget(parent)
{
    setMinimumWidth(110);
    setStyleSheet(
        "MetricCard{"
        "  background:rgba(128,128,128,0.12);"
        "  border:1px solid rgba(128,128,128,0.25);"
        "  border-radius:8px;"
        "}");

    auto* lay = new QVBoxLayout(this);
    lay->setContentsMargins(14,10,14,10);
    lay->setSpacing(4);

    m_label = new QLabel(label, this);
    m_label->setStyleSheet("font-size:11px;color:rgba(180,180,180,0.7);");

    m_value = new QLabel("—", this);
    m_value->setStyleSheet("font-size:22px;font-weight:500;");

    m_sub = new QLabel("", this);
    m_sub->setStyleSheet("font-size:11px;color:rgba(150,150,150,0.6);");

    lay->addWidget(m_label);
    lay->addWidget(m_value);
    lay->addWidget(m_sub);
}

void MetricCard::setValue(const QString& val, const QString& sub, bool positive)
{
    QString color = positive ? "#22c55e" : "#ef4444";
    m_value->setStyleSheet(
        QString("font-size:22px;font-weight:500;color:%1;").arg(color));
    m_value->setText(val);
    m_sub->setText(sub);
}

void MetricCard::clear()
{
    m_value->setText("—");
    m_value->setStyleSheet("font-size:22px;font-weight:500;");
    m_sub->setText("");
}

// ═════════════════════════════════════════════════════════════════════════════
// BacktestWidget
// ═════════════════════════════════════════════════════════════════════════════
BacktestWidget::BacktestWidget(AsyncWorker*     worker,
                               DatabaseManager* db,
                               QWidget*         parent)
    : QWidget(parent), m_worker(worker), m_db(db)
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(12,12,12,12);
    root->setSpacing(10);

    root->addWidget(buildParamsPanel());
    root->addWidget(buildMetricCards());

    auto* splitter = new QSplitter(Qt::Horizontal, this);
    splitter->addWidget(buildPnlChart());
    splitter->addWidget(buildTradeLog());
    splitter->setStretchFactor(0, 3);
    splitter->setStretchFactor(1, 2);
    root->addWidget(splitter, 1);

    connect(m_worker, &AsyncWorker::backtestFinished,
            this, &BacktestWidget::onBacktestFinished);
    connect(m_worker, &AsyncWorker::progressUpdated,
            this, &BacktestWidget::onProgressUpdated);
}

// ─────────────────────────────────────────────────────────────────────────────
// buildParamsPanel
// ─────────────────────────────────────────────────────────────────────────────
QWidget* BacktestWidget::buildParamsPanel()
{
    auto* box  = new QGroupBox("Covered Call Strategy Parameters", this);
    auto* grid = new QGridLayout(box);
    grid->setHorizontalSpacing(12);
    grid->setVerticalSpacing(6);

    // Symbol
    grid->addWidget(new QLabel("Symbol", box), 0, 0);
    m_symCombo = new QComboBox(box);
    // 從 DB 載入可用的 symbol
    for (const auto& sym : m_db->availableSymbols())
        m_symCombo->addItem(sym);
    if (m_symCombo->count() == 0)
        m_symCombo->addItems({"SOXX", "SMH", "QQQI"});
    m_symCombo->setMinimumWidth(100);
    grid->addWidget(m_symCombo, 1, 0);

    auto dbl = [&](const QString& lbl, double mn, double mx,
                   double v, double step, int dec,
                   int col) -> QDoubleSpinBox* {
        grid->addWidget(new QLabel(lbl, box), 0, col);
        auto* sb = new QDoubleSpinBox(box);
        sb->setRange(mn,mx); sb->setValue(v);
        sb->setSingleStep(step); sb->setDecimals(dec);
        sb->setMinimumWidth(100);
        grid->addWidget(sb, 1, col);
        return sb;
    };

    m_strikeOff = dbl("Strike offset (OTM %)", 0.001, 0.5,  0.05, 0.01, 2, 1);
    m_iv        = dbl("Implied vol (σ)",        0.01,  2.0,  0.20, 0.01, 2, 2);
    m_rfRate    = dbl("Risk-free rate",          0.0,   0.2,  0.05, 0.005,3, 3);

    grid->addWidget(new QLabel("DTE (days)", box), 0, 4);
    m_dte = new QSpinBox(box);
    m_dte->setRange(7, 90); m_dte->setValue(30);
    m_dte->setMinimumWidth(80);
    grid->addWidget(m_dte, 1, 4);

    // Run button
    m_runBtn = new QPushButton("Run Backtest", box);
    m_runBtn->setMinimumHeight(36); m_runBtn->setMinimumWidth(120);
    m_runBtn->setStyleSheet(
        "QPushButton{background:#16a34a;color:white;border-radius:6px;font-weight:500;}"
        "QPushButton:hover{background:#15803d;}"
        "QPushButton:disabled{background:#475569;}");
    grid->addWidget(m_runBtn, 1, 5);
    connect(m_runBtn, &QPushButton::clicked,
            this, &BacktestWidget::onRunBacktest);

    grid->setColumnStretch(6, 1);

    // Progress bar
    m_progress = new QProgressBar(box);
    m_progress->setRange(0, 100);
    m_progress->setValue(0);
    m_progress->setVisible(false);
    m_progress->setMaximumHeight(6);
    m_progress->setStyleSheet(
        "QProgressBar{border:none;background:rgba(128,128,128,0.2);border-radius:3px;}"
        "QProgressBar::chunk{background:#16a34a;border-radius:3px;}");
    grid->addWidget(m_progress, 2, 0, 1, 6);

    return box;
}

// ─────────────────────────────────────────────────────────────────────────────
// buildMetricCards
// ─────────────────────────────────────────────────────────────────────────────
QWidget* BacktestWidget::buildMetricCards()
{
    auto* w   = new QWidget(this);
    auto* row = new QHBoxLayout(w);
    row->setContentsMargins(0,0,0,0); row->setSpacing(8);

    m_cardReturn  = new MetricCard("Total return",        w);
    m_cardSharpe  = new MetricCard("Sharpe ratio",        w);
    m_cardMaxDD   = new MetricCard("Max drawdown",        w);
    m_cardPremium = new MetricCard("Premium collected",   w);

    for (auto* c : {m_cardReturn, m_cardSharpe, m_cardMaxDD, m_cardPremium})
        row->addWidget(c, 1);
    return w;
}

// ─────────────────────────────────────────────────────────────────────────────
// buildPnlChart
// ─────────────────────────────────────────────────────────────────────────────
QWidget* BacktestWidget::buildPnlChart()
{
    auto* box = new QGroupBox("Portfolio value", this);
    auto* lay = new QVBoxLayout(box);

    m_chart = new QChart;
    m_chart->legend()->setVisible(true);
    m_chart->legend()->setAlignment(Qt::AlignBottom);
    m_chart->legend()->setLabelColor(QColor(180,180,180));
    m_chart->setMargins(QMargins(4,4,4,4));
    m_chart->setBackgroundVisible(false);

    m_seriesCC = new QLineSeries; m_seriesCC->setName("Covered call");
    m_seriesBH = new QLineSeries; m_seriesBH->setName("Buy & hold");
    QPen pcc(QColor("#3b82f6")); pcc.setWidth(2); m_seriesCC->setPen(pcc);
    QPen pbh(QColor("#6b7280")); pbh.setWidth(1);
    pbh.setStyle(Qt::DashLine); m_seriesBH->setPen(pbh);
    m_chart->addSeries(m_seriesCC);
    m_chart->addSeries(m_seriesBH);

    auto mkAxis = [](const QString& title, const QString& fmt) {
        auto* a = new QValueAxis;
        a->setTitleText(title);
        a->setLabelFormat(fmt);
        a->setLabelsColor(QColor(180,180,180));
        a->setTitleBrush(QColor(180,180,180));
        QPen gp(QColor(55,55,55)); gp.setWidth(1);
        a->setGridLinePen(gp);
        return a;
    };
    auto* axX = mkAxis("Day",   "%.0f");  axX->setTickCount(7);
    auto* axY = mkAxis("Value", "%.0f");  axY->setTickCount(6);
    m_chart->addAxis(axX, Qt::AlignBottom);
    m_chart->addAxis(axY, Qt::AlignLeft);
    m_seriesCC->attachAxis(axX); m_seriesCC->attachAxis(axY);
    m_seriesBH->attachAxis(axX); m_seriesBH->attachAxis(axY);

    m_chartView = new QChartView(m_chart, box);
    m_chartView->setRenderHint(QPainter::Antialiasing);
    m_chartView->setMinimumHeight(220);
    lay->addWidget(m_chartView);
    return box;
}

// ─────────────────────────────────────────────────────────────────────────────
// buildTradeLog
// ─────────────────────────────────────────────────────────────────────────────
QWidget* BacktestWidget::buildTradeLog()
{
    auto* box = new QGroupBox("Trade log", this);
    auto* lay = new QVBoxLayout(box);

    m_tradeLog = new QTableWidget(0, 4, box);
    m_tradeLog->setHorizontalHeaderLabels({"Day","Spot","Strike","Premium"});
    m_tradeLog->horizontalHeader()->setStretchLastSection(true);
    m_tradeLog->horizontalHeader()
        ->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_tradeLog->verticalHeader()->setVisible(false);
    m_tradeLog->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_tradeLog->setAlternatingRowColors(true);
    m_tradeLog->setShowGrid(false);
    m_tradeLog->setMaximumWidth(360);
    lay->addWidget(m_tradeLog);
    return box;
}

// ─────────────────────────────────────────────────────────────────────────────
// onRunBacktest
// ─────────────────────────────────────────────────────────────────────────────
void BacktestWidget::onRunBacktest()
{
    QString sym = m_symCombo->currentText();

    // 從 DB 讀歷史收盤價
    QDateTime from(QDate(2000,1,1), QTime(0,0));
    QDateTime to   = QDateTime::currentDateTime();
    auto bars = m_db->queryBars(sym, from, to);

    if (bars.size() < 30) {
        emit statusMessage(sym + ": not enough data in DB ("
                           + QString::number(bars.size()) + " bars). "
                           "Please wait for history fetch to complete.");
        return;
    }

    QVector<double> closes;
    closes.reserve(bars.size());
    for (const auto& b : bars) closes.append(b.close);

    BacktestRequest req;
    req.symbol         = sym;
    req.closePrices    = closes;
    req.strikeOffsetPct= m_strikeOff->value();
    req.dteDays        = m_dte->value();
    req.impliedVol     = m_iv->value();
    req.riskFreeRate   = m_rfRate->value();

    setRunning(true);
    m_worker->submitBacktest(req);
    emit statusMessage(QString("Running backtest: %1 (%2 bars)...")
                       .arg(sym).arg(closes.size()));
}

// ─────────────────────────────────────────────────────────────────────────────
// onBacktestFinished
// ─────────────────────────────────────────────────────────────────────────────
void BacktestWidget::onBacktestFinished(const BacktestResult& result)
{
    setRunning(false);

    if (!result.success) {
        emit statusMessage("Backtest error: " + result.errorMsg);
        return;
    }

    // ── 指標卡 ────────────────────────────────────────────────────────────────
    double ret = result.totalReturn * 100.0;
    m_cardReturn->setValue(
        QString("%1%2%").arg(ret>=0?"+":"").arg(ret,0,'f',2),
        "",
        ret >= 0);

    m_cardSharpe->setValue(
        QString::number(result.sharpeRatio,'f',3),
        "annualized",
        result.sharpeRatio >= 0);

    double dd = result.maxDrawdown * 100.0;
    m_cardMaxDD->setValue(
        QString("-%1%").arg(dd,0,'f',2),
        "peak to trough",
        false);

    m_cardPremium->setValue(
        QString("$%1").arg(result.premiumCollected,0,'f',2),
        "total collected",
        true);

    // ── PnL 圖表 ──────────────────────────────────────────────────────────────
    const auto& vals = result.portfolioValues;
    QVector<QPointF> ccPts, bhPts;
    ccPts.reserve(vals.size());
    bhPts.reserve(vals.size());

    double init = vals.isEmpty() ? 1.0 : vals.first();
    // Buy & Hold：從 DB 讀原始收盤，需要從 request 裡拿
    // 這裡簡化：用 portfolio 第一個值 * (close/close[0]) 模擬 B&H
    // 實際上 BacktestResult 可以擴充存 closes，這裡用近似值
    for (int i = 0; i < vals.size(); ++i) {
        ccPts.append({static_cast<double>(i), vals[i]});
        // B&H 近似：假設 portfolio 第一筆是持股市值
        if (i < result.buyHoldValues.size())
            bhPts.append({ static_cast<double>(i), result.buyHoldValues[i] });
    }

    m_seriesCC->replace(ccPts);
    m_seriesBH->replace(bhPts);

    // 更新軸範圍
    auto axX = m_chart->axes(Qt::Horizontal);
    auto axY = m_chart->axes(Qt::Vertical);
    if (!axX.isEmpty())
        qobject_cast<QValueAxis*>(axX.first())
            ->setRange(0, vals.size()-1);
    if (!axY.isEmpty()) {
        double yMin = *std::min_element(vals.begin(), vals.end());
        double yMax = *std::max_element(vals.begin(), vals.end());
        double margin = (yMax-yMin)*0.08;
        qobject_cast<QValueAxis*>(axY.first())
            ->setRange(yMin-margin, yMax+margin);
    }

    // ── 補充 Trade Log（每隔 DTE 天一筆）────────────────────────────────────
    m_tradeLog->clearContents();
    m_tradeLog->setRowCount(0);

    int dte = m_dte->value();
    int trades = vals.size() / dte;
    for (int i = 0; i < qMin(trades, 200); ++i) {
        int   day  = i * dte;
        double spot   = (day < vals.size()) ? vals[day] : 0;
        double strike = spot * (1.0 + m_strikeOff->value());
        double prem = (i < result.premiumPerTrade.size())
            ? result.premiumPerTrade[i]
            : 0.0;

        int row = m_tradeLog->rowCount();
        m_tradeLog->insertRow(row);
        auto set = [&](int col, const QString& txt) {
            auto* it = new QTableWidgetItem(txt);
            it->setTextAlignment(Qt::AlignCenter|Qt::AlignVCenter);
            m_tradeLog->setItem(row, col, it);
        };
        set(0, QString::number(day));
        set(1, QString::number(spot,  'f',2));
        set(2, QString::number(strike,'f',2));
        set(3, QString::number(prem,  'f',4));
    }

    emit statusMessage(
        QString("Backtest done — Return: %1%  Sharpe: %2  MaxDD: %3%")
        .arg(ret,0,'f',2)
        .arg(result.sharpeRatio,0,'f',3)
        .arg(dd,0,'f',2));
}

void BacktestWidget::onProgressUpdated(int pct)
{
    m_progress->setValue(pct);
}

// ─────────────────────────────────────────────────────────────────────────────
BacktestRequest BacktestWidget::currentRequest() const
{
    return {};  // 僅給 onRunBacktest 直接使用，這裡留空
}

void BacktestWidget::setRunning(bool on)
{
    m_runBtn->setEnabled(!on);
    m_runBtn->setText(on ? "Running..." : "Run Backtest");
    m_progress->setVisible(on);
    if (on) m_progress->setValue(0);
    if (on) {
        m_cardReturn->clear(); m_cardSharpe->clear();
        m_cardMaxDD->clear();  m_cardPremium->clear();
    }
}
