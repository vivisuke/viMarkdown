#include "MainWindow.h"
#include "TextEdit.h"
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QMessageBox>
#include <QSettings>
#include <QTextCursor>
#include <QTextStream>
#include <QTimer>
#include "GapBufferTest.h"
#include "LineMgrTest.h"
#include "ViewLineMgrTest.h"
#include "TextDocumentTest.h"

namespace {
const int N_RECENT_FILES = 10 + 26;
const QStringView KEY_RECENT_FILES(u"recentFilePaths");
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindowClass())
{
    ui->setupUi(this);
    ui->plainTextEdit->setFocusPolicy(Qt::ClickFocus);
    m_textEdit = new TextEdit(this);
    setCentralWidget(m_textEdit);
    m_textEdit->setFocus();
    QTimer::singleShot(0, m_textEdit, qOverload<>(&QWidget::setFocus));
    connect(ui->action_Open, &QAction::triggered, this, &MainWindow::onOpen);
    connect(ui->menu_RecentFiles, &QMenu::aboutToShow, this, &MainWindow::onAboutToShow_RecentFiles);
    connect(ui->action_gap_buffer, &QAction::triggered, this, [this]() {
        do_test_gapBuffer(this);
    });
    connect(ui->action_PieceTable, &QAction::triggered, this, [this]() {
        do_test_pieceTable(this);
    });
    connect(ui->action_replace_All, &QAction::triggered, this, [this]() {
        do_perf_replaceAll(this);
    });
    connect(ui->action_LineMgr, &QAction::triggered, this, [this]() {
        do_test_lineMgr(this);
    });
    connect(ui->action_ViewLineMgr, &QAction::triggered, this, [this]() {
        do_test_viewLineMgr(this);
    });
    connect(ui->action_TextDocument, &QAction::triggered, this, [this]() {
        do_test_textDocument(this);
    });
}

void MainWindow::onOpen()
{
    QString filePath = QFileDialog::getOpenFileName(
        this,
        tr("Open File"),
        m_lastDir,
        tr("All Files (*);;Text Files (*.txt *.md *.cpp *.h)"));
    if (filePath.isEmpty())
        return;

    openFile(filePath);
}

bool MainWindow::openFile(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::warning(this, tr("Open Error"),
            tr("Could not open file:\n%1\n\n%2").arg(filePath, file.errorString()));
        return false;
    }

    QTextStream in(&file);
    QString text = in.readAll();
    file.close();

    QFileInfo fi(filePath);
    if (fi.suffix().compare(QStringLiteral("md"), Qt::CaseInsensitive) == 0) {
        text.replace(QChar('\t'), QStringLiteral("  "));
    }
    QString cleanPath = QDir::cleanPath(fi.absoluteFilePath());
    m_lastDir = fi.absolutePath();
    m_textEdit->setPlainText(text);
    m_textEdit->document()->setModified(false);
    setWindowTitle(fi.fileName() + QStringLiteral(" - TestBuffer"));
    statusBar()->showMessage(
        tr("Opened: %1 (%2 chars, %3 lines)")
            .arg(cleanPath)
            .arg(m_textEdit->document()->characterCount())
            .arg(m_textEdit->document()->lineMgr().lineCount()),
        5000);
    addToRecentFiles(cleanPath);
    m_textEdit->setFocus();
    return true;
}

void MainWindow::addToRecentFiles(const QString &fullPath)
{
    QSettings settings;
    QStringList recentFilePaths = settings.value(KEY_RECENT_FILES).toStringList();
    int ix;
    while ((ix = recentFilePaths.indexOf(fullPath)) >= 0)
        recentFilePaths.removeAt(ix);
    recentFilePaths.push_front(fullPath);
    while (recentFilePaths.size() > N_RECENT_FILES)
        recentFilePaths.pop_back();
    settings.setValue(KEY_RECENT_FILES, recentFilePaths);
}

void MainWindow::onAboutToShow_RecentFiles()
{
    ui->menu_RecentFiles->clear();
    QSettings settings;
    QStringList recentFilePaths = settings.value(KEY_RECENT_FILES).toStringList();
    int k = 0;
    QString key;
    for (const QString &fullPathRaw : recentFilePaths) {
        if (++k <= 10)
            key = QString::number(k % 10);
        else
            key = QChar(u'A' + k - 11);
        QString fullPath = QDir::cleanPath(fullPathRaw);
        QAction *act = ui->menu_RecentFiles->addAction("&" + key + " " + fullPath);
        connect(act, &QAction::triggered, this, [this, fullPath, fullPathRaw]() {
            if (!openFile(fullPath)) {
                QSettings settings;
                QStringList recentFilePaths = settings.value(KEY_RECENT_FILES).toStringList();
                recentFilePaths.removeAll(fullPath);
                recentFilePaths.removeAll(fullPathRaw);
                settings.setValue(KEY_RECENT_FILES, recentFilePaths);
            }
        });
    }
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

