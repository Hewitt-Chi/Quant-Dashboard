#pragma once

#include <QtWidgets/QMainWindow>
#include "ui_QuantMainDlg.h"

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
    Ui::QuantLibQTClass ui;
};

