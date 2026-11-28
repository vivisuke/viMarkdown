#pragma once

#include "TextDocument.h"
#include "TextCursor.h"
#include "ViEngine.h"

#include <QAbstractScrollArea>
#include <QColor>
#include <QRegularExpression>
#include <QVector>
#include <functional>

enum DiffLineFlag : uchar {
	DIFF_NONE = 0,
	DIFF_ADDED = 1,		//	片側のみに存在する行（挿入/削除）
	DIFF_MODIFIED = 2,	//	両側で異なる変更行
	DIFF_DELETED = 3,	//	相手側のみに存在する行のダミー行
	DIFF_PADDING = 4,	//	変更行の高さ合わせ用パディング行
};

struct DiffWordRange {
	int start = 0;		//	論理行内の開始オフセット
	int length = 0;		//	長さ
};

struct DiffLineInfo {
	uchar flag = DIFF_NONE;
	QVector<DiffWordRange> ranges;	//	単語差分ハイライト範囲
	QVector<int> gaps;				//	相手側のみにある単語の欠落位置（論理行内文字位置）
};

struct DiffDisplayRow {
	int line = -1;		//	論理行番号（ダミー行は -1）
	int seg = 0;		//	折り返し段番号（0..）
	uchar flag = DIFF_NONE;
};

//----------------------------------------------------------------------
//	TextEdit: QPlainTextEdit に対応する最小限のプレーンテキスト編集ウィジェット
//
//	・TextDocument + TextCursor を保持し、表示行（ViewLineMgr）単位で描画する
//	・行間は setLineSpacing() で追加ピクセルを指定できる
//	  （QPlainTextEdit では行間が変えられないことが自前化の動機）
//	・折り返しはウィンドウ幅に追従する。横スクロールは無し
//	・カーソル点滅は未実装（後で拡張）
//----------------------------------------------------------------------

class LineNumberArea;
class QFontMetrics;
class QPainter;
class QTimer;

class TextEdit : public QAbstractScrollArea
{
public:
	explicit TextEdit(QWidget *parent = nullptr);
	~TextEdit() override = default;

	TextDocument* document() { return &m_doc; }
	const TextDocument* document() const { return &m_doc; }

	void setPlainText(const QString &text);
	QString toPlainText() const { return m_doc.toPlainText(); }

	TextCursor textCursor() const { return m_cursor; }
	void setTextCursor(const TextCursor &cursor);

	//	行間（フォントの行高に追加するピクセル数）
	void setLineSpacing(int extraPx);
	int lineSpacing() const { return m_lineSpacing; }

	void setEditorFont(const QFont &font);

	void setMarkdownHighlight(bool enabled);
	bool markdownHighlight() const { return m_markdownHighlight; }

	void setHeadingColor(const QColor &color);
	QColor headingColor() const { return m_headingColor; }

	void setEmphasisColor(const QColor &color);
	QColor emphasisColor() const { return m_emphasisColor; }

	void setSelectionColor(const QColor &color);
	QColor selectionColor() const { return m_selectionColor; }

	void setSearchMatchColor(const QColor &color);
	QColor searchMatchColor() const { return m_searchMatchColor; }

	void setSearchHighlight(const QString &pattern, bool ignoreCase = false, bool useRegExp = true);
	void clearSearchHighlight();
	void updateSearchMarkers();
	QString searchHighlightPattern() const { return m_searchHighlightPat; }
	QString wordAtCursor() const;

	//	タブ幅（半角文字数、デフォルト4）
	void setTabWidth(int width);
	int tabWidth() const { return m_doc.lineMgr().tabWidth(); }

	int lineNumberAreaWidth() const;	//	行番号エリアの幅（半角8文字分）
	void lineNumberAreaPaintEvent(QPaintEvent *event);	//	LineNumberArea の描画（エリアから呼ばれる）
	void lineNumberAreaMousePressEvent(QMouseEvent *event);
	void lineNumberAreaMouseMoveEvent(QMouseEvent *event);
	void lineNumberAreaMouseReleaseEvent(QMouseEvent *event);
	void lineNumberAreaLeaveEvent(QEvent *event);

	void undo();
	void redo();
	void cut();
	void copy();
	void paste();
	void selectAll();
	void toggleCheckboxAtLine(int line, bool checked);
	bool toggleCheckboxAtLine(int line);
	bool toggleCheckboxAtCursor();
	bool convertCsvToGfmTable();
	bool convertGfmTableToCsv();
	void setLineAlignment(Qt::Alignment align);
	void indent();
	void unindent();
	void setCursorPosition(int pos, int anchor = -1);
	void applyPreviewEdit(int srcStart, int srcEnd, const QString &text);
	void notifyTextChanged();
	void refreshAfterFold();
	void replayKeyEvent(QKeyEvent *event) { keyPressEvent(event); }
	ViEngine *viEngine() { return &m_vi; }
	const ViEngine *viEngine() const { return &m_vi; }
	void setViEnabled(bool enabled);
	void setKeisenMode(bool enabled) { m_keisenMode = enabled; }
	bool isKeisenMode() const { return m_keisenMode; }
	void setThickKeisen(bool thick) { m_thickKeisen = thick; }
	bool isThickKeisen() const { return m_thickKeisen; }
	void setDiffMode(bool enabled, bool isRightPane = false);
	bool isDiffMode() const { return m_diffMode; }
	bool isDiffRightPane() const { return m_diffRightPane; }
	void setDiffData(QVector<DiffDisplayRow> rows, QVector<DiffLineInfo> lineInfos);
	void clearDiffData();
	int visibleLineCount() const { return pageLines(); }
	void syncImeToViMode();
	void enterInsertModeFromIme();
	void onSaved();
	void setTextChangedCallback(std::function<void()> cb) { m_onTextChanged = std::move(cb); }
	void setCursorChangedCallback(std::function<void()> cb) { m_onCursorChanged = std::move(cb); }
	void setFontSizeChangeCallback(std::function<void(int)> cb) { m_onFontSizeChange = std::move(cb); }
	void setLayoutChangedCallback(std::function<void()> cb) { m_onLayoutChanged = std::move(cb); }
	QVariant inputMethodQuery(Qt::InputMethodQuery query) const override;

protected:
	void paintEvent(QPaintEvent *event) override;
	void keyPressEvent(QKeyEvent *event) override;
	void inputMethodEvent(QInputMethodEvent *event) override;
	void mousePressEvent(QMouseEvent *event) override;
	void mouseDoubleClickEvent(QMouseEvent *event) override;
	void mouseMoveEvent(QMouseEvent *event) override;
	void mouseReleaseEvent(QMouseEvent *event) override;
	void wheelEvent(QWheelEvent *event) override;
	void resizeEvent(QResizeEvent *event) override;
	void focusInEvent(QFocusEvent *event) override;
	void focusOutEvent(QFocusEvent *event) override;
	bool focusNextPrevChild(bool next) override;

private:
	void doKeisenLeft(bool erase, bool thick);
	void doKeisenRight(bool erase, bool thick);
	void doKeisenUp(bool erase, bool thick);
	void doKeisenDown(bool erase, bool thick);
	bool indentSelection(bool unindent);
	void handleReturnKey();
	int lineHeight() const;
	int pageLines() const;
	int tabStopPixels(const QFontMetrics &fm) const;
	int drawLineText(QPainter &painter, int x, int yBaseline, const QString &text, const QFontMetrics &fm, int tabPx) const;
	int xOfPosInViewLine(int viewLine, TextDocument::pos_t pos) const;
	int xOfPos(TextDocument::pos_t pos) const;	//	位置の x 座標（左マージン込み）
	TextDocument::pos_t posFromPoint(const QPoint &pt) const;
	int logicalLineFromY(int y) const;
	void selectLogicalLineRange(int anchorLine, int curLine);
	void wordBoundsAt(TextDocument::pos_t pos, TextDocument::pos_t &outStart, TextDocument::pos_t &outEnd) const;
	void updateScrollBars();
	void updateWrapWidth();
	void ensureCursorVisible();
	void updateLineNumberAreaGeometry();
	void afterChange(bool textChanged = false);	//	編集・移動後のスクロール追随と再描画
	int totalDisplayRows() const;
	int displayRowForViewLine(int viewLine) const;
	int viewLineForDisplayRow(int displayRow) const;

	TextDocument m_doc;
	TextCursor m_cursor;
	int m_lineSpacing = 0;
	bool m_markdownHighlight = true;
	QColor m_headingColor = QColor(0x80, 0x00, 0x00);
	QColor m_emphasisColor = QColor(0x00, 0x33, 0xAA);
	QColor m_selectionColor = QColor(0xC8, 0xF0, 0xC8);
	QColor m_searchMatchColor = QColor(0xFF, 0xF5, 0x9D);
	QString m_searchHighlightPat;
	QRegularExpression m_searchHighlightRe;
	bool m_dragging = false;
	bool m_wordDragging = false;
	TextDocument::pos_t m_wordAnchorStart = 0;
	TextDocument::pos_t m_wordAnchorEnd = 0;
	bool m_lineDragging = false;
	int m_lineAnchor = -1;
	int m_hoverFoldLine = -1;
	QString m_preedit;
	int m_preeditCursor = 0;
	LineNumberArea *m_lineNumberArea = nullptr;
	QTimer *m_blinkTimer = nullptr;
	bool m_caretVisible = true;
	bool m_keisenMode = false;
	bool m_thickKeisen = false;
	bool m_diffMode = false;
	bool m_diffRightPane = false;
	QVector<DiffDisplayRow> m_diffRows;
	QVector<DiffLineInfo> m_diffLineInfos;
	QVector<int> m_viewLineToDisplayRow;
	ViEngine m_vi;
	std::function<void()> m_onTextChanged;
	std::function<void()> m_onCursorChanged;
	std::function<void(int)> m_onFontSizeChange;
	std::function<void()> m_onLayoutChanged;
};

//----------------------------------------------------------------------
//	LineNumberArea: TextEdit の左端に置く行番号表示専用ウィジェット
//	描画は TextEdit::lineNumberAreaPaintEvent に委譲する
//----------------------------------------------------------------------
class LineNumberArea : public QWidget
{
public:
	explicit LineNumberArea(TextEdit *editor) : QWidget(editor), m_editor(editor) {
		setMouseTracking(true);
	}
	QSize sizeHint() const override { return QSize(m_editor->lineNumberAreaWidth(), 0); }
protected:
	void paintEvent(QPaintEvent *event) override { m_editor->lineNumberAreaPaintEvent(event); }
	void mousePressEvent(QMouseEvent *event) override { m_editor->lineNumberAreaMousePressEvent(event); }
	void mouseMoveEvent(QMouseEvent *event) override { m_editor->lineNumberAreaMouseMoveEvent(event); }
	void mouseReleaseEvent(QMouseEvent *event) override { m_editor->lineNumberAreaMouseReleaseEvent(event); }
	void leaveEvent(QEvent *event) override { m_editor->lineNumberAreaLeaveEvent(event); }
private:
	TextEdit *m_editor;
};

//	Unicode 罫線文字 (U+2500..U+257F) の連結ビットを返す（範囲外は 0）
ushort getKeisenConnectionBits(QChar ch);
