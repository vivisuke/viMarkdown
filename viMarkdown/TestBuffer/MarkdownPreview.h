#pragma once

#include <QTextEdit>
#include <functional>

class TextDocument;

class MarkdownPreview : public QTextEdit
{
public:
	explicit MarkdownPreview(QWidget *parent = nullptr);
	~MarkdownPreview() override = default;

	void setMarkdown(const QString &markdown);
	void setMarkdown(TextDocument *srcDoc);
	void setFontSizeChangeCallback(std::function<void(int)> cb) { m_onFontSizeChange = std::move(cb); }

protected:
	void paintEvent(QPaintEvent *event) override;
	void wheelEvent(QWheelEvent *event) override;

private:
	std::function<void(int)> m_onFontSizeChange;
};
