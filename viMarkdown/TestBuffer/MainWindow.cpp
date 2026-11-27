#include "MainWindow.h"
#include "MarkdownPreview.h"
#include "TextEdit.h"
#include <QApplication>
#include <QCalendarWidget>
#include <QCheckBox>
#include <QCloseEvent>
#include <QColorDialog>
#include <QComboBox>
#include <QDate>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QDirIterator>
#include <QDockWidget>
#include <QElapsedTimer>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFontComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QIcon>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPainter>
#include <QPixmap>
#include <QPrinter>
#include <QPushButton>
#include <QSaveFile>
#include <QScrollBar>
#include <QSettings>
#include <QShortcut>
#include <QSpinBox>
#include <QSplitter>
#include <QStandardPaths>
#include <QStringDecoder>
#include <QStyle>
#include <QTabWidget>
#include <QTextBlock>
#include <QTextCharFormat>
#include <QTextCursor>
#include <QTextEdit>
#include <QTextStream>
#include <QTimer>
#include <QTreeWidget>
#include <QUrl>
#include <QVBoxLayout>
#include <memory>
#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <imm.h>
#pragma comment(lib, "imm32.lib")
#endif
#include "ver.h"
#include "GapBufferTest.h"
#include "LineMgrTest.h"
#include "ViewLineMgrTest.h"
#include "TextDocumentTest.h"
#include "ViTest.h"

namespace {
const int N_RECENT_FILES = 10 + 26;
const QStringView KEY_RECENT_FILES(u"recentFilePaths");
const QStringView KEY_FAVORITE_FILES(u"favoriteFilePaths");
const QStringView KEY_LINE_SPACING_PERCENT(u"editor/lineSpacingPercent");
const QStringView KEY_HEADING_COLOR(u"editor/headingColor");
const QStringView KEY_EMPHASIS_COLOR(u"editor/emphasisColor");
const QStringView KEY_SEARCH_MATCH_COLOR(u"editor/searchMatchColor");
const QStringView KEY_EDITOR_FONT_FAMILY(u"editor/fontFamily");
const QStringView KEY_EDITOR_FONT_SIZE(u"editor/fontSize");
const QStringView KEY_PREVIEW_FONT_FAMILY(u"preview/fontFamily");
const QStringView KEY_PREVIEW_FONT_SIZE(u"preview/fontSize");
const QStringView KEY_OUTPUT_FONT_SIZE(u"output/fontSize");
const QStringView KEY_TABLE_HEADER_COLOR(u"preview/tableHeaderColor");
const QStringView KEY_TABLE_ZEBRA_COLOR1(u"preview/tableZebraColor1");
const QStringView KEY_TABLE_ZEBRA_COLOR2(u"preview/tableZebraColor2");
const QStringView KEY_VI_KEYBIND(u"editor/viKeybind");
const QStringView KEY_SEARCH_HISTORY(u"search/history");
const QStringView KEY_REPLACE_HISTORY(u"search/replace");
const QStringView KEY_SEARCH_IGNORE_CASE(u"search/ignoreCase");
const QStringView KEY_SEARCH_REGEXP(u"search/regexp");
const QStringView KEY_GREP_DIR_HISTORY(u"search/grepDir");
const QStringView KEY_GREP_SUBDIR(u"search/grepSubdir");
const QStringView KEY_GREP_CLEAR_OUTPUT(u"search/grepClearOutput");
const QStringView KEY_GREP_FILTER(u"search/grepFilter");
const QStringView KEY_DIRBAR_FILTER(u"dirBar/filter");
const QStringView KEY_WINDOW_GEOMETRY(u"window/geometry");
const QStringView KEY_WINDOW_STATE(u"window/state");
const char PROP_FILE_PATH[] = "filePath";
const char PROP_ENCODING[] = "encoding";
const char PROP_WITH_BOM[] = "withBOM";
const char PROP_USE_CRLF[] = "useCRLF";

bool readTextFileWithMeta(const QString &filePath, QString &content,
                          QStringConverter::Encoding &encoding, bool &withBOM,
                          bool &useCRLF, QString &errorMsg)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        errorMsg = file.errorString();
        return false;
    }
    const QByteArray raw = file.readAll();
    file.close();

    withBOM = false;
    const auto bomEnc = QStringConverter::encodingForData(raw);
    if (bomEnc.has_value()) {
        encoding = bomEnc.value();
        withBOM = true;
        QStringDecoder dec(encoding);
        content = dec(raw);
    } else {
        QStringDecoder utf8Dec(QStringConverter::Utf8);
        QString decoded = utf8Dec(raw);
        if (!utf8Dec.hasError()) {
            encoding = QStringConverter::Utf8;
            content = decoded;
        } else {
            encoding = QStringConverter::System;
            QStringDecoder sysDec(encoding);
            content = sysDec(raw);
        }
    }
    useCRLF = content.contains(QStringLiteral("\r\n"));
    content.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
    content.replace(QChar('\r'), QChar('\n'));
    return true;
}

void setColorButtonStyle(QPushButton *button, const QColor &color)
{
    button->setStyleSheet(QStringLiteral(
        "background-color: %1; border: 1px solid gray; min-width: 60px; height: 22px;"
    ).arg(color.name()));
}

struct DiaryDayInfo {
    int totalTodo = 0;
    int openTodo = 0;
};

class DiaryCalendarWidget : public QCalendarWidget
{
public:
    explicit DiaryCalendarWidget(QWidget *parent = nullptr)
        : QCalendarWidget(parent) {}

    void clearDayInfo() {
        m_dayInfo.clear();
        updateCells();
    }
    void setDayInfo(const QDate &date, const DiaryDayInfo &di) {
        m_dayInfo[date] = di;
        updateCell(date);
    }

protected:
    void paintCell(QPainter *painter, const QRect &rect, QDate date) const override {
        auto it = m_dayInfo.constFind(date);
        if (it == m_dayInfo.constEnd()) {
            QCalendarWidget::paintCell(painter, rect, date);
            return;
        }
        const DiaryDayInfo &di = it.value();
        painter->save();
        if (di.totalTodo > 0) {
            if (di.openTodo == 0) {
                painter->fillRect(rect, QColor(0xC0, 0xF0, 0xC0));	//	全ToDo完了（淡緑）
            } else {
                painter->fillRect(rect, QColor(0xF0, 0xC0, 0xC0));	//	未完ToDo有り（淡赤）
                const int closedCount = di.totalTodo - di.openTodo;
                const int fillH = rect.height() * closedCount / di.totalTodo;
                if (fillH > 0) {
                    painter->fillRect(QRect(rect.left(), rect.top() + rect.height() - fillH, rect.width(), fillH),
                                      QColor(0xC0, 0xF0, 0xC0));
                }
            }
        } else {
            painter->fillRect(rect, QColor(0xC0, 0xC0, 0xF0));		//	Notesのみ（淡青）
        }
        painter->restore();
        QCalendarWidget::paintCell(painter, rect, date);
    }

private:
    QMap<QDate, DiaryDayInfo> m_dayInfo;
};
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindowClass())
{
    ui->setupUi(this);
    setFocusPolicy(Qt::StrongFocus);
    ui->plainTextEdit->setFocusPolicy(Qt::ClickFocus);
    ui->plainTextEdit->viewport()->installEventFilter(this);
    {
        QSettings settings;
        int pct = settings.value(KEY_LINE_SPACING_PERCENT, 0).toInt();
        pct = qBound(0, (pct / 10) * 10, 100);
        m_lineSpacingPercent = pct;
        QColor hc = settings.value(KEY_HEADING_COLOR, m_headingColor).value<QColor>();
        if (hc.isValid()) m_headingColor = hc;
        QColor ec = settings.value(KEY_EMPHASIS_COLOR, m_emphasisColor).value<QColor>();
        if (ec.isValid()) m_emphasisColor = ec;
        QColor smc = settings.value(KEY_SEARCH_MATCH_COLOR, m_searchMatchColor).value<QColor>();
        if (smc.isValid()) m_searchMatchColor = smc;
        m_editorFontFamily = settings.value(KEY_EDITOR_FONT_FAMILY).toString();
        m_editorFontSize = qBound(3, settings.value(KEY_EDITOR_FONT_SIZE, 10).toInt(), 64);
        m_previewFontFamily = settings.value(KEY_PREVIEW_FONT_FAMILY).toString();
        m_previewFontSize = qBound(3, settings.value(KEY_PREVIEW_FONT_SIZE, 10).toInt(), 64);
        m_outputFontSize = qBound(3, settings.value(KEY_OUTPUT_FONT_SIZE, 10).toInt(), 64);
        QColor thc = settings.value(KEY_TABLE_HEADER_COLOR, m_tableHeaderColor).value<QColor>();
        if (thc.isValid()) m_tableHeaderColor = thc;
        QColor tz1 = settings.value(KEY_TABLE_ZEBRA_COLOR1, m_tableZebraColor1).value<QColor>();
        if (tz1.isValid()) m_tableZebraColor1 = tz1;
        QColor tz2 = settings.value(KEY_TABLE_ZEBRA_COLOR2, m_tableZebraColor2).value<QColor>();
        if (tz2.isValid()) m_tableZebraColor2 = tz2;
        m_viKeybind = settings.value(KEY_VI_KEYBIND, false).toBool();
        m_searchIgnoreCase = settings.value(KEY_SEARCH_IGNORE_CASE, true).toBool();
        m_searchRegExp = settings.value(KEY_SEARCH_REGEXP, true).toBool();
        m_grepSubDir = settings.value(KEY_GREP_SUBDIR, false).toBool();
        m_grepClearOutput = settings.value(KEY_GREP_CLEAR_OUTPUT, true).toBool();
        m_grepFilter = settings.value(KEY_GREP_FILTER, QStringLiteral("*.md")).toString();
        if (m_grepFilter.trimmed().isEmpty())
            m_grepFilter = QStringLiteral("*.md");
        m_toolbarSearchHist = settings.value(KEY_SEARCH_HISTORY).toStringList();
        while (m_toolbarSearchHist.size() > 20)
            m_toolbarSearchHist.pop_back();
        m_replaceHist = settings.value(KEY_REPLACE_HISTORY).toStringList();
        while (m_replaceHist.size() > 20)
            m_replaceHist.pop_back();
        m_grepDirHist = settings.value(KEY_GREP_DIR_HISTORY).toStringList();
        while (m_grepDirHist.size() > 20)
            m_grepDirHist.pop_back();
    }
    ui->action_ViKeybind->setChecked(m_viKeybind);
    applyOutputFontSize(m_outputFontSize);
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
    m_outlineTree->setFocusPolicy(Qt::ClickFocus);
    m_outlineTree->setHeaderHidden(true);
    m_outlineTree->setIndentation(14);
    m_outlineDock->setWidget(m_outlineTree);
    addDockWidget(Qt::LeftDockWidgetArea, m_outlineDock);
    connect(m_outlineTree, &QTreeWidget::itemClicked, this, &MainWindow::onOutlineItemClicked);

    m_dirDock = new QDockWidget(tr("DirectoryBar"), this);
    m_dirDock->setObjectName(QStringLiteral("directoryBar"));
    auto *dirContainer = new QWidget(m_dirDock);
    auto *dirLayout = new QVBoxLayout(dirContainer);
    dirLayout->setContentsMargins(2, 2, 2, 2);
    dirLayout->setSpacing(2);

    auto *dirHeaderRow = new QHBoxLayout();
    dirHeaderRow->setContentsMargins(0, 0, 0, 0);
    dirHeaderRow->setSpacing(2);
    m_dirPathLabel = new QLabel(dirContainer);
    m_dirPathLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    auto *dirUpBtn = new QPushButton(style()->standardIcon(QStyle::SP_FileDialogToParent), QString(), dirContainer);
    dirUpBtn->setFixedWidth(28);
    dirUpBtn->setToolTip(tr("Parent Directory"));
    dirUpBtn->setFocusPolicy(Qt::ClickFocus);
    connect(dirUpBtn, &QPushButton::clicked, this, [this]() {
        if (m_currentDirBarPath.isEmpty()) return;
        QDir d(m_currentDirBarPath);
        if (d.cdUp())
            updateDirBar(d.absolutePath());
    });
    auto *dirBrowseBtn = new QPushButton(QStringLiteral("..."), dirContainer);
    dirBrowseBtn->setFixedWidth(28);
    dirBrowseBtn->setToolTip(tr("Select Folder"));
    dirBrowseBtn->setFocusPolicy(Qt::ClickFocus);
    connect(dirBrowseBtn, &QPushButton::clicked, this, [this]() {
        const QString chosen = QFileDialog::getExistingDirectory(this, tr("Select Folder"), m_currentDirBarPath);
        if (!chosen.isEmpty())
            updateDirBar(chosen);
    });
    dirHeaderRow->addWidget(m_dirPathLabel, 1);
    dirHeaderRow->addWidget(dirUpBtn);
    dirHeaderRow->addWidget(dirBrowseBtn);
    dirLayout->addLayout(dirHeaderRow);

    m_dirFilterCB = new QComboBox(dirContainer);
    m_dirFilterCB->setEditable(true);
    m_dirFilterCB->setFocusPolicy(Qt::ClickFocus);
    m_dirFilterCB->addItems({
        QStringLiteral("*.md"),
        QStringLiteral("*.md; *.txt"),
        QStringLiteral("*.cpp; *.h"),
        QStringLiteral("*.*")
    });
    {
        QSettings settings;
        const QString savedFilter = settings.value(KEY_DIRBAR_FILTER, QStringLiteral("*.md")).toString().trimmed();
        m_dirFilterCB->setCurrentText(savedFilter.isEmpty() ? QStringLiteral("*.md") : savedFilter);
    }
    connect(m_dirFilterCB, &QComboBox::currentTextChanged, this, [this](const QString &f) {
        QSettings settings;
        settings.setValue(KEY_DIRBAR_FILTER, f.trimmed());
        updateDirBar();
    });
    dirLayout->addWidget(m_dirFilterCB);

    m_dirTree = new QTreeWidget(dirContainer);
    m_dirTree->setFocusPolicy(Qt::ClickFocus);
    m_dirTree->setHeaderHidden(true);
    m_dirTree->setRootIsDecorated(false);
    connect(m_dirTree, &QTreeWidget::itemDoubleClicked, this, &MainWindow::onDirItemDoubleClicked);
    dirLayout->addWidget(m_dirTree, 1);

    m_dirDock->setWidget(dirContainer);
    addDockWidget(Qt::LeftDockWidgetArea, m_dirDock);
    splitDockWidget(m_outlineDock, m_dirDock, Qt::Vertical);

    m_calendarDock = new QDockWidget(tr("CalendarBar"), this);
    m_calendarDock->setObjectName(QStringLiteral("calendarBar"));
    auto *cal = new DiaryCalendarWidget(m_calendarDock);
    cal->setFocusPolicy(Qt::ClickFocus);
    cal->setFirstDayOfWeek(Qt::Monday);
    cal->setStyleSheet(QStringLiteral(
        "QCalendarWidget QWidget#qt_calendar_navigationbar { background-color: #808080; }"
    ));
    m_calendarWidget = cal;
    m_calendarDock->setWidget(m_calendarWidget);
    addDockWidget(Qt::BottomDockWidgetArea, m_calendarDock);
    splitDockWidget(ui->outputBar, m_calendarDock, Qt::Horizontal);
    connect(m_calendarWidget, &QCalendarWidget::currentPageChanged, this, &MainWindow::onCalendarPageChanged);
    connect(m_calendarWidget, &QCalendarWidget::clicked, this, &MainWindow::openDiaryDate);

    QMenu *viewMenu = new QMenu(tr("&View"), this);
    QAction *outlineAct = m_outlineDock->toggleViewAction();
    outlineAct->setText(tr("&OutlineBar"));
    QAction *dirAct = m_dirDock->toggleViewAction();
    dirAct->setText(tr("&DirectoryBar"));
    QAction *outputAct = ui->outputBar->toggleViewAction();
    outputAct->setText(tr("Outpu&tBar"));
    QAction *calAct = m_calendarDock->toggleViewAction();
    calAct->setText(tr("&CalendarBar"));
    viewMenu->addAction(outlineAct);
    viewMenu->addAction(dirAct);
    viewMenu->addAction(outputAct);
    viewMenu->addAction(calAct);
    menuBar()->insertMenu(ui->menuTest->menuAction(), viewMenu);
#if defined(Q_OS_WIN) || defined(Q_OS_LINUX)
    //	メニューバーをメインツールバーに統合（viMarkdown と同じ方式）
    ui->mainToolBar->insertWidget(ui->action_Undo, ui->menuBar);
#endif
    auto makeIgnoreCaseIcon = []() -> QIcon {
        QPixmap pm(48, 48);
        pm.fill(Qt::transparent);
        QPainter p(&pm);
        p.setRenderHint(QPainter::Antialiasing, true);
        p.setRenderHint(QPainter::TextAntialiasing, true);
        QFont f = QApplication::font();
        f.setPixelSize(24);
        p.setFont(f);
        p.setPen(QColor(0x20, 0x20, 0x20));
        p.drawText(pm.rect(), Qt::AlignCenter, QStringLiteral("Aa"));
        p.setPen(QPen(QColor(0x20, 0x20, 0x20), 2.5));
        p.drawLine(8, 8, 40, 40);
        return QIcon(pm);
    };
    auto makeRegExpIcon = []() -> QIcon {
        QPixmap pm(48, 48);
        pm.fill(Qt::transparent);
        QPainter p(&pm);
        p.setRenderHint(QPainter::TextAntialiasing, true);
        QFont f = QApplication::font();
        f.setPixelSize(18);
        p.setFont(f);
        p.setPen(QColor(0x20, 0x20, 0x20));
        p.drawText(QRect(0, 3, 48, 22), Qt::AlignCenter, QStringLiteral("reg"));
        p.drawText(QRect(0, 22, 48, 22), Qt::AlignCenter, QStringLiteral("exp"));
        return QIcon(pm);
    };
    m_searchCB = new QComboBox(this);
    m_searchCB->setEditable(true);
    m_searchCB->setMaxCount(20);
    m_searchCB->setMinimumWidth(160);
    m_searchCB->setMaximumWidth(220);
    m_searchCB->setFocusPolicy(Qt::ClickFocus);
    m_searchCB->setInsertPolicy(QComboBox::NoInsert);
    if (QLineEdit *le = m_searchCB->lineEdit()) {
        le->setPlaceholderText(tr("search text"));
        le->setClearButtonEnabled(true);
        le->installEventFilter(this);
    }
    {
        QSignalBlocker sb(m_searchCB);
        m_searchCB->addItems(m_toolbarSearchHist);
        m_searchCB->setCurrentIndex(-1);
        m_searchCB->clearEditText();
    }
    m_actIgnoreCase = new QAction(makeIgnoreCaseIcon(), tr("Ignore Case"), this);
    m_actIgnoreCase->setCheckable(true);
    m_actIgnoreCase->setChecked(m_searchIgnoreCase);
    m_actIgnoreCase->setToolTip(tr("Ignore Case (Aa)"));
    m_actRegExp = new QAction(makeRegExpIcon(), tr("RegExp"), this);
    m_actRegExp->setCheckable(true);
    m_actRegExp->setChecked(m_searchRegExp);
    m_actRegExp->setToolTip(tr("Regular Expression"));

    //	Redo ボタンのすぐ右（AlignLeft の手前）に検索コンボ＋Aa＋regexp を配置
    ui->mainToolBar->setStyleSheet(
        "QToolButton:checked {"
        "   background-color: rgba(0, 120, 215, 60);"
        "   border: 1px solid #0078d7;"
        "   border-radius: 2px;"
        "   color: black;"
        "}"
        "QToolButton:hover {"
        "   background-color: rgba(0, 120, 215, 30);"
        "}");
    ui->mainToolBar->insertWidget(ui->action_AlignLeft, m_searchCB);
    ui->mainToolBar->insertAction(ui->action_AlignLeft, m_actIgnoreCase);
    ui->mainToolBar->insertAction(ui->action_AlignLeft, m_actRegExp);
    ui->mainToolBar->insertSeparator(ui->action_AlignLeft);
    auto *searchEscShortcut = new QShortcut(QKeySequence(Qt::Key_Escape), m_searchCB);
    searchEscShortcut->setContext(Qt::WidgetWithChildrenShortcut);
    connect(searchEscShortcut, &QShortcut::activated, this, [this]() {
        if (TextEdit *edit = currentTextEdit())
            edit->setFocus();
        else
            setFocus();
    });

    auto applySearchOptsToAllTabs = [this]() {
        if (!m_tabWidget) return;
        for (int i = 0; i < m_tabWidget->count(); ++i) {
            if (TextEdit *edit = textEditAt(i)) {
                edit->viEngine()->setSearchOptions(m_searchIgnoreCase, m_searchRegExp);
                const QString hl = edit->searchHighlightPattern();
                if (!hl.isEmpty())
                    edit->setSearchHighlight(hl, m_searchIgnoreCase, m_searchRegExp);
            }
        }
    };
    connect(m_actIgnoreCase, &QAction::toggled, this, [this, applySearchOptsToAllTabs](bool checked) {
        m_searchIgnoreCase = checked;
        QSettings settings;
        settings.setValue(KEY_SEARCH_IGNORE_CASE, m_searchIgnoreCase);
        applySearchOptsToAllTabs();
    });
    connect(m_actRegExp, &QAction::toggled, this, [this, applySearchOptsToAllTabs](bool checked) {
        m_searchRegExp = checked;
        QSettings settings;
        settings.setValue(KEY_SEARCH_REGEXP, m_searchRegExp);
        applySearchOptsToAllTabs();
    });
    auto runToolbarSearch = [this](const QString &pat, bool forward) {
        TextEdit *edit = currentTextEdit();
        if (!edit || pat.isEmpty()) return;
        m_toolbarSearchHist.removeAll(pat);
        m_toolbarSearchHist.push_front(pat);
        while (m_toolbarSearchHist.size() > 20)
            m_toolbarSearchHist.pop_back();
        {
            QSignalBlocker sb(m_searchCB);
            m_searchCB->clear();
            m_searchCB->addItems(m_toolbarSearchHist);
            m_searchCB->setCurrentText(pat);
        }
        QSettings settings;
        settings.setValue(KEY_SEARCH_HISTORY, m_toolbarSearchHist);
        edit->viEngine()->setSearchOptions(m_searchIgnoreCase, m_searchRegExp);
        edit->viEngine()->search(edit, pat, forward);
        edit->setFocus();
    };
    if (QLineEdit *le = m_searchCB->lineEdit()) {
        connect(le, &QLineEdit::returnPressed, this, [this, runToolbarSearch]() {
            const QString pat = m_searchCB->currentText();
            if (pat.isEmpty()) {
                if (TextEdit *edit = currentTextEdit())
                    edit->setFocus();
                return;
            }
            const bool forward = (QApplication::keyboardModifiers() & Qt::ShiftModifier) == 0;
            runToolbarSearch(pat, forward);
        });
    }
    connect(m_searchCB, &QComboBox::textActivated, this, [runToolbarSearch](const QString &text) {
        runToolbarSearch(text, true);
    });
    connect(m_searchCB, &QComboBox::editTextChanged, this, [this](const QString &text) {
        if (text.isEmpty()) {
            if (TextEdit *edit = currentTextEdit())
                edit->clearSearchHighlight();
        }
    });
    QMenu *searchMenu = new QMenu(tr("&Search"), this);
    QAction *actFind = searchMenu->addAction(tr("&Find"));
    actFind->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_F));
    connect(actFind, &QAction::triggered, this, [this]() {
        if (!m_searchCB) return;
        if (TextEdit *edit = currentTextEdit()) {
            const QString sel = edit->textCursor().selectedText();
            if (!sel.isEmpty() && !sel.contains(QChar('\n')))
                m_searchCB->setCurrentText(sel);
        }
        m_searchCB->setFocus();
        if (QLineEdit *le = m_searchCB->lineEdit())
            le->selectAll();
    });
    QAction *actFindNext = searchMenu->addAction(tr("Find&Next"));
    actFindNext->setShortcut(QKeySequence(Qt::Key_F3));
    connect(actFindNext, &QAction::triggered, this, [this, runToolbarSearch]() {
        TextEdit *edit = currentTextEdit();
        if (!edit) return;
        const QString pat = m_searchCB ? m_searchCB->currentText() : QString();
        if (!pat.isEmpty())
            runToolbarSearch(pat, true);
        else
            edit->viEngine()->searchNext(edit, true);
    });
    QAction *actFindPrev = searchMenu->addAction(tr("Find&Prev"));
    actFindPrev->setShortcut(QKeySequence(Qt::SHIFT | Qt::Key_F3));
    connect(actFindPrev, &QAction::triggered, this, [this, runToolbarSearch]() {
        TextEdit *edit = currentTextEdit();
        if (!edit) return;
        const QString pat = m_searchCB ? m_searchCB->currentText() : QString();
        if (!pat.isEmpty())
            runToolbarSearch(pat, false);
        else
            edit->viEngine()->searchNext(edit, false);
    });
    QAction *actFindWord = searchMenu->addAction(tr("Find&Word"));
    actFindWord->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_F3));
    connect(actFindWord, &QAction::triggered, this, [this, runToolbarSearch]() {
        TextEdit *edit = currentTextEdit();
        if (!edit) return;
        QString word = edit->textCursor().hasSelection()
            ? edit->textCursor().selectedText()
            : edit->wordAtCursor();
        if (word.isEmpty() || word.contains(QChar('\n'))) return;
        const QString pat = m_searchRegExp ? QRegularExpression::escape(word) : word;
        runToolbarSearch(pat, true);
    });
    QAction *actClearHL = searchMenu->addAction(tr("&Clear Search Highlight"));
    actClearHL->setShortcut(QKeySequence(Qt::ALT | Qt::Key_F3));
    connect(actClearHL, &QAction::triggered, this, [this]() {
        if (TextEdit *edit = currentTextEdit())
            edit->clearSearchHighlight();
        if (m_searchCB) {
            QSignalBlocker sb(m_searchCB);
            m_searchCB->clearEditText();
        }
    });
    searchMenu->addSeparator();
    QAction *actReplace = searchMenu->addAction(tr("&Replace..."));
    actReplace->setShortcut(QKeySequence(Qt::Key_F4));
    connect(actReplace, &QAction::triggered, this, &MainWindow::onReplace);
    QAction *actGrep = searchMenu->addAction(tr("&Grep..."));
    actGrep->setShortcut(QKeySequence(Qt::SHIFT | Qt::Key_F4));
    connect(actGrep, &QAction::triggered, this, &MainWindow::onGrep);
    ui->menuBar->insertMenu(viewMenu->menuAction(), searchMenu);

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
    connect(ui->action_ExportPDF, &QAction::triggered, this, &MainWindow::onExportPDF);
    connect(ui->action_Close, &QAction::triggered, this, &MainWindow::onClose);
    connect(ui->action_Exit, &QAction::triggered, this, &QWidget::close);
    connect(ui->action_Undo, &QAction::triggered, this, &MainWindow::onUndo);
    connect(ui->action_Redo, &QAction::triggered, this, &MainWindow::onRedo);
    connect(ui->action_Cut, &QAction::triggered, this, [this]() {
        if (QWidget *fw = QApplication::focusWidget()) {
            if (auto *le = qobject_cast<QLineEdit *>(fw)) { le->cut(); return; }
            if (auto *prev = dynamic_cast<MarkdownPreview *>(fw)) { prev->cutToSource(); return; }
            if (auto *te = qobject_cast<QTextEdit *>(fw)) { te->cut(); return; }
            if (qobject_cast<QPlainTextEdit *>(fw)) return;
        }
        if (TextEdit *edit = currentTextEdit())
            edit->cut();
    });
    connect(ui->action_Copy, &QAction::triggered, this, [this]() {
        if (QWidget *fw = QApplication::focusWidget()) {
            if (auto *le = qobject_cast<QLineEdit *>(fw)) { le->copy(); return; }
            if (auto *te = qobject_cast<QTextEdit *>(fw)) { te->copy(); return; }
            if (auto *pte = qobject_cast<QPlainTextEdit *>(fw)) { pte->copy(); return; }
        }
        if (TextEdit *edit = currentTextEdit())
            edit->copy();
    });
    connect(ui->action_Paste, &QAction::triggered, this, [this]() {
        if (QWidget *fw = QApplication::focusWidget()) {
            if (auto *le = qobject_cast<QLineEdit *>(fw)) { le->paste(); return; }
            if (auto *prev = dynamic_cast<MarkdownPreview *>(fw)) { prev->pasteToSource(); return; }
            if (auto *te = qobject_cast<QTextEdit *>(fw)) { te->paste(); return; }
            if (qobject_cast<QPlainTextEdit *>(fw)) return;
        }
        if (TextEdit *edit = currentTextEdit())
            edit->paste();
    });
    connect(ui->action_SelectAll, &QAction::triggered, this, [this]() {
        if (QWidget *fw = QApplication::focusWidget()) {
            if (auto *le = qobject_cast<QLineEdit *>(fw)) { le->selectAll(); return; }
            if (auto *te = qobject_cast<QTextEdit *>(fw)) { te->selectAll(); return; }
            if (auto *pte = qobject_cast<QPlainTextEdit *>(fw)) { pte->selectAll(); return; }
        }
        if (TextEdit *edit = currentTextEdit())
            edit->selectAll();
    });
    connect(ui->action_CsvToGfmTable, &QAction::triggered, this, [this]() {
        if (TextEdit *edit = currentTextEdit()) {
            if (edit->convertCsvToGfmTable())
                statusBar()->showMessage(tr("Converted CSV to GFM Table"), 3000);
            else
                statusBar()->showMessage(tr("No CSV block or selection to convert"), 3000);
        }
    });
    connect(ui->action_GfmToCsvTable, &QAction::triggered, this, [this]() {
        if (TextEdit *edit = currentTextEdit()) {
            if (edit->convertGfmTableToCsv())
                statusBar()->showMessage(tr("Converted GFM Table to CSV"), 3000);
            else
                statusBar()->showMessage(tr("No GFM Table at cursor or selection"), 3000);
        }
    });
    connect(ui->action_AlignLeft, &QAction::triggered, this, [this]() {
        if (TextEdit *edit = currentTextEdit())
            edit->setLineAlignment(Qt::AlignLeft);
    });
    connect(ui->action_AlignCenter, &QAction::triggered, this, [this]() {
        if (TextEdit *edit = currentTextEdit())
            edit->setLineAlignment(Qt::AlignCenter);
    });
    connect(ui->action_AlignRight, &QAction::triggered, this, [this]() {
        if (TextEdit *edit = currentTextEdit())
            edit->setLineAlignment(Qt::AlignRight);
    });
    connect(ui->action_Indent, &QAction::triggered, this, [this]() {
        if (TextEdit *edit = currentTextEdit())
            edit->indent();
    });
    connect(ui->action_Unindent, &QAction::triggered, this, [this]() {
        if (TextEdit *edit = currentTextEdit())
            edit->unindent();
    });
    updateUndoRedoEnabled();
    ui->action_Settings->setShortcut(QKeySequence(Qt::Key_F8));
    connect(ui->action_Settings, &QAction::triggered, this, [this]() { onSettings(0); });
    auto *fontColorSettingsSc = new QShortcut(QKeySequence(Qt::SHIFT | Qt::Key_F8), this);
    connect(fontColorSettingsSc, &QShortcut::activated, this, [this]() { onSettings(1); });
    m_cmdLine = new QLineEdit(this);
    m_cmdLine->setFrame(false);
    m_cmdLine->hide();
    m_cmdLine->installEventFilter(this);
    statusBar()->addPermanentWidget(m_cmdLine, 1);
    connect(m_cmdLine, &QLineEdit::textChanged, this, &MainWindow::onViCmdLineTextChanged);
    connect(m_cmdLine, &QLineEdit::returnPressed, this, &MainWindow::onViCmdLineReturn);

    connect(ui->action_ViKeybind, &QAction::toggled, this, [this](bool checked) {
        m_viKeybind = checked;
        QSettings settings;
        settings.setValue(KEY_VI_KEYBIND, m_viKeybind);
        if (m_tabWidget) {
            for (int i = 0; i < m_tabWidget->count(); ++i) {
                if (TextEdit *edit = textEditAt(i))
                    edit->setViEnabled(m_viKeybind);
            }
        }
        statusBar()->showMessage(m_viKeybind ? tr("vi keybind: ON (-- NORMAL --)") : tr("vi keybind: OFF"), 3000);
    });
    connect(ui->menu_RecentFiles, &QMenu::aboutToShow, this, &MainWindow::onAboutToShow_RecentFiles);
    connect(ui->menu_FavoriteFiles, &QMenu::aboutToShow, this, &MainWindow::onAboutToShow_FavoriteFiles);
    connect(ui->action_AddThisFavorite, &QAction::triggered, this, &MainWindow::onAction_AddThisFavorite);
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
    connect(ui->action_TestVi, &QAction::triggered, this, [this]() {
        do_test_vi(this);
    });
    connect(ui->action_BenchmarkPushBack, &QAction::triggered, this, &MainWindow::onBenchmarkPushBack);
    connect(ui->action_BenchmarkPushFront, &QAction::triggered, this, &MainWindow::onBenchmarkPushFront);
	connect(ui->action_AboutViMarkdown, &QAction::triggered, this, &MainWindow::onAction_About);
    {
        const QDate today = QDate::currentDate();
        onCalendarPageChanged(today.year(), today.month());
    }
    {
        const QString initDir = defaultDiaryBaseDir();
        if (QDir(initDir).exists()) {
            QDir::setCurrent(initDir);
            m_lastDir = initDir;
        }
    }
    updateDirBar(QDir::currentPath());
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
    edit->setSearchMatchColor(m_searchMatchColor);
    edit->setViEnabled(m_viKeybind);
    edit->viEngine()->setSearchOptions(m_searchIgnoreCase, m_searchRegExp);
    edit->viEngine()->setCallbacks({
        [this](const QString &msg) { statusBar()->showMessage(msg, 5000); },
        [this](QChar prefix) { openViCmdLine(prefix); },
        [this](bool force) {
            if (!m_tabWidget) return;
            const int ix = m_tabWidget->currentIndex();
            if (force) closeTabForce(ix);
            else onTabCloseRequested(ix);
        },
        [this]() { return m_tabWidget ? saveTab(m_tabWidget->currentIndex()) : false; }
    });
    auto *preview = new MarkdownPreview(splitter);
    preview->setPlaceholderText(tr("You can also make simple edits (insert/delete text) here."));
    {
        QFont pf = preview->font();
        if (!m_previewFontFamily.isEmpty())
            pf.setFamily(m_previewFontFamily);
        else
            m_previewFontFamily = pf.family();
        pf.setPointSize(m_previewFontSize);
        preview->setFont(pf);
        preview->document()->setDefaultFont(pf);
        preview->setTableColors(m_tableHeaderColor, m_tableZebraColor1, m_tableZebraColor2);
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
    preview->setCheckboxToggledCallback([edit](int line, bool checked) {
        edit->toggleCheckboxAtLine(line, checked);
    });
    preview->setEditCallback([edit](int srcStart, int srcEnd, const QString &text) {
        edit->applyPreviewEdit(srcStart, srcEnd, text);
    });
    preview->setUndoRedoCallback([edit](bool isRedo) {
        if (isRedo)
            edit->redo();
        else
            edit->undo();
    });
    auto syncingCursor = std::make_shared<bool>(false);
    edit->setTextChangedCallback([this, splitter, edit, preview, syncingCursor]() {
        bool headingsChanged = false;
        if (edit->markdownHighlight()) {
            const TextCursor tc = edit->textCursor();
            const int curLine = edit->document()->lineMgr().lineAtPos(tc.position());
            headingsChanged = true;
            preview->updateMarkdown(edit->document(), curLine, &headingsChanged);
            if (!*syncingCursor) {
                *syncingCursor = true;
                preview->setCursorFromSrcPos(
                    static_cast<int>(tc.position()),
                    tc.hasSelection() ? static_cast<int>(tc.anchor()) : -1);
                *syncingCursor = false;
            }
        }
        if (headingsChanged)
            updateOutlineForTab(splitter);
        updateUndoRedoEnabled();
    });
    edit->setCursorChangedCallback([edit, preview, syncingCursor]() {
        if (*syncingCursor || !edit->markdownHighlight())
            return;
        *syncingCursor = true;
        const TextCursor tc = edit->textCursor();
        preview->setCursorFromSrcPos(
            static_cast<int>(tc.position()),
            tc.hasSelection() ? static_cast<int>(tc.anchor()) : -1);
        *syncingCursor = false;
    });
    connect(preview, &QTextEdit::cursorPositionChanged, edit, [edit, preview, syncingCursor]() {
        if (*syncingCursor || !edit->markdownHighlight())
            return;
        *syncingCursor = true;
        const QTextCursor pc = preview->textCursor();
        const int srcPos = preview->prvToSrcPos(pc.position());
        const int srcAnchor = pc.hasSelection() ? preview->prvToSrcPos(pc.anchor()) : -1;
        edit->setCursorPosition(srcPos, srcAnchor);
        *syncingCursor = false;
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
    auto syncingScroll = std::make_shared<bool>(false);
    connect(edit->verticalScrollBar(), &QScrollBar::valueChanged, preview, [edit, preview, syncingScroll](int val) {
        if (*syncingScroll || !edit->markdownHighlight())
            return;
        QScrollBar *editSb = edit->verticalScrollBar();
        QScrollBar *prevSb = preview->verticalScrollBar();
        if (!editSb || !prevSb)
            return;
        *syncingScroll = true;
        if (editSb->maximum() <= 0 || val <= editSb->minimum()) {
            prevSb->setValue(prevSb->minimum());
        } else if (val >= editSb->maximum()) {
            prevSb->setValue(prevSb->maximum());
        } else {
            const ViewLineMgr &mgr = edit->document()->lineMgr();
            const int ln = mgr.viewLineToLine(val);
            const int firstVl = mgr.lineToViewLine(ln);
            const int nVl = mgr.viewLineCountIn(ln);
            const qreal frac = (nVl > 1) ? qreal(val - firstVl) / qreal(nVl) : 0.0;
            preview->scrollToSourceLine(ln, frac);
        }
        *syncingScroll = false;
    });
    connect(preview->verticalScrollBar(), &QScrollBar::valueChanged, edit, [edit, preview, syncingScroll](int val) {
        if (*syncingScroll || !edit->markdownHighlight())
            return;
        QScrollBar *editSb = edit->verticalScrollBar();
        QScrollBar *prevSb = preview->verticalScrollBar();
        if (!editSb || !prevSb)
            return;
        *syncingScroll = true;
        if (prevSb->maximum() <= 0 || val <= prevSb->minimum()) {
            editSb->setValue(editSb->minimum());
        } else if (val >= prevSb->maximum()) {
            editSb->setValue(editSb->maximum());
        } else {
            int srcLine = 0;
            qreal frac = 0.0;
            if (preview->topSourceLine(srcLine, frac)) {
                const ViewLineMgr &mgr = edit->document()->lineMgr();
                const int clampedLine = qBound(0, srcLine, qMax(0, mgr.lineCount() - 1));
                const int firstVl = mgr.lineToViewLine(clampedLine);
                const int nVl = mgr.viewLineCountIn(clampedLine);
                const int targetVl = firstVl + qBound(0, static_cast<int>(frac * nVl), qMax(0, nVl - 1));
                editSb->setValue(qBound(editSb->minimum(), targetVl, editSb->maximum()));
            }
        }
        *syncingScroll = false;
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

TextEdit *MainWindow::textEditAt(int index) const
{
    if (!m_tabWidget || index < 0 || index >= m_tabWidget->count())
        return nullptr;
    auto *splitter = qobject_cast<QSplitter *>(m_tabWidget->widget(index));
    return splitter ? dynamic_cast<TextEdit *>(splitter->widget(0)) : nullptr;
}

TextEdit *MainWindow::currentTextEdit() const
{
    return m_tabWidget ? textEditAt(m_tabWidget->currentIndex()) : nullptr;
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
#ifdef Q_OS_WIN
    constexpr Qt::CaseSensitivity cs = Qt::CaseInsensitive;
#else
    constexpr Qt::CaseSensitivity cs = Qt::CaseSensitive;
#endif
    for (int i = 0; i < m_tabWidget->count(); ++i) {
        QWidget *w = m_tabWidget->widget(i);
        if (w && w->property(PROP_FILE_PATH).toString().compare(cleanPath, cs) == 0)
            return i;
    }
    return -1;
}

void MainWindow::onCurrentTabChanged(int index)
{
    if (m_cmdLine && m_cmdLine->isVisible()) {
        m_cmdLine->hide();
        cancelViIncSearch();
    }
    if (index < 0) {
        setWindowTitle(QStringLiteral("TestBuffer"));
        updateUndoRedoEnabled();
        setFocus();
        return;
    }
    setWindowTitle(m_tabWidget->tabText(index) + QStringLiteral(" - TestBuffer"));
    updateUndoRedoEnabled();
    if (QWidget *w = m_tabWidget->widget(index)) {
        const QString fp = w->property(PROP_FILE_PATH).toString();
        if (!fp.isEmpty())
            updateDirBar(QFileInfo(fp).absolutePath());
    }
    if (TextEdit *edit = currentTextEdit())
        edit->setFocus();
}

bool MainWindow::maybeSaveTab(int index)
{
    TextEdit *edit = textEditAt(index);
    if (!edit || !edit->document()->isModified())
        return true;

    m_tabWidget->setCurrentIndex(index);
    const QString title = m_tabWidget->tabText(index);
    const QMessageBox::StandardButton ret = QMessageBox::warning(
        this,
        tr("Unsaved Changes"),
        tr("'%1' has been modified.\nDo you want to save your changes?").arg(title),
        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel,
        QMessageBox::Save);
    if (ret == QMessageBox::Save)
        return saveTab(index);
    if (ret == QMessageBox::Discard)
        return true;
    return false;
}

void MainWindow::closeTabForce(int index)
{
    if (!m_tabWidget || index < 0 || index >= m_tabWidget->count())
        return;
    QWidget *w = m_tabWidget->widget(index);
    removeOutlineForTab(w);
    m_tabWidget->removeTab(index);
    if (w)
        w->deleteLater();
}

void MainWindow::onTabCloseRequested(int index)
{
    if (!m_tabWidget || index < 0 || index >= m_tabWidget->count())
        return;
    if (!maybeSaveTab(index))
        return;
    closeTabForce(index);
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

void MainWindow::applyColors(const QColor &heading, const QColor &emphasis, const QColor &searchMatch)
{
    if (heading.isValid()) m_headingColor = heading;
    if (emphasis.isValid()) m_emphasisColor = emphasis;
    if (searchMatch.isValid()) m_searchMatchColor = searchMatch;
    if (!m_tabWidget)
        return;
    for (int i = 0; i < m_tabWidget->count(); ++i) {
        auto *splitter = qobject_cast<QSplitter *>(m_tabWidget->widget(i));
        auto *edit = splitter ? dynamic_cast<TextEdit *>(splitter->widget(0)) : nullptr;
        if (edit) {
            edit->setHeadingColor(m_headingColor);
            edit->setEmphasisColor(m_emphasisColor);
            edit->setSearchMatchColor(m_searchMatchColor);
        }
    }
}

void MainWindow::applyTableColors(const QColor &header, const QColor &zebra1, const QColor &zebra2)
{
    if (header.isValid()) m_tableHeaderColor = header;
    if (zebra1.isValid()) m_tableZebraColor1 = zebra1;
    if (zebra2.isValid()) m_tableZebraColor2 = zebra2;
    if (!m_tabWidget)
        return;
    for (int i = 0; i < m_tabWidget->count(); ++i) {
        auto *splitter = qobject_cast<QSplitter *>(m_tabWidget->widget(i));
        auto *edit = splitter ? dynamic_cast<TextEdit *>(splitter->widget(0)) : nullptr;
        auto *preview = splitter ? dynamic_cast<MarkdownPreview *>(splitter->widget(1)) : nullptr;
        if (preview) {
            preview->setTableColors(m_tableHeaderColor, m_tableZebraColor1, m_tableZebraColor2);
            if (edit && edit->markdownHighlight())
                preview->setMarkdown(edit->document());
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

void MainWindow::applyOutputFontSize(int pointSize)
{
    m_outputFontSize = qBound(3, pointSize, 64);
    if (ui && ui->plainTextEdit) {
        QFont f = ui->plainTextEdit->font();
        f.setPointSize(m_outputFontSize);
        ui->plainTextEdit->setFont(f);
    }
}

void MainWindow::onSettings(int initialTab)
{
    const int origPercent = m_lineSpacingPercent;
    const QColor origHeading = m_headingColor;
    const QColor origEmphasis = m_emphasisColor;
    const QColor origSearchMatch = m_searchMatchColor;
    const QString origFontFamily = m_editorFontFamily;
    const int origFontSize = m_editorFontSize;
    const QString origPreviewFontFamily = m_previewFontFamily;
    const int origPreviewFontSize = m_previewFontSize;
    const int origOutputFontSize = m_outputFontSize;
    const QColor origTableHeader = m_tableHeaderColor;
    const QColor origTableZebra1 = m_tableZebraColor1;
    const QColor origTableZebra2 = m_tableZebraColor2;
    const QString origDefaultDir = defaultDiaryBaseDir();
    const bool origViKeybind = m_viKeybind;

    QDialog dlg(this);
    dlg.setWindowTitle(tr("Settings"));
    dlg.resize(640, 340);
    auto *layout = new QVBoxLayout(&dlg);
    auto *tabs = new QTabWidget(&dlg);

    //	Page 0: General
    auto *generalPage = new QWidget(tabs);
    auto *generalLayout = new QVBoxLayout(generalPage);
    auto *generalForm = new QFormLayout();
    auto *defaultDirRow = new QHBoxLayout();
    auto *defaultDirEdit = new QLineEdit(origDefaultDir, generalPage);
    auto *defaultDirBrowse = new QPushButton(tr("Browse..."), generalPage);
    connect(defaultDirBrowse, &QPushButton::clicked, &dlg, [&dlg, defaultDirEdit]() {
        const QString chosen = QFileDialog::getExistingDirectory(&dlg, tr("Select Default Directory"), defaultDirEdit->text());
        if (!chosen.isEmpty())
            defaultDirEdit->setText(QDir::cleanPath(chosen));
    });
    defaultDirRow->addWidget(defaultDirEdit, 1);
    defaultDirRow->addWidget(defaultDirBrowse);
    generalForm->addRow(tr("Default Directory:"), defaultDirRow);

    auto *viKeybindChk = new QCheckBox(tr("Enable vi keybindings"), generalPage);
    viKeybindChk->setChecked(m_viKeybind);
    generalForm->addRow(QString(), viKeybindChk);
    generalLayout->addLayout(generalForm);
    generalLayout->addStretch();
    tabs->addTab(generalPage, tr("General"));

    //	Page 1: Font / Color
    auto *fontColorPage = new QWidget(tabs);
    auto *fontColorLayout = new QVBoxLayout(fontColorPage);
    auto *groupsRow = new QHBoxLayout();

    auto *editorGroup = new QGroupBox(tr("Editor"), fontColorPage);
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
            applyColors(c, m_emphasisColor, m_searchMatchColor);
        }
    });
    editorForm->addRow(tr("Heading Color:"), headingColorBtn);

    auto *emphasisColorBtn = new QPushButton(editorGroup);
    setColorButtonStyle(emphasisColorBtn, m_emphasisColor);
    connect(emphasisColorBtn, &QPushButton::clicked, &dlg, [this, &dlg, emphasisColorBtn]() {
        QColor c = QColorDialog::getColor(m_emphasisColor, &dlg, tr("Select Emphasis Color"));
        if (c.isValid()) {
            setColorButtonStyle(emphasisColorBtn, c);
            applyColors(m_headingColor, c, m_searchMatchColor);
        }
    });
    editorForm->addRow(tr("Emphasis Color:"), emphasisColorBtn);

    auto *searchMatchColorBtn = new QPushButton(editorGroup);
    setColorButtonStyle(searchMatchColorBtn, m_searchMatchColor);
    connect(searchMatchColorBtn, &QPushButton::clicked, &dlg, [this, &dlg, searchMatchColorBtn]() {
        QColor c = QColorDialog::getColor(m_searchMatchColor, &dlg, tr("Select Search Match Color"));
        if (c.isValid()) {
            setColorButtonStyle(searchMatchColorBtn, c);
            applyColors(m_headingColor, m_emphasisColor, c);
        }
    });
    editorForm->addRow(tr("Search Match Color:"), searchMatchColorBtn);
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

    auto *tableHeaderColorBtn = new QPushButton(previewGroup);
    setColorButtonStyle(tableHeaderColorBtn, m_tableHeaderColor);
    connect(tableHeaderColorBtn, &QPushButton::clicked, &dlg, [this, &dlg, tableHeaderColorBtn]() {
        QColor c = QColorDialog::getColor(m_tableHeaderColor, &dlg, tr("Select Table Header Color"));
        if (c.isValid()) {
            setColorButtonStyle(tableHeaderColorBtn, c);
            applyTableColors(c, m_tableZebraColor1, m_tableZebraColor2);
        }
    });
    previewForm->addRow(tr("Table Header Color:"), tableHeaderColorBtn);

    auto *tableZebra1ColorBtn = new QPushButton(previewGroup);
    setColorButtonStyle(tableZebra1ColorBtn, m_tableZebraColor1);
    connect(tableZebra1ColorBtn, &QPushButton::clicked, &dlg, [this, &dlg, tableZebra1ColorBtn]() {
        QColor c = QColorDialog::getColor(m_tableZebraColor1, &dlg, tr("Select Table Zebra Color 1"));
        if (c.isValid()) {
            setColorButtonStyle(tableZebra1ColorBtn, c);
            applyTableColors(m_tableHeaderColor, c, m_tableZebraColor2);
        }
    });
    previewForm->addRow(tr("Table Zebra Color 1:"), tableZebra1ColorBtn);

    auto *tableZebra2ColorBtn = new QPushButton(previewGroup);
    setColorButtonStyle(tableZebra2ColorBtn, m_tableZebraColor2);
    connect(tableZebra2ColorBtn, &QPushButton::clicked, &dlg, [this, &dlg, tableZebra2ColorBtn]() {
        QColor c = QColorDialog::getColor(m_tableZebraColor2, &dlg, tr("Select Table Zebra Color 2"));
        if (c.isValid()) {
            setColorButtonStyle(tableZebra2ColorBtn, c);
            applyTableColors(m_tableHeaderColor, m_tableZebraColor1, c);
        }
    });
    previewForm->addRow(tr("Table Zebra Color 2:"), tableZebra2ColorBtn);
    groupsRow->addWidget(previewGroup, 1);

    fontColorLayout->addLayout(groupsRow);

    auto *outputGroup = new QGroupBox(tr("Output"), fontColorPage);
    auto *outputForm = new QFormLayout(outputGroup);
    auto *outputFontSizeSpin = new QSpinBox(outputGroup);
    outputFontSizeSpin->setRange(3, 64);
    outputFontSizeSpin->setSingleStep(1);
    outputFontSizeSpin->setSuffix(QStringLiteral(" pt"));
    outputFontSizeSpin->setValue(m_outputFontSize);
    connect(outputFontSizeSpin, &QSpinBox::valueChanged, this, [this](int val) {
        applyOutputFontSize(val);
    });
    outputForm->addRow(tr("Font Size:"), outputFontSizeSpin);
    fontColorLayout->addWidget(outputGroup);
    fontColorLayout->addStretch();
    tabs->addTab(fontColorPage, tr("Font / Color"));

    tabs->setCurrentIndex(qBound(0, initialTab, tabs->count() - 1));
    layout->addWidget(tabs, 1);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    layout->addWidget(buttons);

    if (dlg.exec() == QDialog::Accepted) {
        const QString newDefaultDir = QDir::cleanPath(defaultDirEdit->text().trimmed());
        if (!newDefaultDir.isEmpty()) {
            QSettings settings;
            settings.setValue(QStringLiteral("defaultDir"), newDefaultDir);
            if (newDefaultDir != origDefaultDir && m_calendarWidget) {
                if (auto *cal = dynamic_cast<DiaryCalendarWidget *>(m_calendarWidget))
                    cal->clearDayInfo();
                onCalendarPageChanged(m_calendarWidget->yearShown(), m_calendarWidget->monthShown());
            }
        }
        if (viKeybindChk->isChecked() != origViKeybind)
            ui->action_ViKeybind->setChecked(viKeybindChk->isChecked());
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
        settings.setValue(KEY_SEARCH_MATCH_COLOR, m_searchMatchColor);
        settings.setValue(KEY_TABLE_HEADER_COLOR, m_tableHeaderColor);
        settings.setValue(KEY_TABLE_ZEBRA_COLOR1, m_tableZebraColor1);
        settings.setValue(KEY_TABLE_ZEBRA_COLOR2, m_tableZebraColor2);
        settings.setValue(KEY_OUTPUT_FONT_SIZE, m_outputFontSize);
    } else {
        applyEditorFont(origFontFamily, origFontSize);
        applyPreviewFont(origPreviewFontFamily, origPreviewFontSize);
        applyOutputFontSize(origOutputFontSize);
        applyLineSpacing(origPercent);
        applyColors(origHeading, origEmphasis, origSearchMatch);
        applyTableColors(origTableHeader, origTableZebra1, origTableZebra2);
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

bool MainWindow::saveTab(int index, const QString &explicitPath)
{
    if (!m_tabWidget || index < 0 || index >= m_tabWidget->count())
        return false;
    QWidget *tab = m_tabWidget->widget(index);
    TextEdit *edit = textEditAt(index);
    if (!tab || !edit)
        return false;

    QString filePath = explicitPath.isEmpty() ? tab->property(PROP_FILE_PATH).toString() : explicitPath;
    const bool isNewPath = !explicitPath.isEmpty() || filePath.isEmpty();
    if (filePath.isEmpty()) {
        filePath = QFileDialog::getSaveFileName(
            this,
            tr("Save File"),
            m_lastDir,
            tr("Markdown Files (*.md);;Text Files (*.txt);;All Files (*)"));
        if (filePath.isEmpty())
            return false;
    }
    if (isNewPath) {
        filePath = QDir::cleanPath(QFileInfo(filePath).absoluteFilePath());
        const int other = findTabByFilePath(filePath);
        if (other >= 0 && other != index) {
            QMessageBox::warning(this, tr("Save Error"),
                tr("The file is already open in another tab:\n%1").arg(filePath));
            return false;
        }
    }

    const auto encoding = tab->property(PROP_ENCODING).isValid()
        ? static_cast<QStringConverter::Encoding>(tab->property(PROP_ENCODING).toInt())
        : QStringConverter::Utf8;
    const bool withBOM = tab->property(PROP_WITH_BOM).isValid()
        ? tab->property(PROP_WITH_BOM).toBool()
        : false;
#ifdef Q_OS_WIN
    const bool defaultCRLF = true;
#else
    const bool defaultCRLF = false;
#endif
    const bool useCRLF = tab->property(PROP_USE_CRLF).isValid()
        ? tab->property(PROP_USE_CRLF).toBool()
        : defaultCRLF;

    QString outText = edit->toPlainText();
    if (useCRLF)
        outText.replace(QChar('\n'), QStringLiteral("\r\n"));

    QSaveFile file(filePath);
    file.setDirectWriteFallback(true);
    if (!file.open(QIODevice::WriteOnly)) {
        QMessageBox::warning(this, tr("Save Error"),
            tr("Could not save file:\n%1\n\n%2").arg(filePath, file.errorString()));
        return false;
    }
    QTextStream out(&file);
    out.setEncoding(encoding);
    out.setGenerateByteOrderMark(withBOM);
    out << outText;
    out.flush();
    if (out.status() != QTextStream::Ok || !file.commit()) {
        QMessageBox::warning(this, tr("Save Error"),
            tr("Could not save file:\n%1\n\n%2").arg(filePath, file.errorString()));
        return false;
    }

    if (isNewPath) {
        QFileInfo fi(filePath);
        const bool isMarkdown = (fi.suffix().compare(QStringLiteral("md"), Qt::CaseInsensitive) == 0
            || fi.suffix().compare(QStringLiteral("markdown"), Qt::CaseInsensitive) == 0);
        tab->setProperty(PROP_FILE_PATH, filePath);
        m_tabWidget->setTabText(index, fi.fileName());
        m_tabWidget->setTabToolTip(index, filePath);
        if (index == m_tabWidget->currentIndex())
            setWindowTitle(fi.fileName() + QStringLiteral(" - TestBuffer"));
        edit->setMarkdownHighlight(isMarkdown);
        if (auto *splitter = qobject_cast<QSplitter *>(tab)) {
            if (auto *preview = dynamic_cast<MarkdownPreview *>(splitter->widget(1))) {
                if (isMarkdown) {
                    preview->document()->setBaseUrl(QUrl::fromLocalFile(fi.absolutePath() + QLatin1Char('/')));
                    preview->setMarkdown(edit->document());
                    edit->updateSearchMarkers();
                } else {
                    preview->clear();
                }
            }
        }
        updateOutlineForTab(tab);
        m_lastDir = fi.absolutePath();
        addToRecentFiles(filePath);
    }

    edit->onSaved();
    if (m_calendarWidget) {
        const QDate d = QDate::fromString(QFileInfo(filePath).baseName(), QStringLiteral("yyyyMMdd"));
        if (d.isValid() && d.year() == m_calendarWidget->yearShown() && d.month() == m_calendarWidget->monthShown())
            onCalendarPageChanged(d.year(), d.month());
    }
    statusBar()->showMessage(tr("Saved: %1").arg(filePath), 5000);
    return true;
}

void MainWindow::onSave()
{
    if (m_tabWidget)
        saveTab(m_tabWidget->currentIndex());
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

    QString text;
    QStringConverter::Encoding encoding = QStringConverter::Utf8;
    bool withBOM = false;
    bool useCRLF = false;
    QString errMsg;
    if (!readTextFileWithMeta(filePath, text, encoding, withBOM, useCRLF, errMsg)) {
        QMessageBox::warning(this, tr("Open Error"),
            tr("Could not open file:\n%1\n\n%2").arg(filePath, errMsg));
        return false;
    }

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
    if (QWidget *activeTab = m_tabWidget->currentWidget()) {
        activeTab->setProperty(PROP_ENCODING, static_cast<int>(encoding));
        activeTab->setProperty(PROP_WITH_BOM, withBOM);
        activeTab->setProperty(PROP_USE_CRLF, useCRLF);
    }

    edit->setMarkdownHighlight(isMarkdown);
    edit->setPlainText(text);
    edit->document()->setModified(false);
    if (MarkdownPreview *preview = currentPreviewEdit()) {
        if (isMarkdown) {
            preview->document()->setBaseUrl(QUrl::fromLocalFile(fi.absolutePath() + QLatin1Char('/')));
            preview->setMarkdown(edit->document());
            edit->updateSearchMarkers();
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
    updateDirBar(fi.absolutePath());
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

void MainWindow::onAboutToShow_FavoriteFiles()
{
    QList<QAction *> actions = ui->menu_FavoriteFiles->actions();
    for (int i = 2; i < actions.size(); ++i) {
        QAction *act = actions.at(i);
        ui->menu_FavoriteFiles->removeAction(act);
        delete act;
    }
    QWidget *curTab = m_tabWidget ? m_tabWidget->currentWidget() : nullptr;
    const QString curPath = curTab ? curTab->property(PROP_FILE_PATH).toString() : QString();
    ui->action_AddThisFavorite->setEnabled(!curPath.isEmpty());

    QSettings settings;
    QStringList favoriteFilePaths = settings.value(KEY_FAVORITE_FILES).toStringList();
    int k = 0;
    QString key;
    for (const QString &fullPathRaw : favoriteFilePaths) {
        if (++k <= 10)
            key = QString::number(k % 10);
        else
            key = QChar(u'A' + k - 11);
        QString fullPath = QDir::cleanPath(fullPathRaw);
        QAction *act = ui->menu_FavoriteFiles->addAction("&" + key + " " + fullPath);
        connect(act, &QAction::triggered, this, [this, fullPath, fullPathRaw]() {
            QSettings settings;
            QStringList favs = settings.value(KEY_FAVORITE_FILES).toStringList();
            favs.removeAll(fullPath);
            favs.removeAll(fullPathRaw);
            if (openFile(fullPath))
                favs.push_front(fullPath);
            settings.setValue(KEY_FAVORITE_FILES, favs);
        });
    }
}

void MainWindow::onAction_AddThisFavorite()
{
    QWidget *curTab = m_tabWidget ? m_tabWidget->currentWidget() : nullptr;
    if (!curTab) return;
    const QString fullPath = QDir::cleanPath(curTab->property(PROP_FILE_PATH).toString());
    if (fullPath.isEmpty()) return;

    QSettings settings;
    QStringList favoriteFilePaths = settings.value(KEY_FAVORITE_FILES).toStringList();
    int ix;
    while ((ix = favoriteFilePaths.indexOf(fullPath)) >= 0)
        favoriteFilePaths.removeAt(ix);
    favoriteFilePaths.push_front(fullPath);
    while (favoriteFilePaths.size() > N_RECENT_FILES)
        favoriteFilePaths.pop_back();
    settings.setValue(KEY_FAVORITE_FILES, favoriteFilePaths);
    statusBar()->showMessage(tr("Added to favorites: %1").arg(fullPath), 3000);
}

void MainWindow::onExportPDF()
{
    MarkdownPreview *preview = currentPreviewEdit();
    if (!preview)
        return;

    QString defaultPath;
    if (QWidget *curTab = m_tabWidget ? m_tabWidget->currentWidget() : nullptr) {
        const QString filePath = curTab->property(PROP_FILE_PATH).toString();
        if (!filePath.isEmpty()) {
            const QFileInfo fi(filePath);
            defaultPath = fi.absolutePath() + QLatin1Char('/') + fi.completeBaseName() + QStringLiteral(".pdf");
        } else if (m_tabWidget) {
            QString title = m_tabWidget->tabText(m_tabWidget->currentIndex());
            if (title.endsWith(QLatin1Char('*')))
                title.chop(1);
            defaultPath = QDir::currentPath() + QLatin1Char('/') + title + QStringLiteral(".pdf");
        }
    }

    QString fileName = QFileDialog::getSaveFileName(
        this,
        tr("Export as PDF"),
        defaultPath,
        tr("PDF Files (*.pdf)"));
    if (fileName.isEmpty())
        return;
    if (!fileName.endsWith(QStringLiteral(".pdf"), Qt::CaseInsensitive))
        fileName += QStringLiteral(".pdf");

    QPrinter printer(QPrinter::HighResolution);
    printer.setOutputFormat(QPrinter::PdfFormat);
    printer.setOutputFileName(fileName);
    preview->print(&printer);
    statusBar()->showMessage(tr("PDF exported: %1").arg(fileName), 5000);
}

void MainWindow::do_output(const QString& txt)
{
    ui->outputBar->setVisible(true);
    QTextCursor cursor = ui->plainTextEdit->textCursor();
    cursor.movePosition(QTextCursor::End);
    cursor.insertText(txt);
    ui->plainTextEdit->setTextCursor(cursor);
    ui->plainTextEdit->ensureCursorVisible();
    if (txt.contains(QChar('\n')))
        QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
}

void MainWindow::openViCmdLine(QChar prefix)
{
    if (!m_cmdLine) return;
    m_cmdPrefix = prefix;
    m_exHistIdx = -1;
    m_incSearchActive = false;
    m_incSearchEdit.clear();
    if (prefix == QChar('/') || prefix == QChar('?')) {
        if (TextEdit *edit = currentTextEdit()) {
            m_incSearchActive = true;
            m_incSearchEdit = edit;
            m_incSearchOrigPos = static_cast<int>(edit->textCursor().position());
            m_incSearchOrigScroll = edit->verticalScrollBar()->value();
            m_incSearchOrigHighlight = edit->searchHighlightPattern();
        }
    }
    statusBar()->clearMessage();
    m_cmdLine->setText(QString(prefix));
    m_cmdLine->show();
    m_cmdLine->setFocus();
    m_cmdLine->setCursorPosition(1);
}

void MainWindow::onViCmdLineTextChanged(const QString &raw)
{
    if (!m_incSearchActive || !m_cmdLine || !m_cmdLine->isVisible())
        return;
    TextEdit *edit = static_cast<TextEdit *>(m_incSearchEdit.data());
    if (!edit || raw.isEmpty())
        return;
    const QChar pref = raw[0];
    if (pref != QChar('/') && pref != QChar('?'))
        return;
    const QString pat = raw.mid(1);
    if (pat.isEmpty()) {
        TextCursor c = edit->textCursor();
        c.setPosition(qBound(0, m_incSearchOrigPos, static_cast<int>(edit->document()->characterCount())));
        edit->setTextCursor(c);
        edit->verticalScrollBar()->setValue(m_incSearchOrigScroll);
        edit->setSearchHighlight(m_incSearchOrigHighlight);
        return;
    }
    if (!edit->viEngine()->previewSearch(edit, pat, pref == QChar('/'), m_incSearchOrigPos)) {
        TextCursor c = edit->textCursor();
        c.setPosition(qBound(0, m_incSearchOrigPos, static_cast<int>(edit->document()->characterCount())));
        edit->setTextCursor(c);
        edit->verticalScrollBar()->setValue(m_incSearchOrigScroll);
    }
}

void MainWindow::cancelViIncSearch()
{
    if (!m_incSearchActive)
        return;
    m_incSearchActive = false;
    if (TextEdit *edit = static_cast<TextEdit *>(m_incSearchEdit.data())) {
        TextCursor c = edit->textCursor();
        c.setPosition(qBound(0, m_incSearchOrigPos, static_cast<int>(edit->document()->characterCount())));
        edit->setTextCursor(c);
        edit->verticalScrollBar()->setValue(m_incSearchOrigScroll);
        edit->setSearchHighlight(m_incSearchOrigHighlight);
    }
    m_incSearchEdit.clear();
}

void MainWindow::onViCmdLineReturn()
{
    if (!m_cmdLine) return;
    QString raw = m_cmdLine->text();
    const bool wasIncSearch = m_incSearchActive;
    TextEdit *incEdit = static_cast<TextEdit *>(m_incSearchEdit.data());
    m_incSearchActive = false;
    m_incSearchEdit.clear();
    m_cmdLine->hide();
    TextEdit *edit = currentTextEdit();
    if (edit)
        edit->setFocus();
    else
        setFocus();

    if (raw.isEmpty()) return;
    const QChar pref = raw[0];
    const QString body = (pref == QChar(':') || pref == QChar('/') || pref == QChar('?')) ? raw.mid(1) : raw;
    if (!body.isEmpty()) {
        QStringList &hist = (pref == QChar('/') || pref == QChar('?')) ? m_searchHist : m_exHist;
        hist.removeAll(raw);
        hist.push_front(raw);
        while (hist.size() > 50) hist.pop_back();
    }

    if (pref == QChar('/') || pref == QChar('?')) {
        TextEdit *target = incEdit ? incEdit : edit;
        if (!target) return;
        if (!body.isEmpty()) {
            if (wasIncSearch) {
                TextCursor c = target->textCursor();
                c.setPosition(qBound(0, m_incSearchOrigPos, static_cast<int>(target->document()->characterCount())));
                target->setTextCursor(c);
            }
            target->viEngine()->search(target, body, pref == QChar('/'));
            if (m_searchCB) {
                m_toolbarSearchHist.removeAll(body);
                m_toolbarSearchHist.push_front(body);
                while (m_toolbarSearchHist.size() > 20)
                    m_toolbarSearchHist.pop_back();
                QSignalBlocker sb(m_searchCB);
                m_searchCB->clear();
                m_searchCB->addItems(m_toolbarSearchHist);
                m_searchCB->setCurrentText(body);
                QSettings settings;
                settings.setValue(KEY_SEARCH_HISTORY, m_toolbarSearchHist);
            }
        }
        return;
    }

    ViEngine::ExCallbacks cb;
    cb.showStatus = [this](const QString &msg) { statusBar()->showMessage(msg, 5000); };
    cb.showOutput = [this](const QString &txt) { do_output(txt); };
    cb.saveFile = [this](const QString &path) {
        return m_tabWidget ? saveTab(m_tabWidget->currentIndex(), path) : false;
    };
    cb.closeTab = [this](bool force) {
        if (!m_tabWidget || m_tabWidget->count() == 0) {
            close();
            return;
        }
        const int ix = m_tabWidget->currentIndex();
        if (force) closeTabForce(ix);
        else onTabCloseRequested(ix);
    };
    cb.openFile = [this](const QString &path) {
        if (path.isEmpty()) onOpen();
        else openFile(path);
    };
    cb.newTab = [this]() {
        onNewTab();
    };
    const QString prevDir = QDir::currentPath();
    if (edit) {
        edit->viEngine()->executeExCommand(edit, body, cb);
    } else {
        ViEngine tempEngine;
        tempEngine.executeExCommand(nullptr, body, cb);
    }
    if (QDir::currentPath() != prevDir)
        updateDirBar(QDir::currentPath());
}

void MainWindow::keyPressEvent(QKeyEvent *event)
{
    if (!currentTextEdit() && (!m_cmdLine || !m_cmdLine->isVisible())) {
        if (event->text() == QStringLiteral(":")
            && !(event->modifiers() & (Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier))) {
            openViCmdLine(QChar(':'));
            event->accept();
            return;
        }
    }
    QMainWindow::keyPressEvent(event);
}

bool MainWindow::eventFilter(QObject *obj, QEvent *event)
{
    if (ui && ui->plainTextEdit && obj == ui->plainTextEdit->viewport()
        && event->type() == QEvent::MouseButtonDblClick) {
        auto *me = static_cast<QMouseEvent *>(event);
        if (me->button() == Qt::LeftButton) {
            const QTextCursor tc = ui->plainTextEdit->cursorForPosition(me->pos());
            if (jumpToOutputLine(tc.blockNumber()))
                return true;
        }
    }
    if (m_searchCB && obj == m_searchCB->lineEdit() && event->type() == QEvent::KeyPress) {
        auto *ke = static_cast<QKeyEvent *>(event);
        if (ke->key() == Qt::Key_Escape) {
            if (TextEdit *edit = currentTextEdit())
                edit->setFocus();
            else
                setFocus();
            return true;
        }
    }
    if (obj == m_cmdLine && event->type() == QEvent::KeyPress) {
        auto *ke = static_cast<QKeyEvent *>(event);
        if (ke->key() == Qt::Key_Escape) {
#ifdef Q_OS_WIN
            if (HWND hwnd = reinterpret_cast<HWND>(effectiveWinId())) {
                if (HIMC himc = ImmGetContext(hwnd)) {
                    ImmSetOpenStatus(himc, FALSE);
                    ImmReleaseContext(hwnd, himc);
                }
            }
#endif
            m_cmdLine->hide();
            cancelViIncSearch();
            if (TextEdit *edit = currentTextEdit())
                edit->setFocus();
            else
                setFocus();
            return true;
        }
        const bool willClearAll = m_cmdLine->hasSelectedText()
            && m_cmdLine->selectionStart() == 0
            && m_cmdLine->selectedText().size() >= m_cmdLine->text().size();
        if ((ke->key() == Qt::Key_Backspace && (m_cmdLine->text().size() <= 1 || willClearAll))
            || (ke->key() == Qt::Key_Delete && willClearAll)) {
#ifdef Q_OS_WIN
            if (HWND hwnd = reinterpret_cast<HWND>(effectiveWinId())) {
                if (HIMC himc = ImmGetContext(hwnd)) {
                    ImmSetOpenStatus(himc, FALSE);
                    ImmReleaseContext(hwnd, himc);
                }
            }
#endif
            m_cmdLine->hide();
            cancelViIncSearch();
            if (TextEdit *edit = currentTextEdit())
                edit->setFocus();
            else
                setFocus();
            return true;
        }
        const QStringList &hist = (m_cmdPrefix == QChar('/') || m_cmdPrefix == QChar('?')) ? m_searchHist : m_exHist;
        if (ke->key() == Qt::Key_Up && !hist.isEmpty()) {
            m_exHistIdx = qMin(m_exHistIdx + 1, static_cast<int>(hist.size()) - 1);
            m_cmdLine->setText(hist[m_exHistIdx]);
            return true;
        }
        if (ke->key() == Qt::Key_Down) {
            if (m_exHistIdx > 0) {
                --m_exHistIdx;
                m_cmdLine->setText(hist[m_exHistIdx]);
            } else {
                m_exHistIdx = -1;
                m_cmdLine->setText(QString(m_cmdPrefix));
            }
            return true;
        }
    }
    return QMainWindow::eventFilter(obj, event);
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (m_tabWidget) {
        for (int i = 0; i < m_tabWidget->count(); ++i) {
            if (!maybeSaveTab(i)) {
                event->ignore();
                return;
            }
        }
    }
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
void MainWindow::onBenchmarkPushBack()
{
    TextEdit *edit = currentTextEdit();
    if (!edit) {
        onNewTab();
        edit = currentTextEdit();
        if (!edit) return;
    }

    //	1セクション = 見出し1行 + 本文24行 = 25行、4セクションで合計100行
    constexpr int kSections = 4;
    constexpr int kBodyLinesPerSection = 24;
    const QString bodyLine = QStringLiteral("body text ").repeated(10) + QChar('\n');
    QString payload;
    payload.reserve(kSections * (16 + kBodyLinesPerSection * bodyLine.size()));
    for (int s = 1; s <= kSections; ++s) {
        payload += QStringLiteral("## heading %1\n").arg(s);
        for (int b = 0; b < kBodyLinesPerSection; ++b)
            payload += bodyLine;
    }

    const bool wasVi = edit->viEngine()->isEnabled();
    if (wasVi)

        edit->setViEnabled(false);
    //	文書末尾へ移動してから1文字ずつ挿入
    TextCursor cur = edit->textCursor();
    cur.setPosition(edit->document()->characterCount());
    edit->setTextCursor(cur);

    const int totalLines = kSections * (1 + kBodyLinesPerSection);
    const qsizetype totalChars = payload.size();
    do_output(QStringLiteral("\n[Benchmark] Push_back: inserting %1 lines (%2 chars) 1 char at a time...\n")
                  .arg(totalLines).arg(totalChars));

    QElapsedTimer timer;
    timer.start();
    for (qsizetype i = 0; i < totalChars; ++i) {
        const QChar ch = payload[i];
        if (ch == QChar('\n')) {
            QKeyEvent ev(QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier, QStringLiteral("\n"));
            edit->replayKeyEvent(&ev);
        } else {
            QKeyEvent ev(QEvent::KeyPress, 0, Qt::NoModifier, QString(ch));
            edit->replayKeyEvent(&ev);
        }
    }
    const qint64 elapsedMs = timer.elapsed();

    if (wasVi)
        edit->setViEnabled(true);

    const double avgMs = totalChars > 0 ? static_cast<double>(elapsedMs) / static_cast<double>(totalChars) : 0.0;
    do_output(QStringLiteral("[Benchmark] Push_back done: %1 ms (%2 lines, %3 chars, %4 ms/char)\n")
                  .arg(elapsedMs).arg(totalLines).arg(totalChars).arg(avgMs, 0, 'f', 4));
}

void MainWindow::onBenchmarkPushFront()
{
    TextEdit *edit = currentTextEdit();
    if (!edit) {
        onNewTab();
        edit = currentTextEdit();
        if (!edit) return;
    }

    //	1セクション = 見出し1行 + 本文24行 = 25行、4セクションで合計100行
    constexpr int kSections = 4;
    constexpr int kBodyLinesPerSection = 24;
    const QString bodyLine = QStringLiteral("body text ").repeated(10) + QChar('\n');
    QString payload;
    payload.reserve(kSections * (16 + kBodyLinesPerSection * bodyLine.size()));
    for (int s = 1; s <= kSections; ++s) {
        payload += QStringLiteral("## heading %1\n").arg(s);
        for (int b = 0; b < kBodyLinesPerSection; ++b)
            payload += bodyLine;
    }

    const bool wasVi = edit->viEngine()->isEnabled();
    if (wasVi)
        edit->setViEnabled(false);

    const int totalLines = kSections * (1 + kBodyLinesPerSection);
    const qsizetype totalChars = payload.size();
    do_output(QStringLiteral("\n[Benchmark] push_front: inserting %1 lines (%2 chars) 1 char at a time at pos 0...\n")
                  .arg(totalLines).arg(totalChars));

    QElapsedTimer timer;
    timer.start();
    for (qsizetype i = totalChars - 1; i >= 0; --i) {
        TextCursor cur = edit->textCursor();
        cur.setPosition(0);
        edit->setTextCursor(cur);
        const QChar ch = payload[i];
        if (ch == QChar('\n')) {
            QKeyEvent ev(QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier, QStringLiteral("\n"));
            edit->replayKeyEvent(&ev);
        } else {
            QKeyEvent ev(QEvent::KeyPress, 0, Qt::NoModifier, QString(ch));
            edit->replayKeyEvent(&ev);
        }
    }
    const qint64 elapsedMs = timer.elapsed();

    if (wasVi)
        edit->setViEnabled(true);

    const double avgMs = totalChars > 0 ? static_cast<double>(elapsedMs) / static_cast<double>(totalChars) : 0.0;
    do_output(QStringLiteral("[Benchmark] push_front done: %1 ms (%2 lines, %3 chars, %4 ms/char)\n")
                  .arg(elapsedMs).arg(totalLines).arg(totalChars).arg(avgMs, 0, 'f', 4));
}

void MainWindow::onAction_About() {
	QMessageBox::about(this, 
		"About viMarkdown", // タイトルバー
		
		"<p><big><b>viMarkdown</b></big> " + VER_STR + "</p>"
		"<p>The efficient visual Markdown editor"
		"<br>Copyright (C) 2025, 2026 by N.Tsuda"
		"<br>Powered by C++, <a href=\"https://www.qt.io/\">Qt6</a>, "
		"<a href=\"https://github.com/cubicdaiya/dtl\">dtl</a>, "
		"and <a href=\"https://github.com/sammycage/lunasvg\">LunaSVG</a></p>"
	);
}

namespace {
QString expandReplaceText(const QString &dst, const QRegularExpressionMatch &match, bool useRegExp)
{
    if (!useRegExp)
        return dst;
    QString out;
    out.reserve(dst.size());
    for (qsizetype i = 0; i < dst.size(); ++i) {
        const QChar c = dst[i];
        if (c == QChar('\\') && i + 1 < dst.size()) {
            const QChar nxt = dst[i + 1];
            if (nxt == QChar('\\')) { out.append(QChar('\\')); ++i; }
            else if (nxt == QChar('n')) { out.append(QChar('\n')); ++i; }
            else if (nxt == QChar('t')) { out.append(QChar('\t')); ++i; }
            else if (nxt == QChar('&')) { out.append(QChar('&')); ++i; }
            else if (nxt >= QChar('0') && nxt <= QChar('9')) {
                const int idx = nxt.unicode() - '0';
                if (idx <= match.lastCapturedIndex())
                    out.append(match.captured(idx));
                ++i;
            } else {
                out.append(c);
            }
        } else if (c == QChar('&')) {
            out.append(match.captured(0));
        } else if (c == QChar('$') && i + 1 < dst.size()) {
            const QChar nxt = dst[i + 1];
            if (nxt == QChar('&')) { out.append(match.captured(0)); ++i; }
            else if (nxt >= QChar('0') && nxt <= QChar('9')) {
                const int idx = nxt.unicode() - '0';
                if (idx <= match.lastCapturedIndex())
                    out.append(match.captured(idx));
                ++i;
            } else {
                out.append(c);
            }
        } else {
            out.append(c);
        }
    }
    return out;
}
}

bool MainWindow::findAndSelectMatch(const QString &src, bool forward)
{
    TextEdit *edit = currentTextEdit();
    if (!edit || src.isEmpty())
        return false;

    const QString eff = m_searchRegExp ? src : QRegularExpression::escape(src);
    const auto opt = m_searchIgnoreCase ? QRegularExpression::CaseInsensitiveOption : QRegularExpression::NoPatternOption;
    QRegularExpression re(eff, opt);
    if (!re.isValid()) {
        statusBar()->showMessage(tr("Invalid regular expression: %1").arg(re.errorString()), 5000);
        return false;
    }

    m_toolbarSearchHist.removeAll(src);
    m_toolbarSearchHist.push_front(src);
    while (m_toolbarSearchHist.size() > 20)
        m_toolbarSearchHist.pop_back();
    if (m_searchCB) {
        QSignalBlocker sb(m_searchCB);
        m_searchCB->clear();
        m_searchCB->addItems(m_toolbarSearchHist);
        m_searchCB->setCurrentText(src);
    }
    {
        QSettings settings;
        settings.setValue(KEY_SEARCH_HISTORY, m_toolbarSearchHist);
    }
    edit->viEngine()->setSearchOptions(m_searchIgnoreCase, m_searchRegExp);
    edit->setSearchHighlight(src, m_searchIgnoreCase, m_searchRegExp);

    const QString fullText = edit->document()->toPlainText();
    if (fullText.isEmpty()) {
        statusBar()->showMessage(tr("Pattern not found: %1").arg(src), 5000);
        return false;
    }

    const TextCursor cur = edit->textCursor();
    qsizetype foundStart = -1;
    qsizetype foundLen = 0;
    if (forward) {
        const qsizetype start = cur.hasSelection()
            ? qMin(static_cast<qsizetype>(cur.selectionEnd()), fullText.size())
            : qMin(static_cast<qsizetype>(cur.position()), fullText.size());
        QRegularExpressionMatch m = re.match(fullText, start);
        if (!m.hasMatch())
            m = re.match(fullText, 0);
        if (m.hasMatch()) {
            foundStart = m.capturedStart();
            foundLen = m.capturedLength();
        }
    } else {
        const qsizetype limit = cur.hasSelection()
            ? static_cast<qsizetype>(cur.selectionStart())
            : static_cast<qsizetype>(cur.position());
        auto it = re.globalMatch(fullText);
        qsizetype lastBeforeStart = -1, lastBeforeLen = 0;
        qsizetype lastAnyStart = -1, lastAnyLen = 0;
        while (it.hasNext()) {
            const QRegularExpressionMatch m = it.next();
            if (m.capturedStart() < limit) {
                lastBeforeStart = m.capturedStart();
                lastBeforeLen = m.capturedLength();
            }
            lastAnyStart = m.capturedStart();
            lastAnyLen = m.capturedLength();
        }
        if (lastBeforeStart >= 0) {
            foundStart = lastBeforeStart;
            foundLen = lastBeforeLen;
        } else if (lastAnyStart >= 0) {
            foundStart = lastAnyStart;
            foundLen = lastAnyLen;
        }
    }

    if (foundStart < 0) {
        statusBar()->showMessage(tr("Pattern not found: %1").arg(src), 5000);
        return false;
    }

    TextDocument *doc = edit->document();
    if (doc->lineMgr().unfoldContaining(doc->lineMgr().lineAtPos(static_cast<TextDocument::pos_t>(foundStart))))
        edit->refreshAfterFold();
    TextCursor c(doc);
    c.setPosition(static_cast<TextDocument::pos_t>(foundStart));
    c.setPosition(static_cast<TextDocument::pos_t>(foundStart + foundLen), TextCursor::KeepAnchor);
    edit->setTextCursor(c);
    return true;
}

void MainWindow::doReplaceNext(const QString &src, const QString &dst)
{
    TextEdit *edit = currentTextEdit();
    if (!edit || src.isEmpty())
        return;

    const QString eff = m_searchRegExp ? src : QRegularExpression::escape(src);
    const auto opt = m_searchIgnoreCase ? QRegularExpression::CaseInsensitiveOption : QRegularExpression::NoPatternOption;
    QRegularExpression re(eff, opt);
    if (!re.isValid()) {
        statusBar()->showMessage(tr("Invalid regular expression: %1").arg(re.errorString()), 5000);
        return;
    }

    m_replaceHist.removeAll(dst);
    m_replaceHist.push_front(dst);
    while (m_replaceHist.size() > 20)
        m_replaceHist.pop_back();
    {
        QSettings settings;
        settings.setValue(KEY_REPLACE_HISTORY, m_replaceHist);
    }

    const TextCursor cur = edit->textCursor();
    if (cur.hasSelection()) {
        const QString sel = cur.selectedText();
        const QRegularExpressionMatch m = re.match(sel);
        if (m.hasMatch() && m.capturedStart() == 0 && m.capturedLength() == sel.size()) {
            const QString rep = expandReplaceText(dst, m, m_searchRegExp);
            edit->applyPreviewEdit(static_cast<int>(cur.selectionStart()), static_cast<int>(cur.selectionEnd()), rep);
            findAndSelectMatch(src, true);
            return;
        }
    }
    findAndSelectMatch(src, true);
}

void MainWindow::doReplaceAll(const QString &src, const QString &dst)
{
    TextEdit *edit = currentTextEdit();
    if (!edit || src.isEmpty())
        return;

    const QString eff = m_searchRegExp ? src : QRegularExpression::escape(src);
    const auto opt = m_searchIgnoreCase ? QRegularExpression::CaseInsensitiveOption : QRegularExpression::NoPatternOption;
    QRegularExpression re(eff, opt);
    if (!re.isValid()) {
        statusBar()->showMessage(tr("Invalid regular expression: %1").arg(re.errorString()), 5000);
        return;
    }

    m_toolbarSearchHist.removeAll(src);
    m_toolbarSearchHist.push_front(src);
    while (m_toolbarSearchHist.size() > 20)
        m_toolbarSearchHist.pop_back();
    m_replaceHist.removeAll(dst);
    m_replaceHist.push_front(dst);
    while (m_replaceHist.size() > 20)
        m_replaceHist.pop_back();
    if (m_searchCB) {
        QSignalBlocker sb(m_searchCB);
        m_searchCB->clear();
        m_searchCB->addItems(m_toolbarSearchHist);
        m_searchCB->setCurrentText(src);
    }
    {
        QSettings settings;
        settings.setValue(KEY_SEARCH_HISTORY, m_toolbarSearchHist);
        settings.setValue(KEY_REPLACE_HISTORY, m_replaceHist);
    }

    TextDocument *doc = edit->document();
    const QString fullText = doc->toPlainText();
    struct MatchItem {
        qsizetype start;
        qsizetype len;
        QString rep;
    };
    QVector<MatchItem> matches;
    auto it = re.globalMatch(fullText);
    while (it.hasNext()) {
        const QRegularExpressionMatch m = it.next();
        if (m.capturedLength() == 0 && !matches.isEmpty() && matches.back().start == m.capturedStart())
            continue;
        matches.push_back({m.capturedStart(), m.capturedLength(), expandReplaceText(dst, m, m_searchRegExp)});
    }
    if (matches.isEmpty()) {
        statusBar()->showMessage(tr("Pattern not found: %1").arg(src), 5000);
        return;
    }

    doc->undoMgr().openBlock();
    for (qsizetype i = matches.size() - 1; i >= 0; --i) {
        const auto &mi = matches[i];
        if (mi.len > 0)
            doc->erase(static_cast<TextDocument::pos_t>(mi.start), static_cast<TextDocument::pos_t>(mi.start + mi.len));
        if (!mi.rep.isEmpty())
            doc->insert(static_cast<TextDocument::pos_t>(mi.start), mi.rep);
    }
    doc->undoMgr().closeBlock();

    TextCursor c(doc);
    const auto &first = matches.first();
    c.setPosition(static_cast<TextDocument::pos_t>(first.start + first.rep.size()));
    edit->setTextCursor(c);
    edit->notifyTextChanged();
    edit->setSearchHighlight(src, m_searchIgnoreCase, m_searchRegExp);
    statusBar()->showMessage(tr("%1 occurrence(s) replaced.").arg(matches.size()), 5000);
}

void MainWindow::onReplace()
{
    if (!currentTextEdit())
        return;

    QDialog dlg(this);
    dlg.setWindowTitle(tr("Replace"));
    dlg.setMinimumWidth(460);
    auto *layout = new QVBoxLayout(&dlg);
    auto *form = new QFormLayout();

    auto *srcCombo = new QComboBox(&dlg);
    srcCombo->setEditable(true);
    srcCombo->setInsertPolicy(QComboBox::NoInsert);
    srcCombo->addItems(m_toolbarSearchHist);
    QString initSrc;
    if (TextEdit *edit = currentTextEdit()) {
        const QString sel = edit->textCursor().selectedText();
        if (!sel.isEmpty() && !sel.contains(QChar('\n')))
            initSrc = sel;
    }
    if (initSrc.isEmpty() && m_searchCB)
        initSrc = m_searchCB->currentText();
    if (!initSrc.isEmpty())
        srcCombo->setCurrentText(initSrc);
    if (QLineEdit *le = srcCombo->lineEdit())
        le->selectAll();
    form->addRow(tr("&Find what:"), srcCombo);

    auto *dstCombo = new QComboBox(&dlg);
    dstCombo->setEditable(true);
    dstCombo->setInsertPolicy(QComboBox::NoInsert);
    dstCombo->addItems(m_replaceHist);
    if (!m_replaceHist.isEmpty())
        dstCombo->setCurrentText(m_replaceHist.first());
    form->addRow(tr("Re&place with:"), dstCombo);
    layout->addLayout(form);

    auto *optsRow = new QHBoxLayout();
    auto *chkIgnoreCase = new QCheckBox(tr("&Ignore Case"), &dlg);
    chkIgnoreCase->setChecked(m_searchIgnoreCase);
    auto *chkRegExp = new QCheckBox(tr("&Regular Expression"), &dlg);
    chkRegExp->setChecked(m_searchRegExp);
    optsRow->addWidget(chkIgnoreCase);
    optsRow->addWidget(chkRegExp);
    optsRow->addStretch();
    layout->addLayout(optsRow);

    connect(chkIgnoreCase, &QCheckBox::toggled, &dlg, [this](bool checked) {
        if (m_actIgnoreCase) m_actIgnoreCase->setChecked(checked);
        else m_searchIgnoreCase = checked;
    });
    connect(chkRegExp, &QCheckBox::toggled, &dlg, [this](bool checked) {
        if (m_actRegExp) m_actRegExp->setChecked(checked);
        else m_searchRegExp = checked;
    });

    auto *btnRow1 = new QHBoxLayout();
    auto *btnPrev = new QPushButton(tr("Find &Prev"), &dlg);
    auto *btnNext = new QPushButton(tr("Find &Next"), &dlg);
    auto *btnReplace = new QPushButton(tr("&Replace"), &dlg);
    auto *btnReplaceAll = new QPushButton(tr("Replace &All"), &dlg);
    btnNext->setDefault(true);
    btnRow1->addWidget(btnPrev);
    btnRow1->addWidget(btnNext);
    btnRow1->addWidget(btnReplace);
    btnRow1->addWidget(btnReplaceAll);
    layout->addLayout(btnRow1);

    auto *btnRow2 = new QHBoxLayout();
    auto *btnUndo = new QPushButton(tr("&Undo"), &dlg);
    auto *btnRedo = new QPushButton(tr("Re&do"), &dlg);
    auto *btnClose = new QPushButton(tr("&Close"), &dlg);
    btnRow2->addWidget(btnUndo);
    btnRow2->addWidget(btnRedo);
    btnRow2->addStretch();
    btnRow2->addWidget(btnClose);
    layout->addLayout(btnRow2);

    auto refreshCombos = [this, srcCombo, dstCombo](const QString &src, const QString &dst) {
        {
            QSignalBlocker sb(srcCombo);
            srcCombo->clear();
            srcCombo->addItems(m_toolbarSearchHist);
            srcCombo->setCurrentText(src);
        }
        {
            QSignalBlocker sb(dstCombo);
            dstCombo->clear();
            dstCombo->addItems(m_replaceHist);
            dstCombo->setCurrentText(dst);
        }
    };

    connect(btnPrev, &QPushButton::clicked, &dlg, [this, srcCombo, dstCombo, refreshCombos]() {
        const QString src = srcCombo->currentText();
        if (src.isEmpty()) return;
        findAndSelectMatch(src, false);
        refreshCombos(src, dstCombo->currentText());
    });
    connect(btnNext, &QPushButton::clicked, &dlg, [this, srcCombo, dstCombo, refreshCombos]() {
        const QString src = srcCombo->currentText();
        if (src.isEmpty()) return;
        findAndSelectMatch(src, true);
        refreshCombos(src, dstCombo->currentText());
    });
    connect(btnReplace, &QPushButton::clicked, &dlg, [this, srcCombo, dstCombo, refreshCombos]() {
        const QString src = srcCombo->currentText();
        const QString dst = dstCombo->currentText();
        if (src.isEmpty()) return;
        doReplaceNext(src, dst);
        refreshCombos(src, dst);
    });
    connect(btnReplaceAll, &QPushButton::clicked, &dlg, [this, srcCombo, dstCombo, refreshCombos]() {
        const QString src = srcCombo->currentText();
        const QString dst = dstCombo->currentText();
        if (src.isEmpty()) return;
        doReplaceAll(src, dst);
        refreshCombos(src, dst);
    });
    connect(btnUndo, &QPushButton::clicked, &dlg, [this]() { onUndo(); });
    connect(btnRedo, &QPushButton::clicked, &dlg, [this]() { onRedo(); });
    connect(btnClose, &QPushButton::clicked, &dlg, &QDialog::reject);

    auto *undoSc = new QShortcut(QKeySequence::Undo, &dlg);
    connect(undoSc, &QShortcut::activated, &dlg, [this]() { onUndo(); });
    auto *redoSc = new QShortcut(QKeySequence::Redo, &dlg);
    connect(redoSc, &QShortcut::activated, &dlg, [this]() { onRedo(); });

    srcCombo->setFocus();
    dlg.exec();
    if (TextEdit *edit = currentTextEdit())
        edit->setFocus();
}

void MainWindow::onGrep()
{
    QDialog dlg(this);
    dlg.setWindowTitle(tr("Grep"));
    dlg.setMinimumWidth(520);
    auto *layout = new QVBoxLayout(&dlg);
    auto *form = new QFormLayout();

    auto *patCombo = new QComboBox(&dlg);
    patCombo->setEditable(true);
    patCombo->setInsertPolicy(QComboBox::NoInsert);
    patCombo->addItems(m_toolbarSearchHist);
    QString initPat;
    if (TextEdit *edit = currentTextEdit()) {
        const QString sel = edit->textCursor().selectedText();
        if (!sel.isEmpty() && !sel.contains(QChar('\n')))
            initPat = sel;
    }
    if (initPat.isEmpty() && m_searchCB)
        initPat = m_searchCB->currentText();
    if (!initPat.isEmpty())
        patCombo->setCurrentText(initPat);
    if (QLineEdit *le = patCombo->lineEdit())
        le->selectAll();
    form->addRow(tr("&Find what:"), patCombo);

    auto *dirRow = new QHBoxLayout();
    auto *dirCombo = new QComboBox(&dlg);
    dirCombo->setEditable(true);
    dirCombo->setInsertPolicy(QComboBox::NoInsert);
    dirCombo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    dirCombo->addItems(m_grepDirHist);
    QString initDir;
    if (QWidget *curTab = m_tabWidget ? m_tabWidget->currentWidget() : nullptr) {
        const QString fp = curTab->property(PROP_FILE_PATH).toString();
        if (!fp.isEmpty())
            initDir = QFileInfo(fp).absolutePath();
    }
    if (initDir.isEmpty())
        initDir = !m_lastDir.isEmpty() ? m_lastDir : (!m_grepDirHist.isEmpty() ? m_grepDirHist.first() : QDir::currentPath());
    dirCombo->setCurrentText(initDir);
    auto *browseBtn = new QPushButton(tr("Browse..."), &dlg);
    connect(browseBtn, &QPushButton::clicked, &dlg, [&dlg, dirCombo]() {
        const QString chosen = QFileDialog::getExistingDirectory(&dlg, tr("Select Folder"), dirCombo->currentText());
        if (!chosen.isEmpty())
            dirCombo->setCurrentText(QDir::cleanPath(chosen));
    });
    dirRow->addWidget(dirCombo, 1);
    dirRow->addWidget(browseBtn);
    form->addRow(tr("&Folder:"), dirRow);

    auto *filterEdit = new QLineEdit(m_grepFilter, &dlg);
    filterEdit->setPlaceholderText(QStringLiteral("*.md"));
    form->addRow(tr("File &Types:"), filterEdit);
    layout->addLayout(form);

    auto *optsRow1 = new QHBoxLayout();
    auto *chkIgnoreCase = new QCheckBox(tr("&Ignore Case"), &dlg);
    chkIgnoreCase->setChecked(m_searchIgnoreCase);
    auto *chkRegExp = new QCheckBox(tr("&Regular Expression"), &dlg);
    chkRegExp->setChecked(m_searchRegExp);
    optsRow1->addWidget(chkIgnoreCase);
    optsRow1->addWidget(chkRegExp);
    optsRow1->addStretch();
    layout->addLayout(optsRow1);

    auto *optsRow2 = new QHBoxLayout();
    auto *chkSubDir = new QCheckBox(tr("Include &Subdirectories"), &dlg);
    chkSubDir->setChecked(m_grepSubDir);
    auto *chkClearOut = new QCheckBox(tr("&Clear Output"), &dlg);
    chkClearOut->setChecked(m_grepClearOutput);
    optsRow2->addWidget(chkSubDir);
    optsRow2->addWidget(chkClearOut);
    optsRow2->addStretch();
    layout->addLayout(optsRow2);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    if (QPushButton *okBtn = buttons->button(QDialogButtonBox::Ok))
        okBtn->setText(tr("Grep"));
    connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    layout->addWidget(buttons);

    patCombo->setFocus();
    if (dlg.exec() != QDialog::Accepted)
        return;

    const QString searchText = patCombo->currentText();
    const QString dirPath = QDir::cleanPath(dirCombo->currentText().trimmed());
    QString filterText = filterEdit->text().trimmed();
    if (filterText.isEmpty())
        filterText = QStringLiteral("*.md");
    if (searchText.isEmpty() || dirPath.isEmpty())
        return;

    if (m_actIgnoreCase)
        m_actIgnoreCase->setChecked(chkIgnoreCase->isChecked());
    else
        m_searchIgnoreCase = chkIgnoreCase->isChecked();
    if (m_actRegExp)
        m_actRegExp->setChecked(chkRegExp->isChecked());
    else
        m_searchRegExp = chkRegExp->isChecked();
    m_grepSubDir = chkSubDir->isChecked();
    m_grepClearOutput = chkClearOut->isChecked();
    m_grepFilter = filterText;

    doGrep(searchText, dirPath, filterText, m_grepSubDir, m_grepClearOutput);
}

void MainWindow::doGrep(const QString &searchText, const QString &dirPath, const QString &filterText, bool subDir, bool clearOutput)
{
    const Qt::CaseSensitivity ic = m_searchIgnoreCase ? Qt::CaseInsensitive : Qt::CaseSensitive;
    QRegularExpression re;
    if (m_searchRegExp) {
        const auto opt = m_searchIgnoreCase ? QRegularExpression::CaseInsensitiveOption : QRegularExpression::NoPatternOption;
        re = QRegularExpression(searchText, opt);
        if (!re.isValid()) {
            const QString msg = tr("Invalid regular expression: %1").arg(re.errorString());
            do_output(msg + QLatin1Char('\n'));
            statusBar()->showMessage(msg, 5000);
            return;
        }
    }

    //	履歴・設定の保存（正規表現検証を通過してから更新）
    m_toolbarSearchHist.removeAll(searchText);
    m_toolbarSearchHist.push_front(searchText);
    while (m_toolbarSearchHist.size() > 20)
        m_toolbarSearchHist.pop_back();
    if (m_searchCB) {
        QSignalBlocker sb(m_searchCB);
        m_searchCB->clear();
        m_searchCB->addItems(m_toolbarSearchHist);
        m_searchCB->setCurrentText(searchText);
    }
    m_grepDirHist.removeAll(dirPath);
    m_grepDirHist.push_front(dirPath);
    while (m_grepDirHist.size() > 20)
        m_grepDirHist.pop_back();
    {
        QSettings settings;
        settings.setValue(KEY_SEARCH_HISTORY, m_toolbarSearchHist);
        settings.setValue(KEY_GREP_DIR_HISTORY, m_grepDirHist);
        settings.setValue(KEY_GREP_SUBDIR, m_grepSubDir);
        settings.setValue(KEY_GREP_CLEAR_OUTPUT, m_grepClearOutput);
        settings.setValue(KEY_GREP_FILTER, m_grepFilter);
    }

    if (clearOutput)
        ui->plainTextEdit->clear();
    ui->outputBar->setVisible(true);

    QStringList filters = filterText.split(QRegularExpression(QStringLiteral("[;\\s,]+")), Qt::SkipEmptyParts);
    if (filters.isEmpty())
        filters << QStringLiteral("*.md");

    QString outBuf;
    outBuf.reserve(8192);
    outBuf += tr("Grepping '%1' in %2 under '%3' ...\n").arg(searchText, filters.join(QStringLiteral(", ")), dirPath);

    QDirIterator::IteratorFlags flags = QDirIterator::NoIteratorFlags;
    if (subDir)
        flags |= QDirIterator::Subdirectories;

    int matchedFiles = 0;
    int matchedLines = 0;
    QDirIterator it(dirPath, filters, QDir::Files | QDir::NoDotAndDotDot, flags);
    while (it.hasNext()) {
        it.next();
        const QString absPath = QDir::cleanPath(it.fileInfo().absoluteFilePath());
        QString content, errMsg;
        QStringConverter::Encoding enc = QStringConverter::Utf8;
        bool bom = false, crlf = false;
        if (!readTextFileWithMeta(absPath, content, enc, bom, crlf, errMsg))
            continue;
        const QStringList lines = content.split(QChar('\n'));
        bool headerPrinted = false;
        for (int i = 0; i < lines.size(); ++i) {
            const QString &line = lines[i];
            const bool hit = m_searchRegExp ? re.match(line).hasMatch() : line.contains(searchText, ic);
            if (hit) {
                if (!headerPrinted) {
                    headerPrinted = true;
                    ++matchedFiles;
                    outBuf += QStringLiteral("\n\"%1\"\n").arg(absPath);
                }
                outBuf += QStringLiteral("%1: %2\n").arg(i + 1, 4).arg(line);
                ++matchedLines;
            }
        }
    }
    outBuf += tr("\n%1 line(s) matched in %2 file(s).\n").arg(matchedLines).arg(matchedFiles);
    do_output(outBuf);
    statusBar()->showMessage(tr("Grep: %1 line(s) in %2 file(s)").arg(matchedLines).arg(matchedFiles), 5000);
}

bool MainWindow::jumpToOutputLine(int blockNumber)
{
    if (!ui || !ui->plainTextEdit || blockNumber < 0)
        return false;
    QTextDocument *doc = ui->plainTextEdit->document();
    const QTextBlock clicked = doc->findBlockByNumber(blockNumber);
    if (!clicked.isValid())
        return false;
    static const QRegularExpression reHit(QStringLiteral(R"(^\s*(\d+):)"));
    const QRegularExpressionMatch m = reHit.match(clicked.text());
    if (!m.hasMatch())
        return false;
    const int targetLine = m.captured(1).toInt() - 1;
    if (targetLine < 0)
        return false;

    QString filePath;
    for (QTextBlock b = clicked.previous(); b.isValid(); b = b.previous()) {
        const QString t = b.text().trimmed();
        if (t.size() >= 2 && t.startsWith(QChar('"')) && t.endsWith(QChar('"'))) {
            filePath = t.mid(1, t.size() - 2);
            break;
        }
    }
    if (filePath.isEmpty() || !QFileInfo::exists(filePath))
        return false;
    if (!openFile(filePath))
        return false;
    if (TextEdit *edit = currentTextEdit()) {
        const ViewLineMgr &mgr = edit->document()->lineMgr();
        const int clampedLn = qBound(0, targetLine, qMax(0, mgr.lineCount() - 1));
        TextCursor cur(edit->document());
        cur.setPosition(mgr.lineStartPos(clampedLn));
        edit->setTextCursor(cur);
        edit->verticalScrollBar()->setValue(mgr.lineToViewLine(clampedLn));
        if (m_searchCB && !m_searchCB->currentText().isEmpty()) {
            edit->viEngine()->setSearchOptions(m_searchIgnoreCase, m_searchRegExp);
            edit->setSearchHighlight(m_searchCB->currentText(), m_searchIgnoreCase, m_searchRegExp);
        }
        edit->setFocus();
    }
    return true;
}
QString MainWindow::defaultDiaryBaseDir() const
{
    QSettings settings;
    QString dir = settings.value(QStringLiteral("defaultDir")).toString();
    if (dir.isEmpty())
        dir = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    if (dir.isEmpty())
        dir = QDir::homePath();
    return QDir::cleanPath(dir);
}

void MainWindow::onCalendarPageChanged(int year, int month)
{
    auto *cal = dynamic_cast<DiaryCalendarWidget *>(m_calendarWidget);
    if (!cal) return;
    const QString dirPath = QStringLiteral("%1/diary/%2/%3")
        .arg(defaultDiaryBaseDir())
        .arg(year, 4, 10, QChar('0'))
        .arg(month, 2, 10, QChar('0'));
    QDir dir(dirPath);
    if (!dir.exists()) return;

    QTextCharFormat transparentFmt;
    transparentFmt.setBackground(Qt::transparent);

    const QFileInfoList files = dir.entryInfoList({QStringLiteral("*.md")}, QDir::Files);
    for (const QFileInfo &fi : files) {
        const QDate date = QDate::fromString(fi.baseName(), QStringLiteral("yyyyMMdd"));
        if (!date.isValid()) continue;
        QString content, errMsg;
        QStringConverter::Encoding enc = QStringConverter::Utf8;
        bool bom = false, crlf = false;
        if (!readTextFileWithMeta(fi.absoluteFilePath(), content, enc, bom, crlf, errMsg))
            continue;
        int nUndone = 0, nDone = 0;
        for (const QString &line : content.split(QChar('\n'))) {
            const QString t = line.trimmed();
            if (t.startsWith(QStringLiteral("- [ ] "))) ++nUndone;
            else if (t.startsWith(QStringLiteral("- [x] "), Qt::CaseInsensitive)) ++nDone;
        }
        DiaryDayInfo di;
        di.totalTodo = nUndone + nDone;
        di.openTodo = nUndone;
        cal->setDayInfo(date, di);
        cal->setDateTextFormat(date, transparentFmt);
    }
}

void MainWindow::openDiaryDate(const QDate &date)
{
    if (!date.isValid()) return;
    const QString baseDir = defaultDiaryBaseDir();
    const QString dirPath = QStringLiteral("%1/diary/%2/%3")
        .arg(baseDir, date.toString(QStringLiteral("yyyy")), date.toString(QStringLiteral("MM")));
    QDir diaryDir(dirPath);
    if (!diaryDir.exists() && !diaryDir.mkpath(QStringLiteral("."))) {
        statusBar()->showMessage(tr("Failed to create diary directory: %1").arg(dirPath), 5000);
        return;
    }
    const QString filePath = QDir::cleanPath(
        QStringLiteral("%1/%2.md").arg(dirPath, date.toString(QStringLiteral("yyyyMMdd"))));

    if (!QFileInfo::exists(filePath)) {
        const QString templatePath = baseDir + QStringLiteral("/diary/template.md");
        QString content;
        if (QFileInfo::exists(templatePath)) {
            QString errMsg;
            QStringConverter::Encoding enc = QStringConverter::Utf8;
            bool bom = false, crlf = false;
            readTextFileWithMeta(templatePath, content, enc, bom, crlf, errMsg);
        }
        if (content.isEmpty())
            content = QStringLiteral("# {{yyyy-MM-dd}}\n\n## Todo\n- [ ] \n\n## Notes\n- \n\n");
        static const QRegularExpression reDate(QStringLiteral(R"(\{\{(.+?)\}\})"));
        QRegularExpressionMatch m;
        while ((m = reDate.match(content)).hasMatch()) {
            content.replace(m.capturedStart(0), m.capturedLength(0), date.toString(m.captured(1)));
        }
        QSaveFile f(filePath);
        f.setDirectWriteFallback(true);
        if (f.open(QIODevice::WriteOnly)) {
            QTextStream out(&f);
            out.setEncoding(QStringConverter::Utf8);
            out.setGenerateByteOrderMark(false);
#ifdef Q_OS_WIN
            content.replace(QChar('\n'), QStringLiteral("\r\n"));
#endif
            out << content;
            out.flush();
            f.commit();
        }
        onCalendarPageChanged(date.year(), date.month());
    }
    openFile(filePath);
}

void MainWindow::updateDirBar(const QString &dirPath)
{
    if (!m_dirTree) return;
    if (!dirPath.isEmpty())
        m_currentDirBarPath = QDir::cleanPath(dirPath);
    if (m_currentDirBarPath.isEmpty())
        m_currentDirBarPath = QDir::cleanPath(QDir::currentPath());

    QDir dir(m_currentDirBarPath);
    if (m_dirPathLabel) {
        const QString displayName = dir.dirName().isEmpty() ? m_currentDirBarPath : dir.dirName();
        m_dirPathLabel->setText(displayName);
        m_dirPathLabel->setToolTip(m_currentDirBarPath);
    }

    m_dirTree->clear();
    if (!dir.exists()) return;
    QDir::setCurrent(m_currentDirBarPath);

    const QIcon dirIcon = style()->standardIcon(QStyle::SP_DirIcon);
    const QIcon fileIcon = style()->standardIcon(QStyle::SP_FileIcon);

    //	サブディレクトリを先に表示
    const QFileInfoList subDirs = dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name | QDir::IgnoreCase);
    for (const QFileInfo &fi : subDirs) {
        auto *item = new QTreeWidgetItem(m_dirTree);
        item->setIcon(0, dirIcon);
        item->setText(0, fi.fileName());
        item->setData(0, Qt::UserRole, QDir::cleanPath(fi.absoluteFilePath()));
        item->setData(0, Qt::UserRole + 1, true);	//	isDir
        item->setToolTip(0, QDir::cleanPath(fi.absoluteFilePath()));
    }

    //	フィルタに一致するファイルを表示
    const QString filterText = m_dirFilterCB ? m_dirFilterCB->currentText().trimmed() : QStringLiteral("*.md");
    QStringList filters = filterText.split(QRegularExpression(QStringLiteral("[;\\s,]+")), Qt::SkipEmptyParts);
    if (filters.isEmpty())
        filters << QStringLiteral("*.md");
    const QFileInfoList files = dir.entryInfoList(filters, QDir::Files, QDir::Name | QDir::IgnoreCase);
    for (const QFileInfo &fi : files) {
        auto *item = new QTreeWidgetItem(m_dirTree);
        item->setIcon(0, fileIcon);
        item->setText(0, fi.fileName());
        item->setData(0, Qt::UserRole, QDir::cleanPath(fi.absoluteFilePath()));
        item->setData(0, Qt::UserRole + 1, false);	//	isDir
        item->setToolTip(0, QDir::cleanPath(fi.absoluteFilePath()));
    }
}

void MainWindow::onDirItemDoubleClicked(QTreeWidgetItem *item, int column)
{
    Q_UNUSED(column);
    if (!item) return;
    const QString path = item->data(0, Qt::UserRole).toString();
    const bool isDir = item->data(0, Qt::UserRole + 1).toBool();
    if (path.isEmpty()) return;
    if (isDir) {
        updateDirBar(path);
    } else {
        openFile(path);
    }
}

MainWindow::~MainWindow()
{
    if (m_tabWidget)
        m_tabWidget->disconnect(this);
    delete ui;
}

