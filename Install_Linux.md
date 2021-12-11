### Floorp for Linux のインストールはこちら。ターミナル（端末）に貼り付けてください。


```
sudo apt install flatpak && sudo apt install curl

sudo flatpak remote-add --if-not-exists flathub https://flathub.org/repo/flathub.flatpakrepo

sudo flatpak install org.freedesktop.Platform/x86_64/21.08

sudo wget https://sda1.net/pubkeys/repo.gpg && flatpak remote-add --if-not-exists floorp https://sda1.net/repo/floorp/ --gpg-import=repo.gpg && rm repo.gpg && flatpak install org.ablaze.floorp 
```
