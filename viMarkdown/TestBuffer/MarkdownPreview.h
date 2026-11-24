#pragma once

#include <QColor>
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
	void setTableColors(const QColor &header, const QColor &zebra1, const QColor &zebra2);
	void scrollToSourceLine(int srcLine, qreal subLineFrac = 0.0);
	bool topSourceLine(int &outLine, qreal &outFrac) const;
	void setFontSizeChangeCallback(std::function<void(int)> cb) { m_onFontSizeChange = std::move(cb); }
	void setCheckboxToggledCallback(std::function<void(int, bool)> cb) { m_onCheckboxToggled = std::move(cb); }

protected:
	void paintEvent(QPaintEvent *event) override;
	void mousePressEvent(QMouseEvent *event) override;
	void mouseMoveEvent(QMouseEvent *event) override;
	void wheelEvent(QWheelEvent *event) override;
	void resizeEvent(QResizeEvent *event) override;

private:
	void updateImageSizes();

	QVector<int> m_srcLineToBlock;
	QVector<int> m_blockToSrcLine;
	bool m_hasImages = false;
	int m_lastImageMaxWidth = -1;
	QColor m_tableHeaderColor = QColor(0xE6, 0xF0, 0xFF);
	QColor m_tableZebraColor1 = QColor(0xFF, 0xFF, 0xFF);
	QColor m_tableZebraColor2 = QColor(0xFF, 0xFF, 0xE0);
	std::function<void(int)> m_onFontSizeChange;
	std::function<void(int, bool)> m_onCheckboxToggled;
};
