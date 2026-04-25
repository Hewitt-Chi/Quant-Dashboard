#pragma once
#include <QObject>
#include <QFuture>
#include <QtConcurrent>
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
// OptionChainRow — 選擇權鏈的單一行（一個 Strike）
// ─────────────────────────────────────────────────────────────────────────────
struct OptionChainRow
{
    double strike      = 0.0;
    // Call
    double callPrice   = 0.0;
    double callDelta   = 0.0;
    double callIV      = 0.0;   // Implied vol %
    // Put
    double putPrice    = 0.0;
    double putDelta    = 0.0;
    double putIV       = 0.0;
    // Shared
    double gamma       = 0.0;
    double vega        = 0.0;
    double theta       = 0.0;
    bool   isATM       = false; // 最接近 Spot 的 Strike
};

struct OptionChainRequest
{
    double spot          = 100.0;
    double riskFree      = 0.05;
    double dividendYield = 0.0;
    double maturityYears = 30.0 / 365.0;
    double baseVol       = 0.20;
    double strikeMin     = 0.70;  // Spot 的倍數
    double strikeMax     = 1.30;
    int    strikeSteps   = 20;    // 幾個 Strike
};

struct OptionChainResult
{
    bool    success = false;
    QString errorMsg;
    QVector<OptionChainRow> rows;
};

// ─────────────────────────────────────────────────────────────────────────────
// VolSurfaceResult — Implied Vol 曲面（Strike × Maturity 二維網格）
// ─────────────────────────────────────────────────────────────────────────────
struct VolSurfaceResult
{
    bool    success = false;
    QString errorMsg;

    QVector<double> strikes;     // X 軸
    QVector<double> maturities;  // Z 軸（年）
    // Y 值：vol[i * strikes.size() + j] = implied vol at maturity[i], strike[j]
    QVector<double> impliedVols;
};

// ─────────────────────────────────────────────────────────────────────────────
// BacktestRequest / BacktestResult
// ─────────────────────────────────────────────────────────────────────────────
// ─────────────────────────────────────────────────────────────────────────────
// YieldCurveRequest / YieldCurveResult
// ─────────────────────────────────────────────────────────────────────────────
struct TenorRate
{
    int    months = 0;     // 期限（月）
    double rate   = 0.0;   // 年化利率（decimal，e.g. 0.05）
};

struct YieldCurveResult
{
    bool    success = false;
    QString errorMsg;

    QVector<double> maturitiesYears;  // X 軸：期限（年）
    QVector<double> spotRates;        // 即期利率（%）
    QVector<double> forwardRates;         // 1M 瞬間遠期利率（%）
    QVector<double> smoothedForwardRates; // 3M 移動平均平滑版
    QVector<double> discountFactors;      // 折現因子
    QVector<double> zeroRates;            // 零息利率（%）
};


// ─────────────────────────────────────────────────────────────────────────────
// MonteCarloResult — 多路徑 Monte Carlo 模擬
// ─────────────────────────────────────────────────────────────────────────────
struct MonteCarloRequest
{
    double spot        = 100.0;
    double mu          = 0.08;    // 年化漂移率
    double sigma       = 0.20;    // 年化波動率
    int    paths       = 200;     // 路徑數量
    int    steps       = 252;     // 每條路徑的步數（交易日）
    double riskFree    = 0.05;
    double strikeOffsetPct = 0.05;
    int    dteDays     = 30;
};

struct MonteCarloResult
{
    bool    success = false;
    QString errorMsg;

    // paths 條 GBM 路徑（每條 steps+1 個點）
    QVector<QVector<double>> paths;

    // 統計
    double meanReturn  = 0.0;
    double stdReturn   = 0.0;
    double pct5        = 0.0;   // 5th percentile（最壞情境）
    double pct25       = 0.0;
    double pct50       = 0.0;   // 中位數
    double pct75       = 0.0;
    double pct95       = 0.0;   // 95th percentile（最佳情境）
};

// ─────────────────────────────────────────────────────────────────────────────
// NelsonSiegelResult — Nelson-Siegel 殖利率曲線模型擬合
// ─────────────────────────────────────────────────────────────────────────────
struct NelsonSiegelResult
{
    bool    success = false;
    QString errorMsg;

    // 擬合參數
    double beta0 = 0.0;   // 長期水準
    double beta1 = 0.0;   // 短期成分
    double beta2 = 0.0;   // 中期駝峰
    double tau   = 1.0;   // 衰退速率

    // 曲線點（與 YieldCurveResult 格式相同，方便疊加）
    QVector<double> maturitiesYears;
    QVector<double> nsRates;          // Nelson-Siegel 擬合利率（%）
    double          fitError = 0.0;   // RMSE vs 輸入 tenor rates
};

// ─────────────────────────────────────────────────────────────────────────────
// BacktestRequest / BacktestResult
// ─────────────────────────────────────────────────────────────────────────────
struct BacktestRequest
{
    enum class Strategy {
        CoveredCall,      // 賣出 OTM Call + 持股
        ProtectivePut,    // 買入 OTM Put + 持股
        IronCondor,       // 賣 OTM Call + 買更 OTM Call + 賣 OTM Put + 買更 OTM Put
        Collar,           // 買 OTM Put + 賣 OTM Call（同時保護下檔 + 犧牲上漲）
        CashSecuredPut    // 賣 OTM Put + 持有現金（等待低價買入）
    };

    QString symbol;
    QVector<double> closePrices;

    Strategy strategy        = Strategy::CoveredCall;
    double strikeOffsetPct   = 0.05;   // OTM% for short legs
    double wingWidthPct      = 0.05;   // Iron Condor: width of long legs
    int    dteDays           = 30;
    double impliedVol        = 0.20;
    double riskFreeRate      = 0.05;
};

struct BacktestResult
{
    bool    success = false;
    QString errorMsg;

    QVector<double> portfolioValues;  // Covered call 每期組合淨值
    QVector<double> buyHoldValues;    // Buy & Hold 每期市值（原始收盤）
    QVector<double> premiumPerTrade;  // 每次賣 call 的實際 premium
    QVector<int>    assignmentEvents; // 被 assigned 的 bar index

    double totalReturn      = 0.0;
    double sharpeRatio      = 0.0;
    double maxDrawdown      = 0.0;
    double premiumCollected = 0.0;
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

    // 提交殖利率曲線計算（非阻塞）
    void submitYieldCurve(const QVector<TenorRate>& tenors, double riskFreeRate);

    // 提交選擇權鏈計算（非阻塞）
    void submitOptionChain(const OptionChainRequest& req);

    // 提交 Implied Vol 曲面計算（非阻塞）
    void submitVolSurface(const OptionChainRequest& req);

    // 提交 Monte Carlo 模擬（非阻塞）
    void submitMonteCarlo(const MonteCarloRequest& req);

    // 提交 Nelson-Siegel 擬合（非阻塞）
    void submitNelsonSiegel(const QVector<TenorRate>& tenors);

    // 取消所有進行中的工作
    void cancelAll();

    // 是否有工作正在執行
    bool isBusy() const;

    // 同步計算（UI thread 少量呼叫用）
    static PricingResult staticComputePricing(const PricingRequest& req)
    { return computePricing(req); }

    static YieldCurveResult staticComputeYieldCurve(const QVector<TenorRate>& t, double rf)
    { return computeYieldCurve(t, rf); }

signals:
    void pricingFinished(const PricingResult& result);
    void backtestFinished(const BacktestResult& result);
    void yieldCurveFinished(const YieldCurveResult& result);
    void optionChainFinished(const OptionChainResult& result);
    void volSurfaceFinished(const VolSurfaceResult& result);
    void monteCarloFinished(const MonteCarloResult& result);
    void nelsonSiegelFinished(const NelsonSiegelResult& result);
    void progressUpdated(int percent);   // 回測進度 0–100

private:
    // 靜態計算函式（在 worker thread 執行，無 Qt 物件限制）
    static PricingResult     computePricing(PricingRequest req);
    static BacktestResult    computeBacktest(BacktestRequest req, AsyncWorker* self);
    static YieldCurveResult   computeYieldCurve(QVector<TenorRate> tenors, double rf);
    static OptionChainResult  computeOptionChain(OptionChainRequest req);
    static VolSurfaceResult      computeVolSurface(OptionChainRequest req);
    static MonteCarloResult      computeMonteCarlo(MonteCarloRequest req);
    static NelsonSiegelResult    computeNelsonSiegel(QVector<TenorRate> tenors);

    QFutureWatcher<PricingResult>*    m_pricingWatcher   = nullptr;
    QFutureWatcher<BacktestResult>*   m_backtestWatcher  = nullptr;
    QFutureWatcher<YieldCurveResult>*   m_yieldWatcher   = nullptr;
    QFutureWatcher<OptionChainResult>*  m_chainWatcher   = nullptr;
    QFutureWatcher<VolSurfaceResult>*    m_surfaceWatcher  = nullptr;
    QFutureWatcher<MonteCarloResult>*    m_mcWatcher       = nullptr;
    QFutureWatcher<NelsonSiegelResult>*  m_nsWatcher       = nullptr;
};
