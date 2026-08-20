#include <QTextCursor>
#include <QTextDocument>
#include "MainWindow.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindowClass())
{
    ui->setupUi(this);
    connect(ui->action_Gen10000lines, &QAction::triggered, this, &MainWindow::onAction_gen10000lines);
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::onAction_gen10000lines() {
	qDebug() << "MainWindow::onAction_gen10000lines()";
	QTextCursor cursor = ui->plainTextEdit->textCursor();
	for(int k = 0; k < 10; ++k) {
		cursor.insertText(QString("## %1. heading\n").arg(k+1));
		for(int i = 0; i < 99; ++i) {
			cursor.insertText("body text body text body text body text body text body text body text \n");
		}
	}
	ui->plainTextEdit->setTextCursor(cursor);
}
