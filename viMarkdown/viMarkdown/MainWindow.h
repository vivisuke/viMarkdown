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

protected:
    void	setup_connections();
    QWidget	*newTabWidget();

    void	onAction_New();
    void	onAction_Close();

private:
    int		m_tab_number = 0;
    Ui::MainWindowClass *ui;
};

