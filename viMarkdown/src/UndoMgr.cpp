#include "UndoMgr.h"
#include <QTextDocument>
#include <QTextCursor>
#include <QDebug>
#include <algorithm>

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
}
//template<typename T>
void UndoMgr::init()
{
	m_cur = 0;
	m_savePointCur = 0;
	m_lastDelTextSize = 0;
	m_lastInsTextSize = 0;
	//m_blockOpened = false;
	m_actionPushed = false;
	m_stack.clear();
	m_insText.clear();
	m_delText.clear();
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
bool UndoMgr::push_back(UndoAction *ptr)
{
	try {
		if( m_stack.size() > (size_t)m_cur ) {
			int ix = m_insText.size();
			for(int i = m_stack.size(); --i >= m_cur; ) {
				switch( m_stack[i]->m_type ) {
				case UndoAction::ACT_INSERT: {
					auto p = (UndoActionInsert *)m_stack[i];
					ix = std::min(ix, p->m_ix);
					break;
				}
				case UndoAction::ACT_DELETE: {
					auto p = (UndoActionDelete *)m_stack[i];
					break;
				}
				}
			}
			m_insText.resize(ix);
			m_stack.resize(m_cur);
		}
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
	if( m_cur != 0 ) {
		UndoActionInsert *ptr = (UndoActionInsert *)m_stack[m_cur - 1];
		if( ptr->m_type == UndoAction::ACT_INSERT )
			ptr->m_prohibitMerge = true;
	}
}
bool UndoMgr::push_back_insText(int pos, int sz /*, int ln*/)
{
	// 改行以外の文字を連続した場所に挿入した場合は、直前のアクションに結合（マージ）する
	if( m_cur != 0 && !isNewLine(m_buffer->characterAt(pos)) ) {
		UndoActionInsert *ptr = (UndoActionInsert *)m_stack[m_cur - 1];
		if( ptr->m_type == UndoAction::ACT_INSERT && !ptr->m_prohibitMerge
			&& ptr->m_pos + ptr->m_size == pos )
		{
			ptr->m_size += sz;
			m_stack.resize(m_cur);
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
	} while( flag );
	
	m_buffer->setModified(m_cur != m_savePointCur);
	return pos;
}

int UndoMgr::redo()
{
	int pos = 0;
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
		}
	} while( flag );
	
	m_buffer->setModified(m_cur != m_savePointCur);
	return pos;
}
UndoActionInsert *UndoMgr::newActInsert()
{
	int pix = m_actInsPoolSize / POOL_SIZE;
	int mod = m_actInsPoolSize % POOL_SIZE;
	while( pix >= m_actInsPool.size() )
		m_actInsPool.push_back((UndoActionInsert *)(new char[sizeof(UndoActionInsert)*POOL_SIZE]()));
	UndoActionInsert *ptr = m_actInsPool[pix] + mod;
	++m_actInsPoolSize;
	ptr->m_type = UndoAction::ACT_INSERT;
	ptr->m_flags = 0;
	return ptr;
}

UndoActionDelete *UndoMgr::newActDelete()
{
	int pix = m_actDelPoolSize / POOL_SIZE;
	int mod = m_actDelPoolSize % POOL_SIZE;
	while( pix >= m_actDelPool.size() )
		m_actDelPool.push_back((UndoActionDelete *)(new char[sizeof(UndoActionDelete)*POOL_SIZE]()));
	UndoActionDelete *ptr = m_actDelPool[pix] + mod;
	++m_actDelPoolSize;
	ptr->m_type = UndoAction::ACT_DELETE;
	ptr->m_flags = 0;
	return ptr;
}

UndoActionReplace *UndoMgr::newActReplace()
{
	int pix = m_actRepPoolSize / POOL_SIZE;
	int mod = m_actRepPoolSize % POOL_SIZE;
	while( pix >= m_actRepPool.size() )
		m_actRepPool.push_back((UndoActionReplace *)(new char[sizeof(UndoActionReplace)*POOL_SIZE]()));
	UndoActionReplace *ptr = m_actRepPool[pix] + mod;
	++m_actRepPoolSize;
	ptr->m_type = UndoAction::ACT_REPLACE;
	ptr->m_flags = 0;
	return ptr;
}
