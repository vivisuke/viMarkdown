#pragma once

#include "VLineMgr.h"
#include "UndoMgr.h"
#include "LineBlock.h"
#include "VLineBlock.h"

#include <QChar>
#include <QString>
#include <QVector>

class TextCursor;

//----------------------------------------------------------------------
//	TextDocument: QTextDocument に対応するプレーンテキスト文書
//
//	・1次元の文字の並びを管理する。テキスト本体は VLineMgr（LineMgr）が
//	  gap_buffer<QChar> に保持し、改行は '\n' としてバッファに含まれる
//	・UndoMgr と VLineMgr を保持する
//	・編集は undo 対象の insert / erase と、対象外の insertRaw / eraseRaw に分ける。
//	  Undo/Redo の実行自体は Raw 系を使う（記録してしまうと無限ループになるため）
//----------------------------------------------------------------------

class TextDocument
{
public:
	typedef LineMgr::pos_t pos_t;

	TextDocument() : m_undoMgr(this) {}

	//	文字・テキスト取得
	int characterCount() const { return (int)m_lineMgr.size(); }	//	実文字数（'\n' を含む）
	QChar characterAt(pos_t pos) const {
		if( pos < 0 || pos >= m_lineMgr.size() ) return QChar();
		return m_lineMgr[pos];
	}
	QString toPlainText() const {
		QString text;
		text.resize((int)m_lineMgr.size());
		for( pos_t i = 0; i < m_lineMgr.size(); ++i )
			text[(int)i] = m_lineMgr[i];
		return text;
	}
	void setPlainText(const QString &text) {
		clear();
		insertRaw(0, text);
	}
	void clear() {
		m_lineMgr.clear();
		adjustCursorsReset();
		m_undoMgr.init();	//	undo 履歴も破棄する（QTextDocument::clear と同様）
		m_modified = true;
	}

	//	編集（undo 対象外）
	void insertRaw(pos_t pos, const QString &text) {
		if( text.isEmpty() ) return;
		m_lineMgr.insert(pos, text.constData(), (pos_t)text.size());
		adjustCursorsForInsert(pos, (pos_t)text.size());
	}
	void eraseRaw(pos_t pos1, pos_t pos2) {	//	[pos1, pos2) を削除
		if( pos2 > pos1 ) {
			m_lineMgr.erase(pos1, pos2 - pos1);
			adjustCursorsForErase(pos1, pos2);
		}
	}

	//	カーソル登録（TextCursor の生成・破棄から呼ばれる）
	void registerCursor(TextCursor *c) {
		if( !m_cursors.contains(c) ) m_cursors.append(c);
	}
	void unregisterCursor(TextCursor *c) { m_cursors.removeOne(c); }

	//	編集（undo 対象。記録の手順はアプリ版の流儀：挿入は実行後に、削除は実行前に記録する）
	void insert(pos_t pos, const QString &text) {
		if( text.isEmpty() ) return;
		insertRaw(pos, text);
		m_undoMgr.push_back_insText((int)pos, (int)text.size());
		m_modified = true;
	}
	void erase(pos_t pos1, pos_t pos2) {	//	[pos1, pos2) を削除
		if( pos2 <= pos1 ) return;
		m_undoMgr.push_back_delText((int)pos1, (int)(pos2 - pos1), false);
		eraseRaw(pos1, pos2);
		m_modified = true;
	}
	void erase(pos_t pos) { erase(pos, pos + 1); }	//	1文字削除

	//	undo / redo（戻り値はカーソル位置。アプリ版 UndoMgr の流儀）
	int undo() { return m_undoMgr.undo(); }
	int redo() { return m_undoMgr.redo(); }
	bool canUndo() const { return m_undoMgr.canUndo(); }
	bool canRedo() const { return m_undoMgr.canRedo(); }

	//	変更状態
	bool isModified() const { return m_modified; }
	void setModified(bool b) { m_modified = b; }	//	UndoMgr の undo/redo からも呼ばれる
	void onSaved() { m_undoMgr.onSaved(); m_modified = false; }	//	保存ポイントを現在位置へ

	//	行管理・undo 管理へのアクセス
	VLineMgr& lineMgr() { return m_lineMgr; }
	const VLineMgr& lineMgr() const { return m_lineMgr; }
	UndoMgr& undoMgr() { return m_undoMgr; }

	//	ブロック取得（QTextDocument の findBlock 系に対応）
	LineBlock findBlock(pos_t pos) const {
		return LineBlock(&m_lineMgr, m_lineMgr.lineAtPos(pos));
	}
	LineBlock findBlockByNumber(int line) const {
		return LineBlock(&m_lineMgr, line);
	}
	LineBlock findBlockByLineNumber(int vline) const {	//	表示行番号から、その行を含む論理行を返す
		if( vline < 0 || vline >= m_lineMgr.vlineCount() ) return LineBlock();
		return LineBlock(&m_lineMgr, m_lineMgr.vlineToLine(vline));
	}
	LineBlock firstBlock() const { return findBlockByNumber(0); }
	LineBlock lastBlock() const { return findBlockByNumber(m_lineMgr.lineCount() - 1); }
	LineBlock begin() const { return firstBlock(); }
	LineBlock end() const { return LineBlock(&m_lineMgr, m_lineMgr.lineCount()); }

	//	表示行取得
	VLineBlock findVLine(pos_t pos) const {
		return VLineBlock(&m_lineMgr, m_lineMgr.vlineAtPos(pos));
	}
	VLineBlock findVLineByNumber(int vline) const {
		return VLineBlock(&m_lineMgr, vline);
	}
	VLineBlock firstVLine() const { return findVLineByNumber(0); }
	VLineBlock lastVLine() const { return findVLineByNumber(m_lineMgr.vlineCount() - 1); }

private:
	//	登録カーソルへの編集通知（TextDocument.cpp で定義）
	void adjustCursorsForInsert(pos_t pos, pos_t len);
	void adjustCursorsForErase(pos_t pos1, pos_t pos2);
	void adjustCursorsReset();

	VLineMgr m_lineMgr;
	QVector<TextCursor*> m_cursors;
	UndoMgr m_undoMgr;
	bool m_modified = false;
};
