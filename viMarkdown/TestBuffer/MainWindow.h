#pragma once

#include <QtWidgets/QMainWindow>
#include <QColor>
#include "ui_MainWindow.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindowClass; };
QT_END_NAMESPACE

class QTabWidget;
class MarkdownPreview;
class TextEdit;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    void do_output(const QString& txt);

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    TextEdit *addEditorTab(const QString &title, const QString &filePath = QString());
    TextEdit *currentTextEdit() const;
    MarkdownPreview *currentPreviewEdit() const;
    int findTabByFilePath(const QString &cleanPath) const;
    void onCurrentTabChanged(int index);
    void onTabCloseRequested(int index);
    void onOpen();
    void onSave();
    void onClose();
    void onSettings();
    void applyLineSpacing(int percent);
    void applyColors(const QColor &heading, const QColor &emphasis);
    void applyEditorFont(const QString &family);
    bool openFile(const QString &filePath);
    void addToRecentFiles(const QString &fullPath);
    void onAboutToShow_RecentFiles();

    Ui::MainWindowClass *ui;
    QTabWidget *m_tabWidget = nullptr;
    int m_tabNumber = 0;
    int m_lineSpacingPercent = 0;
    QColor m_headingColor = QColor(0x80, 0x00, 0x00);
    QColor m_emphasisColor = QColor(0x00, 0x33, 0xAA);
    QString m_editorFontFamily;
    QString m_lastDir;
};

