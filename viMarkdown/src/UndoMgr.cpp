#include "UndoMgr.h"
#include <QTextDocument>
#include <QTextCursor>
#include <QDebug>
#include <algorithm>
#include <new>

inline bool isNewLine(QChar ch) {
	return ch == u'\r' || ch == u'\n';
}
// QTextDocument に対する低レベルのテキスト操作ヘルパー
namespace {
	void basicInsertText(QTextDocument *doc, int pos, const QString &text) {
		QTextCursor cur(doc);
		cur.setPosition(pos);
		cur.insertText(text);
	}

	void basicDeleteText(QTextDocument *doc, int pos, int size) {
		QTextCursor cur(doc);
		cur.setPosition(pos);
		cur.setPosition(pos + size, QTextCursor::KeepAnchor);
		cur.removeSelectedText();
	}

	QString getText(QTextDocument *doc, int pos, int size) {
		QTextCursor cur(doc);
		cur.setPosition(pos);
		cur.setPosition(pos + size, QTextCursor::KeepAnchor);
		// selectedText() は改行を QChar::ParagraphSeparator (0x2029) に変換して返すため、
		// 通常の '\n' に置換して取得します。
		QString text = cur.selectedText();
		text.replace(QChar::ParagraphSeparator, QChar('\n'));
		return text;
	}

	template<typename T>
	T *allocPoolItem(std::vector<T*> &pool, int &usedSize) {
		int pix = usedSize / POOL_SIZE;
		int mod = usedSize % POOL_SIZE;
		while( pix >= (int)pool.size() )
			pool.push_back(reinterpret_cast<T*>(new char[sizeof(T) * POOL_SIZE]));
		T *ptr = pool[pix] + mod;
		++usedSize;
		return new (ptr) T();
	}

	template<typename T>
	void destroyPoolItems(std::vector<T*> &pool, int &usedSize) {
		for( int i = 0; i < usedSize; ++i ) {
			T *ptr = pool[i / POOL_SIZE] + (i % POOL_SIZE);
			ptr->~T();
		}
		usedSize = 0;
	}

	template<typename T>
	void releasePool(std::vector<T*> &pool, int &usedSize) {
		destroyPoolItems(pool, usedSize);
		for( T *chunk : pool )
			delete[] reinterpret_cast<char*>(chunk);
		pool.clear();
	}
}

UndoMgr::UndoMgr(Buffer *buffer)
	: m_buffer(buffer)
	, m_blockLevel(0)
	, m_actInsPoolSize(0)
	, m_actDelPoolSize(0)
	, m_actRepPoolSize(0)
{
	init();
}
//template<typename T>
UndoMgr::~UndoMgr()
{
	releasePool(m_actInsPool, m_actInsPoolSize);
	releasePool(m_actDelPool, m_actDelPoolSize);
	releasePool(m_actRepPool, m_actRepPoolSize);
}
//template<typename T>
void UndoMgr::init()
{
	m_cur = 0;
	m_savePointCur = 0;
	m_lastDelTextSize = 0;
	m_lastInsTextSize = 0;
	m_blockLevel = 0;
	//m_blockOpened = false;
	m_actionPushed = false;
	m_stack.clear();
	m_insText.clear();
	m_delText.clear();
	destroyPoolItems(m_actInsPool, m_actInsPoolSize);
	destroyPoolItems(m_actDelPool, m_actDelPoolSize);
	destroyPoolItems(m_actRepPool, m_actRepPoolSize);
}
void UndoMgr::setFlag(uchar f) {
	if( m_cur == 0 ) return;
	auto ix = m_cur - 1;
	UndoAction *ptr = m_stack[ix];
	ptr->m_flags |= f;
	if( (ptr->m_flags & UndoAction::FLAG_BLOCK) != 0 ) {
		while( --ix >= 0 ) {
			ptr = m_stack[ix];
			ptr->m_flags |= f;
			if( (ptr->m_flags & UndoAction::FLAG_BLOCK) != 0 )
				break;
		}
	}
}
uchar UndoMgr::undoActionFlags() const {
	if( !canUndo() ) return 0;
	UndoAction *ptr = m_stack[m_cur - 1];
	return ptr->m_flags;
}
uchar UndoMgr::redoActionFlags() const {
	if( !canRedo() ) return 0;
	UndoAction *ptr = m_stack[m_cur];
	return ptr->m_flags;
}
void UndoMgr::openBlock()
{
	if( ++m_blockLevel != 1 ) return;
	m_actionPushed = false;
}

void UndoMgr::closeAllBlock()
{
	if( !m_blockLevel ) return;
	m_blockLevel = 0;
	if( !m_actionPushed || m_stack.empty() )
		return;
	int last = m_stack.size() - 1;
	m_stack[last]->m_flags ^= UndoAction::FLAG_BLOCK;
}

void UndoMgr::closeBlock()
{
	if( !m_blockLevel || --m_blockLevel != 0 )
		return;
	if( !m_actionPushed || m_stack.empty() )
		return;
	int last = m_stack.size() - 1;
	m_stack[last]->m_flags ^= UndoAction::FLAG_BLOCK;
}
// 現状態を非モディファイ状態とする（保存時にコールされる）
void UndoMgr::onSaved()
{
	m_savePointCur = m_cur;
	// ※ 個別行の LINEFLAG_SAVED などのビットパック処理は削除
}
void UndoMgr::truncateRedo()
{
	if( m_stack.size() <= (size_t)m_cur ) return;
	int ix = m_insText.size();
	for(int i = m_stack.size(); --i >= m_cur; ) {
		switch( m_stack[i]->m_type ) {
		case UndoAction::ACT_INSERT: {
			auto p = (UndoActionInsert *)m_stack[i];
			ix = std::min<int>(ix, p->m_ix);
			break;
		}
		case UndoAction::ACT_REPLACE: {
			auto p = (UndoActionReplace *)m_stack[i];
			ix = std::min<int>(ix, p->m_ixIns);
			break;
		}
		}
	}
	m_insText.resize(ix);
	m_stack.resize(m_cur);
}
bool UndoMgr::push_back(UndoAction *ptr)
{
	try {
		truncateRedo();
		m_stack.push_back(ptr);
	} catch(...) {
		return false;
	}
	if( isBlockOpened() && !m_actionPushed ) {
		int last = m_stack.size() - 1;
		m_stack[last]->m_flags |= UndoAction::FLAG_BLOCK;
		m_actionPushed = true;
	}
	if( m_savePointCur > m_cur )
		m_savePointCur = -1;
	++m_cur;
	return true;
}

void UndoMgr::prohibitMergeUndo() // 挿入マージ禁止
{
	if( m_cur != 0 && m_stack[m_cur - 1]->m_type == UndoAction::ACT_INSERT ) {
		UndoActionInsert *ptr = (UndoActionInsert *)m_stack[m_cur - 1];
		ptr->m_prohibitMerge = true;
	}
}
bool UndoMgr::push_back_insText(int pos, int sz /*, int ln*/)
{
	// 改行以外の文字を連続した場所に挿入した場合は、直前のアクションに結合（マージ）する
	if( m_cur != 0 && m_stack[m_cur - 1]->m_type == UndoAction::ACT_INSERT
		&& !isNewLine(m_buffer->characterAt(pos)) )
	{
		UndoActionInsert *ptr = (UndoActionInsert *)m_stack[m_cur - 1];
		if( !ptr->m_prohibitMerge && ptr->m_pos + ptr->m_size == pos )
		{
			ptr->m_size += sz;
			truncateRedo();
			if( m_savePointCur >= m_cur )
				m_savePointCur = -1;
			return true;
		}
	}

	UndoActionInsert *act = newActInsert();
	if( act == 0 )
		return false;
	act->m_pos = pos;
	act->m_size = sz;
	// ※ 個別行の LINEFLAG_MODIFIED などの設定処理は削除し、QTextDocument側の modified で一元管理します。
	push_back(act);
	return true;
}

bool UndoMgr::push_back_delText(int pos, int sz, bool BS /*, int ln*/)
{
	const int ix = m_delText.size();
	m_delText.resize(ix + sz); // 削除文字用バッファ容量確保
	m_delText.replace(ix, sz, getText(m_buffer, pos, sz));

	UndoActionDelete *act = newActDelete();
	if( act == 0 )
		return false;
	if( BS )
		act->m_flags |= UndoAction::FLAG_BS;
	act->m_pos = pos;
	act->m_size = sz;
	act->m_ix = ix;
	push_back(act);
	qDebug() << "push_back_delText() pos = " << pos << ", sz = " << sz << ", ix = " << ix << ", delText = " << m_delText.mid(ix, sz);
	return true;
}

UndoActionReplace *UndoMgr::push_back_repText(int pos, int dsz, int isz /*, int ln*/)
{
	const int ix = m_delText.size();
	m_delText.resize(ix + dsz);
	m_delText.replace(ix, dsz, getText(m_buffer, pos, dsz));

	UndoActionReplace *act = newActReplace();
	if( act == 0 )
		return nullptr;
	act->m_pos = pos;
	act->m_sizeDel = dsz;
	act->m_ixDel = ix;
	act->m_sizeIns = isz;
	push_back(act);
	qDebug() << "push_back_repText() pos = " << pos << ", dsz = " << dsz << ", ix = " << ix << ", delText = " << m_delText.mid(ix, dsz);
	return act;
}
int UndoMgr::undo()
{
	int pos = 0;
	int firstPos = -1;
	ushort f0 = undoActionFlags();
	ushort flag = 0;
	do {
		if( !m_cur ) return false;
		UndoAction *ptr = m_stack[--m_cur];
		flag ^= (ptr->m_flags & UndoAction::FLAG_BLOCK);
		switch( ptr->m_type ) {
		case UndoAction::ACT_INSERT: {
			auto p = (UndoActionInsert *)ptr;
			pos = p->m_pos;
			const int ix = p->m_ix = m_insText.size();
			m_insText.resize(ix + p->m_size);
			m_insText.replace(ix, p->m_size, getText(m_buffer, p->m_pos, p->m_size));
			
			// ドキュメントから挿入されたテキストを削除
			basicDeleteText(m_buffer, p->m_pos, p->m_size);
			break;
		}
		case UndoAction::ACT_DELETE: {
			auto p = (UndoActionDelete *)ptr;
			pos = (p->m_flags & UndoAction::FLAG_BS) ? p->m_pos + p->m_size : p->m_pos;
			
			// 削除されていたテキストを復元（再挿入）
			basicInsertText(m_buffer, p->m_pos, m_delText.mid(p->m_ix, p->m_size));
			m_delText.resize(p->m_ix);
			break;
		}
		case UndoAction::ACT_REPLACE: {
			auto p = (UndoActionReplace *)ptr;
			pos = p->m_pos;
			int ix = p->m_ixIns = m_insText.size();
			m_insText.resize(ix + p->m_sizeIns);
			m_insText.replace(ix, p->m_sizeIns, getText(m_buffer, p->m_pos, p->m_sizeIns));
			
			// 新しく挿入したテキストを削除し、元々あったテキストに置換（再挿入）
			basicDeleteText(m_buffer, p->m_pos, p->m_sizeIns);
			basicInsertText(m_buffer, p->m_pos, m_delText.mid(p->m_ixDel, p->m_sizeDel));
			m_delText.resize(p->m_ixDel);
			break;
		}
		}
		if( (f0&UndoAction::FLAG_SHIFT) != 0 ) {
			if( firstPos < 0 || pos < firstPos ) firstPos = pos;
		}
	} while( flag );
	
	m_buffer->setModified(m_cur != m_savePointCur);
	return firstPos >= 0 ? firstPos : pos;
}

int UndoMgr::redo()
{
	int pos = 0;
	int firstPos = -1;
	ushort f0 = redoActionFlags();
	ushort flag = 0;
	do {
		if( (size_t)m_cur >= m_stack.size() ) return false;
		UndoAction *ptr = m_stack[m_cur++];
		flag ^= (ptr->m_flags & UndoAction::FLAG_BLOCK);
		switch( ptr->m_type ) {
		case UndoAction::ACT_INSERT: {
			auto p = (UndoActionInsert *)ptr;
			basicInsertText(m_buffer, p->m_pos, m_insText.mid(p->m_ix, p->m_size));
			m_insText.resize(p->m_ix);
			pos = p->m_pos + p->m_size;
			break;
		}
		case UndoAction::ACT_DELETE: {
			auto p = (UndoActionDelete *)ptr;
			const int ix = m_delText.size();
			m_delText.resize(ix + p->m_size);
			m_delText.replace(ix, p->m_size, getText(m_buffer, p->m_pos, p->m_size));
			
			// 再びテキストを削除
			basicDeleteText(m_buffer, p->m_pos, p->m_size);
			pos = p->m_pos;
			break;
		}
		case UndoAction::ACT_REPLACE: {
			auto p = (UndoActionReplace *)ptr;
			int ix = p->m_ixDel; // undo時に縮小された位置（m_delText.size()）と一致します
			m_delText.resize(ix + p->m_sizeDel);
			m_delText.replace(ix, p->m_sizeDel, getText(m_buffer, p->m_pos, p->m_sizeDel));
			
			// 元々あったテキストを削除し、新しく挿入するテキストに置換（再挿入）
			basicDeleteText(m_buffer, p->m_pos, p->m_sizeDel);
			basicInsertText(m_buffer, p->m_pos, m_insText.mid(p->m_ixIns, p->m_sizeIns));
			m_insText.resize(p->m_ixIns);
			
			// 置換されたテキストの末尾にカーソルを設定
			pos = p->m_pos + p->m_sizeIns;
			break;
		}
		}
		if( (f0&UndoAction::FLAG_SHIFT) != 0 ) {
			if( firstPos < 0 || pos < firstPos ) firstPos = pos;
		}
	} while( flag );
	
	m_buffer->setModified(m_cur != m_savePointCur);
	return firstPos >= 0 ? firstPos : pos;
}
UndoActionInsert *UndoMgr::newActInsert()
{
	return allocPoolItem(m_actInsPool, m_actInsPoolSize);
}

UndoActionDelete *UndoMgr::newActDelete()
{
	return allocPoolItem(m_actDelPool, m_actDelPoolSize);
}

UndoActionReplace *UndoMgr::newActReplace()
{
	return allocPoolItem(m_actRepPool, m_actRepPoolSize);
}
