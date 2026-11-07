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

void UserAuthDialog::accept() {
	//	undone; 商用利用の場合は、パスワードチェック
	//if( ユーザタイプが商用利用 && パスワードが不正 ) {
	//	パスワードが不正な旨の警告表示
	//	return;
	//}
	//	undone: ユーザタイプ保存
	QDialog::accept();
}
