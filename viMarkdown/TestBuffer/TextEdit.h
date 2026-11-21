#pragma once

#include "TextDocument.h"
#include "TextCursor.h"

#include <QAbstractScrollArea>

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

	int lineNumberAreaWidth() const;	//	行番号エリアの幅（半角8文字分）
	void lineNumberAreaPaintEvent(QPaintEvent *event);	//	LineNumberArea の描画（エリアから呼ばれる）

	void undo();
	void redo();
	QVariant inputMethodQuery(Qt::InputMethodQuery query) const override;

protected:
	void paintEvent(QPaintEvent *event) override;
	void keyPressEvent(QKeyEvent *event) override;
	void inputMethodEvent(QInputMethodEvent *event) override;
	void mousePressEvent(QMouseEvent *event) override;
	void mouseMoveEvent(QMouseEvent *event) override;
	void mouseReleaseEvent(QMouseEvent *event) override;
	void resizeEvent(QResizeEvent *event) override;
	void focusInEvent(QFocusEvent *event) override;
	void focusOutEvent(QFocusEvent *event) override;

private:
	int lineHeight() const;
	int pageLines() const;
	int xOfPos(TextDocument::pos_t pos) const;	//	位置の x 座標（左マージン込み）
	TextDocument::pos_t posFromPoint(const QPoint &pt) const;
	void updateScrollBars();
	void updateWrapWidth();
	void ensureCursorVisible();
	void updateLineNumberAreaGeometry();
	void afterChange();	//	編集・移動後のスクロール追随と再描画

	TextDocument m_doc;
	TextCursor m_cursor;
	int m_lineSpacing = 0;
	bool m_dragging = false;
	QString m_preedit;
	int m_preeditCursor = 0;
	LineNumberArea *m_lineNumberArea = nullptr;
};

//----------------------------------------------------------------------
//	LineNumberArea: TextEdit の左端に置く行番号表示専用ウィジェット
//	描画は TextEdit::lineNumberAreaPaintEvent に委譲する
//----------------------------------------------------------------------
class LineNumberArea : public QWidget
{
public:
	explicit LineNumberArea(TextEdit *editor) : QWidget(editor), m_editor(editor) {}
	QSize sizeHint() const override { return QSize(m_editor->lineNumberAreaWidth(), 0); }
protected:
	void paintEvent(QPaintEvent *event) override { m_editor->lineNumberAreaPaintEvent(event); }
private:
	TextEdit *m_editor;
};
