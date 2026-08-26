# セッションログ

## 静的ディスパッチ

GDScriptでは実現されている型が定まっている場合の静的ディスパッチを、L^でも実現するには、何が足らない？

---

核心は3層に分かれる。実測で確認した事実:

層1 — エンジン境界（拡張側だけ）: 今は gdobj.call("add_child", child) → 毎回 L^文字列→String::utf8→StringName 変換 + ClassDB 名前引き + Array 確保 + 引数ごと Variant boxing（lhat_godot_module.cpp:150）。GDScript は MethodBind*をコンパイル時確定 → ptrcall で生ポインタ渡し。足りないのは bind を握る仕組み。gdextension_interface_classdb_get_method_bind(class, method, hash) は godot-cpp にある。ただし hash が要る → 実測: 実行時 ClassDB.class_get_method_list は hash を返さない（MethodInfo::operator Dictionary に無い、method_info.cpp:37）→ extension_api.json からのビルド時生成が必須。登録側は lhat_register_member(..., LhatHostFn, void*context) の context に bind を入れられる → コア変更不要。引数も L^ hostvalue が既にエンジンと同レイアウトの生バイト（Vector2=8B、offset登録済み）→ ptrcall に直接渡せる。

層2 — 型システム（コア変更）: 今は godot.Object ただ1つ。理由は 05 の 8.8 — host型は nominal で継承が書けず、971クラスの木を表せない（lhat_godot_module.h:1-8 がそう書いている）。階層は demo/lhat/Godot.lh の def^ .. def^ で L^ 側にある。足りないのは host型に基底を宣言する手段。無いと godot.Sprite2D という型が作れない。

層3 — VM 特殊化（コア大改造、ただし設計が予告済み）: x.member は LHAT_BC_GETINDEX 汎用命令 — 実行時に受け手の型を見て分岐 → 文字列ハッシュ引き。03 の 5.1 が「まず汎用命令、特殊化は後（CPython 3.11 の順路）」と明言。材料は既にある: 検査器→AST→コード生成の経路（node->checked_arm→PICKARM）、インスタンスが持つ definition ポインタ（生成時固定、object.h:99）、封じられた定義テーブル→entry index 安定。足りないのは3つ: ①静的型が「どの定義か」を指さない（from_definition は bool、11.3 が同一性を構造的に保つ→幅部分型で同形の別定義が同じ型）②スロット番号で引く命令が無い ③境界検査 — 03 の 5.1 が自分で未起票と書いている「未検証の値が入ってくる境界で実物を見る」仕組み。
