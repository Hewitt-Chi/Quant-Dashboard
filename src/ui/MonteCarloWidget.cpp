#include "MonteCarloWidget.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QPushButton>
#include <QLabel>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QHeaderView>
#include <QSplitter>

#include <QtCharts/QChart>
#include <QtCharts/QChartView>
#include <QtCharts/QLineSeries>
#include <QtCharts/QAreaSeries>
#include <QtCharts/QValueAxis>
#include <QtCharts/QLegend>

MonteCarloWidget::MonteCarloWidget(AsyncWorker* worker, QWidget* parent)
    : QWidget(parent), m_worker(worker)
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(12,12,12,12);
    root->setSpacing(10);

    root->addWidget(buildParamsPanel());

    auto* splitter = new QSplitter(Qt::Vertical, this);
    splitter->addWidget(buildPathChart());
    splitter->addWidget(buildStatsTable());
    splitter->setStretchFactor(0, 3);
    splitter->setStretchFactor(1, 1);
    root->addWidget(splitter, 1);

    connect(m_worker, &AsyncWorker::monteCarloFinished,
            this, &MonteCarloWidget::onMCFinished);
}

// ─────────────────────────────────────────────────────────────────────────────
QWidget* MonteCarloWidget::buildParamsPanel()
{
    auto* box = new QGroupBox("GBM Simulation Parameters", this);
    auto* row = new QHBoxLayout(box);
    row->setSpacing(12);

    auto addSpin = [&](const QString& lbl, QWidget* spin) {
        row->addWidget(new QLabel(lbl, box));
        row->addWidget(spin);
    };

    m_spot = new QDoubleSpinBox(box);
    m_spot->setRange(1,99999); m_spot->setValue(100); m_spot->setDecimals(2);
    m_spot->setMinimumWidth(80);
    addSpin("Spot S₀:", m_spot);

    m_mu = new QDoubleSpinBox(box);
    m_mu->setRange(-1,2); m_mu->setValue(0.08);
    m_mu->setSingleStep(0.01); m_mu->setDecimals(3);
    m_mu->setMinimumWidth(72);
    m_mu->setToolTip("Annual drift rate (expected return)");
    addSpin("Drift μ:", m_mu);

    m_sigma = new QDoubleSpinBox(box);
    m_sigma->setRange(0.01,5); m_sigma->setValue(0.20);
    m_sigma->setSingleStep(0.01); m_sigma->setDecimals(3);
    m_sigma->setMinimumWidth(72);
    m_sigma->setToolTip("Annual volatility");
    addSpin("Vol σ:", m_sigma);

    m_paths = new QSpinBox(box);
    m_paths->setRange(10,2000); m_paths->setValue(200);
    m_paths->setMinimumWidth(72);
    addSpin("Paths:", m_paths);

    m_steps = new QSpinBox(box);
    m_steps->setRange(10,504); m_steps->setValue(252);
    m_steps->setMinimumWidth(72);
    m_steps->setToolTip("Steps per path (252 = 1 trading year)");
    addSpin("Steps:", m_steps);

    row->addStretch();

    m_runBtn = new QPushButton("Run simulation", box);
    m_runBtn->setMinimumHeight(34); m_runBtn->setMinimumWidth(130);
    m_runBtn->setStyleSheet(
        "QPushButton{background:#2563eb;color:white;border-radius:6px;font-weight:500;}"
        "QPushButton:hover{background:#1d4ed8;}"
        "QPushButton:disabled{background:#475569;}");
    connect(m_runBtn, &QPushButton::clicked, this, &MonteCarloWidget::onSimulate);
    row->addWidget(m_runBtn);

    return box;
}

// ─────────────────────────────────────────────────────────────────────────────
QWidget* MonteCarloWidget::buildPathChart()
{
    auto* box = new QGroupBox("Price paths — percentile bands", this);
    auto* lay = new QVBoxLayout(box);

    m_chart = new QChart;
    m_chart->legend()->setVisible(true);
    m_chart->legend()->setAlignment(Qt::AlignBottom);
    m_chart->legend()->setLabelColor(QColor(180,180,180));
    m_chart->setMargins(QMargins(4,4,4,4));
    m_chart->setBackgroundVisible(false);

    auto mkSeries = [&](const QString& name, const QString& hex,
        int width = 2, bool dash = false) -> QLineSeries* {
            auto* s = new QLineSeries();
            s->setName(name);

            QPen p;                           // 先預設建構
            p.setColor(QColor(hex));          // 再設顏色
            p.setWidth(width);
            if (dash) p.setStyle(Qt::DashLine);
            s->setPen(p);
            return s;
        };

    m_pct5  = mkSeries("5th pct",  "#ef4444", 1, true);
    m_pct25 = mkSeries("25th pct", "#f97316", 1, false);
    m_pct50 = mkSeries("Median",   "#f59e0b", 2, false);
    m_pct75 = mkSeries("75th pct", "#22c55e", 1, false);
    m_pct95 = mkSeries("95th pct", "#3b82f6", 1, true);

    for (auto* s : {m_pct5, m_pct25, m_pct50, m_pct75, m_pct95})
        m_chart->addSeries(s);

    auto mkAxis = [](const QString& t, const QString& f) {
        auto* a = new QValueAxis;
        a->setTitleText(t); a->setLabelFormat(f); a->setTickCount(7);
        a->setLabelsColor(QColor(180,180,180)); a->setTitleBrush(QColor(180,180,180));
        QPen gp(QColor(55,55,55)); gp.setWidth(1); a->setGridLinePen(gp);
        return a;
    };
    auto* axX = mkAxis("Trading day", "%.0f");
    auto* axY = mkAxis("Price",       "%.2f");
    m_chart->addAxis(axX, Qt::AlignBottom);
    m_chart->addAxis(axY, Qt::AlignLeft);
    for (auto* s : {m_pct5, m_pct25, m_pct50, m_pct75, m_pct95}) {
        s->attachAxis(axX); s->attachAxis(axY);
    }

    m_view = new QChartView(m_chart, box);
    m_view->setRenderHint(QPainter::Antialiasing);
    m_view->setMinimumHeight(250);

    auto* hint = new QLabel(
        "Colored bands = 5/25/50/75/95th percentile of all simulated paths", box);
    hint->setStyleSheet("font-size:10px;color:rgba(150,150,150,0.6);");
    lay->addWidget(m_view, 1);
    lay->addWidget(hint);
    return box;
}

// ─────────────────────────────────────────────────────────────────────────────
QWidget* MonteCarloWidget::buildStatsTable()
{
    auto* box = new QGroupBox("Terminal price distribution", this);
    auto* lay = new QVBoxLayout(box);

    m_statsTable = new QTableWidget(1, 7, box);
    m_statsTable->setHorizontalHeaderLabels({
        "S₀", "5th %ile", "25th %ile", "Median", "75th %ile",
        "95th %ile", "E[return]  ± σ"
    });
    m_statsTable->horizontalHeader()->setStretchLastSection(true);
    m_statsTable->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_statsTable->verticalHeader()->setVisible(false);
    m_statsTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_statsTable->setMaximumHeight(80);
    m_statsTable->setShowGrid(false);
    lay->addWidget(m_statsTable);
    return box;
}

// ─────────────────────────────────────────────────────────────────────────────
void MonteCarloWidget::onSimulate()
{
    m_runBtn->setEnabled(false);
    m_runBtn->setText("Simulating...");

    MonteCarloRequest req;
    req.spot   = m_spot->value();
    req.mu     = m_mu->value();
    req.sigma  = m_sigma->value();
    req.paths  = m_paths->value();
    req.steps  = m_steps->value();

    m_worker->submitMonteCarlo(req);
    emit statusMessage(
        QString("Running Monte Carlo: %1 paths × %2 steps...")
        .arg(req.paths).arg(req.steps));
}

// ─────────────────────────────────────────────────────────────────────────────
void MonteCarloWidget::onMCFinished(const MonteCarloResult& result)
{
    m_runBtn->setEnabled(true);
    m_runBtn->setText("Run simulation");

    if (!result.success) {
        emit statusMessage("Monte Carlo error: " + result.errorMsg);
        return;
    }

    const int steps = m_steps->value();
    const double spot = m_spot->value();

    // 把百分位向量化成 QVector<QPointF>
    // 每個 step 的百分位需要從 paths 計算
    // 這裡用 pre-computed 數據建立「時間序列百分位帶」
    // 為了效能，取每 5 個 step 一個點
    const int stride = qMax(1, steps / 100);
    const int nPaths = result.paths.size();

    auto computeBand = [&](double pct) -> QVector<QPointF> {
        QVector<QPointF> pts;
        for (int t = 0; t <= steps; t += stride) {
            QVector<double> vals;
            vals.reserve(nPaths);
            for (const auto& path : result.paths)
                if (t < path.size()) vals.append(path[t]);
            std::sort(vals.begin(), vals.end());
            int idx = static_cast<int>(vals.size() * pct);
            idx = qMin(idx, vals.size()-1);
            pts.append({static_cast<double>(t), vals[idx]});
        }
        return pts;
    };

    m_pct5->replace(computeBand(0.05));
    m_pct25->replace(computeBand(0.25));
    m_pct50->replace(computeBand(0.50));
    m_pct75->replace(computeBand(0.75));
    m_pct95->replace(computeBand(0.95));

    // 更新軸
    auto axX = m_chart->axes(Qt::Horizontal);
    auto axY = m_chart->axes(Qt::Vertical);
    if (!axX.isEmpty())
        qobject_cast<QValueAxis*>(axX.first())->setRange(0, steps);
    if (!axY.isEmpty()) {
        double yMax = result.pct95 * 1.05;
        double yMin = result.pct5  * 0.95;
        qobject_cast<QValueAxis*>(axY.first())->setRange(yMin, yMax);
    }

    // 更新統計表
    auto set = [&](int col, const QString& txt) {
        auto* it = new QTableWidgetItem(txt);
        it->setTextAlignment(Qt::AlignCenter | Qt::AlignVCenter);
        m_statsTable->setItem(0, col, it);
    };
    set(0, QString::number(spot, 'f', 2));
    set(1, QString::number(result.pct5,  'f', 2));
    set(2, QString::number(result.pct25, 'f', 2));
    set(3, QString::number(result.pct50, 'f', 2));
    set(4, QString::number(result.pct75, 'f', 2));
    set(5, QString::number(result.pct95, 'f', 2));
    set(6, QString("%1%  ±  %2%")
           .arg(result.meanReturn*100, 0,'f',2)
           .arg(result.stdReturn*100,  0,'f',2));

    // 中位數行著色
    if (auto* it = m_statsTable->item(0,3))
        it->setForeground(QColor("#f59e0b"));

    emit statusMessage(
        QString("MC done — Median: %1  5%%: %2  95%%: %3  E[r]: %4%  σ: %5%")
        .arg(result.pct50,0,'f',2)
        .arg(result.pct5, 0,'f',2)
        .arg(result.pct95,0,'f',2)
        .arg(result.meanReturn*100, 0,'f',2)
        .arg(result.stdReturn*100,  0,'f',2));
}

void MonteCarloWidget::setParams(double spot, double mu, double sigma)
{
    m_spot->setValue(spot);
    m_mu->setValue(mu);
    m_sigma->setValue(sigma);
}
