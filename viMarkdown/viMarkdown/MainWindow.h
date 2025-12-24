#pragma once

#include <QtWidgets/QMainWindow>
#include <qsplitter>
#include "ui_MainWindow.h"
//#include "markdowntohtmlconvertor.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindowClass; };
QT_END_NAMESPACE

class DocWidget;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

protected:
    void	setup_connections();
    void	read_settings();
    DocWidget	*newTabWidget(const QString& title, const QString& fullPath);
    void	onMDTextChanged();
    QSplitter	*getCurTabSplitter();
    DocWidget	*getCurDocWidget();
    //DocWidget	*findDocWidget(const QString& fullPath);
    int		tabIndexOf(const QString& title, const QString& fullPath);
    void	addTab(const QString& title, const QString fullPath = "", const QString txt = "");
    void	addTopItemToTreeWidget(const QString& title, const QString fullPath);
    void	updateHTMLModeCheck();
    void	updatePreview();
    void	updateOutlineTree();
    void	insertInline(const QString&);
    QTreeWidgetItem* findTopLevelItemByFullPath(const QString& title, const QString fullPath);
    void	do_open(const QString&);
    void	do_load(const QString&);
    int		treeItemToTabIndex(QTreeWidgetItem *current);
    void	addToRecentFiles(const QString& fullPath);
    void	insertSearchComboBox();
    void	do_find(bool backward=false);

    void	onOutlineBarVisibilityChanged(bool visible);
    void	onTreeCurrentItemChanged(QTreeWidgetItem *current, QTreeWidgetItem *previous);
    void	onTreeItemActivated(QTreeWidgetItem *current, int);
    void	onMdEditCurPosChanged();
    void	onFileChanged(const QString&);
    void	onModificationChanged(bool);
    void	onSearchCBActivated();

    void	onAction_New();
    void	onAction_Open();
    void	onAction_Save();
    void	onAction_Close();
    void	onAction_Undo();
    void	onAction_Redo();
    void	onAction_Bold();
    void	onAction_Italic();
    void	onAction_Strikethrough();
    void	onAction_List();
    void	onAction_NumList();
    void	onAction_Checkbox();
    void	onAction_Indent();
    void	onAction_UnIndent();
    void	onAction_Find();
    void	onAction_ForwardAgain();
    void	onAction_BackwardAgain();
    void	onAction_HTML(bool);
    void	onAction_Source(bool);
    void	onAction_OutlineBar(bool);
    void	onAction_FocusOutline();
    void	onAction_About();
    void	onAction_Exit();

    void	onAboutToShow_RecentFiles();

protected:
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    void closeEvent(QCloseEvent *event) override;

private:
    bool	m_opening_file = false;
    bool	m_ignore_changed = false;
    bool	m_htmlMode = true;
    int		m_tab_number = 0;
    QString	m_plainText;
    //QString	m_htmlText;
    //MarkdownToHtmlConvertor	m_htmlComvertor;
    class QComboBox				*m_searchCB = nullptr;
    class QFileSystemWatcher	*m_watcher;

    Ui::MainWindowClass *ui;
};

