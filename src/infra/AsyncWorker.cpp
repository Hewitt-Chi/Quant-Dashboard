#include "AsyncWorker.h"
#include <QFutureWatcher>
#include <QtConcurrent/QtConcurrent>

// ── QuantLib headers ──────────────────────────────────────────────────────────
#include <ql/quantlib.hpp>

using namespace QuantLib;

// ─────────────────────────────────────────────────────────────────────────────
// 建構 / 解構
// ─────────────────────────────────────────────────────────────────────────────
AsyncWorker::AsyncWorker(QObject* parent)
    : QObject(parent)
{
    // ── Pricing watcher ──────────────────────────────────────────────────────
    m_pricingWatcher = new QFutureWatcher<PricingResult>(this);
    connect(m_pricingWatcher, &QFutureWatcher<PricingResult>::finished,
            this, [this]() {
                emit pricingFinished(m_pricingWatcher->result());
            });

    // ── Backtest watcher ─────────────────────────────────────────────────────
    m_backtestWatcher = new QFutureWatcher<BacktestResult>(this);
    connect(m_backtestWatcher, &QFutureWatcher<BacktestResult>::finished,
            this, [this]() {
                emit backtestFinished(m_backtestWatcher->result());
            });
}

AsyncWorker::~AsyncWorker()
{
    cancelAll();
}

// ─────────────────────────────────────────────────────────────────────────────
// Public API
// ─────────────────────────────────────────────────────────────────────────────
void AsyncWorker::submitPricing(const PricingRequest& req)
{
    if (m_pricingWatcher->isRunning())
        m_pricingWatcher->cancel();

    // 複製 req，在背景執行緒中使用（Qt 不允許跨執行緒共享 QObject）
    auto future = QtConcurrent::run([req]() {
        return computePricing(req);
    });
    m_pricingWatcher->setFuture(future);
}

void AsyncWorker::submitBacktest(const BacktestRequest& req)
{
    if (m_backtestWatcher->isRunning())
        m_backtestWatcher->cancel();

    // 傳入 self 指標用於進度回報（透過 invokeMethod 跨執行緒安全呼叫）
    auto future = QtConcurrent::run([req, this]() {
        return computeBacktest(req, this);
    });
    m_backtestWatcher->setFuture(future);
}

void AsyncWorker::cancelAll()
{
    m_pricingWatcher->cancel();
    m_backtestWatcher->cancel();
    m_pricingWatcher->waitForFinished();
    m_backtestWatcher->waitForFinished();
}

bool AsyncWorker::isBusy() const
{
    return m_pricingWatcher->isRunning() || m_backtestWatcher->isRunning();
}

// ─────────────────────────────────────────────────────────────────────────────
// computePricing — 在 worker thread 執行
// ─────────────────────────────────────────────────────────────────────────────
PricingResult AsyncWorker::computePricing(PricingRequest req)
{
    PricingResult result;
    try {
        // ── 日期設定 ─────────────────────────────────────────────────────────
        Calendar       cal     = NullCalendar();
        Date           today   = Date::todaysDate();
        Date           expiry  = today + static_cast<int>(req.maturityYears * 365);
        DayCounter     dc      = Actual365Fixed();
        Settings::instance().evaluationDate() = today;

        // ── 市場數據 handle（QuantLib 的 Quote 架構）─────────────────────────
        Handle<Quote> spotH(boost::make_shared<SimpleQuote>(req.spot));
        Handle<YieldTermStructure> riskFreeH(
            boost::make_shared<FlatForward>(today, req.riskFree, dc));
        Handle<YieldTermStructure> divH(
            boost::make_shared<FlatForward>(today, req.dividendYield, dc));

        // ── Option type ───────────────────────────────────────────────────────
        Option::Type optType = (req.optionType == PricingRequest::OptionType::Call)
                               ? Option::Call : Option::Put;

        // ── Payoff & Exercise ─────────────────────────────────────────────────
        auto payoff   = boost::make_shared<PlainVanillaPayoff>(optType, req.strike);
        auto exercise = boost::make_shared<EuropeanExercise>(expiry);
        VanillaOption option(payoff, exercise);

        // ── 選擇定價模型 ──────────────────────────────────────────────────────
        if (req.model == PricingRequest::Model::BlackScholes) {

            Handle<BlackVolTermStructure> volH(
                boost::make_shared<BlackConstantVol>(today, cal, req.volatility, dc));

            auto process = boost::make_shared<BlackScholesMertonProcess>(
                spotH, divH, riskFreeH, volH);

            option.setPricingEngine(
                boost::make_shared<AnalyticEuropeanEngine>(process));

        } else if (req.model == PricingRequest::Model::Binomial) {

            Handle<BlackVolTermStructure> volH(
                boost::make_shared<BlackConstantVol>(today, cal, req.volatility, dc));

            auto process = boost::make_shared<BlackScholesMertonProcess>(
                spotH, divH, riskFreeH, volH);

            option.setPricingEngine(
                boost::make_shared<BinomialVanillaEngine<CoxRossRubinstein>>(
                    process, req.binomialSteps));

        } else if (req.model == PricingRequest::Model::Heston) {

            auto hestonProcess = boost::make_shared<HestonProcess>(
                riskFreeH, divH, spotH,
                req.v0, req.kappa, req.theta_h, req.sigma, req.rho_h);
            auto hestonModel = boost::make_shared<HestonModel>(hestonProcess);

            option.setPricingEngine(
                boost::make_shared<AnalyticHestonEngine>(hestonModel));
        }

        // ── 取得結果 ──────────────────────────────────────────────────────────
        result.price = option.NPV();

        // Greeks（Heston analytic engine 也支援）
        result.delta = option.delta();
        result.gamma = option.gamma();
        result.vega  = option.vega();
        result.theta = option.theta();
        result.rho   = option.rho();

        result.success = true;

    } catch (const std::exception& e) {
        result.success  = false;
        result.errorMsg = QString::fromStdString(e.what());
    }
    return result;
}

// ─────────────────────────────────────────────────────────────────────────────
// computeBacktest — Covered Call 策略回測骨架
// ─────────────────────────────────────────────────────────────────────────────
BacktestResult AsyncWorker::computeBacktest(BacktestRequest req,
                                            AsyncWorker*    self)
{
    BacktestResult result;
    if (req.closePrices.size() < 2) {
        result.errorMsg = "Not enough price data";
        return result;
    }

    try {
        DayCounter dc    = Actual365Fixed();
        Calendar   cal   = NullCalendar();
        Date       today = Date::todaysDate();
        Settings::instance().evaluationDate() = today;

        const int n = req.closePrices.size();
        result.portfolioValues.reserve(n);

        double portfolio  = req.closePrices.first(); // 初始持股
        double premium    = 0.0;
        double peakValue  = portfolio;
        double maxDD      = 0.0;
        int    nextExpiry = req.dteDays;             // 下次 call 到期的 index

        for (int i = 0; i < n; ++i) {
            double spot = req.closePrices[i];

            // ── 每隔 dteDays 賣出一次 covered call ───────────────────────────
            if (i == nextExpiry) {
                double strike = spot * (1.0 + req.strikeOffsetPct);
                double T      = req.dteDays / 365.0;

                Handle<Quote>              sH(boost::make_shared<SimpleQuote>(spot));
                Handle<YieldTermStructure> rH(
                    boost::make_shared<FlatForward>(today, req.riskFreeRate, dc));
                Handle<YieldTermStructure> dH(
                    boost::make_shared<FlatForward>(today, 0.0, dc));
                Handle<BlackVolTermStructure> vH(
                    boost::make_shared<BlackConstantVol>(today, cal, req.impliedVol, dc));

                auto process = boost::make_shared<BlackScholesMertonProcess>(sH, dH, rH, vH);
                Date expiry  = today + req.dteDays;
                auto payoff  = boost::make_shared<PlainVanillaPayoff>(Option::Call, strike);
                auto exer    = boost::make_shared<EuropeanExercise>(expiry);
                VanillaOption call(payoff, exer);
                call.setPricingEngine(
                    boost::make_shared<AnalyticEuropeanEngine>(process));

                double callPremium = call.NPV();
                premium           += callPremium;
                portfolio         += callPremium;   // 收到 premium
                result.premiumPerTrade.append(callPremium);  // ← 加在這裡
                nextExpiry        += req.dteDays;
            }

            // ── Portfolio value = 持股市值 ────────────────────────────────────
            // （此骨架假設持有 1 股，不考慮被 assigned 的情況，可自行擴充）
            double value = spot + (premium);
            result.portfolioValues.append(value);
            result.buyHoldValues.append(spot);  // B&H = 純持股市值

            // ── Max drawdown 計算 ─────────────────────────────────────────────
            peakValue = qMax(peakValue, value);
            double dd = (peakValue - value) / peakValue;
            maxDD     = qMax(maxDD, dd);

            // ── 進度回報（透過 invokeMethod 跨執行緒）────────────────────────
            if (i % (n / 20 + 1) == 0) {
                int pct = static_cast<int>(100.0 * i / n);
                QMetaObject::invokeMethod(self, "progressUpdated",
                                          Qt::QueuedConnection,
                                          Q_ARG(int, pct));
            }
        }

        // ── 統計指標 ─────────────────────────────────────────────────────────
        result.maxDrawdown       = maxDD;
        result.premiumCollected  = premium;
        result.totalReturn       = (result.portfolioValues.last()
                                   - result.portfolioValues.first())
                                   / result.portfolioValues.first();

        // Sharpe（簡化版：年化報酬 / 年化標準差）
        double mean = 0.0;
        QVector<double> returns;
        for (int i = 1; i < result.portfolioValues.size(); ++i) {
            double r = (result.portfolioValues[i] - result.portfolioValues[i-1])
                       / result.portfolioValues[i-1];
            returns.append(r);
            mean += r;
        }
        mean /= returns.size();
        double var = 0.0;
        for (double r : returns) var += (r - mean) * (r - mean);
        var /= returns.size();
        double annualizedReturn = mean * 252;
        double annualizedStd   = std::sqrt(var) * std::sqrt(252.0);
        result.sharpeRatio = (annualizedStd > 1e-10)
                             ? (annualizedReturn - req.riskFreeRate) / annualizedStd
                             : 0.0;

        result.success = true;

    } catch (const std::exception& e) {
        result.success  = false;
        result.errorMsg = QString::fromStdString(e.what());
    }
    return result;
}
