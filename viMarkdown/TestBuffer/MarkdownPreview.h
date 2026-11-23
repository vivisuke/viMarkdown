#pragma once

#include <QTextEdit>

class MarkdownPreview : public QTextEdit
{
public:
	explicit MarkdownPreview(QWidget *parent = nullptr);
	~MarkdownPreview() override = default;

	void setMarkdown(const QString &markdown);
};
