#include "UserAuthDialog.h"
#include "MainWindow.h"

extern Global g;

UserAuthDialog::UserAuthDialog(QWidget *parent, bool authenticated)
	: QDialog(parent)
	, ui(new Ui::UserAuthDialogClass())
	, m_authenticated(authenticated)
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
	//ui->lineEdit->setText(g.m_authPassword);
	ui->lineEdit->setText(g.m_userType == UT_COMMERCIAL ? g.m_authPassword : QString());
	connect(ui->radioButton, &QRadioButton::toggled, this, &UserAuthDialog::onModified);
	connect(ui->radioButton_2, &QRadioButton::toggled, this, &UserAuthDialog::onModified);
	connect(ui->radioButton_3, &QRadioButton::toggled, this, &UserAuthDialog::onModified);
	connect(ui->lineEdit, &QLineEdit::textChanged, this, &UserAuthDialog::onModified);
	updateMessLabel();
}

UserAuthDialog::~UserAuthDialog()
{
	delete ui;
}
void UserAuthDialog::onModified() {
	//	種別またはパスワード欄が変更されたら、認証済み表示は取り下げる
	m_authenticated = false;
	updateMessLabel();
}
void UserAuthDialog::updateMessLabel() {
	if( m_authenticated )
		ui->messLabel->setText(tr("You are already authenticated."));
	else if( ui->radioButton_3->isChecked() )
		ui->messLabel->setText(tr("Please enter the password."));
	else
		ui->messLabel->setText(tr("If you purchased a license, authenticate as Commercial use."));
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
