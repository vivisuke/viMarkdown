//----------------------------------------------------------------------
//
//			File:			"diff.cpp"
//			Created:		04-7-2026
//			Author:			vivisuke
//			Description:
//
//----------------------------------------------------------------------

#include <QTextDocument>
#include <QTextBlock>
#include <QFileDialog>
#include <QMessageBox>
#include <QLabel>
#include <QTimer>
#include <vector>
#include <assert.h>
#include "dtl/dtl.hpp"
#include "MainWindow.h"
#include "ui_MainWindow.h"
#include "DocWidget.h"
#include "MarkdownEditor.h"
#include "MarkdownPreview.h"
#include "diff.h"

CharType getCharType(QChar ch);

// 1. ダミー行（高さを揃えるための空行）かどうかの判定
bool isDummyLine(MarkdownEditor* editor, const QTextBlock &block) {
    const auto &vbn = editor->diffBlockNumbers();
    int bn = block.blockNumber();
    return bn < vbn.size() && vbn[bn] < 0;
    //return block.userState() == 0;
}
#if 1
// 2. 行番号（1オリジン）取得（2ビット右シフトするだけ）
int lineNumber(MarkdownEditor* editor, const QTextBlock &block) {
    auto vbn = editor->diffBlockNumbers();
    auto ix = block.blockNumber();
    if( ix < vbn.size() )
	    return editor->diffBlockNumbers()[block.blockNumber()] + 1;
    else
    	return 0;
    //return (unsigned)block.userState() >> 2;
}

// 3. 差分の有無（下位2ビットが 0なら差分なし）
int getDiff(MarkdownEditor*editor, const QTextBlock &block) {
    const auto &flgs = editor->diffFlags();
    int bn = block.blockNumber();
    if( bn < flgs.size() )
	    return editor->diffFlags()[block.blockNumber()];
    else
    	return 0;
    //return (block.userState() & 0x03);
}
bool hasDiff(MarkdownEditor*editor, const QTextBlock &block) {
	return getDiff(editor, block) != 0;
}

// ダミー行をセットする場合
void setDummyLine(QTextBlock block) {
    block.setUserState(0);
}

// 物理的な行をセットする場合
void setPhysicalLine(MarkdownEditor* editor, QTextBlock &block, int ln, uchar flag) {
    editor->diffBlockNumbers().push_back(block.blockNumber() - editor->nDummyLines());
    editor->diffFlags().push_back(flag);
    int state = (ln << 2) | flag;
    block.setUserState(state);
}
#else
// 2. 行番号（1オリジン）取得（1ビット右シフトするだけ）
int lineNumber(const QTextBlock &block) {
    return (unsigned)block.userState() >> 1;
}

// 3. 差分の有無（最下位ビットが 1 なら差分あり / 0 なら差分なし）
bool hasDiff(const QTextBlock &block) {
    return (block.userState() & 0x01) != 0;
}

// ダミー行をセットする場合
void setDummyLine(QTextBlock block) {
    block.setUserState(0);
}

// 物理的な行をセットする場合
void setPhysicalLine(QTextBlock &block, int ln, bool changed) {
    int state = (ln << 1) | (changed ? 1 : 0);
    block.setUserState(state);
}
#endif
int visualLineCount(const QTextBlock &block) {
#if 1
	int vc = 1;			//	表示行数
	QTextLayout *layout = block.layout();
	if( layout != 0 ) vc = layout->lineCount();
	return vc;
#else
	QTextLayout *layout = block.layout();
	//if( layout != 0 ) vc = layout->lineCount();
	if( layout == nullptr ) {
		qreal width = block.document()->textWidth();
        if (width <= 0) {
            return 1; // 幅が未定なら 1行扱い
        }
        layout->beginLayout();
        while (true) {
            QTextLine line = layout->createLine();
            if (!line.isValid()) break;
            line.setLineWidth(width);
        }
        layout->endLayout();
	}
	return qMax(1, layout->lineCount());
#endif
}
int visualLineCount(QTextDocument *doc) {
	int vc = 0;
	QTextBlock block = doc->begin();
	while( block.isValid() ) {
		vc += visualLineCount(block);
		block = block.next();
	}
	return vc;
}
//
void MarkdownEditor::removeAllDummyLines() {
    QTextDocument *doc = document();
    if (!doc) return;

    QTextCursor cursor(doc);
    // 描画更新を一時停止し、処理を高速化
    cursor.beginEditBlock();

    // 末尾のブロックから先頭に向かって逆順に巡回します
    QTextBlock block = doc->lastBlock();
    while (block.isValid()) {
        // 削除操作によってブロックが破棄される前に、前のブロックへの参照を確保しておく
        QTextBlock prevBlock = block.previous();

        if (isDummyLine(this, block)) {
            cursor.setPosition(block.position());

            if (block.next().isValid()) {
                // パターンA: 末尾以外のブロックを削除する場合
                // 「自ブロックの先頭」から「次のブロックの先頭」までを選択（これで改行コードも含まれます）
                cursor.movePosition(QTextCursor::NextBlock, QTextCursor::KeepAnchor);
                cursor.removeSelectedText();
            } else {
                // パターンB: 末尾のブロックを削除する場合
                if (prevBlock.isValid()) {
                    // 前の行が存在する場合、手前の改行コードも含めて後方から選択・削除します
                    cursor.movePosition(QTextCursor::EndOfBlock);
                    cursor.movePosition(QTextCursor::StartOfBlock, QTextCursor::KeepAnchor);
                    cursor.movePosition(QTextCursor::PreviousCharacter, QTextCursor::KeepAnchor); // 手前の改行を巻き込む
                    cursor.removeSelectedText();
                } else {
                    // ドキュメントにこの1行しか残っていない場合は、プレーンにテキストのみクリア
                    cursor.select(QTextCursor::BlockUnderCursor);
                    cursor.removeSelectedText();
                }
            }
        }

        // 次の巡回先（手前のブロック）に移行
        block = prevBlock;
    }

    cursor.endEditBlock(); // レイアウトを再計算して画面を更新
}
// -------------------------------------------------------------
#if 0
void updateMapSub(QPainter &p, int x, int width, QTextDocument* doc, int totalLines, int mapHeight) {
	if (totalLines <= 0) return;

	QTextBlock block = doc->begin();
	int currentVLine = 0; // 現在のビジュアル行インデックス

	while (block.isValid()) {
		QColor col = Qt::white;
		if (isDummyLine(block)) {
			col = QColor("#e8e8e8");
		} else {
			auto d = getDiff(block);
			if (d == ADDED_LINE) col = QColor("#ffa0a0");
			else if (d == CHANGED_LINE) col = QColor("#ffffa0");
		}

		int vc = visualLineCount(block);

		// このブロック（行）の MiniMap 上での Y 座標と高さを比率計算
		int y1 = currentVLine * mapHeight / totalLines;
		int y2 = (currentVLine + vc) * mapHeight / totalLines;
		int h = qMax(1, y2 - y1); // 最低でも1pxは描画

		p.fillRect(x, y1, width, h, col);

		currentVLine += vc;
		block = block.next();
	}
}

void MiniMap::updateMap(QTextDocument* doc1, QTextDocument* doc2) {
	auto ht = rect().height();
	m_mapPixmap = QPixmap(MINMAP_WIDTH, ht);
	m_mapPixmap.fill(QColor("#e8e8e8")); // 背景をデフォルト色で初期化

	int totalLines1 = visualLineCount(doc1);
	int totalLines2 = visualLineCount(doc2);
	// 左右で大きい方の行数に合わせる（通常diffアライメントで同一のはず）
	m_totalLines = qMax(totalLines1, totalLines2);

	if (m_totalLines <= 0) return;

	QPainter p(&m_mapPixmap);
	
	int halfW = MINMAP_WIDTH / 2;
	// 左ペイン用 (0 〜 halfW)
	updateMapSub(p, 0, halfW, doc1, m_totalLines, ht);
	// 右ペイン用 (halfW 〜 MINMAP_WIDTH)
	updateMapSub(p, halfW, MINMAP_WIDTH - halfW, doc2, m_totalLines, ht);

	// ※末尾の余白塗りつぶしは不要になります（全体にスケーリングされるため）
}
#else
void updateMapSub(QPainter &p, int x, QTextDocument* doc, MarkdownEditor* editor) {
	QTextBlock block = doc->begin();
	for(int y = 0; /*y < doc->blockCount() &&*/ block.isValid(); block=block.next()) {
		QColor col = Qt::white;		//QColor("#808080");
		if( isDummyLine(editor, block) ) col = QColor("#e8e8e8");
		//else if( hasDiff(block) ) col = QColor("#ffa0a0");	//QColor("#ccffcc");	//QColor("#ffecec");
		else {
			auto d = getDiff(editor, block);
			if( d == ADDED_LINE ) col = QColor("#ffa0a0");
			else if( d == CHANGED_LINE ) col = QColor("#ffffa0");
		}
		p.setPen(col);
		int vc = visualLineCount(block);
		for(int k = 0; k < vc; ++k) {
			p.drawLine(x, y, x+MINMAP_WIDTH/2-1, y);
			++y;
		}
	}
}
void MiniMap::updateMap(QTextDocument* doc1, MarkdownEditor* editor1, QTextDocument* doc2, MarkdownEditor* editor2) {
	//m_mapPixmap = QPixmap(MINMAP_WIDTH, doc1->blockCount());
	auto ht = rect().height();
	m_mapPixmap = QPixmap(MINMAP_WIDTH, ht);
	QPainter p(&m_mapPixmap);
	int x = 0;
	updateMapSub(p, 0, doc1, editor1);
	updateMapSub(p, MINMAP_WIDTH/2, doc2, editor2);
	p.setBrush(QColor("#e8e8e8"));
	//p.drawRect(0, doc1->blockCount(), MINMAP_WIDTH, ht - doc1->blockCount());
	p.drawRect(0, visualLineCount(doc1), MINMAP_WIDTH, ht - visualLineCount(doc1));
}
#endif
// -------------------------------------------------------------
std::vector<QString> extractLinesFromDocument(const QTextDocument *doc) {
    std::vector<QString> lines;
    if (!doc) return lines;

    // メモリ確保のオーバーヘッドを減らすため大まかにリザーブ
    lines.reserve(doc->blockCount());

    // QTextBlock を走査することで、巨大なテキストでも分割時のメモリ消費を最小限に抑えます
    for (QTextBlock block = doc->begin(); block.isValid(); block = block.next()) {
        lines.push_back(block.text() /*+u'\n'*/);
    }
    //lines.back() += QChar(0xffff);
    lines.back() += QString("\n")+ QChar(0xffff);		//	改行＋EOF
    return lines;
}
void MainWindow::diffview_open() {
	DocWidget *docWidget = getCurDocWidget();
	if( docWidget == nullptr || !docWidget->m_diffMode )
		return;
	QString fullPath = QFileDialog::getOpenFileName(
		this,
		"select diff file",			// ダイアログのタイトル
		QDir::currentPath(),		// 初期ディレクトリ
		"markdown file (*.md *.markdown);;text file(*.txt);;all(*.*)"	// フィルター
	);
	if( fullPath.isEmpty() ) return;
	QFile file(fullPath);
	if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
		QMessageBox::warning(this, tr("Error"), tr("Cannot open file:\n%1").arg(fullPath));
		return;
	}
	QTextStream in(&file);
	in.setAutoDetectUnicode(true); 
    QString content = in.readAll();
    //auto encoding = in.encoding();
    file.close();
    QFileInfo fi2(fullPath);
    m_diffviewLabel->setText(fi2.fileName());
	docWidget->m_diffview->setPlainText(content);
}
void MainWindow::onAction_DiffWithFile() {
	//DocWidget *docWidget = getCurDocWidget();
	//if( docWidget == nullptr ) return;
	QString fullPath = QFileDialog::getOpenFileName(
		this,
		"select diff file",			// ダイアログのタイトル
		QDir::currentPath(),		// 初期ディレクトリ
		"markdown file (*.md *.markdown);;text file(*.txt);;all(*.*)"	// フィルター
	);
	if( fullPath.isEmpty() ) return;
	do_diff(fullPath);
}
void MainWindow::do_diff(const QString &fullPath) {
	DocWidget *docWidget = getCurDocWidget();
	if( docWidget == nullptr ) return;
	QFile file(fullPath);
	if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
		QMessageBox::warning(this, tr("Error"), tr("Cannot open file:\n%1").arg(fullPath));
		return;
	}
	QTextStream in(&file);
	in.setAutoDetectUnicode(true); 
    QString content = in.readAll();
    //auto encoding = in.encoding();
    file.close();
    QFileInfo fi2(fullPath);
    m_diffviewLabel->setText(fi2.fileName());
	docWidget->m_diffview->setPlainText(content);
	//
	docWidget->m_diffMode = true;
	docWidget->m_editor->setDiffMode(true);
	docWidget->m_editor->expandAll();
	docWidget->m_editor->setHighlightDiff(true);
	docWidget->m_diffview->setHighlightDiff(true);
	docWidget->m_editor->setHighlightMarkdown(false);
	//docWidget->m_editor->setLineWrapMode(QPlainTextEdit::NoWrap);
	docWidget->m_editor->rehighlight();
	docWidget->updatePanes();
    ui->action_DiffMode->setChecked(true);
    //do_diff();
}
void MainWindow::onAction_DiffMode(bool checked) {
	DocWidget *docWidget = getCurDocWidget();
	if( docWidget == nullptr ) return;
	docWidget->m_diffMode = checked;
	docWidget->m_editor->setDiffMode(checked);
	QScrollBar *bar1 = docWidget->m_editor->horizontalScrollBar();
	QScrollBar *bar2 = docWidget->m_diffview->horizontalScrollBar();
	if (checked) {
		docWidget->m_editor->expandAll();
		docWidget->m_editor->setHighlightDiff(true);
		docWidget->m_diffview->setHighlightDiff(true);
		docWidget->m_editor->setHighlightMarkdown(false);
		//docWidget->m_editor->setLineWrapMode(QPlainTextEdit::NoWrap);
		//##do_diff();
		connect(docWidget->m_editor->verticalScrollBar(), &QScrollBar::valueChanged,
            docWidget, &DocWidget::syncScrollFromLeft);
	    connect(docWidget->m_diffview->verticalScrollBar(), &QScrollBar::valueChanged,
            docWidget, &DocWidget::syncScrollFromRight);
	    docWidget->m_diffview->verticalScrollBar()->setValue(docWidget->m_diffview->verticalScrollBar()->value());
	    int fvl = docWidget->m_editor->verticalScrollBar()->value();
        int vl = docWidget->m_editor->getVisibleLineCount();
        docWidget->setMiniMapCurPos(fvl, vl);
        connect(docWidget->m_editor->verticalScrollBar(), &QScrollBar::valueChanged,
		        docWidget, &DocWidget::syncMinimapWithEditor);
        connect(bar1, &QScrollBar::valueChanged, bar2, &QScrollBar::setValue);
		connect(bar2, &QScrollBar::valueChanged, bar1, &QScrollBar::setValue);
		QTimer::singleShot(0, this, [this]() {
            do_diff();
        });
		//bool b = docWidget->m_editor->document()->isModified();
		//qDebug() << "modified = " << b;
	} else {
		if( docWidget->m_docType == DocType::Markdown )
			docWidget->m_editor->setHighlightMarkdown(true);
		docWidget->m_editor->setHighlightDiff(false);
		docWidget->m_diffview->setHighlightDiff(false);
		//docWidget->m_editor->setLineWrapMode(QPlainTextEdit::WidgetWidth);
		QTextDocument *doc1 = docWidget->m_editor->document();
		QTextDocument *doc2 = docWidget->m_diffview->document();
		bool modified1 = doc1->isModified();
		bool modified2 = doc2->isModified();
#if 0
		if( docWidget->m_editor->dummyInserted() )
			doc1->undo();
		if( docWidget->m_diffview->dummyInserted() )
			doc2->undo();
#else
		if( docWidget->m_editor->dummyInserted() )
            docWidget->m_editor->removeAllDummyLines();
		if( docWidget->m_diffview->dummyInserted() )
            docWidget->m_diffview->removeAllDummyLines();
#endif
	    docWidget->m_editor->setDummyInserted(false);
	    docWidget->m_diffview->setDummyInserted(false);
		doc1->setModified(modified1);
		doc2->setModified(modified2);
		disconnect(docWidget->m_editor->verticalScrollBar(), &QScrollBar::valueChanged,
            docWidget, &DocWidget::syncScrollFromLeft);
	    disconnect(docWidget->m_diffview->verticalScrollBar(), &QScrollBar::valueChanged,
            docWidget, &DocWidget::syncScrollFromRight);
        disconnect(bar1, &QScrollBar::valueChanged, bar2, &QScrollBar::setValue);
		disconnect(bar2, &QScrollBar::valueChanged, bar1, &QScrollBar::setValue);
	}
	//bool b = docWidget->m_editor->document()->isModified();
	//qDebug() << "modified = " << b;
	docWidget->m_editor->rehighlight();
	docWidget->updatePanes();
	//b = docWidget->m_editor->document()->isModified();
	//qDebug() << "modified = " << b;
}
void MainWindow::onDiffViewChanged() {	//	比較先エディタが編集された場合
	//##qDebug() << "MainWindow::onDiffViewChanged()";
    if (m_processing!=0) return;
    ++m_processing;
    do_diff();
    --m_processing;
}
#if 0
void applyInlineHighlight(QTextBlock &block, const DiffBlockUserData *userData, bool isLeft = true) {
    if (!block.isValid()) return;

    // データが空、または差分範囲がない場合はフォーマットを空にしてリセット
    if (!userData || userData->ranges.isEmpty()) {
        block.layout()->setFormats(QList<QTextLayout::FormatRange>()); // 空リストでリセット [1.1]
        return;
    }

    QList<QTextLayout::FormatRange> formats;

    // 1. 強調したい色を設定（ダミー行よりも一段階「濃い」パステルカラー）
    QTextCharFormat format;
    if (isLeft) {
        format.setBackground(QColor("#ffc1c1")); // 削除された文字：少し濃い目のパステル赤
    } else {
        format.setBackground(QColor("#b2f0b2")); // 挿入された文字：少し濃い目のパステル緑
    }

    // 2. 自前の範囲データ（ranges）を、Qtが描画に使う FormatRange 構造体に変換して追加します [1.1, 1.2]
    for (const auto &range : userData->ranges) {
        QTextLayout::FormatRange fRange;
        fRange.start = range.start;
        fRange.length = range.length;
        fRange.format = format;
        formats.append(fRange);
    }

    // 3. 【マジック】レイアウトに対してフォーマットを直接登録します [1.1, 1.2]
    block.layout()->setFormats(formats);

    // 4. 【重要】この行（ブロック）の再描画が必要であることをQtの描画エンジンに通知します [1.1, 1.2]
    if (block.document()) {
        const_cast<QTextDocument*>(block.document())->markContentsDirty(block.position(), block.length());
    }
}
#endif
std::vector<WordToken> tokenize(const QString &text) {
    std::vector<WordToken> tokens;
    int i = 0;
    int len = text.length();

    while (i < len) {
        if (text[i].isSpace()) {
            // 空白セグメント（スペースやタブの連続）
            int start = i;
            while (i < len && text[i].isSpace()) {
                i++;
            }
            tokens.push_back({text.mid(start, i - start), start});
        } 
        else if (text[i].isLetterOrNumber()) {
        	auto t = getCharType(text[i]);
            // 英数字・日本語ワードセグメント
            int start = i;
            while (i < len && text[i].isLetterOrNumber() && getCharType(text[i]) == t) {
                i++;
            }
            tokens.push_back({text.mid(start, i - start), start});
        } 
        else {
            // 記号（カンマ、ピリオド、ブラケットなど）は1文字ずつトークン化
            tokens.push_back({text.mid(i, 1), i});
            i++;
        }
    }
    return tokens;
}
void calculateAndSetWordDiff(QTextBlock block1, QTextBlock block2, const QString& text1, const QString& text2) {
	std::vector<WordToken> tokens1 = tokenize(text1);
    std::vector<WordToken> tokens2 = tokenize(text2);
    // 2. dtl::Diff<単語の型, ベクターの型> で単語単位の比較を実行！
    dtl::Diff<WordToken, std::vector<WordToken>> d(tokens1, tokens2);
    d.compose();
    auto ses = d.getSes().getSequence();
    DiffBlockUserData *userData1 = nullptr;
    DiffBlockUserData *userData2 = nullptr;
    int offset1 = 0, offset2 = 0;
    for (const auto &item : ses) {
    	const WordToken &token = item.first;
        dtl::elemInfo info = item.second;
        switch( info.type ) {
    	case dtl::SES_COMMON:
    		break;
    	case dtl::SES_DELETE:
    		if( token.start > block1.text().size() + offset1 ) {
		        offset1 += block1.text().size() + 1;
		        block1.setUserData(userData1);
		        block1 = block1.next();
                userData1 = new DiffBlockUserData();
    		} else if (userData1 == nullptr) {
                userData1 = new DiffBlockUserData();
            }
            userData1->ranges.append({ token.start - offset1, (int)token.text.size() });
    		break;
    	case dtl::SES_ADD:
    		if( token.start > block2.text().size() + offset2 ) {
		        offset2 += block2.text().size() + 1;
		        block2.setUserData(userData2);
		        block2 = block2.next();
                userData2 = new DiffBlockUserData();
    		} else if (userData2 == nullptr) {
                userData2 = new DiffBlockUserData();
            }
            userData2->ranges.append({token.start - offset2, (int)token.text.size()});
    		break;
        }
    }
    if (block1.isValid()) {
        block1.setUserData(userData1);
    }
    if (block2.isValid()) {
        block2.setUserData(userData2);
    }
}
//void calculateAndSetCharDiff(QTextBlock block1, QTextBlock block2, const std::vector<QChar>& text1, const std::vector<QChar>& text2) {
void calculateAndSetCharDiff(QTextBlock block1, QTextBlock block2, const QString& text1, const QString& text2) {
    //QString t1(text1.data(), text1.size()), t2(text2.data(), text2.size());
	//##qDebug() << "calculateAndSetCharDiff(" << text1 << ", " << text2 << ")";
	std::vector<QChar> t1(text1.data(), text1.data() + text1.size());
	std::vector<QChar> t2(text2.data(), text2.data() + text2.size());
	auto b1 = block1, b2 = block2;
	dtl::Diff<QChar, std::vector<QChar>> d(t1, t2);
    d.compose();
    auto ses = d.getSes().getSequence();
    DiffBlockUserData *userData1 = nullptr;
    DiffBlockUserData *userData2 = nullptr;
    int ix1 = 0, ix2 = 0;
    int deleteStart = -1;
    int deleteLen = 0;
    int addStart = -1;
    int addLen = 0;
	auto flushDeleteRange = [&]() {
        if (deleteStart != -1) {
            if (!userData1) userData1 = new DiffBlockUserData();
            userData1->ranges.append({deleteStart, deleteLen});
            //##qDebug() << "flushDeleteRange " << deleteStart << ", " << deleteLen;
            deleteStart = -1;
            deleteLen = 0;
        }
    };

    // 溜まっている追加（右側）のハイライト範囲を確定してuserDataに保存するラムダ
    auto flushAddRange = [&]() {
        if (addStart != -1) {
            if (!userData2) userData2 = new DiffBlockUserData();
            userData2->ranges.append({addStart, addLen});
            //##qDebug() << "flushAddRange " << addStart << ", " << addLen;
            addStart = -1;
            addLen = 0;
        }
    };
    for (const auto &item : ses) {
        dtl::elemInfo info = item.second;
        switch( info.type ) {
    	case dtl::SES_COMMON:
			flushDeleteRange();
            flushAddRange();
            if( ++ix1 > block1.text().size() ) {
				block1.setUserData(userData1);
				userData1 = nullptr;
    			ix1 = 0;
    			block1 = block1.next();
    		}
    		if( ++ix2 > block2.text().size() ) {
				block2.setUserData(userData2);
				userData2 = nullptr;
    			ix2 = 0;
    			block2 = block2.next();
    		}
        	break;
    	case dtl::SES_DELETE:	//	左側（doc1）のみに存在する行
        	flushAddRange(); // 右側の追加区間が終了したので確定
            // 削除された文字の範囲を記録
            if (deleteStart == -1) {
                deleteStart = ix1;
            }
            deleteLen++;
            ix1++; // 【重要】左側（doc1）に存在する文字なのでインデックスを進める
        	break;
    	case dtl::SES_ADD:		//	右側（doc2）で新しく追加された行
        	flushDeleteRange(); // 左側の削除区間が終了したので確定
            // 追加された文字の範囲を記録
            if (addStart == -1) {
                addStart = ix2;
            }
            addLen++;
            ix2++; // 【重要】右側（doc2）に存在する文字なのでインデックスを進める
        	break;
        }
    }
	flushDeleteRange();
    flushAddRange();
    if (block1.isValid()) {
        block1.setUserData(userData1);
    }
    if (block2.isValid()) {
        block2.setUserData(userData2);
    }
}
//
#if 0
std::vector<int>	g_vc1, g_vc2;
void buildVcTable(QTextDocument *doc, std::vector<int>& vc) {
	vc.clear();
	QTextBlock block = doc->begin();
	while( block.isValid() ) {
		vc.push_back(visualLineCount(block));
		block = block.next();
	}
}
#endif
void MainWindow::insertDummyLines(MarkdownEditor* editor, QTextCursor &cur, QTextBlock &block, int count) {
	if (count <= 0) return;
#if 1
    QTextDocument* doc = cur.document();
	int insertPos;
    bool atEnd = !block.isValid();

    if (atEnd) {
        // block が無効（ドキュメント末尾）の場合は、文書の終端位置に挿入
        insertPos = doc->characterCount() - 1;
        cur.setPosition(insertPos);
        // 末尾に改行を入れてダミー行を作成
        for (int i = 0; i < count; ++i) {
            cur.insertText("\n");
        }
    } else {
        // 通常は block の手前に挿入
        insertPos = block.position();
        cur.setPosition(insertPos);
        for (int i = 0; i < count; ++i) {
            cur.insertText("\n");
        }
    }

    // 挿入された空ブロック群をダミー行に設定
    QTextBlock dummy = doc->findBlock(insertPos);
    if (!atEnd) {
        for (int i = 0; i < count && dummy.isValid(); ++i) {
            editor->incNDummyLines();
            editor->diffBlockNumbers().push_back(DUMMY_LINE);
            editor->diffFlags().push_back(0);
            setDummyLine(dummy);
            dummy = dummy.next();
        }
        block = dummy; // 押し出された元のテキストブロックを指すように更新
    } else {
        // 末尾に追加した場合は、新しく末尾にできたダミー行群を設定
        dummy = dummy.next(); // 挿入した改行の次のブロックからがダミー
        for (int i = 0; i < count && dummy.isValid(); ++i) {
            editor->incNDummyLines();
            editor->diffBlockNumbers().push_back(DUMMY_LINE);
            editor->diffFlags().push_back(0);
            setDummyLine(dummy);
            dummy = dummy.next();
        }
        block = doc->end(); // 末尾のまま
    }
#else
    int insertPos = block.position();
    cur.setPosition(insertPos);
    for (int i = 0; i < count; ++i) {
        cur.insertText("\n");
    }
    // 挿入された空ブロック群をダミーに設定
    QTextBlock dummy = cur.document()->findBlock(insertPos);
    for (int i = 0; i < count; ++i) {
        setDummyLine(dummy);
        dummy = dummy.next();
    }
    block = dummy; // 元のテキストブロックを指すように更新
#endif
}
// 左側のみ存在（右側で削除）
void MainWindow::applyDeleteHunk(DocWidget* docWidget,
    int diffLn, int endLn, int &ln1,
    QTextBlock &block1, QTextBlock &block2,
    QTextCursor &cur2, const std::vector<QString> &lines1) 
{
    for (int ln = diffLn; ln < endLn; ++ln) {
        if (ln - 1 < lines1.size())
            do_output(QString("- %1 0 '%2'\n").arg(ln).arg(lines1[ln - 1]));

        setPhysicalLine(docWidget->m_editor, block1, ++ln1, ADDED_LINE);
        int vc = qMax(1, visualLineCount(block1));
        block1 = block1.next();

        // 右側にダミー行を挿入
        insertDummyLines(docWidget->m_diffview, cur2, block2, vc);
    }
}

// 右側のみ存在（右側で追加）
void MainWindow::applyAddHunk(DocWidget* docWidget,
    int diffLn, int endLn, int &ln2,
    QTextBlock &block1, QTextBlock &block2,
    QTextCursor &cur1, const std::vector<QString> &lines2) 
{
    for (int ln = diffLn; ln < endLn; ++ln) {
        if (ln - 1 >= lines2.size()) break;
        do_output(QString("+ 0 %1 '%2'\n").arg(ln).arg(lines2[ln - 1]));

        setPhysicalLine(docWidget->m_diffview, block2, ++ln2, ADDED_LINE);
        int vc = qMax(1, visualLineCount(block2));
        block2 = block2.next();

        // 左側にダミー行を挿入
        insertDummyLines(docWidget->m_editor, cur1, block1, vc);
    }
}

// 変更行（両側で異なる）
void MainWindow::applyModifyHunk(DocWidget* docWidget,
    int diffLn1, int endLn1, int diffLn2, int endLn2,
    int nDelete, int nAdd, int &ln1, int &ln2,
    QTextBlock &block1, QTextBlock &block2,
    QTextCursor &cur1, QTextCursor &cur2) 
{
    // 単語差分用テキストの抽出
    QString text1, text2;
    auto b1 = block1, b2 = block2;
    for (int i = 0; i < nDelete && b1.isValid(); ++i, b1 = b1.next())
        text1 += b1.text() + "\n";
    for (int i = 0; i < nAdd && b2.isValid(); ++i, b2 = b2.next())
        text2 += b2.text() + "\n";

    calculateAndSetWordDiff(block1, block2, text1, text2);

    // 左側の行属性設定と表示行数カウント
    int totalVc1 = 0;
    for (int ln = diffLn1; ln < endLn1; ++ln) {
        setPhysicalLine(docWidget->m_editor, block1, ++ln1, CHANGED_LINE);
        totalVc1 += qMax(1, visualLineCount(block1));
        block1 = block1.next();
    }

    // 右側の行属性設定と表示行数カウント
    int totalVc2 = 0;
    for (int ln = diffLn2; ln < endLn2; ++ln) {
        setPhysicalLine(docWidget->m_diffview, block2, ++ln2, CHANGED_LINE);
        totalVc2 += qMax(1, visualLineCount(block2));
        block2 = block2.next();
    }
	qDebug() << "totalVc1 = " << totalVc1 << ", totalVc2 = " << totalVc2;
    // 高さの差をダミー行で埋める
    int d = totalVc1 - totalVc2;
    if (d > 0) {
        insertDummyLines(docWidget->m_diffview, cur2, block2, d);
    } else if (d < 0) {
        insertDummyLines(docWidget->m_editor, cur1, block1, -d);
    }
}
void MainWindow::applyDiffToDocuments(
    DocWidget *docWidget,
    const std::vector<QString> &lines1,
    const std::vector<QString> &lines2,
    const dtl::Ses<QString> &ses) 
{
	docWidget->m_editor->diffFlags().clear();
	docWidget->m_editor->diffBlockNumbers().clear();
	docWidget->m_editor->clearNDummyLines();
	docWidget->m_diffview->diffFlags().clear();
	docWidget->m_diffview->diffBlockNumbers().clear();
	docWidget->m_diffview->clearNDummyLines();
    QTextDocument *doc1 = docWidget->m_editor->document();
    QTextDocument *doc2 = docWidget->m_diffview->document();
    QTextBlock block1 = doc1->begin();
    QTextBlock block2 = doc2->begin();
    QTextCursor cur1 = docWidget->m_editor->textCursor();
    QTextCursor cur2 = docWidget->m_diffview->textCursor();

    cur1.beginEditBlock();
    cur2.beginEditBlock();

    int ln1 = 0, ln2 = 0;
    int diffLn1 = INT_MAX, diffLn2 = INT_MAX;
    int nDelete = 0, nAdd = 0;

    auto flush = [&](int endLn1, int endLn2) {
        if (nDelete == 0 && nAdd == 0) return;
        if (nAdd == 0) {
            applyDeleteHunk(docWidget, diffLn1, endLn1, ln1, block1, block2, cur2, lines1);
        } else if (nDelete == 0) {
            applyAddHunk(docWidget, diffLn2, endLn2, ln2, block1, block2, cur1, lines2);
        } else {
            applyModifyHunk(docWidget, diffLn1, endLn1, diffLn2, endLn2,
            					nDelete, nAdd, ln1, ln2, block1, block2, cur1, cur2);
        }
        nDelete = nAdd = 0;
        diffLn1 = diffLn2 = INT_MAX;
    };

    for (const auto &item : ses.getSequence()) {
        const QString &line = item.first;
        dtl::elemInfo info = item.second;
        switch (info.type) {
        case dtl::SES_COMMON:
            flush(info.beforeIdx, info.afterIdx);
            do_output(QString("= %1 %2 '%3'\n").arg(info.beforeIdx).arg(info.afterIdx).arg(line));
            setPhysicalLine(docWidget->m_editor, block1, ++ln1, 0);
            block1 = block1.next();
            setPhysicalLine(docWidget->m_diffview, block2, ++ln2, 0);
            block2 = block2.next();
            break;
        case dtl::SES_DELETE:
            diffLn1 = qMin(diffLn1, info.beforeIdx);
            nDelete++;
            break;
        case dtl::SES_ADD:
            diffLn2 = qMin(diffLn2, info.afterIdx);
            nAdd++;
            break;
        }
    }
    flush(doc1->blockCount() + 1, doc2->blockCount() + 1);

    cur1.endEditBlock();
    cur2.endEditBlock();
}
void MainWindow::do_diff() {
	if (m_processing != 0) return;
    DocWidget *docWidget = getCurDocWidget();
    if (!docWidget || !docWidget->m_diffMode) return;

    ++m_processing;
    //m_diffBlockNumbers.clear();
    // --- 1. 前処理 ---
    QTextDocument *doc1 = docWidget->m_editor->document();
    QTextDocument *doc2 = docWidget->m_diffview->document();
    //qreal width2 = docWidget->m_diffview->viewport()->width();
    //doc2->setTextWidth(width2);
    //doc2->adjustSize();
    //buildVcTable(doc1, g_vc1);
    //buildVcTable(doc2, g_vc2);
    bool modified1 = doc1->isModified();
    bool modified2 = doc2->isModified();

    if (docWidget->m_editor->dummyInserted())   docWidget->m_editor->removeAllDummyLines();
    if (docWidget->m_diffview->dummyInserted()) docWidget->m_diffview->removeAllDummyLines();

    std::vector<QString> lines1 = extractLinesFromDocument(doc1);
    std::vector<QString> lines2 = extractLinesFromDocument(doc2);

    // --- 2. diff 計算 ---
    dtl::Diff<QString, std::vector<QString>> d(lines1, lines2);
    d.compose();

    // --- 3. ドキュメント反映 ---
    applyDiffToDocuments(docWidget, lines1, lines2, d.getSes());

    // --- 4. 後処理・UI更新 ---
    docWidget->m_editor->setDummyInserted(true);
    docWidget->m_diffview->setDummyInserted(true);
    docWidget->m_minimap->updateMap(doc1, docWidget->m_editor, doc2, docWidget->m_diffview);
    docWidget->m_editor->rehighlight();
    docWidget->m_diffview->rehighlight();
    doc1->setModified(modified1);
    doc2->setModified(modified2);

    const auto vbn1 = docWidget->m_editor->diffBlockNumbers();
    const auto flg1 = docWidget->m_editor->diffFlags();
    const auto vbn2 = docWidget->m_diffview->diffBlockNumbers();
    const auto flg2 = docWidget->m_diffview->diffFlags();
    assert( vbn1.size() == flg1.size() );
    assert( vbn2.size() == flg2.size() );

    --m_processing;
}
