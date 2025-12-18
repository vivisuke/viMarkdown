# viMarkdown spec

# viMarkdown マークダウン仕様

viMarkdown で実装済み・予定 Markdown 仕様

[CommonMark](https://commonmark.org/) をベースに
GitHub Flavored Markdown (GFM) の拡張をサポートする。

## 概要

### ブロック要素

- [x] 段落
- [x] 見出し
- [x] リスト
  - [x] 箇条書き
  - [x] 連番リスト
- [x] 水平線
- [ ] 引用
- [ ] コードブロック
- [ ] テーブル

### インライン要素

- [ ] 強調
  - [x] **ボールド**
  - [x] *イタリック*
  - [x] ~~打ち消し線~~
- [x] チェック
- [x] リンク
- [ ] 画像
- [ ] エスケープ

## 詳細

### ブロック要素

#### 引用

行頭に > を記述すると引用
例：
> 引用されたテキスト