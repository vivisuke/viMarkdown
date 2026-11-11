#pragma once

#include <cstdint>
#include <cstring>
#include <type_traits>
#include <vector>

//----------------------------------------------------------------------
//	piece_table: gap_buffer と同一インタフェースのピーステーブル実装
//
//	初期テキスト（original）と追記専用バッファ（add）の2バッファと、
//	その部分列を指すピース列で文書を構成する。編集はピース列の操作のみで、
//	既存テキストの移動は発生しない。
//
//	gap_buffer とのインタフェース上の相違点：
//	・ギャップが無いため setGapIndex は no-op、gapIndex() は size() を返す
//	・capacity() は「現在のサイズ＋追加バッファの空き」で定義する
//	・代入演算子が使える（バッファ・ピース列とも値コピー）
//	・indexOf の末尾一致も検出する（gap_buffer 側の既知の問題とは異なる）
//	・挿入元が自バッファ内部を指す自己参照挿入は未対応（gap_buffer と同じ）
//
//	位置探索（locate）は、先頭・前回位置（キャッシュ）・末尾の3アンカーの
//	うち最も近い所から前後へ辿る。キャッシュは locate 自身と編集操作が
//	「開始位置が確定しているピース」を記録することで常に正しく保たれる。
//----------------------------------------------------------------------

template<typename Type>
class piece_table
{
public:
	typedef Type value_type;
	typedef int64_t size_type;
	typedef int64_t ssize_t;
	typedef int64_t difference_type;
	typedef int64_t index_type;
	typedef int64_t pos_t;

	typedef Type& reference;
	typedef const Type& const_reference;
	typedef Type* pointer;
	typedef const Type* const_pointer;

private:
	struct Piece {
		pos_t	m_start;		//	元バッファ内の開始位置
		pos_t	m_length;		//	文字数
		bool	m_fromAdd;		//	true: add バッファ、false: original バッファ
	};

	std::vector<value_type>	m_original;		//	初期テキスト（変更しない）
	std::vector<value_type>	m_add;			//	追記専用バッファ
	std::vector<Piece>		m_pieces;		//	文書順のピース列
	size_type				m_size = 0;		//	総文字数
	mutable size_t			m_cacheIx = 0;		//	前回 locate したピース
	mutable pos_t			m_cacheStart = 0;	//	そのピースの文書内開始位置

	const std::vector<value_type>& source(bool fromAdd) const {
		return fromAdd ? m_add : m_original;
	}
	std::vector<value_type>& source(bool fromAdd) {
		return fromAdd ? m_add : m_original;
	}
	//	文書位置 pos（pos < size()）を含むピースのインデックスとピース内オフセットを返す
	//	先頭・キャッシュ・末尾の3アンカーのうち最も近い所から探索し、結果をキャッシュする
	void locate(pos_t pos, size_t& pieceIx, pos_t& offset) const {
		if( m_pieces.empty() ) {
			pieceIx = 0;
			offset = 0;
			return;
		}
		size_t ix = 0;				//	先頭アンカー
		pos_t start = 0;
		pos_t best = pos;			//	先頭アンカーとの距離
		{	//	末尾アンカー
			size_t lastIx = m_pieces.size() - 1;
			pos_t lastStart = m_size - m_pieces[lastIx].m_length;
			pos_t d = pos >= lastStart ? pos - lastStart : lastStart - pos;
			if( d < best ) {
				best = d;
				ix = lastIx;
				start = lastStart;
			}

		}
		if( m_cacheIx < m_pieces.size() ) {	//	キャッシュアンカー
			pos_t d = pos >= m_cacheStart ? pos - m_cacheStart : m_cacheStart - pos;
			if( d < best ) {
				best = d;
				ix = m_cacheIx;
				start = m_cacheStart;
			}
		}
		while( start > pos ) {		//	アンカーより前なら後方へ辿る
			--ix;
			start -= m_pieces[ix].m_length;
		}
		while( pos >= start + m_pieces[ix].m_length ) {	//	前方へ辿る
			start += m_pieces[ix].m_length;
			++ix;
		}
		m_cacheIx = ix;
		m_cacheStart = start;
		pieceIx = ix;
		offset = pos - start;
	}
	//	add バッファの [addStart, addStart + len) を文書位置 pos へ挿入する（m_size は呼び出し側で更新済みでないこと）
	void insertPiece(pos_t pos, pos_t addStart, pos_t len) {
		if( len <= 0 ) return;
		if( pos == m_size ) {			//	末尾への追加：直前ピースと連結できる場合は伸ばす
			if( !m_pieces.empty() ) {
				Piece& last = m_pieces.back();
				if( last.m_fromAdd && last.m_start + last.m_length == addStart ) {
					m_cacheIx = m_pieces.size() - 1;
					m_cacheStart = m_size - last.m_length;
					last.m_length += len;
					m_size += len;
					return;
				}
			}
			m_pieces.push_back(Piece{addStart, len, true});
			m_cacheIx = m_pieces.size() - 1;
			m_cacheStart = pos;
		} else {
			size_t ix;
			pos_t off;
			locate(pos, ix, off);
			if( off == 0 ) {			//	ピース先頭：直前ピースと連結できる場合は伸ばす
				if( ix > 0 ) {
					Piece& prev = m_pieces[ix - 1];
					if( prev.m_fromAdd && prev.m_start + prev.m_length == addStart ) {
						m_cacheIx = ix - 1;
						m_cacheStart = pos - prev.m_length;
						prev.m_length += len;
						m_size += len;
						return;
					}
				}
				m_pieces.insert(m_pieces.begin() + ix, Piece{addStart, len, true});
				m_cacheIx = ix;
				m_cacheStart = pos;
			} else if( off == m_pieces[ix].m_length ) {	//	ピース末尾
				Piece& pc = m_pieces[ix];
				if( pc.m_fromAdd && pc.m_start + pc.m_length == addStart ) {
					m_cacheIx = ix;
					m_cacheStart = pos - off;
					pc.m_length += len;
				} else {
					m_pieces.insert(m_pieces.begin() + ix + 1, Piece{addStart, len, true});
					m_cacheIx = ix + 1;
					m_cacheStart = pos;
				}
			} else {					//	ピースを分割して間に挟む
				Piece pc = m_pieces[ix];
				m_pieces[ix].m_length = off;
				m_pieces.insert(m_pieces.begin() + ix + 1, Piece{addStart, len, true});
				m_pieces.insert(m_pieces.begin() + ix + 2,
					Piece{pc.m_start + off, pc.m_length - off, pc.m_fromAdd});
				m_cacheIx = ix + 1;
				m_cacheStart = pos;
			}
		}
		m_size += len;
	}

public:
	piece_table() {}
	piece_table(const value_type *data, size_type sz) {	//	初期テキストで構築
		if( sz > 0 ) {
			m_original.assign(data, data + sz);
			m_pieces.push_back(Piece{0, sz, false});
			m_size = sz;
		}
	}
	//	コピー・代入は std::vector の値コピーで正しく動作する（キャッシュもピース列と整合したままコピーされる）

	bool isEmpty() const { return !m_size; }
	bool empty() const { return !m_size; }
	size_type size() const { return m_size; }
	size_type capacity() const {	//	現在のサイズ＋追加バッファの空き
		return m_size + (size_type)(m_add.capacity() - m_add.size());
	}
	pos_t gapIndex() const { return m_size; }	//	ギャップは無い
	void setGapIndex(pos_t) const {}			//	ギャップは無いため何もしない

	value_type operator[](pos_t ix) const {		//	operator[] は範囲チェックを行わない
		size_t pi;
		pos_t off;
		locate(ix, pi, off);
		const Piece& pc = m_pieces[pi];
		return source(pc.m_fromAdd)[pc.m_start + off];
	}
	value_type at(pos_t ix) const { return operator[](ix); }
	value_type& ref(pos_t ix) {
		size_t pi;
		pos_t off;
		locate(ix, pi, off);
		const Piece& pc = m_pieces[pi];
		return source(pc.m_fromAdd)[pc.m_start + off];
	}
	size_type get_data(pos_t ix, pointer buf, int bufSize) const	//	[ix, ix+bufSize) を buf にコピー
	{
		if( ix < 0 ) ix = 0;
		if( ix >= m_size || bufSize <= 0 ) return 0;
		if( ix + bufSize > m_size ) bufSize = (int)(m_size - ix);
		size_t pi;
		pos_t off;
		locate(ix, pi, off);
		int copied = 0;
		while( copied < bufSize && pi < m_pieces.size() ) {
			const Piece& pc = m_pieces[pi];
			pos_t seg = pc.m_length - off;
			if( seg > bufSize - copied ) seg = bufSize - copied;
			memcpy((void *)(buf + copied),
				(const void *)(source(pc.m_fromAdd).data() + pc.m_start + off),
				(size_t)seg * sizeof(value_type));
			copied += (int)seg;
			++pi;
			off = 0;
		}
		return copied;
	}
	static value_type toLowerCase(value_type ch)	//	大文字小文字を区別しない検索用
	{
		if constexpr (std::is_integral_v<value_type>)
			return (value_type)::tolower((int)ch);
		else
			return ch.toLower();	//	QChar 等
	}
	bool isMatch(pos_t ix, const value_type *pat, ssize_t sz) const
	{
		while( ix < size() ) {
			if( *pat != operator[](ix) ) break;
			if( --sz <= 0 ) return true;
			++pat;
			++ix;
		}
		return false;
	}
	bool isMatchLC(pos_t ix, const value_type *pat, size_t sz) const
	{
		while( ix < size() ) {
			if( *pat != toLowerCase(operator[](ix)) ) break;
			if( --sz <= 0 ) return true;
			++pat;
			++ix;
		}
		return false;
	}
	pos_t indexOf(const value_type *pat, size_t sz, pos_t ix = 0, bool ic = false) const
	{
		if( sz == 0 ) return -1;
		if( !ic ) {
			while( ix + (pos_t)sz <= size() ) {
				if( isMatch(ix, pat, sz) ) return ix;
				++ix;
			}
		} else {
			std::vector<value_type> lcpat(sz, value_type());
			for( size_t i = 0; i < sz; ++i ) lcpat[i] = toLowerCase(pat[i]);
			while( ix + (pos_t)sz <= size() ) {
				if( isMatchLC(ix, &lcpat[0], sz) ) return ix;
				++ix;
			}
		}
		return -1;
	}
	bool operator==(const piece_table<value_type> &x) const
	{
		if( size() != x.size() ) return false;
		for( pos_t i = 0; i != size(); ++i ) {
			if( at(i) != x.at(i) ) return false;
		}
		return true;
	}
	bool operator!=(const piece_table<value_type> &x) const
	{
		return !operator==(x);
	}

public:
	void clear()
	{
		m_pieces.clear();
		m_size = 0;		//	バッファ側は保持する（再利用のため）
		m_cacheIx = 0;
		m_cacheStart = 0;
	}
	value_type& front()
	{
		return ref(0);
	}
	value_type& back()
	{
		return ref(m_size - 1);
	}
	void resize(size_type sz, Type t = Type())
	{
		if( sz < 0 ) sz = 0;
		if( sz < m_size )
			eraseFL((size_t)sz, (size_t)m_size);
		else if( sz > m_size ) {
			std::vector<value_type> fill((size_t)(sz - m_size), t);
			insert(m_size, fill.data(), (ssize_t)fill.size());
		}
	}
	bool reserve(size_type sz)		//	追加バッファの容量を確保しておく
	{
		if( sz > m_size )
			m_add.reserve(m_add.size() + (size_t)(sz - m_size));
		return true;
	}
	void setAt(pos_t ix, value_type v)	//	ピース構造上、削除＋挿入で置き換える
	{
		if( ix < 0 || ix >= size() ) return;
		eraseFL((size_t)ix, (size_t)ix + 1);
		insert(ix, v);
	}
	void push_back(value_type v)
	{
		insert(m_size, v);
	}
	void push_front(value_type v)
	{
		insert(0, v);
	}
	bool insert(pos_t ix, value_type v)
	{
		if( ix < 0 ) ix = 0;
		if( ix > m_size ) ix = m_size;
		pos_t addStart = (pos_t)m_add.size();
		m_add.push_back(v);
		insertPiece(ix, addStart, 1);
		return true;
	}
	bool insert(pos_t ix, const_pointer first, const_pointer last)
	{
		return insert(ix, first, last - first);
	}
	bool insert(pos_t ix, const_pointer first, ssize_t sz)
	{
		if( sz <= 0 ) return true;
		if( ix < 0 ) ix = 0;
		if( ix > m_size ) ix = m_size;
		pos_t addStart = (pos_t)m_add.size();
		m_add.insert(m_add.end(), first, first + sz);
		insertPiece(ix, addStart, (pos_t)sz);
		return true;
	}
	void pop_back()
	{
		if( !isEmpty() )
			eraseFL((size_t)(m_size - 1), (size_t)m_size);
	}
	void pop_front()
	{
		erase(0);
	}
	void erase(pos_t ix)
	{
		if( ix < 0 || ix >= size() ) return;
		eraseFL((size_t)ix, (size_t)ix + 1);
	}
	void erase(size_t first, size_t sz)
	{
		eraseFL(first, first + sz);
	}
	void eraseFL(size_t first, size_t last)
	{
		if( last > (size_t)size() ) last = (size_t)size();
		if( first >= last ) return;
		std::vector<Piece> np;
		np.reserve(m_pieces.size() + 1);
		size_t aIx = 0;			//	削除位置を含む（または直前の）残存ピース：キャッシュのアンカー
		pos_t aStart = 0;
		bool haveAnchor = false;
		pos_t acc = 0;
		for( const Piece& pc : m_pieces ) {
			pos_t ps = acc;					//	このピースの文書内範囲 [ps, pe)
			pos_t pe = acc + pc.m_length;
			acc = pe;
			if( pe <= (pos_t)first || ps >= (pos_t)last ) {
				np.push_back(pc);			//	削除範囲と重ならない
				pos_t nStart = ps >= (pos_t)last ? ps - ((pos_t)last - (pos_t)first) : ps;
				if( nStart <= (pos_t)first ) {
					aIx = np.size() - 1;
					aStart = nStart;
					haveAnchor = true;
				}
				continue;
			}
			if( ps < (pos_t)first ) {		//	前半が残る
				np.push_back(Piece{pc.m_start, (pos_t)first - ps, pc.m_fromAdd});
				aIx = np.size() - 1;		//	このピースが削除位置を含む
				aStart = ps;
				haveAnchor = true;
			}
			if( pe > (pos_t)last ) {		//	後半が残る（新位置は削除開始位置）
				np.push_back(Piece{pc.m_start + ((pos_t)last - ps), pe - (pos_t)last, pc.m_fromAdd});
				aIx = np.size() - 1;
				aStart = (pos_t)first;
				haveAnchor = true;
			}
		}
		m_pieces = np;
		m_size -= (pos_t)last - (pos_t)first;
		m_cacheIx = haveAnchor ? aIx : 0;
		m_cacheStart = haveAnchor ? aStart : 0;
	}
};
