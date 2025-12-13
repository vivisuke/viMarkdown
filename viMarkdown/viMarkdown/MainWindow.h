#pragma once

#include <QtWidgets/QMainWindow>
#include <qsplitter>
#include "ui_MainWindow.h"
#include "markdowntohtmlconvertor.h"

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
    DocWidget	*newTabWidget(const QString& title, const QString& fullPath);
    void	onMDTextChanged();
    QSplitter	*getCurTabSplitter();
    void	addTab(const QString& title, const QString fullPath = "", const QString txt = "");
    void	updateHTMLModeCheck();
    void	updatePreview();

    void	onAction_New();
    void	onAction_Open();
    void	onAction_Save();
    void	onAction_Close();
    void	onAction_HTML(bool);
    void	onAction_Source(bool);

private:
    bool	m_opening_file = false;
    bool	m_htmlMode = true;
    int		m_tab_number = 0;
    QString	m_plainText;
    QString	m_htmlText;
    MarkdownToHtmlConvertor	m_htmlComvertor;

    Ui::MainWindowClass *ui;
};

