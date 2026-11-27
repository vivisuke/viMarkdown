#include <QFileDialog>
#include <QDir>
#include "GrepDialog.h"
#include "MainWindow.h"

extern Global g;

GrepDialog::GrepDialog(const QStringList &hist, const QStringList &dirHist, QWidget *parent)
	: QDialog(parent)
	, ui(new Ui::GrepDialogClass())
{
	ui->setupUi(this);
	ui->searchTextCB->clear();
	ui->searchTextCB->addItems(hist);
	QString currentPath = QDir::currentPath();
	ui->dirCB->clear();
	ui->dirCB->addItems(dirHist);
	if (!dirHist.contains(currentPath)) {
		ui->dirCB->addItem(currentPath);
	}
	ui->dirCB->setCurrentText(dirHist.isEmpty() ? currentPath : dirHist.first());
	ui->ignoreCase->setChecked(g.m_ignoreCase);
	ui->regexp->setChecked(g.m_regexp);
	ui->grepSubDir->setChecked(g.m_grepSubDir);
	ui->clearOutput->setChecked(g.m_clearOutput);
	connect(ui->dirPB, &QPushButton::clicked, this, &GrepDialog::onDirBtnClicked);
	connect(ui->buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
	connect(ui->buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

GrepDialog::~GrepDialog()
{
	delete ui;
}

void GrepDialog::accept() {
	g.m_ignoreCase = ui->ignoreCase->isChecked();
	g.m_regexp = ui->regexp->isChecked();
	g.m_grepSubDir = ui->grepSubDir->isChecked();
	g.m_clearOutput = ui->clearOutput->isChecked();
	QDialog::accept();
}

void GrepDialog::onDirBtnClicked() {
	QString initialDir = ui->dirCB->currentText();
	if (initialDir.isEmpty())
		initialDir = QDir::currentPath();
	QString dir = QFileDialog::getExistingDirectory(
		this,
		tr("Select Directory"), // ダイアログのタイトル
		initialDir,             // 開始ディレクトリ
		QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks
	);
	if (!dir.isEmpty()) {
		ui->dirCB->setCurrentText(dir);
	}
}
