#pragma once

#include <QtWidgets/QMainWindow>
#include "ui_MainWindow.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindowClass; };
QT_END_NAMESPACE

class TextEdit;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    void do_output(const QString& txt);

private:
    void onOpen();
    bool openFile(const QString &filePath);
    void addToRecentFiles(const QString &fullPath);
    void onAboutToShow_RecentFiles();

    Ui::MainWindowClass *ui;
    TextEdit *m_textEdit = nullptr;
    QString m_lastDir;
};

