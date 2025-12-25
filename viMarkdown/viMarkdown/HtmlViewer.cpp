#include <QTextBlock>
#include "HtmlViewer.h"

void HtmlViewer::mousePressEvent(QMouseEvent *e)
{
	if (e->button() == Qt::LeftButton) {
	    // 1. クリックされた位置のカーソルオブジェクトを取得
	    // viewport()->mapFromGlobal(e->globalPos()) ではなく e->pos() でOKです
	    QTextCursor cursor = cursorForPosition(e->pos());

	    // 2. その位置のブロック番号を取得（0ベース）
	    int blockNumber = cursor.blockNumber();

	    // 3. シグナルを発行
	    emit lineClicked(blockNumber);
	}

    // 4. 標準の挙動（カーソル移動や選択など）を維持するために親クラスのイベントを呼ぶ
    QTextEdit::mousePressEvent(e);
}
