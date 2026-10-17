<p align="center">
  <b>English</b> &nbsp;|&nbsp; <a href="README.pt-BR.md">Português (Brasil)</a>
</p>

<p align="center">
  <img src="assets/gordeck-banner.png" alt="Gordeck - Remote Play">
</p>

<h3 align="center">Play your PlayStation on the Steam Deck, from the couch, the bed or anywhere in the house.</h3>

<p align="center">
  <a href="https://github.com/Brunnexo/gordeck/releases/latest"><img src="https://img.shields.io/github/v/release/Brunnexo/gordeck?label=download&style=for-the-badge&color=2f8cff" alt="Download"></a>
  <a href="https://github.com/Brunnexo/gordeck/actions/workflows/build.yml"><img src="https://img.shields.io/github/actions/workflow/status/Brunnexo/gordeck/build.yml?branch=main&label=build&style=for-the-badge" alt="Build"></a>
  <a href="COPYING"><img src="https://img.shields.io/badge/license-AGPL--3.0-blue?style=for-the-badge" alt="AGPL-3.0 License"></a>
</p>

---

**Gordeck** streams your **PS5 or PS4** games to the **Steam Deck** over your home network, with video, audio and controller input, just like sitting in front of the TV. Free, open source and built to be used with nothing but the controller: open it, pick your console, press **A** and play.

<p align="center">
  <img src="assets/capturas/tela-principal.png" width="80%" alt="Gordeck main screen">
</p>

## Why Gordeck

🎮 **Built for the controller.** A full-screen interface in the style of emulation frontends, with a console carousel, menus and dialogs you navigate using only the D-pad and buttons. No keyboard, no mouse.

⚡ **One button to play.** If the console is in rest mode, Gordeck wakes the PlayStation and starts streaming on its own as soon as it is ready.

🕹️ **Shows the right buttons.** On-screen hints adapt to the controller you are using: Steam Deck, DualSense/DualShock or a generic gamepad.

🌎 **In your language.** The interface is available in English and Brazilian Portuguese, switchable in the settings.

🖥️ **Screen always on.** While streaming, the Steam Deck never dims the screen or goes to sleep while you play with the controller alone.

🚪 **Emergency exit.** Hold the home button for 7 seconds to end the stream at any time.

📚 **Right in your Steam library.** The installer adds Gordeck to Steam with an icon, artwork and banner, ready to launch from Gaming Mode.

<p align="center">
  <img src="assets/capturas/configuracoes.png" width="49%" alt="Settings">
  <img src="assets/capturas/opcoes.png" width="49%" alt="Console options">
</p>

## Installing on the Steam Deck

1. Switch to **Desktop Mode** (Steam button › Power › Switch to Desktop).
2. Download **`Gordeck-Instalador.tar.gz`** from the [releases page](https://github.com/Brunnexo/gordeck/releases/latest) and extract it.
3. Double-click **Instalar Gordeck** and follow the instructions.
4. Go back to **Gaming Mode**: Gordeck will be in your library, under **Non-Steam**.

The installer takes care of everything: it downloads the required components, installs the app, imports the consoles you had already registered in Chiaki4deck and adds Gordeck to Steam. Use the same installer to update or uninstall.

## Other Linux distributions

| System | Download |
|---|---|
| Any distribution (Flatpak) | `gordeck.flatpak` › `flatpak install --user gordeck.flatpak` |
| Ubuntu 24.04 and derivatives | `gordeck-*-ubuntu-24.04-*.deb` |
| Debian 12 | `gordeck-*-debian-12-*.deb` |
| Fedora 41 | `gordeck-*-fedora-41-*.rpm` |

All files are on the [releases page](https://github.com/Brunnexo/gordeck/releases/latest).

## Getting started

1. Turn on the PlayStation and connect the Steam Deck to the **same network**.
2. Open Gordeck. The console shows up on screen by itself (or use **Add console** to enter its IP).
3. Select the console and press **A** to register. On the PlayStation, open **Settings › System › Remote Play › Link Device** and type the PIN shown there into Gordeck.
4. Done! From now on, just select the console and press **A** to play.

> For both PS4 and PS5, Gordeck also asks for your **PSN account ID**. See how to get it in the [chiaki4deck documentation](https://streetpea.github.io/chiaki4deck/setup/configuration/).

## Controls

| Controller | Keyboard | Action |
|---|---|---|
| D-pad / stick | ← → | Pick a console |
| **A** | Enter | Connect, register or add |
| **B** | Esc | Back / quit |
| **X** | W | Wake the console |
| **Y** | O | Console options |
| **≡** Menu | F1 | Settings |
| **⧉** View | F3 | Automatic console discovery |
| Hold **home** for 7 s | — | End the stream |

## FAQ

**Do I need PlayStation Plus or any subscription?**
No. Remote Play is a free feature of the PS4 and PS5.

**Does it work away from home?**
It works best on the same Wi-Fi network. To play from outside, you need to configure your router; see the [chiaki4deck documentation](https://streetpea.github.io/chiaki4deck/).

**The controller does not respond in Gordeck.**
Launch Gordeck through Steam (in Gaming Mode or as a non-Steam game). Outside Steam, the Steam Deck controller is not recognized by the app.

**I was already using Chiaki4deck. Do I lose my consoles?**
No. The installer imports your registered consoles into Gordeck.

## Credits

Gordeck was born from [chiaki4deck](https://streetpea.github.io/chiaki4deck/), by Street Pea, which in turn is based on [Chiaki](https://git.sr.ht/~thestr4ng3r/chiaki), by thestr4ng3r. Thanks to everyone who contributed to those projects.

Want to build or contribute? See the [development guide](DESENVOLVIMENTO.md) (in Portuguese).

## License

Free software, distributed under the [GNU Affero General Public License version 3](COPYING).

<sub>PlayStation, PS4, PS5 and DualSense are trademarks of Sony Interactive Entertainment. Steam and Steam Deck are trademarks of Valve Corporation. Gordeck is an independent project, not affiliated with, endorsed by or certified by Sony or Valve.</sub>
