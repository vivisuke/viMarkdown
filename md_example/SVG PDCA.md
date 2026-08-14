# SVG PDCA

```SVG
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 640 640" width="100%" height="100%" style="background-color: #ffffff; font-family: 'Segoe UI', 'Helvetica Neue', sans-serif;">
  <defs>
    <!-- ドロップシャドウ フィルター -->
    <filter id="shadow" x="-10%" y="-10%" width="120%" height="120%">
      <feDropShadow dx="0" dy="4" stdDeviation="6" flood-color="#000000" flood-opacity="0.1"/>
    </filter>

    <!-- 矢印マーカーの定義 -->
    <marker id="arrow-plan" viewBox="0 0 10 10" refX="6" refY="5" markerWidth="7" markerHeight="7" orient="auto-start-reverse">
      <path d="M 0 1 L 10 5 L 0 9 z" fill="#339af0"/>
    </marker>
    <marker id="arrow-do" viewBox="0 0 10 10" refX="6" refY="5" markerWidth="7" markerHeight="7" orient="auto-start-reverse">
      <path d="M 0 1 L 10 5 L 0 9 z" fill="#51cf66"/>
    </marker>
    <marker id="arrow-check" viewBox="0 0 10 10" refX="6" refY="5" markerWidth="7" markerHeight="7" orient="auto-start-reverse">
      <path d="M 0 1 L 10 5 L 0 9 z" fill="#fcc419"/>
    </marker>
    <marker id="arrow-act" viewBox="0 0 10 10" refX="6" refY="5" markerWidth="7" markerHeight="7" orient="auto-start-reverse">
      <path d="M 0 1 L 10 5 L 0 9 z" fill="#ff6b6b"/>
    </marker>
  </defs>

  <!-- タイトル -->
  <text x="320" y="40" text-anchor="middle" font-size="20" font-weight="bold" fill="#212529">PDCA Management Cycle</text>

  <!-- 周回コネクタ矢印 -->
  <!-- Plan -> Do -->
  <path d="M 480 265 A 170 170 0 0 1 480 375" fill="none" stroke="#339af0" stroke-width="5" stroke-dasharray="8 4" marker-end="url(#arrow-do)"/>
  <!-- Do -> Check -->
  <path d="M 375 480 A 170 170 0 0 1 265 480" fill="none" stroke="#51cf66" stroke-width="5" stroke-dasharray="8 4" marker-end="url(#arrow-check)"/>
  <!-- Check -> Act -->
  <path d="M 160 375 A 170 170 0 0 1 160 265" fill="none" stroke="#fcc419" stroke-width="5" stroke-dasharray="8 4" marker-end="url(#arrow-act)"/>
  <!-- Act -> Plan -->
  <path d="M 265 160 A 170 170 0 0 1 375 160" fill="none" stroke="#ff6b6b" stroke-width="5" stroke-dasharray="8 4" marker-end="url(#arrow-plan)"/>

  <!-- 中央のサークル -->
  <circle cx="320" cy="320" r="85" fill="#f8f9fa" stroke="#e9ecef" stroke-width="3" filter="url(#shadow)"/>
  <text x="320" y="300" text-anchor="middle" font-size="22" font-weight="bold" fill="#212529">PDCA</text>
  <text x="320" y="325" text-anchor="middle" font-size="14" font-weight="bold" fill="#495057">CYCLE</text>
  <text x="320" y="348" text-anchor="middle" font-size="11" fill="#868e96">Continuous</text>
  <text x="320" y="362" text-anchor="middle" font-size="11" fill="#868e96">Improvement</text>

  <!-- 1. PLAN (右上) -->
  <g filter="url(#shadow)">
    <circle cx="460" cy="180" r="75" fill="#339af0"/>
    <text x="460" y="158" text-anchor="middle" font-size="20" font-weight="bold" fill="#ffffff">PLAN</text>
    <text x="460" y="182" text-anchor="middle" font-size="12" font-weight="bold" fill="#e7f5ff">1. Set Goals</text>
    <text x="460" y="198" text-anchor="middle" font-size="12" fill="#e7f5ff">&amp; Strategy</text>
  </g>

  <!-- 2. DO (右下) -->
  <g filter="url(#shadow)">
    <circle cx="460" cy="460" r="75" fill="#51cf66"/>
    <text x="460" y="438" text-anchor="middle" font-size="20" font-weight="bold" fill="#ffffff">DO</text>
    <text x="460" y="462" text-anchor="middle" font-size="12" font-weight="bold" fill="#ebfbee">2. Implement</text>
    <text x="460" y="478" text-anchor="middle" font-size="12" fill="#ebfbee">&amp; Execute</text>
  </g>

  <!-- 3. CHECK (左下) -->
  <g filter="url(#shadow)">
    <circle cx="180" cy="460" r="75" fill="#fcc419"/>
    <text x="180" y="438" text-anchor="middle" font-size="20" font-weight="bold" fill="#ffffff">CHECK</text>
    <text x="180" y="462" text-anchor="middle" font-size="12" font-weight="bold" fill="#fff9db">3. Measure &amp;</text>
    <text x="180" y="478" text-anchor="middle" font-size="12" fill="#fff9db">Evaluate</text>
  </g>

  <!-- 4. ACT (左上) -->
  <g filter="url(#shadow)">
    <circle cx="180" cy="180" r="75" fill="#ff6b6b"/>
    <text x="180" y="158" text-anchor="middle" font-size="20" font-weight="bold" fill="#ffffff">ACT</text>
    <text x="180" y="182" text-anchor="middle" font-size="12" font-weight="bold" fill="#fff5f5">4. Adjust &amp;</text>
    <text x="180" y="198" text-anchor="middle" font-size="12" fill="#fff5f5">Improve</text>
  </g>
</svg>
```

