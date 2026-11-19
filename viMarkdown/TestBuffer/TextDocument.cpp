#include "TextDocument.h"
#include "TextCursor.h"

//----------------------------------------------------------------------
//	TextDocument: 登録カーソルへの編集通知
//	（TextCursor のメソッドを呼ぶため、完全型の見えるここに置く）
//----------------------------------------------------------------------

void TextDocument::adjustCursorsForInsert(pos_t pos, pos_t len)
{
	for( TextCursor *c : m_cursors )
		c->adjustForInsert(pos, len);
}

void TextDocument::adjustCursorsForErase(pos_t pos1, pos_t pos2)
{
	for( TextCursor *c : m_cursors )
		c->adjustForErase(pos1, pos2);
}

void TextDocument::adjustCursorsReset()
{
	for( TextCursor *c : m_cursors )
		c->adjustReset();
}
