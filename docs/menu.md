# Menu List

[README_ja](../../README.md) > Menu List

**Note on Document Rendering:**
> 
> The documentation in this repository includes extended syntax (CSV blocks) specific to viMarkdown.
> Since this is not supported by GitHub's Markdown rendering engine, it will be displayed as plain CSV text.
> To view the documents in their intended formatting (as rendered tables), please use viMarkdown.

## File menu

```CSV
" Category ", " Item ", " Description ", " Shortcut "
"File", "New Window", "Create new main window", "Ctrl + N"
"File", "New Tab", "Create new tab", "Ctrl + T"
"File", "Open", "Open Markdown document", "Ctrl + O"
"File", "Diary > Today's Diary", "Open today's diary file", ""
"File", "Reload", "Reload current file", ""
"File", "Save", "Save Markdown document", "Ctrl + S"
"File", "Save As", "Save Markdown document with a new name", "Ctrl + Shift + S"
"File", "Save All", "Save all open documents", ""
"File > Export", "PDF", "Export as PDF", ""
"File", "Close", "Close current document", "Ctrl + W"
"File > Recent", "Recent Markdown Path", "Open recent file", ""
"File > Favorites", "Favorite Markdown Path", "Open favorite file", ""
"File", "Print", "Print preview", "Ctrl + P"
"File", "Exit", "Close main window", "Alt + F4"
```

## Edit menu

```CSV
" Category ", " Item ", " Description ", " Shortcut "
"Edit", "Undo", "Undo the last edit", "Ctrl + Z"
"Edit", "Redo", "Redo the last edit", "Ctrl + Y"
"Edit", "Cut", "Cut selection to clipboard", "Ctrl + X"
"Edit", "Copy", "Copy selection to clipboard", "Ctrl + C"
"Edit", "Paste", "Paste from clipboard", "Ctrl + V"
"Edit", "SelectAll", "Select all text", "Ctrl + A"
"Edit > Block", "List", "Convert selected lines to a bullet list", ""
"Edit > Block", "NumList", "Convert selected lines to a numbered list", ""
"Edit > Block", "Checkbox", "Convert selected lines to a task list", ""
"Edit > Block", "Heading", "Format selected lines as heading", ""
"Edit > Inline", "Bold", "Make selection bold", ""
"Edit > Inline", "Italic", "Make selection italic", ""
"Edit > Inline", "Strikethrough", "Apply strikethrough to selection", ""
"Edit > Format", "AlignLeft", "Align left within table cell", ""
"Edit > Format", "AlignCenter", "Align center within table cell", ""
"Edit > Format", "AlignRight", "Align right within table cell", ""
"Edit", "Indent", "Indent line", "Tab"
"Edit", "Unindent", "Unindent line", "Shift + Tab"
"Edit > Insert", "&1 yyyy-MM-dd", "Insert today's date", ""
"Edit > Insert", "&2 MM-dd", "Insert today's date", ""
"Edit > Insert", "&3 dd-MMM-yyyy", "Insert today's date", ""
"Edit > Convert", "CSV -> Markdown Table", "Convert CSV to Markdown table", ""
"Edit > Convert", "Markdown Table -> CSV", "Convert Markdown table to CSV", ""
```

## Search menu

```CSV
" Category ", " Item ", " Description ", " Shortcut "
"Search", "Find", "Move focus to search box", "Ctrl + F"
"Search", "ForwardAgain", "Search next", "F3"
"Search", "BackwardAgain", "Search previous", "Shift + F3"
"Search", "FindWord", "Search for word at cursor", "Ctrl + F3"
"Search", "ClearHighlight", "Clear search match highlights", "Alt + F3"
"Search", "Replace...", "Show search dialog", "F4"
"Search", "grep...", "Find in files", "Shift + F4"
"Search", "IgnoreCase", "Case-insensitive search", ""
"Search", "Regexp", "Regular expression search", ""
```

## View menu

```CSV
" Category ", " Item ", " Description ", " Shortcut "
"View", "OutlineBar", "Toggle Outline Bar", ""
"View", "FocusOutline", "Move focus to Outline Bar", "Ctrl + Q"
"View", "OutputBar", "Toggle Output Bar", ""
"View", "ClearOutput", "Clear output contents", ""
"View", "CalendarBar", "Toggle Calendar Bar", ""
```

## Navigation menu

```CSV
" Category ", " Item ", " Description ", " Shortcut "
"Navigation", "NextTab", "Move focus to next tab", "Ctrl + Tab"
"Navigation", "PrevTab", "Move focus to previous tab", "Ctrl + Shift + Tab"
"Navigation", "ToggleFocus", "Toggle focus between editor and preview", "Ctrl + \\"
"Navigation", "SwitchToAltFile", "Switch to alternate file", "Ctrl + ^"
"Navigation", "Back", "Navigate back in history", "Alt + ←"
"Navigation", "Forward", "Navigate forward in history", "Alt + →"
"Navigation", "TagJump", "Open link under cursor", "F9"
```

## Tool menu

```CSV
" Category ", " Item ", " Description ", " Shortcut "
"Tools", "Diff", "Compare documents", "Ctrl + Alt + D"
"Tools", "DiffWithFile", "Compare with file", ""
"Tools", "KeisenMode", "Toggle box-drawing line mode ON/OFF", "Shift + F5"
"Tools", "ThinKeisen", "Thin box-drawing line mode", ""
"Tools", "ThickKeisen", "Thick box-drawing line mode", ""
"Tools", "OpenPrev", "Insert blank line above with connected ruling lines", "Shift + F7"
<!--"Tools", "OpenNext", "Insert blank line below with connected ruling lines", "F7"-->
```

## Other menu

```CSV
" Category ", " Item ", " Description ", " Shortcut "
"Other", "Language...", "Show language settings dialog", ""
"Other", "Settings...", "Show [Settings Dialog](dialogs.md#設定ダイアログ)", "F8"
"Other", "Color Settings...", "Show color settings dialog", ""
"Other", "vi Keybindings", "Enable/Disable vi commands", ""
"Other", "Help", "Show help document", "F1"
"Other > Cheat Sheet", "Markdown", "Show Markdown cheat sheet", ""
"Other > Cheat Sheet", "vi", "Show vi cheat sheet", ""
"Other > Test", "etc", "Various debug tests", ""
"Other", "About viMarkdown...", "Show About viMarkdown dialog", ""
```

