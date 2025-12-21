#pragma once
#include "C:\Qt\6.10.0\msvc2022_64\include\QtWidgets\qwidget.h"
#include "markdowntohtmlconvertor.h"

//class QPlainTextEdit;
class MarkdownEditor;
class QTextEdit;

class DocWidget : public QWidget
{
public:
	DocWidget(const QString& title, const QString& fullPath, QWidget* parent = nullptr);

public:
	bool	m_modified = false;		//	編集＆未保存状態
	QString	m_title;				//	タブタイトル
	QString	m_fullPath;
	MarkdownEditor	*m_mdEditor = nullptr;			//	マークダウンエディタへのポインタ
	QTextEdit		*m_previewer = nullptr;			//	マークダウンプレビューワへのポインタ
    MarkdownToHtmlConvertor	m_htmlComvertor;
};

