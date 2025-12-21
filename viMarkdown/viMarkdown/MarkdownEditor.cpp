#include <QPlainTextEdit>
#include <QTextCursor>
#include <QTextBlock>
#include "MarkdownEditor.h"

void MarkdownEditor::keyPressEvent(QKeyEvent *e) {
	if (e->key() == Qt::Key_Return || e->key() == Qt::Key_Enter) {
		QTextCursor cursor = this->textCursor();
        QTextBlock currentBlock = cursor.block();
        QString text = currentBlock.text();
        int n = 0;
        while( n < text.length() && text[n].isSpace() ) ++n;
        QString atxt = text.left(n);
        if( text.mid(n).startsWith("- [ ] ") )
			atxt += "- [ ] ";
        else if( text.mid(n).startsWith("- [x] ") || text.mid(n).startsWith("- [X] ") )
			atxt += "- [x] ";
        else if( text.mid(n).startsWith("- ") )
			atxt += "- ";
        cursor.insertText("\n" + atxt);
        // カーソル位置を画面内に維持
        this->ensureCursorVisible();
		return;
	}
    QPlainTextEdit::keyPressEvent(e);	// Enter 以外のキーは通常通りの処理
}
//void MarkdownEditor::scrollToTop(int ln) {		//	ln: 0 org.
//}
void MarkdownEditor::scrollToTop(const QTextCursor &cursor) {
	int visualLineNum = getVisualLineNumber(cursor);
    this->scrollToTop(visualLineNum);
}
int MarkdownEditor::getVisualLineNumber(const QTextCursor &cursor) const {
	int visualLineNum = 0;
    QTextBlock targetBlock = cursor.block();

    // 1. カーソルがあるブロックより前の全ブロックの表示行数を合計する
    for (QTextBlock block = document()->begin(); block != targetBlock; block = block.next()) {
        if (block.isValid()) {
            // block.layout() から、そのブロックが何行に折り返されているかを取得
            visualLineNum += block.layout()->lineCount();
        }
    }

    // 2. カーソルがある現在のブロック内で、カーソルが「何行目の折り返し」にいるかを取得
    // cursor.positionInBlock() はブロック先頭からの文字数
    int relativePos = cursor.position() - targetBlock.position();
    
    // layout()->lineForTextPosition(n) で、文字位置 n が含まれる QTextLine を取得できる
    int lineInBlock = targetBlock.layout()->lineForTextPosition(relativePos).lineNumber();

    return visualLineNum + lineInBlock;
}
