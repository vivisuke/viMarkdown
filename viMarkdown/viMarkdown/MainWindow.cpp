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
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QUrl>
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
	setWindowTitle("viMarkdown ver 0.0.001 Dev"); 

	setAcceptDrops(true);		//	ファイルドロップ可
	setup_connections();
	onAction_New();
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
	connect(ui->action_Indent, &QAction::triggered, this, &MainWindow::onAction_Indent);
	connect(ui->action_UnIndent, &QAction::triggered, this, &MainWindow::onAction_UnIndent);
	connect(ui->action_Bold, &QAction::triggered, this, &MainWindow::onAction_Bold);
	connect(ui->action_Italic, &QAction::triggered, this, &MainWindow::onAction_Italic);
	connect(ui->action_Strikethrough, &QAction::triggered, this, &MainWindow::onAction_Strikethrough);
	connect(ui->action_HTML, &QAction::toggled, this, &MainWindow::onAction_HTML);
	connect(ui->action_Source, &QAction::toggled, this, &MainWindow::onAction_Source);
	connect(ui->action_OutlineBar, &QAction::toggled, this, &MainWindow::onAction_OutlineBar);
	connect(ui->outlineBar, &QDockWidget::visibilityChanged, this, &MainWindow::onOutlineBarVisibilityChanged);
	connect(ui->treeWidget, &QTreeWidget::currentItemChanged, this, &MainWindow::onTreeSelectionChanged);
	connect(ui->action_AboutViMarkdown, &QAction::triggered, this, &MainWindow::onAction_About);
}
void MainWindow::closeEvent(QCloseEvent *event) {
	for(int ix = 0; ix < ui->tabWidget->count(); ++ix) {
		DocWidget *docWidget = (DocWidget*)ui->tabWidget->widget(ix);
		if( docWidget->m_modified ) {
			ui->tabWidget->setCurrentIndex(ix);
			QMessageBox::StandardButton reply = QMessageBox::question(this,
                                  "Confirm save",                // タイトル
                                  "The document has been modified.\nDo you want to save your changes?", // 本文
                                  QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel); // ボタンの種類

		    if (reply == QMessageBox::Yes) {
				onAction_Save();
		    } else if (reply == QMessageBox::Cancel) {
				event->ignore();
			    return;
		    }
		}
	}
	event->accept();
}
void MainWindow::updateHTMLModeCheck() {
	ui->action_HTML->setChecked(m_htmlMode);
	ui->action_Source->setChecked(!m_htmlMode);
}

DocWidget *MainWindow::newTabWidget(const QString& title, const QString& fullPath) {
	//auto containerWidget = new QWidget;
	auto *docWidget = new DocWidget(title, fullPath);
	docWidget->setStyleSheet("font-size: 12pt; line-height: 200%;");
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
void MainWindow::dragEnterEvent(QDragEnterEvent *event)
{
    // ドラッグされているデータがファイル（URL）を含んでいるかチェック
    if (event->mimeData()->hasUrls()) {
        // 受け入れる（カーソルが「コピー」や「リンク」の形に変わる）
        event->acceptProposedAction();
    }
}
void MainWindow::dropEvent(QDropEvent *event)
{
    const QList<QUrl> urls = event->mimeData()->urls();    // ドロップされたデータのURLリストを取得
    //if (urls.isEmpty()) return;
    for(const QUrl &url : urls ) {
    	QString fullPath = url.toLocalFile();
    	qDebug() << "dropped fullPath = " << fullPath;
    	do_open(fullPath);
    }
}
#if 0
QSplitter *MainWindow::getCurTabSplitter() {
	auto docWidget = ui->tabWidget->currentWidget();
	if( docWidget == nullptr ) return nullptr;
	return docWidget->findChild<QSplitter*>();
}
#endif
DocWidget *MainWindow::getCurDocWidget() {
	//int ix = ui->tabWidget->currentIndex();
	//auto ptr = (DocWidget*)ui->tabWidget->widget(ix);
	return (DocWidget*)ui->tabWidget->currentWidget();
}

void MainWindow::onAboutToShow_RecentFiles() {
	qDebug() << "MainWindow::onAboutToShow_RecentFiles()";

	ui->menu_RecentFiles->clear();
	QSettings settings;
	QStringList recentFilePaths = settings.value("recentFilePaths").toStringList();
	int k = 0;
	for(const QString &fullPath : recentFilePaths) {
		auto key = QString::number(k = (k+1) % 10);
		QAction *act = ui->menu_RecentFiles->addAction("&" + key + " " + fullPath);
		connect(act, &QAction::triggered, this, [this, fullPath]() {
			QString pathArg = fullPath;
	        do_open(pathArg); 
		});
	}
}

void MainWindow::onAction_New() {
	qDebug() << "MainWindow::onAction_New()";

	addTab(QString("無題-%1").arg(++m_tab_number));
}
void MainWindow::addTab(const QString &title, const QString fullPath, const QString txt) {
	auto docWidget = newTabWidget(title, fullPath);		//	新規タブ生成
	int ix = ui->tabWidget->addTab(docWidget, title);		//	新規タブを追加
	ui->tabWidget->setCurrentIndex(ix);				//	新規タブをカレントに

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
		do_open(fullPath);
	}
}
int MainWindow::tabIndexOf(const QString& title, const QString& fullPath) {
	for(int ix = 0; ix < ui->tabWidget->count(); ++ix) {
		DocWidget *docWidget = (DocWidget*)ui->tabWidget->widget(ix);
		if( fullPath.isEmpty() ) {
			if( docWidget->m_title == title )
				return ix;
		} else {
			if( docWidget->m_fullPath == fullPath )
				return ix;
		}
	}
	return -1;
}
void MainWindow::addToRecentFiles(const QString& fullPath) {
	QSettings settings;
	QStringList recentFilePaths = settings.value("recentFilePaths").toStringList();
	int ix;
	while( (ix = recentFilePaths.indexOf(fullPath)) >= 0 )
		recentFilePaths.removeAt(ix);
	recentFilePaths.push_front(fullPath);
	while( recentFilePaths.size() > 10 ) recentFilePaths.pop_back();
	settings.setValue("recentFilePaths", recentFilePaths);
}
void MainWindow::do_open(const QString& fullPath) {
	int tix = tabIndexOf("", fullPath);
	if( tix >= 0 ) {		//	すでにオープン済み
		ui->tabWidget->setCurrentIndex(tix);
		return;
	}
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

	addToRecentFiles(fullPath);
}
void MainWindow::onAction_Save() {
	int ix = ui->tabWidget->currentIndex();
	if( ix < 0 ) return;
	DocWidget *docWidget = (DocWidget*)ui->tabWidget->widget(ix);
	QString fullPath = docWidget->m_fullPath;
	if( fullPath.isEmpty() ) {		//	フルパスを持っていない場合
		QString oldTitle = docWidget->m_title;
		fullPath = QFileDialog::getSaveFileName(
						this,				   // 親ウィジェット（メインウィンドウがあれば this）
						"Save File",		 // ダイアログのタイトル
						QDir::currentPath(),		  // 初期ディレクトリ
						"markdown (*.md)"  // ファイルフィルタ
					);
		if( fullPath.isEmpty() ) return;
		docWidget->m_fullPath = fullPath;
		QFileInfo fileInfo(fullPath);
		docWidget->m_title = fileInfo.fileName();
		ui->tabWidget->setTabText(ix, docWidget->m_title);
		QTreeWidgetItem *top = findTopLevelItemByFullPath(oldTitle, "");
		top->setText(0, docWidget->m_title);
	}
	addToRecentFiles(fullPath);
	QFile file(fullPath);
	if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
		QTextStream out(&file);
		QPlainTextEdit *mdEditor = getCurDocWidget()->m_mdEditor;
		out << mdEditor->toPlainText();
		file.close();
		//QMessageBox::information(nullptr, "成功", "ファイルが保存されました:\n" + fullPath);
		docWidget->m_modified = false;		//	保存済み
		ui->tabWidget->setTabText(ix, docWidget->m_title);
	} else {
		//QMessageBox::warning(nullptr, "エラー", "ファイルを開けませんでした。");
	}
}
void MainWindow::onAction_Close() {
	qDebug() << "MainWindow::onAction_Close()";

	DocWidget *docWidget = getCurDocWidget();
	if (docWidget == nullptr) return;
	if( docWidget->m_modified ) {
		QMessageBox::StandardButton reply = QMessageBox::question(this,
                                  "Confirm save",                // タイトル
                                  "The document has been modified.\nDo you want to save your changes?", // 本文
                                  QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel); // ボタンの種類

	    if (reply == QMessageBox::Yes) {
			onAction_Save();
	    } else if (reply == QMessageBox::Cancel) {
		    return;
	    }
	}
	int ix = ui->tabWidget->currentIndex();
	if (ix >= 0)
		ui->tabWidget->removeTab(ix);
	QTreeWidgetItem *top = findTopLevelItemByFullPath(docWidget->m_title, docWidget->m_fullPath);
	if( top != nullptr ) {
		//ui->treeWidget->removeItemWidget(top);
		delete top;			//	TreeWidget から top アイテム以下をすべて削除
	}
}
int isListBlock(const QTextBlock& block) {		//	空白+ "- " で始まるか？ return 0 for not List, 1以上 for 文字数
	const QString txt = block.text();
	int i = 0;
	while( i < txt.size() && txt[i] == ' ' ) ++i;
	if( !txt.mid(i).startsWith("- ") ) return 0;
	return i + 2;
}
int isNumListBlock(const QTextBlock& block) {		//	空白+ "数字. " で始まるか？ return 0 for not NumList, 1以上 for 文字数
	const QString txt = block.text();
	int i = 0;
	while( i < txt.size() && txt[i] == ' ' ) ++i;
	if( i >= txt.size() || !txt[i].isNumber() ) return 0;
	if( !txt.mid(i+1).startsWith(". ") ) return 0;
	return i + 3;
}
void MainWindow::onAction_Indent() {
	QPlainTextEdit *mdEditor = getCurDocWidget()->m_mdEditor;
	QTextCursor cursor = mdEditor->textCursor();
    QTextDocument *doc = mdEditor->document();
	cursor.beginEditBlock();
	int startPos = cursor.selectionStart();
	int endPos = cursor.selectionEnd();
	// 範囲に含まれる最初のブロックと最後のブロックを取得
	QTextBlock currentBlock = doc->findBlock(startPos);
	QTextBlock endBlock = doc->findBlock(endPos);
	if (endPos > startPos && endPos == endBlock.position())
		endBlock = endBlock.previous();		//	最終ブロック修正
	while (currentBlock.isValid() && currentBlock.blockNumber() <= endBlock.blockNumber()) {
	    cursor.setPosition(currentBlock.position());	//	行頭位置
	    cursor.insertText("  ");
	    currentBlock = currentBlock.next();	    // 次のブロックへ
	}
	cursor.endEditBlock();
	mdEditor->setTextCursor(cursor);
}
void MainWindow::onAction_UnIndent() {
	QPlainTextEdit *mdEditor = getCurDocWidget()->m_mdEditor;
	QTextCursor cursor = mdEditor->textCursor();
    QTextDocument *doc = mdEditor->document();
	cursor.beginEditBlock();
	int startPos = cursor.selectionStart();
	int endPos = cursor.selectionEnd();
	// 範囲に含まれる最初のブロックと最後のブロックを取得
	QTextBlock currentBlock = doc->findBlock(startPos);
	QTextBlock endBlock = doc->findBlock(endPos);
	if (endPos > startPos && endPos == endBlock.position())
		endBlock = endBlock.previous();		//	最終ブロック修正
	while (currentBlock.isValid() && currentBlock.blockNumber() <= endBlock.blockNumber()) {
	    cursor.setPosition(currentBlock.position());	//	行頭位置
	    if( currentBlock.text().startsWith("  ") ) {
    		cursor.movePosition(QTextCursor::NextCharacter, QTextCursor::KeepAnchor, 2);
    		cursor.removeSelectedText();
	    }
	    currentBlock = currentBlock.next();	    // 次のブロックへ
	}
	cursor.endEditBlock();
	mdEditor->setTextCursor(cursor);
}
void MainWindow::onAction_List() {
	qDebug() << "MainWindow::onAction_List()";
	QPlainTextEdit *mdEditor = getCurDocWidget()->m_mdEditor;
	QTextCursor cursor = mdEditor->textCursor();
    QTextDocument *doc = mdEditor->document();
	cursor.beginEditBlock();
	int startPos = cursor.selectionStart();
	int endPos = cursor.selectionEnd();
	QTextBlock startBlock = doc->findBlock(startPos);		// 範囲に含まれる最初のブロックと最後のブロックを取得
	QTextBlock currentBlock = startBlock;
	QTextBlock endBlock = doc->findBlock(endPos);
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
	    	if( !isListBlock(currentBlock) ) {
			    int n = isNumListBlock(currentBlock);	//	空白+ "数字. "
			    if( n > 0 ) {
			    	cursor.setPosition(currentBlock.position() + n - 3);		//	3 for "数字. ".length()
			    	cursor.movePosition(QTextCursor::NextCharacter, QTextCursor::KeepAnchor, 3);
			    }
			    cursor.insertText("- ");
	    	}
	    }
	    currentBlock = currentBlock.next();	    // 次のブロックへ
	}
	cursor.setPosition(startBlock.position());	//	行頭位置
	cursor.setPosition(endBlock.next().position(), QTextCursor::KeepAnchor);	//	行頭位置
	cursor.endEditBlock();
	mdEditor->setTextCursor(cursor);
}
void MainWindow::onAction_NumList() {
	qDebug() << "MainWindow::onAction_NumList()";
	QPlainTextEdit *mdEditor = getCurDocWidget()->m_mdEditor;
	QTextCursor cursor = mdEditor->textCursor();
    QTextDocument *doc = mdEditor->document();
	cursor.beginEditBlock();
	int startPos = cursor.selectionStart();
	int endPos = cursor.selectionEnd();
	QTextBlock startBlock = doc->findBlock(startPos);		// 範囲に含まれる最初のブロックと最後のブロックを取得
	QTextBlock currentBlock = startBlock;
	QTextBlock endBlock = doc->findBlock(endPos);
	bool remove_list = isNumListBlock(currentBlock) != 0;
	if (endPos > startPos && endPos == endBlock.position())
		endBlock = endBlock.previous();		//	最終ブロック修正
	while (currentBlock.isValid() && currentBlock.blockNumber() <= endBlock.blockNumber()) {
	    cursor.setPosition(currentBlock.position());	//	行頭位置
    	//cursor.movePosition(QTextCursor::StartOfBlock);			//	行頭移動　←　何故かうまく動作しない？？？
	    if( remove_list ) {
	    	int n = isNumListBlock(currentBlock);
	    	if( n != 0 ) {
	    		cursor.movePosition(QTextCursor::NextCharacter, QTextCursor::KeepAnchor, n);
	    		cursor.removeSelectedText();
	    	}
	    } else {
	    	if( !isNumListBlock(currentBlock) ) {
			    int n = isListBlock(currentBlock);	//	空白+ "- "
			    if( n > 0 ) {
			    	cursor.setPosition(currentBlock.position() + n - 2);		//	2 for "- ".length()
			    	cursor.movePosition(QTextCursor::NextCharacter, QTextCursor::KeepAnchor, 2);
			    }
			    cursor.insertText("1. ");
	    	}
	    }
	    currentBlock = currentBlock.next();	    // 次のブロックへ
	}
	cursor.setPosition(startBlock.position());	//	行頭位置
	cursor.setPosition(endBlock.next().position(), QTextCursor::KeepAnchor);	//	行頭位置
	cursor.endEditBlock();
	mdEditor->setTextCursor(cursor);
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
void MainWindow::onAction_Strikethrough() {
	insertInline("~~");
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
void MainWindow::onTreeSelectionChanged(QTreeWidgetItem *current, QTreeWidgetItem *previous) {
	if( current == nullptr ) return;
	auto top = current;
	while( top->parent() != nullptr ) top = top->parent();
	QString fullPath = top->data(0, Qt::UserRole).toString();
	//if( !fullPath.isEmpty() ) {
		int tix = tabIndexOf(current->text(0), fullPath);
		if( tix >= 0 )
			ui->tabWidget->setCurrentIndex(tix);
	//}
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
	if( docWidget == nullptr ) return;
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

	if( m_ignore_changed ) return;
	m_ignore_changed = true;
	QPlainTextEdit *mdEditor = getCurDocWidget()->m_mdEditor;
	m_plainText = mdEditor->toPlainText();
	m_htmlComvertor.setMarkdownText(m_plainText);
	m_htmlText = m_htmlComvertor.convert();
	const vector<char>& blockType = m_htmlComvertor.getBlockType();
	QTextCursor cursor(mdEditor->document()); 
	QTextCharFormat fmt_darkred, fmt_black;
    fmt_darkred.setForeground(QColor("darkred"));
    fmt_black.setForeground(QColor("black"));
	QTextBlock block = mdEditor->document()->firstBlock();
	for(int ln = 0; block.isValid() && ln < blockType.size(); ++ln) {
		cursor.setPosition(block.position());
		cursor.select(QTextCursor::BlockUnderCursor);
		if( blockType[ln] == '#' ) {
			cursor.mergeCharFormat(fmt_darkred);
		} else {
			cursor.mergeCharFormat(fmt_black);
		}
		block = block.next();
	}
	updatePreview();
	updateOutlineTree();
	if( !m_opening_file ) {
		DocWidget *docWidget = getCurDocWidget();
		if( docWidget == nullptr ) return;
		docWidget->m_modified = true;
		ui->tabWidget->setTabText(ui->tabWidget->currentIndex(), docWidget->m_title + " *");
	}
	m_ignore_changed = false;
}
void MainWindow::onAction_About() {
	qDebug() << "MainWindow::onAction_About()";

	QMessageBox::about(this, 
        "About viMarkdown", // タイトルバー
        
        "<p><b>viMarkdown</b> version 0.0.001 Dev</p>"
        "<p>The efficient Markdown editor.</p>"
        "<p>Copyright (C) 2025 by N.Tsuda</p>"
        "<p>Powered by Qt 6 and C++.</p>"
    );
}
