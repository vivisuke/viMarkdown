# Menu List

[README_ja](../../README.md) > Menu List

**Note on Document Rendering:**
> 
> The documentation in this repository includes extended syntax (CSV blocks) specific to viMarkdown.
> Since this is not supported by GitHub's Markdown rendering engine, it will be displayed as plain CSV text.
> To view the documents in their intended formatting (as rendered tables), please use viMarkdown.

## File メニュー

```CSV
" カテゴリ ", " 項目 ", " 説明 ", " ショートカット "
"File ", "NewWin", "新規メインウィンドウ作成", "Ctrl + N"
"File ", "NewTab", "新規タブ作成", "Ctrl + T"
"File ", "Open", "MD文書読込", "Ctrl + O"
"ファイル", "日記 > 今日の日記", "今日の日記ファイルを開く", 
"ファイル", "再ロード", "現ファイルを再読込", 
"File ", "Save", "MD文書保存", "Ctrl + S"
"File ", "SaveAs", "MD文書別名保存", "Ctrl + Shift + S"
"ファイル", "全文書保存", "開いているすべての文書を保存", 
"File > Export", "PDF", "PDF出力", ""
"ファイル", "閉じる", "現文書を閉じる", 
"File > Recent", "最近保存MDパス名", "最近のファイル読込", ""
"File > Favoraite", "お気に入りMDパス名", "お気に入りのファイル読込", ""
"File ", "Print", "プレビュー印刷", "Ctrl + P"
"File ", "Exit", "メインウィンドウを閉じる", "Alt + F4"
```

## Edit メニュー

```CSV
" カテゴリ ", " 項目 ", " 説明 ", " ショートカット "
"Edit", "Undo", "編集を元に戻す", "Ctrl + Z"
"Edit", "Redo", "編集を再実行", "Ctrl + Y"
"Edit", "Cut", "選択範囲を削除・クリップボードにコピー", "Ctrl + X"
"Edit", "Copy", "選択範囲をクリップボードにコピー", "Ctrl + C"
"Edit", "Paste", "クリップボードからペースト", "Ctrl + V"
"Edit", "SelectAll", "文書全選択", "Ctrl + A"
"Edit > Block", "List", "選択行をリストに", ""
"Edit > Block", "NumList", "選択行を連番リストに", ""
"Edit > Block", "Checkbox", "選択行をチェックボックスに", ""
"Edit > Block", "Heading", "選択行を見出し用に", ""
"Edit > Inline", "Bold", "選択範囲をボールドに", ""
"Edit > Inline", "Italic", "選択範囲をイタリックに", ""
"Edit > Inline", "Strikethrough", "選択範囲を打ち消し線に", ""
"Edit > Format", "AlignLeft", "罫線枠内左寄せ", ""
"Edit > Format", "AlignCenter", "罫線枠内中央揃え", ""
"Edit > Format", "AlignRight", "罫線枠内右寄せ", ""
"Edit", "Indent", "インデント", "Tab"
"Edit", "Unindent", "逆インデント", "Shift + Tab"
"Edit > Insert", "&1 yyyy-MM-dd", "今日の日付", ""
"Edit > Insert", "&2 MM-dd", "今日の日付", ""
"Edit > Insert", "&3 dd-MMM-yyyy", "今日の日付", ""
"Edit > Conver", "CSV -> Markdown Table", "CSV → Markdown表変換", ""
"Edit > Conver", "Markdown Table -> CSV", "Markdown表 → CSV変換", ""
```

## Search メニュー

```CSV
" カテゴリ ", " 項目 ", " 説明 ", " ショートカット "
"Search", "Find", "検索ボックスにフォーカス移動", "Ctrl + F"
"Search", "DorwardAgain", "次検索", "F3"
"Search", "BackwardAgain", "前検索", "Shift + F3"
"Search", "FindWord", "カーソル位置単語検索", "Ctrl + F3"
"Search", "ClearSearchHighlight", "検索マッチ強調消去", "Alt + F3"
"Search", "Replace...", "検索ダイアログ表示", "F4"
```

## View メニュー

```CSV
" カテゴリ ", " 項目 ", " 説明 ", " ショートカット "
"View", "OutlineBar", "アウトラインバー表示・非表示", ""
"View", "FocusOutline", "アウトラインバーにフォーカスを移す", "Ctrl + Q"
"View", "NextTab", "次のタブにフォーカスを移す", "Ctrl + Tab"
"View", "PrevTab", "前のタブにフォーカスを移す", "Ctrl + Shift + Tab"
"View", "ToggleFocus", "エディタ←→プレビューフォーカス移動", "Ctrl + \\"
"View", "SwitchToAltFile", "直前ファイル切り替え", "Ctrl + ^"
"View", "Back", "前履歴ファイル移動", "Alt + ←"
"View", "Forward", "次履歴ファイル移動", "Alt + →"
"View", "TagJump", "カーソルがリンク上にあればリンク先を開く", "F9"
```

## Tool メニュー

```CSV
" カテゴリ ", " 項目 ", " 説明 ", " ショートカット "
"Tool", "KeisenMode", "罫線モード ON/OFF", "Shift + F5"
"Tool", "ThinKeisen", "細罫線モード", ""
"Tool", "ThickKeisen", "太罫線モード", ""
"Tool", "OpenPrev", "カーソル行前に連結罫線空行挿入", "Shift + F7"
"Tool", "OpenNext", "カーソル行次に連結罫線空行挿入", "F7"
```

## Other メニュー

```CSV
" カテゴリ ", " 項目 ", " 説明 ", " ショートカット "
"Other", "Settings...", "[設定ダイアログ](dialogs.md#設定ダイアログ)表示", "F8"
"Other", "Help", "ヘルプ文書表示", "F1"
"Other", "About viMarkdown...", "About viMarkdown ダイアログ表示", ""
```

