#pragma once
#include "C:\Qt\6.10.0\msvc2022_64\include\QtWidgets\qtextedit.h"
class HtmlViewer : public QTextEdit
{
	Q_OBJECT 

public:
    HtmlViewer(QWidget* parent = nullptr) : QTextEdit(parent)
    {
    }

signals:
    // クリックされたブロック番号を通知するシグナル
    void lineClicked(int blockNumber);

protected:
    void mousePressEvent(QMouseEvent *e) override;    // マウスクリックイベントをオーバーライド
};

