#pragma once
#include <QWidget>

QT_BEGIN_NAMESPACE
class QLineEdit;
class QDoubleSpinBox;
class QSpinBox;
class QComboBox;
class QCheckBox;
class QLabel;
class QPushButton;
QT_END_NAMESPACE

// ─────────────────────────────────────────────────────────────────────────────
// SettingsWidget
//
// 全域設定頁面，對應 AppSettings 的所有 key。
// 「Save」時寫入 QSettings，「Reset」恢復預設值。
//
// 分為四個區塊：
//   1. Quote provider  — Yahoo / Polygon + API key
//   2. Refresh         — 輪詢間隔
//   3. Pricing defaults— 預設無風險利率、波動率
//   4. Database        — DB 路徑、大小資訊
// ─────────────────────────────────────────────────────────────────────────────
class SettingsWidget : public QWidget
{
    Q_OBJECT

public:
    explicit SettingsWidget(QWidget* parent = nullptr);

signals:
    void statusMessage(const QString& msg);
    // 設定儲存後通知 MainWindow 重新套用（例如重啟輪詢）
    void settingsChanged();
    void themeChanged(bool dark);      // 通知 MainWindow 切換主題

private slots:
    void onSave();
    void onReset();
    void onProviderChanged(int index);
    void onBrowseDb();
    void onThemeToggled(bool dark);
    void onAddWatchSymbol();
    void onRemoveWatchSymbol();

private:
    void loadFromSettings();
    void buildUi();

    // ── Quote ─────────────────────────────────────────────────────────────────
    QComboBox*  m_provider    = nullptr;   // Yahoo / Polygon
    QLineEdit*  m_apiKey      = nullptr;
    QSpinBox*   m_refreshSec  = nullptr;

    // ── Pricing ───────────────────────────────────────────────────────────────
    QDoubleSpinBox* m_riskFree  = nullptr;
    QDoubleSpinBox* m_defaultVol= nullptr;

    // ── Database ──────────────────────────────────────────────────────────────
    QLineEdit*  m_dbPath      = nullptr;
    QLabel*     m_dbSizeLabel = nullptr;

    // ── Watchlist ─────────────────────────────────────────────────────────────
    class QListWidget* m_watchSymList = nullptr;
    class QLineEdit*   m_watchSymInput= nullptr;

    // ── Theme ─────────────────────────────────────────────────────────────────
    class QCheckBox*   m_darkModeChk  = nullptr;

    // ── Buttons ───────────────────────────────────────────────────────────────
    QPushButton* m_saveBtn  = nullptr;
    QPushButton* m_resetBtn = nullptr;

    // API key 顯示/隱藏 toggle
    QPushButton* m_toggleKeyBtn = nullptr;
    bool         m_keyVisible   = false;
};
