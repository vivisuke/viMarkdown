#pragma once

#include "LineMgr.h"

#include <QChar>
#include <QString>

//----------------------------------------------------------------------
//	LineBlock: 論理行への一時参照ハンドル（QTextBlock 対応）
//
//	・mgr 参照と行番号だけを保持する軽量な値型。position / length は
//	  必要時に mgr の配列引き（O(1)）で求める
//	・編集をまたいで位置を保持する用途には使わない（それは TextCursor の役割）
//	・text() は '\n' 付き（Qt の QTextBlock::text() とは異なる。全ブロックの
//	  text() を連結すると文書全体と一致する）。length() も '\n' を含む
//	・無効ハンドルでは position() = -1、length() = 0、text() = 空文字列
//	・next() の終端は行数（そこで無効化し、previous() で最終行へ戻れる）。
//	  previous() は 0 から -1（無効）。-1 からの next() は -1 のまま
//----------------------------------------------------------------------

class LineBlock
{
public:
	typedef LineMgr::pos_t pos_t;

	LineBlock() : m_mgr(nullptr), m_line(-1) {}
	LineBlock(const LineMgr *mgr, int line) : m_mgr(mgr), m_line(line) {}

	bool isValid() const {
		return m_mgr != nullptr && 0 <= m_line && m_line < m_mgr->lineCount();
	}
	LineBlock next() const {
		if( m_line < 0 ) return *this;	//	-1 からは復活させない
		return LineBlock(m_mgr, m_line + 1);
	}
	LineBlock previous() const {
		if( m_line < 0 ) return *this;
		return LineBlock(m_mgr, m_line - 1);
	}
	int blockNumber() const { return m_line; }
	pos_t position() const {	//	行頭の文字位置
		return isValid() ? m_mgr->lineStartPos(m_line) : -1;
	}
	int length() const {		//	'\n' を含む文字数（text().size() と一致）
		if( !isValid() ) return 0;
		pos_t len = m_mgr->lineLength(m_line);
		if( hasNewline() ) ++len;
		return (int)len;
	}
	QString text() const {		//	'\n' 付き
		if( !isValid() ) return QString();
		pos_t pos = m_mgr->lineStartPos(m_line);
		int len = length();
		QString text;
		text.resize(len);
		for( int i = 0; i < len; ++i )
			text[i] = (*m_mgr)[pos + i];
		return text;
	}
	const LineMgr* mgr() const { return m_mgr; }

	bool operator==(const LineBlock& o) const { return m_mgr == o.m_mgr && m_line == o.m_line; }
	bool operator!=(const LineBlock& o) const { return !(*this == o); }

private:
	bool hasNewline() const {	//	この行の末尾に '\n' が存在するか
		return m_mgr->lineStartPos(m_line) + m_mgr->lineLength(m_line) < m_mgr->size();
	}

	const LineMgr *m_mgr;
	int m_line;
};
