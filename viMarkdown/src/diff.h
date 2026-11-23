//----------------------------------------------------------------------
//
//			File:			"diff.h"
//			Created:		11-7-2026
//			Author:			vivisuke
//			Description:
//
//----------------------------------------------------------------------

#pragma once

#if 0
test
#endif

struct WordToken {
    QString text;
    int start;
    //int length;

    // dtl が単語同士を比較するための演算子定義
    bool operator==(const WordToken &other) const {
        return text == other.text;
    }
};

struct DiffRange {
    int start;
    int length;
    int peerBlockNum = -1;	// 相手側ブロック番号（calculateAndSetWordDiff で設定、v0.3 #1457）
    int peerStart = -1;		// 相手側ブロック内開始位置
    int peerLength = 0;		// 相手側置換長（純挿入時は 0）
public:
    DiffRange(int s, int l, int pbn = -1, int ps = -1, int pl = 0)
    	: start(s), length(l), peerBlockNum(pbn), peerStart(ps), peerLength(pl)
    {}
};

class DiffBlockUserData : public QTextBlockUserData {
public:
    QList<DiffRange> ranges;
    QList<int> gaps;	// 相手側にだけある単語の欠落位置（ブロック内文字位置。v0.3 #1363）
};

