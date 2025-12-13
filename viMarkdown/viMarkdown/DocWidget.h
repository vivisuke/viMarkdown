#pragma once
#include "C:\Qt\6.10.0\msvc2022_64\include\QtWidgets\qwidget.h"
class DocWidget : public QWidget
{
public:
	DocWidget(const QString& title, const QString& fullPath, QWidget* parent = nullptr);

public:
	bool	m_modified_notSaved = false;
	QString	m_title;			//	タブタイトル
	QString	m_fullPath;
};

