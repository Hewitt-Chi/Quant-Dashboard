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
class QListWidget;
QT_END_NAMESPACE

class SettingsWidget : public QWidget
{
    Q_OBJECT
public:
    explicit SettingsWidget(QWidget* parent = nullptr);

signals:
    void statusMessage(const QString& msg);
    void settingsChanged();
    void themeChanged(bool dark);

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

    // Quote
    QComboBox*  m_provider    = nullptr;
    QLineEdit*  m_apiKey      = nullptr;
    QSpinBox*   m_refreshSec  = nullptr;

    // Pricing
    QDoubleSpinBox* m_riskFree   = nullptr;
    QDoubleSpinBox* m_defaultVol = nullptr;

    // Watchlist
    QListWidget* m_watchSymList  = nullptr;
    QLineEdit*   m_watchSymInput = nullptr;

    // Theme
    QCheckBox*  m_darkModeChk = nullptr;

    // Database
    QLineEdit*  m_dbPath      = nullptr;
    QLabel*     m_dbSizeLabel = nullptr;

    // Buttons
    QPushButton* m_saveBtn      = nullptr;
    QPushButton* m_resetBtn     = nullptr;
    QPushButton* m_toggleKeyBtn = nullptr;
    bool         m_keyVisible   = false;
};
