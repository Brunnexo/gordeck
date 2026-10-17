// SPDX-License-Identifier: LicenseRef-AGPL-3.0-only-OpenSSL

#ifndef CHIAKI_GAMEPADNAVIGATOR_H
#define CHIAKI_GAMEPADNAVIGATOR_H

#include "controllermanager.h"

#include <QMap>
#include <QObject>
#include <QTimer>

class Controller;
class QWidget;

enum class GamepadButton
{
	Up = (1 << 0),
	Down = (1 << 1),
	Left = (1 << 2),
	Right = (1 << 3),
	A = (1 << 4),
	B = (1 << 5),
	X = (1 << 6),
	Y = (1 << 7),
	Start = (1 << 8),
	Select = (1 << 9),
	L1 = (1 << 10),
	R1 = (1 << 11)
};

/**
 * Lets the user navigate the menu interface with a gamepad.
 *
 * Button presses are emitted as signals while main_window is the active window.
 * While a dialog or popup is open, they are translated to key events instead,
 * so all regular dialogs can be used with the gamepad as well.
 */
class GamepadNavigator : public QObject
{
	Q_OBJECT

	private:
		QWidget *main_window;
		QMap<int, Controller *> controllers;
		QMap<int, uint32_t> controller_buttons;
		bool suspended;

		// the controller that was used last, determines the button layout shown
		int active_controller;
		void SetActiveController(int id);

		QTimer repeat_timer;
		int repeat_controller;
		GamepadButton repeat_button;

		void ReleaseControllers();
		void ControllerStateChanged(Controller *controller);
		void Dispatch(GamepadButton button);
		bool DispatchToPopupOrDialog(GamepadButton button);

	protected:
		bool eventFilter(QObject *watched, QEvent *event) override;

	private slots:
		void UpdateControllers();
		void Repeat();

	public:
		explicit GamepadNavigator(QWidget *main_window);
		~GamepadNavigator() override;

		/**
		 * Release all controllers, e.g. so a stream session can open them.
		 */
		void Suspend();
		void Resume();

		bool HasControllers() const		{ return !controllers.isEmpty(); }
		ControllerLayout GetActiveLayout() const;
		QString GetActiveControllerName() const;

	signals:
		void ButtonPressed(GamepadButton button);
		void ControllersChanged();
};

#endif // CHIAKI_GAMEPADNAVIGATOR_H
