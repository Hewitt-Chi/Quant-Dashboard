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
#include <QColor>
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

    // 左欄：上 PnL 圖 + 下 Drawdown 圖
    auto* leftSplit = new QSplitter(Qt::Vertical, this);
    leftSplit->addWidget(buildPnlChart());
    m_ddPanel = buildDrawdownChart();
    m_ddPanel->setVisible(false);   // 預設隱藏，Protective Put 時顯示
    leftSplit->addWidget(m_ddPanel);
    leftSplit->setStretchFactor(0, 3);
    leftSplit->setStretchFactor(1, 2);

    auto* splitter = new QSplitter(Qt::Horizontal, this);
    splitter->addWidget(leftSplit);
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
    auto* box  = new QGroupBox("Strategy Parameters", this);
    auto* grid = new QGridLayout(box);
    grid->setHorizontalSpacing(12);
    grid->setVerticalSpacing(6);

    // ── Strategy 選擇 (col 0) ────────────────────────────────────────────────
    grid->addWidget(new QLabel("Strategy", box), 0, 0);
    m_strategyCombo = new QComboBox(box);
    m_strategyCombo->addItem("Covered Call",   0);
    m_strategyCombo->addItem("Protective Put", 1);
    m_strategyCombo->addItem("Iron Condor",    2);
    m_strategyCombo->setMinimumWidth(130);
    grid->addWidget(m_strategyCombo, 1, 0);

    // ── Symbol (col 1) ────────────────────────────────────────────────────────
    grid->addWidget(new QLabel("Symbol", box), 0, 1);
    m_symCombo = new QComboBox(box);
    for (const auto& sym : m_db->availableSymbols())
        m_symCombo->addItem(sym);
    if (m_symCombo->count() == 0)
        m_symCombo->addItems({"SOXX", "SMH", "QQQI"});
    m_symCombo->setMinimumWidth(90);
    grid->addWidget(m_symCombo, 1, 1);

    auto dbl = [&](const QString& lbl, double mn, double mx,
                   double v, double step, int dec,
                   int col) -> QDoubleSpinBox* {
        grid->addWidget(new QLabel(lbl, box), 0, col);
        auto* sb = new QDoubleSpinBox(box);
        sb->setRange(mn,mx); sb->setValue(v);
        sb->setSingleStep(step); sb->setDecimals(dec);
        sb->setMinimumWidth(90);
        grid->addWidget(sb, 1, col);
        return sb;
    };

    m_strikeOff = dbl("Strike offset %", 0.001, 0.5,  0.05, 0.01, 2, 2);
    m_wingWidth = dbl("Wing width %",    0.01,  0.3,  0.05, 0.01, 2, 3);
    m_iv        = dbl("Implied vol σ",   0.01,  2.0,  0.20, 0.01, 2, 4);
    m_rfRate    = dbl("Risk-free r",     0.0,   0.2,  0.05, 0.005,3, 5);

    grid->addWidget(new QLabel("DTE (days)", box), 0, 6);
    m_dte = new QSpinBox(box);
    m_dte->setRange(7, 90); m_dte->setValue(30);
    m_dte->setMinimumWidth(70);
    grid->addWidget(m_dte, 1, 6);

    // Wing width 只在 Iron Condor 時啟用
    connect(m_strategyCombo, &QComboBox::currentIndexChanged, [this](int idx){
        m_wingWidth->setEnabled(idx == 2);
        m_wingWidth->setToolTip(idx == 2
            ? "Width of the long legs (OTM% beyond short strike)"
            : "Only used for Iron Condor strategy");
    });
    m_wingWidth->setEnabled(false);

    // Run button
    m_runBtn = new QPushButton("Run Backtest", box);
    m_runBtn->setMinimumHeight(36); m_runBtn->setMinimumWidth(120);
    m_runBtn->setStyleSheet(
        "QPushButton{background:#16a34a;color:white;border-radius:6px;font-weight:500;}"
        "QPushButton:hover{background:#15803d;}"
        "QPushButton:disabled{background:#475569;}");
    grid->addWidget(m_runBtn, 1, 7);
    connect(m_runBtn, &QPushButton::clicked,
            this, &BacktestWidget::onRunBacktest);

    grid->setColumnStretch(8, 1);

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
// buildDrawdownChart  —  Protective Put 專用：策略 vs B&H 回撤比較
// ─────────────────────────────────────────────────────────────────────────────
QWidget* BacktestWidget::buildDrawdownChart()
{
    auto* box = new QGroupBox("Drawdown comparison (Protective Put)", this);
    auto* lay = new QVBoxLayout(box);

    auto* infoLbl = new QLabel(
        "Shows how the Put hedge limits downside vs unhedged B&H position.", box);
    infoLbl->setStyleSheet("font-size:11px;color:rgba(150,150,150,0.7);");
    lay->addWidget(infoLbl);

    m_ddChart    = new QChart;
    m_ddChart->legend()->setVisible(true);
    m_ddChart->legend()->setAlignment(Qt::AlignBottom);
    m_ddChart->legend()->setLabelColor(QColor(180,180,180));
    m_ddChart->setMargins(QMargins(4,4,4,4));
    m_ddChart->setBackgroundVisible(false);

    m_ddStrategy = new QLineSeries; m_ddStrategy->setName("Strategy drawdown %");
    m_ddBH       = new QLineSeries; m_ddBH->setName("B&H drawdown %");
    QPen ps(QColor("#3b82f6")); ps.setWidth(2); m_ddStrategy->setPen(ps);
    QPen pb(QColor("#ef4444")); pb.setWidth(1); pb.setStyle(Qt::DashLine);
    m_ddBH->setPen(pb);
    m_ddChart->addSeries(m_ddStrategy);
    m_ddChart->addSeries(m_ddBH);

    auto mkAx = [](const QString& t, const QString& f) {
        auto* a = new QValueAxis;
        a->setTitleText(t); a->setLabelFormat(f); a->setTickCount(6);
        a->setLabelsColor(QColor(180,180,180));
        a->setTitleBrush(QColor(180,180,180));
        QPen gp(QColor(55,55,55)); gp.setWidth(1); a->setGridLinePen(gp);
        return a;
    };
    auto* axX = mkAx("Day",          "%.0f");
    auto* axY = mkAx("Drawdown (%)", "%.1f");
    m_ddChart->addAxis(axX, Qt::AlignBottom);
    m_ddChart->addAxis(axY, Qt::AlignLeft);
    m_ddStrategy->attachAxis(axX); m_ddStrategy->attachAxis(axY);
    m_ddBH->attachAxis(axX);       m_ddBH->attachAxis(axY);

    m_ddView = new QChartView(m_ddChart, box);
    m_ddView->setRenderHint(QPainter::Antialiasing);
    m_ddView->setMinimumHeight(160);
    lay->addWidget(m_ddView);
    return box;
}

void BacktestWidget::updateDrawdownChart(const BacktestResult& r)
{
    // 計算 rolling drawdown（負值，單位 %）
    auto calcDD = [](const QVector<double>& vals) -> QVector<QPointF> {
        QVector<QPointF> pts;
        pts.reserve(vals.size());
        double peak = vals.isEmpty() ? 1.0 : vals.first();
        for (int i = 0; i < vals.size(); ++i) {
            peak = qMax(peak, vals[i]);
            double dd = (peak > 0) ? (vals[i] - peak) / peak * 100.0 : 0.0;
            pts.append({static_cast<double>(i), dd});
        }
        return pts;
    };

    auto stratPts = calcDD(r.portfolioValues);
    auto bhPts    = calcDD(r.buyHoldValues);

    m_ddStrategy->replace(stratPts);
    m_ddBH->replace(bhPts);

    // 軸範圍
    auto axX = m_ddChart->axes(Qt::Horizontal);
    auto axY = m_ddChart->axes(Qt::Vertical);
    if (!axX.isEmpty())
        qobject_cast<QValueAxis*>(axX.first())
            ->setRange(0, r.portfolioValues.size()-1);
    if (!axY.isEmpty()) {
        double yMin = -0.01;
        for (const auto& p : stratPts + bhPts)
            yMin = qMin(yMin, p.y());
        qobject_cast<QValueAxis*>(axY.first())->setRange(yMin * 1.05, 1.0);
    }
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
    req.strategy       = static_cast<BacktestRequest::Strategy>(
                             m_strategyCombo->currentIndex());
    req.strikeOffsetPct= m_strikeOff->value();
    req.wingWidthPct   = m_wingWidth->value();
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

    bool isProtPut2 = (m_strategyCombo->currentIndex() == 1);
    if (isProtPut2) {
        m_cardPremium->setValue(
            QString("$%1").arg(qAbs(result.premiumCollected),0,'f',2),
            "insurance cost",
            false);  // 保護成本 = 負面
    } else {
        m_cardPremium->setValue(
            QString("$%1").arg(result.premiumCollected,0,'f',2),
            "total collected",
            true);
    }

    // ── PnL 圖表 ──────────────────────────────────────────────────────────────
    const auto& vals = result.portfolioValues;
    const auto& bhVals = result.buyHoldValues;
    QVector<QPointF> ccPts, bhPts;
    ccPts.reserve(vals.size());
    bhPts.reserve(vals.size());

    for (int i = 0; i < vals.size(); ++i) {
        ccPts.append({static_cast<double>(i), vals[i]});
        if (i < bhVals.size())
            bhPts.append({static_cast<double>(i), bhVals[i]});
    }

    // 根據策略更新 series 名稱
    switch (result.assignmentEvents.size() > 0 ? 0 : 0) { default: break; }
    QString stratLabel =
        m_strategyCombo->currentIndex() == 0 ? "Covered call" :
        m_strategyCombo->currentIndex() == 1 ? "Protective put" : "Iron condor";
    m_seriesCC->setName(stratLabel);

    m_seriesCC->replace(ccPts);
    m_seriesBH->replace(bhPts);

    // Protective Put：顯示 Drawdown 比較圖
    bool isProtPut = (m_strategyCombo->currentIndex() == 1);
    m_ddPanel->setVisible(isProtPut);
    if (isProtPut) updateDrawdownChart(result);

    // 軸範圍：取 CC + BH 兩條線的合併 min/max
    auto axX = m_chart->axes(Qt::Horizontal);
    auto axY = m_chart->axes(Qt::Vertical);
    if (!axX.isEmpty())
        qobject_cast<QValueAxis*>(axX.first())
            ->setRange(0, vals.size()-1);
    if (!axY.isEmpty()) {
        double yMin = vals.isEmpty() ? 0 : *std::min_element(vals.begin(), vals.end());
        double yMax = vals.isEmpty() ? 1 : *std::max_element(vals.begin(), vals.end());
        if (!bhVals.isEmpty()) {
            yMin = qMin(yMin, *std::min_element(bhVals.begin(), bhVals.end()));
            yMax = qMax(yMax, *std::max_element(bhVals.begin(), bhVals.end()));
        }
        double margin = (yMax-yMin)*0.08;
        qobject_cast<QValueAxis*>(axY.first())
            ->setRange(yMin-margin, yMax+margin);
    }

    // ── Trade Log（使用真實 per-trade premium + assignment 標記）────────────
    m_tradeLog->clearContents();
    m_tradeLog->setRowCount(0);

    int dte    = m_dte->value();
    int trades = result.premiumPerTrade.size();
    for (int i = 0; i < qMin(trades, 200); ++i) {
        int    day    = i * dte;
        double bh     = (day < bhVals.size()) ? bhVals[day] : 0.0;
        double strike = bh * (1.0 + m_strikeOff->value());
        double prem   = result.premiumPerTrade[i];
        bool   assigned = result.assignmentEvents.contains(day + dte);

        int row = m_tradeLog->rowCount();
        m_tradeLog->insertRow(row);

        auto set = [&](int col, const QString& txt,
                       Qt::Alignment a = Qt::AlignCenter) {
            auto* it = new QTableWidgetItem(txt);
            it->setTextAlignment(a | Qt::AlignVCenter);
            m_tradeLog->setItem(row, col, it);
        };
        set(0, QString::number(day));
        set(1, QString::number(bh,    'f', 2));
        set(2, QString::number(strike,'f', 2));
        set(3, QString::number(prem,  'f', 4));

        // Assignment 行標紅色背景
        if (assigned) {
            for (int c = 0; c < 4; ++c) {
                if (auto* it = m_tradeLog->item(row, c)) {
                    it->setBackground(QColor("#3f1515"));
                    it->setForeground(QColor("#fca5a5"));
                }
            }
        }
    }

    // assignment 次數摘要
    int assignCount = result.assignmentEvents.size();
    QString assignStr = assignCount > 0
        ? QString("  ·  Assigned %1×").arg(assignCount)
        : "  ·  No assignments";

    emit statusMessage(
        QString("Backtest done — Return: %1%  Sharpe: %2  MaxDD: %3%%4")
        .arg(ret,0,'f',2)
        .arg(result.sharpeRatio,0,'f',3)
        .arg(dd,0,'f',2)
        .arg(assignStr));
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
