### Floorp for Linux のインストールはこちら。ターミナル（端末）に貼り付けてください。

#### (y/n)の選択が出てきた場合、y を選択してください flatpak、curl が必要ですので最初にインストールします。

```
sudo apt install flatpak && sudo apt install curl

sudo flatpak remote-add --if-not-exists flathub https://flathub.org/repo/flathub.flatpakrepo

sudo flatpak install org.freedesktop.Platform/x86_64/21.08

sudo wget https://sda1.net/pubkeys/repo.gpg && flatpak remote-add --if-not-exists floorp https://sda1.net/repo/floorp/ --gpg-import=repo.gpg && sudo rm repo.gpg && sudo flatpak install org.ablaze.floorp

```
完了後、再起動をすることをお勧めします。アプリ一覧に表示されない場合があるからです。

また、既定のブラウザへの設定が動作しない場合があります。Linux の設定から、既定のブラウザを変更してみてください。設定から既定のブラウザの確認をオフにできます。
