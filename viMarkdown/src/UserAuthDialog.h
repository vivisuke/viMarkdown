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

private:
	Ui::UserAuthDialogClass *ui;
};

