#include "AppSettings.h"
#include <QSettings>
#include <QCoreApplication>
#include <QDir>

// ── Key 常數（集中定義，避免 typo）─────────────────────────────────────────────
namespace Key {
    constexpr auto ApiKey          = "api/key";
    constexpr auto DbPath          = "db/path";
    constexpr auto RiskFreeRate    = "pricing/riskFreeRate";
    constexpr auto DefaultVol      = "pricing/defaultVolatility";
    constexpr auto QuoteRefresh    = "quote/refreshSec";
    constexpr auto QuoteProvider   = "quote/provider";
    constexpr auto DarkMode        = "ui/darkMode";
}

// ── 預設值 ────────────────────────────────────────────────────────────────────
namespace Default {
    constexpr auto ApiKey        = "";
    constexpr auto QuoteProvider = "yahoo";
    constexpr double RiskFreeRate   = 0.05;
    constexpr double DefaultVol     = 0.20;
    constexpr int    QuoteRefresh   = 60;
    constexpr bool   DarkMode       = false;
}

// ── 取得預設 db 路徑（同執行檔目錄）──────────────────────────────────────────
static QString defaultDbPath()
{
    return QDir(QCoreApplication::applicationDirPath())
               .filePath("quant.db");
}

// ─────────────────────────────────────────────────────────────────────────────

AppSettings& AppSettings::instance()
{
    static AppSettings s;
    return s;
}

AppSettings::AppSettings()
{
    // 使用 INI 格式存在使用者設定目錄
    // Windows: %APPDATA%\QuantDashboard\QuantDashboard.ini
    // Linux/macOS: ~/.config/QuantDashboard/QuantDashboard.ini
    QSettings::setDefaultFormat(QSettings::IniFormat);
}

// ── 私有輔助：取得 QSettings 物件 ────────────────────────────────────────────
static QSettings cfg()
{
    return QSettings("QuantDashboard", "QuantDashboard");
}

// ── API ───────────────────────────────────────────────────────────────────────
QString AppSettings::apiKey() const
{
    return cfg().value(Key::ApiKey, Default::ApiKey).toString();
}
void AppSettings::setApiKey(const QString& key)
{
    auto s = cfg(); s.setValue(Key::ApiKey, key);
}

// ── Database ──────────────────────────────────────────────────────────────────
QString AppSettings::dbPath() const
{
    return cfg().value(Key::DbPath, defaultDbPath()).toString();
}
void AppSettings::setDbPath(const QString& path)
{
    auto s = cfg(); s.setValue(Key::DbPath, path);
}

// ── Pricing ───────────────────────────────────────────────────────────────────
double AppSettings::riskFreeRate() const
{
    return cfg().value(Key::RiskFreeRate, Default::RiskFreeRate).toDouble();
}
void AppSettings::setRiskFreeRate(double r)
{
    auto s = cfg(); s.setValue(Key::RiskFreeRate, r);
}

double AppSettings::defaultVolatility() const
{
    return cfg().value(Key::DefaultVol, Default::DefaultVol).toDouble();
}
void AppSettings::setDefaultVolatility(double v)
{
    auto s = cfg(); s.setValue(Key::DefaultVol, v);
}

// ── Quote ─────────────────────────────────────────────────────────────────────
int AppSettings::quoteRefreshSec() const
{
    return cfg().value(Key::QuoteRefresh, Default::QuoteRefresh).toInt();
}
void AppSettings::setQuoteRefreshSec(int sec)
{
    auto s = cfg(); s.setValue(Key::QuoteRefresh, sec);
}

QString AppSettings::quoteProvider() const
{
    return cfg().value(Key::QuoteProvider, Default::QuoteProvider).toString();
}
void AppSettings::setQuoteProvider(const QString& provider)
{
    auto s = cfg(); s.setValue(Key::QuoteProvider, provider);
}

// ── UI ────────────────────────────────────────────────────────────────────────
bool AppSettings::darkMode() const
{
    return cfg().value(Key::DarkMode, Default::DarkMode).toBool();
}
void AppSettings::setDarkMode(bool on)
{
    auto s = cfg(); s.setValue(Key::DarkMode, on);
}

// ── Reset ─────────────────────────────────────────────────────────────────────
void AppSettings::reset()
{
    auto s = cfg();
    s.clear();
    s.sync();
}
