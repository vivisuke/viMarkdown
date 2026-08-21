#include <QTextCursor>
#include <QTextDocument>
#include <QElapsedTimer>
#include "MainWindow.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindowClass())
{
    ui->setupUi(this);
    connect(ui->action_Gen1000lines, &QAction::triggered, this, &MainWindow::onAction_gen1000lines);
    connect(ui->action_Gen5000lines, &QAction::triggered, this, &MainWindow::onAction_gen5000lines);
    connect(ui->action_Gen10000lines, &QAction::triggered, this, &MainWindow::onAction_gen10000lines);
    connect(ui->action_SetMarkdown, &QAction::triggered, this, &MainWindow::onAction_SetMarkdown);
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::gen_lines(int n) {
	QTextCursor cursor = ui->plainTextEdit->textCursor();
	cursor.beginEditBlock();
	for(int k = 0; k < n/100; ++k) {
		cursor.insertText(QString("## %1. heading\n").arg(k+1));
		for(int i = 0; i < 99; ++i) {
			cursor.insertText("body text body text body text body text body text body text body text  \n");
		}
	}
	cursor.endEditBlock();
	ui->plainTextEdit->setTextCursor(cursor);
}

void MainWindow::onAction_gen1000lines() {
	gen_lines(1000);
}
void MainWindow::onAction_gen5000lines() {
	gen_lines(5000);
}
void MainWindow::onAction_gen10000lines() {
	gen_lines(10000);
}
void MainWindow::onAction_SetMarkdown() {
	auto md = ui->plainTextEdit->toPlainText();
	QElapsedTimer timer;
    timer.start();
	ui->textEdit->setMarkdown(md);
	// ※ QTextDocumentの内部レイアウト計算を強制完了させて描画遅延も含める場合
    //ui->textEdit->document()->documentLayout()->requestUpdate();
    qint64 elapsedMs = timer.elapsed();
    qDebug() << "[Benchmark] setMarkdown completed:" << elapsedMs << "ms (" 
             << timer.nsecsElapsed() / 1000.0 << "μs)";
}
