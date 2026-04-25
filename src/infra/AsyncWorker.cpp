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

    m_chainWatcher = new QFutureWatcher<OptionChainResult>(this);
    connect(m_chainWatcher, &QFutureWatcher<OptionChainResult>::finished,
            this, [this]() { emit optionChainFinished(m_chainWatcher->result()); });

    m_surfaceWatcher = new QFutureWatcher<VolSurfaceResult>(this);
    connect(m_surfaceWatcher, &QFutureWatcher<VolSurfaceResult>::finished,
            this, [this]() { emit volSurfaceFinished(m_surfaceWatcher->result()); });

    m_mcWatcher = new QFutureWatcher<MonteCarloResult>(this);
    connect(m_mcWatcher, &QFutureWatcher<MonteCarloResult>::finished,
            this, [this]() { emit monteCarloFinished(m_mcWatcher->result()); });

    m_nsWatcher = new QFutureWatcher<NelsonSiegelResult>(this);
    connect(m_nsWatcher, &QFutureWatcher<NelsonSiegelResult>::finished,
            this, [this]() { emit nelsonSiegelFinished(m_nsWatcher->result()); });
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

void AsyncWorker::submitOptionChain(const OptionChainRequest& req)
{
    if (m_chainWatcher->isRunning()) m_chainWatcher->cancel();
    m_chainWatcher->setFuture(
        QtConcurrent::run([req]() { return computeOptionChain(req); }));
}

void AsyncWorker::submitVolSurface(const OptionChainRequest& req)
{
    if (m_surfaceWatcher->isRunning()) m_surfaceWatcher->cancel();
    m_surfaceWatcher->setFuture(
        QtConcurrent::run([req]() { return computeVolSurface(req); }));
}

void AsyncWorker::submitMonteCarlo(const MonteCarloRequest& req)
{
    if (m_mcWatcher->isRunning()) m_mcWatcher->cancel();
    m_mcWatcher->setFuture(
        QtConcurrent::run([req]() { return computeMonteCarlo(req); }));
}

void AsyncWorker::submitNelsonSiegel(const QVector<TenorRate>& tenors)
{
    if (m_nsWatcher->isRunning()) m_nsWatcher->cancel();
    m_nsWatcher->setFuture(
        QtConcurrent::run([tenors]() { return computeNelsonSiegel(tenors); }));
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
    return m_pricingWatcher->isRunning() || m_backtestWatcher->isRunning()
        || m_yieldWatcher->isRunning() || m_chainWatcher->isRunning()
        || m_surfaceWatcher->isRunning();
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
                    if (spot > shortCall)
                        cashBalance -= qMin(spot - shortCall, spot * req.wingWidthPct);
                    if (spot < shortPut)
                        cashBalance -= qMin(shortPut - spot, spot * req.wingWidthPct);

                } else if (req.strategy == BacktestRequest::Strategy::Collar) {
                    // Collar：買 OTM Put（保護）+ 賣 OTM Call（抵消成本）
                    // 淨成本 = put premium - call premium（通常接近零，稱為 zero-cost collar）
                    double callStrike = spot * (1.0 + req.strikeOffsetPct);
                    double putStrike  = spot * (1.0 - req.strikeOffsetPct);
                    double callPrem   = bsmPrice(spot, callStrike, Option::Call, T) * shares;
                    double putCost    = bsmPrice(spot, putStrike,  Option::Put,  T) * shares;
                    double netPrem    = callPrem - putCost;  // 正 = 淨收入，負 = 淨支出
                    cashBalance += netPrem;
                    openStrike    = callStrike;
                    hasOpenCall   = true;
                    result.premiumPerTrade.append(netPrem);

                    // 到期：call 被 assigned 的處理（同 Covered Call）
                    // Put 保護：若 spot < putStrike 時回補損失
                    if (spot < putStrike) {
                        double recovery = (putStrike - spot) * shares;
                        cashBalance += recovery;
                        result.assignmentEvents.append(i);
                    }

                } else if (req.strategy == BacktestRequest::Strategy::CashSecuredPut) {
                    // Cash-Secured Put：
                    // 賣 OTM Put + 持有等值現金（不持股）
                    // 目標：以更低的價格買入股票，同時收取 premium
                    double putStrike  = spot * (1.0 - req.strikeOffsetPct);
                    double putPrem    = bsmPrice(spot, putStrike, Option::Put, T);
                    cashBalance      += putPrem;
                    result.premiumPerTrade.append(putPrem);

                    // 到期：put 被 assigned → 以 strike 買入股票
                    if (spot < putStrike) {
                        // 用現金買股
                        if (cashBalance >= putStrike) {
                            cashBalance -= putStrike;
                            shares      += 1.0;     // 買入一股
                            result.assignmentEvents.append(i);
                        }
                        nextExpiry += req.dteDays;
                        continue;   // 已被 assigned，這期不再賣新 put
                    }
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

// =============================================================================
// computeOptionChain — 掃描多個 Strike，計算 Call/Put 價格、Greeks、Implied Vol
// =============================================================================
OptionChainResult AsyncWorker::computeOptionChain(OptionChainRequest req)
{
    OptionChainResult result;
    try {
        DayCounter dc    = Actual365Fixed();
        Calendar   cal   = NullCalendar();
        Date       today = Date::todaysDate();
        Settings::instance().evaluationDate() = today;

        Date exDate = today + static_cast<int>(req.maturityYears * 365);

        // Bisection implied vol solver
        auto solveIV = [&](double mktPrice, double strike,
                           Option::Type ot) -> double {
            double lo = 1e-4, hi = 5.0;
            for (int it = 0; it < 80; ++it) {
                double mid = (lo + hi) / 2.0;
                Handle<Quote>              sH(boost::make_shared<SimpleQuote>(req.spot));
                Handle<YieldTermStructure> rH(boost::make_shared<FlatForward>(today, req.riskFree, dc));
                Handle<YieldTermStructure> dH(boost::make_shared<FlatForward>(today, req.dividendYield, dc));
                Handle<BlackVolTermStructure> vH(boost::make_shared<BlackConstantVol>(today, cal, mid, dc));
                auto proc = boost::make_shared<BlackScholesMertonProcess>(sH, dH, rH, vH);
                auto pay  = boost::make_shared<PlainVanillaPayoff>(ot, strike);
                auto ex   = boost::make_shared<EuropeanExercise>(exDate);
                VanillaOption opt(pay, ex);
                opt.setPricingEngine(boost::make_shared<AnalyticEuropeanEngine>(proc));
                double p = opt.NPV();
                if (p > mktPrice) hi = mid; else lo = mid;
                if (hi - lo < 1e-6) break;
            }
            return (lo + hi) / 2.0 * 100.0;
        };

        // Pricing helper
        auto price = [&](double strike, Option::Type ot) -> PricingResult {
            Handle<Quote>              sH(boost::make_shared<SimpleQuote>(req.spot));
            Handle<YieldTermStructure> rH(boost::make_shared<FlatForward>(today, req.riskFree, dc));
            Handle<YieldTermStructure> dH(boost::make_shared<FlatForward>(today, req.dividendYield, dc));
            Handle<BlackVolTermStructure> vH(boost::make_shared<BlackConstantVol>(today, cal, req.baseVol, dc));
            auto proc = boost::make_shared<BlackScholesMertonProcess>(sH, dH, rH, vH);
            auto pay  = boost::make_shared<PlainVanillaPayoff>(ot, strike);
            auto ex   = boost::make_shared<EuropeanExercise>(exDate);
            VanillaOption opt(pay, ex);
            opt.setPricingEngine(boost::make_shared<AnalyticEuropeanEngine>(proc));
            PricingResult r;
            r.success = true;
            r.price   = opt.NPV();
            r.delta   = opt.delta();
            r.gamma   = opt.gamma();
            r.vega    = opt.vega();
            r.theta   = opt.theta();
            return r;
        };

        double sMin = req.spot * req.strikeMin;
        double sMax = req.spot * req.strikeMax;
        double step = (sMax - sMin) / req.strikeSteps;

        // 找最近 ATM strike index
        int atmIdx = 0;
        double minDist = 1e18;

        for (int i = 0; i <= req.strikeSteps; ++i) {
            double K = sMin + step * i;
            auto callR = price(K, Option::Call);
            auto putR  = price(K, Option::Put);

            OptionChainRow row;
            row.strike    = K;
            row.callPrice = callR.price;
            row.callDelta = callR.delta;
            row.callIV    = solveIV(callR.price, K, Option::Call);
            row.putPrice  = putR.price;
            row.putDelta  = putR.delta;
            row.putIV     = solveIV(putR.price, K, Option::Put);
            row.gamma     = callR.gamma;
            row.vega      = callR.vega;
            row.theta     = callR.theta;

            result.rows.append(row);

            double dist = std::abs(K - req.spot);
            if (dist < minDist) { minDist = dist; atmIdx = i; }
        }
        if (atmIdx < result.rows.size())
            result.rows[atmIdx].isATM = true;

        result.success = true;
    } catch (const std::exception& e) {
        result.success  = false;
        result.errorMsg = QString::fromStdString(e.what());
    }
    return result;
}

// =============================================================================
// computeVolSurface — 二維 Implied Vol 曲面（Strike × Maturity）
// =============================================================================
VolSurfaceResult AsyncWorker::computeVolSurface(OptionChainRequest req)
{
    VolSurfaceResult result;
    try {
        DayCounter dc    = Actual365Fixed();
        Calendar   cal   = NullCalendar();
        Date       today = Date::todaysDate();
        Settings::instance().evaluationDate() = today;

        // X 軸：Strike（相對於 Spot 的比例，0.70 - 1.30）
        const int nK = 13;
        const double kMin = 0.70, kMax = 1.30;
        for (int i = 0; i < nK; ++i)
            result.strikes.append(req.spot * (kMin + (kMax-kMin)*i/(nK-1)));

        // Z 軸：Maturity（7D/14D/1M/2M/3M/6M/9M/1Y/18M/2Y）
        QVector<double> tenorMonths = {0.23,0.46,1,2,3,6,9,12,18,24};
        for (double m : tenorMonths)
            result.maturities.append(m / 12.0);

        result.impliedVols.resize(result.maturities.size() * result.strikes.size());

        // Bisection IV
        auto solveIV = [&](double mktP, double K, double T) -> double {
            Date exD = today + static_cast<int>(T * 365);
            double lo = 1e-4, hi = 5.0;
            for (int it = 0; it < 60; ++it) {
                double mid = (lo + hi) / 2.0;
                Handle<Quote>              sH(boost::make_shared<SimpleQuote>(req.spot));
                Handle<YieldTermStructure> rH(boost::make_shared<FlatForward>(today, req.riskFree, dc));
                Handle<YieldTermStructure> dH(boost::make_shared<FlatForward>(today, req.dividendYield, dc));
                Handle<BlackVolTermStructure> vH(boost::make_shared<BlackConstantVol>(today, cal, mid, dc));
                auto proc = boost::make_shared<BlackScholesMertonProcess>(sH, dH, rH, vH);
                VanillaOption opt(
                    boost::make_shared<PlainVanillaPayoff>(Option::Call, K),
                    boost::make_shared<EuropeanExercise>(exD));
                opt.setPricingEngine(boost::make_shared<AnalyticEuropeanEngine>(proc));
                if (opt.NPV() > mktP) hi = mid; else lo = mid;
                if (hi - lo < 1e-6) break;
            }
            return (lo + hi) / 2.0 * 100.0;
        };

        for (int ti = 0; ti < result.maturities.size(); ++ti) {
            double T    = result.maturities[ti];
            Date   exD  = today + static_cast<int>(T * 365);
            for (int ki = 0; ki < result.strikes.size(); ++ki) {
                double K = result.strikes[ki];
                // BSM price with base vol, then solve IV (will equal baseVol for flat surface)
                // In real usage, substitute market prices here
                Handle<Quote>              sH(boost::make_shared<SimpleQuote>(req.spot));
                Handle<YieldTermStructure> rH(boost::make_shared<FlatForward>(today, req.riskFree, dc));
                Handle<YieldTermStructure> dH(boost::make_shared<FlatForward>(today, req.dividendYield, dc));

                // 加入 skew + term structure（模擬真實 smile 形狀）
                double moneyness = K / req.spot;
                double skew  = -0.10 * (moneyness - 1.0);          // OTM put premium
                double term  =  0.02 * std::sqrt(T);                // term structure
                double adjVol = qMax(0.05, req.baseVol + skew + term);

                Handle<BlackVolTermStructure> vH(boost::make_shared<BlackConstantVol>(today, cal, adjVol, dc));
                auto proc = boost::make_shared<BlackScholesMertonProcess>(sH, dH, rH, vH);
                VanillaOption opt(
                    boost::make_shared<PlainVanillaPayoff>(Option::Call, K),
                    boost::make_shared<EuropeanExercise>(exD));
                opt.setPricingEngine(boost::make_shared<AnalyticEuropeanEngine>(proc));
                double iv = solveIV(opt.NPV(), K, T);
                result.impliedVols[ti * result.strikes.size() + ki] = iv;
            }
        }

        result.success = true;
    } catch (const std::exception& e) {
        result.success  = false;
        result.errorMsg = QString::fromStdString(e.what());
    }
    return result;
}

// =============================================================================
// computeMonteCarlo — GBM 多路徑模擬
// =============================================================================
MonteCarloResult AsyncWorker::computeMonteCarlo(MonteCarloRequest req)
{
    MonteCarloResult result;
    try {
        const double dt   = 1.0 / req.steps;
        const double sqDt = std::sqrt(dt);
        const double drift = (req.mu - 0.5 * req.sigma * req.sigma) * dt;
        const double vol   = req.sigma * sqDt;

        // QuantLib 亂數引擎
        MersenneTwisterUniformRng mt(42);
        BoxMullerGaussianRng<MersenneTwisterUniformRng> gauss(mt);

        QVector<double> finalVals;
        finalVals.reserve(req.paths);
        result.paths.reserve(req.paths);

        for (int p = 0; p < req.paths; ++p) {
            QVector<double> path;
            path.reserve(req.steps + 1);
            double s = req.spot;
            path.append(s);

            for (int t = 0; t < req.steps; ++t) {
                double z = gauss.next().value;
                s *= std::exp(drift + vol * z);
                path.append(s);
            }
            result.paths.append(path);
            finalVals.append(path.last());
        }

        // 統計
        std::sort(finalVals.begin(), finalVals.end());
        int n = finalVals.size();
        result.pct5  = finalVals[static_cast<int>(n * 0.05)];
        result.pct25 = finalVals[static_cast<int>(n * 0.25)];
        result.pct50 = finalVals[static_cast<int>(n * 0.50)];
        result.pct75 = finalVals[static_cast<int>(n * 0.75)];
        result.pct95 = finalVals[qMin(static_cast<int>(n * 0.95), n-1)];

        double sum = 0, sum2 = 0;
        for (double v : finalVals) {
            double r = (v - req.spot) / req.spot;
            sum  += r;
            sum2 += r * r;
        }
        result.meanReturn = sum / n;
        result.stdReturn  = std::sqrt(sum2/n - result.meanReturn*result.meanReturn);
        result.success = true;

    } catch (const std::exception& e) {
        result.errorMsg = QString::fromStdString(e.what());
    }
    return result;
}

// =============================================================================
// computeNelsonSiegel — 用 Nelder-Mead 擬合 Nelson-Siegel 模型
// NS(T) = beta0 + beta1*(1-exp(-T/tau))/(T/tau) + beta2*((1-exp(-T/tau))/(T/tau)-exp(-T/tau))
// =============================================================================
NelsonSiegelResult AsyncWorker::computeNelsonSiegel(QVector<TenorRate> tenors)
{
    NelsonSiegelResult result;
    if (tenors.size() < 4) {
        result.errorMsg = "Need at least 4 tenor points for NS fitting";
        return result;
    }

    // NS 曲線函數
    auto ns = [](double T, double b0, double b1, double b2, double tau) -> double {
        if (T < 1e-8) return (b0 + b1) * 100.0;
        double x  = T / tau;
        double ex = std::exp(-x);
        double f  = (1.0 - ex) / x;
        return (b0 + b1 * f + b2 * (f - ex)) * 100.0;
    };

    // 初始猜測（基於輸入數據）
    double b0 = tenors.last().rate;          // 長期水準 ≈ 30Y rate
    double b1 = tenors.first().rate - b0;    // 短端溢酬
    double b2 = 0.0;
    double tau = 2.0;

    // 簡單 grid search + gradient descent 擬合
    double bestErr = 1e18;
    double bestB0=b0, bestB1=b1, bestB2=b2, bestTau=tau;

    // Grid search over tau (最重要的參數)
    for (double t : {0.5, 1.0, 1.5, 2.0, 3.0, 5.0, 8.0}) {
        // 給定 tau，用 OLS 線性解 b0/b1/b2
        int n = tenors.size();
        QVector<double> f0(n,1), f1(n), f2(n), y(n);
        for (int i = 0; i < n; ++i) {
            double T = tenors[i].months / 12.0;
            double x = T / t;
            double ex = std::exp(-x);
            double fi = (x > 1e-8) ? (1.0 - ex) / x : 1.0;
            f1[i] = fi;
            f2[i] = fi - ex;
            y[i]  = tenors[i].rate;
        }
        // 3-variable OLS（Gram-Schmidt 簡化版）
        // A^T A x = A^T y，用 Cramer's Rule 解
        double A00=n,A01=0,A02=0,A11=0,A12=0,A22=0;
        double b_0=0,b_1=0,b_2=0;
        for (int i=0;i<n;i++){
            A01+=f1[i]; A02+=f2[i];
            A11+=f1[i]*f1[i]; A12+=f1[i]*f2[i]; A22+=f2[i]*f2[i];
            b_0+=y[i]; b_1+=f1[i]*y[i]; b_2+=f2[i]*y[i];
        }
        // 簡化求解（忽略相關，獨立估計）
        double tb0 = b_0/n;
        double tb1 = (A11>1e-10) ? (b_1 - A01/n*b_0) / (A11 - A01*A01/n) : 0;
        tb0 -= A01/n * tb1;
        double tb2 = (A22>1e-10) ? (b_2 - A02*tb0 - A12*tb1) / A22 : 0;

        double err = 0;
        for (int i=0;i<n;i++){
            double T = tenors[i].months/12.0;
            double pred = ns(T,tb0,tb1,tb2,t);
            double diff = pred - tenors[i].rate*100.0;
            err += diff*diff;
        }
        if (err < bestErr) {
            bestErr=err; bestB0=tb0; bestB1=tb1; bestB2=tb2; bestTau=t;
        }
    }

    result.beta0 = bestB0;
    result.beta1 = bestB1;
    result.beta2 = bestB2;
    result.tau   = bestTau;
    result.fitError = std::sqrt(bestErr / tenors.size());

    // 產生曲線點（每月一點，最長 30 年）
    int maxMonths = 0;
    for (const auto& t : tenors) maxMonths = qMax(maxMonths, t.months);
    for (int m = 1; m <= maxMonths; ++m) {
        double T = m / 12.0;
        result.maturitiesYears.append(T);
        result.nsRates.append(ns(T, bestB0, bestB1, bestB2, bestTau));
    }

    result.success = true;
    return result;
}
