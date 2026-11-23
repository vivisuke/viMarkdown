#include "MainWindow.h"
#include "TextEdit.h"
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QLineEdit>
#include <QMessageBox>
#include <QSettings>
#include <QSpinBox>
#include <QSplitter>
#include <QTabWidget>
#include <QTextCursor>
#include <QTextEdit>
#include <QTextStream>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>
#include "GapBufferTest.h"
#include "LineMgrTest.h"
#include "ViewLineMgrTest.h"
#include "TextDocumentTest.h"

namespace {
const int N_RECENT_FILES = 10 + 26;
const QStringView KEY_RECENT_FILES(u"recentFilePaths");
const QStringView KEY_LINE_SPACING_PERCENT(u"editor/lineSpacingPercent");
const char PROP_FILE_PATH[] = "filePath";
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindowClass())
{
    ui->setupUi(this);
    ui->plainTextEdit->setFocusPolicy(Qt::ClickFocus);
    {
        QSettings settings;
        int pct = settings.value(KEY_LINE_SPACING_PERCENT, 0).toInt();
        pct = qBound(0, (pct / 10) * 10, 100);
        m_lineSpacingPercent = pct;
    }
    m_tabWidget = new QTabWidget(this);
    m_tabWidget->setTabsClosable(true);
    m_tabWidget->setMovable(true);
    m_tabWidget->setDocumentMode(true);
    setCentralWidget(m_tabWidget);
    connect(m_tabWidget, &QTabWidget::currentChanged, this, &MainWindow::onCurrentTabChanged);
    connect(m_tabWidget, &QTabWidget::tabCloseRequested, this, &MainWindow::onTabCloseRequested);

    TextEdit *initialEdit = addEditorTab(QStringLiteral("Untitled-%1").arg(++m_tabNumber));
    initialEdit->setFocus();
    QTimer::singleShot(0, initialEdit, qOverload<>(&QWidget::setFocus));
    connect(ui->action_Open, &QAction::triggered, this, &MainWindow::onOpen);
    connect(ui->action_Close, &QAction::triggered, this, &MainWindow::onClose);
    connect(ui->action_Settings, &QAction::triggered, this, &MainWindow::onSettings);
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

TextEdit *MainWindow::addEditorTab(const QString &title, const QString &filePath)
{
    auto *splitter = new QSplitter(Qt::Horizontal, m_tabWidget);
    auto *edit = new TextEdit(splitter);
    edit->setLineSpacing(qRound(edit->fontMetrics().height() * m_lineSpacingPercent / 100.0));
    auto *preview = new QTextEdit(splitter);
    splitter->addWidget(edit);
    splitter->addWidget(preview);
    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 1);
    splitter->setSizes({1000, 1000});
    splitter->setProperty(PROP_FILE_PATH, filePath);

    int index = m_tabWidget->addTab(splitter, title);
    if (!filePath.isEmpty())
        m_tabWidget->setTabToolTip(index, filePath);
    m_tabWidget->setCurrentIndex(index);
    return edit;
}

TextEdit *MainWindow::currentTextEdit() const
{
    if (!m_tabWidget)
        return nullptr;
    auto *splitter = qobject_cast<QSplitter *>(m_tabWidget->currentWidget());
    return splitter ? dynamic_cast<TextEdit *>(splitter->widget(0)) : nullptr;
}

QTextEdit *MainWindow::currentPreviewEdit() const
{
    if (!m_tabWidget)
        return nullptr;
    auto *splitter = qobject_cast<QSplitter *>(m_tabWidget->currentWidget());
    return splitter ? qobject_cast<QTextEdit *>(splitter->widget(1)) : nullptr;
}

int MainWindow::findTabByFilePath(const QString &cleanPath) const
{
    if (!m_tabWidget || cleanPath.isEmpty())
        return -1;
    for (int i = 0; i < m_tabWidget->count(); ++i) {
        QWidget *w = m_tabWidget->widget(i);
        if (w && w->property(PROP_FILE_PATH).toString().compare(cleanPath, Qt::CaseInsensitive) == 0)
            return i;
    }
    return -1;
}

void MainWindow::onCurrentTabChanged(int index)
{
    if (index < 0) {
        setWindowTitle(QStringLiteral("TestBuffer"));
        return;
    }
    setWindowTitle(m_tabWidget->tabText(index) + QStringLiteral(" - TestBuffer"));
    if (TextEdit *edit = currentTextEdit())
        edit->setFocus();
}

void MainWindow::onTabCloseRequested(int index)
{
    if (!m_tabWidget || index < 0 || index >= m_tabWidget->count())
        return;
    QWidget *w = m_tabWidget->widget(index);
    m_tabWidget->removeTab(index);
    if (w)
        w->deleteLater();
}

void MainWindow::onClose()
{
    if (m_tabWidget)
        onTabCloseRequested(m_tabWidget->currentIndex());
}

void MainWindow::applyLineSpacing(int percent)
{
    m_lineSpacingPercent = qBound(0, (percent / 10) * 10, 100);
    if (!m_tabWidget)
        return;
    for (int i = 0; i < m_tabWidget->count(); ++i) {
        auto *splitter = qobject_cast<QSplitter *>(m_tabWidget->widget(i));
        auto *edit = splitter ? dynamic_cast<TextEdit *>(splitter->widget(0)) : nullptr;
        if (edit)
            edit->setLineSpacing(qRound(edit->fontMetrics().height() * m_lineSpacingPercent / 100.0));
    }
}

void MainWindow::onSettings()
{
    const int origPercent = m_lineSpacingPercent;

    QDialog dlg(this);
    dlg.setWindowTitle(tr("Settings"));
    dlg.resize(320, 160);
    auto *layout = new QVBoxLayout(&dlg);

    auto *form = new QFormLayout();
    auto *lineSpacingSpin = new QSpinBox(&dlg);
    lineSpacingSpin->setRange(0, 100);
    lineSpacingSpin->setSingleStep(10);
    lineSpacingSpin->setSuffix(QStringLiteral("%"));
    lineSpacingSpin->setValue(m_lineSpacingPercent);
    if (QLineEdit *le = lineSpacingSpin->findChild<QLineEdit *>()) {
        le->setReadOnly(true);
        le->setFocusPolicy(Qt::NoFocus);
    }
    connect(lineSpacingSpin, &QSpinBox::valueChanged, this, [this](int val) {
        applyLineSpacing(val);
    });
    form->addRow(tr("Line Spacing:"), lineSpacingSpin);
    layout->addLayout(form);
    layout->addStretch();

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    layout->addWidget(buttons);

    if (dlg.exec() == QDialog::Accepted) {
        applyLineSpacing(lineSpacingSpin->value());
        QSettings settings;
        settings.setValue(KEY_LINE_SPACING_PERCENT, m_lineSpacingPercent);
    } else {
        applyLineSpacing(origPercent);
    }
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
    QFileInfo fi(filePath);
    QString cleanPath = QDir::cleanPath(fi.absoluteFilePath());

    int existing = findTabByFilePath(cleanPath);
    if (existing >= 0) {
        m_tabWidget->setCurrentIndex(existing);
        if (TextEdit *edit = currentTextEdit())
            edit->setFocus();
        return true;
    }

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::warning(this, tr("Open Error"),
            tr("Could not open file:\n%1\n\n%2").arg(filePath, file.errorString()));
        return false;
    }

    QTextStream in(&file);
    QString text = in.readAll();
    file.close();

    const bool isMarkdown = (fi.suffix().compare(QStringLiteral("md"), Qt::CaseInsensitive) == 0
        || fi.suffix().compare(QStringLiteral("markdown"), Qt::CaseInsensitive) == 0);
    if (isMarkdown) {
        text.replace(QChar('\t'), QStringLiteral("  "));
    }
    m_lastDir = fi.absolutePath();

    // カレントタブが未保存変更なしの空の無題タブなら再利用し、そうでなければ新規タブを追加
    QWidget *curTab = m_tabWidget->currentWidget();
    TextEdit *edit = currentTextEdit();
    bool reuseTab = (curTab != nullptr && edit != nullptr
        && curTab->property(PROP_FILE_PATH).toString().isEmpty()
        && !edit->document()->isModified()
        && edit->document()->characterCount() == 0);
    if (reuseTab) {
        int ix = m_tabWidget->currentIndex();
        curTab->setProperty(PROP_FILE_PATH, cleanPath);
        m_tabWidget->setTabText(ix, fi.fileName());
        m_tabWidget->setTabToolTip(ix, cleanPath);
    } else {
        edit = addEditorTab(fi.fileName(), cleanPath);
    }

    edit->setPlainText(text);
    edit->document()->setModified(false);
    if (QTextEdit *preview = currentPreviewEdit()) {
        if (isMarkdown) {
            preview->document()->setBaseUrl(QUrl::fromLocalFile(fi.absolutePath() + QLatin1Char('/')));
            preview->setMarkdown(text);
        } else {
            preview->clear();
        }
    }
    setWindowTitle(fi.fileName() + QStringLiteral(" - TestBuffer"));
    statusBar()->showMessage(
        tr("Opened: %1 (%2 chars, %3 lines)")
            .arg(cleanPath)
            .arg(edit->document()->characterCount())
            .arg(edit->document()->lineMgr().lineCount()),
        5000);
    addToRecentFiles(cleanPath);
    edit->setFocus();
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

