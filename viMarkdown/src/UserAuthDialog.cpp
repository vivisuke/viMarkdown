#include "UserAuthDialog.h"
#include "MainWindow.h"

extern Global g;

UserAuthDialog::UserAuthDialog(QWidget *parent)
	: QDialog(parent)
	, ui(new Ui::UserAuthDialogClass())
{
	ui->setupUi(this);
	connect(ui->buttonBox, &QDialogButtonBox::accepted, this, &UserAuthDialog::accept);
	connect(ui->radioButton_3, &QRadioButton::toggled, ui->lineEdit, &QWidget::setEnabled);
	connect(ui->radioButton_3, &QRadioButton::toggled, ui->label, &QWidget::setEnabled);
	ui->lineEdit->setEnabled(g.m_userType == UT_COMMERCIAL);
	ui->label->setEnabled(g.m_userType == UT_COMMERCIAL);
	switch (g.m_userType) {
	case UT_STUDENT:	ui->radioButton->setChecked(true);	break;
	case UT_COMMERCIAL:	ui->radioButton_3->setChecked(true);	break;
	default:			ui->radioButton_2->setChecked(true);	break;	//	UT_NONCOMMERCIAL
	}
	ui->lineEdit->setText(g.m_authPassword);
}

UserAuthDialog::~UserAuthDialog()
{
	delete ui;
}

void UserAuthDialog::accept() {
	//	undone; 商用利用の場合は、パスワードチェック
	//if( ユーザタイプが商用利用 && パスワードが不正 ) {
	//	パスワードが不正な旨の警告表示
	//	return;
	//}
	//	undone: ユーザタイプ保存
	if( ui->radioButton->isChecked() )
		g.m_userType = UT_STUDENT;
	else if( ui->radioButton_3->isChecked() )
		g.m_userType = UT_COMMERCIAL;
	else
		g.m_userType = UT_NONCOMMERCIAL;
	QDialog::accept();
}
