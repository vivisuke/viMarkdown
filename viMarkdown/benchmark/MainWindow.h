#pragma once

#include <QtWidgets/QMainWindow>
#include "ui_MainWindow.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindowClass; };
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

protected:
    void	gen_lines(int n);
    void	onEditorContentsChange(int position, int charsRemoved, int charsAdded);

    void	onAction_gen1000lines();
    void	onAction_gen2000lines();
    void	onAction_gen5000lines();
    void	onAction_gen10000lines();
    void	onAction_gen20000lines();
    void	onAction_SetMarkdown();

private:
    bool	m_processing = false;
    Ui::MainWindowClass *ui;
};

