#pragma once
#include <QWidget>
#include "../infra/AsyncWorker.h"

QT_BEGIN_NAMESPACE
class QDoubleSpinBox;
class QSpinBox;
class QPushButton;
class QLabel;
class QTableWidget;
class QComboBox;
QT_END_NAMESPACE

class QChart;
class QChartView;
class QLineSeries;
class QScatterSeries;

// ─────────────────────────────────────────────────────────────────────────────
// OptionChainWidget
//
// 顯示完整的選擇權鏈（Option Chain）：
//   - 左側：Call 的 Price/Delta/IV
//   - 中間：Strike（ATM 標黃）
//   - 右側：Put 的 Price/Delta/IV
//   - 下方：Call/Put IV smile 折線圖
// ─────────────────────────────────────────────────────────────────────────────
class OptionChainWidget : public QWidget
{
    Q_OBJECT
public:
    explicit OptionChainWidget(AsyncWorker* worker, QWidget* parent = nullptr);

    // 從外部同步參數（PricerWidget 計算後可呼叫）
    void setParams(double spot, double riskFree, double vol, double matDays);

signals:
    void statusMessage(const QString& msg);

private slots:
    void onCalculate();
    void onChainFinished(const OptionChainResult& result);

private:
    QWidget* buildParamsPanel();
    QWidget* buildChainTable();
    QWidget* buildSmileChart();

    AsyncWorker* m_worker = nullptr;

    // Params
    QDoubleSpinBox* m_spot    = nullptr;
    QDoubleSpinBox* m_rf      = nullptr;
    QDoubleSpinBox* m_vol     = nullptr;
    QSpinBox*       m_days    = nullptr;
    QSpinBox*       m_steps   = nullptr;
    QPushButton*    m_calcBtn = nullptr;

    // Table
    QTableWidget*   m_table   = nullptr;

    // Smile chart
    QChart*      m_smileChart  = nullptr;
    QChartView*  m_smileView   = nullptr;
    QLineSeries* m_callSmile   = nullptr;
    QLineSeries* m_putSmile    = nullptr;
};
