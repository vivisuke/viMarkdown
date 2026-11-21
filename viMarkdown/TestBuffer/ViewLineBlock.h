#pragma once

#include "ViewLineMgr.h"

#include <QChar>
#include <QString>

//----------------------------------------------------------------------
//	ViewLineBlock: 表示行への一時参照ハンドル（QTextLine 対応）
//
//	・mgr 参照と表示行番号だけを保持する軽量な値型。position / length は
//	  必要時に mgr から求める
//	・text() / length() の '\n' は、論理行の最終表示行でのみ含む
//	  （全表示行の text() を連結すると文書全体と一致する）
//	・無効ハンドルでは position() = -1、length() = 0、text() = 空文字列
//	・next() / previous() の終端規則は LineBlock と同じ
//----------------------------------------------------------------------

class ViewLineBlock
{
public:
	typedef ViewLineMgr::pos_t pos_t;

	ViewLineBlock() : m_mgr(nullptr), m_viewLine(-1) {}
	ViewLineBlock(const ViewLineMgr *mgr, int viewLine) : m_mgr(mgr), m_viewLine(viewLine) {}

	bool isValid() const {
		return m_mgr != nullptr && 0 <= m_viewLine && m_viewLine < m_mgr->viewLineCount();
	}
	ViewLineBlock next() const {
		if( m_viewLine < 0 ) return *this;	//	-1 からは復活させない
		return ViewLineBlock(m_mgr, m_viewLine + 1);
	}
	ViewLineBlock previous() const {
		if( m_viewLine < 0 ) return *this;
		return ViewLineBlock(m_mgr, m_viewLine - 1);
	}
	int lineNumber() const { return m_viewLine; }	//	表示行番号
	int line() const {		//	属する論理行番号
		return isValid() ? m_mgr->viewLineToLine(m_viewLine) : -1;
	}
	pos_t position() const {	//	表示行頭の文字位置
		return isValid() ? m_mgr->viewLineStartPos(m_viewLine) : -1;
	}
	int length() const {		//	'\n' を含む文字数（text().size() と一致）
		if( !isValid() ) return 0;
		pos_t len = m_mgr->viewLineLength(m_viewLine);
		if( isLastInLine() && hasNewline() ) ++len;
		return (int)len;
	}
	QString text() const {
		if( !isValid() ) return QString();
		pos_t pos = m_mgr->viewLineStartPos(m_viewLine);
		int len = length();
		QString text;
		text.resize(len);
		for( int i = 0; i < len; ++i )
			text[i] = (*m_mgr)[pos + i];
		return text;
	}
	const ViewLineMgr* mgr() const { return m_mgr; }

	bool operator==(const ViewLineBlock& o) const { return m_mgr == o.m_mgr && m_viewLine == o.m_viewLine; }
	bool operator!=(const ViewLineBlock& o) const { return !(*this == o); }

private:
	bool isLastInLine() const {	//	論理行の最終表示行か
		int ln = m_mgr->viewLineToLine(m_viewLine);
		return m_viewLine == m_mgr->lineToViewLine(ln) + m_mgr->viewLineCountIn(ln) - 1;
	}
	bool hasNewline() const {	//	属する論理行の末尾に '\n' が存在するか
		int ln = m_mgr->viewLineToLine(m_viewLine);
		return m_mgr->lineStartPos(ln) + m_mgr->lineLength(ln) < m_mgr->size();
	}

	const ViewLineMgr *m_mgr;
	int m_viewLine;
};
