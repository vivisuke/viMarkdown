#pragma once
#include <QScrollBar>
#include "C:\Qt\6.10.0\msvc2022_64\include\QtWidgets\qplaintextedit.h"

class MarkdownEditor : public QPlainTextEdit
{
public:
	MarkdownEditor(QWidget *parent = nullptr);
public:
	void	scrollToTop(int lineNum) {		//	lineNum: 0 org.
		verticalScrollBar()->setValue(lineNum);
	}
	void	scrollToTop(const QTextCursor &cursor);
	//{
	//	this->scrollToTop(cursor.blockNumber());
	//}
	int getVisualLineNumber(const QTextCursor &cursor) const;
protected:
    void keyPressEvent(QKeyEvent *e) override;

private:
    class MarkdownHighlighter *m_highlighter;
};

