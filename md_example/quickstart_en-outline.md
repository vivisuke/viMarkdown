# viMarkdown Quick Start Guide

## Table of Contents
[Part 1: Learn the Basics of vi & Markdown in 5 Minutes]
1. Getting Started
  - Creating a New Document
  - Opening a File
  - Cursor Movement
2. Writing Markdown Comfortably
  - List Auto-Completion
  - Indent / Outdent
  - Undo / Redo
  - In-Preview Editing
3. Saving and Closing Files

[Part 2: Powerful Features to Master viMarkdown]
4. Diary & Note Management: Instant Logging from the Calendar
5. Data Editing: Seamless CSV Tables within Markdown
6. Visuals: Displaying and Editing SVG Graphics
7. Version Control Feel: Comparing Differences with diff
8. Command Cheat Sheets

---

# [Part 1: Learn the Basics of vi & Markdown in 5 Minutes]

## 1. Getting Started
### Creating a New Document
- **File > New Tab** (`Ctrl + T`): Opens a new document tab.
- You can also click the **"+"** button on the left side of the tab bar.

### Opening a File
- **File > Open** (`Ctrl + O`): Opens the file dialog to select and open a file.

### Cursor Movement
- **Mouse Click**: Moves the cursor to the clicked position.
- **Arrow Keys (↑ ↓ ← →)**: Moves the cursor up, down, left, and right.
- **Home / End**: Moves to the beginning / end of the line.
- **Ctrl + Home / Ctrl + End**: Moves to the very beginning / end of the document.

---

## 2. Writing Markdown Comfortably

### List Auto-Completion
```markdown
- Item text [Cursor]
```
- Pressing **Enter** in the state above automatically continues the list:
```markdown
- Item text
- [Cursor]
```
- Pressing **Enter** on an empty list item will remove the list marker.

*Note: Also works seamlessly with numbered lists (`1.`) and task lists (`- [ ]`).*

### Indent / Outdent
- **Tab**: Indents the line.
- **Shift + Tab**: Outdents (un-indents) the line.
- *Note: Indentation uses 2 half-width spaces.*

### Undo / Redo
- **Edit > Undo** (`Ctrl + Z`): Undoes the last editing action.
- **Edit > Redo** (`Ctrl + Y`): Redoes the undone action.

### In-Preview Editing
- Simple edits, such as typing and deleting text, can also be performed directly inside the preview pane.

---

## 3. Saving and Closing Files
- **File > Save** (`Ctrl + S`): Saves the active document.
- **File > Save As...** (`Ctrl + Shift + S`): Saves the document under a new name.
- **File > Close** (`Ctrl + W`): Closes the active document tab.
- **File > Exit**: Exits viMarkdown.

---

# [Part 2: Powerful Features to Master viMarkdown]

## 4. Diary & Note Management: Instant Logging from the Calendar
- **View > Calendar Bar**: Toggles the calendar side-bar on/off.
- **Click on a date**: Instantly creates or opens the ToDo / Note document for that day.
- Diary files are automatically stored under `<Default Directory>/diary/YYYY/MM/`.
- You can customize the document template by editing `diary/template.md`.

---

## 5. Data Editing: Seamless CSV Tables within Markdown
- Enclose CSV data inside ` ```csv ` and ` ``` ` code fences to render it as a clean, formatted table.
- Since it is standard CSV, you can easily copy and paste data to and from spreadsheet tools (like Excel).
- When the cursor is inside a CSV table, choose **Edit > Convert > CSV → GFM Table** to convert it into a standard Markdown table.

---

## 6. Visuals: Displaying and Editing SVG Graphics
- Enclose SVG code inside ` ```svg ` and ` ``` ` code fences to render vector graphics directly in the preview.
- Press **Ctrl + Space** inside an SVG block to bring up the SVG syntax completion dialog.
- **Tip**: You can ask a generative AI to create SVG graphics and immediately preview and fine-tune them inside viMarkdown.

---

## 7. Version Control Feel: Comparing Differences with diff
- **Tools > diff**: Compares the active document with a blank document.
- **Tools > diff with file...**: Compares the active document with another selected file.
- **In diff mode:**
  - Corresponding lines are aligned side-by-side.
  - **Tools > diff**: Exits diff mode.
  - Click the **≪** or **≫** buttons to apply changes from one document to the other.
  - Right-click an inserted word inside a diff block and select **"Apply to Other Document"** to merge individual words.

---

## 8. Command Cheat Sheets
Quickly look up keybindings and syntax whenever you need:
- **Help / Misc > Cheat Sheet > Markdown**
- **Help / Misc > Cheat Sheet > vi**
