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
#if 0
    int hogehoge;
#endif
public:
    DiffRange(int s, int l)
    	: start(s), length(l)
    {}
};

class DiffBlockUserData : public QTextBlockUserData {
public:
    QList<DiffRange> ranges;
    QList<int> gaps;	// 相手側にだけある単語の欠落位置（ブロック内文字位置。v0.3 #1363）
};

