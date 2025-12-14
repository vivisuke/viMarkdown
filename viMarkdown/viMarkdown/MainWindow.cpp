#include <qsplitter.h>
#include <qplaintextedit>
#include <qtextedit>
#include <QFileDialog>
#include <qsplitter.h>
#include <QMessageBox>
#include <QScrollBar>
#include <QSettings>
#include <QTextBlock>
#include "MainWindow.h"
#include "DocWidget.h"

MainWindow::MainWindow(QWidget *parent)
	: QMainWindow(parent)
	, ui(new Ui::MainWindowClass())
{
	setWindowTitle("viMarkdown ver 0.001");
	ui->setupUi(this);
	updateHTMLModeCheck();

	setup_connections();
	onAction_New();

	QSettings settings;
	QString recentFilePath = settings.value("recentFilePath").toString();
	qDebug() << "recentFilePath = " << recentFilePath;
}

MainWindow::~MainWindow()
{
	delete ui;
}

void MainWindow::setup_connections() {
	connect(ui->action_New, &QAction::triggered, this, &MainWindow::onAction_New);
	connect(ui->action_Open, &QAction::triggered, this, &MainWindow::onAction_Open);
	connect(ui->action_Save, &QAction::triggered, this, &MainWindow::onAction_Save);
	connect(ui->action_Close, &QAction::triggered, this, &MainWindow::onAction_Close);
	connect(ui->action_Bold, &QAction::triggered, this, &MainWindow::onAction_Bold);
	connect(ui->action_Italic, &QAction::triggered, this, &MainWindow::onAction_Italic);
	connect(ui->action_HTML, &QAction::toggled, this, &MainWindow::onAction_HTML);
	connect(ui->action_Source, &QAction::toggled, this, &MainWindow::onAction_Source);
}
void MainWindow::updateHTMLModeCheck() {
	ui->action_HTML->setChecked(m_htmlMode);
	ui->action_Source->setChecked(!m_htmlMode);
}

DocWidget *MainWindow::newTabWidget(const QString& title, const QString& fullPath) {
	//auto containerWidget = new QWidget;
	auto docWidget = new DocWidget(title, fullPath);
	QSplitter *splitter = new QSplitter(Qt::Horizontal, docWidget);
	QPlainTextEdit *mdEditor = new QPlainTextEdit(splitter);
	//QTextEdit *mdEditor = new QTextEdit(splitter);
	mdEditor->setPlaceholderText("ここにMarkdownを入力...");
	QTextEdit *previewer = new QTextEdit(splitter);
	previewer->setReadOnly(true); // プレビューなので読み取り専用にする
	previewer->setPlaceholderText("プレビュー画面");
	splitter->addWidget(mdEditor);
	splitter->addWidget(previewer);
	splitter->setSizes(QList<int>() << 500 << 500);
	QVBoxLayout *layout = new QVBoxLayout(docWidget);
	layout->addWidget(splitter);
	layout->setContentsMargins(0, 0, 0, 0); // 余白をなくして端まで広げる

	connect(mdEditor, &QPlainTextEdit::textChanged, this, &MainWindow::onMDTextChanged);
	//connect(mdEditor, &QTextEdit::textChanged, this, &MainWindow::onMDTextChanged);

	return docWidget;
}

QSplitter *MainWindow::getCurTabSplitter() {
	auto docWidget = ui->tabWidget->currentWidget();
	if( docWidget == nullptr ) return nullptr;
	return docWidget->findChild<QSplitter*>();
}

void MainWindow::onAction_New() {
	qDebug() << "MainWindow::onAction_New()";

	addTab(QString("無題-%1").arg(++m_tab_number));
}
void MainWindow::addTab(const QString &title, const QString fullPath, const QString txt) {
	auto ptr = newTabWidget(title, fullPath);		//	新規タブ生成
	int ix = ui->tabWidget->addTab(ptr, title);		//	新規タブを追加
	ui->tabWidget->setCurrentIndex(ix);				//	新規タブをカレントに

	QSplitter *splitter = getCurTabSplitter();
	if( splitter == nullptr ) return;
	QPlainTextEdit *mdEditor = (QPlainTextEdit*)splitter->widget(0);
	//QTextEdit *mdEditor = (QTextEdit*)splitter->widget(0);
	if( !txt.isEmpty() )
		mdEditor->setPlainText(txt);
	mdEditor->setFocus();

	addTopItemToTreeWidget(title, fullPath);
}
void MainWindow::addTopItemToTreeWidget(const QString &title, const QString fullPath) {
	QTreeWidgetItem *item = new QTreeWidgetItem();
	item->setText(0, title);
	item->setData(0, Qt::UserRole, fullPath);
	ui->treeWidget->addTopLevelItem(item);
	QTreeWidgetItem *item2 = new QTreeWidgetItem();
	item2->setText(0, "見出し");
	item->addChild(item2);
}
void MainWindow::onAction_Open() {
	QString fullPath = QFileDialog::getOpenFileName(
		this,
		"open File",			// ダイアログのタイトル
		QDir::currentPath(),		// 初期ディレクトリ
		"markdown file (*.md)"	// フィルター
	);

	if (!fullPath.isEmpty()) {
		qDebug() << "path = " << fullPath;
		m_opening_file = true;
		QFile file(fullPath);
		if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
			QMessageBox::warning(this, tr("エラー"), tr("ファイルが開けません:\n%1").arg(fullPath));
			return;
		}

		QString content = file.readAll();
		QFileInfo fileInfo(fullPath);
		addTab(fileInfo.fileName(), fullPath, content);
		QDir::setCurrent(fileInfo.path());
		m_opening_file = false;

		QSettings settings;
		settings.setValue("recentFilePath", fullPath);
	}
}
void MainWindow::onAction_Save() {
	int ix = ui->tabWidget->currentIndex();
	if( ix < 0 ) return;
	DocWidget *doc = (DocWidget*)ui->tabWidget->widget(ix);
	QString fullPath = doc->m_fullPath;
	if( fullPath.isEmpty() )
		fullPath = QFileDialog::getSaveFileName(
						this,				   // 親ウィジェット（メインウィンドウがあれば this）
						"Save File",		 // ダイアログのタイトル
						QDir::currentPath(),		  // 初期ディレクトリ
						"markdown (*.md)"  // ファイルフィルタ
					);
	if( fullPath.isEmpty() ) return;
	QFile file(fullPath);
	if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
		QTextStream out(&file);
		QSplitter *splitter = getCurTabSplitter();
		if( splitter == nullptr ) return;
		QPlainTextEdit *mdEditor = (QPlainTextEdit*)splitter->widget(0);
		out << mdEditor->toPlainText();
		file.close();
		//QMessageBox::information(nullptr, "成功", "ファイルが保存されました:\n" + fullPath);
		doc->m_modified = false;		//	保存済み
		ui->tabWidget->setTabText(ix, doc->m_title);
	} else {
		//QMessageBox::warning(nullptr, "エラー", "ファイルを開けませんでした。");
	}
}
void MainWindow::onAction_Close() {
	qDebug() << "MainWindow::onAction_Close()";

	int ix = ui->tabWidget->currentIndex();
	if( ix >= 0 )
		ui->tabWidget->removeTab(ix);
}
void MainWindow::insertInline(const QString& delimiter) {
	QSplitter *splitter = getCurTabSplitter();
	if( splitter == nullptr ) return;
	QPlainTextEdit *textEdit = (QPlainTextEdit*)splitter->widget(0);

	QTextCursor cursor = textEdit->textCursor();
	if (cursor.hasSelection()) {
		// 2. 複数行にまたがっているかチェック
	    QTextDocument *doc = textEdit->document();
	    // 選択範囲の「開始位置」と「終了位置」が属するブロック（行）を取得
	    QTextBlock startBlock = doc->findBlock(cursor.selectionStart());
	    QTextBlock endBlock   = doc->findBlock(cursor.selectionEnd());
	    // ブロック番号が異なる場合＝複数行選択されている場合は無視
	    if (startBlock.blockNumber() != endBlock.blockNumber())
	        return;
		QString newText = delimiter + cursor.selectedText() + delimiter;
		cursor.insertText(newText);
	} else {
		cursor.insertText(delimiter + delimiter);
		cursor.movePosition(QTextCursor::Left, QTextCursor::MoveAnchor, delimiter.length());
	}
	textEdit->setTextCursor(cursor);
}
void MainWindow::onAction_Bold() {
	insertInline("**");
}
void MainWindow::onAction_Italic() {
	insertInline("*");
}
void MainWindow::onAction_HTML(bool checked) {
	//if( m_htmlMode ) return;
	m_htmlMode = checked;
	updateHTMLModeCheck();
	updatePreview();
}
void MainWindow::onAction_Source(bool checked) {
	m_htmlMode = !checked;
	updateHTMLModeCheck();
	updatePreview();
}
void MainWindow::updatePreview() {
	QSplitter *splitter = getCurTabSplitter();
	if( splitter == nullptr ) return;
	QTextEdit *textEdit = (QTextEdit*)splitter->widget(1);
	QScrollBar *vScrollBar = textEdit->verticalScrollBar();
	int currentPos = vScrollBar->value();
	if( m_htmlMode )
		textEdit->setHtml(m_htmlText);
	else
		textEdit->setPlainText(m_htmlText);
	vScrollBar->setValue(currentPos);
}
void MainWindow::onMDTextChanged() {
	//qDebug() << "MainWindow::onMDTextChanged()";

	QPlainTextEdit *mdEditor = (QPlainTextEdit *)sender();
	//QTextEdit *mdEditor = (QTextEdit *)sender();
	m_plainText = mdEditor->toPlainText();
	m_htmlComvertor.setMarkdownText(m_plainText);
	m_htmlText = m_htmlComvertor.convert();
	updatePreview();
	if( !m_opening_file ) {
		DocWidget *docWidget = (DocWidget*)ui->tabWidget->currentWidget();
		if( docWidget == nullptr ) return;
		docWidget->m_modified = true;
		ui->tabWidget->setTabText(ui->tabWidget->currentIndex(), docWidget->m_title + " *");
	}
}

