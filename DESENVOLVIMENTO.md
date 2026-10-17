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
