#pragma once

#include <QColor>
#include <QTextEdit>
#include <QVector>
#include <functional>

class QTimer;
class TextDocument;

class MarkdownPreview : public QTextEdit
{
public:
	struct SyncSpan {
		int srcPos = 0;	//	TextDocument 側の文字位置
		int prvPos = 0;	//	MarkdownPreview 側の文字位置
		int len = 0;	//	1対1対応する文字数（空行・空項目アンカーは 0）
	};

	explicit MarkdownPreview(QWidget *parent = nullptr);
	~MarkdownPreview() override = default;

	void setMarkdown(const QString &markdown);
	void setMarkdown(TextDocument *srcDoc);
	void updateMarkdown(TextDocument *srcDoc, int hintSrcLine, bool *outHeadingsChanged = nullptr);
	void setTableColors(const QColor &header, const QColor &zebra1, const QColor &zebra2);
	void scrollToSourceLine(int srcLine, qreal subLineFrac = 0.0);
	bool topSourceLine(int &outLine, qreal &outFrac) const;
	int srcToPrvPos(int srcPos) const;
	int prvToSrcPos(int prvPos) const;
	void setCursorFromSrcPos(int srcPos, int srcAnchor = -1);
	void setFontSizeChangeCallback(std::function<void(int)> cb) { m_onFontSizeChange = std::move(cb); }
	void setCheckboxToggledCallback(std::function<void(int, bool)> cb) { m_onCheckboxToggled = std::move(cb); }
	void setEditCallback(std::function<void(int, int, const QString &)> cb) { m_onEdit = std::move(cb); }
	void setUndoRedoCallback(std::function<void(bool)> cb) { m_onUndoRedo = std::move(cb); }

	void cutToSource();
	void pasteToSource();

protected:
	void paintEvent(QPaintEvent *event) override;
	void keyPressEvent(QKeyEvent *event) override;
	void inputMethodEvent(QInputMethodEvent *event) override;
	void mousePressEvent(QMouseEvent *event) override;
	void mouseMoveEvent(QMouseEvent *event) override;
	void wheelEvent(QWheelEvent *event) override;
	void resizeEvent(QResizeEvent *event) override;
	void focusInEvent(QFocusEvent *event) override;
	void focusOutEvent(QFocusEvent *event) override;

private:
	void updateImageSizes();

	QVector<int> m_srcLineToBlock;
	QVector<int> m_blockToSrcLine;
	QVector<SyncSpan> m_syncSpans;
	int m_prevSrcLineCount = 0;
	int m_prevSrcCharCount = 0;
	bool m_prevHasMultiLineState = false;
	bool m_hasImages = false;
	int m_lastImageMaxWidth = -1;
	QColor m_tableHeaderColor = QColor(0xE6, 0xF0, 0xFF);
	QColor m_tableZebraColor1 = QColor(0xFF, 0xFF, 0xFF);
	QColor m_tableZebraColor2 = QColor(0xFF, 0xFF, 0xE0);
	QTimer *m_blinkTimer = nullptr;
	bool m_caretVisible = true;
	std::function<void(int)> m_onFontSizeChange;
	std::function<void(int, bool)> m_onCheckboxToggled;
	std::function<void(int, int, const QString &)> m_onEdit;
	std::function<void(bool)> m_onUndoRedo;
};
