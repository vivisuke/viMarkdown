#pragma once

#include <QTextEdit>
#include <QVector>
#include <functional>

class TextDocument;

class MarkdownPreview : public QTextEdit
{
public:
	explicit MarkdownPreview(QWidget *parent = nullptr);
	~MarkdownPreview() override = default;

	void setMarkdown(const QString &markdown);
	void setMarkdown(TextDocument *srcDoc);
	void scrollToSourceLine(int srcLine, qreal subLineFrac = 0.0);
	bool topSourceLine(int &outLine, qreal &outFrac) const;
	void setFontSizeChangeCallback(std::function<void(int)> cb) { m_onFontSizeChange = std::move(cb); }
	void setCheckboxToggledCallback(std::function<void(int, bool)> cb) { m_onCheckboxToggled = std::move(cb); }

protected:
	void paintEvent(QPaintEvent *event) override;
	void mousePressEvent(QMouseEvent *event) override;
	void wheelEvent(QWheelEvent *event) override;

private:
	QVector<int> m_srcLineToBlock;
	QVector<int> m_blockToSrcLine;
	std::function<void(int)> m_onFontSizeChange;
	std::function<void(int, bool)> m_onCheckboxToggled;
};
