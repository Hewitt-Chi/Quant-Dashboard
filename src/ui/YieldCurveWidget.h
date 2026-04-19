#pragma once
#include <QWidget>
#include "../infra/AsyncWorker.h"

QT_BEGIN_NAMESPACE
class QDoubleSpinBox;
class QSpinBox;
class QPushButton;
class QLabel;
class QTabWidget;
class QTableWidget;
class QComboBox;
QT_END_NAMESPACE

class QChart;
class QChartView;
class QLineSeries;
class QValueAxis;

// ─────────────────────────────────────────────────────────────────────────────
// YieldCurveWidget
//
// 使用 QuantLib PiecewiseYieldCurve bootstrap 殖利率曲線
//
// 功能：
//   - 輸入各期限利率（3M/6M/1Y/2Y/5Y/10Y/30Y）
//   - 支援預設套用（現行 Fed Rate、Flat Curve、Inverted Curve）
//   - 三個圖表 Tab：Spot/Zero、Forward、Discount Factor
//   - 數值表格顯示每個月的精確數字
// ─────────────────────────────────────────────────────────────────────────────
class YieldCurveWidget : public QWidget
{
    Q_OBJECT
public:
    explicit YieldCurveWidget(AsyncWorker* worker, QWidget* parent = nullptr);

signals:
    void statusMessage(const QString& msg);

private slots:
    void onCalculate();
    void onYieldCurveFinished(const YieldCurveResult& result);
    void onPresetChanged(int index);

private:
    QWidget* buildInputPanel();
    QWidget* buildChartTabs();
    QWidget* buildDataTable();

    QWidget* buildSpotForwardTab();
    QWidget* buildDiscountTab();

    void updateCharts(const YieldCurveResult& r);
    void updateTable(const YieldCurveResult& r);

    AsyncWorker* m_worker = nullptr;

    // ── Input ─────────────────────────────────────────────────────────────────
    // 7 standard tenors: 3M 6M 1Y 2Y 5Y 10Y 30Y
    struct TenorInput {
        int             months;
        QString         label;
        QDoubleSpinBox* spin = nullptr;
    };
    QVector<TenorInput> m_tenors;

    QComboBox*   m_presetCombo = nullptr;
    QPushButton* m_calcBtn     = nullptr;

    // ── Tab 1: Spot + Zero ────────────────────────────────────────────────────
    QTabWidget*  m_tabWidget    = nullptr;
    QChart*      m_spotChart    = nullptr;
    QChartView*  m_spotView     = nullptr;
    QLineSeries* m_spotSeries   = nullptr;
    QLineSeries* m_zeroSeries   = nullptr;
    QLineSeries* m_fwdSeries    = nullptr;

    // ── Tab 2: Discount Factor ────────────────────────────────────────────────
    QChart*      m_discChart    = nullptr;
    QChartView*  m_discView     = nullptr;
    QLineSeries* m_discSeries   = nullptr;

    // ── Data Table ────────────────────────────────────────────────────────────
    QTableWidget* m_dataTable   = nullptr;
};
