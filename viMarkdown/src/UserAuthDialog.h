#pragma once

#include <QDialog>
#include "ui_UserAuthDialog.h"

QT_BEGIN_NAMESPACE
namespace Ui { class UserAuthDialogClass; };
QT_END_NAMESPACE

class UserAuthDialog : public QDialog
{
	Q_OBJECT

public:
	UserAuthDialog(QWidget *parent = nullptr);
	~UserAuthDialog();

	//QString	password() const;		//	入力されたパスワード
	QString password() const {
		return ui->lineEdit->text();
	}

protected:
	void accept() override;

private:

	Ui::UserAuthDialogClass *ui;
};

