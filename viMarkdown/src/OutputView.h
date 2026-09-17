#pragma once

#include <QPlainTextEdit>

class OutputView  : public QPlainTextEdit
{
	Q_OBJECT

public:
	OutputView(QWidget *parent);
	~OutputView();

    void	highlightSearchText(const QString &searchText);

signals:
	void	do_open(const QString path, int ln);

protected:
    void	mouseDoubleClickEvent(QMouseEvent *e) override;
    void	keyPressEvent(QKeyEvent *e) override;
    bool	event(QEvent *e) override;
};

