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
}

void MainWindow::onAction_New() {
	qDebug() << "MainWindow::onAction_New()";

	auto ptr = new QWidget;
	int ix = ui->tabWidget->addTab(ptr, QString("Tab-%1").arg(ui->tabWidget->count()+1));
	ui->tabWidget->setCurrentIndex(ix);
}

