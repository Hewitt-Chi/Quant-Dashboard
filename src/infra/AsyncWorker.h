#pragma once
#include <QObject>
#include <QFuture>
#include <QtConcurrent/QtConcurrent>
#include <functional>

// ─────────────────────────────────────────────────────────────────────────────
// PricingResult  — 定價計算的輸出結構
// ─────────────────────────────────────────────────────────────────────────────
struct PricingResult
{
    bool   success = false;
    QString errorMsg;

    double price = 0.0;

    // Greeks
    double delta = 0.0;
    double gamma = 0.0;
    double vega  = 0.0;
    double theta = 0.0;
    double rho   = 0.0;
};

// ─────────────────────────────────────────────────────────────────────────────
// PricingRequest — 定價計算的輸入參數
// ─────────────────────────────────────────────────────────────────────────────
struct PricingRequest
{
    enum class OptionType { Call, Put };
    enum class Model { BlackScholes, Binomial, Heston };

    double spot       = 100.0;
    double strike     = 100.0;
    double riskFree   = 0.05;
    double volatility = 0.20;
    double dividendYield = 0.0;
    double maturityYears = 1.0;

    OptionType optionType = OptionType::Call;
    Model      model      = Model::BlackScholes;

    // Binomial 步數（model == Binomial 時使用）
    int binomialSteps = 200;

    // Heston 參數（model == Heston 時使用）
    double v0      = 0.04;   // 初始變異數
    double kappa   = 1.5;    // 均值回歸速率
    double theta_h = 0.04;   // 長期均值變異數
    double sigma   = 0.3;    // 波動率的波動率
    double rho_h   = -0.7;   // 相關係數
};

// ─────────────────────────────────────────────────────────────────────────────
// BacktestRequest / BacktestResult
// ─────────────────────────────────────────────────────────────────────────────
struct BacktestRequest
{
    QString symbol;
    QVector<double> closePrices;   // 按時間排序的收盤價

    // Covered Call 參數
    double strikeOffsetPct = 0.05; // OTM 百分比，例如 0.05 = 5% OTM
    int    dteDays         = 30;   // Days to expiration
    double impliedVol      = 0.20;
    double riskFreeRate    = 0.05;
};

struct BacktestResult
{
    bool   success = false;
    QString errorMsg;

    QVector<double> portfolioValues;  // 每期組合淨值
    double totalReturn  = 0.0;
    double sharpeRatio  = 0.0;
    double maxDrawdown  = 0.0;
    double premiumCollected = 0.0;    // 累計 premium
};

// ─────────────────────────────────────────────────────────────────────────────
// AsyncWorker
//
// 使用 QtConcurrent 在背景執行緒執行 QuantLib 計算，
// 完成後透過 Qt signal 回傳結果，安全地跨執行緒更新 UI。
//
// 使用範例：
//   auto* worker = new AsyncWorker(this);
//   connect(worker, &AsyncWorker::pricingFinished,
//           this,   &MyWidget::onPricingDone);
//   worker->submitPricing(req);
// ─────────────────────────────────────────────────────────────────────────────
class AsyncWorker : public QObject
{
    Q_OBJECT

public:
    explicit AsyncWorker(QObject* parent = nullptr);
    ~AsyncWorker();

    // 提交定價計算（非阻塞）
    void submitPricing(const PricingRequest& req);

    // 提交回測（非阻塞）
    void submitBacktest(const BacktestRequest& req);

    // 取消所有進行中的工作
    void cancelAll();

    // 是否有工作正在執行
    bool isBusy() const;

signals:
    void pricingFinished(const PricingResult& result);
    void backtestFinished(const BacktestResult& result);
    void progressUpdated(int percent);   // 回測進度 0–100

private:
    // 靜態計算函式（在 worker thread 執行，無 Qt 物件限制）
    static PricingResult  computePricing(PricingRequest req);
    static BacktestResult computeBacktest(BacktestRequest req,
                                          AsyncWorker* self);

    QFutureWatcher<PricingResult>*  m_pricingWatcher  = nullptr;
    QFutureWatcher<BacktestResult>* m_backtestWatcher = nullptr;
};
