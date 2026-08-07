#pragma once

#include <QDialog>
#include "ui_GrepDialog.h"

QT_BEGIN_NAMESPACE
namespace Ui { class GrepDialogClass; };
QT_END_NAMESPACE

class GrepDialog : public QDialog
{
	Q_OBJECT

public:
	GrepDialog(const QStringList &hist, const QStringList &dirHist, QWidget *parent = nullptr);
	~GrepDialog();

	const QString searchText() const { return ui->searchTextCB->currentText(); }
	const QString dirText() const { return ui->dirCB->currentText(); }
	bool isGrepSubDir() const { return ui->grepSubDir->isChecked(); }

protected:
	void	onDirBtnClicked();
	void	onIgnoreCaseCheckStateChanged(Qt::CheckState state);
	void	onRegexpCheckStateChanged(Qt::CheckState state);
	void	onGrepSubDirCheckStateChanged(Qt::CheckState state);
	void	onClearOutputCheckStateChanged(Qt::CheckState state);

private:
	Ui::GrepDialogClass *ui;
};

