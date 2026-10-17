#!/bin/bash
# SPDX-License-Identifier: LicenseRef-AGPL-3.0-only-OpenSSL
#
# Graphical installer of Gordeck for the Steam Deck (desktop mode).
# Installs the flatpak bundle next to this script, its runtime from Flathub,
# and optionally adds Gordeck to the Steam library with its artwork.

DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
APP_ID="com.bnx.gordeck"
LEGACY_APP_ID="re.chiaki.Chiaki4deck"
RUNTIME="org.kde.Platform//5.15-25.08"
BUNDLE="$DIR/gordeck.flatpak"
ICON="$DIR/steam-art/icon.png"
TITLE="Instalador do Gordeck"

# dark dialogs, like the app
export GTK_THEME="${GTK_THEME:-Adwaita:dark}"

if ! command -v zenity >/dev/null; then
	echo "zenity não foi encontrado. Rode este instalador no modo Desktop do Steam Deck." >&2
	exit 1
fi

ZENITY=(zenity --title="$TITLE" --width=480)
[ -f "$ICON" ] && ZENITY+=(--window-icon="$ICON")

info() { "${ZENITY[@]}" --info --text="$1" 2>/dev/null; }
error() { "${ZENITY[@]}" --error --text="$1" 2>/dev/null; }
ask() { "${ZENITY[@]}" --question --text="$1" --ok-label="${2:-Sim}" --cancel-label="${3:-Não}" 2>/dev/null; }

# Run a command while showing a progress dialog.
# The output of the failed command is kept in LAST_LOG.
LAST_LOG=""
run_step() {
	local text="$1"
	shift
	local log
	log="$(mktemp)"
	( "$@" >"$log" 2>&1; echo $? >"$log.rc" ) &
	local pid=$!
	(
		while kill -0 "$pid" 2>/dev/null; do
			echo "# $text"
			sleep 0.5
		done
		echo 100
	) | "${ZENITY[@]}" --progress --pulsate --auto-close --no-cancel --text="$text" 2>/dev/null
	wait "$pid"
	local rc
	rc="$(cat "$log.rc" 2>/dev/null || echo 1)"
	LAST_LOG="$(tail -n 15 "$log")"
	rm -f "$log" "$log.rc"
	return "$rc"
}

is_installed() {
	flatpak info --user "$1" >/dev/null 2>&1 || flatpak info --system "$1" >/dev/null 2>&1
}

steam_running() {
	pgrep -x steam >/dev/null
}

close_steam() {
	steam_running || return 0
	ask "A Steam precisa ser fechada para adicionar o Gordeck à biblioteca, senão ela desfaz a alteração ao sair.\n\nFechar a Steam agora?" "Fechar a Steam" "Cancelar" || return 1
	steam -shutdown >/dev/null 2>&1 &
	wait_steam_closed() {
		for _ in $(seq 1 60); do
			steam_running || return 0
			sleep 1
		done
		return 1
	}
	if ! run_step "Fechando a Steam…" wait_steam_closed; then
		error "A Steam não fechou a tempo. Feche a Steam e tente novamente."
		return 1
	fi
}

add_to_steam() {
	close_steam || return 1

	local args=()
	if python3 "$DIR/add-to-steam.py" --has-legacy 2>/dev/null; then
		if ask "Foi encontrado na biblioteca da Steam um atalho do antigo Chiaki4deck.\n\nRemover esse atalho?" "Remover" "Manter"; then
			args+=(--remove-legacy)
		fi
	fi

	if ! run_step "Adicionando o Gordeck à biblioteca da Steam…" python3 "$DIR/add-to-steam.py" --art-dir "$DIR/steam-art" "${args[@]}"; then
		error "Não foi possível adicionar o Gordeck à Steam:\n\n$LAST_LOG"
		return 1
	fi

	if ask "O Gordeck foi adicionado à biblioteca da Steam, com ícone e artes.\n\nAbrir a Steam agora?" "Abrir a Steam" "Agora não"; then
		setsid steam >/dev/null 2>&1 < /dev/null &
	fi
}

import_settings() {
	local old="$HOME/.var/app/$LEGACY_APP_ID/config/Chiaki/Chiaki.conf"
	local new="$HOME/.var/app/$APP_ID/config/Gordeck/Gordeck.conf"
	[ -f "$old" ] && [ ! -f "$new" ] || return 0
	if ask "Foram encontradas configurações do Chiaki4deck, incluindo os consoles registrados.\n\nImportar para o Gordeck?" "Importar" "Não importar"; then
		mkdir -p "$(dirname "$new")" && cp "$old" "$new"
	fi
}

install_gordeck() {
	if [ ! -f "$BUNDLE" ]; then
		error "O arquivo gordeck.flatpak não foi encontrado ao lado do instalador."
		return 1
	fi

	if ! run_step "Configurando o Flathub…" flatpak remote-add --user --if-not-exists flathub https://dl.flathub.org/repo/flathub.flatpakrepo; then
		error "Não foi possível configurar o Flathub:\n\n$LAST_LOG"
		return 1
	fi

	if ! run_step "Baixando os componentes do KDE (pode levar alguns minutos na primeira vez)…" \
			flatpak install --user -y --noninteractive flathub "$RUNTIME"; then
		error "Não foi possível instalar os componentes necessários.\nVerifique a conexão com a internet.\n\n$LAST_LOG"
		return 1
	fi

	if ! run_step "Instalando o Gordeck…" flatpak install --user -y --noninteractive --reinstall --bundle "$BUNDLE"; then
		error "Não foi possível instalar o Gordeck:\n\n$LAST_LOG"
		return 1
	fi

	import_settings

	if is_installed "$LEGACY_APP_ID"; then
		if ask "O antigo Chiaki4deck ainda está instalado.\n\nDesinstalar? (as configurações dele são mantidas)" "Desinstalar" "Manter"; then
			run_step "Desinstalando o Chiaki4deck…" flatpak uninstall -y --noninteractive "$LEGACY_APP_ID" \
				|| error "Não foi possível desinstalar o Chiaki4deck:\n\n$LAST_LOG"
		fi
	fi

	if ask "Gordeck instalado!\n\nAdicionar o Gordeck à biblioteca da Steam, com ícone e artes, para usar no modo Jogo?" "Adicionar" "Agora não"; then
		add_to_steam
	else
		info "Gordeck instalado! Ele está no menu de aplicativos."
	fi
}

uninstall_gordeck() {
	ask "Desinstalar o Gordeck?\n\nAs configurações e os consoles registrados são mantidos." "Desinstalar" "Cancelar" || return 0
	if is_installed "$APP_ID"; then
		if ! run_step "Desinstalando o Gordeck…" flatpak uninstall -y --noninteractive "$APP_ID"; then
			error "Não foi possível desinstalar o Gordeck:\n\n$LAST_LOG"
			return 1
		fi
	fi
	if ask "Remover também o Gordeck da biblioteca da Steam?" "Remover" "Manter"; then
		close_steam || return 1
		run_step "Removendo da biblioteca da Steam…" python3 "$DIR/add-to-steam.py" --remove \
			|| { error "Não foi possível remover da Steam:\n\n$LAST_LOG"; return 1; }
	fi
	info "Gordeck desinstalado."
}

main() {
	local install_label="Instalar o Gordeck"
	is_installed "$APP_ID" && install_label="Atualizar / reinstalar o Gordeck"

	local choice
	choice="$("${ZENITY[@]}" --list --radiolist --height=300 \
		--text="<big><b>Gordeck</b></big>\nRemote Play para PlayStation no Steam Deck.\n\nO que você quer fazer?" \
		--column="" --column="Ação" --hide-header \
		TRUE "$install_label" \
		FALSE "Adicionar à biblioteca da Steam" \
		FALSE "Desinstalar o Gordeck" \
		--ok-label="Continuar" --cancel-label="Sair" 2>/dev/null)" || exit 0

	case "$choice" in
		"$install_label") install_gordeck ;;
		"Adicionar à biblioteca da Steam")
			if ! is_installed "$APP_ID"; then
				error "Instale o Gordeck primeiro."
				exit 1
			fi
			add_to_steam ;;
		"Desinstalar o Gordeck") uninstall_gordeck ;;
	esac
}

main
