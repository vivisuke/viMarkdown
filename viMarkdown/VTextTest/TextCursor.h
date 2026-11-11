#pragma once

#include "TextDocument.h"
#include "LineBlock.h"

#include <QChar>
#include <QString>

//----------------------------------------------------------------------
//	TextCursor: QTextCursor に対応するカーソル
//
//	・文書への参照と位置・アンカー（選択の起点）を保持する
//	・文書が他のカーソルや直接編集で変更されると、TextDocument からの通知で
//	  位置・アンカーが自動調整される。調整規則：
//	    挿入：挿入位置より後ろは後退する。挿入位置ちょうどは動かない
//	          （後続の文字に付く）
//	    削除：範囲内は範囲先頭へクランプ、範囲の後ろは前進する
//	・selectedText() は '\n' をそのまま含む（QTextCursor の U+2029 置換はしない）
//	・Up / Down の目標カラムは表示幅（半角単位）で保持する
//	・単語単位の移動は未実装（「単語」の定義が未決のため）
//----------------------------------------------------------------------

class TextCursor
{
public:
	typedef TextDocument::pos_t pos_t;

	enum MoveMode { MoveAnchor, KeepAnchor };
	enum MoveOperation {
		Left, Right, Up, Down,
		UpLine, DownLine,	//	論理行単位（目標カラムは文字単位で保持）
		StartOfLine, EndOfLine, Start, End,
	};

	TextCursor() : m_doc(nullptr), m_pos(0), m_anchor(0), m_goalColumn(-1), m_goalLineColumn(-1) {}
	explicit TextCursor(TextDocument *doc) : m_doc(doc), m_pos(0), m_anchor(0), m_goalColumn(-1), m_goalLineColumn(-1) {
		if( m_doc != nullptr ) m_doc->registerCursor(this);
	}
	TextCursor(TextDocument *doc, pos_t pos) : m_doc(doc), m_pos(0), m_anchor(0), m_goalColumn(-1), m_goalLineColumn(-1) {
		if( m_doc != nullptr ) {
			m_doc->registerCursor(this);
			m_pos = m_anchor = clampPos(pos);
		}
	}
	TextCursor(const TextCursor &o)
		: m_doc(o.m_doc), m_pos(o.m_pos), m_anchor(o.m_anchor), m_goalColumn(o.m_goalColumn), m_goalLineColumn(o.m_goalLineColumn) {
		if( m_doc != nullptr ) m_doc->registerCursor(this);
	}
	TextCursor& operator=(const TextCursor &o) {
		if( this != &o ) {
			if( m_doc != nullptr ) m_doc->unregisterCursor(this);
			m_doc = o.m_doc; m_pos = o.m_pos; m_anchor = o.m_anchor; m_goalColumn = o.m_goalColumn; m_goalLineColumn = o.m_goalLineColumn;
			if( m_doc != nullptr ) m_doc->registerCursor(this);
		}
		return *this;
	}
	~TextCursor() {
		if( m_doc != nullptr ) m_doc->unregisterCursor(this);
	}

	bool isNull() const { return m_doc == nullptr; }
	TextDocument* document() const { return m_doc; }

	//	位置・選択
	pos_t position() const { return m_pos; }
	pos_t anchor() const { return m_anchor; }
	bool hasSelection() const { return m_pos != m_anchor; }
	pos_t selectionStart() const { return m_pos < m_anchor ? m_pos : m_anchor; }
	pos_t selectionEnd() const { return m_pos < m_anchor ? m_anchor : m_pos; }
	QString selectedText() const {
		if( m_doc == nullptr || !hasSelection() ) return QString();
		pos_t s = selectionStart();
		int len = (int)(selectionEnd() - s);
		QString text;
		text.resize(len);
		for( int i = 0; i < len; ++i )
			text[i] = m_doc->characterAt(s + i);
		return text;
	}
	void clearSelection() { m_anchor = m_pos; }
	void setPosition(pos_t pos, MoveMode mode = MoveAnchor) {
		if( m_doc == nullptr ) return;
		m_pos = clampPos(pos);
		if( mode == MoveAnchor ) m_anchor = m_pos;
		m_goalColumn = -1;
		m_goalLineColumn = -1;
	}

	//	ブロック・カラム
	LineBlock block() const {
		return m_doc != nullptr ? m_doc->findBlock(m_pos) : LineBlock();
	}
	int blockNumber() const {
		return m_doc != nullptr ? m_doc->lineMgr().lineAtPos(m_pos) : -1;
	}
	int positionInBlock() const {	//	行頭からの文字数
		if( m_doc == nullptr ) return 0;
		return (int)(m_pos - m_doc->lineMgr().lineStartPos(blockNumber()));
	}
	int columnNumber() const { return positionInBlock(); }

	//	移動（移動できたら true）
	bool movePosition(MoveOperation op, MoveMode mode = MoveAnchor, int n = 1) {
		if( m_doc == nullptr ) return false;
		pos_t before = m_pos;
		for( int i = 0; i < n; ++i )
			if( !moveOnce(op, mode) ) break;
		if( mode == MoveAnchor ) m_anchor = m_pos;	//	移動できなくても選択は解除される
		return m_pos != before;
	}

	//	編集（undo 対象）
	void insertText(const QString &text) {
		if( m_doc == nullptr || text.isEmpty() ) return;
		bool blocked = hasSelection();
		if( blocked ) m_doc->undoMgr().openBlock();
		if( hasSelection() ) removeSelectedText();
		pos_t p = m_pos;
		m_doc->insert(p, text);
		if( blocked ) m_doc->undoMgr().closeBlock();
		setPosition(p + (pos_t)text.size());
	}
	void removeSelectedText() {
		if( m_doc == nullptr || !hasSelection() ) return;
		m_doc->erase(selectionStart(), selectionEnd());	//	調整で位置・アンカーとも開始位置へ
		m_goalColumn = -1;
		m_goalLineColumn = -1;
	}
	void deleteChar() {	//	カーソル位置の文字を削除（前方削除）
		if( m_doc == nullptr ) return;
		if( hasSelection() ) { removeSelectedText(); return; }
		if( m_pos < m_doc->characterCount() ) m_doc->erase(m_pos);
		m_goalColumn = -1;
		m_goalLineColumn = -1;
	}
	void deletePreviousChar() {	//	カーソル直前の文字を削除（BS）
		if( m_doc == nullptr ) return;
		if( hasSelection() ) { removeSelectedText(); return; }
		if( m_pos > 0 ) m_doc->erase(m_pos - 1);	//	調整で位置も前へ
		m_goalColumn = -1;
		m_goalLineColumn = -1;
	}
	void beginEditBlock() { if( m_doc != nullptr ) m_doc->undoMgr().openBlock(); }
	void endEditBlock() { if( m_doc != nullptr ) m_doc->undoMgr().closeBlock(); }

	bool operator==(const TextCursor &o) const {
		return m_doc == o.m_doc && m_pos == o.m_pos && m_anchor == o.m_anchor;
	}
	bool operator!=(const TextCursor &o) const { return !(*this == o); }

private:
	friend class TextDocument;

	//	TextDocument からの編集通知
	void adjustForInsert(pos_t pos, pos_t len) {
		if( m_pos > pos ) m_pos += len;
		if( m_anchor > pos ) m_anchor += len;
		m_goalColumn = -1;
		m_goalLineColumn = -1;
	}
	void adjustForErase(pos_t pos1, pos_t pos2) {
		m_pos = adjustOne(m_pos, pos1, pos2);
		m_anchor = adjustOne(m_anchor, pos1, pos2);
		m_goalColumn = -1;
		m_goalLineColumn = -1;
	}
	void adjustReset() {
		m_pos = m_anchor = 0;
		m_goalColumn = -1;
		m_goalLineColumn = -1;
	}
	static pos_t adjustOne(pos_t p, pos_t pos1, pos_t pos2) {
		if( p <= pos1 ) return p;
		if( p >= pos2 ) return p - (pos2 - pos1);
		return pos1;
	}

	pos_t clampPos(pos_t pos) const {
		if( m_doc == nullptr || pos < 0 ) return 0;
		pos_t sz = m_doc->characterCount();
		return pos < sz ? pos : sz;
	}
	void moveTo(pos_t pos, MoveMode mode) {
		m_pos = clampPos(pos);
		if( mode == MoveAnchor ) m_anchor = m_pos;
	}
	int displayColumn(pos_t pos) const {	//	表示行内での表示カラム（半角単位）
		const VLineMgr &mgr = m_doc->lineMgr();
		int vline = mgr.vlineAtPos(pos);
		pos_t s = mgr.vlineStartPos(vline);
		int col = 0;
		for( pos_t i = s; i < pos; ++i )
			col += VLineMgr::charWidth(m_doc->characterAt(i));
		return col;
	}
	pos_t posAtDisplayColumn(int vline, int goal) const {
		const VLineMgr &mgr = m_doc->lineMgr();
		pos_t s = mgr.vlineStartPos(vline);
		pos_t len = mgr.vlineLength(vline);
		int col = 0;
		pos_t i = 0;
		while( i < len && col < goal ) {
			col += VLineMgr::charWidth(m_doc->characterAt(s + i));
			++i;
		}
		return s + i;
	}
	bool moveOnce(MoveOperation op, MoveMode mode) {
		const VLineMgr &mgr = m_doc->lineMgr();
		switch( op ) {
		case Left:
			if( m_pos <= 0 ) return false;
			moveTo(m_pos - 1, mode);
			break;
		case Right:
			if( m_pos >= m_doc->characterCount() ) return false;
			moveTo(m_pos + 1, mode);
			break;
		case StartOfLine:
			if( m_pos == mgr.lineStartPos(mgr.lineAtPos(m_pos)) ) return false;
			moveTo(mgr.lineStartPos(mgr.lineAtPos(m_pos)), mode);
			break;
		case EndOfLine: {
			pos_t e = mgr.lineStartPos(mgr.lineAtPos(m_pos)) + mgr.lineLength(mgr.lineAtPos(m_pos));
			if( m_pos == e ) return false;
			moveTo(e, mode);
			break;
		}
		case Start:
			if( m_pos == 0 ) return false;
			moveTo(0, mode);
			break;
		case End:
			if( m_pos == m_doc->characterCount() ) return false;
			moveTo(m_doc->characterCount(), mode);
			break;
		case UpLine:
		case DownLine: {
			int line = mgr.lineAtPos(m_pos);
			int target = line + (op == DownLine ? 1 : -1);
			if( target < 0 || target >= mgr.lineCount() ) return false;
			if( m_goalLineColumn < 0 ) m_goalLineColumn = (int)(m_pos - mgr.lineStartPos(line));
			pos_t tp = mgr.lineStartPos(target) + m_goalLineColumn;
			pos_t e = mgr.lineStartPos(target) + mgr.lineLength(target);
			if( tp > e ) tp = e;
			moveTo(tp, mode);
			break;
		}
		case Up:
		case Down: {
			int vline = mgr.vlineAtPos(m_pos);
			int target = vline + (op == Down ? 1 : -1);
			if( target < 0 || target >= mgr.vlineCount() ) return false;
			if( m_goalColumn < 0 ) m_goalColumn = displayColumn(m_pos);
			int goal = m_goalColumn;
			moveTo(posAtDisplayColumn(target, goal), mode);
			m_goalColumn = goal;	//	moveTo では消えないが、他経路に備えて保持を明示
			m_goalLineColumn = -1;	//	表示単位へ切り替えたら論理側の目標は破棄
			return true;
		}
		}
		if( op != Up && op != Down ) m_goalColumn = -1;
		if( op != UpLine && op != DownLine ) m_goalLineColumn = -1;
		return true;
	}

	TextDocument *m_doc;
	pos_t m_pos;
	pos_t m_anchor;
	int m_goalColumn;	//	Up/Down 継続時の目標カラム（表示幅単位、-1 = 未設定）
	int m_goalLineColumn;	//	UpLine/DownLine 継続時の目標カラム（文字単位、-1 = 未設定）
};
