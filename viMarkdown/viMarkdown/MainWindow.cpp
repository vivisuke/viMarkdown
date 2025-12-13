#include <qsplitter.h>
#include <qplaintextedit>
#include <qtextedit>
#include "MainWindow.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindowClass())
{
    ui->setupUi(this);
    setWindowTitle("viMarkdown ver 0.001");

	setup_connections();
	onAction_New();
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::setup_connections() {
    connect(ui->action_New, &QAction::triggered, this, &MainWindow::onAction_New);
    connect(ui->action_Close, &QAction::triggered, this, &MainWindow::onAction_Close);
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

void MainWindow::onAction_New() {
	qDebug() << "MainWindow::onAction_New()";

	auto ptr = newTabWidget();
	int ix = ui->tabWidget->addTab(ptr, QString("Tab-%1").arg(++m_tab_number));
	ui->tabWidget->setCurrentIndex(ix);
}
void MainWindow::onAction_Close() {
	qDebug() << "MainWindow::onAction_Close()";

	int ix = ui->tabWidget->currentIndex();
	if( ix >= 0 )
		ui->tabWidget->removeTab(ix);
}
void MainWindow::onPlainTextChanged() {
	//qDebug() << "MainWindow::onPlainTextChanged()";

	QPlainTextEdit *mdEditor = (QPlainTextEdit *)sender();
	m_plainText = mdEditor->toPlainText();
	m_htmlComvertor.setMarkdownText(m_plainText);
	m_htmlText = m_htmlComvertor.convert();
	auto containerWidget = ui->tabWidget->currentWidget();
	if( containerWidget == nullptr ) return;
	QSplitter *splitter = containerWidget->findChild<QSplitter*>();
	if( splitter == nullptr ) return;
	QTextEdit *previewer = (QTextEdit*)splitter->widget(1);
	previewer->setHtml(m_htmlText);
}

