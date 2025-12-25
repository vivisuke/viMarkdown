#pragma once
#include "C:\Qt\6.10.0\msvc2022_64\include\QtWidgets\qtextedit.h"
class HtmlViewer :
    public QTextEdit
{
public:
    HtmlViewer(QWidget* parent = nullptr) : QTextEdit(parent)
    {
    }
};

