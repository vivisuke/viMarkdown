#include "MainWindow.h"
#include "GapBufferTest.h"
#include "TextDocument.h"
#include "LineMgrTest.h"
#include "VLineMgrTest.h"
#include "TextDocumentTest.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindowClass())
{
    ui->setupUi(this);
    //do_output("hello\n");
    connect(ui->action_AutoTest, &QAction::triggered, this, &MainWindow::onAction_AutoTest);
    connect(ui->action_PerformanceTest, &QAction::triggered, this, &MainWindow::onAction_PerformanceTest);
    connect(ui->action_TestLineMgr, &QAction::triggered, this, &MainWindow::onAction_TestLineMgr);
    connect(ui->action_TestVLineMgr, &QAction::triggered, this, &MainWindow::onAction_TestVLineMgr);
    connect(ui->action_TestTextDocument, &QAction::triggered, this, &MainWindow::onAction_TestTextDocument);
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::do_output(const QString& txt)
{
    ui->outputBar->setVisible(true);
    QTextCursor cursor = ui->plainTextEdit->textCursor();
    cursor.movePosition(QTextCursor::End);
    cursor.insertText(txt);
    ui->plainTextEdit->setTextCursor(cursor);
}


void MainWindow::onAction_AutoTest()
{
    do_test_gapBuffer(this);
}

void MainWindow::onAction_PerformanceTest()
{
    do_perf_gapBuffer(this);
}

void MainWindow::onAction_TestLineMgr()
{
    do_test_lineMgr(this);
}

void MainWindow::onAction_TestVLineMgr()
{
    do_test_vlineMgr(this);
}

void MainWindow::onAction_TestTextDocument()
{
    do_test_textDocument(this);
}
