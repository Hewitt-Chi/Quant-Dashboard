#pragma once
#include <QWidget>
#include "../infra/AsyncWorker.h"

QT_BEGIN_NAMESPACE
class QDoubleSpinBox;
class QSpinBox;
class QPushButton;
class QLabel;
class QTableWidget;
QT_END_NAMESPACE

class QChart;
class QChartView;
class QLineSeries;
class QValueAxis;

// ─────────────────────────────────────────────────────────────────────────────
// MonteCarloWidget
//
// GBM（幾何布朗運動）多路徑 Monte Carlo 模擬
// 使用 QuantLib MersenneTwister + BoxMuller
//
// 顯示：
//   - 上方：所有路徑的折線圖（灰色細線）
//           + 5%/25%/50%/75%/95% 百分位帶
//   - 下方：期末價格分佈統計表
// ─────────────────────────────────────────────────────────────────────────────
class MonteCarloWidget : public QWidget
{
    Q_OBJECT
public:
    explicit MonteCarloWidget(AsyncWorker* worker, QWidget* parent = nullptr);

    // 從 Backtest 頁同步參數
    void setParams(double spot, double mu, double sigma);

signals:
    void statusMessage(const QString& msg);

private slots:
    void onSimulate();
    void onMCFinished(const MonteCarloResult& result);

private:
    QWidget* buildParamsPanel();
    QWidget* buildPathChart();
    QWidget* buildStatsTable();

    AsyncWorker* m_worker = nullptr;

    // Params
    QDoubleSpinBox* m_spot   = nullptr;
    QDoubleSpinBox* m_mu     = nullptr;
    QDoubleSpinBox* m_sigma  = nullptr;
    QSpinBox*       m_paths  = nullptr;
    QSpinBox*       m_steps  = nullptr;
    QPushButton*    m_runBtn = nullptr;

    // Path chart
    QChart*      m_chart     = nullptr;
    QChartView*  m_view      = nullptr;
    QLineSeries* m_pct5      = nullptr;
    QLineSeries* m_pct25     = nullptr;
    QLineSeries* m_pct50     = nullptr;
    QLineSeries* m_pct75     = nullptr;
    QLineSeries* m_pct95     = nullptr;

    // Stats table
    QTableWidget* m_statsTable = nullptr;
};
