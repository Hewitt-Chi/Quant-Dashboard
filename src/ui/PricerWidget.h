#pragma once
#include <QWidget>
#include <QVector>
#include <QtCharts/QChartView>
#include "../infra/AsyncWorker.h"

QT_BEGIN_NAMESPACE
class QDoubleSpinBox;
class QSpinBox;
class QComboBox;
class QLabel;
class QPushButton;
class QTableWidget;
class QButtonGroup;
class QTabWidget;
class QGraphicsLineItem;
class QGraphicsSimpleTextItem;
QT_END_NAMESPACE

class QChart;
class QLineSeries;
class QChartView;
class QValueAxis;

// ─────────────────────────────────────────────────────────────────────────────
// ResultCard
// ─────────────────────────────────────────────────────────────────────────────
class ResultCard : public QWidget
{
    Q_OBJECT
public:
    explicit ResultCard(const QString& label, QWidget* parent = nullptr);
    void setValue(double v, int decimals = 4);
    void setColor(const QString& hex);   // 設定數值顏色
    void clear();
private:
    QLabel* m_label = nullptr;
    QLabel* m_value = nullptr;
};

// ─────────────────────────────────────────────────────────────────────────────
// CrosshairChartView  —  帶十字準線 + Tooltip 的 QChartView
// ─────────────────────────────────────────────────────────────────────────────
class CrosshairChartView : public QChartView
{
    Q_OBJECT
public:
    explicit CrosshairChartView(QChart* chart, QWidget* parent = nullptr);

    // 設定 Y 值的精度與單位（例如 Gamma 需要 6 位，Theta 顯示負號）
    void setYFormat(int decimals, const QString& unit = "");

protected:
    void mouseMoveEvent(QMouseEvent* event) override;
    void leaveEvent(QEvent* event) override;

private:
    void updateCrosshair(const QPointF& scenePos);
    void hideCrosshair();

    QGraphicsLineItem*       m_vLine   = nullptr;
    QGraphicsLineItem*       m_hLine   = nullptr;
    QGraphicsSimpleTextItem* m_tooltip = nullptr;
    QGraphicsSimpleTextItem* m_bgRect  = nullptr;   // tooltip 背景（用矩形模擬）

    int     m_decimals = 4;
    QString m_unit;
};

// ─────────────────────────────────────────────────────────────────────────────
// PricerWidget
// ─────────────────────────────────────────────────────────────────────────────
class PricerWidget : public QWidget
{
    Q_OBJECT

public:
    explicit PricerWidget(AsyncWorker* worker, QWidget* parent = nullptr);

signals:
    void statusMessage(const QString& msg);

private slots:
    void onCalculate();
    void onPricingFinished(const PricingResult& result);
    void onModelChanged(int index);
    void onGreekSelectorChanged(int index);
    void onTabChanged(int index);

private:
    QWidget* buildInputPanel();
    QWidget* buildResultCards();
    QWidget* buildChartTabs();
    QWidget* buildHistoryTable();

    QWidget* buildGreekVsSpotTab();
    QWidget* buildVolSmileTab();
    QWidget* buildModelCompareTab();

    void updateGreekVsSpot();
    void updateVolSmile();
    void updateModelCompare();

    void addHistoryRow(const PricingRequest& req, const PricingResult& res);
    PricingRequest currentRequest() const;
    void setLoading(bool on);
    double computeGreek(int greekIdx, double spot, const PricingRequest& base);

    // Y 軸動態精度：根據數值範圍決定小數位數
    static int autoDecimals(double rangeMin, double rangeMax);

    AsyncWorker* m_worker = nullptr;

    // ── Input ─────────────────────────────────────────────────────────────────
    QDoubleSpinBox* m_spot     = nullptr;
    QDoubleSpinBox* m_strike   = nullptr;
    QDoubleSpinBox* m_vol      = nullptr;
    QDoubleSpinBox* m_rate     = nullptr;
    QDoubleSpinBox* m_div      = nullptr;
    QSpinBox*       m_maturity = nullptr;
    QComboBox*      m_model    = nullptr;
    QButtonGroup*   m_typeGroup= nullptr;
    QPushButton*    m_calcBtn  = nullptr;

    QWidget*        m_hestonPanel = nullptr;
    QDoubleSpinBox* m_v0      = nullptr;
    QDoubleSpinBox* m_kappa   = nullptr;
    QDoubleSpinBox* m_thetaH  = nullptr;
    QDoubleSpinBox* m_sigmaH  = nullptr;
    QDoubleSpinBox* m_rhoH    = nullptr;

    // ── Result cards ──────────────────────────────────────────────────────────
    ResultCard* m_cardPrice = nullptr;
    ResultCard* m_cardDelta = nullptr;
    ResultCard* m_cardGamma = nullptr;
    ResultCard* m_cardVega  = nullptr;
    ResultCard* m_cardTheta = nullptr;
    ResultCard* m_cardRho   = nullptr;

    // ── Tabs ──────────────────────────────────────────────────────────────────
    QTabWidget*          m_tabWidget     = nullptr;
    QComboBox*           m_greekSelector = nullptr;

    // Tab 1
    QChart*              m_greekChart    = nullptr;
    CrosshairChartView*  m_greekView     = nullptr;
    QLineSeries*         m_greekSeries   = nullptr;

    // Tab 2
    QChart*              m_smileChart    = nullptr;
    CrosshairChartView*  m_smileView     = nullptr;
    QLineSeries*         m_smileSeriesC  = nullptr;
    QLineSeries*         m_smileSeriesP  = nullptr;

    // Tab 3
    QChart*              m_compareChart  = nullptr;
    CrosshairChartView*  m_compareView   = nullptr;
    QLineSeries*         m_compareBSM    = nullptr;
    QLineSeries*         m_compareHeston = nullptr;

    // ── History ───────────────────────────────────────────────────────────────
    QTableWidget* m_historyTable = nullptr;

    PricingResult  m_lastResult;
    PricingRequest m_lastRequest;
    bool           m_hasResult = false;
};
