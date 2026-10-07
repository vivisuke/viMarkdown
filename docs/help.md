# viMarkdown Help
[日本語 (Japanese)](./ja/help.md)
## Table of Contents
- [Introduction](#Introduction)
- [Basic Operations](#Basic-Operations)
- [Markdown Specifications](#Markdown-Specifications)
  - Block Elements
    - Titles & Headings
    - Lists
    - Checkboxes
    - Ordered Lists
    - Code Blocks
    - Blockquotes
  - Inline Elements
    - Bold
    - Italic
    - Strikethrough
    - Links
    - Images
- [viMarkdown Original Features](#viMarkdown-Original-Features)
  - Box Drawing (Keisen) Blocks
  - CSV Blocks
  - In-Preview Editing
- [FAQ](#FAQ)
- [Appendix](#Appendix) (External Links)
  - Menu List
  - Shortcut Key List

## Introduction
viMarkdown is a Markdown editor with synchronized editor and preview panes, offering lightweight performance and a rich set of unique features.

It supports vi commands for efficient text editing, along with a diff feature for reviewing document changes and a powerful grep feature for string search. Beyond editing in the editor pane, you can also perform basic operations—such as inserting and deleting text, making selections, and cutting and pasting—directly in the preview pane.

In addition to standard GFM tables, viMarkdown supports CSV-formatted tables for smooth data exchange with other applications. It also includes drawing features such as box-drawing (Keisen) blocks and SVG blocks, making it easy to create class diagrams, UI mockups, and more.

This document explains the basic operations of viMarkdown, which offers these distinctive features.
## ■ New Features in ver0.3
### vi
Features a vi editing system (Normal, Insert, Visual, and Command-line modes). Supports cursor navigation with `hjkl`, various editing commands (`x`, `dd`, `yy`, `p`, `c`, `s`, `.`), search using `/` and `?`, and ex commands such as `:w`, `:q`, and `:e`. Additionally, a custom Undo/Redo management engine optimized specifically for vi operations has been implemented.
### Diff
Added Diff mode, allowing side-by-side comparison of differences between two documents, or a document and an external file. Additions, deletions, and modifications are highlighted with background colors at both the line and word levels, and differences can be applied to either document using the `≪` and `≫` buttons. It also features a "MiniMap" to visually monitor overall differences and your current scroll position.
### Calendar & Diary
Integrated with the sidebar calendar, simply clicking any date automatically generates and opens diary files or notes (`diary/YYYY/MM/YYYYMMDD.md`) for present, past, or future dates.
The background of each date on the calendar is automatically color-coded based on the completion status of ToDo items in the diary (Incomplete: Light Red / All Completed: Light Green / No ToDo: Light Blue), allowing you to check task progress at a glance.
### Heading Folding
Allows you to fold and unfold text content according to the Markdown heading structure (`#` to `###`).
In addition to clicking the icons (▼ / ▶) next to line numbers, it supports vi commands (`zc`, `zo`, `za`, `zM`, `zR`), significantly improving readability for long documents.
### SVG
SVG data written inside ```SVG ... ``` code blocks is rendered in real time using the LunaSVG engine.
In addition, an auto-completion dialog triggered by `Ctrl + Space` allows you to easily insert templates for `<svg>` tags and various shape elements (`rect`, `ellipse`, `text`, `path`, etc.).
### Grep & Search Enhancements
Equipped with a Grep search dialog for searching across files within a specified directory.
It supports regular expression search and case-insensitive search (IgnoreCase) toggling, as well as quick searching from the toolbar and search result highlighting.
### OutputBar
Added the "OutputBar" at the bottom of the screen to display search results and logs.
It can be used to jump to corresponding lines by double-clicking Grep search results, view SVG syntax error notifications, check vi automated test results, reference cheat sheets, and more.
### Localization & Resource Support
Supports automatic switching between Japanese and English UIs based on OS language settings (Locale).
The display language can be changed at any time from the Language Dialog (takes effect after restarting), allowing all menus and dialogs to be fully localized.

## Basic Operations
### UI Overview
![UI Overview](screen_v03.png)

- (1) Title Bar
  Application name, Minimize / Maximize / Close buttons
- (2) Menu Bar & Toolbar
  Execute various actions via mouse clicks, access keys, or keyboard shortcuts.
- (3) Outline Bar
  - List of currently open documents
  - Heading tree of the document; click to jump directly to the target heading.
- (4) Document Tabs
  Switch between multiple documents using tabs.
  - Editor (Left Pane)
    Edit your Markdown source text.
  - Preview (Right Pane)
    Displays the formatted Markdown layout.
    - Checkboxes: Toggle state with a single mouse click.
    - Direct editing (inserting/deleting text) is also supported.
- (5) Output Bar
  Displays status messages, Grep search results, SVG / custom block outputs, and input guides (such as keybindings for Ruled Line mode).
- (6) Calendar Bar
  Displays a monthly calendar.  
  Click a date to create or open that day's daily note, and visually track ToDo task progress.
- (7) Status Bar
  Displays notifications/messages, cursor position, and character encoding.
## Markdown Syntax Specifications
### Block Elements
Defines structural elements on a line-by-line basis, such as headings and lists.

#### Headings
Creates document titles and section headings.  
Place a hash symbol `#` followed by a space at the beginning of a line.  
The number of `#` symbols (1 to 6) determines the heading level and text size.

Example:
```markdown
# Document Title
## Heading 1
### Heading 2
#### Heading 3
```

#### Unordered Lists (Bullet Lists)
Creates a bulleted list.  
Start the line with a hyphen `-`, plus `+`, or asterisk `*`, followed by a space.

Example:
```markdown
- Apple
- Orange
```
Result:
- Apple
- Orange

#### Ordered Lists (Numbered Lists)
Creates a sequential list. Start the line with a number and a period (`1.`), followed by a space.  
*(Note: Even if you write `1.` for all items, they will automatically be numbered sequentially when displayed.)*

Example:
```markdown
1. First step
1. Next step
```
Result:
1. First step
2. Next step

#### Task Lists (Checkboxes)
Creates a ToDo list by placing `[ ]` (space inside) or `[x]` immediately after the list marker.

Example:
```markdown
- [ ] Incomplete task
- [x] Completed task
```
Result:
- [ ] Incomplete task
- [x] Completed task

#### Blockquotes
Indicates that the text is quoted from another source. Place a greater-than symbol `>` followed by a space at the beginning of the line.

Example:
```markdown
> To be, or not to be,
> that is the question.
```
Result:
> To be, or not to be,
> that is the question.

#### Code Blocks
Used to display source code or preformatted text.  
Enclose the target text between lines consisting only of three backticks ( ` ``` ` ). Specifying a language name (e.g., `cpp` or `python`) right after the opening backticks enables syntax highlighting.

Example:
````markdown
```cpp
int main() {
    return 0;
}
```
````

---

### Inline Elements
Formats specific text within a sentence, or inserts links and images.  
These can be included inside block elements.

**💡 Helpful Tip**  
In addition to typing symbols manually, you can easily apply formatting by **selecting text and clicking toolbar icons or navigating to the menu (`Edit` > `Inline`)**.

#### Bold
Surround the text with two asterisks (`**`) or two underscores (`__`). Used to emphasize text.
- Menu: `Edit` > `Inline` > `Bold`
- Example: `This is **bold** text.`
- Result: This is **bold** text.

#### Italic
Surround the text with a single asterisk (`*`) or underscore (`_`). Displays text in italics.
- Menu: `Edit` > `Inline` > `Italic`
- Example: `This is *italic* text.`
- Result: This is *italic* text.

#### Strikethrough
Surround the text with two tildes (`~~`). Draws a horizontal line through the center of the text.
- Menu: `Edit` > `Inline` > `Strikethrough`
- Example: `This is ~~strikethrough~~ text.`
- Result: This is ~~strikethrough~~ text.

#### Inline Code
Used to display code snippets or specific symbols literally. Surround the text with backticks ( `` ` `` ).  
Markdown formatting characters (like `**` or `~~`) inside inline code will not be styled and will appear as raw text.
- Example: `` `**raw bold syntax**` ``
- Result: `**raw bold syntax**`

#### Links
Creates a hyperlink using the syntax `[Link Text](URL)`.
- Example: `For details, visit [Google](https://google.com).`
- Result: For details, visit [Google](https://google.com).

#### Images
Embeds an image into the document by adding an exclamation mark `!` before the link syntax: `![Alt Text](Path or URL to image)`.
- Example: `![Logo](https://example.com/logo.png)`
## viMarkdown Original Features
## ■ Unique Features of viMarkdown

### Calendar and Diary
The CalendarBar lets you quickly create, view, and edit daily records, reports, and notes from a calendar.
- **Create and Open Notes with One Click**
  - Click a target date on the CalendarBar to open that date's note (`YYYYMMDD`) immediately.
  - If the file does not exist, the system automatically creates it from a template that contains date headings and sections (`Todo`, `Notes`, etc.).
- **Color Highlights for Entry Status**
  - Dates with existing notes show a color highlight on the calendar. You can check your progress at a glance.
- **Flexible Docking Window**
  - You can dock the CalendarBar at the bottom, left, or right of the screen, or use it as a floating window.
  - Show or hide the CalendarBar at any time from the sidebar icon or the menu.
- **Seamless Link with the Outline Bar**
  - Headings in the open daily note (`Todo`, project items, etc.) show in the left Outline Bar immediately. You can jump to your target tasks quickly.
- **Week Numbers and Easy Month Navigation**
  - The calendar shows week numbers on the left edge.
  - Click the arrow buttons (◀ / ▶) to switch between past and future months to review or plan your notes.

### SVG Block
An SVG block lets you write Scalable Vector Graphics (SVG) code directly in Markdown and render shapes or illustrations in the preview.  
Write SVG tags between \```svg and \``` to show inline diagrams without external image files.

> <svg width="120" height="60" viewBox="0 0 120 60">
>   <rect x="5" y="5" width="110" height="50" rx="10" fill="#e1f5fe" stroke="#0288d1" stroke-width="2"/>
>   <text x="60" y="35" font-size="14" text-anchor="middle" fill="#01579b">SVG Block</text>
> </svg>

　　↓


```SVG
<svg width="120" height="60" viewBox="0 0 120 60">
  <rect x="5" y="5" width="110" height="50" rx="10" fill="#e1f5fe" stroke="#0288d1" stroke-width="2"/>
  <text x="60" y="35" font-size="14" text-anchor="middle" fill="#01579b">SVG Block</text>
</svg>
```
### CSV Block
A CSV block lets you easily write spreadsheet-like tables in Markdown.  
Write data separated by commas between \```CSV and \```. The preview automatically converts it into a clean table.

> ```CSV
> Name,Age,Department
> Taro Tanaka,30,Sales
> Hanako Sato,25,Development
> ```

　　↓

```CSV
Name,Age,Department
Taro Tanaka,30,Sales
Hanako Sato,25,Development
```
### Live Editing in Preview
Live Editing lets you edit documents directly in the preview pane.  
Click text to set focus and edit it immediately without returning to the editor pane.

- **Basic Operations**
  - Click text in the preview to edit it directly.
  - You do not need to switch frequently between the editor and preview panes.
- **Editor Synchronization**
  - Edits in the preview update the Markdown source instantly.
  - The cursor position and editing state stay synchronized across both panes.
- **Checkbox Operations**
  - Click a checkbox (`[ ]`) in the preview to toggle it to `[x]`.
  - Manage your tasks using mouse clicks only.
- **Input Assistance**
  - Supports keyboard shortcuts (bold, italic, lists, headings, etc.).
  - Edit documents intuitively while you view the rendered result.
## FAQ
```CSV
, Questions & Answers
**Q.**, "How can I convert a CSV block into a standard Markdown table?"
**A.**, "Place the cursor inside the target CSV block and run **Edit > Convert > CSV -> Markdown Table** from the menu. Conversely, you can also convert standard tables into CSV blocks for management."
**Q.**, "In Box-drawing mode (Shift + F5), drawn lines automatically connect or change their shapes."
**A.**, "viMarkdown features an ""Auto-Join"" function that automatically determines connections in all four directions (up, down, left, right) and selects the optimal box-drawing character. To prevent unintended connections, place half-width spaces around the line or use **Ctrl + Shift + Arrow keys** to erase unwanted parts and adjust."
**Q.**, "Why doesn't the frame shift to the right when typing text inside a box-drawing frame?"
**A.**, "This is due to the ""Border Protection"" feature. Even when inserting or deleting characters inside a frame, it tries to maintain the position of the vertical border on the right. This allows you to edit text without breaking the layout of class diagrams or UI mockups."
**Q.**, "The area scrolled into view in the editor is not shown in the preview."
**A.**, "The editor and preview are **synchronized based on cursor position**. When you move the cursor (or type text) in the editor, the preview automatically scrolls to keep the corresponding heading or text within view. To switch focus between them, use **View > Toggle Focus (Ctrl + \)**."
**Q.**, "After navigating between multiple documents, I lost track of where I was editing."
**A.**, "Just like in a web browser, you can use **Alt + ← (Back) and Alt + → (Forward)**. This works not only for navigating between files, but also for returning after jumping to a link within the same document, fully restoring your previous cursor position."
**Q.**, "How do I clear the search highlights (yellow)?"
**A.**, "To clear search highlights, press **Alt + F3 (Clear Search Highlight)**."
**Q.**, "I heard vi-like operations are supported. Which commands are available?"
**A.**, "Basic vi keybindings (vi commands) are supported. By enabling ""Other > vi Keybindings"" from the menu, you can use various commands including basic movements (hjkl, w, b, gg, G, etc.), editing (i, a, o, d, c, y, p, r, etc.), search (/, ?, n), Ex commands (:w, :q, :s, etc.), and heading folding (za, zM, etc.). For a detailed list of supported commands, please refer to ""Other > Cheat Sheet > Vi"" in the menu."
**Q.**, "The app becomes sluggish when editing very long documents (tens of thousands of lines or more)."
**A.**, "Currently (v0.4 and earlier), rendering huge files may take time because **incremental parsing is not supported**. We recommend **splitting files into chunks of roughly a thousand lines** for optimal performance. You can easily navigate between split files by linking them using \[Title\](filename.md)."
```
## Appendix
