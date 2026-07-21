#pragma once

#include <vector>
#include <QString>

#define		POOL_SIZE		1024

class MarkdownEditor;
class QTextDocument;

using uchar = unsigned char;
using pos_t = int;
//using Buffer = MarkdownEditor;
using Buffer = QTextDocument;

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
struct UndoActionInsert : public UndoAction
{
	pos_t	m_pos;		//	挿入・削除位置
	int		m_size;		//	挿入・削除文字数（for UTF16）
	int		m_ix;		//	挿入・削除文字列のUndoバッファインデックス
	int		m_ln;		//	pos の行番号 0 org
	int		m_lc;		//	挿入行数
	bool	m_modified;	//	挿入前に m_ln 行が編集済みだったかどうか
	bool	m_saved;	//	挿入前に m_ln 行が保存済みだったかどうか
	bool	m_prohibitMerge;	//	マージ禁止
	//std::vector<bool>	m_mdfyFlags;
public:
	UndoActionInsert(pos_t pos = 0, int size = 0 /*, uchar flags = 0*/)
		: UndoAction(ACT_INSERT /*, flags*/)
		, m_pos(pos)
		, m_size(size)
		, m_ix(0)
		, m_prohibitMerge(false)
		{}
	~UndoActionInsert() {}
};
struct UndoActionDelete : public UndoAction
{
	pos_t	m_pos;		//	挿入・削除位置
	int		m_size;		//	挿入・削除文字数（for UTF16）
	int		m_ix;		//	挿入・削除文字列のUndoバッファインデックス
	int		m_ln;		//	pos の行番号 0 org
	int		m_lc;		//	削除行数
	//int		m_mdfyIX;	//	行情報保存位置
	//std::vector<bool>	m_mdfyFlags;
public:
	UndoActionDelete(pos_t pos = 0, int size = 0, int ix = 0, bool BS = false)
		: UndoAction(ACT_DELETE, (BS ? FLAG_BS : 0))
		, m_pos(pos)
		, m_size(size)
		, m_ix(ix)
		{}
	~UndoActionDelete() {}
};
struct UndoActionReplace : public UndoAction
{
	pos_t	m_pos;		//	挿入・削除位置
	int		m_ln;		//	pos の行番号 0 org
	int		m_sizeIns;	//	挿入文字数（for UTF16）
	int		m_ixIns;	//	挿入文字列のUndoバッファインデックス
	int		m_lcIns;	//	挿入行数
	int		m_sizeDel;	//	削除文字数（for UTF16）
	int		m_ixDel;	//	削除文字列のUndoバッファインデックス
	int		m_lcDel;	//	削除行数
public:
	UndoActionReplace(pos_t pos = 0, int ln = 0,
						int sizeIns = 0, int ixIns = 0, int lcIns = 0,
						int sizeDel = 0, int ixDel = 0, int lcDel = 0)
		: UndoAction(ACT_REPLACE /*, flags*/)
		, m_pos(pos)
		, m_ln(ln)
		, m_sizeIns(sizeIns)
		, m_ixIns(ixIns)
		, m_sizeDel(sizeDel)
		, m_ixDel(ixDel)
		, m_lcDel(lcDel)
		{}
	~UndoActionReplace() {}
};

class UndoMgr
{
public:
	UndoMgr(Buffer* = nullptr);
	~UndoMgr();

public:
	bool	canUndo() const { return m_cur != 0; }
	bool	canRedo() const { return (size_t)m_cur < m_stack.size(); }
	int		undoIndex() const { return m_cur; }
	bool	isBlockOpened() const { return m_blockLevel != 0; }
	//bool	isBlockOpened() const { return m_blockOpened; }

public:
	void	init();
	int		undo();
	int		redo();
	bool	push_back(UndoAction *);
	bool	push_back_delText(pos_t, int, bool BS, int ln);
	bool	push_back_insText(pos_t, int, int ln);
	UndoActionReplace	*push_back_repText(pos_t, int dsz, int isz, int ln);
	void	setLcIns(int);
	void	openBlock();
	void	closeBlock();
	void	closeAllBlock();
	//void	resetModified();	//	現状態を非モディファイ状態とする（保存時にコールされる）
	void	onSaved();			//	現状態を非モディファイ状態とする（保存時にコールされる）
	void	prohibitMergeUndo();			//	挿入マージ禁止

protected:
	UndoActionInsert	*newActInsert();
	UndoActionDelete	*newActDelete();
	UndoActionReplace	*newActReplace();

//private:
public:
	Buffer	*m_buffer;		//	編集バッファ
	int		m_cur;			//	m_stack 現在位置
	int		m_savePointCur;		//	モディファイ状態の場所（保存ポイント）
	int		m_lastDelTextSize;		//	最後の削除文字長
	int		m_lastInsTextSize;
	int		m_blockLevel;
	//bool	m_blockOpened;
	bool	m_actionPushed;			//	オープン中にアクションが追加された
	std::vector<UndoAction *>	m_stack;
	//std::vector<wchar_t>		m_delText;		//	削除文字列
	//std::vector<wchar_t>		m_insText;		//	挿入文字列
	QString		m_delText;		//	削除文字列
	QString		m_insText;		//	挿入文字列

	int		m_actInsPoolSize;
	int		m_actDelPoolSize;
	int		m_actRepPoolSize;
	std::vector<UndoActionInsert *>	m_actInsPool;
	std::vector<UndoActionDelete *>	m_actDelPool;
	std::vector<UndoActionReplace *>	m_actRepPool;

};

