<p align="center"><img src="gui/res/gordeck.svg" width="160" alt="Ícone do Gordeck"></p>

# Gordeck

Cliente de **Remote Play para PlayStation 4 e PlayStation 5**, livre e de código aberto, pensado para o **Steam Deck**.

O Gordeck é baseado no [chiaki4deck](https://streetpea.github.io/chiaki4deck/), de Street Pea, que por sua vez é baseado no [Chiaki](https://git.sr.ht/~thestr4ng3r/chiaki), de thestr4ng3r.

**Aviso:** este projeto não é endossado nem certificado pela Sony Interactive Entertainment LLC.

## Novidades em relação ao chiaki4deck

- **Interface em tela cheia no estilo EmulationStation**: carrossel horizontal de consoles com cards grandes, animações e o estado de cada console (pronto, em repouso ou adicionado manualmente).
- **Navegação completa pelo controle**: tela principal, menus e diálogos podem ser usados sem teclado nem mouse.
- **Botões do controle que você está usando**: a barra de dicas mostra os botões do Steam Deck, do DualShock/DualSense ou de um controle genérico, conforme o último controle usado. Sem controle, mostra as teclas do teclado.
- **Ligar e conectar com um botão**: se o console estiver em repouso, o Gordeck manda o sinal para ligá-lo e inicia a transmissão sozinho assim que o console estiver pronto.
- **Totalmente em português do Brasil**, com o inglês como idioma alternativo (em *Configurações → Interface → Idioma*).
- Tema escuro em toda a interface, ícones novos e tela de toque (toque para selecionar, toque de novo para conectar, deslize para trocar de console).

## Comandos

| Controle | Teclado | Ação |
|---|---|---|
| Direcional / analógico esquerdo | ← → | Navegar entre os consoles |
| A | Enter | Conectar, registrar ou adicionar console |
| B | Esc | Sair |
| X | W | Ligar o console |
| Y | O | Opções do console (editar, excluir) |
| Menu (≡) | F1 | Configurações |
| View (⧉) | F3 | Ativar ou desativar a busca automática |
| L1 / R1 | Home / End | Ir para o primeiro / último console |
| — | F11 | Alternar tela cheia |

Dentro dos diálogos, use cima/baixo para mudar o foco, A para confirmar e B para cancelar.

> **Dica:** abra o Gordeck pela Steam (no modo Jogo ou como jogo não-Steam no modo Desktop). Fora da Steam, o controle embutido do Steam Deck não é reconhecido como controle.

## Instalação no Steam Deck

1. No modo Desktop, baixe e extraia o `Gordeck-Instalador.tar.gz`.
2. Dê um duplo clique em **Instalar Gordeck** (ou rode `./install.sh`).
3. Escolha **Instalar o Gordeck** e siga as instruções.

O instalador gráfico baixa do Flathub os componentes do KDE que o Gordeck usa, instala o app, importa os consoles registrados do Chiaki4deck (se houver) e, se você quiser, **adiciona o Gordeck à biblioteca da Steam com ícone, capas, banner e logo**, pronto para o modo Jogo. O mesmo instalador atualiza e desinstala o Gordeck.

Também é possível só adicionar à Steam um Gordeck já instalado:

```bash
python3 installer/add-to-steam.py
```

(feche a Steam antes; a Steam reescreve a lista de atalhos ao sair).

### Gerar o pacote do instalador

```bash
./installer/build-package.sh
```

O pacote fica em `dist/Gordeck-Instalador.tar.gz`. As artes da Steam ficam em `installer/steam-art/` e são geradas por `installer/steam-art/generate.py`.

## Compilação e instalação (Flatpak)

São necessários o `flatpak` e o [Flatpak Builder](https://flathub.org/apps/org.flatpak.Builder), além do SDK do KDE:

```bash
flatpak install --user flathub org.kde.Sdk//5.15-25.08 org.kde.Platform//5.15-25.08 org.flatpak.Builder
```

Para compilar e instalar:

```bash
flatpak run org.flatpak.Builder --user --install --force-clean build-dir flatpak-build.yaml
```

Depois, é só abrir o Gordeck pelo menu de aplicativos ou adicioná-lo à Steam como jogo não-Steam.

## Traduções

Os textos da interface ficam em `gui/i18n/`. Para atualizar o arquivo de tradução depois de mudar textos no código:

```bash
cd gui
lupdate -no-obsolete -locations none src include -ts i18n/chiaki_pt_BR.ts
```

As traduções são compiladas e embutidas no executável durante o build.

## Documentação

A documentação do chiaki4deck, que também vale para o Gordeck (registro do console, encaminhamento de portas, uso remoto etc.), está em [streetpea.github.io/chiaki4deck](https://streetpea.github.io/chiaki4deck/).

## Licença

Distribuído sob a [GNU Affero General Public License versão 3](COPYING) (AGPLv3), como o Chiaki e o chiaki4deck.
