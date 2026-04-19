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
    void onYieldCurveFinished(const YieldCurveResult& result);
    void onPresetChanged(int index);
    void onExportCsv();

private:
    QWidget* buildInputPanel();
    QWidget* buildChartTabs();
    QWidget* buildDataTable();
    QWidget* buildSpotForwardTab();
    QWidget* buildDiscountTab();
    void updateCharts(const YieldCurveResult& r);
    void updateTable(const YieldCurveResult& r);

    AsyncWorker* m_worker = nullptr;

    struct TenorInput {
        int             months;
        QString         label;
        QDoubleSpinBox* spin = nullptr;
    };
    QVector<TenorInput> m_tenors;

    QComboBox* m_presetCombo = nullptr;
    QPushButton* m_calcBtn = nullptr;
    QPushButton* m_exportBtn = nullptr;   // Export CSV

    // Tab 1: Spot + Zero + Forward
    QTabWidget* m_tabWidget = nullptr;
    QChart* m_spotChart = nullptr;
    QChartView* m_spotView = nullptr;
    QLineSeries* m_spotSeries = nullptr;
    QLineSeries* m_zeroSeries = nullptr;
    QLineSeries* m_fwdSeries = nullptr;    // raw forward (鋸齒)
    QLineSeries* m_fwdSmoothed = nullptr;    // 3M 平滑版

    // Tab 2: Discount Factor
    QChart* m_discChart = nullptr;
    QChartView* m_discView = nullptr;
    QLineSeries* m_discSeries = nullptr;

    // Data Table
    QTableWidget* m_dataTable = nullptr;
    YieldCurveResult m_lastResult;            // 供 Export CSV 使用
};
