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
    m_pricingWatcher = new QFutureWatcher<PricingResult>(this);
    connect(m_pricingWatcher, &QFutureWatcher<PricingResult>::finished,
            this, [this]() { emit pricingFinished(m_pricingWatcher->result()); });

    m_backtestWatcher = new QFutureWatcher<BacktestResult>(this);
    connect(m_backtestWatcher, &QFutureWatcher<BacktestResult>::finished,
            this, [this]() { emit backtestFinished(m_backtestWatcher->result()); });

    m_yieldWatcher = new QFutureWatcher<YieldCurveResult>(this);
    connect(m_yieldWatcher, &QFutureWatcher<YieldCurveResult>::finished,
            this, [this]() { emit yieldCurveFinished(m_yieldWatcher->result()); });
}

AsyncWorker::~AsyncWorker()
{
    cancelAll();
}

// ─────────────────────────────────────────────────────────────────────────────
void AsyncWorker::submitPricing(const PricingRequest& req)
{
    if (m_pricingWatcher->isRunning()) m_pricingWatcher->cancel();
    m_pricingWatcher->setFuture(
        QtConcurrent::run([req]() { return computePricing(req); }));
}

void AsyncWorker::submitBacktest(const BacktestRequest& req)
{
    if (m_backtestWatcher->isRunning()) m_backtestWatcher->cancel();
    m_backtestWatcher->setFuture(
        QtConcurrent::run([req, this]() { return computeBacktest(req, this); }));
}

void AsyncWorker::submitYieldCurve(const QVector<TenorRate>& tenors, double riskFreeRate)
{
    if (m_yieldWatcher->isRunning()) m_yieldWatcher->cancel();
    m_yieldWatcher->setFuture(
        QtConcurrent::run([tenors, riskFreeRate]() {
            return computeYieldCurve(tenors, riskFreeRate);
        }));
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
    return m_pricingWatcher->isRunning() || m_backtestWatcher->isRunning() || m_yieldWatcher->isRunning();
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

        double cashBalance = 0.0;
        double shares      = 1.0;
        double openStrike  = 0.0;
        bool   hasOpenCall = false;
        double peakValue   = req.closePrices.first();
        double maxDD       = 0.0;
        int    nextExpiry  = req.dteDays;

        // BSM price helper lambda (used by all strategies)
        auto bsmPrice = [&](double spot, double strike, Option::Type ot,
                            double T) -> double {
            Handle<Quote>              sH(boost::make_shared<SimpleQuote>(spot));
            Handle<YieldTermStructure> rH(boost::make_shared<FlatForward>(today, req.riskFreeRate, dc));
            Handle<YieldTermStructure> dH(boost::make_shared<FlatForward>(today, 0.0, dc));
            Handle<BlackVolTermStructure> vH(boost::make_shared<BlackConstantVol>(today, cal, req.impliedVol, dc));
            auto process = boost::make_shared<BlackScholesMertonProcess>(sH, dH, rH, vH);
            Date exDate  = today + static_cast<int>(T * 365);
            auto payoff  = boost::make_shared<PlainVanillaPayoff>(ot, strike);
            auto exer    = boost::make_shared<EuropeanExercise>(exDate);
            VanillaOption opt(payoff, exer);
            opt.setPricingEngine(boost::make_shared<AnalyticEuropeanEngine>(process));
            return opt.NPV();
        };

        double T = req.dteDays / 365.0;

        for (int i = 0; i < n; ++i) {
            double spot = req.closePrices[i];

            if (i == nextExpiry) {
                // ── 到期：平倉所有選擇權 ─────────────────────────────────────

                if (req.strategy == BacktestRequest::Strategy::CoveredCall && hasOpenCall) {
                    if (spot > openStrike) {
                        cashBalance += shares * openStrike;
                        shares = 0.0;
                        if (cashBalance >= spot) { cashBalance -= spot; shares = 1.0; }
                        else { shares = cashBalance / spot; cashBalance = 0.0; }
                        result.assignmentEvents.append(i);
                    }
                    hasOpenCall = false;
                }

                // ── 開新倉 ───────────────────────────────────────────────────
                if (req.strategy == BacktestRequest::Strategy::CoveredCall && shares > 0.0) {
                    // 賣 OTM call
                    double strike = spot * (1.0 + req.strikeOffsetPct);
                    double prem   = bsmPrice(spot, strike, Option::Call, T) * shares;
                    cashBalance  += prem;
                    openStrike    = strike;
                    hasOpenCall   = true;
                    result.premiumPerTrade.append(prem);

                } else if (req.strategy == BacktestRequest::Strategy::ProtectivePut) {
                    // 買 OTM put（付 premium，保護下檔）
                    double strike = spot * (1.0 - req.strikeOffsetPct);
                    double cost   = bsmPrice(spot, strike, Option::Put, T) * shares;
                    cashBalance  -= cost;   // 付出保險費
                    result.premiumPerTrade.append(-cost);  // 負值 = 付出

                } else if (req.strategy == BacktestRequest::Strategy::IronCondor) {
                    // Iron Condor：
                    //   賣 shortCallStrike OTM call + 買 longCallStrike 更 OTM call
                    //   賣 shortPutStrike  OTM put  + 買 longPutStrike  更 OTM put
                    double shortCall = spot * (1.0 + req.strikeOffsetPct);
                    double longCall  = spot * (1.0 + req.strikeOffsetPct + req.wingWidthPct);
                    double shortPut  = spot * (1.0 - req.strikeOffsetPct);
                    double longPut   = spot * (1.0 - req.strikeOffsetPct - req.wingWidthPct);

                    double premium =
                        bsmPrice(spot, shortCall, Option::Call, T) -
                        bsmPrice(spot, longCall,  Option::Call, T) +
                        bsmPrice(spot, shortPut,  Option::Put,  T) -
                        bsmPrice(spot, longPut,   Option::Put,  T);
                    cashBalance += premium;
                    result.premiumPerTrade.append(premium);

                    // 到期時損益（簡化：只考慮 short legs 被 assigned）
                    if (spot > shortCall)
                        cashBalance -= qMin(spot - shortCall, spot * req.wingWidthPct);
                    if (spot < shortPut)
                        cashBalance -= qMin(shortPut - spot, spot * req.wingWidthPct);
                }

                nextExpiry += req.dteDays;
            }

            // ── Portfolio value = 持股市值 + 現金 ────────────────────────────
            double value = shares * spot + cashBalance;
            result.portfolioValues.append(value);
            result.buyHoldValues.append(spot);

            peakValue = qMax(peakValue, value);
            double dd = (peakValue - value) / peakValue;
            maxDD     = qMax(maxDD, dd);

            if (i % (n / 20 + 1) == 0) {
                int pct = static_cast<int>(100.0 * i / n);
                QMetaObject::invokeMethod(self, "progressUpdated",
                                          Qt::QueuedConnection,
                                          Q_ARG(int, pct));
            }
        }

        // ── 統計指標 ─────────────────────────────────────────────────────────
        result.maxDrawdown       = maxDD;
        // premiumCollected = 所有 per-trade premium 的總和
        double totalPremium = 0.0;
        for (double p : result.premiumPerTrade) totalPremium += p;
        result.premiumCollected = totalPremium;
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

// =============================================================================
// computeYieldCurve — QuantLib Piecewise Bootstrap
// =============================================================================
YieldCurveResult AsyncWorker::computeYieldCurve(QVector<TenorRate> tenors,
                                                  double rf)
{
    YieldCurveResult result;
    if (tenors.size() < 2) {
        result.errorMsg = "Need at least 2 tenor points";
        return result;
    }

    try {
        DayCounter dc    = Actual365Fixed();
        Calendar   cal   = NullCalendar();
        Date       today = Date::todaysDate();
        Settings::instance().evaluationDate() = today;

        // 把輸入的 tenor/rate 對轉成 QuantLib RateHelper
        std::vector<boost::shared_ptr<RateHelper>> helpers;
        for (const auto& tr : tenors) {
            Period tenor(tr.months, Months);
            Handle<Quote> rateH(boost::make_shared<SimpleQuote>(tr.rate));
            auto helper = boost::make_shared<DepositRateHelper>(
                rateH, tenor, 0, cal, ModifiedFollowing, false, dc);
            helpers.push_back(helper);
        }

        // Piecewise bootstrap 殖利率曲線
        auto curve = boost::make_shared<
            PiecewiseYieldCurve<Discount, LogLinear>>(today, helpers, dc);
        curve->enableExtrapolation();

        // 掃描輸出點：每個月一個點，最多到最長期限
        int maxMonths = 0;
        for (const auto& tr : tenors) maxMonths = qMax(maxMonths, tr.months);

        for (int m = 1; m <= maxMonths; ++m) {
            double T = m / 12.0;
            Date   d = today + Period(m, Months);

            double df    = curve->discount(d);
            double spot  = -std::log(df) / T * 100.0;           // 連續複利 %
            double fwd   = 0.0;
            if (m > 1) {
                Date   d1  = today + Period(m-1, Months);
                double df1 = curve->discount(d1);
                double T1  = (m-1) / 12.0;
                double dT  = T - T1;
                fwd = -std::log(df / df1) / dT * 100.0;
            } else {
                fwd = spot;
            }

            result.maturitiesYears.append(T);
            result.spotRates.append(spot);
            result.forwardRates.append(fwd);
            result.discountFactors.append(df);
            result.zeroRates.append(
                curve->zeroRate(d, dc, Continuous).rate() * 100.0);
        }

        // ── Forward rate 3M 移動平均平滑 ────────────────────────────────────
        const int W = 3;  // window = 3 個月
        const int N = result.forwardRates.size();
        result.smoothedForwardRates.resize(N);
        for (int i = 0; i < N; ++i) {
            int lo = qMax(0, i - W/2);
            int hi = qMin(N-1, i + W/2);
            double sum = 0.0;
            for (int j = lo; j <= hi; ++j) sum += result.forwardRates[j];
            result.smoothedForwardRates[i] = sum / (hi - lo + 1);
        }

        result.success = true;
    } catch (const std::exception& e) {
        result.success  = false;
        result.errorMsg = QString::fromStdString(e.what());
    }
    return result;
}
