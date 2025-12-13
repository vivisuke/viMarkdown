#include <qsplitter.h>
#include <qplaintextedit>
#include <qtextedit>
#include <QFileDialog>
#include <qsplitter.h>
#include <QMessageBox>
#include "MainWindow.h"

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

QWidget *MainWindow::newTabWidget() {
	auto containerWidget = new QWidget;
	QSplitter *splitter = new QSplitter(Qt::Horizontal, containerWidget);
	QPlainTextEdit *mdEditor = new QPlainTextEdit(splitter);
	mdEditor->setPlaceholderText("ここにMarkdownを入力...");
	QTextEdit *previewer = new QTextEdit(splitter);
	previewer->setReadOnly(true); // プレビューなので読み取り専用にする
	previewer->setPlaceholderText("プレビュー画面");
	splitter->addWidget(mdEditor);
	splitter->addWidget(previewer);
	splitter->setSizes(QList<int>() << 500 << 500);
	QVBoxLayout *layout = new QVBoxLayout(containerWidget);
	layout->addWidget(splitter);
	layout->setContentsMargins(0, 0, 0, 0); // 余白をなくして端まで広げる

	connect(mdEditor, &QPlainTextEdit::textChanged, this, &MainWindow::onPlainTextChanged);

	return containerWidget;
}

QSplitter *MainWindow::getCurTabSplitter() {
	auto containerWidget = ui->tabWidget->currentWidget();
	if( containerWidget == nullptr ) return nullptr;
	return containerWidget->findChild<QSplitter*>();
}

void MainWindow::onAction_New() {
	qDebug() << "MainWindow::onAction_New()";

	addTab(QString("無題-%1").arg(++m_tab_number));
}
void MainWindow::addTab(const QString &name, const QString txt) {
	auto ptr = newTabWidget();
	int ix = ui->tabWidget->addTab(ptr, name);
	ui->tabWidget->setCurrentIndex(ix);

	QSplitter *splitter = getCurTabSplitter();
	if( splitter == nullptr ) return;
	QPlainTextEdit *mdEditor = (QPlainTextEdit*)splitter->widget(0);
	if( !txt.isEmpty() )
		mdEditor->setPlainText(txt);
	mdEditor->setFocus();

}
void MainWindow::onAction_Open() {
	QString filePath = QFileDialog::getOpenFileName(
	    this,
	    "open File",            // ダイアログのタイトル
	    QDir::homePath(),       // 初期ディレクトリ（ホームフォルダ）
	    "markdown file (*.md)"  // フィルター
	);

	if (!filePath.isEmpty()) {
		qDebug() << "path = " << filePath;
		QFile file(filePath);
	    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
	        QMessageBox::warning(this, tr("エラー"), tr("ファイルが開けません:\n%1").arg(filePath));
	        return;
	    }

	    QString content = file.readAll();
	    QFileInfo fileInfo(filePath);
	    addTab(fileInfo.fileName(), content);
	}
}
void MainWindow::onAction_Save() {
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

