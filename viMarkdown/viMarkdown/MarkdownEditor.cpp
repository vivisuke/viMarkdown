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
        if( text.mid(n).startsWith("- ") )
			atxt += "- ";
        cursor.insertText("\n" + atxt);
        // カーソル位置を画面内に維持
        this->ensureCursorVisible();
		return;
	}
    QPlainTextEdit::keyPressEvent(e);	// Enter 以外のキーは通常通りの処理
}
