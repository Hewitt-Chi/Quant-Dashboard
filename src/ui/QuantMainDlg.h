#pragma once

#include <QtWidgets/QMainWindow>
#include "ui_QuantMainDlg.h"
class AsyncWorker;
class DatabaseManager;
class QuoteFetcher;
class PricerWidget;

//QT_BEGIN_NAMESPACE
//namespace Ui { class QuantMainDlg; }
//QT_END_NAMESPACE

class QuantMainDlg : public QMainWindow
{
    Q_OBJECT

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
};

