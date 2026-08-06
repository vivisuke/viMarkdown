#include <QSettings>
#include <QColorDialog>
#include <QFileDialog>
#include "MainWindow.h"
#include "SettingsDialog.h"

extern Global g;

SettingsDialog::SettingsDialog(QWidget *parent, int page)
	: QDialog(parent)
	, ui(new Ui::SettingsDialogClass())
{
	ui->setupUi(this);
	if (this->layout()) {
        this->layout()->setSizeConstraint(QLayout::SetFixedSize);
    }
	//ui->treeWidget->setCurrentItem(ui->treeWidget->topLevelItem(page));
	//ui->stackedWidget->setCurrentWidget(ui->page_6); // General
	QSettings settings;
	ui->editorFontSize->setValue(g.m_editorFontSize);
	ui->previewFontSize->setValue(g.m_previewFontSize);
	ui->autoSvgCompleter->setChecked(g.m_auto_svg_completer);
	ui->defaultDir->setText(g.m_defaultDir);
	ui->firstDayOfWeekCB->setCurrentIndex(g.m_firstDayOfWeek - 1);
	updateColorButtons();
	//QColor color("#800000");
	connect(ui->defaultDirPB, &QPushButton::clicked, this, &SettingsDialog::onDefaultDir);
	connect(ui->editorFontSize, &QSpinBox::valueChanged, this, &SettingsDialog::onEditorFontSizeChanged);
	connect(ui->previewFontSize, &QSpinBox::valueChanged, this, &SettingsDialog::onPreviewFontSizeChanged);
	connect(ui->headingsColorPB, &QPushButton::clicked, this, &SettingsDialog::onHeadingColorButtonClicked);
	connect(ui->activeLineColorPB, &QPushButton::clicked, this, &SettingsDialog::onActiveLineColorButtonClicked);
	connect(ui->inactiveLineColorPB, &QPushButton::clicked, this, &SettingsDialog::onInactiveLineColorButtonClicked);
	connect(ui->boldItalicColorPB, &QPushButton::clicked, this, &SettingsDialog::onBoldItalicColorButtonClicked);
	connect(ui->boldColorPB, &QPushButton::clicked, this, &SettingsDialog::onBoldColorButtonClicked);
	connect(ui->italicColorPB, &QPushButton::clicked, this, &SettingsDialog::onItalicColorButtonClicked);
	connect(ui->strikethroughColorPB, &QPushButton::clicked, this, &SettingsDialog::onStrikethroughColorButtonClicked);
	connect(ui->matchColorPB, &QPushButton::clicked, this, &SettingsDialog::onMatchColorButtonClicked);
	connect(ui->CSVHeaderPB, &QPushButton::clicked, this, &SettingsDialog::onCSVHeaderColorButtonClicked);
	connect(ui->CSVZebra1PB, &QPushButton::clicked, this, &SettingsDialog::onCSVZebraColor1ButtonClicked);
	connect(ui->CSVZebra2PB, &QPushButton::clicked, this, &SettingsDialog::onCSVZebraColor2ButtonClicked);
	connect(ui->quotePB, &QPushButton::clicked, this, &SettingsDialog::onQuoteColorButtonClicked);
	connect(ui->codeBlockPB, &QPushButton::clicked, this, &SettingsDialog::onCodeBlockColorButtonClicked);
	connect(ui->keisenBlockPB, &QPushButton::clicked, this, &SettingsDialog::onKeisenBlockColorButtonClicked);
	connect(ui->completedPB, &QPushButton::clicked, this, &SettingsDialog::onCompletedColorButtonClicked);
	connect(ui->pendingPB, &QPushButton::clicked, this, &SettingsDialog::onPendingColorButtonClicked);
	connect(ui->notesOnlyPB, &QPushButton::clicked, this, &SettingsDialog::onNotesOnlyColorButtonClicked);
	connect(ui->treeWidget, &QTreeWidget::currentItemChanged, this, &SettingsDialog::onTreeItemChanged);
	setPage(page);
}

SettingsDialog::~SettingsDialog()
{
	delete ui;
}
void SettingsDialog::onTreeItemChanged(QTreeWidgetItem *current, QTreeWidgetItem *previous) {
    if (!current) return;

    // 選択されたアイテムが「上から何番目の項目か」を取得（0: General, 1: Color）
    int index = ui->treeWidget->indexOfTopLevelItem(current);
    setPage(index);
}
void SettingsDialog::setPage(int page) {
	QTreeWidgetItem *item = ui->treeWidget->topLevelItem(page);
	if( item != nullptr )
	    ui->treeWidget->setCurrentItem(item);
    if (page == 0) {
        ui->stackedWidget->setCurrentWidget(ui->page_6); // General
    } else if (page == 1) {
        ui->stackedWidget->setCurrentWidget(ui->page_5); // Color
    }
}
void SettingsDialog::onDefaultDir() {
	QString dir = QFileDialog::getExistingDirectory(
				    this,
				    tr("Select Directory"),
				    g.m_defaultDir );
	if( !dir.isEmpty() ) {
		ui->defaultDir->setText(dir);
	}
}
static void setColorButtonStyle(QPushButton* button, const QColor& color) {
    button->setStyleSheet(QString(
        "background-color: %1;"		// 背景色
        "border: 1px solid gray;"	// 枠線
        "height: 20px;"
    ).arg(color.name()));
}
void SettingsDialog::updateColorButtons() {
#if 1
	setColorButtonStyle(ui->headingsColorPB,     g.m_headingsColor);
    setColorButtonStyle(ui->activeLineColorPB,   g.m_activeLnColor);
    setColorButtonStyle(ui->inactiveLineColorPB, g.m_inactiveLnColor);
    setColorButtonStyle(ui->boldItalicColorPB,   g.m_boldItalicColor);
    setColorButtonStyle(ui->boldColorPB,         g.m_boldColor);
    setColorButtonStyle(ui->italicColorPB,       g.m_italicColor);
    setColorButtonStyle(ui->strikethroughColorPB, g.m_strikethroughColor);
    setColorButtonStyle(ui->matchColorPB,        g.m_matchColor);
    setColorButtonStyle(ui->CSVHeaderPB,         g.m_CSVHeaderColor);
    setColorButtonStyle(ui->CSVZebra1PB,         g.m_CSVZebraColor1);
    setColorButtonStyle(ui->CSVZebra2PB,         g.m_CSVZebraColor2);
    setColorButtonStyle(ui->quotePB,             g.m_quoteColor);
    setColorButtonStyle(ui->codeBlockPB,         g.m_codeBlockColor);
    setColorButtonStyle(ui->keisenBlockPB,       g.m_keisenBlockColor);
    setColorButtonStyle(ui->completedPB,		g.m_completedColor);
    setColorButtonStyle(ui->pendingPB,			g.m_pendingColor);
    setColorButtonStyle(ui->notesOnlyPB,		g.m_notesOnlyColor);
#else
	ui->headingsColorPB->setStyleSheet(QString(
        "background-color: %1;" // 背景色をセット
        "border: 1px solid gray;" // 枠線をつける
        "height: 20px;" // 必要に応じて高さを固定
    ).arg(g.m_headingsColor.name()));
	ui->activeLineColorPB->setStyleSheet(QString(
        "background-color: %1;" // 背景色をセット
        "border: 1px solid gray;" // 枠線をつける
        "height: 20px;" // 必要に応じて高さを固定
    ).arg(g.m_activeLnColor.name()));
	ui->inactiveLineColorPB->setStyleSheet(QString(
        "background-color: %1;" // 背景色をセット
        "border: 1px solid gray;" // 枠線をつける
        "height: 20px;" // 必要に応じて高さを固定
    ).arg(g.m_inactiveLnColor.name()));
	ui->boldColorPB->setStyleSheet(QString(
        "background-color: %1;" // 背景色をセット
        "border: 1px solid gray;" // 枠線をつける
        "height: 20px;" // 必要に応じて高さを固定
    ).arg(g.m_boldColor.name()));
	ui->matchColorPB->setStyleSheet(QString(
        "background-color: %1;" // 背景色をセット
        "border: 1px solid gray;" // 枠線をつける
        "height: 20px;" // 必要に応じて高さを固定
    ).arg(g.m_matchColor.name()));
	ui->CSVHeaderPB->setStyleSheet(QString(
        "background-color: %1;" // 背景色をセット
        "border: 1px solid gray;" // 枠線をつける
        "height: 20px;" // 必要に応じて高さを固定
    ).arg(g.m_CSVHeaderColor.name()));
	ui->CSVZebra1PB->setStyleSheet(QString(
        "background-color: %1;" // 背景色をセット
        "border: 1px solid gray;" // 枠線をつける
        "height: 20px;" // 必要に応じて高さを固定
    ).arg(g.m_CSVZebraColor1.name()));
	ui->CSVZebra2PB->setStyleSheet(QString(
        "background-color: %1;" // 背景色をセット
        "border: 1px solid gray;" // 枠線をつける
        "height: 20px;" // 必要に応じて高さを固定
    ).arg(g.m_CSVZebraColor2.name()));
#endif
}

void SettingsDialog::accept() {
	QSettings settings;
	settings.setValue(KEY_EDITOR_FONT_SIZE, ui->editorFontSize->value());
	settings.setValue(KEY_PREVIEW_FONT_SIZE, ui->previewFontSize->value());
	settings.setValue(KEY_AUTO_SVG_CMPL, g.m_auto_svg_completer = ui->autoSvgCompleter->isChecked());
	settings.setValue(KEY_DEFAULT_DIR, g.m_defaultDir = ui->defaultDir->text());
	settings.setValue(KEY_FIRST_DAY_OF_WEEK, g.m_firstDayOfWeek = ui->firstDayOfWeekCB->currentIndex()+1);
	QDialog::accept();
}
void SettingsDialog::pickColor(QColor &targetColor, const QString &title) {
	QColor selectedColor = QColorDialog::getColor(targetColor, this, QString("Select %1 Color").arg(title));
	if (selectedColor.isValid()) {
		targetColor = selectedColor;
		updateColorButtons();
		emit settingsChanged();
	}
}
void SettingsDialog::onEditorFontSizeChanged(int sz) {
	g.m_editorFontSize = sz;
	emit settingsChanged();
}
void SettingsDialog::onPreviewFontSizeChanged(int sz) {
	g.m_previewFontSize = sz;
	emit settingsChanged();
}
void SettingsDialog::onActiveLineColorButtonClicked() {
	pickColor(g.m_activeLnColor, "Active Line");
}
void SettingsDialog::onInactiveLineColorButtonClicked() {
	pickColor(g.m_inactiveLnColor, "Inactive Line");
}
void SettingsDialog::onHeadingColorButtonClicked() {
	pickColor(g.m_headingsColor, "Heading");
}
void SettingsDialog::onBoldItalicColorButtonClicked() {
	pickColor(g.m_boldItalicColor, "BoldItalic");
}
void SettingsDialog::onBoldColorButtonClicked() {
	pickColor(g.m_boldColor, "Bold");
}
void SettingsDialog::onItalicColorButtonClicked() {
	pickColor(g.m_italicColor, "Italic");
}
void SettingsDialog::onStrikethroughColorButtonClicked() {
	pickColor(g.m_strikethroughColor, "Strikethrough");
}
void SettingsDialog::onMatchColorButtonClicked() {
	pickColor(g.m_matchColor, "Match");
}
void SettingsDialog::onCSVHeaderColorButtonClicked() {
	pickColor(g.m_CSVHeaderColor, "CSV Header");
}
void SettingsDialog::onCSVZebraColor1ButtonClicked() {
	pickColor(g.m_CSVZebraColor1, "CSV Zebra-1 ");
}
void SettingsDialog::onCSVZebraColor2ButtonClicked() {
	pickColor(g.m_CSVZebraColor2, "CSV Zebra-2 ");
}
void SettingsDialog::onQuoteColorButtonClicked() {
	pickColor(g.m_quoteColor, "Quote ");
}
void SettingsDialog::onCodeBlockColorButtonClicked() {
	pickColor(g.m_codeBlockColor, "Code Block ");
}
void SettingsDialog::onKeisenBlockColorButtonClicked() {
	pickColor(g.m_keisenBlockColor, "Keisen Block ");
}
void SettingsDialog::onCompletedColorButtonClicked() {
	pickColor(g.m_completedColor, "Completed");
}
void SettingsDialog::onPendingColorButtonClicked() {
	pickColor(g.m_pendingColor, "Pending");
}
void SettingsDialog::onNotesOnlyColorButtonClicked() {
	pickColor(g.m_notesOnlyColor, "Notes Only");
}
