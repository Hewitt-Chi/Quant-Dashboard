#include "QuoteFetcher.h"
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QTimer>
#include <QUrl>
#include <QUrlQuery>

// ─────────────────────────────────────────────────────────────────────────────
QuoteFetcher::QuoteFetcher(QObject* parent)
    : QObject(parent)
    , m_nam(new QNetworkAccessManager(this))
    , m_timer(new QTimer(this))
{
    m_timer->setSingleShot(false);
    connect(m_timer, &QTimer::timeout, this, &QuoteFetcher::onPollTimer);
}

QuoteFetcher::~QuoteFetcher()
{
    stopPolling();
}

// ─────────────────────────────────────────────────────────────────────────────
void QuoteFetcher::setProvider(Provider p) { m_provider = p; }
void QuoteFetcher::setApiKey(const QString& key) { m_apiKey = key; }

// ─────────────────────────────────────────────────────────────────────────────
// 輪詢控制
// ─────────────────────────────────────────────────────────────────────────────
void QuoteFetcher::startPolling(const QStringList& symbols, int intervalSec)
{
    m_watchList = symbols;
    onPollTimer();                         // 立即抓一次
    m_timer->start(intervalSec * 1000);
}

void QuoteFetcher::stopPolling()
{
    m_timer->stop();
}

bool QuoteFetcher::isPolling() const { return m_timer->isActive(); }

void QuoteFetcher::onPollTimer()
{
    fetchQuotes(m_watchList);
}

void QuoteFetcher::fetchQuotes(const QStringList& symbols)
{
    for (const QString& sym : symbols) {
        if (m_provider == Provider::Yahoo)
            fetchYahooQuote(sym);
        else
            fetchPolygonQuote(sym);
    }
}

void QuoteFetcher::fetchHistory(const QString& symbol, const QString& range)
{
    if (m_provider == Provider::Yahoo)
        fetchYahooHistory(symbol, range);
    else
        fetchPolygonHistory(symbol, range);
}

// ─────────────────────────────────────────────────────────────────────────────
// Yahoo Finance — 即時報價
//
// v8/finance/quote 的 JSON 欄位被 Yahoo 包進 "body" 結構，直接抓常被回空值。
// 改用 chart endpoint（range=5d interval=1d）取最近兩根 K 線算漲跌，
// 這個 endpoint 不需要 cookie / crumb，穩定可靠。
// ─────────────────────────────────────────────────────────────────────────────
void QuoteFetcher::fetchYahooQuote(const QString& symbol)
{
    QUrl url(QString("https://query1.finance.yahoo.com/v8/finance/chart/%1")
             .arg(symbol));
    QUrlQuery q;
    q.addQueryItem("range",    "5d");
    q.addQueryItem("interval", "1d");
    url.setQuery(q);

    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::UserAgentHeader,
                  "Mozilla/5.0 (Windows NT 10.0; Win64; x64) "
                  "AppleWebKit/537.36 Chrome/120.0 Safari/537.36");
    req.setRawHeader("Accept",          "application/json");
    req.setRawHeader("Accept-Language", "en-US,en;q=0.9");
    req.setRawHeader("Referer",         "https://finance.yahoo.com/");
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                     QNetworkRequest::NoLessSafeRedirectPolicy);

    auto* reply = m_nam->get(req);
    reply->setProperty("symbol",   symbol);
    reply->setProperty("quoteMode", true);   // 標記：這是即時報價請求
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        parseYahooQuote(reply, reply->property("symbol").toString());
        reply->deleteLater();
    });
}

void QuoteFetcher::parseYahooQuote(QNetworkReply* reply, const QString& symbol)
{
    if (reply->error() != QNetworkReply::NoError) {
        emit fetchError(symbol, reply->errorString());
        return;
    }

    const QByteArray raw = reply->readAll();
    if (raw.isEmpty()) {
        emit fetchError(symbol, "Empty response from Yahoo");
        return;
    }

    const auto doc = QJsonDocument::fromJson(raw);
    if (doc.isNull()) {
        emit fetchError(symbol, "Invalid JSON from Yahoo");
        return;
    }

    // chart.result[0] 路徑
    const QJsonObject result0 = doc["chart"]["result"].toArray()
                                    .first().toObject();
    if (result0.isEmpty()) {
        // 嘗試讀取 error 訊息
        QString err = doc["chart"]["error"]["description"].toString();
        emit fetchError(symbol, err.isEmpty() ? "No chart data" : err);
        return;
    }

    // 取最後一根 K 線
    const QJsonObject meta   = result0["meta"].toObject();
    const QJsonObject quote0 = result0["indicators"]["quote"]
                                       .toArray().first().toObject();
    const QJsonArray closes  = quote0["close"].toArray();
    const QJsonArray volumes = quote0["volume"].toArray();

    if (closes.isEmpty()) {
        emit fetchError(symbol, "No close data");
        return;
    }

    // 找最後一個非 null 的收盤價
    double lastClose = 0.0;
    int    lastIdx   = -1;
    for (int i = closes.size()-1; i >= 0; --i) {
        if (!closes[i].isNull()) {
            lastClose = closes[i].toDouble();
            lastIdx   = i;
            break;
        }
    }
    if (lastIdx < 0) {
        emit fetchError(symbol, "All close values are null");
        return;
    }

    // 前一根收盤（算漲跌）
    double prevClose = lastClose;
    for (int i = lastIdx-1; i >= 0; --i) {
        if (!closes[i].isNull()) {
            prevClose = closes[i].toDouble();
            break;
        }
    }

    qint64 vol = 0;
    if (lastIdx < volumes.size() && !volumes[lastIdx].isNull())
        vol = static_cast<qint64>(volumes[lastIdx].toDouble());

    // meta 也有 regularMarketPrice（盤中即時，比 K 線更新）
    double metaPrice = meta["regularMarketPrice"].toDouble();
    if (metaPrice > 0.0) lastClose = metaPrice;

    Quote q;
    q.symbol    = symbol;
    q.price     = lastClose;
    q.change    = lastClose - prevClose;
    q.changePct = prevClose > 0 ? (lastClose - prevClose) / prevClose * 100.0 : 0.0;
    q.volume    = vol;
    q.timestamp = QDateTime::currentDateTime();

    emit quoteReceived(q);
}

// ─────────────────────────────────────────────────────────────────────────────
// Yahoo Finance — 歷史 K 線
// ─────────────────────────────────────────────────────────────────────────────
void QuoteFetcher::fetchYahooHistory(const QString& symbol, const QString& range)
{
    QUrl url(QString("https://query1.finance.yahoo.com/v8/finance/chart/%1")
             .arg(symbol));
    QUrlQuery q;
    q.addQueryItem("range",    range);
    q.addQueryItem("interval", "1d");
    q.addQueryItem("events",   "history");
    url.setQuery(q);

    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::UserAgentHeader,
                  "Mozilla/5.0 QuantDashboard/1.0");

    auto* reply = m_nam->get(req);
    reply->setProperty("symbol", symbol);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        parseYahooHistory(reply, reply->property("symbol").toString());
        reply->deleteLater();
    });
}

void QuoteFetcher::parseYahooHistory(QNetworkReply* reply, const QString& symbol)
{
    if (reply->error() != QNetworkReply::NoError) {
        emit fetchError(symbol, reply->errorString());
        return;
    }

    const auto doc = QJsonDocument::fromJson(reply->readAll());
    // 路徑：chart.result[0].timestamp / indicators.quote[0]
    const QJsonObject result0 = doc["chart"]["result"].toArray()
                                    .first().toObject();
    const QJsonArray timestamps = result0["timestamp"].toArray();
    const QJsonObject quote0    = result0["indicators"]["quote"]
                                        .toArray().first().toObject();

    const QJsonArray opens   = quote0["open"].toArray();
    const QJsonArray highs   = quote0["high"].toArray();
    const QJsonArray lows    = quote0["low"].toArray();
    const QJsonArray closes  = quote0["close"].toArray();
    const QJsonArray volumes = quote0["volume"].toArray();

    QVector<OhlcBar> bars;
    bars.reserve(timestamps.size());

    for (int i = 0; i < timestamps.size(); ++i) {
        // 跳過空值（Yahoo 有時回傳 null）
        if (closes[i].isNull()) continue;

        OhlcBar bar;
        bar.symbol   = symbol;
        bar.datetime = QDateTime::fromSecsSinceEpoch(
                           static_cast<qint64>(timestamps[i].toDouble()));
        bar.open     = opens[i].toDouble();
        bar.high     = highs[i].toDouble();
        bar.low      = lows[i].toDouble();
        bar.close    = closes[i].toDouble();
        bar.volume   = static_cast<qint64>(volumes[i].toDouble());
        bars.append(bar);
    }

    emit historyReceived(symbol, bars);
}

// ─────────────────────────────────────────────────────────────────────────────
// Polygon.io — 即時報價
// ─────────────────────────────────────────────────────────────────────────────
void QuoteFetcher::fetchPolygonQuote(const QString& symbol)
{
    // Snapshot endpoint（需 Starter plan 以上）
    QUrl url(QString("https://api.polygon.io/v2/snapshot/locale/us/markets/stocks/tickers/%1")
             .arg(symbol));
    QUrlQuery q;
    q.addQueryItem("apiKey", m_apiKey);
    url.setQuery(q);

    auto* reply = m_nam->get(QNetworkRequest(url));
    reply->setProperty("symbol", symbol);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        parsePolygonQuote(reply, reply->property("symbol").toString());
        reply->deleteLater();
    });
}

void QuoteFetcher::parsePolygonQuote(QNetworkReply* reply, const QString& symbol)
{
    if (reply->error() != QNetworkReply::NoError) {
        emit fetchError(symbol, reply->errorString());
        return;
    }

    const auto doc = QJsonDocument::fromJson(reply->readAll());
    // 路徑：ticker.day.c（今日收盤）、prevDay.c（前日收盤）
    const QJsonObject ticker = doc["ticker"].toObject();
    const QJsonObject day    = ticker["day"].toObject();
    const QJsonObject prev   = ticker["prevDay"].toObject();

    double price = day["c"].toDouble();
    double prev_close = prev["c"].toDouble();

    Quote q;
    q.symbol    = symbol;
    q.price     = price;
    q.change    = price - prev_close;
    q.changePct = prev_close > 0 ? (price - prev_close) / prev_close * 100.0 : 0.0;
    q.volume    = static_cast<qint64>(day["v"].toDouble());
    q.timestamp = QDateTime::currentDateTime();

    emit quoteReceived(q);
}

// ─────────────────────────────────────────────────────────────────────────────
// Polygon.io — 歷史 K 線
// ─────────────────────────────────────────────────────────────────────────────
void QuoteFetcher::fetchPolygonHistory(const QString& symbol, const QString& range)
{
    // 把 "1y" 轉換成 from/to 日期
    QDate to   = QDate::currentDate();
    QDate from = to.addYears(-1);
    if      (range == "6mo") from = to.addMonths(-6);
    else if (range == "3mo") from = to.addMonths(-3);
    else if (range == "1mo") from = to.addMonths(-1);

    QUrl url(QString("https://api.polygon.io/v2/aggs/ticker/%1/range/1/day/%2/%3")
             .arg(symbol)
             .arg(from.toString("yyyy-MM-dd"))
             .arg(to.toString("yyyy-MM-dd")));
    QUrlQuery q;
    q.addQueryItem("adjusted", "true");
    q.addQueryItem("sort",     "asc");
    q.addQueryItem("limit",    "365");
    q.addQueryItem("apiKey",   m_apiKey);
    url.setQuery(q);

    auto* reply = m_nam->get(QNetworkRequest(url));
    reply->setProperty("symbol", symbol);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        parsePolygonHistory(reply, reply->property("symbol").toString());
        reply->deleteLater();
    });
}

void QuoteFetcher::parsePolygonHistory(QNetworkReply* reply, const QString& symbol)
{
    if (reply->error() != QNetworkReply::NoError) {
        emit fetchError(symbol, reply->errorString());
        return;
    }

    const auto doc = QJsonDocument::fromJson(reply->readAll());
    const QJsonArray results = doc["results"].toArray();

    QVector<OhlcBar> bars;
    bars.reserve(results.size());

    for (const QJsonValue& v : results) {
        const QJsonObject obj = v.toObject();
        OhlcBar bar;
        bar.symbol   = symbol;
        // Polygon 的 t 是 Unix timestamp（ms）
        bar.datetime = QDateTime::fromMSecsSinceEpoch(
                           static_cast<qint64>(obj["t"].toDouble()));
        bar.open     = obj["o"].toDouble();
        bar.high     = obj["h"].toDouble();
        bar.low      = obj["l"].toDouble();
        bar.close    = obj["c"].toDouble();
        bar.volume   = static_cast<qint64>(obj["v"].toDouble());
        bars.append(bar);
    }

    emit historyReceived(symbol, bars);
}
