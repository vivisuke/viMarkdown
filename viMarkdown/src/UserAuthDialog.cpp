#include "UserAuthDialog.h"

UserAuthDialog::UserAuthDialog(QWidget *parent)
	: QDialog(parent)
	, ui(new Ui::UserAuthDialogClass())
{
	ui->setupUi(this);
}

UserAuthDialog::~UserAuthDialog()
{
	delete ui;
}

