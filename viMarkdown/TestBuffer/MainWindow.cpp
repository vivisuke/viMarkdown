#include "MainWindow.h"
#include "MarkdownPreview.h"
#include "TextEdit.h"
#include <QCloseEvent>
#include <QColorDialog>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QDockWidget>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFontComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollBar>
#include <QSettings>
#include <QSpinBox>
#include <QSplitter>
#include <QTabWidget>
#include <QTextCursor>
#include <QTextEdit>
#include <QTextStream>
#include <QTimer>
#include <QTreeWidget>
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
const QStringView KEY_HEADING_COLOR(u"editor/headingColor");
const QStringView KEY_EMPHASIS_COLOR(u"editor/emphasisColor");
const QStringView KEY_EDITOR_FONT_FAMILY(u"editor/fontFamily");
const QStringView KEY_EDITOR_FONT_SIZE(u"editor/fontSize");
const QStringView KEY_PREVIEW_FONT_FAMILY(u"preview/fontFamily");
const QStringView KEY_PREVIEW_FONT_SIZE(u"preview/fontSize");
const QStringView KEY_WINDOW_GEOMETRY(u"window/geometry");
const QStringView KEY_WINDOW_STATE(u"window/state");
const char PROP_FILE_PATH[] = "filePath";

void setColorButtonStyle(QPushButton *button, const QColor &color)
{
    button->setStyleSheet(QStringLiteral(
        "background-color: %1; border: 1px solid gray; min-width: 60px; height: 22px;"
    ).arg(color.name()));
}
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
        QColor hc = settings.value(KEY_HEADING_COLOR, m_headingColor).value<QColor>();
        if (hc.isValid()) m_headingColor = hc;
        QColor ec = settings.value(KEY_EMPHASIS_COLOR, m_emphasisColor).value<QColor>();
        if (ec.isValid()) m_emphasisColor = ec;
        m_editorFontFamily = settings.value(KEY_EDITOR_FONT_FAMILY).toString();
        m_editorFontSize = qBound(3, settings.value(KEY_EDITOR_FONT_SIZE, 10).toInt(), 64);
        m_previewFontFamily = settings.value(KEY_PREVIEW_FONT_FAMILY).toString();
        m_previewFontSize = qBound(3, settings.value(KEY_PREVIEW_FONT_SIZE, 10).toInt(), 64);
    }
    m_tabWidget = new QTabWidget(this);
    m_tabWidget->setTabsClosable(true);
    m_tabWidget->setMovable(true);
    m_tabWidget->setDocumentMode(true);
    setCentralWidget(m_tabWidget);
    connect(m_tabWidget, &QTabWidget::currentChanged, this, &MainWindow::onCurrentTabChanged);
    connect(m_tabWidget, &QTabWidget::tabCloseRequested, this, &MainWindow::onTabCloseRequested);

    m_outlineDock = new QDockWidget(tr("OutlineBar"), this);
    m_outlineDock->setObjectName(QStringLiteral("outlineBar"));
    m_outlineTree = new QTreeWidget(m_outlineDock);
    m_outlineTree->setHeaderHidden(true);
    m_outlineTree->setIndentation(14);
    m_outlineDock->setWidget(m_outlineTree);
    addDockWidget(Qt::LeftDockWidgetArea, m_outlineDock);
    connect(m_outlineTree, &QTreeWidget::itemClicked, this, &MainWindow::onOutlineItemClicked);

    QMenu *viewMenu = new QMenu(tr("&View"), this);
    QAction *outlineAct = m_outlineDock->toggleViewAction();
    outlineAct->setText(tr("&OutlineBar"));
    QAction *outputAct = ui->outputBar->toggleViewAction();
    outputAct->setText(tr("Outpu&tBar"));
    viewMenu->addAction(outlineAct);
    viewMenu->addAction(outputAct);
    menuBar()->insertMenu(ui->menuTest->menuAction(), viewMenu);
#if defined(Q_OS_WIN) || defined(Q_OS_LINUX)
    //	メニューバーをメインツールバーに統合（viMarkdown と同じ方式）
    ui->mainToolBar->insertWidget(ui->action_Undo, ui->menuBar);
#endif

    TextEdit *initialEdit = addEditorTab(QStringLiteral("Untitled-%1").arg(++m_tabNumber));
    initialEdit->setFocus();
    QTimer::singleShot(0, initialEdit, qOverload<>(&QWidget::setFocus));
    {
        QSettings settings;
        const QByteArray geom = settings.value(KEY_WINDOW_GEOMETRY).toByteArray();
        if (!geom.isEmpty())
            restoreGeometry(geom);
        const QByteArray state = settings.value(KEY_WINDOW_STATE).toByteArray();
        if (!state.isEmpty())
            restoreState(state);
    }
    connect(ui->action_NewTab, &QAction::triggered, this, &MainWindow::onNewTab);
    connect(ui->action_Open, &QAction::triggered, this, &MainWindow::onOpen);
    connect(ui->action_Save, &QAction::triggered, this, &MainWindow::onSave);
    connect(ui->action_Close, &QAction::triggered, this, &MainWindow::onClose);
    connect(ui->action_Undo, &QAction::triggered, this, &MainWindow::onUndo);
    connect(ui->action_Redo, &QAction::triggered, this, &MainWindow::onRedo);
    connect(ui->action_Cut, &QAction::triggered, this, [this]() {
        if (TextEdit *edit = currentTextEdit())
            edit->cut();
    });
    connect(ui->action_Copy, &QAction::triggered, this, [this]() {
        if (QWidget *fw = QApplication::focusWidget()) {
            if (auto *te = qobject_cast<QTextEdit *>(fw)) { te->copy(); return; }
            if (auto *pte = qobject_cast<QPlainTextEdit *>(fw)) { pte->copy(); return; }
        }
        if (TextEdit *edit = currentTextEdit())
            edit->copy();
    });
    connect(ui->action_Paste, &QAction::triggered, this, [this]() {
        if (TextEdit *edit = currentTextEdit())
            edit->paste();
    });
    connect(ui->action_SelectAll, &QAction::triggered, this, [this]() {
        if (QWidget *fw = QApplication::focusWidget()) {
            if (auto *te = qobject_cast<QTextEdit *>(fw)) { te->selectAll(); return; }
            if (auto *pte = qobject_cast<QPlainTextEdit *>(fw)) { pte->selectAll(); return; }
        }
        if (TextEdit *edit = currentTextEdit())
            edit->selectAll();
    });
    updateUndoRedoEnabled();
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
    {
        QFont f = edit->font();
        if (!m_editorFontFamily.isEmpty())
            f.setFamily(m_editorFontFamily);
        else
            m_editorFontFamily = f.family();
        f.setPointSize(m_editorFontSize);
        edit->setEditorFont(f);
    }
    edit->setLineSpacing(qRound(edit->fontMetrics().height() * m_lineSpacingPercent / 100.0));
    edit->setHeadingColor(m_headingColor);
    edit->setEmphasisColor(m_emphasisColor);
    auto *preview = new MarkdownPreview(splitter);
    {
        QFont pf = preview->font();
        if (!m_previewFontFamily.isEmpty())
            pf.setFamily(m_previewFontFamily);
        else
            m_previewFontFamily = pf.family();
        pf.setPointSize(m_previewFontSize);
        preview->setFont(pf);
        preview->document()->setDefaultFont(pf);
    }
    preview->setFontSizeChangeCallback([this](int delta) {
        const int newSize = qBound(3, m_previewFontSize + delta, 64);
        if (newSize != m_previewFontSize) {
            applyPreviewFont(m_previewFontFamily, newSize);
            QSettings settings;
            settings.setValue(KEY_PREVIEW_FONT_SIZE, m_previewFontSize);
        }
        statusBar()->showMessage(tr("Preview Font Size: %1 pt").arg(m_previewFontSize), 5000);
    });
    edit->setTextChangedCallback([this, splitter, edit, preview]() {
        preview->setMarkdown(edit->document());
        updateOutlineForTab(splitter);
        updateUndoRedoEnabled();
    });
    edit->setFontSizeChangeCallback([this](int delta) {
        const int newSize = qBound(3, m_editorFontSize + delta, 64);
        if (newSize != m_editorFontSize) {
            applyEditorFont(m_editorFontFamily, newSize);
            QSettings settings;
            settings.setValue(KEY_EDITOR_FONT_SIZE, m_editorFontSize);
        }
        statusBar()->showMessage(tr("Editor Font Size: %1 pt").arg(m_editorFontSize), 5000);
    });
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
    updateOutlineForTab(splitter);
    return edit;
}

TextEdit *MainWindow::currentTextEdit() const
{
    if (!m_tabWidget)
        return nullptr;
    auto *splitter = qobject_cast<QSplitter *>(m_tabWidget->currentWidget());
    return splitter ? dynamic_cast<TextEdit *>(splitter->widget(0)) : nullptr;
}

MarkdownPreview *MainWindow::currentPreviewEdit() const
{
    if (!m_tabWidget)
        return nullptr;
    auto *splitter = qobject_cast<QSplitter *>(m_tabWidget->currentWidget());
    return splitter ? dynamic_cast<MarkdownPreview *>(splitter->widget(1)) : nullptr;
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
        updateUndoRedoEnabled();
        return;
    }
    setWindowTitle(m_tabWidget->tabText(index) + QStringLiteral(" - TestBuffer"));
    updateUndoRedoEnabled();
    if (TextEdit *edit = currentTextEdit())
        edit->setFocus();
}

void MainWindow::onTabCloseRequested(int index)
{
    if (!m_tabWidget || index < 0 || index >= m_tabWidget->count())
        return;
    QWidget *w = m_tabWidget->widget(index);
    removeOutlineForTab(w);
    m_tabWidget->removeTab(index);
    if (w)
        w->deleteLater();
}

void MainWindow::onClose()
{
    if (m_tabWidget)
        onTabCloseRequested(m_tabWidget->currentIndex());
}

void MainWindow::onUndo()
{
    if (TextEdit *edit = currentTextEdit())
        edit->undo();
    updateUndoRedoEnabled();
}

void MainWindow::onRedo()
{
    if (TextEdit *edit = currentTextEdit())
        edit->redo();
    updateUndoRedoEnabled();
}

void MainWindow::updateUndoRedoEnabled()
{
    TextEdit *edit = currentTextEdit();
    ui->action_Undo->setEnabled(edit && edit->document()->canUndo());
    ui->action_Redo->setEnabled(edit && edit->document()->canRedo());
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

void MainWindow::applyColors(const QColor &heading, const QColor &emphasis)
{
    if (heading.isValid()) m_headingColor = heading;
    if (emphasis.isValid()) m_emphasisColor = emphasis;
    if (!m_tabWidget)
        return;
    for (int i = 0; i < m_tabWidget->count(); ++i) {
        auto *splitter = qobject_cast<QSplitter *>(m_tabWidget->widget(i));
        auto *edit = splitter ? dynamic_cast<TextEdit *>(splitter->widget(0)) : nullptr;
        if (edit) {
            edit->setHeadingColor(m_headingColor);
            edit->setEmphasisColor(m_emphasisColor);
        }
    }
}

void MainWindow::applyEditorFont(const QString &family, int pointSize)
{
    if (!family.isEmpty())
        m_editorFontFamily = family;
    m_editorFontSize = qBound(3, pointSize, 64);
    if (!m_tabWidget)
        return;
    for (int i = 0; i < m_tabWidget->count(); ++i) {
        auto *splitter = qobject_cast<QSplitter *>(m_tabWidget->widget(i));
        auto *edit = splitter ? dynamic_cast<TextEdit *>(splitter->widget(0)) : nullptr;
        if (edit) {
            QFont f = edit->font();
            if (!m_editorFontFamily.isEmpty())
                f.setFamily(m_editorFontFamily);
            f.setPointSize(m_editorFontSize);
            edit->setEditorFont(f);
            edit->setLineSpacing(qRound(edit->fontMetrics().height() * m_lineSpacingPercent / 100.0));
        }
    }
}

void MainWindow::applyPreviewFont(const QString &family, int pointSize)
{
    if (!family.isEmpty())
        m_previewFontFamily = family;
    m_previewFontSize = qBound(3, pointSize, 64);
    if (!m_tabWidget)
        return;
    for (int i = 0; i < m_tabWidget->count(); ++i) {
        auto *splitter = qobject_cast<QSplitter *>(m_tabWidget->widget(i));
        auto *edit = splitter ? dynamic_cast<TextEdit *>(splitter->widget(0)) : nullptr;
        auto *preview = splitter ? dynamic_cast<MarkdownPreview *>(splitter->widget(1)) : nullptr;
        if (preview) {
            QFont f = preview->font();
            if (!m_previewFontFamily.isEmpty())
                f.setFamily(m_previewFontFamily);
            f.setPointSize(m_previewFontSize);
            preview->setFont(f);
            preview->document()->setDefaultFont(f);
            if (edit && edit->markdownHighlight())
                preview->setMarkdown(edit->document());
        }
    }
}

void MainWindow::onSettings()
{
    const int origPercent = m_lineSpacingPercent;
    const QColor origHeading = m_headingColor;
    const QColor origEmphasis = m_emphasisColor;
    const QString origFontFamily = m_editorFontFamily;
    const int origFontSize = m_editorFontSize;
    const QString origPreviewFontFamily = m_previewFontFamily;
    const int origPreviewFontSize = m_previewFontSize;

    QDialog dlg(this);
    dlg.setWindowTitle(tr("Settings"));
    dlg.resize(620, 260);
    auto *layout = new QVBoxLayout(&dlg);
    auto *groupsRow = new QHBoxLayout();

    auto *editorGroup = new QGroupBox(tr("Editor"), &dlg);
    auto *editorForm = new QFormLayout(editorGroup);
    auto *fontCombo = new QFontComboBox(editorGroup);
    fontCombo->setFontFilters(QFontComboBox::MonospacedFonts);
    if (!m_editorFontFamily.isEmpty())
        fontCombo->setCurrentFont(QFont(m_editorFontFamily));
    connect(fontCombo, &QFontComboBox::currentFontChanged, this, [this](const QFont &f) {
        applyEditorFont(f.family(), m_editorFontSize);
    });
    editorForm->addRow(tr("Font:"), fontCombo);

    auto *fontSizeSpin = new QSpinBox(editorGroup);
    fontSizeSpin->setRange(3, 64);
    fontSizeSpin->setSingleStep(1);
    fontSizeSpin->setSuffix(QStringLiteral(" pt"));
    fontSizeSpin->setValue(m_editorFontSize);
    connect(fontSizeSpin, &QSpinBox::valueChanged, this, [this](int val) {
        applyEditorFont(m_editorFontFamily, val);
    });
    editorForm->addRow(tr("Font Size:"), fontSizeSpin);

    auto *lineSpacingSpin = new QSpinBox(editorGroup);
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
    editorForm->addRow(tr("Line Spacing:"), lineSpacingSpin);

    auto *headingColorBtn = new QPushButton(editorGroup);
    setColorButtonStyle(headingColorBtn, m_headingColor);
    connect(headingColorBtn, &QPushButton::clicked, &dlg, [this, &dlg, headingColorBtn]() {
        QColor c = QColorDialog::getColor(m_headingColor, &dlg, tr("Select Heading Color"));
        if (c.isValid()) {
            setColorButtonStyle(headingColorBtn, c);
            applyColors(c, m_emphasisColor);
        }
    });
    editorForm->addRow(tr("Heading Color:"), headingColorBtn);

    auto *emphasisColorBtn = new QPushButton(editorGroup);
    setColorButtonStyle(emphasisColorBtn, m_emphasisColor);
    connect(emphasisColorBtn, &QPushButton::clicked, &dlg, [this, &dlg, emphasisColorBtn]() {
        QColor c = QColorDialog::getColor(m_emphasisColor, &dlg, tr("Select Emphasis Color"));
        if (c.isValid()) {
            setColorButtonStyle(emphasisColorBtn, c);
            applyColors(m_headingColor, c);
        }
    });
    editorForm->addRow(tr("Emphasis Color:"), emphasisColorBtn);
    groupsRow->addWidget(editorGroup, 1);

    auto *previewGroup = new QGroupBox(tr("Preview"), &dlg);
    auto *previewForm = new QFormLayout(previewGroup);
    auto *previewFontCombo = new QFontComboBox(previewGroup);
    if (!m_previewFontFamily.isEmpty())
        previewFontCombo->setCurrentFont(QFont(m_previewFontFamily));
    connect(previewFontCombo, &QFontComboBox::currentFontChanged, this, [this](const QFont &f) {
        applyPreviewFont(f.family(), m_previewFontSize);
    });
    previewForm->addRow(tr("Font:"), previewFontCombo);

    auto *previewFontSizeSpin = new QSpinBox(previewGroup);
    previewFontSizeSpin->setRange(3, 64);
    previewFontSizeSpin->setSingleStep(1);
    previewFontSizeSpin->setSuffix(QStringLiteral(" pt"));
    previewFontSizeSpin->setValue(m_previewFontSize);
    connect(previewFontSizeSpin, &QSpinBox::valueChanged, this, [this](int val) {
        applyPreviewFont(m_previewFontFamily, val);
    });
    previewForm->addRow(tr("Font Size:"), previewFontSizeSpin);
    groupsRow->addWidget(previewGroup, 1);

    layout->addLayout(groupsRow);
    layout->addStretch();

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    layout->addWidget(buttons);

    if (dlg.exec() == QDialog::Accepted) {
        applyEditorFont(fontCombo->currentFont().family(), fontSizeSpin->value());
        applyPreviewFont(previewFontCombo->currentFont().family(), previewFontSizeSpin->value());
        applyLineSpacing(lineSpacingSpin->value());
        QSettings settings;
        settings.setValue(KEY_EDITOR_FONT_FAMILY, m_editorFontFamily);
        settings.setValue(KEY_EDITOR_FONT_SIZE, m_editorFontSize);
        settings.setValue(KEY_PREVIEW_FONT_FAMILY, m_previewFontFamily);
        settings.setValue(KEY_PREVIEW_FONT_SIZE, m_previewFontSize);
        settings.setValue(KEY_LINE_SPACING_PERCENT, m_lineSpacingPercent);
        settings.setValue(KEY_HEADING_COLOR, m_headingColor);
        settings.setValue(KEY_EMPHASIS_COLOR, m_emphasisColor);
    } else {
        applyEditorFont(origFontFamily, origFontSize);
        applyPreviewFont(origPreviewFontFamily, origPreviewFontSize);
        applyLineSpacing(origPercent);
        applyColors(origHeading, origEmphasis);
    }
}

void MainWindow::onNewTab()
{
    TextEdit *edit = addEditorTab(QStringLiteral("Untitled-%1").arg(++m_tabNumber));
    edit->setFocus();
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

void MainWindow::onSave()
{
    TextEdit *edit = currentTextEdit();
    if (!edit || !m_tabWidget)
        return;
    const int ix = m_tabWidget->currentIndex();
    QWidget *curTab = m_tabWidget->widget(ix);
    if (!curTab)
        return;

    QString filePath = curTab->property(PROP_FILE_PATH).toString();
    if (filePath.isEmpty()) {
        filePath = QFileDialog::getSaveFileName(
            this,
            tr("Save File"),
            m_lastDir,
            tr("Markdown Files (*.md);;Text Files (*.txt);;All Files (*)"));
        if (filePath.isEmpty())
            return;
        QFileInfo fi(filePath);
        filePath = QDir::cleanPath(fi.absoluteFilePath());
        curTab->setProperty(PROP_FILE_PATH, filePath);
        m_tabWidget->setTabText(ix, fi.fileName());
        m_tabWidget->setTabToolTip(ix, filePath);
        setWindowTitle(fi.fileName() + QStringLiteral(" - TestBuffer"));
        updateOutlineForTab(curTab);
        m_lastDir = fi.absolutePath();
        addToRecentFiles(filePath);
    }

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, tr("Save Error"),
            tr("Could not save file:\n%1\n\n%2").arg(filePath, file.errorString()));
        return;
    }
    QTextStream out(&file);
    out << edit->toPlainText();
    file.close();

    edit->onSaved();
    statusBar()->showMessage(tr("Saved: %1").arg(filePath), 5000);
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

    edit->setMarkdownHighlight(isMarkdown);
    edit->setPlainText(text);
    edit->document()->setModified(false);
    if (MarkdownPreview *preview = currentPreviewEdit()) {
        if (isMarkdown) {
            preview->document()->setBaseUrl(QUrl::fromLocalFile(fi.absolutePath() + QLatin1Char('/')));
            preview->setMarkdown(edit->document());
            edit->viewport()->update();
        } else {
            preview->clear();
        }
    }
    setWindowTitle(fi.fileName() + QStringLiteral(" - TestBuffer"));
    updateOutlineForTab(m_tabWidget->currentWidget());
    updateUndoRedoEnabled();
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

void MainWindow::closeEvent(QCloseEvent *event)
{
    QSettings settings;
    settings.setValue(KEY_WINDOW_GEOMETRY, saveGeometry());
    settings.setValue(KEY_WINDOW_STATE, saveState());
    QMainWindow::closeEvent(event);
}

QTreeWidgetItem *MainWindow::findOutlineTopItem(QWidget *tabWidget) const
{
    if (!m_outlineTree || !tabWidget)
        return nullptr;
    const quintptr key = reinterpret_cast<quintptr>(tabWidget);
    for (int i = 0; i < m_outlineTree->topLevelItemCount(); ++i) {
        QTreeWidgetItem *item = m_outlineTree->topLevelItem(i);
        if (item && item->data(0, Qt::UserRole).value<quintptr>() == key)
            return item;
    }
    return nullptr;
}

void MainWindow::removeOutlineForTab(QWidget *tabWidget)
{
    QTreeWidgetItem *top = findOutlineTopItem(tabWidget);
    delete top;
}

void MainWindow::updateOutlineForTab(QWidget *tabWidget)
{
    if (!m_outlineTree || !m_tabWidget || !tabWidget)
        return;
    const int tabIdx = m_tabWidget->indexOf(tabWidget);
    if (tabIdx < 0)
        return;

    QTreeWidgetItem *top = findOutlineTopItem(tabWidget);
    if (!top) {
        top = new QTreeWidgetItem(m_outlineTree);
        top->setData(0, Qt::UserRole, QVariant::fromValue(reinterpret_cast<quintptr>(tabWidget)));
    }
    top->setText(0, m_tabWidget->tabText(tabIdx));
    qDeleteAll(top->takeChildren());

    auto *splitter = qobject_cast<QSplitter *>(tabWidget);
    auto *edit = splitter ? dynamic_cast<TextEdit *>(splitter->widget(0)) : nullptr;
    if (!edit || !edit->markdownHighlight()) {
        top->setExpanded(true);
        return;
    }

    TextDocument *doc = edit->document();
    const ViewLineMgr &mgr = doc->lineMgr();
    const int nLines = mgr.lineCount();

    QVector<QPair<int, QTreeWidgetItem *>> stack;
    stack.push_back({0, top});
    for (int ln = 0; ln < nLines; ++ln) {
        if (mgr.lineType(ln) != LineMgr::LT_HEADING)
            continue;
        QString line = doc->findBlockByNumber(ln).text();
        if (line.endsWith(QChar('\n'))) line.chop(1);
        if (line.endsWith(QChar('\r'))) line.chop(1);

        int level = 0;
        while (level < line.size() && level < 6 && line[level] == QChar('#'))
            ++level;
        if (level == 0)
            continue;
        qsizetype pos = level;
        while (pos < line.size() && (line[pos] == QChar(' ') || line[pos] == QChar('\t')))
            ++pos;
        const QString headingText = line.mid(pos);

        while (stack.size() > 1 && stack.back().first >= level)
            stack.pop_back();
        auto *item = new QTreeWidgetItem(stack.back().second);
        item->setText(0, QStringLiteral("▪ ") + headingText);
        item->setData(0, Qt::UserRole, ln);
        item->setData(0, Qt::UserRole + 1, QVariant::fromValue(reinterpret_cast<quintptr>(tabWidget)));
        stack.back().second->setExpanded(true);
        stack.push_back({level, item});
    }
    top->setExpanded(true);
}

void MainWindow::onOutlineItemClicked(QTreeWidgetItem *item, int column)
{
    Q_UNUSED(column);
    if (!item || !m_tabWidget)
        return;

    QWidget *tabWidget = nullptr;
    int targetLine = -1;
    if (item->parent() == nullptr) {
        tabWidget = reinterpret_cast<QWidget *>(item->data(0, Qt::UserRole).value<quintptr>());
    } else {
        targetLine = item->data(0, Qt::UserRole).toInt();
        tabWidget = reinterpret_cast<QWidget *>(item->data(0, Qt::UserRole + 1).value<quintptr>());
    }
    const int idx = m_tabWidget->indexOf(tabWidget);
    if (idx < 0)
        return;
    m_tabWidget->setCurrentIndex(idx);

    auto *splitter = qobject_cast<QSplitter *>(tabWidget);
    auto *edit = splitter ? dynamic_cast<TextEdit *>(splitter->widget(0)) : nullptr;
    if (!edit)
        return;
    if (targetLine >= 0) {
        const ViewLineMgr &mgr = edit->document()->lineMgr();
        if (targetLine < mgr.lineCount()) {
            TextCursor cur(edit->document());
            cur.setPosition(mgr.lineStartPos(targetLine));
            edit->setTextCursor(cur);
            edit->verticalScrollBar()->setValue(mgr.lineToViewLine(targetLine));
        }
    }
    edit->setFocus();
}

MainWindow::~MainWindow()
{
    delete ui;
}

