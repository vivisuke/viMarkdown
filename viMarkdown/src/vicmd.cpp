#include <QTextDocument>
#include <QTextCursor>
#include <QTextBlock>
#include <QLineEdit>
#include <QLabel>
#include <QComboBox>
#include <QStatusBar>
#include <QRegularExpression>
#include <QDir>
#include <QApplication>
#include <QKeyEvent>
#include "MainWindow.h"
#include "ui_MainWindow.h"
#include "DocWidget.h"
#include "MarkdownEditor.h"
#include "MarkdownPreview.h"
#include "UndoMgr.h"

#ifdef	Q_OS_WIN
#include <windows.h>
#include <imm.h>
#endif

extern Global g;
extern ViStatus gvi;
extern CharType getCharType(QChar ch);

uchar blockType(const QTextBlock &block);
void setBlockType(QTextBlock block, uchar type);
bool blockFolded(const QTextBlock &block);
void setBlockFolded(QTextBlock &block, bool folded);
void resetBlockGFlag(QTextDocument* doc) {
	QTextBlock block = doc->firstBlock();
	while( block.isValid() ) {
		int us = block.userState();
		if( us != -1 )
			block.setUserState(us & ~BLOCK_GFLAG);
		block = block.next();
	}
}
void setBlockGFlag(QTextBlock &block) {
	int us = block.userState();
	if( us == -1 ) us = BT_DEFAULT;
	block.setUserState(us | BLOCK_GFLAG);
}
bool blockGFlag(const QTextBlock &block) {
	int us = block.userState();
	return us != -1 && (us & BLOCK_GFLAG) != 0;
}
int getRepeatCount() {
	if( gvi.m_repeatCount == 0 ) return 1;
	return gvi.m_repeatCount;
}
//	cw 専用：カーソル位置から変更対象の末尾へ移動する（#1272, #1273）
//	単語上であればその単語（同種文字の連続）の末尾、空白上であれば連続する空白の末尾（次単語の手前）まで。
//	改行の上にある場合は false を返し、従来の ce としての処理に任せる
bool clampCwToCurWordEnd(QTextCursor& cursor) {
	QTextDocument *doc = cursor.document();
	const int end = doc->characterCount();	//	-1 しない（moveToNextWordEnd と同様）
	int pos = cursor.position();
	if( pos >= end - 1 ) return false;
	CharType ct = getCharType(doc->characterAt(pos));
	if( ct == Type_NewLine ) return false;
	if( ct == Type_Space ) {
		//	空白上：連続する空白の末尾まで（次単語は消さない、#1273）
		while( pos + 1 < end && getCharType(doc->characterAt(pos + 1)) == Type_Space )
			++pos;
		cursor.setPosition(pos, QTextCursor::KeepAnchor);
		return true;
	}
	while( pos + 1 < end && getCharType(doc->characterAt(pos + 1)) == ct )
		++pos;
	cursor.setPosition(pos, QTextCursor::KeepAnchor);
	return true;
}
bool isSpaceChar(QChar ch) {
	return ch == u' ' || ch == u'\t' || ch == u'\n'|| ch.unicode() == 0x2029;	//	0x2029: 改行コード
}
bool isNewline(QChar ch) {
	return ch == u'\n'|| ch.unicode() == 0x2029;	//	0x2029: 改行コード
}
bool clampCwToCurWORDEnd(QTextCursor& cursor) {
	QTextDocument *doc = cursor.document();
	const int end = doc->characterCount();	//	-1 しない（moveToNextWordEnd と同様）
	int pos = cursor.position();
	if( pos >= end - 1 ) return false;
	if( isNewline(doc->characterAt(pos)) ) return false;
	bool onSpace = isSpaceChar(doc->characterAt(pos));
	while( pos + 1 < end && !isNewline(doc->characterAt(pos + 1)) && isSpaceChar(doc->characterAt(pos + 1)) == onSpace )
		++pos;
	cursor.setPosition(pos, QTextCursor::KeepAnchor);
	return true;
}
void moveLeftIfAtEol(QTextCursor& cursor, QTextCursor::MoveMode moveMode = QTextCursor::MoveAnchor) {
	QTextBlock block = cursor.block();
	if( cursor.position() != block.position() && cursor.position() == block.position() + block.text().size() )
		cursor.movePosition(QTextCursor::Left, moveMode);
}
void hat(QTextCursor& cursor, QTextCursor::MoveMode moveMode = QTextCursor::MoveAnchor) {
	cursor.movePosition(QTextCursor::StartOfBlock, moveMode);
	for(;;) {
		QChar ch = cursor.document()->characterAt(cursor.position());
		if( ch != u' ' && ch != u'\t' ) break;
		cursor.movePosition(QTextCursor::Right, moveMode);	//	空白だけの行で行末まで行っちゃうけど、まあいいか・・・
	}
}
void do_r(QChar ch, QTextCursor& cursor, int rcnt) {
	QTextBlock block = cursor.block();
    int available = block.position() + block.text().size() - cursor.position();
    if( rcnt > available ) return;   // 行末を超える場合は無視（viの仕様）
	cursor.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor, rcnt);
	assert( gvi.m_editor != nullptr );
	gvi.m_editor->do_insertText(cursor, QString(rcnt, ch));
	if( ch != u'\n' )
		cursor.movePosition(QTextCursor::Left);
	gvi.m_isEditCommand = true;
}
void do_openline(QTextCursor& cursor, bool before) {
	assert( gvi.m_editor != nullptr );
	gvi.m_autotext.clear();
	bool removed;
	if( before ) {
		cursor.movePosition(QTextCursor::StartOfBlock);
		//cursor.insertText("\n");
		if( gvi.m_editor != nullptr ) {
			auto atxt = gvi.m_autotext = gvi.m_editor->autoIndentText(cursor, removed);
			//if( !atxt.isEmpty() )
			gvi.m_editor->do_insertText(cursor, atxt + "\n");
			cursor.movePosition(QTextCursor::PreviousBlock);
			cursor.movePosition(QTextCursor::Right, QTextCursor::MoveAnchor, atxt.size());
		}
	} else {
		cursor.setPosition(cursor.block().position() + cursor.block().text().size());
		//cursor.insertText("\n");
		if( gvi.m_editor != nullptr ) {
			auto atxt = gvi.m_autotext = gvi.m_editor->autoIndentText(cursor, removed);
			//if( !atxt.isEmpty() )
			gvi.m_editor->do_insertText(cursor, "\n" + atxt);
		}
	}
}
void do_swap_case(QTextCursor& cursor, int rcnt) {
	QString text;
	QTextBlock block = cursor.block();
	const QString buf = block.text();
	int ix = cursor.position() - block.position();
	int ix9 = qMin(ix + rcnt, buf.size());
	if( ix9 == ix ) return;
	for(int i = ix; i < ix9; ++i) {
		if( buf[i].isLower() ) text += buf[i].toUpper();
		else if( buf[i].isUpper() ) text += buf[i].toLower();
		else text += buf[i];
	}
	cursor.setPosition(block.position() + ix9, QTextCursor::KeepAnchor);
	//cursor.insertText(text);
	gvi.m_editor->do_insertText(cursor, text);
}
void save_preffered_x() {
	if( gvi.m_editor != nullptr ) {
	}
}
void MainWindow::do_cdy_moved(QTextCursor& cursor) {
	assert( gvi.m_editor != nullptr );
	if( gvi.m_operator == ' ' ) return;
#if 0
	if( cursor.hasSelection() && gvi.m_linewiseMoved ) {
		int startPos = cursor.selectionStart();
        int endPos = cursor.selectionEnd();
		QTextCursor cStart = cursor;
        cStart.setPosition(startPos);
        cStart.movePosition(QTextCursor::StartOfBlock);
        QTextCursor cEnd = cursor;
        cEnd.setPosition(endPos);
        if (!cEnd.movePosition(QTextCursor::NextBlock))
            cEnd.movePosition(QTextCursor::EndOfBlock);
        if (cursor.position() >= cursor.anchor()) {
            // 上から下への選択
            cursor.setPosition(cStart.position());
            cursor.setPosition(cEnd.position(), QTextCursor::KeepAnchor);
        } else {
            // 下から上への選択
            cursor.setPosition(cEnd.position());
            cursor.setPosition(cStart.position(), QTextCursor::KeepAnchor);
        }
	}
#endif
	if( !gvi.m_redoing && gvi.m_operator != u'y' )
		gvi.m_lastEditCommand = gvi.m_operator + gvi.m_lastMoveCommand;
	if( gvi.m_operator == 'c' ) {		//	c<move>
		if( cursor.hasSelection() ) {
			//cursor.deleteChar();
			if( gvi.m_editor != nullptr ) {
				gvi.m_editor->openUndoBlock();
				gvi.m_yankBuffer = cursor.selectedText();
				gvi.m_editor->do_deleteText(cursor);
			}
		}
		gvi.m_currentMode = ViMode::Insert;
		//gvi.m_viCmdMode = false;
	} else if( gvi.m_operator == 'd' || gvi.m_operator == 'y' ) {	//	d<move> or y<move>
		if( cursor.hasSelection() ) {
			gvi.m_linewiseYanked = gvi.m_linewiseMoved;
			if( gvi.m_linewiseMoved ) {
				auto start = cursor.selectionStart();
				auto end = cursor.selectionEnd();
				cursor.setPosition(start);
				cursor.movePosition(QTextCursor::StartOfBlock);
				cursor.setPosition(end, QTextCursor::KeepAnchor);
				if( !cursor.movePosition(QTextCursor::NextBlock, QTextCursor::KeepAnchor) )
					cursor.movePosition(QTextCursor::End, QTextCursor::KeepAnchor);	//	最終行には次ブロックが無いため、文書末尾まで広げる
				//cursor.movePosition(QTextCursor::NextBlock, QTextCursor::KeepAnchor);
			}
			gvi.m_yankBuffer = cursor.selectedText();
			if( gvi.m_operator == 'd' ) {
				if(gvi.m_editor != nullptr)
					gvi.m_editor->do_deleteText(cursor);
				//cursor.deleteChar();
				if( !gvi.m_linewiseMoved )
					moveLeftIfAtEol(cursor);
				else {
					if( !cursor.block().next().isValid() ) {	//	最終行にいる場合
						cursor.movePosition(QTextCursor::Up);
						hat(cursor);
					}
				}
			} else {		//	y<move>
				statusBar()->showMessage(QString("%1 charactors yanked.").arg(gvi.m_yankBuffer.size()), 5000);
				cursor.setPosition(qMin(cursor.anchor(), cursor.position()));
				//cursor.clearSelection();
			}
		}
	}
}
bool do_fFtT(QTextCursor& cursor, QChar cmd, QChar ch, int rcnt, bool isRepeat = false) {
	gvi.m_linewiseMoved = false;	//	f/F/t/T は文字単位モーション（#1373）
	QTextBlock block = cursor.block();
	const QString buf = block.text();
	int ix = cursor.position() - block.position();		//	ブロック内インデックス
	int ix0 = ix, ix2 = -1;
	if( cmd== 'f' || cmd== 't' ) {		//	順方向検索
		if( isRepeat && cmd == 't' && ix + 1 < buf.size() && buf[ix + 1] == ch )
			++ix;
		for(int i = 0; i != rcnt; ++i) {
			ix2 = buf.indexOf(ch, ix+1);
			if( ix2 < 0 ) break;	//	未発見
			ix = ix2;
		}
		if( cmd== 't' ) --ix;
		if( ix2 < 0 ) ix = ix0;		//	未発見の場合
	} else if( ix != 0 ) {		//	F T 逆方向検索
		if( isRepeat && cmd == 'T' && ix - 1 >= 0 && buf[ix - 1] == ch )
			--ix;
		for(int i = 0; i != rcnt; ++i) {
			if( ix <= 0 ) { ix2 = -1; break; }
			ix2 = buf.lastIndexOf(ch, ix-1);
			if( ix2 < 0 ) break;	//	未発見
			ix = ix2;
		}
		if( cmd== 'T' ) ++ix;
		if( ix2 < 0 ) ix = ix0;		//	未発見の場合
	}
	if( !isRepeat && ix2 >= 0 ) {
		gvi.m_last_fFtT = cmd;
		gvi.m_last_fFtT_char = ch;
	}
	//	順方向 't' で対象が直後の場合、着地位置は変わらないが演算子範囲は1文字ある（#1376）
	if( ix != ix0 || (ix2 >= 0 && cmd == 't' && gvi.m_operator != ' ') ) {
		if( gvi.m_operator == ' ' )
			cursor.setPosition(block.position() + ix);
		else {
			cursor.setPosition(block.position() + ix0);
			//	順方向は対象文字を含むため終端は ix+1、逆方向はカーソル文字を含まないため終端は ix（#1374）
			cursor.setPosition(block.position() + ix + (cmd == 'f' || cmd == 't' ? 1 : 0), QTextCursor::KeepAnchor);
		}
		return true;
	} else
		return false;
}
void do_vi_change_line(QTextCursor& cursor, int &rcnt) {		//	for S, cc
	cursor.beginEditBlock();	//	１文字削除とその後の文字挿入を１回でundo可能にするため
	gvi.m_editor->openUndoBlock();
	//g.m_editBlockOpen = true;
	hat(cursor);
	if( rcnt > 1 )
		cursor.movePosition(QTextCursor::NextBlock, QTextCursor::KeepAnchor, rcnt - 1);
	cursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
	if( cursor.hasSelection() ) {
		gvi.m_yankBuffer = cursor.selectedText();
		//cursor.deleteChar();
		gvi.m_editor->do_deleteText(cursor);
	}
	cursor.endEditBlock();
	//gvi.m_joinEditBlock = true;
	//gvi.m_repeatCount = 0;
	//gvi.m_insRepCount = 1;
	gvi.m_currentMode = ViMode::Insert;
	//gvi.m_viCmdMode = false;
	rcnt = 1;
}
void MainWindow::do_vi_insert(QChar cmd, QTextCursor& cursor, int rcnt) {
	assert( gvi.m_editor != nullptr );
	QTextBlock block = cursor.block();
	int eolpos = block.position() + block.text().size();
	switch( cmd.unicode() ) {
	case 'a':
		if( cursor.position() < block.position() + block.text().size() )	//	行末でない場合
			cursor.movePosition(QTextCursor::Right);
		break;
	case 'i':
		break;
	case 'A':
		cursor.setPosition(block.position() + block.text().size());
		break;
	case 'I':
		cursor.movePosition(QTextCursor::StartOfBlock);
		for(;;) {
			QChar ch = cursor.document()->characterAt(cursor.position());
			if( ch != u' ' && ch != u'\t' ) break;
			cursor.movePosition(QTextCursor::Right);
		}
		break;
	case 'S':		//	行を消して挿入モードへ
		do_vi_change_line(cursor, rcnt);
		break;
	case 's':
		//cursor.beginEditBlock();	//	文字削除とその後の文字挿入を１回でundo可能にするため
		//g.m_editBlockOpen = true;
		rcnt = qMin(rcnt, eolpos - cursor.position());
		cursor.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor, rcnt);
		if( gvi.m_editor != nullptr )
			gvi.m_editor->openUndoBlock();
		if( cursor.hasSelection() ) {
			gvi.m_yankBuffer = cursor.selectedText();
			if( gvi.m_editor != nullptr )
				gvi.m_editor->do_deleteText(cursor);
			//cursor.deleteChar();
			//gvi.m_joinEditBlock = true;
		}
		rcnt = 1;
		//cursor.endEditBlock();
		break;
	case 'O':
		if( gvi.m_editor != nullptr )
			gvi.m_editor->openUndoBlock();
		do_openline(cursor, true);
		//cursor.beginEditBlock();
		//g.m_editBlockOpen = true;
		//do_openline(cursor, true);
		//cursor.movePosition(QTextCursor::StartOfBlock);
		//cursor.insertText("\n");
		//cursor.movePosition(QTextCursor::PreviousBlock);
		//cursor.endEditBlock();
		//gvi.m_joinEditBlock = true;
		break;
	case 'o':
		//cursor.beginEditBlock();
		//g.m_editBlockOpen = true;
		if( gvi.m_editor != nullptr )
			gvi.m_editor->openUndoBlock();
#if 1
		do_openline(cursor, false);
#else
		//cursor.movePosition(QTextCursor::EndOfBlock);
		//insertEnter();
#endif
		//cursor.endEditBlock();
		//gvi.m_joinEditBlock = true;
		break;
	case 'C':		//	カーソル位置から行末まで削除して挿入モードへ遷移
		if( gvi.m_editor != nullptr )
			gvi.m_editor->openUndoBlock();
		cursor.beginEditBlock();
		//g.m_editBlockOpen = true;
		if( rcnt > 1 ) {
			cursor.movePosition(QTextCursor::Down, QTextCursor::KeepAnchor, rcnt - 1);
			rcnt = 1;
		}
		cursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
		if( cursor.hasSelection() ) {
			gvi.m_yankBuffer = cursor.selectedText();
			gvi.m_linewiseYanked = false;
			//cursor.deleteChar();
			if( gvi.m_editor != nullptr )
				gvi.m_editor->do_deleteText(cursor);
			else
				cursor.deleteChar();
		}
		cursor.endEditBlock();
		//gvi.m_joinEditBlock = true;
		break;
	default:
		return;
	}
	gvi.m_insRepCount = rcnt;
	gvi.m_isEditCommand = true;
	gvi.m_currentMode = ViMode::Insert;
	//gvi.m_viCmdMode = false;
}
static void selectVisualLineRange(QTextCursor &cursor, int anchorBlockNum) {
	QTextDocument *doc = cursor.document();
	int curBn = cursor.block().blockNumber();
	int startBn = qMin(anchorBlockNum, curBn);
	int endBn = qMax(anchorBlockNum, curBn);
	QTextBlock startBlock = doc->findBlockByNumber(startBn);
	QTextBlock endBlock = doc->findBlockByNumber(endBn);
	if( !startBlock.isValid() || !endBlock.isValid() ) return;
	cursor.setPosition(startBlock.position());
	if( endBlock.next().isValid() ) {
		cursor.setPosition(endBlock.next().position(), QTextCursor::KeepAnchor);
	} else {
		cursor.movePosition(QTextCursor::End, QTextCursor::KeepAnchor);
	}
}
void MainWindow::do_vi_delete(QChar cmd, QTextCursor& cursor, int rcnt) {		//	x X D
	assert( gvi.m_editor != nullptr );
	if( gvi.m_vMode == u'v' ) {
		switch( cmd.unicode() ) {
		case 'x':
		case 'X':
			if( gvi.m_vAnchor <= cursor.position() ) {
				cursor.movePosition(QTextCursor::Right);
				cursor.setPosition(gvi.m_vAnchor, QTextCursor::KeepAnchor);
			} else {
				cursor.setPosition(gvi.m_vAnchor, QTextCursor::KeepAnchor);
				cursor.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor);
			}
			gvi.m_yankBuffer = cursor.selectedText();
			gvi.m_linewiseYanked = false;
			if( gvi.m_editor != nullptr )
				gvi.m_editor->do_deleteText(cursor);
			else
				cursor.deleteChar();
			moveLeftIfAtEol(cursor);
			gvi.m_vMode = u' ';
			break;
		}
		return;
	}
	if( gvi.m_vMode == u'V' ) {
		switch( cmd.unicode() ) {
		case 'x':
		case 'X':
			selectVisualLineRange(cursor, gvi.m_vAnchorBlock);
			gvi.m_yankBuffer = cursor.selectedText();
			gvi.m_linewiseYanked = true;
			if( gvi.m_editor != nullptr )
				gvi.m_editor->do_deleteText(cursor);
			else
				cursor.deleteChar();
			gvi.m_vMode = u' ';
			if( gvi.m_editor != nullptr )
				gvi.m_editor->highlightVText(cursor);
			break;
		}
		return;
	}
	QTextBlock block = cursor.block();
	switch( cmd.unicode() ) {
	case 'x':
		if( !cursor.hasSelection() ) {
			while( --rcnt >= 0 && cursor.position() < block.position() + block.text().size() )	//	行末でない場合
				cursor.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor);
		}
		if( cursor.hasSelection() ) {
			gvi.m_yankBuffer = cursor.selectedText();
			gvi.m_linewiseYanked = false;
			if( gvi.m_editor != nullptr )
				gvi.m_editor->do_deleteText(cursor);
			else
				cursor.deleteChar();
			moveLeftIfAtEol(cursor);
		}
		break;
	case 'X':
		while( --rcnt >= 0 && cursor.position() > block.position() )	//	行頭でない場合
			cursor.movePosition(QTextCursor::Left, QTextCursor::KeepAnchor);
		if( cursor.hasSelection() ) {
			gvi.m_yankBuffer = cursor.selectedText();
			gvi.m_linewiseYanked = false;
			if( gvi.m_editor != nullptr )
				gvi.m_editor->do_deleteText(cursor);
			else
				cursor.deleteChar();
		}
		break;
	case 'D':
		if( rcnt > 1 )
			cursor.movePosition(QTextCursor::Down, QTextCursor::KeepAnchor, rcnt - 1);
		cursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
		if( cursor.hasSelection() ) {
			gvi.m_yankBuffer = cursor.selectedText();
			gvi.m_linewiseYanked = false;
			if( gvi.m_editor != nullptr )
				gvi.m_editor->do_deleteText(cursor);
			else
				cursor.deleteChar();
			moveLeftIfAtEol(cursor);
		}
		break;
	default:
		return;
	}
	gvi.m_vMode = u' ';
	gvi.m_isEditCommand = true;
}
void do_yank_line(QTextCursor& cursor, int rcnt) {
#if 1
	auto pos = cursor.position();
    // 1. カレントブロックの先頭へ移動
    cursor.movePosition(QTextCursor::StartOfBlock);
    // 2. rcnt 分だけブロックを進める（末尾に達した場合はそこまで）
    bool moved = false;
    for (int i = 0; i < rcnt; ++i) {
        if (cursor.movePosition(QTextCursor::NextBlock, QTextCursor::KeepAnchor)) {
            moved = true;
        } else {
            // NextBlock が失敗した＝最終ブロックに達している
            // そのブロックの末尾（ドキュメントの本当の末尾）まで選択を伸ばす
            cursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
            break;
        }
    }
    // 3. テキストを取得（Qt はブロック区切りに U+2029 を使うため \n に置換）
    QString text = cursor.selectedText();
    text.replace(QChar::ParagraphSeparator, '\n');
    // 4. 行ヤンク(linewise)なので、必ず末尾を '\n' で終わらせる
    // （最終行を含んでいる場合、selectedText() には末尾改行が含まれないため）
    if (!text.endsWith('\n')) {
        text += '\n';
    }
    // 5. バッファに格納
    gvi.m_yankBuffer = text;
    gvi.m_linewiseYanked = true;
    // 6. 選択を解除して元のカーソル位置に戻す
    cursor.clearSelection();
    cursor.setPosition(pos);
#else
	auto pos = cursor.position();
	cursor.movePosition(QTextCursor::StartOfBlock);
	cursor.movePosition(QTextCursor::NextBlock, QTextCursor::KeepAnchor, rcnt);
	if( cursor.hasSelection() ) {
		gvi.m_yankBuffer = cursor.selectedText();
		//statusBar()->showMessage("%1 charactors yanked.", 5000).arg(gvi.m_yankBuffer.size());
		gvi.m_linewiseYanked = true;
		cursor.clearSelection();
	}
	cursor.setPosition(pos);
#endif
}
int heading_level(QTextBlock block) {
	QString text = block.text().trimmed();
	int i = 0;
	while( i < text.size() && text[i] == u'#' ) ++i;
	if( i != 0 ) return i;
	auto bt = blockType(block);
	if( bt == BT_LIST || bt == BT_CHECKBOX || bt == BT_NUMLIST ) {
		text = block.text();
		while( i < text.size() && text[i] == u' ' ) ++i;
		i += 10;
	}
	return i;
}
bool is_folded(QTextBlock block);
bool is_foldable(QTextBlock block);
void do_fold(QTextBlock block /*, QTextDocument *doc*/) {	//	block は見出し行
	if( !block.isValid() ) return;
	int lvl = heading_level(block);
	if( lvl == 0 ) return;
	setBlockFolded(block, true);	//	折り畳み状態フラグON
	QTextDocument *doc = (QTextDocument*)block.document();
	while( (block = block.next()).isValid() ) {
		//if( blockType(block) == BT_HEADING && heading_level(block) <= lvl )
		//	break;
		int l2 = heading_level(block);
		if( l2 != 0 && l2 <= lvl )
			break;
        block.setVisible(false); // ブロックを非表示に設定
        doc->markContentsDirty(block.position(), block.length());
	}
}
void do_unfold(QTextBlock block /*, QTextDocument *doc*/) {	//	block は見出し行
	if( !is_folded(block) ) return;
	setBlockFolded(block, false);	//	折り畳み状態フラグOFF
	QTextDocument *doc = (QTextDocument*)block.document();
	while( (block = block.next()).isValid() && !block.isVisible() ) {
        block.setVisible(true); // ブロックを表示
        doc->markContentsDirty(block.position(), block.length());
        if( blockType(block) == BT_HEADING && blockFolded(block) ) {
            int child_lvl = heading_level(block);
            // 子見出しと同等以上のレベルの見出しが現れるまで、ポインタだけを進めてスキップ
            while( block.next().isValid() ) {
                QTextBlock nextBlock = block.next();
                if( blockType(nextBlock) == BT_HEADING && heading_level(nextBlock) <= child_lvl ) {
                    break;
                }
                block = nextBlock; // 表示を切り替えず、ただポインタを進める
            }
        }
	}
}
void MainWindow::do_prefix_cmd(QChar cmd, QTextCursor& cursor, int rcnt, DocWidget* docWidget) {		//	{g z r [ ]} cmd
	QTextBlock block = cursor.block();
	QTextDocument *doc = cursor.document();
	switch( gvi.m_prefix.unicode() ) {
	case 'g':
		switch( cmd.unicode() ) {
		case 'g':		//	gg
			if( gvi.m_operator != ' ' ) {	//	演算子＋gg は行単位モーションとして処理（#1372）
				if( gvi.m_repeatCount == 0 ) {
					cursor.movePosition(QTextCursor::Start, QTextCursor::KeepAnchor);
				} else {
					if( !(block = doc->findBlockByNumber(gvi.m_repeatCount - 1)).isValid() ) {
						cursor.movePosition(QTextCursor::End, QTextCursor::KeepAnchor);
						if( cursor.block().text().isEmpty() )
							cursor.movePosition(QTextCursor::PreviousBlock, QTextCursor::KeepAnchor);
					} else
						cursor.setPosition(block.position(), QTextCursor::KeepAnchor);
				}
				hat(cursor, QTextCursor::KeepAnchor);
				gvi.m_linewiseMoved = true;
				gvi.m_lastMoveCommand = "gg";
				do_cdy_moved(cursor);
			} else if( gvi.m_repeatCount == 0 ) {
				cursor.movePosition(QTextCursor::Start);
			} else {
				if( !(block = doc->findBlockByNumber(gvi.m_repeatCount - 1)).isValid() ) {
					cursor.movePosition(QTextCursor::End);
					if( cursor.block().text().isEmpty() )
						cursor.movePosition(QTextCursor::PreviousBlock);
				} else
					cursor.setPosition(block.position());
			}
			if( gvi.m_operator == ' ' )
				hat(cursor);
			break;
		}
		break;
	case 'z':
		switch( cmd.unicode() ) {
		case '\n':
		case '\r':
		case 't': {		// zt: カーソル行を画面最上行にする（カーソル位置はそのまま）
			int blockNum = cursor.blockNumber();
			gvi.m_editor->verticalScrollBar()->setValue(blockNum);
    		if( cmd.unicode() == u'\n' || cmd.unicode() == u'\r' ) hat(cursor);
    		break;
		}
		case '-':
		case 'b': {		// zb: カーソル行を画面最下行にする（カーソル位置はそのまま）
			int blockNum = cursor.blockNumber();
		    auto editor = gvi.m_editor;
		    int lineH = qMax(1, editor->fontMetrics().height());
		    int visibleLines = editor->viewport()->height() / lineH;
		    editor->verticalScrollBar()->setValue(qMax(0, blockNum - visibleLines + 1));
    		if( cmd.unicode() == u'-' ) hat(cursor);
			break;
		}
		case '.':
		case 'z': {		// zz: カーソル行を画面中央行にする（カーソル位置はそのまま）
			auto editor = gvi.m_editor;
		    int blockNum = cursor.blockNumber();
		    int lineH = qMax(1, editor->fontMetrics().height());
		    int visibleLines = editor->viewport()->height() / lineH;
		    editor->verticalScrollBar()->setValue( qMax(0, blockNum - visibleLines / 2) );
    		if( cmd.unicode() == u'.' ) hat(cursor);
			break;
		}
		case 'a':		//	za
			if( docWidget->m_diffMode ) break;
			if (!block.isValid()) break;
			if( is_folded(block) ) {
				do_unfold(block);
				docWidget->m_editor->drawFoldLine(cursor, block);
			} else if( is_foldable(block) ) {
				do_fold(block);
				docWidget->m_editor->clearFoldLine();
			}
			onMDTextChanged();
			break;
		case 'c':		//	zc
			if( docWidget->m_diffMode ) break;
			if( !block.isValid() || !is_foldable(block) ) break;
			do_fold(block);
			docWidget->m_editor->clearFoldLine();
			onMDTextChanged();
			break;
		case 'o':		//	zo
			if( docWidget->m_diffMode ) break;
			do_unfold(block);
			docWidget->m_editor->drawFoldLine(cursor, block);
			onMDTextChanged();
			break;
		case 'M':		//	zM	すべて折り畳み
			onAction_FoldAll();
			break;
		case 'R':		//	zR	すべて展開
			onAction_UnfoldAll();
			break;
		}
		break;
	case 'Z':
		switch( cmd.unicode() ) {
		case 'Z':		//	ZZ
			if( do_save() )
				do_close();
			break;
		}
		break;
	case '[':
		switch( cmd.unicode() ) {
		case '[': {	//	[[
			QTextBlock block = cursor.block().previous();
			while (block.isValid()) {
				if (blockType(block) == BT_HEADING) {
					if (--rcnt == 0) {
						cursor.setPosition(block.position());
						break;
					}
				}
				block = block.previous();
			}
			break;
		}
		case 'c':		//	[c：前の差分ブロック先頭へ（diff モード時のみ）
			do_diffJump(cursor, rcnt, false, docWidget);
			break;
		}
		break;
	case ']':
		switch( cmd.unicode() ) {
		case ']': {	//	]]
			QTextBlock block = cursor.block().next();
			while (block.isValid()) {
				if (blockType(block) == BT_HEADING) {
					if (--rcnt == 0) {
						cursor.setPosition(block.position());
						break;
					}
				}
				block = block.next();
			}
			break;
		}
		case 'c':		//	]c：次の差分ブロック先頭へ（diff モード時のみ）
			do_diffJump(cursor, rcnt, true, docWidget);
			break;
		}
		break;
	}
}
void MainWindow::do_diffJump(QTextCursor& cursor, int rcnt, bool forward, DocWidget* docWidget) {		//	]c / [c：差分ブロック先頭へ移動
	if( docWidget == nullptr || !docWidget->m_diffMode ) return;
	MarkdownEditor *editor = docWidget->m_editor;
	if( docWidget->m_diffview != nullptr && cursor.document() == docWidget->m_diffview->document() )
		editor = docWidget->m_diffview;
	const auto &flgs = editor->diffFlags();
	auto isChanged = [&flgs](int bn) {
		return bn >= 0 && bn < (int)flgs.size() && flgs[bn] != EQU_LINE;
	};
	QTextDocument *doc = cursor.document();
	int cnt = qMax(1, rcnt);
	int target = -1;
	if( forward ) {
		for( int bn = cursor.blockNumber() + 1; bn < doc->blockCount() && cnt > 0; ++bn ) {
			if( isChanged(bn) && !isChanged(bn - 1) ) { target = bn; --cnt; }
		}
	} else {
		for( int bn = cursor.blockNumber() - 1; bn >= 0 && cnt > 0; --bn ) {
			if( isChanged(bn) && !isChanged(bn - 1) ) { target = bn; --cnt; }
		}
	}
	if( target >= 0 )
		cursor.setPosition(doc->findBlockByNumber(target).position());
}
bool MainWindow::do_cdy(QChar cmd, QTextCursor& cursor) {
	assert( gvi.m_editor != nullptr );
	if( gvi.m_vMode == u'v' || gvi.m_vMode == u'V' ) {
		if( gvi.m_vMode == u'v' ) {
			if( gvi.m_vAnchor <= cursor.position() ) {
				cursor.movePosition(QTextCursor::Right);
				cursor.setPosition(gvi.m_vAnchor, QTextCursor::KeepAnchor);
			} else {
				cursor.setPosition(gvi.m_vAnchor, QTextCursor::KeepAnchor);
				cursor.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor);
			}
			gvi.m_linewiseYanked = false;
		} else {
			selectVisualLineRange(cursor, gvi.m_vAnchorBlock);
			gvi.m_linewiseYanked = true;
		}
		gvi.m_vMode = u' ';
		if( cursor.hasSelection() ) {
			switch( cmd.unicode() ) {
			case 'c':	//	選択状態で c
				gvi.m_yankBuffer = cursor.selectedText();
				if( gvi.m_editor != nullptr ) {
					gvi.m_editor->openUndoBlock();
					gvi.m_editor->do_deleteText(cursor);
				}
				//gvi.m_viCmdMode = false;
				gvi.m_currentMode = ViMode::Insert;
				gvi.m_vMode = u' ';
				gvi.m_editor->highlightVText(cursor);
				return true;
			case 'd':	//	選択状態で d
				gvi.m_yankBuffer = cursor.selectedText();
				//cursor.deleteChar();
				if( gvi.m_editor != nullptr )
					gvi.m_editor->do_deleteText(cursor);
				//	選択が行末まで達していた場合、カーソルを最後の文字の上に置く（#1377）
				if( cursor.block().text().length() > 0 && cursor.positionInBlock() >= cursor.block().text().length() )
					cursor.movePosition(QTextCursor::Left);
				gvi.m_vMode = u' ';
				gvi.m_editor->highlightVText(cursor);
				return true;
			case 'y':	//	選択状態で y
				gvi.m_yankBuffer = cursor.selectedText();
				statusBar()->showMessage(QString("%1 charactors yanked.").arg(gvi.m_yankBuffer.size()), 5000);
				cursor.setPosition(cursor.selectionStart());
				cursor.clearSelection();
				gvi.m_vMode = u' ';
				gvi.m_editor->highlightVText(cursor);
				return true;
			}
		}
	}
	gvi.m_operator = cmd;
	if( gvi.m_repeatCount != 0 ) gvi.m_opCount = gvi.m_repeatCount;
	gvi.m_repeatCount = 0;
	return false;
}
bool MainWindow::do_vi_operator(QChar cmd, QTextCursor& cursor, int rcnt, DocWidget* docWidget) {		//	{c d y < >}<move>
	assert( gvi.m_editor != nullptr );
	if( gvi.m_operator == ' ' ) {
		if( cursor.hasSelection() ) {
			switch( cmd.unicode() ) {
			case 'c':
				gvi.m_yankBuffer = cursor.selectedText();
				cursor.beginEditBlock();
				cursor.deleteChar();
				cursor.endEditBlock();
				//gvi.m_joinEditBlock = true;
				//gvi.m_viCmdMode = false;
				gvi.m_currentMode = ViMode::Insert;
				return true;
			case 'd':
				gvi.m_yankBuffer = cursor.selectedText();
				cursor.deleteChar();
				return true;
			case 'y':
				gvi.m_yankBuffer = cursor.selectedText();
				statusBar()->showMessage(QString("%1 charactors yanked.").arg(gvi.m_yankBuffer.size()), 5000);
				cursor.setPosition(cursor.selectionStart());
				return true;
			}
		}
		gvi.m_operator = cmd;
		if( gvi.m_repeatCount != 0 )
			gvi.m_opCount = gvi.m_repeatCount;
		gvi.m_repeatCount = 0;
		return false;
	}
	QTextDocument *doc = cursor.document();
	QTextBlock block = cursor.block();
	if( gvi.m_operator == cmd ) {		//	cc dd yy gg zz << >> の場合
		switch( cmd.unicode() ) {
		case 'c':	//	cc
			do_vi_change_line(cursor, rcnt);
			break;
		case 'd':	//	dd
#if 1
			cursor.movePosition(QTextCursor::StartOfBlock);
		    if (cursor.blockNumber() + rcnt >= doc->blockCount()) {    // 最終行を含む削除：行頭から EndOfBlock まで選択
		        // ---ただし前の行の改行ごと削除するため、1つ前の行末から選択？？？---
		        if (cursor.blockNumber() > 0) {
		            //##cursor.movePosition(QTextCursor::PreviousBlock);
		            //##cursor.movePosition(QTextCursor::EndOfBlock);
		            cursor.movePosition(QTextCursor::End, QTextCursor::KeepAnchor);
		        } else {
		            // ドキュメント全体が対象
		            cursor.movePosition(QTextCursor::End, QTextCursor::KeepAnchor);
		        }
		    } else {
		        cursor.movePosition(QTextCursor::NextBlock, QTextCursor::KeepAnchor, rcnt);
		        while( !cursor.block().isVisible() && cursor.movePosition(QTextCursor::NextBlock, QTextCursor::KeepAnchor) ) {}
		        if( !cursor.block().isVisible() )
			        cursor.movePosition(QTextCursor::End, QTextCursor::KeepAnchor);
		    }
#else
			cursor.movePosition(QTextCursor::StartOfBlock);
			cursor.movePosition(QTextCursor::NextBlock, QTextCursor::KeepAnchor, rcnt);
#endif
			if( cursor.hasSelection() ) {
				gvi.m_yankBuffer = cursor.selectedText();
				gvi.m_linewiseYanked = true;
				//cursor.deleteChar();
				if( gvi.m_editor != nullptr ) {
					gvi.m_editor->do_deleteText(cursor);
					gvi.m_editor->setActionFlag(UndoAction::FLAG_LINEWISE);
				}
				gvi.m_isEditCommand = true;
				int pos = cursor.position();
				int sz = gvi.m_editor->document()->characterCount();
				int lastPos = gvi.m_editor->document()->characterCount() - 1;
				if( cursor.position() >= lastPos ) {
					if( lastPos - 1 >= 0 && isNewline(gvi.m_editor->document()->characterAt(lastPos-1)) ) {
						cursor.movePosition(QTextCursor::PreviousBlock);
					} else {
						hat(cursor);
					}
				}
			}
			break;
		case 'y':	//	yy
			do_yank_line(cursor, rcnt);
			statusBar()->showMessage(QString("%1 charactors yanked.").arg(gvi.m_yankBuffer.size()), 5000);
			break;
		//case 'g':	//	gg
		//	cursor.movePosition(QTextCursor::Start);
		//	break;
#if 0
		case ']': {	//	]]
			QTextBlock block = cursor.block().next();
			while (block.isValid()) {
				if (blockType(block) == BT_HEADING) {
					if (--rcnt == 0) {
						cursor.setPosition(block.position());
						break;
					}
				}
				block = block.next();
			}
			break;
		}
		case '[': {	//	[[
			QTextBlock block = cursor.block().previous();
			while (block.isValid()) {
				if (blockType(block) == BT_HEADING) {
					if (--rcnt == 0) {
						cursor.setPosition(block.position());
						break;
					}
				}
				block = block.previous();
			}
			break;
		}
		case 'z':	//	zz
			if( cursor.document() == docWidget->m_editor->document() )
				docWidget->m_editor->centerCursor();
			//else
			//	docWidget->m_preview->centerCursor();
			break;
#endif
		case '>': {	//	>>
			int startBn = cursor.blockNumber();
			gvi.m_editor->openUndoBlock();
			cursor.beginEditBlock();
			for(int i = 0; i < rcnt; ++i) {
				onAction_Indent();
				block = block.next();
				if( !block.isValid() ) break;
				cursor.setPosition(block.position());
				if( cursor.document() == docWidget->m_editor->document() )
					docWidget->m_editor->setTextCursor(cursor);
				else if( docWidget->m_diffview != nullptr && cursor.document() == docWidget->m_diffview->document() )
					docWidget->m_diffview->setTextCursor(cursor);
				else
					docWidget->m_preview->setTextCursor(cursor);
			}
			cursor.endEditBlock();
			gvi.m_editor->closeUndoBlock();
			gvi.m_editor->setActionFlag(UndoAction::FLAG_SHIFT);
			QTextBlock sb = doc->findBlockByNumber(startBn);
			if( sb.isValid() ) cursor.setPosition(sb.position());
			hat(cursor);
			gvi.m_isEditCommand = true;
			break;
		}
		case '<': {	//	<<
			int startBn = cursor.blockNumber();
			gvi.m_editor->openUndoBlock();
			cursor.beginEditBlock();
			for(int i = 0; i < rcnt; ++i) {
				onAction_UnIndent();
				block = block.next();
				if( !block.isValid() ) break;
				cursor.setPosition(block.position());
				if( cursor.document() == docWidget->m_editor->document() )
					docWidget->m_editor->setTextCursor(cursor);
				else if( docWidget->m_diffview != nullptr && cursor.document() == docWidget->m_diffview->document() )
					docWidget->m_diffview->setTextCursor(cursor);
				else
					docWidget->m_preview->setTextCursor(cursor);
			}
			cursor.endEditBlock();
			gvi.m_editor->closeUndoBlock();
			gvi.m_editor->setActionFlag(UndoAction::FLAG_SHIFT);
			QTextBlock sb = doc->findBlockByNumber(startBn);
			if( sb.isValid() ) cursor.setPosition(sb.position());
			hat(cursor);
			gvi.m_isEditCommand = true;
			break;
		}
		}
	} else {
#if 0
		switch( gvi.m_operator.unicode() ) {
		case 'z':
			switch( cmd.unicode() ) {
			case 't': {		// zt: カーソル行を画面最上行にする（カーソル位置はそのまま）
				int blockNum = cursor.blockNumber();
				docWidget->m_editor->verticalScrollBar()->setValue(blockNum);
				break;
			}
			case '\n':		//	zEnter カーソル行が画面最上行になるようにスクロール
				break;
			case '.':		//	z. カーソル行が画面中央になるようにスクロール
				break;
			case '-':		//	z- カーソル行が画面最下行になるようにスクロール
				break;
			case 'a':		//	za: toggle fold
				if (block.isValid()) {
			        block.setVisible(false); // ブロックを表示に設定
			        doc->markContentsDirty(block.position(), block.length());
			    }
				break;
			}
			break;
		}
#endif
	}
	return true;
}
void do_vi_H(QTextCursor& cursor, int rcnt, DocWidget* docWidget) {
	QPoint topLeft(0, 0);	//	左上点
	if( cursor.document() == docWidget->m_editor->document() )
		cursor = docWidget->m_editor->cursorForPosition(topLeft);
	else if( docWidget->m_diffview != nullptr && cursor.document() == docWidget->m_diffview->document() )
		cursor = docWidget->m_diffview->cursorForPosition(topLeft);
	else
		cursor = docWidget->m_preview->cursorForPosition(topLeft);
	if( rcnt > 1 )
		cursor.movePosition(QTextCursor::NextBlock, QTextCursor::MoveAnchor, rcnt - 1);
	hat(cursor);
}
void do_vi_L(QTextCursor& cursor, int rcnt, DocWidget* docWidget) {
	if( cursor.document() == docWidget->m_editor->document() )
		cursor = docWidget->m_editor->cursorForPosition(QPoint(0, docWidget->m_editor->viewport()->height() - 1));
	else if( docWidget->m_diffview != nullptr && cursor.document() == docWidget->m_diffview->document() )
		cursor = docWidget->m_diffview->cursorForPosition(QPoint(0, docWidget->m_diffview->viewport()->height() - 1));
	else
		cursor = docWidget->m_preview->cursorForPosition(QPoint(0, docWidget->m_preview->viewport()->height() - 1));
	if( rcnt > 1 )
		cursor.movePosition(QTextCursor::PreviousBlock, QTextCursor::MoveAnchor, rcnt - 1);
	hat(cursor);
}
void do_vi_M(QTextCursor& cursor, DocWidget* docWidget) {
	int h, l;	//	H, L block number
	QPoint topLeft(0, 0);	//	左上点
	if( cursor.document() == docWidget->m_editor->document() ) {
		QPoint bottomLeft(0, docWidget->m_editor->viewport()->height() - 1);
		h = docWidget->m_editor->cursorForPosition(topLeft).block().blockNumber();
		l = docWidget->m_editor->cursorForPosition(bottomLeft).block().blockNumber();
	} else if( docWidget->m_diffview != nullptr && cursor.document() == docWidget->m_diffview->document() ) {
		QPoint bottomLeft(0, docWidget->m_diffview->viewport()->height() - 1);
		h = docWidget->m_diffview->cursorForPosition(topLeft).block().blockNumber();
		l = docWidget->m_diffview->cursorForPosition(bottomLeft).block().blockNumber();
	} else {
		QPoint bottomLeft(0, docWidget->m_preview->viewport()->height() - 1);
		h = docWidget->m_preview->cursorForPosition(topLeft).block().blockNumber();
		l = docWidget->m_preview->cursorForPosition(bottomLeft).block().blockNumber();
	}
	QTextBlock block = cursor.document()->findBlockByNumber((h+l)/2);
	cursor.setPosition(block.position());
	hat(cursor);
}
void do_match_paren(QTextCursor& cursor) {
	QTextDocument *doc = cursor.document();
	QTextBlock block = cursor.block();
	int lineEnd = block.position() + block.text().size();
	const QString paren = "(){}[]";
	int pix;
	int pos = cursor.position();
	for(;;) {
		if( pos >= lineEnd ) return;
		if( (pix = paren.indexOf(doc->characterAt(pos))) >= 0 ) {
			break;
		}
		++pos;
	}
	int depth = 1;
	if( pix%2 == 0 ) {		//	openParen[pix] を発見（pix は偶数）
		const QString pp = paren.mid(pix, 2);
		while( ++pos < doc->characterCount() ) {
			int ix = pp.indexOf(doc->characterAt(pos));
			if( ix == 0 ) ++depth;
			else if( ix == 1 ) {
				if( --depth == 0 ) {
					cursor.setPosition(pos);
					return;
				}
			}
		}
	} else {				//	closeParen[pix] を発見（pix は奇数）
		const QString pp = paren.mid(pix-1, 2);
		while( --pos >= 0 ) {
			int ix = pp.indexOf(doc->characterAt(pos));
			if( ix == 1 ) ++depth;
			else if( ix == 0 ) {
				if( --depth == 0 ) {
					cursor.setPosition(pos);
					return;
				}
			}
		}
	}
}
void MainWindow::do_vi_motion(QChar cmd, QTextCursor& cursor, int rcnt, DocWidget* docWidget) {		//	hjkl等
	assert( gvi.m_editor != nullptr );
	auto p = cursor.position();		//	for Debug
	gvi.m_linewiseMoved = false;
	MarkdownEditor *targetEd = cursor.document() == docWidget->m_editor->document() ? docWidget->m_editor
		: (docWidget->m_diffview != nullptr && cursor.document() == docWidget->m_diffview->document() ? docWidget->m_diffview : nullptr);
	bool isEditor = targetEd != nullptr;
	auto moveMode = gvi.m_vMode != u' ' || (gvi.m_operator == ' ' && gvi.m_vMode == ' ') ?
						QTextCursor::MoveAnchor : QTextCursor::KeepAnchor;
	QTextDocument *doc = cursor.document();
	QTextBlock block = cursor.block();
	bool isCw = false;
	if( gvi.m_operator == 'c' && cmd == u'w' ) {
		cmd = u'e';		//	cw は ce として処理
		isCw = true;
	}
	if( gvi.m_operator == 'c' && cmd == u'W' ) {
		cmd = u'E';		//	cW は cE として処理
		isCw = true;
	}
	gvi.m_lastMoveCommand = cmd;
	switch( cmd.unicode() ) {
	case 'k': {
		do {
			if( !cursor.movePosition(QTextCursor::Up, moveMode, rcnt) ) break;
		} while( cursor.block().isValid() && !cursor.block().isVisible() );
		block = cursor.block();
		assert( block.isValid() );
		//if( gvi.m_preferred_x == INT_MAX ) {
		//	cursor.setPosition(block.position() + block.text().size());
		//	moveLeftIfAtEol(cursor);
		//} else {
		int offset = gvi.m_editor->getPrefferdOffset(block);
		if( offset == INT_MAX ) {
			cursor.setPosition(block.position() + block.text().size(), moveMode);
			moveLeftIfAtEol(cursor, moveMode);
		} else {
			cursor.setPosition(block.position() + offset, moveMode);
			if( block.isValid() && !block.text().isEmpty() && cursor.position() == block.position() + block.text().size() )
				cursor.movePosition(QTextCursor::Left, moveMode);		//	非空行の改行位置には移動不可
		}
		gvi.m_linewiseMoved = true;
		break;
	}
	case 'j': {
		do {
			if( !cursor.movePosition(QTextCursor::NextBlock, moveMode, rcnt) ) break;
		} while( cursor.block().isValid() && !cursor.block().isVisible() );
		block = cursor.block();
		assert( block.isValid() );
		//if( gvi.m_preferred_x == INT_MAX ) {	//	'$' で行末移動してた場合
		//	cursor.setPosition(block.position() + block.text().size());
		//	moveLeftIfAtEol(cursor);
		//} else {
		int offset = gvi.m_editor->getPrefferdOffset(block);
		if( offset == INT_MAX ) {
			cursor.setPosition(block.position() + block.text().size(), moveMode);
			moveLeftIfAtEol(cursor, moveMode);
		} else {
			cursor.setPosition(block.position() + offset, moveMode);
			if( /*block.isValid() &&*/ !block.text().isEmpty() && cursor.position() == block.position() + block.text().size() )
				cursor.movePosition(QTextCursor::Left, moveMode);		//	非空行の改行位置には移動不可
		}
		gvi.m_linewiseMoved = true;
		break;
	}
	case 'h':
		if( cursor.position() == block.position() ) {		//	行頭にいる場合
			if( !docWidget->m_diffMode ) {
				if( is_folded(block) ) {
					do_unfold(block);
					docWidget->m_editor->drawFoldLine(cursor, block);
					onMDTextChanged();
				} else if( is_foldable(block) ) {
					do_fold(block);
					docWidget->m_editor->clearFoldLine();
					onMDTextChanged();
				}
			}
		} else {
			rcnt = qMin(rcnt, cursor.position() - block.position());	//	行頭対応
			cursor.movePosition(QTextCursor::Left, moveMode, rcnt);
		}
		break;
	case 'l':
	case ' ': {
		int pos = block.position() + block.text().size() - (moveMode == QTextCursor::MoveAnchor ? 1 : 0);
		if( cursor.position() < pos ) {
			rcnt = qMin(rcnt, pos - cursor.position());
			cursor.movePosition(QTextCursor::Right, moveMode, rcnt);
		}
		break;
	}
	case 'w':
		for(int i = 0; i < rcnt; ++i) {
			auto pos = cursor.position();
			if( isEditor ) {
				targetEd->moveToNextWord(cursor, /*select = */gvi.m_operator != ' ');
				if( i+1 < rcnt && cursor.position() == block.position() + block.text().size() )		//	行末にいる場合
					cursor.movePosition(QTextCursor::Right, moveMode);
			} else
				docWidget->m_preview->moveToNextWord(cursor, /*select = */gvi.m_operator != ' ');
			if( cursor.position() == pos ) break;
		}
		if( isEditor ) {
			QTextBlock block = cursor.block();
			if( gvi.m_operator == ' ' && !block.text().isEmpty() && cursor.position() == block.position() + block.text().size() ) {
				//	非空行行末にいる場合
				if( cursor.position() >= doc->characterCount() - 1 ) {	//	EOF にいる場合
					cursor.movePosition(QTextCursor::Left, moveMode);
				} else {
					cursor.movePosition(QTextCursor::NextBlock, moveMode);
					hat(cursor, moveMode);
				}
			}
		}
		break;
	case 'W': {
		const int maxPos = doc->characterCount() - 1;
		for(int i = 0; i < rcnt; ++i) {
			// 現在の WORD(非空白の連続)をスキップ
			if( cursor.block().text().isEmpty() ) {	//	空行にいる場合
				if( !cursor.movePosition(QTextCursor::Right, moveMode) ) goto doneW;
			} else {
				while( cursor.position() < maxPos && !isSpaceChar(doc->characterAt(cursor.position())) )
					if( !cursor.movePosition(QTextCursor::Right, moveMode) ) goto doneW;
			}
			// 空白をスキップ。ただし「行頭かつ改行文字」(=空行)に来たら停止
			while( cursor.position() < maxPos && isSpaceChar(doc->characterAt(cursor.position())) ) {
				if( cursor.block().text().isEmpty() ) break;	//	空行にいる場合
				if( !cursor.movePosition(QTextCursor::Right, moveMode) ) goto doneW;
			}
			if( cursor.position() >= maxPos ) break;
		}
doneW:
		if (cursor.position() == maxPos && moveMode == QTextCursor::MoveAnchor)
			cursor.movePosition(QTextCursor::Left);
		break;
	}
	case 'e':
		for(int i = 0; i < rcnt; ++i) {
			auto pos = cursor.position();
			if( isCw && rcnt == 1 && clampCwToCurWordEnd(cursor) ) {
				//	cw の場合は自単語の末尾までに制限（#1272）
			} else if( isEditor )
				targetEd->moveToNextWordEnd(cursor, /*select = */gvi.m_operator != ' ');
			else
				docWidget->m_preview->moveToNextWordEnd(cursor, /*select = */gvi.m_operator != ' ');
			if( cursor.position() == pos ) break;
		}
		if( gvi.m_operator == u'c' || gvi.m_operator == u'd' || gvi.m_operator == u'y' )
			cursor.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor);	//	ce、de、ye の場合：単語末尾も含める（#1274）
		break;
	case 'E': {
		const int maxPos = doc->characterCount() - 1;
		for(int i = 0; i < rcnt; ++i) {
			if( isCw && rcnt == 1 && clampCwToCurWORDEnd(cursor) ) {
				//	cW の場合は自WORDの末尾までに制限（cw と同様）
			} else {
				if( !cursor.movePosition(QTextCursor::Right, moveMode) ) break;	// すでに末尾にいる場合はこれ以上進めないため、
				// 1. スペース文字をスキップ（次の単語の先頭に達するまで右へ）
				while( cursor.position() < maxPos && isSpaceChar(doc->characterAt(cursor.position())) )
					if( !cursor.movePosition(QTextCursor::Right, moveMode) ) break;
				// 2. スペース以外の文字をスキップ（単語を通り過ぎるまで右へ）
				while( cursor.position() < maxPos && !isSpaceChar(doc->characterAt(cursor.position())) )
					if( !cursor.movePosition(QTextCursor::Right, moveMode) ) break;
				// 単語を通り過ぎてスペース（または終端）の上にいるので、
				// 1文字左に戻ることで「単語の最後の文字の上」にカーソルを合わせます。
				cursor.movePosition(QTextCursor::Left, moveMode);
				if( cursor.position() >= maxPos ) break;
			}
		}
		if( gvi.m_operator == u'c' || gvi.m_operator == u'd' || gvi.m_operator == u'y' )
			cursor.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor);	//	cE、dE、yE の場合：WORD末尾も含める（#1274）
	    break;
	}
    case 'b':
		for(int i = 0; i < rcnt; ++i) {
			if( isEditor )
				targetEd->moveToPrevWord(cursor, /*select = */gvi.m_operator != ' ');
			else
				docWidget->m_preview->moveToPrevWord(cursor, /*select = */gvi.m_operator != ' ');
			if( cursor.position() == 0 ) break;
		}
		break;
	case 'B':
		for(int i = 0; i < rcnt; ++i) {
			while( cursor.position() > 0 && isSpaceChar(doc->characterAt(cursor.position()-1)) ) {
				cursor.movePosition(QTextCursor::Left, moveMode);
				if( cursor.block().text().isEmpty() ) break;	//	空行にいる場合
			}
			while( cursor.position() > 0 && !isSpaceChar(doc->characterAt(cursor.position()-1)) ) {
				cursor.movePosition(QTextCursor::Left, moveMode);
			}
			if( cursor.position() == 0 ) break;
		}
		break;
	case '$':
		if( gvi.m_editor != nullptr )
			gvi.m_editor->setPrefferedX(INT_MAX);
		if( rcnt > 1 ) {
			cursor.movePosition(QTextCursor::NextBlock, moveMode, rcnt-1);
			block = cursor.block();
		}
		if( block.text().isEmpty() && block.blockNumber() > 0 && block.blockNumber() == doc->blockCount() - 1 ) {
			//	末尾改行で生じる空ブロックに着地した場合は、実在する最終行を対象にする（v0.3 #1409）
			block = block.previous();
			if( block.text().isEmpty() )
				cursor.setPosition(block.position(), moveMode);
		}
		if( !block.text().isEmpty() ) {
			cursor.setPosition(block.position() + block.text().size() - 1, moveMode);
			if( gvi.m_operator == u'y' || gvi.m_operator == u'd' || gvi.m_operator == u'c' )
				cursor.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor);	//	y$/d$/c$ の場合：行末文字も含める（#1274）
		}
		break;
	case '-':
		if( cursor.movePosition(QTextCursor::PreviousBlock, moveMode, rcnt) )
			hat(cursor, moveMode);
		gvi.m_linewiseMoved = true;
		break;
	case '\n':
	case '+':
		if( cursor.movePosition(QTextCursor::NextBlock, moveMode, rcnt) )
			hat(cursor, moveMode);
		gvi.m_linewiseMoved = true;
		break;
	case '^':
		hat(cursor, moveMode);
		break;
	case '%':
		do_match_paren(cursor);
		break;
	case 'G':
		if( gvi.m_repeatCount == 0 || !(block = doc->findBlockByNumber(gvi.m_repeatCount - 1)).isValid() ) {
			cursor.movePosition(QTextCursor::End, moveMode);
			if( cursor.block().text().isEmpty() )
				cursor.movePosition(QTextCursor::PreviousBlock, moveMode);
		} else {
			//block = doc->findBlockByNumber(gvi.m_repeatCount - 1);
			cursor.setPosition(block.position(), moveMode);
		}
		hat(cursor, moveMode);
		if( gvi.m_operator != ' ' && !cursor.hasSelection() )	//	開始位置＝着地位置だと選択範囲が空になり、演算子が効かない（v0.3 #1226）
			cursor.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor);
		gvi.m_linewiseMoved = true;
		break;
	case 'H':
		do_vi_H(cursor, rcnt, docWidget);
		break;
	case 'L':
		do_vi_L(cursor, rcnt, docWidget);
		break;
	case 'M':
		do_vi_M(cursor, docWidget);
		break;
	case '*':		//	カーソル位置単語検索
		onAction_FindWord();
		break;
	case 'n':
	case 'N':
		if( g.m_lastSearchedPat.isEmpty() ) return;
		if( isEditor && docWidget->m_diffview != nullptr && targetEd == docWidget->m_diffview ) {
			bool forward = (g.m_searchForward && cmd == 'n') || (!g.m_searchForward && cmd == 'N');
			QTextDocument::FindFlags flags;
			if( !forward ) flags |= QTextDocument::FindBackward;
			if( !g.m_ignoreCase ) flags |= QTextDocument::FindCaseSensitively;
			auto findInDoc = [&](int fromPos) -> QTextCursor {
				if( g.m_regexp ) {
					QRegularExpression::PatternOptions opts = g.m_ignoreCase
						? QRegularExpression::CaseInsensitiveOption : QRegularExpression::NoPatternOption;
					QRegularExpression rx(g.m_lastSearchedPat, opts);
					return rx.isValid() ? doc->find(rx, fromPos, flags) : QTextCursor();
				}
				return doc->find(g.m_lastSearchedPat, fromPos, flags);
			};
			QTextCursor match;
			if( forward ) {
				int startPos = cursor.position() + 1;
				if( startPos < doc->characterCount() )
					match = findInDoc(startPos);
				if( match.isNull() )
					match = findInDoc(0);
			} else {
				match = findInDoc(cursor.position());
				if( match.isNull() )
					match = findInDoc(qMax(0, doc->characterCount() - 1));
			}
			if( !match.isNull() ) {
				cursor.setPosition(match.anchor());
			} else {
				statusBar()->showMessage(tr("Pattern not found: %1").arg(g.m_lastSearchedPat), 5000);
			}
			break;
		}
		if( g.m_searchForward && cmd == 'n' || !g.m_searchForward && cmd == 'N' ) {
			int pos0 = cursor.position();
			cursor.movePosition(QTextCursor::Right);
			if( isEditor )
				targetEd->setTextCursor(cursor);
			do_find();
			if( g.m_matchedPosition >= 0 )
				cursor.setPosition(g.m_matchedPosition);
			else {
				cursor.setPosition(pos0);
	            statusBar()->showMessage(tr("Pattern not found: %1").arg(g.m_lastSearchedPat), 5000);
			}
		} else {
			do_find(true);
			if( g.m_matchedPosition >= 0 )
				cursor.setPosition(g.m_matchedPosition);
			else
	            statusBar()->showMessage(tr("Pattern not found: %1").arg(g.m_lastSearchedPat), 5000);
		}
		break;
	default:
		return;
	}
	do_cdy_moved(cursor);
	if( gvi.m_vMode != u' ' )
		gvi.m_editor->highlightVText(cursor);
	//if( cmd.unicode() != u'$' && cmd.unicode() != u'j' && cmd.unicode() != u'k' ) {
	//	auto r = docWidget->m_editor->cursorRect();
	//	gvi.m_preferred_x = r.x();
	//}
}
void do_join(QTextCursor& cursor, int rcnt) {
	assert( gvi.m_editor != nullptr );
	auto cur0 = cursor;
	QTextDocument *doc = cursor.document();
	int joins = (rcnt <= 1) ? 1 : (rcnt - 1);
	if( gvi.m_editor != nullptr )
		gvi.m_editor->openUndoBlock();
	cursor.beginEditBlock();
	int finalCursorPos = -1;
	int njoined = 0;
	for (int i = 0; i < joins; ++i) {
		cursor.movePosition(QTextCursor::EndOfBlock);
		QTextBlock nextBlock = cursor.block().next();
		if (!nextBlock.isValid()) break; // 次の行がない（ドキュメント末尾）場合は結合処理を終了
		QString currentText = cursor.block().text();
		QString nextText = nextBlock.text();
		int trailingSpaces = 0;
		while (trailingSpaces < currentText.size() && 
			   (currentText[currentText.size() - 1 - trailingSpaces] == ' ' || 
				currentText[currentText.size() - 1 - trailingSpaces] == '\t')) {
			trailingSpaces++;
		}
		int leadingSpaces = 0;
		while (leadingSpaces < nextText.size() && 
			   (nextText[leadingSpaces] == ' ' || nextText[leadingSpaces] == '\t')) {
			leadingSpaces++;
		}
		if (trailingSpaces > 0) {	//	行末に空白があれば削除
			cursor.movePosition(QTextCursor::Left, QTextCursor::KeepAnchor, trailingSpaces);
			//cursor.removeSelectedText();
			gvi.m_editor->do_deleteText(cursor);
		}
		cursor.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor, 1 + leadingSpaces);
		//cursor.removeSelectedText();
		gvi.m_editor->do_deleteText(cursor);
		bool needsSpace = true;
		if (currentText.size() == trailingSpaces || nextText.size() == leadingSpaces) {
			needsSpace = false;
		} else if (leadingSpaces < nextText.size() && nextText[leadingSpaces] == ')') {
			needsSpace = false;
		}
		
		if (needsSpace) {
			//cursor.insertText(" ");
			gvi.m_editor->do_insertText(cursor, " ");
		}
		if (i == 0) {
			finalCursorPos = cursor.position();
			if (needsSpace && finalCursorPos > 0) {
				finalCursorPos--; // 挿入したスペースの上にカーソルを合わせるため1文字左へ
			}
		}
		++njoined;
	}
	gvi.m_editor->closeUndoBlock();
	cursor.endEditBlock();
	gvi.m_editor->setActionFlag(UndoAction::FLAG_JOIN);
	if( njoined == 0 )
		cursor = cur0;
	else if (finalCursorPos >= 0)
		cursor.setPosition(finalCursorPos);
}
void MainWindow::do_viCmd(QChar cmd, QTextCursor& cursor) {
	//if( cmd.isEmpty() ) return;
	DocWidget *docWidget = getCurDocWidget();
	if( docWidget == nullptr ) return;
	gvi.m_editor = nullptr;
	//gvi.m_preview = nullptr;
	QWidget *w = QApplication::focusWidget();
	if( w == docWidget->m_editor ) gvi.m_editor = (MarkdownEditor*)w;
	else if( w == docWidget->m_diffview ) gvi.m_editor = (MarkdownEditor*)w;
	//else if( w == docWidget->m_preview ) gvi.m_preview = (MarkdownPreview*)w;
	//bool isEditor = cursor.document() == docWidget->m_editor->document();
	if( gvi.m_editor == nullptr )		//	コマンドラインにフォーカスがある場合？
		gvi.m_editor = docWidget->m_editor;
	assert( gvi.m_editor != nullptr );
	bool completed = true;
	int rcnt = getRepeatCount();
	gvi.m_pendingCommand += cmd;
	//	カウント前置（例: 2/）でもコマンドラインを開くため、pendingCommand の完全一致ではなく
	//	入力文字そのもので判定する。オペレータ・プレフィックス・fFtT 待ちの場合は対象外。
	if( (cmd == ':' || cmd == '/' || cmd == '?') && gvi.m_operator == ' ' && gvi.m_prefix == ' ' && gvi.m_fFtT == ' ' ) {
		statusBar()->clearMessage();
		gvi.m_currentMode = ViMode::CommandLine;
		//gvi.m_cmdlineMode = true;
		gvi.m_prevFocusWidget = cursor.document() == docWidget->m_editor->document() ?
									(QWidget*)docWidget->m_editor :
									docWidget->m_diffview != nullptr && cursor.document() == docWidget->m_diffview->document() ?
									(QWidget*)docWidget->m_diffview : (QWidget*)docWidget->m_preview;
		m_prefixLabel->setText(QString(cmd));
		m_prefixLabel->show();
		m_cmdLine->setText("");
		m_cmdLine->show();
		m_cmdLine->setFocus();
		if( cmd == ':' ) {
			//gvi.m_exhist.push_front(":");
			while( gvi.m_exhist.size() > 64 ) gvi.m_exhist.pop_back();
			gvi.m_exhist_ix = -1;
		} else if( cmd == '/' || cmd == '?' ) {
			gvi.m_searchhist_ix = -1;
		}
		return;
	} else if( gvi.m_fFtT == 'f' || gvi.m_fFtT == 'F' || gvi.m_fFtT == 't' || gvi.m_fFtT == 'T' ) {
		gvi.m_lastMoveCommand = QString(gvi.m_fFtT) + cmd;
		if( do_fFtT(cursor, gvi.m_fFtT, cmd, rcnt) )
			do_cdy_moved(cursor);
	} else if( gvi.m_prefix == 'r' ) {
		do_r(cmd, cursor, rcnt);
	} else if( gvi.m_prefix != ' ' ) {
		do_prefix_cmd(cmd, cursor, rcnt, docWidget);		//	{g z r [ ]} cmd
	} else if( cmd == 'g' || cmd == 'z' || cmd == 'Z' || cmd == 'r' || cmd == '[' || cmd == ']' ) {
		gvi.m_prefix = cmd;
		completed = false;
	} else if( gvi.m_operator != ' ' && gvi.m_operator == cmd ) {		//	cc dd yy << >>
		completed = do_vi_operator(cmd, cursor, rcnt * gvi.m_opCount, docWidget);		//	op cmd
	} else if( cmd == 'c' || cmd == 'd' || cmd == 'y' || cmd == '<' || cmd == '>' ) {
		completed = do_cdy(cmd, cursor);
		//gvi.m_operator = cmd;
		//if( gvi.m_repeatCount != 0 ) gvi.m_opCount = gvi.m_repeatCount;
		//gvi.m_repeatCount = 0;
		//completed = false;
	} else if( cmd == 'i' || cmd == 'I' || cmd == 'a' || cmd == 'A' || cmd == 'o' || cmd == 'O' || cmd == 's' || cmd == 'S' || cmd == 'C' ) {
		do_vi_insert(cmd, cursor, rcnt);
	} else if( cmd == u'R' ) {
		gvi.m_currentMode = ViMode::Replace;
		gvi.m_editor->setOverwriteMode(true);
		gvi.m_editor->openUndoBlock();
	} else if( cmd == 'x' || cmd == 'X' || cmd == 'D' ) {
		do_vi_delete(cmd, cursor,rcnt);
	}
	// else if( cmd == 'c' || cmd == 'd' || cmd == 'y' || cmd == 'g' || cmd == 'z' || cmd == 'r' ||
	//	cmd == '<' || cmd == '>' || cmd == '[' || cmd == ']' )
	//{
	//	completed = do_vi_operator(cmd, cursor, rcnt, docWidget);
	//}
	else if( cmd.unicode() >= '0' && cmd.unicode() <= '9' ) {
		if( cmd == u'0' && gvi.m_repeatCount == 0 ) {
			auto moveMode = gvi.m_operator == ' ' ? QTextCursor::MoveAnchor : QTextCursor::KeepAnchor;
			cursor.movePosition(QTextCursor::StartOfBlock, moveMode);
			do_cdy_moved(cursor);
		} else {
			gvi.m_repeatCount = gvi.m_repeatCount * 10 + (cmd.unicode() - u'0');
			completed = false;
		}
	} else if( cmd == 'h' || cmd == 'j' || cmd == 'k' || cmd == 'l' || cmd == ' ' || cmd == '%' || 
	           cmd == 'w' || cmd == 'W' || cmd == 'b' || cmd == 'B' || cmd == 'e' || cmd == 'E' ||
	           cmd == '$' || cmd == '^' || cmd == '-' || cmd == '+' || cmd == 'n' || cmd == 'N' || cmd == '*' ||
	           cmd == '\n' || cmd == 'G' || cmd == 'H' || cmd == 'L' || cmd == 'M' )
	{
	    do_vi_motion(cmd, cursor, rcnt*gvi.m_opCount, docWidget);
	} else {
		auto moveMode = gvi.m_operator == ' ' ? QTextCursor::MoveAnchor : QTextCursor::KeepAnchor;
		QTextDocument *doc = cursor.document();
		QTextBlock block = cursor.block();
		QChar c;		//	for , ;
		QString buf;	//	for .
		switch( cmd.unicode() ) {
		case 'f':
		case 'F':
		case 't':
		case 'T':
			if( gvi.m_fFtT == ' ' ) {
				gvi.m_fFtT = cmd.unicode();
				completed = false;
			}
			break;
		case '~':
			do_swap_case(cursor, rcnt);		//	大文字・小文字反転
			moveLeftIfAtEol(cursor);
			gvi.m_isEditCommand = true;
			break;
		case 'Y':
			do_yank_line(cursor, rcnt);
			statusBar()->showMessage(QString("%1 charactors yanked.").arg(gvi.m_yankBuffer.size()), 5000);
			break;
		case 'p':
			if( !gvi.m_yankBuffer.isEmpty() ) {
#if 1
				gvi.m_isEditCommand = true;
				int insertPos = -1;
				bool undoOpened = false;
				if( gvi.m_linewiseYanked ) {
					// 次のブロックへ移動を試みる
					if( !cursor.movePosition(QTextCursor::NextBlock) ) {
						// 【EOF行の場合】NextBlock が失敗するので、末尾に改行を足して新しい行を作る
						cursor.movePosition(QTextCursor::EndOfBlock);
						if( gvi.m_editor != nullptr ) {
							gvi.m_editor->openUndoBlock();
							undoOpened = true;
							gvi.m_editor->do_insertText(cursor, "\n");
						} else {
							cursor.insertText("\n");
						}
						// 改行を入れた直後の位置（＝新しく作られた行の先頭）を記録
						insertPos = cursor.position();
					} else {
						// 【通常行の場合】次の行の先頭位置を記録
						insertPos = cursor.position();
					}
				} else {
					if( !cursor.atBlockEnd() )
						cursor.movePosition(QTextCursor::Right);
					insertPos = cursor.position();
				}
				// テキスト挿入
				if( gvi.m_editor != nullptr )
					gvi.m_editor->do_insertText(cursor, gvi.m_yankBuffer.repeated(rcnt));
				else
					cursor.insertText(gvi.m_yankBuffer.repeated(rcnt));
				if( undoOpened )
					gvi.m_editor->closeUndoBlock();
				// ペースト後のカーソル位置調整
				if( gvi.m_linewiseYanked ) {    // 行単位ペースト
					cursor.setPosition(insertPos);
					hat(cursor); // 行頭（または最初の非空白文字）へ
				} else {        // 文字単位ペースト
					cursor.movePosition(QTextCursor::Left);
				}
#else
				if( gvi.m_linewiseYanked ) {
					cursor.movePosition(QTextCursor::NextBlock);
					//do_openline(cursor, false);
				} else
					cursor.movePosition(QTextCursor::Right);
				//cursor.insertText(gvi.m_yankBuffer.repeated(rcnt));
				auto pos = cursor.position();
				if( gvi.m_editor != nullptr )
					gvi.m_editor->do_insertText(cursor, gvi.m_yankBuffer.repeated(rcnt));
				if( gvi.m_linewiseYanked ) {	//	行単位ペースト
					//cursor.movePosition(QTextCursor::Up);
					cursor.setPosition(pos);
					hat(cursor);
				} else		//	文字単位ペースト
					cursor.movePosition(QTextCursor::Left);
#endif
			}
			break;
		case 'P':
			if( !gvi.m_yankBuffer.isEmpty() ) {
				gvi.m_isEditCommand = true;
				if( gvi.m_linewiseYanked ) {
					cursor.movePosition(QTextCursor::StartOfBlock);
					//do_openline(cursor, true);
				}
				auto pos = cursor.position();
				//cursor.insertText(gvi.m_yankBuffer.repeated(rcnt));
				gvi.m_editor->do_insertText(cursor, gvi.m_yankBuffer.repeated(rcnt));
				if( gvi.m_linewiseYanked ) {	//	行単位ペースト
					//cursor.movePosition(QTextCursor::Up);
					cursor.setPosition(pos);
					hat(cursor);
				} else		//	文字単位ペースト
					cursor.movePosition(QTextCursor::Left);
			}
			break;
		case 'J':
			gvi.m_isEditCommand = true;
			do_join(cursor, rcnt);
			break;
		case 'u': {
			docWidget->removeDummyBlocks();
			int pos = 0;
			uchar f = gvi.m_editor->undoActionFlags();
			for(int i = 0; i < rcnt; ++i) {
				pos = gvi.m_editor->do_undo();
				//pos = docWidget->m_editor->do_undo();
			}
			cursor = gvi.m_editor->textCursor();
			cursor.setPosition(pos);
			if( (f&UndoAction::FLAG_SHIFT) != 0 || (f&UndoAction::FLAG_LINEWISE) != 0 )
				hat(cursor);
			moveLeftIfAtEol(cursor);
			break;
		}
		case 'U': {
			int pos = 0;
			uchar f = gvi.m_editor->redoActionFlags();
			for(int i = 0; i < rcnt; ++i) {
				pos = gvi.m_editor->do_redo();
				//pos = docWidget->m_editor->do_redo();
			}
			cursor = gvi.m_editor->textCursor();
			cursor.setPosition(pos);
			if( (f&UndoAction::FLAG_SHIFT) != 0 || (f&UndoAction::FLAG_LINEWISE) != 0 )
				hat(cursor);
			moveLeftIfAtEol(cursor);
			break;
		}
		case ';':		//	同方向再検索
			if( do_fFtT(cursor, gvi.m_last_fFtT, gvi.m_last_fFtT_char, rcnt, true) )
				do_cdy_moved(cursor);	//	演算子＋; 対応（#1375）
#if 0
			switch( gvi.m_last_fFtT.unicode() ) {
			case 'f':
			case 'F':
				c = 'f';
				break;
			case 't':
			case 'T':
				c = 't';
				break;
			}
			do_fFtT(cursor, c, gvi.m_last_fFtT_char, rcnt);
#endif
			break;
		case ',':		//	（前回と）逆方向再検索
			switch( gvi.m_last_fFtT.unicode()) {
			case 'f':
			case 'F':
				c = QChar(u'F' + u'f' - gvi.m_last_fFtT.unicode());
				break;
			case 't':
			case 'T':
				c = QChar(u'T' + u't' - gvi.m_last_fFtT.unicode());
				break;
			}
			if( do_fFtT(cursor, c, gvi.m_last_fFtT_char, rcnt, true) )
				do_cdy_moved(cursor);	//	演算子＋, 対応（#1375）
			break;
		case '.':	//	redo
			if( gvi.m_redoing || gvi.m_lastEditCommand.isEmpty() ) break;
			//##qDebug() << "'.': gvi.m_lastEditCommand = " << gvi.m_lastEditCommand;
			gvi.m_redoing = true;
			buf = gvi.m_lastEditCommand;
			if( gvi.m_repeatCount != 0 ) {		//	<num>. の場合
				//gvi.m_redoCount = gvi.m_repeatCount;
				int i = 0;
				while( i < buf.size() && buf[i].isDigit() )		//	数字部分をスキップ
					++i;
				buf = QString::number(gvi.m_repeatCount) + buf.mid(i);
			} //else
				//gvi.m_redoCount = 1;
			//buf += gvi.m_insertedText;
			//##qDebug() << "redo buf = " << buf;
			gvi.m_repeatCount = 0;
			gvi.m_editor->openUndoBlock();
			for(QChar ch: buf) {
				do_viCmd(ch, cursor);
			}
			gvi.m_editor->closeAllUndoBlock();
			gvi.m_redoing = false;
			break;
		case 'v':
			if( gvi.m_vMode == u'v' ) {
				gvi.m_vMode = u' ';
				gvi.m_vAnchor = -1;
			} else {
				gvi.m_vMode = u'v';
				gvi.m_vAnchor = cursor.position();
			}
			gvi.m_editor->highlightVText(cursor);
			break;
		case 'V':
			if( gvi.m_vMode == u'V' ) {
				gvi.m_vMode = u' ';
				gvi.m_vAnchorBlock = -1;
			} else {
				gvi.m_vMode = u'V';
				gvi.m_vAnchorBlock = cursor.block().blockNumber();
			}
			gvi.m_editor->highlightVText(cursor);
			break;
		default:	//	不正コマンド
			//	undone: エラー表示
			completed = true;
		}
	}
	auto p2 = cursor.position();	//	for Debug
#if 0
	if( completed ) {	//	コマンド完結
		if( cmd != 'j' && cmd != 'k' && cmd != '$' ) {
			if( gvi.m_editor != nullptr )
				gvi.m_editor->savePrefferedX(cursor);
		}
		if( gvi.m_redoing && gvi.m_currentMode == ViMode::Insert && !gvi.m_insertedText.isEmpty() ) {
			auto text = gvi.m_insertedText;
			if( gvi.m_repeatCount > 1 )
				text = gvi.m_insertedText.repeated(gvi.m_repeatCount);
			gvi.m_editor->do_insertText(cursor, text);
			cursor.movePosition(QTextCursor::Left);
			//cursor.insertText(gvi.m_insertedText.repeated(gvi.m_repeatCount));
			//gvi.m_viCmdMode = true;
			gvi.m_currentMode = ViMode::Normal;
		}
		gvi.m_opCount = 1;
		gvi.m_repeatCount = 0;
		gvi.m_operator = ' ';
		gvi.m_prefix = ' ';
		gvi.m_fFtT = ' ';
		if( gvi.m_isEditCommand && !gvi.m_redoing )
			gvi.m_lastEditCommand = gvi.m_pendingCommand;
		//else
		//	gvi.m_lastEditCommand.clear();
		if( gvi.m_currentMode == ViMode::Insert ) {
			gvi.m_recInsertedText = true;
			gvi.m_insertedText.clear();
		} else
			gvi.m_recInsertedText = false;
		gvi.m_pendingCommand.clear();
		gvi.m_isEditCommand = false;
		if( docWidget != nullptr ) {
			gvi.m_editor->viewport()->update();
			gvi.m_editor->prohibitMergeUndo();
			docWidget->m_preview->viewport()->update();
		}
		//##qDebug() << "gvi.m_lastEditCommand = " << gvi.m_lastEditCommand;
	}
#endif
	if( completed )	//	コマンド完結
		do_viCmd_completed(cursor, cmd);
	if( gvi.m_currentMode == ViMode::Insert ) {
		statusBar()->showMessage("-- INSERT --");
	} else if( gvi.m_currentMode == ViMode::Replace ) {
		statusBar()->showMessage("-- REPLACE --");
	} else if( gvi.m_vMode == u'v' ) {
		statusBar()->showMessage("-- VISUAL --");
	} else if( gvi.m_vMode == u'V' ) {
		statusBar()->showMessage("-- VISUAL LINE --");
	} //else
	//	statusBar()->clearMessage();
}
void MainWindow::do_viCmd_completed(QTextCursor& cursor, QChar cmd) {	//	vicmd 完結時処理（#0365）
	DocWidget *docWidget = getCurDocWidget();
	if( cmd != 'j' && cmd != 'k' && cmd != '$' ) {
		if( gvi.m_editor != nullptr )
			gvi.m_editor->savePrefferedX(cursor);
	}
	if( gvi.m_redoing && gvi.m_currentMode == ViMode::Insert ) {
		if( !gvi.m_insertedText.isEmpty() ) {
			auto text = gvi.m_insertedText;
			if( gvi.m_repeatCount > 1 )
				text = gvi.m_insertedText.repeated(gvi.m_repeatCount);
			gvi.m_editor->do_insertText(cursor, text);
		}
		if( cursor.position() > cursor.block().position() )		//	[#1280] exitInsertMode と同様、ブロック先頭では左移動しない
			cursor.movePosition(QTextCursor::Left);
		gvi.m_currentMode = ViMode::Normal;
	}
	gvi.m_opCount = 1;
	gvi.m_repeatCount = 0;
	gvi.m_operator = ' ';
	gvi.m_prefix = ' ';
	gvi.m_fFtT = ' ';
	if( gvi.m_isEditCommand && !gvi.m_redoing )
		gvi.m_lastEditCommand = gvi.m_pendingCommand;
	//else
	//	gvi.m_lastEditCommand.clear();
	if( gvi.m_currentMode == ViMode::Insert ) {
		gvi.m_recInsertedText = true;
		gvi.m_insertedText.clear();
	} else
		gvi.m_recInsertedText = false;
	gvi.m_pendingCommand.clear();
	gvi.m_isEditCommand = false;
	if( docWidget != nullptr ) {
		gvi.m_editor->viewport()->update();
		gvi.m_editor->prohibitMergeUndo();
		docWidget->m_preview->viewport()->update();
	}
	//##qDebug() << "gvi.m_lastEditCommand = " << gvi.m_lastEditCommand;
}
void MainWindow::exitInsertMode(QTextCursor& cursor) {
	if( gvi.m_insRepCount > 1 && !gvi.m_insertedText.isEmpty() ) {
		QString txt = gvi.m_insertedText.repeated(gvi.m_insRepCount - 1);
		//QString txt;
		//for(int i = 0; i < gvi.m_insRepCount - 1; ++i)
		//	txt += gvi.m_insertedText;
		//cursor.insertText(txt);
		if( !txt.isEmpty() ) {
			if( gvi.m_editor != nullptr )
				gvi.m_editor->do_insertText(cursor, txt);
		}
		gvi.m_insRepCount = 1;
	}
	//if( g.m_editBlockOpen ) {
	//	//cursor.endEditBlock();
	//	g.m_editBlockOpen = false;
	//}
	if( !gvi.m_autotext.isEmpty() && cursor.block().text() == gvi.m_autotext ) {
		cursor.movePosition(QTextCursor::StartOfBlock, QTextCursor::KeepAnchor);
		if(gvi.m_editor != nullptr)
			gvi.m_editor->do_deleteText(cursor);
	} else if( /*!gvi.m_insertedText.isEmpty() &&*/ cursor.position() > cursor.block().position()) {
		cursor.movePosition(QTextCursor::Left);			//	１カラム左に移動
		//##this->setTextCursor(cursor);
	}
	if( gvi.m_editor != nullptr )
		gvi.m_editor->closeAllUndoBlock();
	if( gvi.m_editor != nullptr )
		gvi.m_editor->savePrefferedX(cursor);	//	j/k の着地列を挿入終了位置に合わせる（#1378）
	//##setOverwriteMode(false);
	gvi.m_currentMode = ViMode::Normal;
	//gvi.m_viCmdMode = true;
	gvi.m_vMode = u' ';
	gvi.m_recInsertedText = false;
	//highlightVText(cursor);
	statusBar()->clearMessage();
}
void MainWindow::exitVisualMode(QTextCursor& cursor) {
	cursor.setPosition(cursor.selectionStart());
	cursor.clearSelection();
	gvi.m_vMode = u' ';
	gvi.m_editor->highlightVText(cursor);
	statusBar()->clearMessage();
}
int do_search_line(const QString &text, int &i, int currentLine, const QTextDocument *doc) {
	QChar c = text[i++];
	QString pat;
	while( i < text.size() && text[i] != c ) {
		if( text[i] == '\\' ) {
			pat += text[i++];
			if( i >= text.size() ) break;
		}
		pat += text[i++];
	}
	if( i >= text.size() || pat.isEmpty() )		//	undone: // ?? の場合は再検索対応
		return -1;
	++i;
	QTextBlock block = doc->findBlockByNumber(currentLine - 1);
	if( !block.isValid() ) return -1;
	QRegularExpression::PatternOptions opts = g.m_ignoreCase
		? QRegularExpression::CaseInsensitiveOption : QRegularExpression::NoPatternOption;
	QRegularExpression re(pat, opts);
	if( !re.isValid() ) return -1;
	if( c == '/' ) {	//	順方向検索
		if( !(block = block.next()).isValid() ) return -1;
		QTextCursor cur = doc->find(re, block.position());
		if( cur.isNull() ) return -1;	//	not found
		return cur.block().blockNumber() + 1;
	} else {	//	逆方向検索
		//if( !(block = block.next()).isValid() ) return -1;
		QTextCursor cur = doc->find(re, block.position(), QTextDocument::FindBackward);
		if( cur.isNull() ) return -1;	//	not found
		return cur.block().blockNumber() + 1;
	}
}
int parseLineSpec(const QString &text, int &i, int currentLine, int totalLines, const QTextDocument *doc) {
	if (i >= text.size()) return -1;
    int line = -1;
    QChar c = text.at(i);
    if (c.isDigit()) {
        int start = i;
        while (i < text.size() && text.at(i).isDigit()) {
            i++;
        }
        line = text.mid(start, i - start).toInt();
    } else if (c == '.') {
        line = currentLine;
        i++;
    } else if (c == '$') {
        line = totalLines;
        i++;
    } else if( c == '/' || c == '?' ) {
    	line = do_search_line(text, i, currentLine, doc);
    	if( line < 0 ) return -1;	//	未発見・不正構文はクランプせずエラーとして返す（#1450）
    } else if( c == '+' || c == '-' ) {
        line = currentLine;
    } else {
        // 行指定の記号ではない（"w" や "q" などのコマンド文字に達した）
        return -1;
    }
    // 2. オプションのオフセット（例: "+3" や "-5"）の解析
    if (i < text.size()) {
        QChar sign = text.at(i);
        if (sign == '+' || sign == '-') {
            i++;
            int offset = 0;
            int start = i;
            while (i < text.size() && text.at(i).isDigit()) {
                i++;
            }
            if (i > start) {
                offset = text.mid(start, i - start).toInt();
            } else {
                offset = 1; // ".+" や "$-" のように数字が省略された場合は 1 とする
            }
            
            if (sign == '+') {
                line += offset;
            } else {
                line -= offset;
            }
        }
    }
    // ドキュメントの実際の範囲内にクランプ（丸め込み）
    if (line < 1) line = 1;
    if (line > totalLines) line = totalLines;

    return line;
}
//	cmd が pat にマッチするか？
//	pat は ( を含み、( 直前までマッチすればおｋ
//	( 以降は何文字マッチしてもおｋ、だが不一致は false を返す
//	pat: "q(uit" の場合、"q", "qu", "qui", "quit" でマッチ、"qx" は不一致
//	pat: "quit" の場合、"quit" のみがマッチ
bool is_match(const QString &cmd, const QString &pat) {
	//gvi.m_cmdArg.clear();
	int i = 0, k = 0;
	bool paren = false;		//	'(' フラグ
	while( i < cmd.size() && k < pat.size() ) {
		if( pat[k] == '(' ) {
			paren = true;
			++k;
			continue;
			//break;
		}
		if( cmd[i++] != pat[k++] ) return false;	//	不一致
	}
	if( i != cmd.size() && cmd[i] != ' ' )
		return false;	//	cmd の最後までマッチしていない
	if( k == pat.size() || (k < pat.size() && pat[k] == '(') || paren ) {	//	次が '(' または既に発見
		while( i < cmd.size() - 1 && cmd[i] != ' ' ) ++i;
		while( i < cmd.size() - 1 && cmd[i] == ' ' ) ++i;
		//if( i < cmd.size() - 1 )
		//	gvi.m_cmdArg = cmd.mid(i);
		return true;
	} else
		return false;
}
bool parse_subst(const QString &text, int &ix, QString &pat, QString &after, bool &global) {
	if (ix >= text.size()) {
		return false;
	}
	QChar delim = text[ix++]; // 開始デリミタを決定（通常は '/'）

	// 1. パターン（pat）の抽出とエスケープ解除
	pat.clear();
	while (ix < text.size() && text[ix] != delim) {
		if (text[ix] == '\\') {
			ix++;
			if (ix >= text.size()) {
				pat += '\\';
				break;
			}
			if (text[ix] == delim) {
				pat += text[ix++]; // デリミタのエスケープを解除してパターンに追加
			} else {
				pat += '\\';
				pat += text[ix++];
			}
		} else {
			pat += text[ix++];
		}
	}

	if (ix < text.size() && text[ix] == delim) {
		ix++; // 閉じデリミタをスキップ
	} else {
		return false; // デリミタが一致しない不正な構文
	}

	// --- 空パターンの場合は前回値を参照。新規パターンなら保存する処理を追加 ---
	if (pat.isEmpty()) {
		if (g.m_lastSearchedPat.isEmpty()) {
			return false; // 前回パターンが存在しない場合はエラー（パース失敗）
		}
		pat = g.m_lastSearchedPat; // 前回保存したパターンを再利用
	} else {
		g.m_lastSearchedPat = pat; // 新しく指定されたパターンを保存
	}

	// 2. 置換後文字列（after）の抽出とエスケープ解除
	after.clear();
	while (ix < text.size() && text[ix] != delim) {
		if (text[ix] == '\\') {
			ix++;
			if (ix >= text.size()) {
				after += '\\';
				break;
			}
			if (text[ix] == delim) {
				after += text[ix++]; // デリミタのエスケープのみ解除（\\ は展開側で扱う）
			} else {
				after += '\\';
				after += text[ix++];
			}
		} else {
			after += text[ix++];
		}
	}

	if (ix < text.size() && text[ix] == delim) {
		ix++; // 閉じデリミタをスキップ
	}

	// 3. フラグ（g）の解析
	global = false;
	while (ix < text.size()) {
		QChar flag = text[ix];
		if (flag == 'g') {
			global = true;
		} else {
			break;
		}
		ix++;
	}

	return true;
}
//	diff モード時、1始まり行範囲 [rangeStart, rangeEnd] 内の実在行を物理行番号（1始まり）へ変換し、
//	両ペインのダミー行を除去する。範囲内に実在行が無ければ false を返す（#1451, #1455）
static bool stripDummyAndConvertRange(DocWidget *docWidget, MarkdownEditor *ed, int &rangeStart, int &rangeEnd, bool &removedDummy) {
	removedDummy = false;
	if (docWidget == nullptr || !docWidget->m_diffMode || ed == nullptr)
		return true;
	if (!docWidget->m_editor->dummyInserted() && !docWidget->m_diffview->dummyInserted())
		return true;
	const auto &vbn = ed->diffBlockNumbers();
	int physStart = -1, physEnd = -1;
	for (int line = rangeStart; line <= rangeEnd; ++line) {
		int bn = line - 1;
		if (bn >= 0 && bn < (int)vbn.size() && vbn[bn] >= 0) {
			int phys = vbn[bn] + 1;
			if (physStart < 0) physStart = phys;
			physEnd = phys;
		}
	}
	if (physStart < 0)
		return false;
	docWidget->m_editor->removeAllDummyLines();
	docWidget->m_diffview->removeAllDummyLines();
	removedDummy = true;
	rangeStart = physStart;
	rangeEnd = physEnd;
	return true;
}
void MainWindow::do_global(const QString &text, int ix, QTextCursor& cursor, QTextDocument* doc, DocWidget* docWidget) {
	while (ix < text.length() && text.at(ix).isSpace()) ++ix;
	if (ix >= text.length())	// エラー：セパレータが存在しない
		return;
	QChar sep = text.at(ix);
	if (sep.isLetterOrNumber() || sep.isSpace() || sep == '\\' || sep == '"' || sep == '|')
		return;		// エラー：不正なセパレータ文字
	QString pat;
	int i = ix + 1;
	bool found_end = false;
	for (; i < text.length(); ++i) {	// 4. パターン（pat）の抽出
		QChar c = text.at(i);
		// エスケープ処理（\\ や \<sep> を 2 文字ペアで消費し、\\ の直後のセパレータ誤判定を防ぐ）
		if (c == '\\' && i + 1 < text.length()) {
			pat.append(c);
			pat.append(text.at(i + 1));
			i++;
		} else if (c == sep) {
			found_end = true;
			break; // 2つ目のセパレータが見つかったのでループを抜ける
		} else {
			pat.append(c);
		}
	}
	if (!found_end)
		return;		// エラー：セパレータが閉じられていません
	if( pat.isEmpty() ) pat = g.m_lastSearchedPat;
	if( pat.isEmpty() ) return;
	QString cmd = text.mid(i + 1).trimmed();
	//##qDebug() << "pat = " << pat;
	//##qDebug() << "cmd = " << cmd;
	QRegularExpression::PatternOptions options = QRegularExpression::NoPatternOption;
    if (g.m_ignoreCase) { // 大文字小文字同一視オプションの反映
        options |= QRegularExpression::CaseInsensitiveOption;
    }
    QRegularExpression re(pat, options);
    if (!re.isValid()) {
        //##qDebug() << "Invalid regexp:" << re.errorString();
        return; // パターンが不正な場合は中断
    }
    int rangeStart = gvi.m_nRange == 0 ? 1 : qMax(1, gvi.m_rangeStart);
    int rangeEnd = gvi.m_nRange == 0 ? doc->blockCount() : qMin(doc->blockCount(), gvi.m_rangeEnd);
    bool removedDummy = false;
    if (!stripDummyAndConvertRange(docWidget, gvi.m_editor, rangeStart, rangeEnd, removedDummy))
        return;
    int startIdx = qMax(0, rangeStart - 1);
    int endIdx = qMin(doc->blockCount() - 1, rangeEnd - 1);
    //	マッチ行番号（1始まり）を事前収集する。
    //	削除時の再ハイライトによる userState() == -1 化や無効ブロック参照を防ぐため（#1454）
    QList<int> matchedLines;
    QTextBlock block = doc->findBlockByNumber(startIdx);
    while (block.isValid() && block.blockNumber() <= endIdx) {
        if (block.text().contains(re)) {
            matchedLines.append(block.blockNumber() + 1);
        }
        block = block.next();
    }
    if (matchedLines.isEmpty()) {
        if (removedDummy) do_diff();
        return;
    }
    //	コマンド実行
    gvi.m_editor->openUndoBlock();
    if( is_match(cmd, "d(elete") ) {
	    for(int k = (int)matchedLines.size() - 1; k >= 0; --k) {
	        gvi.m_rangeStart = gvi.m_rangeEnd = matchedLines[k];
	        int ix = 0;
	        do_exCmd(cmd, ix, cursor, doc, docWidget);
	    }
    } else {
	    for(int ln : matchedLines) {
	        gvi.m_rangeStart = gvi.m_rangeEnd = ln;
	        int ix = 0;
	        do_exCmd(cmd, ix, cursor, doc, docWidget);
	    }
    }
    gvi.m_editor->closeUndoBlock();
    if (removedDummy && !docWidget->m_editor->dummyInserted() && !docWidget->m_diffview->dummyInserted())
        do_diff();
}
void MainWindow::do_subst(const QString &text, int ix, QTextDocument* doc) {
	QString pat;
	QString after;
	bool global = false;

	// パースの実行
	if (!parse_subst(text, ix, pat, after, global)) {
		statusBar()->showMessage(tr("Invalid substitute syntax."), 5000);
		return;
	}
	//##qDebug() << "range = " << gvi.m_rangeStart << ", " << gvi.m_rangeEnd;
	//##qDebug() << "pat = " << pat;
	//##qDebug() << "after = " << after;
	//##qDebug() << "global = " << global;
	QRegularExpression::PatternOptions opts = g.m_ignoreCase
		? QRegularExpression::CaseInsensitiveOption : QRegularExpression::NoPatternOption;
	QRegularExpression rx(pat, opts);
	if (!rx.isValid()) {
		statusBar()->showMessage(tr("Invalid regular expression."), 5000);
		return;
	}
	auto expandSubstAfter = [&after](const QRegularExpressionMatch &match) -> QString {
		QString res;
		res.reserve(after.size());
		for (int i = 0; i < after.size(); ++i) {
			QChar c = after.at(i);
			if (c == u'\\' && i + 1 < after.size()) {
				QChar next = after.at(i + 1);
				if (next == u'\\' || next == u'&') {
					res.append(next);
					++i;
				} else if (next >= u'0' && next <= u'9') {
					int idx = next.unicode() - '0';
					if (idx <= match.lastCapturedIndex())
						res.append(match.captured(idx));
					++i;
				} else {
					res.append(c);
					res.append(next);
					++i;
				}
			} else if (c == u'&') {
				res.append(match.captured(0));
			} else if (c == u'$' && i + 1 < after.size()) {
				QChar next = after.at(i + 1);
				if (next == u'&') {
					res.append(match.captured(0));
					++i;
				} else if (next >= u'0' && next <= u'9') {
					int idx = next.unicode() - '0';
					if (idx <= match.lastCapturedIndex())
						res.append(match.captured(idx));
					++i;
				} else {
					res.append(c);
				}
			} else {
				res.append(c);
			}
		}
		return res;
	};
	int rangeStart = gvi.m_rangeStart;
	int rangeEnd = gvi.m_rangeEnd;
	DocWidget *docWidget = getCurDocWidget();
	bool removedDummy = false;
	if (!stripDummyAndConvertRange(docWidget, gvi.m_editor, rangeStart, rangeEnd, removedDummy)) {
		statusBar()->showMessage(tr("No match found."), 5000);
		return;
	}
	QTextCursor cursor(doc);
	cursor.beginEditBlock();
	bool undoOpened = false;
	int replacedCount = 0;
	int lastReplacedLine = -1;
	for (int line = rangeStart; line <= rangeEnd; ++line) {
		QTextBlock block = doc->findBlockByNumber(line - 1);	// ex行番号は1始まり
		if (!block.isValid())
			continue;
		QString lineText = block.text();
		QString newLineText = lineText;
		if (global) {
			QRegularExpressionMatchIterator it = rx.globalMatch(lineText);
			if (it.hasNext()) {
				QString out;
				out.reserve(lineText.size());
				int lastEnd = 0;
				while (it.hasNext()) {
					QRegularExpressionMatch match = it.next();
					out.append(QStringView(lineText).mid(lastEnd, match.capturedStart(0) - lastEnd));
					out.append(expandSubstAfter(match));
					lastEnd = match.capturedEnd(0);
				}
				out.append(QStringView(lineText).mid(lastEnd));
				newLineText = out;
			}
		} else {
			// 行内の「最初」のマッチのみ置換
			QRegularExpressionMatch match = rx.match(lineText);
			if (match.hasMatch()) {
				newLineText.replace(match.capturedStart(0), match.capturedLength(0), expandSubstAfter(match));
			}
		}
		if (newLineText != lineText) {
			if (!undoOpened && gvi.m_editor != nullptr) {
				gvi.m_editor->openUndoBlock();
				undoOpened = true;
			}
			QTextCursor lineCursor(block);
			lineCursor.movePosition(QTextCursor::StartOfBlock);
			lineCursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
			if (gvi.m_editor != nullptr)
				gvi.m_editor->do_insertText(lineCursor, newLineText);
			else
				lineCursor.insertText(newLineText);
			replacedCount++;
			lastReplacedLine = line;
		}
	}
	if (undoOpened)
		gvi.m_editor->closeUndoBlock();
	cursor.endEditBlock();
	if (lastReplacedLine > 0 && gvi.m_editor != nullptr) {
	    QTextBlock block = doc->findBlockByNumber(lastReplacedLine - 1);
	    if (block.isValid()) {
	        QTextCursor c(doc);
	        c.setPosition(block.position());
	        hat(c);
	        gvi.m_editor->setTextCursor(c);
	    }
	}
	if (removedDummy && replacedCount == 0)
		do_diff();
	if (replacedCount == 0) {
        statusBar()->showMessage(tr("No match found."), 5000);
    } else {
        statusBar()->showMessage(tr("%1 line(s) substituted.").arg(replacedCount), 5000);
    }
}
void MainWindow::do_vi_search(const QString& text, QTextCursor& cursor, int rcnt, DocWidget* docWidget) {
	QTextDocument *doc = cursor.document();
    QTextCursor currentCursor = cursor;
    
    // 1. 検索方向の判定
    QChar direction = text[0]; // '/' または '?'
    g.m_searchForward = (direction == '/');

    // 2. 検索パターンの抽出
    // コマンドラインの2文字目以降をパターンとする
    QString pat = text.mid(1); 
    if (pat.isEmpty()) {
        // パターンが空の場合は、前回使用した検索パターンを再利用する（Vimの挙動）
        pat = g.m_lastSearchedPat;
    } else {
        // 新しいパターンを履歴に保存
        g.m_lastSearchedPat = pat;
    }
    if (pat.isEmpty()) {
        statusBar()->showMessage(tr("No previous search pattern"), 5000);
        return;
    }
    // 3. 正規表現オブジェクトの作成 (g.m_ignoreCase 設定を反映)
    QRegularExpression::PatternOptions opts = g.m_ignoreCase
        ? QRegularExpression::CaseInsensitiveOption : QRegularExpression::NoPatternOption;
    QRegularExpression rx(pat, opts);
    if (!rx.isValid()) {
        statusBar()->showMessage(tr("Invalid search pattern: %1").arg(rx.errorString()), 5000);
        return;
    }
    // 4. 検索フラグの設定
    QTextDocument::FindFlags flags;
    if (!g.m_searchForward) {
        flags |= QTextDocument::FindBackward;
    }
    // 5. リピート回数（rcnt）を考慮した検索ループとラップスキャン（折り返し検索）
    int loopCount = qMax(1, rcnt);
    QTextCursor matchCursor;
    for (int i = 0; i < loopCount; ++i) {
        // 現在のカーソル位置から検索を試みる
        matchCursor = doc->find(rx, currentCursor, flags);
        // 見つからなかった場合はファイルの端から折り返して再検索（Wrapscan）
        if (matchCursor.isNull()) {
            QTextCursor wrapCursor(doc);
            if (g.m_searchForward) {
                wrapCursor.movePosition(QTextCursor::Start); // 先頭に戻る
            } else {
                wrapCursor.movePosition(QTextCursor::End);   // 末尾に戻る
            }
            matchCursor = doc->find(rx, wrapCursor, flags);
        }
        // それでも見つからない場合はドキュメント内に存在しない
        if (matchCursor.isNull()) {
            statusBar()->showMessage(tr("Pattern not found: %1").arg(pat), 5000);
            return;
        }
        // 次のループ（rcntが2以上の場合）のために検索開始点を更新
        currentCursor = matchCursor;
    }
    // 6. 折りたたまれている領域の中にマッチした場合は自動展開する処理（v0.3 #1416）
    QTextBlock matchedBlock = matchCursor.block();
    while( !matchedBlock.isVisible() ) {
        QTextBlock b = matchedBlock;
        while( (b = b.previous()).isValid() && !b.isVisible() )
            ;
        if( !b.isValid() || !is_folded(b) ) break;
        do_unfold(b);
    }
    // 7. エディタにカーソルを設定し、画面をスクロールしてフォーカスを戻す
    matchCursor.setPosition(matchCursor.anchor());		//	非選択、マッチ先頭に
	//	[v0.3 #1392] マッチした文書を持つビューに着地する
    MarkdownEditor *searchTarget = docWidget->m_editor;
    if( docWidget->m_diffview != nullptr && matchCursor.document() == docWidget->m_diffview->document() )
        searchTarget = docWidget->m_diffview;
    searchTarget->setTextCursor(matchCursor);
    searchTarget->ensureCursorVisible();
    searchTarget->setFocus();
	ui->action_RegExp->setChecked(g.m_regexp = true);
    searchTarget->highlightSearchText(pat);
    statusBar()->showMessage(tr("Searched: %1").arg(pat), 3000);
	m_searchHist.push_front(pat);
	m_searchHist.removeDuplicates();	//	重複削除
	gvi.m_searchhist_ix = -1;
	m_searchCB->clear();
    m_searchCB->addItems(m_searchHist);
    m_searchCB->setCurrentText(pat);
}
void MainWindow::on_cmdLine_enter() {
	//##qDebug() << "MainWindow::on_cmdLine_enter()";
	m_cmdLine->hide();
	QString text = m_cmdLine->text();
	//##qDebug() << "cmdline text = " << text;
	close_cmdLine();
    DocWidget *docWidget = getCurDocWidget();
    if( docWidget == nullptr ) {
		//	文書が無い状態では、文書を必要としないコマンドだけ実行する
		if( m_prefixLabel->text() == ":" ) {
			do_exCmd_noDoc(text);
			DocWidget *dw = getCurDocWidget();
			if( dw != nullptr ) dw->m_editor->setFocus();
		}
		return;
	}
	QTextCursor cursor = gvi.m_prevFocusWidget == (QWidget*)docWidget->m_editor ?
							docWidget->m_editor->textCursor() :
							docWidget->m_diffview != nullptr && gvi.m_prevFocusWidget == (QWidget*)docWidget->m_diffview ?
							docWidget->m_diffview->textCursor() : docWidget->m_preview->textCursor();
	MarkdownEditor *curEd = (docWidget->m_diffview != nullptr && gvi.m_prevFocusWidget == (QWidget*)docWidget->m_diffview)
								? docWidget->m_diffview : docWidget->m_editor;
	gvi.m_editor = curEd;
	auto t = m_prefixLabel->text();
	if( t == "/" || t == "?" ) {
		text = t + text;
		do_vi_search(text, cursor, gvi.m_repeatCount, docWidget);
		//	検索処理終了時にも vicmd 完結時処理をコールする（#0365）
		QTextCursor cur2 = curEd->textCursor();
		do_viCmd_completed(cur2, t[0]);
	} else
		do_exCmd(text, cursor);
}
//	文書が無い状態（タブ全閉じ）で ':' が押されたら ex コマンドラインを開く
bool MainWindow::eventFilter(QObject *obj, QEvent *event) {
	if( event->type() == QEvent::KeyPress && getCurDocWidget() == nullptr
		&& QApplication::activeModalWidget() == nullptr && !m_cmdLine->isVisible() ) {
		QKeyEvent *ke = static_cast<QKeyEvent*>(event);
		if( ke->text() == ":" && (ke->modifiers() & (Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier)) == 0 ) {
			QWidget *fw = QApplication::focusWidget();
			if( fw == nullptr || (qobject_cast<QLineEdit*>(fw) == nullptr
				&& qobject_cast<QTextEdit*>(fw) == nullptr && qobject_cast<QPlainTextEdit*>(fw) == nullptr) ) {
				open_cmdLine();
				return true;
			}
		}
	}
	return QMainWindow::eventFilter(obj, event);
}
void MainWindow::open_cmdLine() {
	statusBar()->clearMessage();
	gvi.m_currentMode = ViMode::CommandLine;
	gvi.m_prevFocusWidget = ui->tabWidget;		//	フォーカスの戻り先（文書が無いのでタブウィジェット）
	m_prefixLabel->setText(":");
	m_prefixLabel->show();
	m_cmdLine->setText("");
	m_cmdLine->show();
	m_cmdLine->setFocus();
	while( gvi.m_exhist.size() > 64 ) gvi.m_exhist.pop_back();
	gvi.m_exhist_ix = -1;
}
//	文書が無い状態で実行できる ex コマンド（:e, :pwd, :cd, :merge のみ。それ以外は illegal command）
void MainWindow::do_exCmd_noDoc(const QString &text) {
	int ix = 0;
	while( ix < text.size() && (text[ix] == u' ' || text[ix] == u'\t') ) ++ix;
	QString cmd, arg;
	while( ix < text.size() && text[ix].isLetter() ) cmd += text[ix++];
	if( ix < text.size() )
		arg = text.mid(ix).trimmed();
	if( is_match(cmd, "pwd") ) {
		do_output(QString("\ncurrent directory: %1\n").arg(QDir::currentPath()));
	} else if( is_match(cmd, "cd") ) {
		if( !g.m_defaultDir.isEmpty() ) {
			QDir::setCurrent(g.m_defaultDir);
			do_output(QString("\ncurrent directory: %1\n").arg(QDir::currentPath()));
		}
	} else if( is_match(cmd, "e(dit") ) {
		if( arg.isEmpty() )
			onAction_Open();
		else
			do_open("", arg);
	} else if( is_match(cmd, "me(rge") ) {
		do_merge(arg);
	} else {
		statusBar()->showMessage(tr("illegal command."), 5000);
	}
	gvi.m_exhist.push_front(text);
	gvi.m_exhist_ix = -1;
}
void MainWindow::do_exCmd(const QString &text, QTextCursor& cursor) {
    DocWidget *docWidget = getCurDocWidget();
    if( docWidget == nullptr ) return;
	//QTextCursor cursor = gvi.m_prevFocusWidget == (QWidget*)docWidget->m_editor ?
	//						docWidget->m_editor->textCursor() : docWidget->m_preview->textCursor();
	QTextDocument *doc = cursor.document();
	//if( text[0] == '/' || text[0] == '?' ) {
	//	do_vi_search(text, cursor, gvi.m_repeatCount, docWidget);
	//	return;
	//}
	//assert( text[0] == ':' );
	int totalLines = doc->blockCount();
	if( doc->lastBlock().text().isEmpty() && totalLines > 1 ) --totalLines;
	//int ix = 1;		//	skip ':'
	int ix = 0;
	auto skipSpaces = [&]() {
		while( ix < text.size() && (text[ix] == u' ' || text[ix] == u'\t') ) ++ix;
	};
	skipSpaces();
	gvi.m_nRange = 0;
	gvi.m_rangeStart = 1;
	if( ix < text.size() && text[ix] == '%' ) {
	    ++ix;
	    skipSpaces();
	    gvi.m_nRange = 2;
	    gvi.m_rangeStart = 1;
	    gvi.m_rangeEnd   = totalLines;
	} else {
		int currentLine = cursor.block().blockNumber() + 1;
		int nr = 0;
		for(;;) {
			skipSpaces();
			int startIx = ix;
			++nr;
			gvi.m_rangeEnd = parseLineSpec(text, ix, currentLine, totalLines, doc);
			//##qDebug() << "line = " << gvi.m_rangeEnd;
			if( ix == startIx ) {
				if( nr == 1 )
					gvi.m_rangeStart = gvi.m_rangeEnd = currentLine;
				break;	//	行番号無し
			}
			++gvi.m_nRange;
			if( gvi.m_rangeEnd < 0 ) {
				statusBar()->showMessage(tr("Pattern not found / invalid range."), 5000);
				return;
			}
			skipSpaces();
			if( ix >= text.size() || text[ix] != ',' && text[ix] != ';' ) break;
			if( text[ix] == ';' ) currentLine = gvi.m_rangeEnd;
			++ix;
			gvi.m_rangeStart = gvi.m_rangeEnd;
		}
		if( nr == 1 )
			gvi.m_rangeStart = gvi.m_rangeEnd;
	}
	skipSpaces();
	if( ix >= text.size() || text[ix] == u'\n' ) {	//	:{range} Enter の場合
		int targetBn = qBound(0, gvi.m_rangeEnd - 1, qMax(0, doc->blockCount() - 1));
		QTextBlock block = doc->findBlockByNumber(targetBn);
		if( !block.isValid() ) return;
		cursor.setPosition(block.position());
		hat(cursor);
		if( gvi.m_prevFocusWidget == (QWidget*)docWidget->m_editor )
			docWidget->m_editor->setTextCursor(cursor);
		else if( docWidget->m_diffview != nullptr && gvi.m_prevFocusWidget == (QWidget*)docWidget->m_diffview )
			docWidget->m_diffview->setTextCursor(cursor);
		else
			docWidget->m_preview->setTextCursor(cursor);
		gvi.m_exhist.push_front(text);
		//if( gvi.m_exhist.isEmpty() ) gvi.m_exhist.resize(1);
		//gvi.m_exhist[0] = text;
		gvi.m_exhist_ix = -1;
			return;
	}
	if( gvi.m_rangeStart > gvi.m_rangeEnd )
		std::swap(gvi.m_rangeStart, gvi.m_rangeEnd);
	gvi.m_inGlobal = false;
	do_exCmd(text, ix, cursor, doc, docWidget);
	//QString cmd;
	//while( ix < text.size() && text[ix].isLetter() ) cmd += text[ix++];
	//QChar nch = ix < text.size() ? text[ix] : u'\0';
	//do_exCmd(cmd, cursor, doc, docWidget);
}
void MainWindow::do_exCmd(const QString &text, int ix, /*QString cmd, QChar nch,*/ QTextCursor &cursor, QTextDocument*doc, DocWidget* docWidget) {
	while( ix < text.size() && (text[ix] == u' ' || text[ix] == u'\t') ) ++ix;
	QString cmd, arg;
	while( ix < text.size() && text[ix].isLetter() ) cmd += text[ix++];
	QChar nch = ix < text.size() ? text[ix] : u'\0';
	int argIx = (nch == u'!') ? ix + 1 : ix;
	if( argIx < text.size() )
		arg = text.mid(argIx).trimmed();
	//QChar nch;
	//if( cmd.endsWith(u'!') ) {
	//	nch = u'!';
	//	cmd = cmd.left(cmd.size() - 1);
	//}
	if( is_match(cmd, "x") ) {
		if( do_save() )
			do_close();
	} else if( is_match(cmd, "q(uit") ) {
		do_close(nch == u'!');
	} else if( is_match(cmd, "pwd") ) {
		do_output(QString("\ncurrent directory: %1\n").arg(QDir::currentPath()));
	} else if( is_match(cmd, "cd") ) {
		QString targetDir = arg;
		if( targetDir.isEmpty() ) {
			targetDir = g.m_defaultDir;
		} else if( targetDir == u"~" ) {
			targetDir = QDir::homePath();
		} else if( targetDir.startsWith(u"~/") || targetDir.startsWith(u"~\\") ) {
			targetDir = QDir::homePath() + targetDir.mid(1);
		}
		if( !targetDir.isEmpty() ) {
			if( QDir::setCurrent(targetDir) ) {
				do_output(QString("\ncurrent directory: %1\n").arg(QDir::currentPath()));
			} else {
				statusBar()->showMessage(tr("Cannot change directory: %1").arg(targetDir), 5000);
				do_output(QString("\ncannot change directory: %1\n").arg(targetDir));
			}
		}
	} else if( is_match(cmd, "e(dit") ) {
		//if( nch == u'\0')
		if( arg.isEmpty() )
			onAction_Open();
		else {
			//QString arg = gvi.m_cmdArg.trimmed();
			//QString arg = text.mid(ix).trimmed();
			do_open("", arg);
		}
	} else if( is_match(cmd, "w(rite") ) {
		if( arg.isEmpty() ) {
			onAction_Save();
		} else {
			do_save(arg);
		}
	} else if( is_match(cmd, "d(elete") ) {
		int rangeStart = gvi.m_rangeStart;
		int rangeEnd = gvi.m_rangeEnd;
		bool removedDummy = false;
		if( stripDummyAndConvertRange(docWidget, gvi.m_editor, rangeStart, rangeEnd, removedDummy) ) {
			QTextBlock startBlock = doc->findBlockByNumber(rangeStart - 1);
			QTextBlock endBlock = doc->findBlockByNumber(rangeEnd - 1);
			if( startBlock.isValid() && endBlock.isValid() ) {
				QTextCursor cursor(doc);
				cursor.beginEditBlock(); // Undo/Redo を1回にまとめる
				
				int startPos = startBlock.position();
				int endPos;
				
				if( endBlock.next().isValid() ) {
					// 次の行が存在する場合は、次の行の開始位置まで選択（対象行の末尾改行も削除に含める）
					endPos = endBlock.next().position();
				} else {
					// 最終行を削除する場合
					endPos = doc->characterCount() - 1;
					// 前の行が存在する場合は、前の行の末尾改行から削除対象に含める（不要な空行残りを防止）
					if( startBlock.previous().isValid() ) {
						startPos = startBlock.previous().position() + startBlock.previous().length() - 1;
					}
				}
				if( endPos > startPos ) {
					cursor.setPosition(startPos);
					cursor.setPosition(endPos, QTextCursor::KeepAnchor);
					//cursor.removeSelectedText();
					gvi.m_editor->do_deleteText(cursor);
				}
				cursor.endEditBlock();
				hat(cursor);
				QTextBlock block = cursor.block();
				if( block.text().isEmpty() && block.blockNumber() == doc->blockCount() - 1 && block.previous().isValid() ) {
				    // 末尾まで削除した場合：カーソルを前の行の非空白先頭へ
				    cursor.setPosition(block.previous().position());
				    hat(cursor);
				}
				gvi.m_editor->setTextCursor(cursor);
			}
		}
	} else if( is_match(cmd, "p(rint") ) {
		const bool isDiff = docWidget->m_diffMode && gvi.m_editor != nullptr && !gvi.m_editor->diffBlockNumbers().empty();
		for(int ln = gvi.m_rangeStart; ln <= gvi.m_rangeEnd; ++ln) {
			int bn = ln - 1;
			if( isDiff ) {
				const auto &vbn = gvi.m_editor->diffBlockNumbers();
				if( bn >= 0 && bn < (int)vbn.size() && vbn[bn] < 0 ) continue;
			}
			QTextBlock block = doc->findBlockByNumber(bn);
			if( !block.isValid() ) break;
			do_output(block.text() + "\n");
		}
	} else if( is_match(cmd, "P(RINT") || is_match(cmd, "nu(mber") ) {
		const bool isDiff = docWidget->m_diffMode && gvi.m_editor != nullptr && !gvi.m_editor->diffBlockNumbers().empty();
		do_output("\n\"" + docWidget->m_fullPath + "\"\n");
		for(int ln = gvi.m_rangeStart; ln <= gvi.m_rangeEnd; ++ln) {
			int bn = ln - 1;
			int dispLn = ln;
			if( isDiff ) {
				const auto &vbn = gvi.m_editor->diffBlockNumbers();
				if( bn >= 0 && bn < (int)vbn.size() ) {
					if( vbn[bn] < 0 ) continue;
					dispLn = vbn[bn] + 1;
				}
			}
			QTextBlock block = doc->findBlockByNumber(bn);
			if( !block.isValid() ) break;
			do_output(QString("%1:").arg(dispLn, 4) + block.text() + "\n");
		}
	} else if( is_match(cmd, "s(ubstitute") ) {
		do_subst(text, ix, doc);
	} else if( is_match(cmd, "g(lobal") ) {
		if( gvi.m_inGlobal ) {
			statusBar()->showMessage(tr("Cannot do :global recursive."), 5000);
			return;
		}
		gvi.m_inGlobal = true;
		do_global(text, ix, cursor, doc, docWidget);
		gvi.m_inGlobal = false;
	} else if( is_match(cmd, "di(ff") ) {
		DocWidget *docWidget = getCurDocWidget();
		if( docWidget == nullptr ) return;
		if( arg.isEmpty() )
			onAction_DiffMode(!docWidget->m_diffMode);
		else
			do_diff(arg);
	} else if( is_match(cmd, "me(rge") ) {
		do_merge(arg);
	} else {
		statusBar()->showMessage(tr("illegal command."), 5000);
	}
	if( !gvi.m_inGlobal ) {
		gvi.m_exhist.push_front(text);
		gvi.m_exhist_ix = -1;
	}
}
void MainWindow::on_cmdLine_escape() {
	//##qDebug() << "MainWindow::on_cmdLine_escape()";
	close_cmdLine();
}
void MainWindow::on_cmdLine_up() {	//	コマンドライン履歴
	const QString pref = m_prefixLabel->text();
	if( pref.isEmpty() ) return;
	QChar ch = pref[0];
	if( ch == ':' ) {
		if( gvi.m_exhist_ix + 1 < gvi.m_exhist.size() ) {
			m_cmdLine->setText(gvi.m_exhist[++gvi.m_exhist_ix]);
		}
	} else if( ch == '/' || ch == '?' ) {
		if( gvi.m_searchhist_ix + 1 < m_searchHist.size() ) {
			m_cmdLine->setText(m_searchHist[++gvi.m_searchhist_ix]);
		}
	}
}
void MainWindow::on_cmdLine_down() {
	const QString pref = m_prefixLabel->text();
	if( pref.isEmpty() ) return;
	QChar ch = pref[0];
	if( ch == ':' ) {
		if( gvi.m_exhist_ix > 0 ) {
			m_cmdLine->setText(gvi.m_exhist[--gvi.m_exhist_ix]);
		} else if( gvi.m_exhist_ix == 0 ) {
			gvi.m_exhist_ix = -1;
			m_cmdLine->clear();
		}
	} else if( ch == '/' || ch == '?' ) {
		if( gvi.m_searchhist_ix > 0 ) {
			m_cmdLine->setText(m_searchHist[--gvi.m_searchhist_ix]);
		} else if( gvi.m_searchhist_ix == 0 ) {
			gvi.m_searchhist_ix = -1;
			m_cmdLine->clear();
		}
	}
}

void MainWindow::close_cmdLine() {
	m_prefixLabel->hide();
	m_cmdLine->hide();
	gvi.m_currentMode = ViMode::Normal;
	//gvi.m_cmdlineMode = false;
	gvi.m_pendingCommand.clear();
	if( gvi.m_prevFocusWidget != nullptr )
		gvi.m_prevFocusWidget->setFocus();
#ifdef Q_OS_WIN
	HWND hwnd;
	if( qobject_cast<MarkdownEditor*>(gvi.m_prevFocusWidget) != nullptr
		|| qobject_cast<MarkdownPreview*>(gvi.m_prevFocusWidget) != nullptr )
		hwnd = (HWND)gvi.m_prevFocusWidget->winId();
	else
		hwnd = (HWND)this->winId();
	HIMC himc = ImmGetContext(hwnd);
	if( himc ) {
		ImmSetOpenStatus(himc, FALSE);
		ImmReleaseContext(hwnd, himc);
	}
#endif

}
