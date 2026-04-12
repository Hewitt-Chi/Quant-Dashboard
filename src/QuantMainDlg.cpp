#include "QuantMainDlg.h"
#include "ui_QuantMainDlg.h"   // ← 加上這行
#include <QApplication>


QuantMainDlg::QuantMainDlg(QWidget *parent)
    : QMainWindow(parent)
{
    ui.setupUi(this);
}

QuantMainDlg::~QuantMainDlg()
{}

