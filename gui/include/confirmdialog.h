// SPDX-License-Identifier: LicenseRef-AGPL-3.0-only-OpenSSL

#ifndef CHIAKI_CONFIRMDIALOG_H
#define CHIAKI_CONFIRMDIALOG_H

#include "hintbar.h"

#include <QDialog>

class QPushButton;

/**
 * Frameless confirmation modal in the style of the main interface,
 * with big buttons that can be used with a gamepad, keyboard or touch.
 */
class ConfirmDialog : public QDialog
{
	Q_OBJECT

	private:
		QPushButton *confirm_button;
		QPushButton *cancel_button;

	protected:
		void paintEvent(QPaintEvent *event) override;
		void showEvent(QShowEvent *event) override;

	public:
		ConfirmDialog(const QString &title, const QString &text,
				const QString &confirm_text, const QString &cancel_text,
				HintStyle style, bool destructive, QWidget *parent = nullptr);

		/**
		 * Show the dialog over a dimmed parent window.
		 * @return true if confirmed
		 */
		static bool Ask(QWidget *parent, HintStyle style, const QString &title, const QString &text,
				const QString &confirm_text, const QString &cancel_text, bool destructive = false);
};

#endif // CHIAKI_CONFIRMDIALOG_H
