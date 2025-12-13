#include "MainWindow.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindowClass())
{
    ui->setupUi(this);
    setWindowTitle("viMarkdown ver 0.001");

	setup_connections();
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::setup_connections() {
    connect(ui->action_New, &QAction::triggered, this, &MainWindow::onAction_New);
    connect(ui->action_Close, &QAction::triggered, this, &MainWindow::onAction_Close);
}

void MainWindow::onAction_New() {
	qDebug() << "MainWindow::onAction_New()";

	auto ptr = new QWidget;
	int ix = ui->tabWidget->addTab(ptr, QString("Tab-%1").arg(ui->tabWidget->count()+1));
	ui->tabWidget->setCurrentIndex(ix);
}
void MainWindow::onAction_Close() {
	qDebug() << "MainWindow::onAction_Close()";

	int ix = ui->tabWidget->currentIndex();
	if( ix >= 0 )
		ui->tabWidget->removeTab(ix);
}

