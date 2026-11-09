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
	UserAuthDialog(QWidget *parent = nullptr, bool authenticated = false);
	~UserAuthDialog();

	//QString	password() const;		//	入力されたパスワード
	QString password() const {
		return ui->lineEdit->text();
	}

	void	onModified();

protected:
	void accept() override;

	void	updateMessLabel();

private:
	Ui::UserAuthDialogClass *ui;
	bool	m_authenticated;
};

