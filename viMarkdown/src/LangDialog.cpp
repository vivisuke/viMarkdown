#include <QSettings>
#include "LangDialog.h"
#include "MainWindow.h"

LangDialog::LangDialog(QWidget *parent)
	: QDialog(parent)
	, ui(new Ui::LangDialogClass())
{
	ui->setupUi(this);
	connect(ui->buttonBox, &QDialogButtonBox::accepted, this, &LangDialog::accept);
	connect(ui->buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

	QSettings settings;
	int lang = settings.value(KEY_LANGUAGE, SystemDefault).toInt();
	if (lang == English)
		ui->english->setChecked(true);
	else if (lang == Japanese)
		ui->japanese->setChecked(true);
	else
		ui->systemDefault->setChecked(true);
}

LangDialog::~LangDialog()
{
	delete ui;
}

void LangDialog::accept() {
	int lang = SystemDefault;
	if( ui->english->isChecked() ) lang = English;
	else if( ui->japanese->isChecked() ) lang = Japanese;
	QSettings settings;
	settings.setValue(KEY_LANGUAGE, lang);
	QDialog::accept();
}
