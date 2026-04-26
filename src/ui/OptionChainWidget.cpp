#include "OptionChainWidget.h"
#include "../infra/AppSettings.h"
#include "ChartContextMenu.h"

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
#include <QtCharts/QValueAxis>
#include <QtCharts/QLegend>

OptionChainWidget::OptionChainWidget(AsyncWorker* worker, QWidget* parent)
    : QWidget(parent), m_worker(worker)
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(12,12,12,12);
    root->setSpacing(10);

    root->addWidget(buildParamsPanel());

    auto* splitter = new QSplitter(Qt::Vertical, this);
    splitter->addWidget(buildChainTable());
    splitter->addWidget(buildSmileChart());
    splitter->setStretchFactor(0, 3);
    splitter->setStretchFactor(1, 2);
    root->addWidget(splitter, 1);

    connect(m_worker, &AsyncWorker::optionChainFinished,
            this, &OptionChainWidget::onChainFinished);
}

// ─────────────────────────────────────────────────────────────────────────────
QWidget* OptionChainWidget::buildParamsPanel()
{
    auto* box  = new QGroupBox("Parameters", this);
    auto* row  = new QHBoxLayout(box);
    row->setSpacing(12);

    auto addSpin = [&](const QString& lbl, QWidget* spin) {
        row->addWidget(new QLabel(lbl, box));
        row->addWidget(spin);
    };

    m_spot = new QDoubleSpinBox(box);
    m_spot->setRange(0.01,99999); m_spot->setValue(540); m_spot->setDecimals(2);
    m_spot->setMinimumWidth(88);
    addSpin("Spot:", m_spot);

    m_rf = new QDoubleSpinBox(box);
    m_rf->setRange(0,0.5); m_rf->setValue(0.05); m_rf->setDecimals(3);
    m_rf->setSingleStep(0.005); m_rf->setMinimumWidth(72);
    addSpin("Risk-free r:", m_rf);

    m_vol = new QDoubleSpinBox(box);
    m_vol->setRange(0.01,5); m_vol->setValue(0.20); m_vol->setDecimals(3);
    m_vol->setSingleStep(0.01); m_vol->setMinimumWidth(72);
    addSpin("Base vol σ:", m_vol);

    m_days = new QSpinBox(box);
    m_days->setRange(1,3650); m_days->setValue(30);
    m_days->setMinimumWidth(64);
    addSpin("Maturity (d):", m_days);

    m_steps = new QSpinBox(box);
    m_steps->setRange(5,50); m_steps->setValue(20);
    m_steps->setMinimumWidth(56);
    addSpin("Steps:", m_steps);

    row->addStretch();

    m_calcBtn = new QPushButton("Build chain", box);
    m_calcBtn->setMinimumHeight(34); m_calcBtn->setMinimumWidth(110);
    m_calcBtn->setStyleSheet(
        "QPushButton{background:#2563eb;color:white;border-radius:6px;font-weight:500;}"
        "QPushButton:hover{background:#1d4ed8;}"
        "QPushButton:disabled{background:#475569;}");
    connect(m_calcBtn, &QPushButton::clicked, this, &OptionChainWidget::onCalculate);
    row->addWidget(m_calcBtn);

    return box;
}

// ─────────────────────────────────────────────────────────────────────────────
QWidget* OptionChainWidget::buildChainTable()
{
    auto* box = new QGroupBox("Option chain", this);
    auto* lay = new QVBoxLayout(box);

    // 欄位：Call Price | Call Δ | Call IV || STRIKE || Put Price | Put Δ | Put IV | Gamma | Vega | Theta
    m_table = new QTableWidget(0, 10, box);
    m_table->setHorizontalHeaderLabels({
        "Call price","Call Δ","Call IV%",
        "Strike",
        "Put price","Put Δ","Put IV%",
        "Gamma","Vega","Theta"
    });
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_table->verticalHeader()->setVisible(false);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setAlternatingRowColors(true);
    m_table->setShowGrid(false);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);

    lay->addWidget(m_table);

    // 說明
    auto* note = new QLabel(
        "ATM strike highlighted yellow  ·  Call side (left) / Put side (right)", box);
    note->setStyleSheet("font-size:10px;color:rgba(150,150,150,0.6);");
    lay->addWidget(note);

    return box;
}

// ─────────────────────────────────────────────────────────────────────────────
QWidget* OptionChainWidget::buildSmileChart()
{
    auto* box = new QGroupBox("Implied volatility smile", this);
    auto* lay = new QVBoxLayout(box);

    m_smileChart  = new QChart;
    m_smileChart->legend()->setVisible(true);
    m_smileChart->legend()->setAlignment(Qt::AlignBottom);
    m_smileChart->legend()->setLabelColor(QColor(180,180,180));
    m_smileChart->setMargins(QMargins(4,4,4,4));
    m_smileChart->setBackgroundVisible(false);

    m_callSmile = new QLineSeries; m_callSmile->setName("Call IV%");
    m_putSmile  = new QLineSeries; m_putSmile->setName("Put IV%");
    QPen pc(QColor("#3b82f6")); pc.setWidth(2); m_callSmile->setPen(pc);
    QPen pp(QColor("#ef4444")); pp.setWidth(2); m_putSmile->setPen(pp);
    m_smileChart->addSeries(m_callSmile);
    m_smileChart->addSeries(m_putSmile);

    auto mkAx = [](const QString& t, const QString& f) {
        auto* a = new QValueAxis;
        a->setTitleText(t); a->setLabelFormat(f); a->setTickCount(7);
        a->setLabelsColor(QColor(180,180,180));
        a->setTitleBrush(QColor(180,180,180));
        QPen gp(QColor(55,55,55)); gp.setWidth(1); a->setGridLinePen(gp);
        return a;
    };
    auto* axX = mkAx("Strike",        "%.0f");
    auto* axY = mkAx("Implied vol %", "%.2f");
    m_smileChart->addAxis(axX, Qt::AlignBottom);
    m_smileChart->addAxis(axY, Qt::AlignLeft);
    m_callSmile->attachAxis(axX); m_callSmile->attachAxis(axY);
    m_putSmile->attachAxis(axX);  m_putSmile->attachAxis(axY);

    m_smileView = new QChartView(m_smileChart, box);
    m_smileView->setRenderHint(QPainter::Antialiasing);
    m_smileView->setMinimumHeight(180);
    ChartContextMenu::install(m_smileView, "option_chain_iv_smile");
    lay->addWidget(m_smileView);
    return box;
}

// ─────────────────────────────────────────────────────────────────────────────
void OptionChainWidget::onCalculate()
{
    m_calcBtn->setEnabled(false);
    m_calcBtn->setText("Calculating...");

    OptionChainRequest req;
    req.spot          = m_spot->value();
    req.riskFree      = m_rf->value();
    req.baseVol       = m_vol->value();
    req.maturityYears = m_days->value() / 365.0;
    req.strikeSteps   = m_steps->value();
    req.strikeMin     = 0.70;
    req.strikeMax     = 1.30;

    m_worker->submitOptionChain(req);
    emit statusMessage(QString("Building option chain: Spot=%1, σ=%2, T=%3d...")
                       .arg(req.spot,0,'f',2)
                       .arg(req.baseVol,0,'f',3)
                       .arg(m_days->value()));
}

// ─────────────────────────────────────────────────────────────────────────────
void OptionChainWidget::onChainFinished(const OptionChainResult& result)
{
    m_calcBtn->setEnabled(true);
    m_calcBtn->setText("Build chain");

    if (!result.success) {
        emit statusMessage("Option chain error: " + result.errorMsg);
        return;
    }

    // ── 填充表格 ──────────────────────────────────────────────────────────────
    m_table->clearContents();
    m_table->setRowCount(result.rows.size());

    QVector<QPointF> callPts, putPts;
    double atm = 0;

    for (int i = 0; i < result.rows.size(); ++i) {
        const auto& r = result.rows[i];

        auto cell = [&](int col, const QString& txt, bool rightAlign = true) {
            auto* it = new QTableWidgetItem(txt);
            it->setTextAlignment((rightAlign ? Qt::AlignRight : Qt::AlignCenter)
                                 | Qt::AlignVCenter);
            m_table->setItem(i, col, it);
        };

        cell(0, QString::number(r.callPrice, 'f', 4));
        cell(1, QString::number(r.callDelta, 'f', 4));
        cell(2, QString::number(r.callIV,    'f', 2));
        cell(3, QString::number(r.strike,    'f', 2), false);  // Strike 置中
        cell(4, QString::number(r.putPrice,  'f', 4));
        cell(5, QString::number(r.putDelta,  'f', 4));
        cell(6, QString::number(r.putIV,     'f', 2));
        cell(7, QString::number(r.gamma,     'f', 6));
        cell(8, QString::number(r.vega,      'f', 4));
        cell(9, QString::number(r.theta,     'f', 4));

        // ATM 行標黃色
        if (r.isATM) {
            for (int c = 0; c < 10; ++c) {
                if (auto* it = m_table->item(i, c)) {
                    it->setBackground(QColor("#423a00"));
                    it->setForeground(QColor("#fbbf24"));
                }
            }
            atm = r.strike;
        }

        // Call 側（ITM call = 深藍色）
        if (r.callDelta > 0.5) {
            for (int c = 0; c < 3; ++c)
                if (auto* it = m_table->item(i, c))
                    it->setForeground(QColor("#93c5fd"));
        }
        // Put 側（ITM put = 深紅色）
        if (r.putDelta < -0.5) {
            for (int c = 4; c < 7; ++c)
                if (auto* it = m_table->item(i, c))
                    it->setForeground(QColor("#fca5a5"));
        }

        callPts.append({r.strike, r.callIV});
        putPts .append({r.strike, r.putIV});
    }

    // ── 更新 Smile 圖 ─────────────────────────────────────────────────────────
    m_callSmile->replace(callPts);
    m_putSmile->replace(putPts);

    // X 軸
    auto axX = m_smileChart->axes(Qt::Horizontal);
    auto axY = m_smileChart->axes(Qt::Vertical);
    if (!callPts.isEmpty() && !axX.isEmpty())
        qobject_cast<QValueAxis*>(axX.first())
            ->setRange(callPts.first().x(), callPts.last().x());
    if (!axY.isEmpty()) {
        double yMin = 999, yMax = -999;
        for (const auto& p : callPts + putPts) {
            yMin = qMin(yMin, p.y()); yMax = qMax(yMax, p.y());
        }
        qobject_cast<QValueAxis*>(axY.first())
            ->setRange(yMin * 0.95, yMax * 1.05);
    }

    emit statusMessage(
        QString("Option chain built: %1 strikes | ATM=%2 | Call IV=%3% | Put IV=%4%")
        .arg(result.rows.size())
        .arg(atm,0,'f',2)
        .arg(result.rows.isEmpty() ? 0 : result.rows[result.rows.size()/2].callIV, 0,'f',2)
        .arg(result.rows.isEmpty() ? 0 : result.rows[result.rows.size()/2].putIV,  0,'f',2));
}

// ─────────────────────────────────────────────────────────────────────────────
void OptionChainWidget::setParams(double spot, double riskFree,
                                   double vol, double matDays)
{
    m_spot->setValue(spot);
    m_rf->setValue(riskFree);
    m_vol->setValue(vol);
    m_days->setValue(static_cast<int>(matDays));
}
