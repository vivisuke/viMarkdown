```SVG
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 400 400" width="360" height="360">
  <defs>
    <!-- 矢印マーカー -->
    <marker id="arrow-blue" markerWidth="6" markerHeight="6" refX="5" refY="3" orient="auto">
      <polygon points="0 0, 6 3, 0 6" fill="#3b82f6" />
    </marker>
    <marker id="arrow-green" markerWidth="6" markerHeight="6" refX="5" refY="3" orient="auto">
      <polygon points="0 0, 6 3, 0 6" fill="#10b981" />
    </marker>
    <marker id="arrow-amber" markerWidth="6" markerHeight="6" refX="5" refY="3" orient="auto">
      <polygon points="0 0, 6 3, 0 6" fill="#f59e0b" />
    </marker>
    <marker id="arrow-purple" markerWidth="6" markerHeight="6" refX="5" refY="3" orient="auto">
      <polygon points="0 0, 6 3, 0 6" fill="#8b5cf6" />
    </marker>
  </defs>

  <style>
    .bg { fill: #ffffff; }
    .box { rx: 8; stroke-width: 1.5; }
    .box-title { font-family: 'MS Gothic', sans-serif; font-size: 15px; font-weight: bold; text-anchor: middle; dominant-baseline: central; }
    .box-sub { font-family: 'MS Gothic', sans-serif; font-size: 12px; text-anchor: middle; dominant-baseline: central; }
    .center-label { font-family: 'MS Gothic', sans-serif; font-size: 18px; font-weight: bold; fill: #64748b; text-anchor: middle; dominant-baseline: central; }
    .arrow-path { fill: none; stroke-width: 2; stroke-dasharray: 4 2; }
  </style>

  <!-- 背景 -->
  <rect class="bg" width="400" height="400" rx="12" />

  <!-- 中央ラベル -->
  <circle cx="200" cy="200" r="35" fill="#f8fafc" stroke="#e2e8f0" stroke-width="1.5" />
  <text class="center-label" x="200" y="200">PDCA</text>

  <!-- 1. Plan (上・青) -->
  <rect class="box" x="140" y="30" width="120" height="56" fill="#eff6ff" stroke="#3b82f6" />
  <text class="box-title" x="200" y="48" fill="#1d4ed8">Plan</text>
  <text class="box-sub" x="200" y="68" fill="#3b82f6">計画</text>

  <!-- 2. Do (右・緑) -->
  <rect class="box" x="270" y="172" width="120" height="56" fill="#ecfdf5" stroke="#10b981" />
  <text class="box-title" x="330" y="190" fill="#047857">Do</text>
  <text class="box-sub" x="330" y="210" fill="#10b981">実行</text>

  <!-- 3. Check (下・黄/オレンジ) -->
  <rect class="box" x="140" y="314" width="120" height="56" fill="#fffbeb" stroke="#f59e0b" />
  <text class="box-title" x="200" y="332" fill="#b45309">Check</text>
  <text class="box-sub" x="200" y="352" fill="#f59e0b">評価・測定</text>

  <!-- 4. Action (左・紫) -->
  <rect class="box" x="10" y="172" width="120" height="56" fill="#f5f3ff" stroke="#8b5cf6" />
  <text class="box-title" x="70" y="190" fill="#6d28d9">Action</text>
  <text class="box-sub" x="70" y="210" fill="#8b5cf6">改善</text>

  <!-- 循環の矢印アーク (時計回り) -->
  <!-- Plan -> Do -->
  <path class="arrow-path" d="M 265 65 A 140 140 0 0 1 325 162" stroke="#3b82f6" marker-end="url(#arrow-blue)" />

  <!-- Do -> Check -->
  <path class="arrow-path" d="M 325 238 A 140 140 0 0 1 265 335" stroke="#10b981" marker-end="url(#arrow-green)" />

  <!-- Check -> Action -->
  <path class="arrow-path" d="M 135 335 A 140 140 0 0 1 75 238" stroke="#f59e0b" marker-end="url(#arrow-amber)" />

  <!-- Action -> Plan -->
  <path class="arrow-path" d="M 75 162 A 140 140 0 0 1 135 65" stroke="#8b5cf6" marker-end="url(#arrow-purple)" />
</svg>
```
