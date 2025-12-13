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

	auto ptr = new QWidget;
	int ix = ui->tabWidget->addTab(ptr, QString("Tab-%1").arg(ui->tabWidget->count()+1));
	ui->tabWidget->setCurrentIndex(ix);
}

