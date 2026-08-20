#pragma once

#include <QDialog>
#include "ui_SettingsDialog.h"

QT_BEGIN_NAMESPACE
namespace Ui { class SettingsDialogClass; };
QT_END_NAMESPACE

class SettingsDialog : public QDialog
{
	Q_OBJECT

public:
	SettingsDialog(QWidget *parent = nullptr, int=0);	//	初期ページ
	~SettingsDialog();

	void	setPage(int);

protected:
	void accept() override;

	void	onDefaultDir();

	void onNumLowFireChanged(int);
	void onNumHighFireChanged(int);
	void onAllCompletedStar(bool);
	void onEditorFontSizeChanged(int);
	void onPreviewFontSizeChanged(int);
	void onHeadingColorButtonClicked();
	void onActiveLineColorButtonClicked();
	void onInactiveLineColorButtonClicked();
	void onBoldItalicColorButtonClicked();
	void onBoldColorButtonClicked();
	void onItalicColorButtonClicked();
	void onStrikethroughColorButtonClicked();
	void onMatchColorButtonClicked();
	void onCSVHeaderColorButtonClicked();
	void onCSVZebraColor1ButtonClicked();
	void onCSVZebraColor2ButtonClicked();
	void onQuoteColorButtonClicked();
	void onCodeBlockColorButtonClicked();
	void onKeisenBlockColorButtonClicked();
	void onCompletedColorButtonClicked();
	void onPendingColorButtonClicked();
	void onNotesOnlyColorButtonClicked();
	void onTreeItemChanged(QTreeWidgetItem *current, QTreeWidgetItem *previous);
	void	updateColorButtons();
	void	pickColor(QColor &targetColor, const QString &title);

signals:
	void	settingsChanged();


private:
	Ui::SettingsDialogClass *ui;
};

