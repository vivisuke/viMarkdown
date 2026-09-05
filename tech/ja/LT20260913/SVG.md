```SVG
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 1020 720" width="100%" height="100%" style="background-color: #f8fafc; font-family: MS Gothic, 'Segoe UI', Roboto, Helvetica, Arial, sans-serif;">
  <defs>
    <!-- ドロップシャドウ -->
    <filter id="card-shadow" x="-6%" y="-4%" width="112%" height="112%" filterUnits="userSpaceOnUse">
      <feDropShadow dx="0" dy="4" stdDeviation="6" flood-color="#0f172a" flood-opacity="0.06"/>
      <feDropShadow dx="0" dy="1" stdDeviation="2" flood-color="#0f172a" flood-opacity="0.04"/>
    </filter>
    <filter id="badge-shadow" x="-10%" y="-10%" width="120%" height="120%" filterUnits="userSpaceOnUse">
      <feDropShadow dx="0" dy="2" stdDeviation="2" flood-color="#000000" flood-opacity="0.1"/>
    </filter>

    <!-- 矢印マーカー -->
    <marker id="arrow" viewBox="0 0 10 10" refX="6" refY="5" markerWidth="6" markerHeight="6" orient="auto-start-reverse">
      <path d="M 0 1 L 8 5 L 0 9 z" fill="#94a3b8" />
    </marker>
  </defs>

  <!-- ==================== ヘッダー ==================== -->
  <g transform="translate(40, 36)">
    <rect x="0" y="0" width="120" height="24" rx="12" fill="#e0e7ff"/>
    <text x="60" y="16" fill="#4338ca" font-size="11" font-weight="700" text-anchor="middle" letter-spacing="0.5">ARCHITECTURE</text>
    
    <text x="0" y="52" fill="#0f172a" font-size="24" font-weight="800">viMarkdown インクリメンタル差分更新パイプライン</text>
    <text x="0" y="74" fill="#64748b" font-size="13">QTextDocument の変更通知からプレビュー局所再描画までのデータフロー</text>
  </g>

  <!-- ==================== パイプラインステップ (1〜5) ==================== -->

  <!-- STEP 1: 差分検出 -->
  <g transform="translate(40, 140)">
    <rect width="172" height="320" rx="14" fill="#ffffff" stroke="#e2e8f0" stroke-width="1.5" filter="url(#card-shadow)"/>
    <path d="M 0 14 Q 0 0 14 0 L 158 0 Q 172 0 172 14 L 172 6 L 0 6 Z" fill="#2563eb" />
    <circle cx="28" cy="34" r="14" fill="#eff6ff" stroke="#bfdbfe" stroke-width="1"/>
    <text x="28" y="39" fill="#1d4ed8" font-size="12" font-weight="800" text-anchor="middle">01</text>
    <text x="50" y="39" fill="#0f172a" font-size="15" font-weight="700">差分検出</text>

    <!-- アイコン: 編集検知 -->
    <g transform="translate(68, 62)">
      <circle cx="18" cy="18" r="22" fill="#eff6ff"/>
      <path d="M12 24 L24 24 M12 20 L18 20 M22 10 L25 13 L15 23 L12 24 L13 21 Z" stroke="#2563eb" stroke-width="2" stroke-linecap="round" stroke-linejoin="round" fill="none"/>
    </g>

    <!-- コアコードバッジ -->
    <rect x="10" y="125" width="152" height="42" rx="6" fill="#f1f5f9" stroke="#cbd5e1" stroke-width="1"/>
    <text x="86" y="142" fill="#0f172a" font-size="9" font-family="monospace" font-weight="700" text-anchor="middle">contentsChange()</text>
    <text x="86" y="156" fill="#64748b" font-size="8.5" font-family="monospace" text-anchor="middle">(int pos, int rem, int add)</text>

    <!-- 説明文 -->
    <text x="14" y="195" fill="#334155" font-size="12" font-weight="600">エディタ変更の傍受</text>
    <text x="14" y="217" fill="#64748b" font-size="11.5" line-height="1.5">
      <tspan x="14" dy="0">文書内の編集操作から</tspan>
      <tspan x="14" dy="17">変更開始位置・削除数・</tspan>
      <tspan x="14" dy="17">挿入文字数を即座に</tspan>
      <tspan x="14" dy="17">検知。</tspan>
    </text>

    <!-- フッタータグ -->
    <rect x="14" y="280" width="144" height="24" rx="6" fill="#f8fafc" stroke="#e2e8f0"/>
    <text x="86" y="296" fill="#64748b" font-size="10.5" text-anchor="middle">Qt イベント駆動</text>
  </g>

  <!-- コネクタ 1 -> 2 -->
  <line x1="216" y1="300" x2="236" y2="300" stroke="#94a3b8" stroke-width="2" marker-end="url(#arrow)" />

  <!-- STEP 2: 影響範囲解析 -->
  <g transform="translate(242, 140)">
    <rect width="172" height="320" rx="14" fill="#ffffff" stroke="#e2e8f0" stroke-width="1.5" filter="url(#card-shadow)"/>
    <path d="M 0 14 Q 0 0 14 0 L 158 0 Q 172 0 172 14 L 172 6 L 0 6 Z" fill="#4f46e5" />
    <circle cx="28" cy="34" r="14" fill="#eef2ff" stroke="#c7d2fe" stroke-width="1"/>
    <text x="28" y="39" fill="#4338ca" font-size="12" font-weight="800" text-anchor="middle">02</text>
    <text x="50" y="39" fill="#0f172a" font-size="15" font-weight="700">影響範囲解析</text>

    <!-- アイコン: スコープ -->
    <g transform="translate(68, 62)">
      <circle cx="18" cy="18" r="22" fill="#eef2ff"/>
      <rect x="10" y="9" width="16" height="6" rx="1" fill="#818cf8"/>
      <rect x="10" y="18" width="16" height="11" rx="1" fill="#c7d2fe"/>
      <circle cx="23" cy="24" r="5" fill="#4f46e5"/>
    </g>

    <rect x="10" y="125" width="152" height="42" rx="6" fill="#f1f5f9" stroke="#cbd5e1" stroke-width="1"/>
    <text x="86" y="143" fill="#4338ca" font-size="10" font-weight="700" text-anchor="middle">境界スキャン</text>
    <text x="86" y="157" fill="#0f172a" font-size="10.5" font-weight="700" text-anchor="middle">見出し ＋ 本文 単位</text>

    <text x="14" y="195" fill="#334155" font-size="12" font-weight="600">セクション境界の特定</text>
    <text x="14" y="217" fill="#64748b" font-size="11.5">
      <tspan x="14" dy="0">変更位置を含む直前見出し</tspan>
      <tspan x="14" dy="17">から次見出しの手前まで</tspan>
      <tspan x="14" dy="17">を更新対象ブロック</tspan>
      <tspan x="14" dy="17">として切り出し。</tspan>
    </text>

    <rect x="14" y="280" width="144" height="24" rx="6" fill="#fdf4ff" stroke="#fae8ff"/>
    <text x="86" y="296" fill="#a21caf" font-size="10" font-weight="600" text-anchor="middle">将来計画: 行・段落単位</text>
  </g>

  <!-- コネクタ 2 -> 3 -->
  <line x1="418" y1="300" x2="438" y2="300" stroke="#94a3b8" stroke-width="2" marker-end="url(#arrow)" />

  <!-- STEP 3: 部分解析 -->
  <g transform="translate(444, 140)">
    <rect width="172" height="320" rx="14" fill="#ffffff" stroke="#e2e8f0" stroke-width="1.5" filter="url(#card-shadow)"/>
    <path d="M 0 14 Q 0 0 14 0 L 158 0 Q 172 0 172 14 L 172 6 L 0 6 Z" fill="#7c3aed" />
    <circle cx="28" cy="34" r="14" fill="#f5f3ff" stroke="#ddd6fe" stroke-width="1"/>
    <text x="28" y="39" fill="#6d28d9" font-size="12" font-weight="800" text-anchor="middle">03</text>
    <text x="50" y="39" fill="#0f172a" font-size="15" font-weight="700">部分解析</text>

    <!-- アイコン: 構文解析 -->
    <g transform="translate(68, 62)">
      <circle cx="18" cy="18" r="22" fill="#f5f3ff"/>
      <path d="M12 12 L17 17 L12 22 M24 24 L18 24" stroke="#7c3aed" stroke-width="2" stroke-linecap="round" stroke-linejoin="round" fill="none"/>
    </g>

    <rect x="10" y="125" width="152" height="42" rx="6" fill="#f1f5f9" stroke="#cbd5e1" stroke-width="1"/>
    <text x="86" y="143" fill="#6d28d9" font-size="9.5" font-weight="700" text-anchor="middle">Scoped Parser</text>
    <text x="86" y="157" fill="#0f172a" font-size="10" font-family="monospace" text-anchor="middle">parse(sub_markdown)</text>

    <text x="14" y="195" fill="#334155" font-size="12" font-weight="600">局所 Markdown パース</text>
    <text x="14" y="217" fill="#64748b" font-size="11.5">
      <tspan x="14" dy="0">ドキュメント全体ではなく</tspan>
      <tspan x="14" dy="17">切り出された対象区間</tspan>
      <tspan x="14" dy="17">のみを解析器へ投入。</tspan>
      <tspan x="14" dy="17">CPU負荷を大幅削減。</tspan>
    </text>

    <rect x="14" y="280" width="144" height="24" rx="6" fill="#f8fafc" stroke="#e2e8f0"/>
    <text x="86" y="296" fill="#64748b" font-size="10.5" text-anchor="middle">高速パース処理</text>
  </g>

  <!-- コネクタ 3 -> 4 -->
  <line x1="620" y1="300" x2="640" y2="300" stroke="#94a3b8" stroke-width="2" marker-end="url(#arrow)" />

  <!-- STEP 4: 差分生成 -->
  <g transform="translate(646, 140)">
    <rect width="172" height="320" rx="14" fill="#ffffff" stroke="#e2e8f0" stroke-width="1.5" filter="url(#card-shadow)"/>
    <path d="M 0 14 Q 0 0 14 0 L 158 0 Q 172 0 172 14 L 172 6 L 0 6 Z" fill="#0891b2" />
    <circle cx="28" cy="34" r="14" fill="#ecfeff" stroke="#a5f3fc" stroke-width="1"/>
    <text x="28" y="39" fill="#0e7490" font-size="12" font-weight="800" text-anchor="middle">04</text>
    <text x="50" y="39" fill="#0f172a" font-size="15" font-weight="700">差分生成</text>

    <!-- アイコン: HTML生成 -->
    <g transform="translate(68, 62)">
      <circle cx="18" cy="18" r="22" fill="#ecfeff"/>
      <path d="M11 15 L15 11 L11 7 M25 7 L21 11 L25 15 M19 6 L17 18" stroke="#0891b2" stroke-width="1.8" stroke-linecap="round" fill="none"/>
    </g>

    <rect x="10" y="125" width="152" height="42" rx="6" fill="#f1f5f9" stroke="#cbd5e1" stroke-width="1"/>
    <text x="86" y="143" fill="#0e7490" font-size="10" font-weight="700" text-anchor="middle">差分リッチテキスト</text>
    <text x="86" y="157" fill="#0f172a" font-size="9.5" font-family="monospace" text-anchor="middle">&lt;div class="sec"&gt;...</text>

    <text x="14" y="195" fill="#334155" font-size="12" font-weight="600">中間断片のレンダリング</text>
    <text x="14" y="217" fill="#64748b" font-size="11.5">
      <tspan x="14" dy="0">解析した構文木をもとに</tspan>
      <tspan x="14" dy="17">該当セクションのみの</tspan>
      <tspan x="14" dy="17">リッチテキスト（HTML断片）</tspan>
      <tspan x="14" dy="17">をオンメモリ生成。</tspan>
    </text>

    <rect x="14" y="280" width="144" height="24" rx="6" fill="#f8fafc" stroke="#e2e8f0"/>
    <text x="86" y="296" fill="#64748b" font-size="10.5" text-anchor="middle">HTML/CSSフラグメント</text>
  </g>

  <!-- コネクタ 4 -> 5 -->
  <line x1="822" y1="300" x2="842" y2="300" stroke="#94a3b8" stroke-width="2" marker-end="url(#arrow)" />

  <!-- STEP 5: 成果物更新 -->
  <g transform="translate(848, 140)">
    <rect width="172" height="320" rx="14" fill="#ffffff" stroke="#e2e8f0" stroke-width="1.5" filter="url(#card-shadow)"/>
    <path d="M 0 14 Q 0 0 14 0 L 158 0 Q 172 0 172 14 L 172 6 L 0 6 Z" fill="#059669" />
    <circle cx="28" cy="34" r="14" fill="#ecfdf5" stroke="#a7f3d0" stroke-width="1"/>
    <text x="28" y="39" fill="#047857" font-size="12" font-weight="800" text-anchor="middle">05</text>
    <text x="50" y="39" fill="#0f172a" font-size="15" font-weight="700">成果物更新</text>

    <!-- アイコン: プレビュー更新 -->
    <g transform="translate(68, 62)">
      <circle cx="18" cy="18" r="22" fill="#ecfdf5"/>
      <rect x="9" y="8" width="18" height="18" rx="2" fill="none" stroke="#059669" stroke-width="2"/>
      <path d="M9 13 L27 13 M18 17 L23 21 M23 17 L18 21" stroke="#059669" stroke-width="1.8" stroke-linecap="round"/>
    </g>

    <rect x="10" y="125" width="152" height="42" rx="6" fill="#f1f5f9" stroke="#cbd5e1" stroke-width="1"/>
    <text x="86" y="143" fill="#047857" font-size="10" font-weight="700" text-anchor="middle">DOM局所置換</text>
    <text x="86" y="157" fill="#0f172a" font-size="9" font-family="monospace" text-anchor="middle">replaceChild() / innerHTML</text>

    <text x="14" y="195" fill="#334155" font-size="12" font-weight="600">プレビューの置換反映</text>
    <text x="14" y="217" fill="#64748b" font-size="11.5">
      <tspan x="14" dy="0">プレビュー画面の該当</tspan>
      <tspan x="14" dy="17">要素のみを差し替え。</tspan>
      <tspan x="14" dy="17">チラつきを抑え、スクロール</tspan>
      <tspan x="14" dy="17">位置も完全維持。</tspan>
    </text>

    <rect x="14" y="280" width="144" height="24" rx="6" fill="#ecfdf5" stroke="#a7f3d0"/>
    <text x="86" y="296" fill="#065f46" font-size="10.5" font-weight="600" text-anchor="middle">高速・シームレス描画</text>
  </g>

  <!-- ==================== 懸念事項 (Edge Cases & Concerns) ==================== -->
  <g transform="translate(40, 485)">
    <!-- セクションタイトル -->
    <g transform="translate(0, 0)">
      <rect x="0" y="2" width="20" height="20" rx="4" fill="#fef3c7"/>
      <path d="M10 6 L10 11 M10 14 L10.01 14" stroke="#d97706" stroke-width="2" stroke-linecap="round"/>
      <text x="28" y="18" fill="#0f172a" font-size="16" font-weight="800">実装上の懸念事項・エッジケース</text>
    </g>

    <!-- 懸念事項 1 -->
    <g transform="translate(0, 32)">
      <rect width="470" height="150" rx="12" fill="#ffffff" stroke="#fed7aa" stroke-width="1.5" filter="url(#card-shadow)"/>
      <rect x="0" y="0" width="6" height="150" rx="3" fill="#f97316"/>
      
      <text x="24" y="32" fill="#9a3412" font-size="14" font-weight="700">懸念 1：見出し行の削除・新規挿入</text>
      <rect x="360" y="18" width="94" height="20" rx="4" fill="#ffedd5"/>
      <text x="407" y="32" fill="#c2410c" font-size="10.5" font-weight="700" text-anchor="middle">セクション構造破壊</text>

      <g transform="translate(24, 46)">
        <text x="0" y="18" fill="#475569" font-size="12">
          <tspan x="0" dy="0" font-weight="600" fill="#1e293b">【現象】</tspan>
          <tspan>見出しの増減により、後続セクションの階層や境界が</tspan>
          <tspan x="48" dy="18">大きく変化（2つのセクションが合体、または分裂）。</tspan>
        </text>

        <rect x="0" y="44" width="422" height="42" rx="6" fill="#fff7ed"/>
        <text x="12" y="61" fill="#c2410c" font-size="11.5" font-weight="700">👉 対策方針：影響範囲解析の段階的フォールバック</text>
        <text x="12" y="77" fill="#64748b" font-size="11">見出し構造の変更を検知した場合は「前後の隣接セクション」または全体へ拡張</text>
      </g>
    </g>

    <!-- 懸念事項 2 -->
    <g transform="translate(510, 32)">
      <rect width="470" height="150" rx="12" fill="#ffffff" stroke="#fed7aa" stroke-width="1.5" filter="url(#card-shadow)"/>
      <rect x="0" y="0" width="6" height="150" rx="3" fill="#ea580c"/>
      
      <text x="24" y="32" fill="#9a3412" font-size="14" font-weight="700">懸念 2：コードブロック内の見出し表記 ( ^# )</text>
      <rect x="375" y="18" width="79" height="20" rx="4" fill="#ffedd5"/>
      <text x="414" y="32" fill="#c2410c" font-size="10.5" font-weight="700" text-anchor="middle">誤判定リスク</text>

      <g transform="translate(24, 46)">
        <text x="0" y="18" fill="#475569" font-size="12">
          <tspan x="0" dy="0" font-weight="600" fill="#1e293b">【現象】</tspan>
          <tspan>``` (コードブロック) 内にコメント等の # 行が存在すると、</tspan>
          <tspan x="48" dy="18">単純な行頭正規表現では誤って見出し境界と誤認。</tspan>
        </text>

        <rect x="0" y="44" width="422" height="42" rx="6" fill="#fff7ed"/>
        <text x="12" y="61" fill="#c2410c" font-size="11.5" font-weight="700">👉 対策方針：フェンス状態（In-Codeblock）のコンテキスト保持</text>
        <text x="12" y="77" fill="#64748b" font-size="11">ブロック開始行からのバッククォート開閉フラグを考慮して正規見出しを判定</text>
      </g>
    </g>
  </g>
</svg>
```
