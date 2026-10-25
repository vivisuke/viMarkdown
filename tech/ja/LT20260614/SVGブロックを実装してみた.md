# マークダウンエディタ viMarkdown に
# SVG ブロックを実装してみたら
# 図が意外と簡単に描けるようになった話
## 目次
- 自己紹介
  - 自宅研究員（首席）
  - 某国立大学大学院 修士課程修了（工学修士）
  - K工学研究所（研究員）、L社（首席 Software Engineer）、J社（SE）
  - 主な作品：K\-Compiler, ViVi editor, さくさくエディタ, AI poker, Sudoku 等
- マークダウンエディタ開発中
  - https://github.com/vivisuke/viMarkdown
  - 主な特徴：
    - リアルタイムプレビュー
    - プレビュー内編集
    - vi コマンド
    - etc...
- SVGブロックを実装してみた（今日の本題）
  - \```SVG \<svg>...\</svg>\```
  - リアルタイムにプレビュー表示
  - 実装
    - \<svg ...> だけをパースし、サイズ取得
    - QImage 生成、LunaSVG で描画、表示
- 通常画像（PNG, JPEG等）との比較
  - メリット・デメリットがある
  - 適材適所
- 生SVGなんて誰が書くの？
  - 補完ダイアログ
    - 自動 or 手動
  - お手軽に図を生成
    - フリー素材
    - ベクター図形エディタで SVG を生成・貼り付け
    - 生成AI利用
- まとめ
  - 開発中の viMarkdown に SVG ブロックを実装してみた
  - 意外と簡単に図形を表示でき表現力が向上した
  - GitHub でソース・Windowsバイナリ公開
    - ダウンロード＆★よろしくー

## 

```CSV
比較項目,通常画像（PNG/JPEG等）,SVGブロック
適した素材,"写真、UIのスクリーンショット、緻密なCG・イラスト","フローチャート、アイコン、数式図、単純な構造図・ワイヤーフレーム"
作成,ペイントソフト,ドローソフト
文字検索,不可能,可能（図の中の単語も検索・置換できる）
表示品質,拡大するとぼやける,拡大しても常にくっきり高解像度
ファイル管理,画像ファイルを別管理（相対パス等）,マークダウンファイル内に内包可能
互換性,極めて高い（どこでも動く）,環境依存あり（エディタの対応が必要）
```

Gemini先生が生成した PDCAサイクル図：
```SVG
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 400 400" width="400" height="400">
  <defs>
    <!-- ドロップシャドウ効果 -->
    <filter id="shadow" x="-10%" y="-10%" width="125%" height="125%">
      <feDropShadow dx="2" dy="3" stdDeviation="3" flood-color="#000000" flood-opacity="0.1"/>
    </filter>
    <!-- 矢印の先端定義 -->
    <marker id="arrow" viewBox="0 0 10 10" refX="6" refY="5" markerWidth="6" markerHeight="6" orient="auto-start-reverse">
      <path d="M 0 1.5 L 8 5 L 0 8.5 z" fill="#A0AEC0"/>
    </marker>
  </defs>

  <!-- 背景グリッド（薄いガイドライン） -->
  <circle cx="200" cy="200" r="125" fill="none" stroke="#EDF2F7" stroke-width="1" stroke-dasharray="4 4"/>

  <!-- 巡回するコネクタ矢印（時計回り） -->
  <g fill="none" stroke="#A0AEC0" stroke-width="3" marker-end="url(#arrow)">
    <!-- P -> D -->
    <path d="M 260 80 Q 330 80 330 153" />
    <!-- D -> C -->
    <path d="M 330 235 Q 330 320 269 320" />
    <!-- C -> A -->
    <path d="M 140 320 Q 70 320 70 247" />
    <!-- A -> P -->
    <path d="M 70 165 Q 70 80 131 80" />
  </g>

  <!-- 1. PLAN (計画) -->
  <g filter="url(#shadow)">
    <rect x="140" y="45" width="120" height="70" rx="15" fill="#EBF3FC" stroke="#3182CE" stroke-width="2"/>
    <text x="200" y="74" font-family="MS Gothic" font-weight="bold" font-size="18" fill="#2B6CB0" text-anchor="middle">PLAN</text>
    <text x="200" y="96" font-family="MS Gothic" font-size="12" fill="#4A5568" text-anchor="middle">計画・設計</text>
  </g>

  <!-- 2. DO (実行) -->
  <g filter="url(#shadow)">
    <rect x="270" y="165" width="120" height="70" rx="15" fill="#F0FDF4" stroke="#38A169" stroke-width="2"/>
    <text x="330" y="194" font-family="MS Gothic" font-weight="bold" font-size="18" fill="#2F855A" text-anchor="middle">DO</text>
    <text x="330" y="216" font-family="MS Gothic" font-size="12" fill="#4A5568" text-anchor="middle">実行・試行</text>
  </g>

  <!-- 3. CHECK (評価) -->
  <g filter="url(#shadow)">
    <rect x="140" y="285" width="120" height="70" rx="15" fill="#FFFDF5" stroke="#D69E2E" stroke-width="2"/>
    <text x="200" y="314" font-family="MS Gothic" font-weight="bold" font-size="18" fill="#B7791F" text-anchor="middle">CHECK</text>
    <text x="200" y="336" font-family="MS Gothic" font-size="12" fill="#4A5568" text-anchor="middle">評価・検証</text>
  </g>

  <!-- 4. ACT (改善) -->
  <g filter="url(#shadow)">
    <rect x="10" y="165" width="120" height="70" rx="15" fill="#FFF5F5" stroke="#E53E3E" stroke-width="2"/>
    <text x="70" y="194" font-family="MS Gothic" font-weight="bold" font-size="18" fill="#C53030" text-anchor="middle">ACT</text>
    <text x="70" y="216" font-family="MS Gothic" font-size="12" fill="#4A5568" text-anchor="middle">改善・処置</text>
  </g>

  <!-- 中央のセンターバッジ -->
  <g filter="url(#shadow)">
    <circle cx="200" cy="200" r="35" fill="#FFFFFF" stroke="#CBD5E0" stroke-width="2"/>
    <text x="200" y="197" font-family="MS Gothic" font-weight="bold" font-size="13" fill="#4A5568" text-anchor="middle">PDCA</text>
    <text x="200" y="212" font-family="MS Gothic" font-size="9" fill="#718096" text-anchor="middle">CYCLE</text>
  </g>
</svg>
```
