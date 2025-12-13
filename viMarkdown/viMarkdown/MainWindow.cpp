#include "MainWindow.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindowClass())
{
    ui->setupUi(this);

    connect(ui->action_New, &QAction::triggered, this, &MainWindow::onAction_New);
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::onAction_New() {
	qDebug() << "MainWindow::onAction_New()";

	ui->tabWidget->addTab(new QWidget, QString("Tab-%1").arg(ui->tabWidget->count()+1));
}

