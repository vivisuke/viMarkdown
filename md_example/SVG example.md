# SVG example

```SVG
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 850 480" width="80%" height="100%">
  <defs>
    <style>
      .title { font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif; font-weight: 700; font-size: 20px; fill: #111827; }
      .header-text { font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif; font-weight: 700; font-size: 11px; fill: #ffffff; letter-spacing: 1.5px; }
      .box-title { font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif; font-weight: 600; font-size: 14px; }
      .box-sub { font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif; font-weight: 400; font-size: 11px; fill: #6b7280; }
      .arrow { stroke: #374151; stroke-width: 2; fill: none; }
      .arrow-head { fill: #374151; }
    </style>
    <!-- Arrow Marker Definition -->
    <marker id="arrow" viewBox="0 0 10 10" refX="6" refY="5" markerWidth="6" markerHeight="6" orient="auto-start-reverse">
      <path d="M 0 1 L 10 5 L 0 9 z" class="arrow-head" />
    </marker>
  </defs>

  <!-- Background Base Canvas -->
  <rect x="10" y="10" width="830" height="460" rx="16" fill="#f8fafc" stroke="#334155" stroke-width="2.5" />

  <!-- Top Title Area -->
  <line x1="40" y1="32" x2="40" y2="52" stroke="#10b981" stroke-width="4" stroke-linecap="round" />
  <text x="52" y="48" class="title">viMarkdown Architecture Flow</text>

  <!-- ==================== 1. PRESENTATION LAYER ==================== -->
  <rect x="35" y="75" width="240" height="370" rx="12" fill="#eff6ff" stroke="#3b82f6" stroke-width="1.5" stroke-dasharray="4 4" />
  <!-- Category Tag -->
  <rect x="50" y="90" width="210" height="28" rx="14" fill="#2563eb" />
  <text x="155" y="108" class="header-text" text-anchor="middle">PRESENTATION</text>
  
  <!-- Editor Box -->
  <rect x="55" y="145" width="200" height="110" rx="10" fill="#ede9fe" stroke="#8b5cf6" stroke-width="1.5" />
  <text x="155" y="195" class="box-title" fill="#5b21b6" text-anchor="middle">vi Editor View</text>
  <text x="155" y="215" class="box-sub" text-anchor="middle">(Qt / QTextCursor)</text>

  <!-- Preview Box -->
  <rect x="55" y="295" width="200" height="110" rx="10" fill="#ede9fe" stroke="#8b5cf6" stroke-width="1.5" />
  <text x="155" y="345" class="box-title" fill="#5b21b6" text-anchor="middle">Live Preview</text>
  <text x="155" y="365" class="box-sub" text-anchor="middle">(Interactive View)</text>

  <!-- ==================== 2. CORE / DOMAIN LAYER ==================== -->
  <rect x="305" y="75" width="240" height="370" rx="12" fill="#f0fdf4" stroke="#22c55e" stroke-width="1.5" stroke-dasharray="4 4" />
  <!-- Category Tag -->
  <rect x="320" y="90" width="210" height="28" rx="14" fill="#059669" />
  <text x="425" y="108" class="header-text" text-anchor="middle">CORE / DOMAIN</text>

  <!-- Parser & AST Box -->
  <rect x="325" y="145" width="200" height="110" rx="10" fill="#fce7f3" stroke="#ec4899" stroke-width="1.5" />
  <text x="425" y="195" class="box-title" fill="#9d174d" text-anchor="middle">Markdown Engine</text>
  <text x="425" y="215" class="box-sub" text-anchor="middle">(Parser &amp; AST)</text>

  <!-- Document Model Box -->
  <rect x="325" y="295" width="200" height="110" rx="10" fill="#fce7f3" stroke="#ec4899" stroke-width="1.5" />
  <text x="425" y="345" class="box-title" fill="#9d174d" text-anchor="middle">Document Model</text>
  <text x="425" y="365" class="box-sub" text-anchor="middle">(State &amp; Vi Buffer)</text>

  <!-- ==================== 3. DATA / INFRASTRUCTURE LAYER ==================== -->
  <rect x="575" y="75" width="240" height="370" rx="12" fill="#fffbeb" stroke="#f59e0b" stroke-width="1.5" stroke-dasharray="4 4" />
  <!-- Category Tag -->
  <rect x="590" y="90" width="210" height="28" rx="14" fill="#d97706" />
  <text x="695" y="108" class="header-text" text-anchor="middle">DATA &amp; SYSTEM</text>

  <!-- File I/O Box -->
  <rect x="595" y="145" width="200" height="110" rx="10" fill="#ffedd5" stroke="#f97316" stroke-width="1.5" />
  <text x="695" y="195" class="box-title" fill="#9a3412" text-anchor="middle">File I/O Manager</text>
  <text x="695" y="215" class="box-sub" text-anchor="middle">(Async Read/Write)</text>

  <!-- System Tools Box -->
  <rect x="595" y="295" width="200" height="110" rx="10" fill="#ffedd5" stroke="#f97316" stroke-width="1.5" />
  <text x="695" y="345" class="box-title" fill="#9a3412" text-anchor="middle">Native Integrations</text>
  <text x="695" y="365" class="box-sub" text-anchor="middle">(Grep / Diff / Git)</text>

  <!-- ==================== ARROWS (FLOWS) ==================== -->
  <!-- Editor -> Parser -->
  <line x1="255" y1="200" x2="320" y2="200" class="arrow" marker-end="url(#arrow)" />
  
  <!-- Parser -> Preview -->
  <path d="M 325 215 C 290 230 290 320 260 335" class="arrow" marker-end="url(#arrow)" />
  
  <!-- Parser -> Document Model -->
  <line x1="425" y1="255" x2="425" y2="290" class="arrow" marker-end="url(#arrow)" />

  <!-- Document Model -> Editor -->
  <path d="M 325 365 C 280 365 280 230 260 215" class="arrow" marker-end="url(#arrow)" />

  <!-- Core <-> File I/O -->
  <line x1="525" y1="200" x2="590" y2="200" class="arrow" marker-end="url(#arrow)" />
  <line x1="595" y1="210" x2="530" y2="210" class="arrow" marker-end="url(#arrow)" />

  <!-- Core <-> System Tools -->
  <line x1="525" y1="350" x2="590" y2="350" class="arrow" marker-end="url(#arrow)" />
  <line x1="595" y1="360" x2="530" y2="360" class="arrow" marker-end="url(#arrow)" />
</svg>
```
