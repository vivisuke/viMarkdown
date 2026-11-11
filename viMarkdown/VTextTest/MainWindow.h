#pragma once

#include <QtWidgets/QMainWindow>
#include "ui_MainWindow.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindowClass; };
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    void do_output(const QString& txt);

private slots:
    void onAction_AutoTest();
    void onAction_PerformanceTest();

private:
    Ui::MainWindowClass *ui;
};

