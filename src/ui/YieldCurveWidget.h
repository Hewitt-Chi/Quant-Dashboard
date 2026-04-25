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

class YieldCurveWidget : public QWidget
{
    Q_OBJECT
public:
    explicit YieldCurveWidget(AsyncWorker* worker, QWidget* parent = nullptr);

signals:
    void statusMessage(const QString& msg);

private slots:
    void onCalculate();
    void onNelsonSiegelFinished(const NelsonSiegelResult& result);
    void onFitNelsonSiegel();
    void onYieldCurveFinished(const YieldCurveResult& result);
    void onPresetChanged(int index);
    void onExportCsv();
    void onApplyShift();     // 情境分析：套用 shift 後重新 bootstrap

private:
    QWidget* buildInputPanel();
    QWidget* buildChartTabs();
    QWidget* buildDataTable();
    QWidget* buildSpotForwardTab();
    QWidget* buildDiscountTab();
    void updateCharts(const YieldCurveResult& r);
    void updateTable(const YieldCurveResult& r);
    QWidget* buildScenarioPanel();              // 情境分析面板
    void     addScenarioSeries(const YieldCurveResult& r,
                                const QString& name,
                                const QColor& color);  // 加疊加線

    AsyncWorker* m_worker = nullptr;

    struct TenorInput {
        int             months;
        QString         label;
        QDoubleSpinBox* spin = nullptr;
    };
    QVector<TenorInput> m_tenors;

    QComboBox*   m_presetCombo = nullptr;
    QPushButton* m_calcBtn     = nullptr;
    QPushButton* m_exportBtn   = nullptr;   // Export CSV

    // Tab 1: Spot + Zero + Forward
    QTabWidget*  m_tabWidget    = nullptr;
    QChart*      m_spotChart    = nullptr;
    QChartView*  m_spotView     = nullptr;
    QLineSeries* m_spotSeries   = nullptr;
    QLineSeries* m_zeroSeries   = nullptr;
    QLineSeries* m_fwdSeries    = nullptr;    // raw forward (鋸齒)
    QLineSeries* m_fwdSmoothed  = nullptr;    // 3M 平滑版

    // Tab 2: Discount Factor
    QChart*      m_discChart    = nullptr;
    QChartView*  m_discView     = nullptr;
    QLineSeries* m_discSeries   = nullptr;

    // Data Table
    QTableWidget*    m_dataTable  = nullptr;
    YieldCurveResult m_lastResult;

    // 情境分析
    QDoubleSpinBox*  m_parallelShift  = nullptr;   // 平移 bps
    QDoubleSpinBox*  m_twistShort     = nullptr;   // 扭轉：短端 bps
    QDoubleSpinBox*  m_twistLong      = nullptr;   // 扭轉：長端 bps
    QDoubleSpinBox*  m_butterflyMid   = nullptr;   // 蝶形：中端 bps
    QPushButton*     m_applyShiftBtn  = nullptr;

    // 情境疊加曲線（最多 3 條，用不同顏色）
    QLineSeries*     m_scenSpot1  = nullptr;
    QLineSeries*     m_scenSpot2  = nullptr;
    QLineSeries*     m_scenSpot3  = nullptr;
    int              m_scenCount  = 0;

    // Nelson-Siegel 擬合曲線
    QLineSeries*     m_nsSeries   = nullptr;
    QPushButton*     m_nsBtn      = nullptr;
    QLabel*          m_nsParamLbl = nullptr;
};
