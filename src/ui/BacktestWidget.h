#pragma once
#include <QWidget>
#include "../infra/AsyncWorker.h"
#include "../infra/DatabaseManager.h"

QT_BEGIN_NAMESPACE
class QComboBox;
class QDoubleSpinBox;
class QSpinBox;
class QPushButton;
class QLabel;
class QProgressBar;
class QTableWidget;
QT_END_NAMESPACE

class QChart;
class QChartView;
class QLineSeries;
class QValueAxis;

// ─────────────────────────────────────────────────────────────────────────────
// MetricCard  —  回測結果指標卡
// ─────────────────────────────────────────────────────────────────────────────
class MetricCard : public QWidget
{
    Q_OBJECT
public:
    explicit MetricCard(const QString& label, QWidget* parent = nullptr);
    void setValue(const QString& val, const QString& sub = "", bool positive = true);
    void clear();
private:
    QLabel* m_label = nullptr;
    QLabel* m_value = nullptr;
    QLabel* m_sub   = nullptr;
};

// ─────────────────────────────────────────────────────────────────────────────
// BacktestWidget
// ─────────────────────────────────────────────────────────────────────────────
class BacktestWidget : public QWidget
{
    Q_OBJECT

public:
    explicit BacktestWidget(AsyncWorker*     worker,
                            DatabaseManager* db,
                            QWidget*         parent = nullptr);

signals:
    void statusMessage(const QString& msg);

private slots:
    void onRunBacktest();
    void onBacktestFinished(const BacktestResult& result);
    void onProgressUpdated(int pct);

private:
    QWidget* buildParamsPanel();
    QWidget* buildMetricCards();
    QWidget* buildPnlChart();
    QWidget* buildDrawdownChart();
    QWidget* buildTradeLog();
    void     updateDrawdownChart(const BacktestResult& r);

    BacktestRequest currentRequest() const;
    void setRunning(bool on);

    AsyncWorker*     m_worker = nullptr;
    DatabaseManager* m_db     = nullptr;

    // ── Params ────────────────────────────────────────────────────────────────
    QComboBox*      m_strategyCombo = nullptr;  // Strategy selector
    QComboBox*      m_symCombo      = nullptr;
    QDoubleSpinBox* m_strikeOff     = nullptr;   // OTM %
    QDoubleSpinBox* m_wingWidth     = nullptr;   // Iron Condor wing width
    QWidget*        m_wingRow       = nullptr;   // shown only for Iron Condor
    QSpinBox*       m_dte         = nullptr;   // Days to expiry
    QDoubleSpinBox* m_iv          = nullptr;   // Implied vol
    QDoubleSpinBox* m_rfRate      = nullptr;   // Risk-free rate
    QPushButton*    m_runBtn      = nullptr;
    QProgressBar*   m_progress    = nullptr;

    // ── Metric cards ─────────────────────────────────────────────────────────
    MetricCard* m_cardReturn    = nullptr;
    MetricCard* m_cardSharpe    = nullptr;
    MetricCard* m_cardMaxDD     = nullptr;
    MetricCard* m_cardPremium   = nullptr;

    // ── PnL Chart ─────────────────────────────────────────────────────────────
    QChart*      m_chart      = nullptr;
    QChartView*  m_chartView  = nullptr;
    QLineSeries* m_seriesCC   = nullptr;   // Strategy (CC / PP / IC)
    QLineSeries* m_seriesBH   = nullptr;   // Buy & Hold

    // ── Drawdown Chart（Protective Put 專用）─────────────────────────────────
    QChart*      m_ddChart    = nullptr;
    QChartView*  m_ddView     = nullptr;
    QLineSeries* m_ddStrategy = nullptr;   // Strategy drawdown %
    QLineSeries* m_ddBH       = nullptr;   // B&H drawdown %
    QWidget*     m_ddPanel    = nullptr;   // 整個 drawdown panel

    // ── Trade log ─────────────────────────────────────────────────────────────
    QTableWidget* m_tradeLog  = nullptr;
};
