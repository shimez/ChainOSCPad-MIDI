# GitHub / Web Installerの運用

- リポジトリ： https://github.com/shimez/ChainOSCPad-MIDI
- Web Installer： https://shimez.github.io/ChainOSCPad-MIDI/installer/

## 自動化

`.github/workflows/firmware.yml`が次を行います。

1. `main`へのpushとPull Requestで全4ボードをビルドします。
2. `v*.*.*`タグのpushでは、タグと`VERSION`の一致を検証します。
3. PlatformIOのfactoryイメージを検証し、各ボードのBINとSHA256を生成します。
4. 全4ボード成功後、GitHub Releaseへ配布ファイルを公開します。
5. 同じ配布ファイルからマニフェストと静的サイトを生成し、GitHub Pagesへデプロイします。

Release作成とPages公開は同じworkflow内で実行します。`GITHUB_TOKEN`によるRelease作成が別workflowを起動しない制約に依存しません。GitHub Settings → Pages → Sourceは **GitHub Actions** に設定してください。

Settings → Environments → `github-pages` → Deployment branches and tagsでは、**Tagの`v*`を許可**してください。Pages初期設定の`main`だけでは、タグからのデプロイが拒否されます。このリポジトリは設定済みです。

## 次のリリース

1. `VERSION`を更新します（例：`0.1.1`）。必要に応じてREADMEとテスト記録を更新します。
2. コミットしてmainへpushします。
3. 同じバージョンのタグをpushします。

```powershell
git add VERSION README.md TESTING.md
git commit -m "Prepare v0.1.1"
git push origin main
git tag -a v0.1.1 -m "ChainOSCPad MIDI v0.1.1"
git push origin v0.1.1
```

公開ページはリリースしたタグの内容です。`docs/`だけの更新も次のリリースに含めて公開してください。手動実行をmainで行うとビルドのみ、タグを選択するとReleaseとPages公開まで実行します。古いタグで再実行するとPagesもその版へ戻るため、再実行するタグを確認してください。

## ローカル検証

```powershell
python -m unittest discover -s tests -v
pio run
python scripts/release.py package --board xiao_esp32s3
python scripts/release.py package --board xiao_esp32c3
python scripts/release.py package --board xiao_esp32c6
python scripts/release.py package --board xiao_esp32c5
python scripts/release.py site
python -m http.server 8080 --directory _site
```

`http://localhost:8080/installer/`で確認します。Web SerialはHTTPSまたはlocalhostで使用します。`docs/`はサイトのソースであり、生成されるマニフェストとBINは`_site/`に入ります。

factoryイメージはアドレス0から書き込みます。C5のブートローダーは内部の0x2000に配置されており、マニフェスト側に0x2000を指定すると二重にずれるため、全ボードの書き込みoffsetは0です。パッケージ作成時にbootloader・partition・applicationとの一致、サイト生成時にSHA256を検証します。

`dist/`、`_site/`、`.pio/`はGit管理しません。配布BINはGitHub ReleasesとActions artifactsで管理します。
