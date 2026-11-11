#pragma once

#include "LineMgr.h"

#include <vector>

//----------------------------------------------------------------------
//	VLineMgr: LineMgr にソフト改行（折り返し）による表示行管理を加えたもの
//
//	・論理行を、行幅（半角単位、デフォルト70）で折り返した表示行に分割する
//	・文字幅：半角=1、全角=2、タブ=2（半角空白2文字分として固定）
//	・全角文字が行幅を跨ぐ場合は、その文字の手前で切り、行末に空きが残る
//	・行頭・行末の禁則処理は行わない
//	・空行（長さ0の論理行）も表示行1行として数える
//	・編集時は、影響を受けた論理行だけを再ラップする
//	・表示行の開始番号（論理行ごと）は、LineMgr と同じ遅延オフセット方式で保持する
//	・利用側は VLineMgr のみを参照・操作する（LineMgr は基底としてのみ使う）
//----------------------------------------------------------------------

class VLineMgr : public LineMgr
{
public:
	VLineMgr() {
		m_vinfo.push_back(SegInfo());
		m_vStart.push_back(0);	//	空テキストでも論理行1行・表示行1行
	}

	//	編集（基底の論理行更新＋影響行の再ラップ）
	void insert(pos_t pos, const QChar *data, pos_t len) override {
		if( len <= 0 ) return;
		if( pos < 0 ) pos = 0;
		if( pos > size() ) pos = size();
		int line = lineAtPos(pos);
		int oldCount = segCount(line);
		int nNewLines = 0;
		for( pos_t i = 0; i < len; ++i )
			if( data[i] == QChar('\n') ) ++nNewLines;
		LineMgr::insert(pos, data, len);
		m_vinfo.insert(m_vinfo.begin() + line + 1, (size_t)nNewLines, SegInfo());
		int newTotal = 0;
		for( int ln = line; ln <= line + nNewLines; ++ln ) {
			rewrapLine(ln);
			newTotal += segCount(ln);
		}
		int delta = newTotal - oldCount;
		moveVGap(line + 1);
		pos_t acc = vStartAt(line);	//	この行の表示行開始番号は変わらない
		for( int i = 1; i <= nNewLines; ++i ) {
			acc += segCount(line + i - 1);
			m_vStart.insert(m_vStart.gapIndex(), acc);	//	新しい行の開始番号（真の値で保持）
		}
		m_vOffset += delta;			//	ギャップ以降の行は delta だけずれる（遅延更新）
		m_vlineTotal += delta;
	}
	void insert(pos_t pos, QChar ch) override {
		insert(pos, &ch, 1);
	}
	void erase(pos_t pos, pos_t len) override {
		if( len <= 0 ) return;
		if( pos < 0 ) {
			len += pos;
			pos = 0;
		}
		if( pos >= size() ) return;
		if( pos + len > size() ) len = size() - pos;
		if( len <= 0 ) return;
		pos_t end = pos + len;
		int line1 = lineAtPos(pos);
		int line2 = lineAtPos(end);
		int oldCount = 0;
		for( int ln = line1; ln <= line2; ++ln )
			oldCount += segCount(ln);
		LineMgr::erase(pos, len);
		m_vinfo.erase(m_vinfo.begin() + line1 + 1, m_vinfo.begin() + line2 + 1);
		rewrapLine(line1);			//	結合した行を再ラップ
		int delta = segCount(line1) - oldCount;
		moveVGap(line1 + 1);
		for( int i = 0; i < line2 - line1; ++i )
			m_vStart.erase(m_vStart.gapIndex());	//	消えた行のエントリを除去
		m_vOffset += delta;
		m_vlineTotal += delta;
	}
	void clear() override {
		LineMgr::clear();
		m_vinfo.clear();
		m_vinfo.push_back(SegInfo());
		m_vStart.clear();
		m_vStart.push_back(0);
		m_vOffset = 0;
		m_vlineTotal = 1;
	}

	//	行幅（半角単位）。0以下は折り返しなし
	void setLineWidth(int width) {
		if( width == m_lineWidth ) return;
		m_lineWidth = width;
		//	全行を再ラップし、表示行開始番号を再構築する
		m_vStart.clear();
		m_vOffset = 0;
		m_vlineTotal = 0;
		pos_t acc = 0;
		for( int ln = 0; ln < lineCount(); ++ln ) {
			m_vStart.push_back(acc);
			rewrapLine(ln);
			acc += segCount(ln);
		}
		m_vlineTotal = (int)acc;
	}
	int lineWidth() const { return m_lineWidth; }

	//	表示行情報
	int vlineCount() const { return m_vlineTotal; }
	int vlineCountIn(int line) const { return segCount(line); }	//	論理行内の表示行数
	int lineToVline(int line) const { return (int)vStartAt(line); }	//	論理行の先頭表示行
	int vlineToLine(int vline) const {	//	表示行を含む論理行
		int lo = 0, hi = lineCount() - 1;	//	vStartAt(0) == 0 <= vline が常に成立
		while( lo < hi ) {
			int mid = lo + (hi - lo + 1) / 2;
			if( vStartAt(mid) <= vline )
				lo = mid;
			else
				hi = mid - 1;
		}
		return lo;
	}
	pos_t vlineStartPos(int vline) const {	//	表示行の開始位置（文字位置）
		int line = vlineToLine(vline);
		int seg = vline - lineToVline(line);
		return lineStartPos(line) + (seg == 0 ? 0 : m_vinfo[line].breaks[seg - 1]);
	}
	pos_t vlineLength(int vline) const {	//	表示行の長さ（文字数、'\n' を含まない）
		int line = vlineToLine(vline);
		int seg = vline - lineToVline(line);
		const std::vector<pos_t>& breaks = m_vinfo[line].breaks;
		pos_t start = (seg == 0) ? 0 : breaks[seg - 1];
		pos_t end = (seg + 1 < segCount(line)) ? breaks[seg] : lineLength(line);
		return end - start;
	}
	int vlineAtPos(pos_t pos) const {	//	pos を含む表示行
		int line = lineAtPos(pos);
		pos_t offset = pos - lineStartPos(line);
		const std::vector<pos_t>& breaks = m_vinfo[line].breaks;
		int seg = 0;	//	offset 以前に現れる切れ目の数が、そのまま段番号になる
		while( seg < (int)breaks.size() && breaks[seg] <= offset ) ++seg;
		return lineToVline(line) + seg;
	}
	int vlineColumnAtPos(pos_t pos) const {	//	表示行内での pos の桁（半角単位）
		pos_t start = vlineStartPos(vlineAtPos(pos));
		int col = 0;
		for( pos_t i = start; i < pos; ) {
			QChar ch = (*this)[i];
			if( ch.isHighSurrogate() && i + 1 < size() && (*this)[i + 1].isLowSurrogate() ) {
				col += 2;
				i += 2;
			} else {
				col += charWidth(ch);
				++i;
			}
		}
		return col;
	}

	//	文字幅（半角単位）：タブ=2、全角=2、それ以外=1
	static int charWidth(QChar ch) {
		if( ch == QChar('\t') ) return 2;
		return isFullWidth(ch.unicode()) ? 2 : 1;
	}
	static bool isFullWidth(uint u) {	//	East Asian Width の W / F（BMP内）
		return (u >= 0x1100 && u <= 0x115F)		//	ハングル字母
			|| (u >= 0x2E80 && u <= 0x303E)		//	CJK部首・漢字構成記述
			|| (u >= 0x3041 && u <= 0x33FF)		//	ひらがな・カタカナ・CJK互換
			|| (u >= 0x3400 && u <= 0x4DBF)		//	CJK拡張A
			|| (u >= 0x4E00 && u <= 0x9FFF)		//	CJK統合漢字
			|| (u >= 0xA000 && u <= 0xA4CF)		//	彝文字
			|| (u >= 0xAC00 && u <= 0xD7A3)		//	ハングル音節
			|| (u >= 0xF900 && u <= 0xFAFF)		//	CJK互換漢字
			|| (u >= 0xFE10 && u <= 0xFE19)		//	縦書き形
			|| (u >= 0xFE30 && u <= 0xFE6B)		//	CJK互換形・小形
			|| (u >= 0xFF00 && u <= 0xFF60)		//	全角形
			|| (u >= 0xFFE0 && u <= 0xFFE6);	//	全角記号
	}

private:
	struct SegInfo {
		std::vector<pos_t> breaks;	//	表示行の切れ目（行先頭からの文字オフセット、昇順）
	};

	int segCount(int line) const { return (int)m_vinfo[line].breaks.size() + 1; }
	//	論理行を現在の行幅で再ラップする
	void rewrapLine(int line) {
		std::vector<pos_t>& breaks = m_vinfo[line].breaks;
		breaks.clear();
		if( m_lineWidth <= 0 ) return;	//	折り返しなし
		pos_t start = lineStartPos(line);
		pos_t len = lineLength(line);
		int width = 0;
		for( pos_t i = 0; i < len; ) {
			QChar ch = (*this)[start + i];
			int cw;
			pos_t adv;
			if( ch.isHighSurrogate() && i + 1 < len && (*this)[start + i + 1].isLowSurrogate() ) {
				cw = 2;		//	BMP外は全角扱い
				adv = 2;
			} else {
				cw = charWidth(ch);
				adv = 1;
			}
			if( width > 0 && width + cw > m_lineWidth ) {
				breaks.push_back(i);	//	この文字の手前で切る
				width = 0;
			}
			width += cw;
			i += adv;
		}
	}
	pos_t vStartAt(int line) const {	//	保持値に、ギャップ以降ならオフセットを加える
		pos_t s = m_vStart[line];
		if( line >= m_vStart.gapIndex() ) s += m_vOffset;
		return s;
	}
	//	表示行開始番号のギャップ移動（LineMgr::moveGap と同方式）
	void moveVGap(int newGap) {
		pos_t gap = m_vStart.gapIndex();
		if( newGap > gap ) {
			for( pos_t i = gap; i < newGap; ++i )
				m_vStart.setAt(i, m_vStart[i] + m_vOffset);
		} else if( newGap < gap ) {
			for( pos_t i = newGap; i < gap; ++i )
				m_vStart.setAt(i, m_vStart[i] - m_vOffset);
		}
		m_vStart.setGapIndex(newGap);
	}

	std::vector<SegInfo>	m_vinfo;		//	論理行ごとの折り返し情報
	gap_buffer<pos_t>		m_vStart;		//	論理行ごとの表示行開始番号（ギャップ以降は保持値＋m_vOffset が真値）
	pos_t					m_vOffset = 0;
	int						m_vlineTotal = 1;	//	表示行の総数
	int						m_lineWidth = 70;	//	行幅（半角単位）
};
