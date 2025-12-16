#include <vector>
#include <qsplitter.h>
#include <qplaintextedit>
#include <qtextedit>
#include <QFileDialog>
#include <qsplitter.h>
#include <QMessageBox>
#include <QScrollBar>
#include <QSettings>
#include <QTextBlock>
#include <QDockWidget>
#include "MainWindow.h"
#include "DocWidget.h"

using namespace std;

MainWindow::MainWindow(QWidget *parent)
	: QMainWindow(parent)
	, ui(new Ui::MainWindowClass())
{
	setWindowTitle("viMarkdown ver 0.001");
	ui->setupUi(this);
	updateHTMLModeCheck();
	ui->action_OutlineBar->setChecked(true);	//	暫定的

	setup_connections();
	onAction_New();

	//QSettings settings;
	//QString recentFilePath = settings.value("recentFilePath").toString();
	//qDebug() << "recentFilePath = " << recentFilePath;
}

MainWindow::~MainWindow()
{
	delete ui;
}

void MainWindow::setup_connections() {
	connect(ui->menu_RecentFiles, &QMenu::aboutToShow, this, &MainWindow::onAboutToShow_RecentFiles);
	connect(ui->action_New, &QAction::triggered, this, &MainWindow::onAction_New);
	connect(ui->action_Open, &QAction::triggered, this, &MainWindow::onAction_Open);
	connect(ui->action_Save, &QAction::triggered, this, &MainWindow::onAction_Save);
	connect(ui->action_Close, &QAction::triggered, this, &MainWindow::onAction_Close);
	connect(ui->action_List, &QAction::triggered, this, &MainWindow::onAction_List);
	connect(ui->action_NumList, &QAction::triggered, this, &MainWindow::onAction_NumList);
	connect(ui->action_Bold, &QAction::triggered, this, &MainWindow::onAction_Bold);
	connect(ui->action_Italic, &QAction::triggered, this, &MainWindow::onAction_Italic);
	connect(ui->action_HTML, &QAction::toggled, this, &MainWindow::onAction_HTML);
	connect(ui->action_Source, &QAction::toggled, this, &MainWindow::onAction_Source);
	connect(ui->action_OutlineBar, &QAction::toggled, this, &MainWindow::onAction_OutlineBar);
	connect(ui->outlineBar, &QDockWidget::visibilityChanged, this, &MainWindow::onOutlineBarVisibilityChanged);
}
void MainWindow::updateHTMLModeCheck() {
	ui->action_HTML->setChecked(m_htmlMode);
	ui->action_Source->setChecked(!m_htmlMode);
}

DocWidget *MainWindow::newTabWidget(const QString& title, const QString& fullPath) {
	//auto containerWidget = new QWidget;
	auto *docWidget = new DocWidget(title, fullPath);
	QSplitter *splitter = new QSplitter(Qt::Horizontal, docWidget);
	QPlainTextEdit *mdEditor = docWidget->m_mdEditor = new QPlainTextEdit(splitter);
	//QTextEdit *mdEditor = new QTextEdit(splitter);
	mdEditor->setPlaceholderText("ここにMarkdownを入力\n# タイトル\n## 大見出し\n- リスト\n1. 連番\n本文...");
	QTextEdit *previewer = docWidget->m_previewer = new QTextEdit(splitter);
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
DocWidget *MainWindow::getCurDocWidget() {
	return (DocWidget*)ui->tabWidget->currentWidget();
}

void MainWindow::onAboutToShow_RecentFiles() {
	qDebug() << "MainWindow::onAboutToShow_RecentFiles()";

	ui->menu_RecentFiles->clear();
	QSettings settings;
	QStringList recentFilePaths = settings.value("recentFilePaths").toStringList();
	//ui->menu_RecentFiles->addAction(recentFilePath);
	for(int i = 0; i < recentFilePaths.size(); ++i) {
		ui->menu_RecentFiles->addAction(recentFilePaths[i]);
	}
}

void MainWindow::onAction_New() {
	qDebug() << "MainWindow::onAction_New()";

	addTab(QString("無題-%1").arg(++m_tab_number));
}
void MainWindow::addTab(const QString &title, const QString fullPath, const QString txt) {
	auto ptr = newTabWidget(title, fullPath);		//	新規タブ生成
	int ix = ui->tabWidget->addTab(ptr, title);		//	新規タブを追加
	ui->tabWidget->setCurrentIndex(ix);				//	新規タブをカレントに

	//QSplitter *splitter = getCurTabSplitter();
	//if( splitter == nullptr ) return;
	//QPlainTextEdit *mdEditor = (QPlainTextEdit*)splitter->widget(0);
	QPlainTextEdit *mdEditor = getCurDocWidget()->m_mdEditor;
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
		updateOutlineTree();
		QDir::setCurrent(fileInfo.path());
		m_opening_file = false;

		QSettings settings;
		QStringList recentFilePaths = settings.value("recentFilePaths").toStringList();
		int ix;
		while( (ix = recentFilePaths.indexOf(fullPath)) >= 0 )
			recentFilePaths.removeAt(ix);
		recentFilePaths.push_front(fullPath);
		while( recentFilePaths.size() > 10 ) recentFilePaths.pop_back();
		settings.setValue("recentFilePaths", recentFilePaths);
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
		//QSplitter *splitter = getCurTabSplitter();
		//if( splitter == nullptr ) return;
		//QPlainTextEdit *mdEditor = (QPlainTextEdit*)splitter->widget(0);
		QPlainTextEdit *mdEditor = getCurDocWidget()->m_mdEditor;
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
int isListBlock(const QTextBlock& block) {		//	空白+ "- " で始まるか？ return 0 for not List, 1以上 for 文字数
	const QString txt = block.text();
	int i = 0;
	while( i < txt.size() && txt[i] == ' ' ) ++i;
	if( !txt.mid(i).startsWith("- ") ) return 0;
	return i + 2;
}
void MainWindow::onAction_List() {
	qDebug() << "MainWindow::onAction_List()";
	QPlainTextEdit *mdEditor = getCurDocWidget()->m_mdEditor;
	QTextCursor cursor = mdEditor->textCursor();
    QTextDocument *doc = mdEditor->document();
	//if (cursor.hasSelection()) {
		cursor.beginEditBlock();
		int startPos = cursor.selectionStart();
		int endPos = cursor.selectionEnd();
		// 範囲に含まれる最初のブロックと最後のブロックを取得
		QTextBlock currentBlock = doc->findBlock(startPos);
		QTextBlock endBlock = doc->findBlock(endPos);
		//bool remove_list = currentBlock.text().startsWith("- ");
		bool remove_list = isListBlock(currentBlock) != 0;
		if (endPos > startPos && endPos == endBlock.position())
			endBlock = endBlock.previous();		//	最終ブロック修正
		while (currentBlock.isValid() && currentBlock.blockNumber() <= endBlock.blockNumber()) {
		    cursor.setPosition(currentBlock.position());	//	行頭位置
	    	//cursor.movePosition(QTextCursor::StartOfBlock);			//	行頭移動　←　何故かうまく動作しない？？？
		    if( remove_list ) {
		    	int n = isListBlock(currentBlock);
		    	if( n != 0 ) {
		    		cursor.movePosition(QTextCursor::NextCharacter, QTextCursor::KeepAnchor, n);
		    		cursor.removeSelectedText();
		    	}
		    } else {
		    	if( !isListBlock(currentBlock) )
				    cursor.insertText("- ");
		    }
		    currentBlock = currentBlock.next();	    // 次のブロックへ
		}
		cursor.endEditBlock();
#if 0
	} else {
    	cursor.movePosition(QTextCursor::StartOfBlock);			//	行頭移動
	    QTextBlock block = doc->findBlock(cursor.position());
	    const QString txt = block.text();
	    int n = isListBlock(block);
	    if( n != 0 ) {
    		cursor.movePosition(QTextCursor::NextCharacter, QTextCursor::KeepAnchor, n);
    		cursor.removeSelectedText();
	    } else {
	    	cursor.insertText("- ");
	    }
	}
#endif
	mdEditor->setTextCursor(cursor);
}
void MainWindow::onAction_NumList() {
	qDebug() << "MainWindow::onAction_NumList()";
}
void MainWindow::insertInline(const QString& delimiter) {
	QPlainTextEdit *mdEditor = getCurDocWidget()->m_mdEditor;
	QTextCursor cursor = mdEditor->textCursor();
	if (cursor.hasSelection()) {
		// 2. 複数行にまたがっているかチェック
	    QTextDocument *doc = mdEditor->document();
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
	mdEditor->setTextCursor(cursor);
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
void MainWindow::onAction_OutlineBar(bool checked) {
	ui->outlineBar->setVisible(checked);
}
void MainWindow::onOutlineBarVisibilityChanged(bool v) {
	ui->action_OutlineBar->setChecked(v);
}
void MainWindow::updatePreview() {
	QTextEdit* textEdit = getCurDocWidget()->m_previewer;
	QScrollBar *vScrollBar = textEdit->verticalScrollBar();
	int currentPos = vScrollBar->value();
	if( m_htmlMode )
		textEdit->setHtml(m_htmlText);
	else
		textEdit->setPlainText(m_htmlText);
	vScrollBar->setValue(currentPos);
}
QTreeWidgetItem* MainWindow::findTopLevelItemByFullPath(const QString& title, const QString fullPath) {
	QTreeWidget *treeWidget = ui->treeWidget;
	int topCount = treeWidget->topLevelItemCount();
    for (int i = 0; i < topCount; ++i) {
        QTreeWidgetItem* item = treeWidget->topLevelItem(i);
        QString itemPath = item->data(0, Qt::UserRole).toString();
        if( itemPath.isEmpty() ) {
        	if( item->text(0) == title )
        		return item;
        } else if( itemPath == fullPath )
        	return item;
    }
    return nullptr;
}
void expandAllChildren(QTreeWidgetItem *item) {
    if (!item) return;
    item->setExpanded(true);  // まず自身を展開
    for (int i = 0; i < item->childCount(); ++i) {
        QTreeWidgetItem *child = item->child(i);
        expandAllChildren(child);  // 子に対して再帰呼び出し
    }
}
void MainWindow::updateOutlineTree() {
	DocWidget *docWidget = getCurDocWidget();
	QTreeWidgetItem* item0 = findTopLevelItemByFullPath(docWidget->m_title, docWidget->m_fullPath);
	if( item0 == nullptr ) return;
	qDeleteAll(item0->takeChildren());
	vector<QTreeWidgetItem*> parents(10, nullptr);
	parents[0] = item0;
	const QStringList &lst = m_htmlComvertor.getHeadings();
	for(int i = 0; i != lst.size(); ++i) {
		QTreeWidgetItem *item2 = new QTreeWidgetItem();
		//bool ok;
		//int val = lst[i].toInt(&ok, 10);
		int val = lst[i][0].unicode() - '0';
		item2->setText(0, lst[i].mid(2));
		int k = val - 1;
		while( parents[k] == nullptr ) --k;
		parents[k]->addChild(item2);
		parents[val] = item2;
		while( ++val < 10 ) parents[val] = nullptr;
	}
	//item0->setExpanded(true);
	expandAllChildren(item0);
}
void MainWindow::onMDTextChanged() {
	//qDebug() << "MainWindow::onMDTextChanged()";

	QPlainTextEdit *mdEditor = getCurDocWidget()->m_mdEditor;
	m_plainText = mdEditor->toPlainText();
	m_htmlComvertor.setMarkdownText(m_plainText);
	m_htmlText = m_htmlComvertor.convert();
	updatePreview();
	updateOutlineTree();
	if( !m_opening_file ) {
		DocWidget *docWidget = getCurDocWidget();
		if( docWidget == nullptr ) return;
		docWidget->m_modified = true;
		ui->tabWidget->setTabText(ui->tabWidget->currentIndex(), docWidget->m_title + " *");
	}
}

