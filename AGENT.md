# AGENT.md

このファイルは、lhat=gdextension リポジトリで AI アシスタントが守るべき実務ルールを定義します。

## プロジェクト概要

新規グルー言語 L^ を Godotエンジンのスクリプトとして使用できるようにするGDExtensionプロジェクト

使用言語 C11
クロスプラットフォームビルドツール Cmake

まだ実用に供されていないので、**後方互換性を保つ必要はない**。
最少差分であることより、**最終的により少ないコードであることを重視**。既存コードを書き換えない100行の追加より、既存コードの形を変え、増分50行で収まる変更のほうを選ぶ。

## コーディング規約

### コメントは英語で書く

新しく書くソースコードのコメント（`.c` / `.h` / テスト / `CMake`）は英語。日本語で書かない。

既存の日本語コメントはそのまま残す。そのコメント自体を書き直すときだけ英語にする。

## その他のリソース

### lhat (L^)

@../lhat/

### godot

godot エンジンのソース

> `git clone --filter=blob:none --depth 1 --branch 4.7.1-stable git@github.com:godotengine/godot.git`

@../godot/

### lhatove

Love2D の L^ 使用版プロジェクト

@../lhatove/

### lhat-svg-tools

L^ソースをシンタックスハイライト済みのSVGに変換するツール

@../lhat-svg-tools/

### Lua 5.5.1

Lua 5.5.1のソース

@../lua-5.5.1/

### Luau

Luau のソース

@../luau/
