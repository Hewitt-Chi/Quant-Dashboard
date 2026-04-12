#pragma once
#include <QString>
#include <QColor>

// ─────────────────────────────────────────────────────────────────────────────
// AppSettings
//
// 全域設定的單例包裝器，底層使用 QSettings（INI 格式）。
// 所有 key 集中在此，避免魔法字串散落各處。
//
// 使用範例：
//   AppSettings::instance().setApiKey("my_polygon_key");
//   QString key = AppSettings::instance().apiKey();
// ─────────────────────────────────────────────────────────────────────────────
class AppSettings
{
public:
    static AppSettings& instance();

    // ── API ──────────────────────────────────────────────────────────────────
    QString apiKey() const;
    void    setApiKey(const QString& key);

    // ── Database ─────────────────────────────────────────────────────────────
    QString dbPath() const;          // 預設：同執行檔目錄 / quant.db
    void    setDbPath(const QString& path);

    // ── Pricing defaults ──────────────────────────────────────────────────────
    double  riskFreeRate() const;    // 預設：0.05 (5%)
    void    setRiskFreeRate(double r);

    double  defaultVolatility() const; // 預設：0.20 (20%)
    void    setDefaultVolatility(double v);

    // ── Quote fetch ───────────────────────────────────────────────────────────
    int     quoteRefreshSec() const; // 預設：60 秒
    void    setQuoteRefreshSec(int sec);

    QString quoteProvider() const;   // "polygon" | "yahoo"
    void    setQuoteProvider(const QString& provider);

    // ── UI ────────────────────────────────────────────────────────────────────
    bool    darkMode() const;
    void    setDarkMode(bool on);

    // ── 重設所有設定為預設值 ───────────────────────────────────────────────────
    void    reset();

private:
    AppSettings();
    ~AppSettings() = default;
    AppSettings(const AppSettings&) = delete;
    AppSettings& operator=(const AppSettings&) = delete;
};
