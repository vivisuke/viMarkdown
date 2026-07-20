#pragma once

using uchar = unsigned char;

struct UndoAction
{
	enum {
		ACT_UNDEF = 0,
		ACT_INSERT,
		ACT_DELETE,
		ACT_REPLACE,
		//ACT_REPLACE_ALL,

		FLAG_BLOCK = 1,		//	このビットが立っていれば、undo block 開始 or 終了
		//FLAG_MODIFIED = 2,	//	モディファイフラグ
		FLAG_BS = 2,		//	BackSpace による削除
	};
	uchar	m_type;
	uchar	m_flags;

public:
	UndoAction(uchar type = 0, uchar flags = 0)
		: m_type(type)
		, m_flags(flags)
		{}
	virtual ~UndoAction() {};
};

class UndoMgr
{
};

