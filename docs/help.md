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
## Markdown Specifications
## viMarkdown Original Features
## FAQ
## Appendix
