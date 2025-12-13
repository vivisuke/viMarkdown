#include <qsplitter.h>
#include <qplaintextedit>
#include <qtextedit>
#include <QFileDialog>
#include <qsplitter.h>
#include <QMessageBox>
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

	connect(mdEditor, &QPlainTextEdit::textChanged, this, &MainWindow::onPlainTextChanged);

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
	auto ptr = newTabWidget(title, fullPath);
	int ix = ui->tabWidget->addTab(ptr, title);
	ui->tabWidget->setCurrentIndex(ix);

	QSplitter *splitter = getCurTabSplitter();
	if( splitter == nullptr ) return;
	QPlainTextEdit *mdEditor = (QPlainTextEdit*)splitter->widget(0);
	if( !txt.isEmpty() )
		mdEditor->setPlainText(txt);
	mdEditor->setFocus();

}
void MainWindow::onAction_Open() {
	QString fullPath = QFileDialog::getOpenFileName(
		this,
		"open File",			// ダイアログのタイトル
		QDir::homePath(),		// 初期ディレクトリ（ホームフォルダ）
		"markdown file (*.md)"	// フィルター
	);

	if (!fullPath.isEmpty()) {
		qDebug() << "path = " << fullPath;
		QFile file(fullPath);
		if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
			QMessageBox::warning(this, tr("エラー"), tr("ファイルが開けません:\n%1").arg(fullPath));
			return;
		}

		QString content = file.readAll();
		QFileInfo fileInfo(fullPath);
		addTab(fileInfo.fileName(), fullPath, content);
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
						QDir::homePath(),		  // 初期ディレクトリ（ホームディレクトリ）
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
	QTextEdit *previewer = (QTextEdit*)splitter->widget(1);
	if( m_htmlMode )
		previewer->setHtml(m_htmlText);
	else
		previewer->setPlainText(m_htmlText);
}
void MainWindow::onPlainTextChanged() {
	//qDebug() << "MainWindow::onPlainTextChanged()";

	QPlainTextEdit *mdEditor = (QPlainTextEdit *)sender();
	m_plainText = mdEditor->toPlainText();
	m_htmlComvertor.setMarkdownText(m_plainText);
	m_htmlText = m_htmlComvertor.convert();
	updatePreview();
}

