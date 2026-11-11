#pragma once

#include "gap_buffer.h"

#include <QChar>

//----------------------------------------------------------------------
//	LineMgr: テキストと論理行インデックスを管理する
//
//	・テキスト本体は gap_buffer<QChar>、行の開始位置は gap_buffer<pos_t> に保持する
//	・編集は LineMgr の insert / erase 経由でのみ行い、両者を同期させる
//	・行開始位置の更新は遅延させる：インデックスのギャップより後ろのエントリは
//	  古い座標のまま保持し、真の開始位置は「保持値＋オフセット」で求める。
//	  ギャップを移動するとき、横切るエントリについてだけオフセットを実体化する
//	・行は '\n' 区切り。行数 = '\n' の数＋1（末尾が '\n' なら最後は空行）
//----------------------------------------------------------------------

class LineMgr
{
public:
	typedef gap_buffer<QChar>::pos_t pos_t;

	LineMgr() {
		m_lines.push_back(0);	//	空テキストでも1行（開始位置0）
	}
	virtual ~LineMgr() = default;	//	VLineMgr が継承する

	//	テキスト参照
	pos_t size() const { return m_text.size(); }
	bool isEmpty() const { return m_text.isEmpty(); }
	QChar operator[](pos_t ix) const { return m_text[ix]; }
	const gap_buffer<QChar>& text() const { return m_text; }

	//	編集（行インデックスも同時に更新される）
	virtual void insert(pos_t pos, const QChar *data, pos_t len) {
		if( len <= 0 ) return;
		if( pos < 0 ) pos = 0;
		if( pos > m_text.size() ) pos = m_text.size();
		int line = lineAtPos(pos);
		moveGap(line + 1);				//	インデックスのギャップを編集行の直後へ
		m_text.insert(pos, data, (gap_buffer<QChar>::ssize_t)len);
		m_offset += len;				//	ギャップ以降の行は len だけ後ろへずれる（遅延更新）
		for( pos_t i = 0; i < len; ++i ) {
			if( data[i] == QChar('\n') )
				m_lines.insert(m_lines.gapIndex(), pos + i + 1);	//	新しい行の開始位置（真の座標で保持）
		}
	}
	virtual void insert(pos_t pos, QChar ch) {
		insert(pos, &ch, 1);
	}
	virtual void erase(pos_t pos, pos_t len) {
		if( len <= 0 ) return;
		if( pos < 0 ) {
			len += pos;
			pos = 0;
		}
		if( pos >= m_text.size() ) return;
		if( pos + len > m_text.size() ) len = m_text.size() - pos;
		if( len <= 0 ) return;
		pos_t end = pos + len;			//	削除範囲 [pos, end)
		int line = lineAtPos(pos);
		moveGap(line + 1);
		//	真の開始位置が (pos, end] にある行は、開始を作った '\n' ごと消える
		while( m_lines.gapIndex() < m_lines.size()
			&& m_lines[m_lines.gapIndex()] + m_offset <= end )
			m_lines.erase(m_lines.gapIndex());
		m_offset -= len;				//	ギャップ以降の行は len だけ前へずれる（遅延更新）
		m_text.erase((size_t)pos, (size_t)len);
	}
	virtual void clear() {
		m_text.clear();
		m_lines.clear();
		m_lines.push_back(0);
		m_offset = 0;
	}

	//	行情報
	int lineCount() const { return (int)m_lines.size(); }
	pos_t lineStartPos(int line) const { return startAt(line); }
	pos_t lineLength(int line) const {	//	'\n' を含まない長さ
		pos_t start = startAt(line);
		pos_t end = (line + 1 < lineCount()) ? startAt(line + 1) - 1 : m_text.size();
		return end - start;
	}
	pos_t lineEndPos(int line) const {	//	'\n' の位置（最終行は size()）
		return startAt(line) + lineLength(line);
	}
	int lineAtPos(pos_t pos) const {	//	pos を含む行（pos == size() の場合は最終行）
		if( pos < 0 ) pos = 0;
		if( pos > m_text.size() ) pos = m_text.size();
		int lo = 0, hi = lineCount() - 1;	//	startAt(0) == 0 <= pos が常に成立
		while( lo < hi ) {
			int mid = lo + (hi - lo + 1) / 2;
			if( startAt(mid) <= pos )
				lo = mid;
			else
				hi = mid - 1;
		}
		return lo;
	}

private:
	pos_t startAt(int line) const {		//	保持値に、ギャップ以降ならオフセットを加える
		pos_t s = m_lines[line];
		if( line >= m_lines.gapIndex() ) s += m_offset;
		return s;
	}
	//	インデックスのギャップを newGap 行目へ移動する。
	//	横切るエントリはオフセットを実体化（右→左：加算、左→右：減算）してから移動する
	void moveGap(int newGap) {
		pos_t gap = m_lines.gapIndex();
		if( newGap > gap ) {
			for( pos_t i = gap; i < newGap; ++i )
				m_lines.setAt(i, m_lines[i] + m_offset);
		} else if( newGap < gap ) {
			for( pos_t i = newGap; i < gap; ++i )
				m_lines.setAt(i, m_lines[i] - m_offset);
		}
		m_lines.setGapIndex(newGap);
	}

	gap_buffer<QChar>	m_text;		//	テキスト本体
	gap_buffer<pos_t>	m_lines;	//	行開始位置（ギャップ以降は保持値＋m_offset が真値）
	pos_t				m_offset = 0;
};
