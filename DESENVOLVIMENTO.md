<p align="center">
  <a href="DEVELOPMENT.md">English</a> &nbsp;|&nbsp; <b>Português (Brasil)</b>
</p>

# Guia de desenvolvimento

Como compilar, testar, traduzir e publicar o Gordeck.

## Compilar o Flatpak

Requisitos: `flatpak`, o [Flatpak Builder](https://flathub.org/apps/org.flatpak.Builder) e o SDK do KDE.

```bash
flatpak install --user flathub org.kde.Sdk//5.15-25.08 org.kde.Platform//5.15-25.08 org.flatpak.Builder
```

Compilar e instalar:

```bash
flatpak run org.flatpak.Builder --user --install --force-clean build-dir flatpak-build.yaml
flatpak run com.bnx.gordeck
```

## Compilar nativamente

O script `.github/scripts/build-native.sh` instala as dependências, compila, roda os testes unitários e gera o pacote `.deb`/`.rpm` em Ubuntu, Debian, Fedora e Arch Linux. Para usá-lo sem mexer no sistema, rode dentro de um contêiner:

```bash
podman run --rm -v "$PWD":/src:ro,Z -w /src -e BUILD_DIR=/tmp/build -e DIST_DIR=/tmp/dist \
	docker.io/library/ubuntu:24.04 bash .github/scripts/build-native.sh
```

Troque a imagem por `debian:12`, `registry.fedoraproject.org/fedora:41` ou `archlinux:latest` para as outras distribuições.

## Instalador e integração com a Steam

| Arquivo | Função |
|---|---|
| `installer/install.sh` | Instalador gráfico (zenity) para o Steam Deck |
| `installer/add-to-steam.py` | Adiciona o Gordeck à Steam como jogo não-Steam, com as artes |
| `installer/steam-art/` | Capas, banner, logo e ícone da Steam; gerados por `generate.py` |
| `installer/build-package.sh` | Compila o Flatpak e gera `dist/Gordeck-Instalador.tar.gz` |

O `add-to-steam.py` altera o `shortcuts.vdf` da Steam, que precisa estar fechada. Opções úteis: `--dry-run`, `--remove`, `--remove-legacy` (remove atalhos do Chiaki4deck) e `--steam-dir` (para testar numa cópia da pasta da Steam).

## Controle, sensores e áudio no Steam Deck

Regras que o código depende e que não são óbvias à primeira vista:

- **Nunca abra o hidraw do controle do Deck** (`28de:1205`). O driver `hid-steam` do kernel remove o gamepad evdev "Steam Deck", que é o que o SDL usa, assim que qualquer processo abre o nó. Por isso o `controllermanager.cpp` desliga o driver HID do SDL para o Deck com a dica `SDL_JOYSTICK_HIDAPI_STEAMDECK=0`, e nada no app fala com o controle por hidraw.
- **Sensores de movimento** vêm do evdev "Steam Deck Motion Sensors", que o kernel publica no mesmo layout do DualShock. A `setsu` o aceita pelo ID da Valve e a sessão o trata como qualquer controle com giroscópio. Controles com sensores vistos pelo SDL usam o caminho em `Controller::HandleSensorEvent`.
- **Trackpads** vêm do gamepad evdev: eixos HAT0 (esquerdo) e HAT1 (direito), cliques em BTN_THUMB e BTN_THUMB2, e (0, 0) quer dizer "sem toque". A `setsu` os apresenta como um touchpad virtual de 1920x1080, cada pad em uma metade, com Y espelhado.
- **Botões do Deck no SDL:** `misc1` é o "…", `paddle1` é R4, `paddle2` é L4, `paddle3` é R5 e `paddle4` é L5. O botão do microfone é escolhido nas configurações a partir desses nomes.
- **Vídeo:** `videoreceiver.c` nunca entrega ao decodificador um frame cuja FEC falhou; ele pede um IDR, pula P-frames até o IDR chegar e rastreia os últimos 16 frames de referência (`bitstream.c`, do chiaki-ng, e `vl_rbsp.h`, do Mesa, licença MIT). O `ffmpegdecoder.c` descarta frames que o FFmpeg marcou como corrompidos.
- **Áudio de saída** usa SDL (`SDL_QueueAudio`), com buffer padrão de 20 ms e descarte do acúmulo acima de 100 ms. O Flatpak compila o SDL com o backend PulseAudio; o backend PipeWire do SDL 2.30 não compila contra o PipeWire 1.4 do SDK.
- **Vibração:** a sessão sempre se apresenta ao console como DualSense (`enable_dualsense`), porque sem a mensagem CONTROLLERCONNECTION o PS5 não vibra nada. O PS5 então envia a háptica como PCM de 3 kHz, roteada a `haptics_sink`, e `StreamSession::PushHapticsFrame` a converte em rumble. No Deck a vibração é fraca porque o kernel usa o nível de intensidade mais baixo do firmware, e mudar isso exigiria o hidraw.
- **Microfone:** captura por SDL a 48 kHz estéreo em frames de 10 ms, `opusencoder.c` codifica, `audiosender.c` monta os pacotes e `chiaki_takion_send_mic_packet` envia. O console precisa receber o STREAMINFO de entrada (`stream_connection_enable_microphone`) e as mensagens de controle 0x30 (conectar) e 0x36 (silenciar).

No log de uma sessão, as linhas que confirmam cada parte: `Takion socket receive buffer`, `Connected Setsu Motion Device`, `Connected Setsu Touchpad Device`, `announcing controller as DualSense`, `enabling microphone input`, `Microphone ... opened with SDL`, `Received IDR frame`.

## Traduções

Os textos da interface ficam em `gui/i18n/` e são embutidos no executável durante o build. Depois de mudar textos no código, atualize o arquivo de tradução e preencha as entradas novas:

```bash
cd gui
lupdate -no-obsolete -locations none src include -ts i18n/chiaki_pt_BR.ts
```

## Integração contínua e releases

O workflow `.github/workflows/build.yml` roda a cada push e pull request:

- compila e roda os testes em Ubuntu 24.04, Debian 12, Fedora 41 e Arch Linux;
- gera os pacotes `.deb` e `.rpm`, o `gordeck.flatpak` e o `Gordeck-Instalador.tar.gz`.

Para publicar uma versão, envie uma tag; o workflow cria o release com todos os arquivos:

```bash
git tag v2.1.1
git push origin v2.1.1
```

A versão fica em `CMakeLists.txt` (`CHIAKI_VERSION_MAJOR`, `MINOR` e `PATCH`).
