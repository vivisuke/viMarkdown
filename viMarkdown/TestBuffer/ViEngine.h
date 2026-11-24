#pragma once

#include "TextDocument.h"
#include "TextCursor.h"

#include <QChar>
#include <QString>
#include <QStringList>
#include <functional>

class QKeyEvent;
class TextEdit;

enum class ViMode {
	Normal = 0,
	Insert,
	Visual,
	VisualLine,
	Replace,
};

class ViEngine
{
public:
	struct Callbacks {
		std::function<void(const QString &)> showStatus;
		std::function<void(QChar)> openCmdLine;	//	':' / '/' / '?'
		std::function<void(bool)> requestQuit;	//	force
		std::function<bool()> requestSave;
	};

	ViEngine() = default;

	void setEnabled(bool enabled);
	bool isEnabled() const { return m_enabled; }

	ViMode mode() const { return m_mode; }
	void setMode(ViMode m, TextEdit *edit = nullptr);
	QString modeLabel() const;
	TextDocument::pos_t visualAnchor() const { return m_visualAnchor; }
	TextDocument::pos_t visualCursor() const { return m_visualCursor; }

	void setCallbacks(Callbacks cb) { m_cb = std::move(cb); }

	//	キーイベントを処理したら true（TextEdit 側の通常処理をスキップ）
	bool handleKeyPress(TextEdit *edit, QKeyEvent *event);

	//	検索（'/' / '?' コマンドライン確定時・'*' / '#' / 'n' / 'N'）
	bool search(TextEdit *edit, const QString &pattern, bool forward, int count = 1);
	bool searchNext(TextEdit *edit, bool sameDir, int count = 1);

	//	ex コマンド実行（':' 以降の文字列）
	struct ExCallbacks {
		std::function<void(const QString &)> showStatus;
		std::function<void(const QString &)> showOutput;
		std::function<bool(const QString &)> saveFile;		//	引数空なら上書き保存
		std::function<void(bool)> closeTab;					//	force
		std::function<void(const QString &)> openFile;		//	引数空ならダイアログ
	};
	void executeExCommand(TextEdit *edit, const QString &cmdLine, const ExCallbacks &cb);

private:
	void resetPending();
	void clampCursorInNormal(TextEdit *edit);
	int effectiveCount() const;
	bool computeMotionTarget(TextEdit *edit, QChar key, bool ctrl, int count,
	                         bool forOperator, QChar opChar,
	                         TextDocument::pos_t &outPos, bool &outLinewise, bool &outInclusive);
	bool applyOperatorRange(TextEdit *edit, QChar op, TextDocument::pos_t p1, TextDocument::pos_t p2,
	                        bool linewise, bool inclusive);
	void updateVisualSelection(TextEdit *edit);
	bool doPut(TextEdit *edit, bool before, int count);
	void notifyMode(TextEdit *edit);

	bool m_enabled = false;
	ViMode m_mode = ViMode::Normal;
	int m_repeatCount = 0;
	QChar m_operator;			//	'd', 'c', 'y', '>', '<'
	QChar m_prefix;				//	'g', 'r', 'f', 'F', 't', 'T', 'Z'
	QChar m_lastFindChar;
	QChar m_lastFindCmd;		//	'f', 'F', 't', 'T'
	TextDocument::pos_t m_visualAnchor = 0;
	TextDocument::pos_t m_visualCursor = 0;
	bool m_changeUndoOpen = false;
	QString m_yankBuf;
	bool m_yankLinewise = false;
	QString m_lastSearchPat;
	bool m_lastSearchForward = true;
	Callbacks m_cb;
};
