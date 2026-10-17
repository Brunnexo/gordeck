// SPDX-License-Identifier: LicenseRef-AGPL-3.0-only-OpenSSL

#ifndef CHIAKI_MODERNWIDGETS_H
#define CHIAKI_MODERNWIDGETS_H

#include <QAbstractButton>

class QFrame;

/**
 * On/off switch, toggled with A/Space/click or set with left/right
 */
class ToggleSwitch : public QAbstractButton
{
	Q_OBJECT

	protected:
		void paintEvent(QPaintEvent *event) override;
		void keyPressEvent(QKeyEvent *event) override;

	public:
		explicit ToggleSwitch(QWidget *parent = nullptr);
		QSize sizeHint() const override;
};

namespace ModernWidgets
{
	/**
	 * Card with a title, an optional description and a control on the right.
	 * The card is highlighted while its control has focus.
	 */
	QFrame *CreateRow(QWidget *parent, const QString &title, const QString &description,
			QWidget *control, int control_width = 340);

	/**
	 * Mark a container whose buttons are navigated in two dimensions with the arrow keys / d-pad
	 */
	void SetGridNavigation(QWidget *container);
	bool HasGridNavigation(QWidget *widget);
}

#endif // CHIAKI_MODERNWIDGETS_H
