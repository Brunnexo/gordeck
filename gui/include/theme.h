// SPDX-License-Identifier: LicenseRef-AGPL-3.0-only-OpenSSL

#ifndef CHIAKI_THEME_H
#define CHIAKI_THEME_H

#include <QColor>
#include <QPixmap>

class QApplication;

namespace Theme
{
	const QColor background(0x07, 0x0c, 0x18);
	const QColor background_light(0x12, 0x22, 0x44);
	const QColor surface(0x15, 0x1d, 0x30);
	const QColor surface_light(0x1f, 0x2a, 0x44);
	const QColor accent(0x2f, 0x8c, 0xff);
	const QColor accent_light(0x6c, 0xb4, 0xff);
	const QColor text(0xec, 0xf1, 0xfa);
	const QColor text_muted(0x8e, 0x9b, 0xb4);
	const QColor state_ready(0x3d, 0xd6, 0x8c);
	const QColor state_standby(0xff, 0xae, 0x2f);
	const QColor state_unknown(0x7a, 0x86, 0x9c);
	const QColor danger(0xff, 0x5c, 0x6c);

	// colors of the face buttons, Xbox layout like on the Steam Deck
	const QColor button_a(0x3d, 0xd6, 0x8c);
	const QColor button_b(0xff, 0x5c, 0x6c);
	const QColor button_x(0x4d, 0xa3, 0xff);
	const QColor button_y(0xff, 0xc8, 0x3d);

	/**
	 * Apply the dark style and stylesheet to the whole application,
	 * including all dialogs.
	 */
	void Apply(QApplication &app);

	/**
	 * Render an svg icon, optionally tinted with color (if valid)
	 */
	QPixmap RenderIcon(const QString &filename, int size, const QColor &color, qreal dpr);
}

#endif // CHIAKI_THEME_H
