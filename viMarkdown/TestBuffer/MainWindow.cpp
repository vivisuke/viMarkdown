#include "MainWindow.h"
#include "TextEdit.h"
#include <QTextCursor>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindowClass())
{
    ui->setupUi(this);
    m_textEdit = new TextEdit(this);
    setCentralWidget(m_textEdit);
    m_textEdit->setFocus();
}

void MainWindow::do_output(const QString& txt)
{
    ui->outputBar->setVisible(true);
    QTextCursor cursor = ui->plainTextEdit->textCursor();
    cursor.movePosition(QTextCursor::End);
    cursor.insertText(txt);
    ui->plainTextEdit->setTextCursor(cursor);
}

MainWindow::~MainWindow()
{
    delete ui;
}

