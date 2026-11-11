#pragma once

#include "VLineMgr.h"

#include <QChar>
#include <QString>

//----------------------------------------------------------------------
//	TextDocument: QTextDocument に対応するプレーンテキスト文書
//
//	・1次元の文字の並びを管理する。テキスト本体は VLineMgr（LineMgr）が
//	  gap_buffer<QChar> に保持し、改行は '\n' としてバッファに含まれる
//	・UndoMgr と VLineMgr を保持する（UndoMgr 本体は統合時に追加する）
//	・編集は undo 対象の insert / erase と、対象外の insertRaw / eraseRaw に分ける。
//	  Undo/Redo の実行自体は Raw 系を使う（記録してしまうと無限ループになるため）
//----------------------------------------------------------------------

class TextDocument
{
public:
	typedef LineMgr::pos_t pos_t;

	TextDocument() {}

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
	void clear() { m_lineMgr.clear(); }

	//	編集（undo 対象外）
	void insertRaw(pos_t pos, const QString &text) {
		if( text.isEmpty() ) return;
		m_lineMgr.insert(pos, text.constData(), (pos_t)text.size());
	}
	void eraseRaw(pos_t pos1, pos_t pos2) {	//	[pos1, pos2) を削除
		if( pos2 > pos1 ) m_lineMgr.erase(pos1, pos2 - pos1);
	}

	//	編集（undo 対象。UndoMgr 統合までは Raw と同じ動作）
	void insert(pos_t pos, const QString &text) { insertRaw(pos, text); }	//	TODO: UndoMgr 統合時に記録処理を挟む
	void erase(pos_t pos1, pos_t pos2) { eraseRaw(pos1, pos2); }			//	TODO: 同上
	void erase(pos_t pos) { eraseRaw(pos, pos + 1); }						//	1文字削除。TODO: 同上

	//	行管理へのアクセス
	VLineMgr& lineMgr() { return m_lineMgr; }
	const VLineMgr& lineMgr() const { return m_lineMgr; }

private:
	VLineMgr m_lineMgr;
	//	UndoMgr m_undoMgr;	//	UndoMgr 統合時に追加
};
