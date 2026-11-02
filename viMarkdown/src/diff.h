//----------------------------------------------------------------------
//
//			File:			"diff.h"
//			Created:		11-7-2026
//			Author:			vivisuke
//			Description:
//
//----------------------------------------------------------------------

#pragma once

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
};

