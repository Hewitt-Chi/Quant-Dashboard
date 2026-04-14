#pragma once
#include <QWidget>
#include <QVector>
#include "../infra/AsyncWorker.h"

QT_BEGIN_NAMESPACE
class QDoubleSpinBox;
class QSpinBox;
class QComboBox;
class QLabel;
class QPushButton;
class QTableWidget;
class QButtonGroup;
QT_END_NAMESPACE

class QChart;
class QLineSeries;
class QChartView;
class QValueAxis;

// ─────────────────────────────────────────────────────────────────────────────
// ResultCard  —  單一 Greek 值的顯示卡片
// ─────────────────────────────────────────────────────────────────────────────
class ResultCard : public QWidget
{
    Q_OBJECT
public:
    explicit ResultCard(const QString& label, QWidget* parent = nullptr);
    void setValue(double v, int decimals = 4);
    void setHighlight(bool red);   // Theta 用紅色，Price 用藍色
    void clear();

private:
    QLabel* m_label = nullptr;
    QLabel* m_value = nullptr;
};

// ─────────────────────────────────────────────────────────────────────────────
// PricerWidget  —  Option Pricer 主頁面
//
// 佈局：
//   ┌─ 上半：輸入表單 + Greeks 結果卡 ─────────────────┐
//   │  [Spot] [Strike] [Maturity] [σ] [r] [Model] [Type]│
//   │  [Calculate]                                       │
//   │  Price  Delta  Gamma  Vega  Theta  Rho             │
//   ├─ 下半：Greeks 曲線圖 + 歷史計算記錄 ─────────────┤
//   │  [Delta vs Spot chart]  |  [Recent table]          │
//   └────────────────────────────────────────────────────┘
// ─────────────────────────────────────────────────────────────────────────────
class PricerWidget : public QWidget
{
    Q_OBJECT

public:
    explicit PricerWidget(AsyncWorker* worker, QWidget* parent = nullptr);

signals:
    void statusMessage(const QString& msg);   // 傳給 MainWindow status bar

private slots:
    void onCalculate();
    void onPricingFinished(const PricingResult& result);
    void onModelChanged(int index);

private:
    // ── Build helpers ─────────────────────────────────────────────────────────
    QWidget*  buildInputPanel();
    QWidget*  buildResultCards();
    QWidget*  buildGreeksChart();
    QWidget*  buildHistoryTable();

    // ── Greeks chart ──────────────────────────────────────────────────────────
    void updateDeltaChart();   // 畫 Delta vs Spot 曲線

    // ── History table ─────────────────────────────────────────────────────────
    void addHistoryRow(const PricingRequest& req, const PricingResult& res);

    // ── 從 UI 讀取當前請求參數 ────────────────────────────────────────────────
    PricingRequest currentRequest() const;

    // ── 套用/移除 loading 狀態 ────────────────────────────────────────────────
    void setLoading(bool on);

    // ── Worker 引用（不擁有，由 MainWindow 管理生命週期）─────────────────────
    AsyncWorker* m_worker = nullptr;

    // ── 輸入控件 ──────────────────────────────────────────────────────────────
    QDoubleSpinBox* m_spot       = nullptr;
    QDoubleSpinBox* m_strike     = nullptr;
    QDoubleSpinBox* m_vol        = nullptr;
    QDoubleSpinBox* m_rate       = nullptr;
    QDoubleSpinBox* m_div        = nullptr;
    QSpinBox*       m_maturity   = nullptr;   // 天數
    QComboBox*      m_model      = nullptr;
    QButtonGroup*   m_typeGroup  = nullptr;   // Call / Put toggle
    QPushButton*    m_calcBtn    = nullptr;

    // Heston 額外參數（model == Heston 時才顯示）
    QWidget*        m_hestonPanel = nullptr;
    QDoubleSpinBox* m_v0          = nullptr;
    QDoubleSpinBox* m_kappa       = nullptr;
    QDoubleSpinBox* m_thetaH      = nullptr;
    QDoubleSpinBox* m_sigmaH      = nullptr;
    QDoubleSpinBox* m_rhoH        = nullptr;

    // ── 結果卡 ────────────────────────────────────────────────────────────────
    ResultCard* m_cardPrice = nullptr;
    ResultCard* m_cardDelta = nullptr;
    ResultCard* m_cardGamma = nullptr;
    ResultCard* m_cardVega  = nullptr;
    ResultCard* m_cardTheta = nullptr;
    ResultCard* m_cardRho   = nullptr;

    // ── Greeks Chart ─────────────────────────────────────────────────────────
    QChart* m_chart = nullptr;
    QChartView* m_chartView = nullptr;
    QLineSeries* m_deltaSeries = nullptr;

    // ── History table ─────────────────────────────────────────────────────────
    QTableWidget* m_historyTable = nullptr;

    // 上一次計算結果（用於更新圖表）
    PricingResult m_lastResult;
    PricingRequest m_lastRequest;
};
