#pragma once
#include "C:\Qt\6.10.0\msvc2022_64\include\QtWidgets\qplaintextedit.h"
class MarkdownEditor : public QPlainTextEdit
{
public:
	MarkdownEditor(QWidget *parent = nullptr) : QPlainTextEdit(parent)
	{
	}
protected:
    void keyPressEvent(QKeyEvent *e) override;
};

