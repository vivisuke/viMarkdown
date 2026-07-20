#include "UndoMgr.h"

UndoMgr::UndoMgr(Buffer *buffer)
	: m_buffer(buffer)
	, m_blockLevel(0)
	, m_actInsPoolSize(0)
	, m_actDelPoolSize(0)
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

