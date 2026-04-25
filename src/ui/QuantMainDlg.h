#pragma once

#include <QtWidgets/QMainWindow>
#include "ui_QuantMainDlg.h"
class AsyncWorker;
class DatabaseManager;
class QuoteFetcher;
class PricerWidget;
class WatchlistWidget;
class BacktestWidget;
class SettingsWidget;
class YieldCurveWidget;
class OptionChainWidget;
class VolSurfaceWidget;
class MonteCarloWidget;

//QT_BEGIN_NAMESPACE
//namespace Ui { class QuantMainDlg; }
//QT_END_NAMESPACE

class QuantMainDlg : public QMainWindow
{
    Q_OBJECT

private slots:
    void onSettingsChanged();

public:
    explicit QuantMainDlg(QWidget *parent = nullptr);
    ~QuantMainDlg();

private:
    void setupInfra();
    void setupUi();
    Ui::QuantLibQTClass ui;
    // ¢w¢w Infrastructure ¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w
    AsyncWorker* m_worker = nullptr;
    DatabaseManager* m_db = nullptr;
    QuoteFetcher* m_fetcher = nullptr;

    // ¢w¢w Pages ¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w¢w
    PricerWidget* m_pricer = nullptr;
    WatchlistWidget* m_watchlist = nullptr;
    BacktestWidget*  m_backtest  = nullptr;
    SettingsWidget*  m_settings  = nullptr;
    YieldCurveWidget* m_yieldCurve  = nullptr;
    OptionChainWidget* m_optionChain  = nullptr;
    VolSurfaceWidget* m_volSurface = nullptr;
    MonteCarloWidget*   m_monteCarlo   = nullptr;
};

