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
	ui->dirCB->addItem(currentPath);
	ui->dirCB->setCurrentText(currentPath);
	ui->ignoreCase->setCheckState(g.m_ignoreCase ? Qt::Checked : Qt::Unchecked);
	ui->regexp->setCheckState(g.m_regexp ? Qt::Checked : Qt::Unchecked);
	ui->grepSubDir->setCheckState(g.m_grepSubDir ? Qt::Checked : Qt::Unchecked);
	ui->clearOutput->setCheckState(g.m_clearOutput ? Qt::Checked : Qt::Unchecked);
	connect(ui->dirPB, &QPushButton::pressed, this, &GrepDialog::onDirBtnClicked);
	connect(ui->ignoreCase, &QCheckBox::checkStateChanged, this, &GrepDialog::onIgnoreCaseCheckStateChanged);
	connect(ui->regexp, &QCheckBox::checkStateChanged, this, &GrepDialog::onRegexpCheckStateChanged);
	connect(ui->grepSubDir, &QCheckBox::checkStateChanged, this, &GrepDialog::onGrepSubDirCheckStateChanged);
	connect(ui->clearOutput, &QCheckBox::checkStateChanged, this, &GrepDialog::onClearOutputCheckStateChanged);
	connect(ui->buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
	connect(ui->buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

GrepDialog::~GrepDialog()
{
	delete ui;
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
void GrepDialog::onIgnoreCaseCheckStateChanged(Qt::CheckState state) {
	g.m_ignoreCase = state == Qt::Checked;
	//qDebug() << "g.m_ignoreCase = " << g.m_ignoreCase;
}
void GrepDialog::onRegexpCheckStateChanged(Qt::CheckState state) {
	g.m_regexp = state == Qt::Checked;
	//qDebug() << "g.m_regexp = " << g.m_regexp;
}
void GrepDialog::onGrepSubDirCheckStateChanged(Qt::CheckState state) {
	g.m_grepSubDir = state == Qt::Checked;
}
void GrepDialog::onClearOutputCheckStateChanged(Qt::CheckState state) {
	g.m_clearOutput = state == Qt::Checked;
}
