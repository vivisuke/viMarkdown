#pragma once

#include <QtWidgets/QMainWindow>
#include "ui_MainWindow.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindowClass; };
QT_END_NAMESPACE

class QTabWidget;
class QTextEdit;
class TextEdit;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    void do_output(const QString& txt);

private:
    TextEdit *addEditorTab(const QString &title, const QString &filePath = QString());
    TextEdit *currentTextEdit() const;
    QTextEdit *currentPreviewEdit() const;
    int findTabByFilePath(const QString &cleanPath) const;
    void onCurrentTabChanged(int index);
    void onTabCloseRequested(int index);
    void onOpen();
    void onClose();
    void onSettings();
    void applyLineSpacing(int percent);
    bool openFile(const QString &filePath);
    void addToRecentFiles(const QString &fullPath);
    void onAboutToShow_RecentFiles();

    Ui::MainWindowClass *ui;
    QTabWidget *m_tabWidget = nullptr;
    int m_tabNumber = 0;
    int m_lineSpacingPercent = 0;
    QString m_lastDir;
};

