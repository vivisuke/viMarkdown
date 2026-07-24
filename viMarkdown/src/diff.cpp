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
#include "dtl/dtl.hpp"
#include "MainWindow.h"
#include "ui_MainWindow.h"
#include "DocWidget.h"
#include "MarkdownEditor.h"
#include "MarkdownPreview.h"
#include "diff.h"

CharType getCharType(QChar ch);

// 1. ダミー行（高さを揃えるための空行）かどうかの判定
bool isDummyLine(const QTextBlock &block) {
    return block.userState() == 0;
}
#if 1
// 2. 行番号（1オリジン）取得（2ビット右シフトするだけ）
int lineNumber(const QTextBlock &block) {
    return (unsigned)block.userState() >> 2;
}

// 3. 差分の有無（下位2ビットが 0なら差分なし）
int getDiff(const QTextBlock &block) {
    return (block.userState() & 0x03);
}
bool hasDiff(const QTextBlock &block) {
	return getDiff(block) != 0;
}

// ダミー行をセットする場合
void setDummyLine(QTextBlock block) {
    block.setUserState(0);
}

// 物理的な行をセットする場合
void setPhysicalLine(QTextBlock &block, int ln, int flag) {
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

        if (isDummyLine(block)) {
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
void updateMapSub(QPainter &p, int x, QTextDocument* doc) {
	QTextBlock block = doc->begin();
	for(int y = 0; y < doc->blockCount() && block.isValid(); ++y, block=block.next()) {
		QColor col = Qt::white;		//QColor("#808080");
		if( isDummyLine(block) ) col = QColor("#e8e8e8");
		//else if( hasDiff(block) ) col = QColor("#ffa0a0");	//QColor("#ccffcc");	//QColor("#ffecec");
		else {
			auto d = getDiff(block);
			if( d == ADDED_LINE ) col = QColor("#ffa0a0");
			else if( d == CHANGED_LINE ) col = QColor("#ffffa0");
		}
		p.setPen(col);
		p.drawLine(x, y, x+MINMAP_WIDTH/2-1, y);
	}
}
void MiniMap::updateMap(QTextDocument* doc1, QTextDocument* doc2) {
	//m_mapPixmap = QPixmap(MINMAP_WIDTH, doc1->blockCount());
	auto ht = rect().height();
	m_mapPixmap = QPixmap(MINMAP_WIDTH, ht);
	QPainter p(&m_mapPixmap);
	int x = 0;
	updateMapSub(p, 0, doc1);
	updateMapSub(p, MINMAP_WIDTH/2, doc2);
	p.setBrush(QColor("#e8e8e8"));
	p.drawRect(0, doc1->blockCount(), MINMAP_WIDTH, ht - doc1->blockCount());
}
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
    lines.back() += QChar(0xffff);
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
		QMessageBox::warning(this, tr("エラー"), tr("ファイルが開けません:\n%1").arg(fullPath));
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
	DocWidget *docWidget = getCurDocWidget();
	if( docWidget == nullptr ) return;
	QString fullPath = QFileDialog::getOpenFileName(
		this,
		"select diff file",			// ダイアログのタイトル
		QDir::currentPath(),		// 初期ディレクトリ
		"markdown file (*.md *.markdown);;text file(*.txt);;all(*.*)"	// フィルター
	);
	if( fullPath.isEmpty() ) return;
	QFile file(fullPath);
	if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
		QMessageBox::warning(this, tr("エラー"), tr("ファイルが開けません:\n%1").arg(fullPath));
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
	ui->action_DiffMode->setChecked(true);
	docWidget->m_diffMode = true;
	docWidget->m_editor->setDiffMode(true);
	docWidget->m_editor->expandAll();
	docWidget->m_editor->setHighlightDiff(true);
	docWidget->m_diffview->setHighlightDiff(true);
	docWidget->m_editor->setHighlightMarkdown(false);
	docWidget->m_editor->setLineWrapMode(QPlainTextEdit::NoWrap);
	docWidget->m_editor->rehighlight();
	docWidget->updatePanes();
	do_diff();
}
void MainWindow::onAction_DiffMode(bool checked) {
	DocWidget *docWidget = getCurDocWidget();
	if( docWidget == nullptr ) return;
	docWidget->m_diffMode = checked;
	docWidget->m_editor->setDiffMode(checked);
	if (checked) {
		docWidget->m_editor->expandAll();
		docWidget->m_editor->setHighlightDiff(true);
		docWidget->m_diffview->setHighlightDiff(true);
		docWidget->m_editor->setHighlightMarkdown(false);
		docWidget->m_editor->setLineWrapMode(QPlainTextEdit::NoWrap);
		do_diff();
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
	} else {
		if( docWidget->m_docType == DocType::Markdown )
			docWidget->m_editor->setHighlightMarkdown(true);
		docWidget->m_editor->setHighlightDiff(false);
		docWidget->m_diffview->setHighlightDiff(false);
		docWidget->m_editor->setLineWrapMode(QPlainTextEdit::WidgetWidth);
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
	}
	docWidget->m_editor->rehighlight();
	docWidget->updatePanes();
}
void MainWindow::onDiffViewChanged() {
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
void MainWindow::do_diff() {
	if( m_processing!=0 ) return;
	DocWidget *docWidget = getCurDocWidget();
	if (docWidget == nullptr || !docWidget->m_diffMode)
		return;
	qDebug() << "MainWindow::do_diff()";
	++m_processing;
	int pos1 = docWidget->m_editor->textCursor().position();
    int pos2 = docWidget->m_diffview->textCursor().position();
	//##disconnect(docWidget->m_editor, &MarkdownEditor::textChanged, this, &MainWindow::onMDTextChanged);
	//##disconnect(docWidget->m_diffview, &MarkdownEditor::textChanged, this, &MainWindow::onMDTextChanged);
	QTextDocument *doc1 = docWidget->m_editor->document();
	QTextDocument *doc2 = docWidget->m_diffview->document();
	bool modified1 = doc1->isModified();
	bool modified2 = doc2->isModified();
    QTextCursor cur1_sv = docWidget->m_editor->textCursor();
    QTextCursor cur2_sv = docWidget->m_diffview->textCursor();
	if( docWidget->m_editor->dummyInserted() )
        docWidget->m_editor->removeAllDummyLines();
	if( docWidget->m_diffview->dummyInserted() )
        docWidget->m_diffview->removeAllDummyLines();
	int activeLine1 = docWidget->m_editor->textCursor().blockNumber();
    int activeCol1 = docWidget->m_editor->textCursor().positionInBlock();
    do_output(QString("activeLine1: %1, activeCol1: %2").arg(activeLine1).arg(activeCol1));
    int activeLine2 = docWidget->m_diffview->textCursor().blockNumber();
    int activeCol2 = docWidget->m_diffview->textCursor().positionInBlock();
	std::vector<QString> lines1 = extractLinesFromDocument(doc1);
    std::vector<QString> lines2 = extractLinesFromDocument(doc2);
#if 0
    if( lines1.back() == lines2.back() ) {
    	lines1.pop_back();
    	lines2.pop_back();
    	if( lines1.empty() && lines2.empty() ) {
    		QTextBlock block1 = doc1->begin();
    		setPhysicalLine(block1, 1, 0);
    		QTextBlock block2 = doc2->begin();
    		setPhysicalLine(block2, 1, 0);
    		--m_processing;
    		return;
    	}
    }
#endif
    dtl::Diff<QString, std::vector<QString>> d(lines1, lines2);
    d.compose();

    do_output("\n");
    QTextBlock block1 = doc1->begin();
    QTextBlock block2 = doc2->begin();
    QTextCursor cur1 = docWidget->m_editor->textCursor();
    QTextCursor cur2 = docWidget->m_diffview->textCursor();
    //QTextCursor cur1_sv = cur1;
    //QTextCursor cur2_sv = cur2;
    //qDebug() << "cur1_sv.position(): " << cur1_sv.position();
    cur1.beginEditBlock();
    cur2.beginEditBlock();
    int ln1 = 0, ln2 = 0;
    auto ses = d.getSes().getSequence();
    int diffLn1 = INT_MAX, diffLn2 = INT_MAX;
    int nDelete = 0, nAdd = 0;
    auto flushPending = [&](int endLn1, int endLn2) {
        if (nDelete == 0 && nAdd == 0) return;
        if (nAdd == 0) {        // 右側（doc2）で削除された場合のみ
            for (int ln = diffLn1; ln < endLn1; ++ln) {
                if( ln-1 < lines1.size() )
                    do_output(QString("- %1 0 '%2'\n").arg(ln).arg(lines1[ln-1]));
	        	setPhysicalLine(block1, ++ln1, ADDED_LINE);
	        	block1.setUserData(nullptr);		//	clear userData
	        	block1 = block1.next();
	        	cur2.setPosition(block2.position()); 
                cur2.insertText("\n");
#if 0
                block2 = cur2.block();
                assert( block2.isValid() );
                int bn = block2.blockNumber();
                QTextBlock dummyBlock = block2.previous();
                assert( dummyBlock.isValid() );
                setDummyLine(dummyBlock);
                dummyBlock.setUserData(nullptr);
#else
                setDummyLine(block2);   // 空になった現在のブロック（行）をダミーに設定
	        	block2.setUserData(nullptr);		//	clear userData
                block2 = block2.next();  // 下に押し出されたテキストが入っているブロックへ進む
#endif
            }
        } else if (nDelete == 0) { // 右側（doc2）で新しく追加された場合のみ
            for (int ln = diffLn2; ln < endLn2; ++ln) {
                if (ln - 1 >= lines2.size()) break;
                do_output(QString("+ 0 %1 '%2'\n").arg(ln).arg(lines2[ln-1]));
	        	setPhysicalLine(block2, ++ln2, ADDED_LINE);
	        	block2.setUserData(nullptr);		//	clear userData
	        	block2 = block2.next();
	        	cur1.setPosition(block1.position());
                cur1.insertText("\n");
#if 0
                block1 = cur1.block();
                QTextBlock dummyBlock = block1.previous();
                setDummyLine(dummyBlock);
                dummyBlock.setUserData(nullptr);
#else
                setDummyLine(block1);   // 空になった現在のブロック（行）をダミーに設定
	        	block1.setUserData(nullptr);		//	clear userData
                block1 = block1.next();  // 下に押し出されたテキストが入っているブロックへ進む
#endif
            }
        } else {	//	変更行
			//std::vector<QChar> text1, text2;
			QString text1, text2;
			auto b1 = block1, b2 = block2;
			for(int i = 0; i < nAdd; ++i, b1=b1.next()) {
				if( !b1.text().isEmpty() ) {
					//const QChar *ptr = b1.text().data();
					//text1.insert(text1.end(), ptr, ptr + b1.text().size());
					text1 += b1.text();
				}
				text1.push_back(QChar(u'\n'));
			}
			for(int i = 0; i < nDelete; ++i, b2=b2.next()) {
				if( !b2.text().isEmpty() ) {
					//const QChar *ptr = b2.text().data();
					//text2.insert(text2.end(), ptr, ptr + b2.text().size());
					text2 += b2.text();
				}
				text2.push_back(QChar(u'\n'));
			}
            //calculateAndSetCharDiff(block1, block2, text1, text2);
            calculateAndSetWordDiff(block1, block2, text1, text2);
            for (int ln = diffLn1; ln < endLn1; ++ln) {
                //##do_output(QString("! %1 0 '%2'\n").arg(ln).arg(lines1[ln-1]));
	        	setPhysicalLine(block1, ++ln1, CHANGED_LINE);
	        	//const auto *userData = dynamic_cast<const DiffBlockUserData*>(block1.userData());
                //applyInlineHighlight(block1, userData);
	        	block1 = block1.next();
            }
            for (int ln = diffLn2; ln < endLn2; ++ln) {
                //##do_output(QString("! 0 %1 '%2'\n").arg(ln).arg(lines2[ln-1]));
	        	setPhysicalLine(block2, ++ln2, CHANGED_LINE);
	        	//const auto *userData = dynamic_cast<const DiffBlockUserData*>(block2.userData());
                //applyInlineHighlight(block2, userData);
	        	block2 = block2.next();
            }
            int d = (endLn1 - diffLn1) - (endLn2 - diffLn2);
            if( d > 0 ) {
            	for(int i = 0; i < d; ++i) {
		        	cur2.setPosition(block2.position());
                    cur2.insertText("\n");
#if 0
	                block2 = cur2.block();
                    QTextBlock dummyBlock = block2.previous();
                    setDummyLine(dummyBlock);
#else
                    setDummyLine(block2);
                    block2 = block2.next();
#endif
            	}
            } else if( d < 0 ) {
            	for(int i = 0; i < -d; ++i) {
		        	cur1.setPosition(block1.position());
                    cur1.insertText("\n");
#if 0
	                block1 = cur1.block();
                    QTextBlock dummyBlock = block1.previous();
                    setDummyLine(dummyBlock);
#else
                    setDummyLine(block1);
                    block1 = block1.next();
#endif
            	}
            }
        }
        nDelete = nAdd = 0;
        diffLn1 = diffLn2 = INT_MAX;
    };
    for (const auto& item : ses) {
        const QString& line = item.first;
        dtl::elemInfo info = item.second;
        switch (info.type) {
        case dtl::SES_COMMON:
       		flushPending(info.beforeIdx, info.afterIdx);
        	do_output(QString("= %1 %2 '%3'\n").arg(info.beforeIdx).arg(info.afterIdx).arg(line));
        	setPhysicalLine(block1, ++ln1, 0);
        	block1.setUserData(nullptr);		//	clear userData
        	block1 = block1.next();
        	setPhysicalLine(block2, ++ln2, 0);
        	block2.setUserData(nullptr);		//	clear userData
        	block2 = block2.next();
        	break;
        case dtl::SES_DELETE:	//	左側（doc1）で削除された行
        	diffLn1 = qMin(diffLn1, info.beforeIdx);
        	nDelete += 1;
        	break;
        case dtl::SES_ADD:		//	右側（doc2）で新しく追加された行
        	diffLn2 = qMin(diffLn2, info.afterIdx);
        	nAdd += 1;
        	break;
        }
    }
    flushPending(doc1->blockCount() + 1, doc2->blockCount() + 1);
    //docWidget->m_editor->setTextCursor(cur1);
    //docWidget->m_diffview->setTextCursor(cur2);
    cur1.endEditBlock();
    cur2.endEditBlock();
    docWidget->m_editor->setDummyInserted(true);
    docWidget->m_diffview->setDummyInserted(true);
    //
	//if( docWidget->m_minimap->m_mapPixmap != nullptr )
	//	delete docWidget->m_minimap->m_mmPixmap;
	//##docWidget->m_mmPixmap = new QPixmap(MINMAP_WIDTH, doc1->blockCount());
	//##docWidget->m_minimap->addChild(docWidget->m_mmPixmap);
	//docWidget->m_minimap->resize(MINMAP_WIDTH, doc1->blockCount());
    docWidget->m_minimap->updateMap(doc1, doc2);
	docWidget->m_editor->rehighlight();
	docWidget->m_diffview->rehighlight();
	doc1->setModified(modified1);
	doc2->setModified(modified2);
    //
	//##connect(docWidget->m_editor, &MarkdownEditor::textChanged, this, &MainWindow::onMDTextChanged);
	//##connect(docWidget->m_diffview, &MarkdownEditor::textChanged, this, &MainWindow::onMDTextChanged);
    //qDebug() << "cur1_sv.position(): " << cur1_sv.position();
    //docWidget->m_editor->setTextCursor(cur1_sv);
    //docWidget->m_diffview->setTextCursor(cur2_sv);
#if 0
    auto restoreCursor = [](MarkdownEditor* editor, int targetRealLine, int targetCol) {
        QTextDocument* doc = editor->document();
        QTextBlock block = doc->begin();
        int realLineCount = 0;
        
        while (block.isValid()) {
            // ダミー行ではない「本物のテキスト行」だけをカウントする
            if (!isDummyLine(block)) {
                if (realLineCount == targetRealLine) {
                    break;
                }
                realLineCount++;
            }
            block = block.next();
        }
        
        if (block.isValid()) {
            QTextCursor cur(block);
            // 行内の元の列位置に移動（行の長さを超えないようにガード）
            int col = qMin(targetCol, block.text().length());
            cur.setPosition(block.position() + col);
            editor->setTextCursor(cur);
        }
    };

    restoreCursor(docWidget->m_editor, activeLine1, activeCol1);
    restoreCursor(docWidget->m_diffview, activeLine2, activeCol2);
#endif
#if 0
    cur1.setPosition(pos1);
	docWidget->m_editor->setTextCursor(cur1);
    cur2.setPosition(pos2);
	docWidget->m_editor->setTextCursor(cur2);
#endif
	//do_output(QString("pos1 = %1, pos2 = %2").arg(pos1).arg(pos2));
	--m_processing;
}
