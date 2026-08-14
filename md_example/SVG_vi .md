# vi 

```SVG
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 740 440" width="100%" height="100%" style="background-color: #ffffff; font-family: 'Segoe UI', 'Hiragino Sans', sans-serif;">
  <defs>
    <!-- ドロップシャドウ -->
    <filter id="shadow" x="-10%" y="-10%" width="120%" height="120%">
      <feDropShadow dx="0" dy="3" stdDeviation="5" flood-color="#000000" flood-opacity="0.1"/>
    </filter>

    <!-- 矢印マーカーの定義 -->
    <marker id="arrow-blue" viewBox="0 0 10 10" refX="6" refY="5" markerWidth="6" markerHeight="6" orient="auto-start-reverse">
      <path d="M 0 1 L 10 5 L 0 9 z" fill="#2563eb"/>
    </marker>
    <marker id="arrow-green" viewBox="0 0 10 10" refX="6" refY="5" markerWidth="6" markerHeight="6" orient="auto-start-reverse">
      <path d="M 0 1 L 10 5 L 0 9 z" fill="#10b981"/>
    </marker>
    <marker id="arrow-orange" viewBox="0 0 10 10" refX="6" refY="5" markerWidth="6" markerHeight="6" orient="auto-start-reverse">
      <path d="M 0 1 L 10 5 L 0 9 z" fill="#d97706"/>
    </marker>
    <marker id="arrow-dark" viewBox="0 0 10 10" refX="6" refY="5" markerWidth="6" markerHeight="6" orient="auto-start-reverse">
      <path d="M 0 1 L 10 5 L 0 9 z" fill="#4b5563"/>
    </marker>
  </defs>

  <!-- タイトル -->
  <text x="370" y="20" text-anchor="middle" font-size="18" font-weight="bold" fill="#1e293b">vi Modal Editing Architecture</text>

  <!-- ==================== 接続ライン ＆ キーラベル ==================== -->

  <!-- 1. NORMAL <-> INSERT (上下関係) -->
  <!-- NORMAL -> INSERT -->
  <path d="M 320 170 L 320 115" fill="none" stroke="#10b981" stroke-width="2.5" marker-end="url(#arrow-green)"/>
  <rect x="250" y="130" width="62" height="20" rx="4" fill="#ecfdf5" stroke="#a7f3d0" stroke-width="1"/>
  <text x="281" y="144" text-anchor="middle" font-size="11" font-weight="bold" fill="#047857">i, a, o, A</text>

  <!-- INSERT -> NORMAL -->
  <path d="M 420 115 L 420 170" fill="none" stroke="#2563eb" stroke-width="2.5" marker-end="url(#arrow-blue)"/>
  <rect x="428" y="130" width="45" height="20" rx="4" fill="#eff6ff" stroke="#bfdbfe" stroke-width="1"/>
  <text x="450" y="144" text-anchor="middle" font-size="11" font-weight="bold" fill="#1d4ed8">Esc</text>


  <!-- 2. NORMAL <-> VISUAL (斜め左下関係) -->
  <!-- NORMAL -> VISUAL -->
  <path d="M 270 270 L 200 320" fill="none" stroke="#d97706" stroke-width="2.5" marker-end="url(#arrow-orange)"/>
  <rect x="250" y="300" width="48" height="20" rx="4" fill="#fffbeb" stroke="#fde68a" stroke-width="1"/>
  <text x="274" y="314" text-anchor="middle" font-size="11" font-weight="bold" fill="#b45309">v, V</text>

  <!-- VISUAL -> NORMAL -->
  <path d="M 170 320 L 240 270" fill="none" stroke="#2563eb" stroke-width="2.5" marker-end="url(#arrow-blue)"/>
  <rect x="105" y="300" width="45" height="20" rx="4" fill="#eff6ff" stroke="#bfdbfe" stroke-width="1"/>
  <text x="127" y="314" text-anchor="middle" font-size="11" font-weight="bold" fill="#1d4ed8">Esc</text>


  <!-- 3. NORMAL <-> COMMAND-LINE (斜め右下関係) -->
  <!-- NORMAL -> COMMAND -->
  <path d="M 470 270 L 540 320" fill="none" stroke="#4b5563" stroke-width="2.5" marker-end="url(#arrow-dark)"/>
  <rect x="452" y="300" width="50" height="20" rx="4" fill="#f3f4f6" stroke="#d1d5db" stroke-width="1"/>
  <text x="477" y="314" text-anchor="middle" font-size="11" font-weight="bold" fill="#374151">:, /, ?</text>
/archi
  <!-- COMMAND -> NORMAL -->
  <path d="M 570 320 L 500 270" fill="none" stroke="#2563eb" stroke-width="2.5" marker-end="url(#arrow-blue)"/>
  <rect x="590" y="300" width="72" height="20" rx="4" fill="#eff6ff" stroke="#bfdbfe" stroke-width="1"/>
  <text x="626" y="314" text-anchor="middle" font-size="11" font-weight="bold" fill="#1d4ed8">Enter / Esc</text>


  <!-- ==================== モード ノード (カード) ==================== -->

  <!-- 1. NORMAL MODE (中央メイン) -->
  <g filter="url(#shadow)">
    <rect x="250" y="170" width="240" height="100" rx="10" fill="#2563eb"/>
    <text x="370" y="200" text-anchor="middle" font-size="16" font-weight="bold" fill="#ffffff">NORMAL MODE</text>
    <line x1="270" y1="212" x2="470" y2="212" stroke="#60a5fa" stroke-width="1"/>
    <text x="370" y="232" text-anchor="middle" font-size="12" fill="#dbeaff">Navigation &amp; Editing</text>
    <text x="370" y="252" text-anchor="middle" font-size="11" font-weight="bold" fill="#93c5fd">hjkl, x, dd, yy, p, c, s, u, .</text>
  </g>

  <!-- 2. INSERT MODE (上) -->
  <g filter="url(#shadow)">
    <rect x="250" y="40" width="240" height="75" rx="8" fill="#10b981"/>
    <text x="370" y="68" text-anchor="middle" font-size="15" font-weight="bold" fill="#ffffff">INSERT MODE</text>
    <line x1="270" y1="78" x2="470" y2="78" stroke="#34d399" stroke-width="1"/>
    <text x="370" y="98" text-anchor="middle" font-size="12" fill="#d1fae5">Direct Text Entry</text>
  </g>

  <!-- 3. VISUAL MODE (左下) -->
  <g filter="url(#shadow)">
    <rect x="40" y="320" width="220" height="80" rx="8" fill="#d97706"/>
    <text x="150" y="348" text-anchor="middle" font-size="15" font-weight="bold" fill="#ffffff">VISUAL MODE</text>
    <line x1="60" y1="358" x2="240" y2="358" stroke="#fbbf24" stroke-width="1"/>
    <text x="150" y="378" text-anchor="middle" font-size="12" fill="#fef3c7">Text Selection (Char / Line)</text>
  </g>

  <!-- 4. COMMAND-LINE MODE (右下) -->
  <g filter="url(#shadow)">
    <rect x="480" y="320" width="220" height="80" rx="8" fill="#4b5563"/>
    <text x="590" y="348" text-anchor="middle" font-size="15" font-weight="bold" fill="#ffffff">COMMAND-LINE</text>
    <line x1="500" y1="358" x2="680" y2="358" stroke="#9ca3af" stroke-width="1"/>
    <text x="590" y="378" text-anchor="middle" font-size="12" fill="#f3f4f6">ex Commands (:w, :q, :e, /pat)</text>
  </g>
</svg>
```
