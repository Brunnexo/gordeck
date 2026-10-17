// SPDX-License-Identifier: LicenseRef-AGPL-3.0-only-OpenSSL

#include <gamepadnavigator.h>
#include <controllermanager.h>

#include <QAbstractButton>
#include <QApplication>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QKeyEvent>
#include <QLineEdit>
#include <QMessageBox>
#include <QWidget>

#include <algorithm>

#define STICK_THRESHOLD 20000
#define REPEAT_DELAY_MS 380
#define REPEAT_INTERVAL_MS 110

static const uint32_t direction_mask = (uint32_t)GamepadButton::Up | (uint32_t)GamepadButton::Down
	| (uint32_t)GamepadButton::Left | (uint32_t)GamepadButton::Right;

GamepadNavigator::GamepadNavigator(QWidget *main_window)
	: QObject(main_window),
	main_window(main_window),
	suspended(false),
	active_controller(-1),
	repeat_controller(-1),
	repeat_button(GamepadButton::Up)
{
	repeat_timer.setSingleShot(false);
	connect(&repeat_timer, &QTimer::timeout, this, &GamepadNavigator::Repeat);

	connect(ControllerManager::GetInstance(), &ControllerManager::AvailableControllersUpdated,
			this, &GamepadNavigator::UpdateControllers);
	UpdateControllers();

	// left/right moves between the buttons of dialogs, also for keyboard input
	// (e.g. Steam Input's desktop configuration maps the d-pad to the arrow keys)
	qApp->installEventFilter(this);
}

bool GamepadNavigator::eventFilter(QObject *watched, QEvent *event)
{
	if(event->type() != QEvent::KeyPress)
		return QObject::eventFilter(watched, event);
	auto key_event = static_cast<QKeyEvent *>(event);
	int key = key_event->key();
	if(key != Qt::Key_Left && key != Qt::Key_Right)
		return QObject::eventFilter(watched, event);

	auto button = qobject_cast<QAbstractButton *>(watched);
	if(!button)
		return QObject::eventFilter(watched, event);
	auto box = qobject_cast<QDialogButtonBox *>(button->parentWidget());
	if(!box)
		return QObject::eventFilter(watched, event);

	QList<QAbstractButton *> buttons;
	for(auto b : box->buttons())
	{
		if(b->isVisible() && b->isEnabled())
			buttons.append(b);
	}
	std::sort(buttons.begin(), buttons.end(), [](QAbstractButton *a, QAbstractButton *b) {
		return a->x() < b->x();
	});
	int index = buttons.indexOf(button);
	if(index < 0)
		return QObject::eventFilter(watched, event);
	index += key == Qt::Key_Left ? -1 : 1;
	if(index >= 0 && index < buttons.count())
		buttons[index]->setFocus(key == Qt::Key_Left ? Qt::BacktabFocusReason : Qt::TabFocusReason);
	return true;
}

GamepadNavigator::~GamepadNavigator()
{
	ReleaseControllers();
}

void GamepadNavigator::ReleaseControllers()
{
	repeat_timer.stop();
	active_controller = -1;
	auto open = controllers.values();
	controllers.clear();
	controller_buttons.clear();
	for(auto controller : open)
		delete controller;
}

void GamepadNavigator::Suspend()
{
	suspended = true;
	ReleaseControllers();
	emit ControllersChanged();
}

void GamepadNavigator::Resume()
{
	suspended = false;
	UpdateControllers();
}

void GamepadNavigator::UpdateControllers()
{
	if(suspended)
		return;

	auto available = ControllerManager::GetInstance()->GetAvailableControllers();
	bool changed = false;

	for(auto it = controllers.begin(); it != controllers.end();)
	{
		if(!available.contains(it.key()) || !it.value()->IsConnected())
		{
			controller_buttons.remove(it.key());
			delete it.value();
			it = controllers.erase(it);
			changed = true;
		}
		else
			++it;
	}

	for(int id : available)
	{
		if(controllers.contains(id))
			continue;
		auto controller = ControllerManager::GetInstance()->OpenController(id);
		if(!controller)
			continue;
		controllers[id] = controller;
		controller_buttons[id] = 0;
		connect(controller, &Controller::StateChanged, this, [this, controller]() {
			ControllerStateChanged(controller);
		});
		changed = true;
	}

	if(!controllers.contains(active_controller))
	{
		active_controller = controllers.isEmpty() ? -1 : controllers.firstKey();
		changed = true;
	}

	if(changed)
		emit ControllersChanged();
}

void GamepadNavigator::SetActiveController(int id)
{
	if(id == active_controller)
		return;
	ControllerLayout prev_layout = GetActiveLayout();
	QString prev_name = GetActiveControllerName();
	active_controller = id;
	if(prev_layout != GetActiveLayout() || prev_name != GetActiveControllerName())
		emit ControllersChanged();
}

ControllerLayout GamepadNavigator::GetActiveLayout() const
{
	auto controller = controllers.value(active_controller, nullptr);
	return controller ? controller->GetLayout() : ControllerLayout::Xbox;
}

QString GamepadNavigator::GetActiveControllerName() const
{
	auto controller = controllers.value(active_controller, nullptr);
	return controller ? controller->GetDisplayName() : QString();
}

void GamepadNavigator::ControllerStateChanged(Controller *controller)
{
	if(suspended)
		return;

	int id = controller->GetDeviceID();
	ChiakiControllerState state = controller->GetState();

	uint32_t buttons = 0;
	auto Map = [&](uint32_t chiaki_button, GamepadButton button) {
		if(state.buttons & chiaki_button)
			buttons |= (uint32_t)button;
	};
	Map(CHIAKI_CONTROLLER_BUTTON_DPAD_UP, GamepadButton::Up);
	Map(CHIAKI_CONTROLLER_BUTTON_DPAD_DOWN, GamepadButton::Down);
	Map(CHIAKI_CONTROLLER_BUTTON_DPAD_LEFT, GamepadButton::Left);
	Map(CHIAKI_CONTROLLER_BUTTON_DPAD_RIGHT, GamepadButton::Right);
	Map(CHIAKI_CONTROLLER_BUTTON_CROSS, GamepadButton::A);
	Map(CHIAKI_CONTROLLER_BUTTON_MOON, GamepadButton::B);
	Map(CHIAKI_CONTROLLER_BUTTON_BOX, GamepadButton::X);
	Map(CHIAKI_CONTROLLER_BUTTON_PYRAMID, GamepadButton::Y);
	Map(CHIAKI_CONTROLLER_BUTTON_OPTIONS, GamepadButton::Start);
	Map(CHIAKI_CONTROLLER_BUTTON_SHARE, GamepadButton::Select);
	Map(CHIAKI_CONTROLLER_BUTTON_L1, GamepadButton::L1);
	Map(CHIAKI_CONTROLLER_BUTTON_R1, GamepadButton::R1);

	// left stick acts like the d-pad
	if(state.left_x < -STICK_THRESHOLD)
		buttons |= (uint32_t)GamepadButton::Left;
	else if(state.left_x > STICK_THRESHOLD)
		buttons |= (uint32_t)GamepadButton::Right;
	if(state.left_y < -STICK_THRESHOLD)
		buttons |= (uint32_t)GamepadButton::Up;
	else if(state.left_y > STICK_THRESHOLD)
		buttons |= (uint32_t)GamepadButton::Down;

	uint32_t prev = controller_buttons.value(id, 0);
	controller_buttons[id] = buttons;
	uint32_t newly_pressed = buttons & ~prev;
	if(newly_pressed)
		SetActiveController(id);

	// stop repeating when the held direction was released
	if(repeat_timer.isActive() && repeat_controller == id && !(buttons & (uint32_t)repeat_button))
		repeat_timer.stop();

	for(uint32_t bit = 1; bit <= (uint32_t)GamepadButton::R1; bit <<= 1)
	{
		if(!(newly_pressed & bit))
			continue;
		auto button = (GamepadButton)bit;
		if(bit & direction_mask)
		{
			repeat_controller = id;
			repeat_button = button;
			repeat_timer.start(REPEAT_DELAY_MS);
		}
		// Dispatch from the event loop instead of from within the SDL polling timer:
		// a button may open a modal dialog with its own event loop, and Qt never fires
		// a timer again while its handler is still running, which would block all
		// gamepad input inside the dialog.
		QMetaObject::invokeMethod(this, [this, button]() {
			Dispatch(button);
		}, Qt::QueuedConnection);
	}
}

void GamepadNavigator::Repeat()
{
	if(suspended || !(controller_buttons.value(repeat_controller, 0) & (uint32_t)repeat_button))
	{
		repeat_timer.stop();
		return;
	}
	repeat_timer.setInterval(REPEAT_INTERVAL_MS);
	Dispatch(repeat_button);
}

static void SendKey(QWidget *target, int key, Qt::KeyboardModifiers modifiers = Qt::NoModifier)
{
	if(!target)
		return;
	QKeyEvent press(QEvent::KeyPress, key, modifiers);
	QCoreApplication::sendEvent(target, &press);
	QKeyEvent release(QEvent::KeyRelease, key, modifiers);
	QCoreApplication::sendEvent(target, &release);
}

bool GamepadNavigator::DispatchToPopupOrDialog(GamepadButton button)
{
	QWidget *popup = QApplication::activePopupWidget();
	if(popup)
	{
		// menus and combo box popups
		QWidget *target = QApplication::focusWidget();
		if(!target || (target != popup && !popup->isAncestorOf(target)))
			target = popup;
		switch(button)
		{
			case GamepadButton::Up: SendKey(target, Qt::Key_Up); break;
			case GamepadButton::Down: SendKey(target, Qt::Key_Down); break;
			case GamepadButton::Left: SendKey(target, Qt::Key_Left); break;
			case GamepadButton::Right: SendKey(target, Qt::Key_Right); break;
			case GamepadButton::A: SendKey(target, Qt::Key_Return); break;
			case GamepadButton::B: SendKey(target, Qt::Key_Escape); break;
			default: break;
		}
		return true;
	}

	QWidget *modal = QApplication::activeModalWidget();
	if(!modal)
		return false;

	QWidget *focus = QApplication::focusWidget();
	if(focus && focus != modal && !modal->isAncestorOf(focus))
		focus = nullptr;
	// the dialog might not be the active window (e.g. if the compositor did not focus it),
	// so fall back to the widget that has focus within the dialog
	if(!focus)
		focus = modal->focusWidget();
	if(!modal->isActiveWindow())
		modal->activateWindow();

	switch(button)
	{
		case GamepadButton::Up:
		case GamepadButton::L1:
			SendKey(modal, Qt::Key_Backtab, Qt::ShiftModifier);
			break;
		case GamepadButton::Down:
		case GamepadButton::R1:
			SendKey(modal, Qt::Key_Tab);
			break;
		case GamepadButton::Left:
			SendKey(focus, Qt::Key_Left);
			break;
		case GamepadButton::Right:
			SendKey(focus, Qt::Key_Right);
			break;
		case GamepadButton::A:
			if(auto b = qobject_cast<QAbstractButton *>(focus))
			{
				if(b->isEnabled())
					b->animateClick();
			}
			else if(auto combo = qobject_cast<QComboBox *>(focus))
				combo->showPopup();
			else if(focus && !qobject_cast<QLineEdit *>(focus))
				SendKey(focus, Qt::Key_Return);
			break;
		case GamepadButton::B:
			if(qobject_cast<QMessageBox *>(modal))
				SendKey(modal, Qt::Key_Escape); // triggers the message box's escape button
			else if(auto dialog = qobject_cast<QDialog *>(modal))
				dialog->reject();
			break;
		default:
			break;
	}
	return true;
}

void GamepadNavigator::Dispatch(GamepadButton button)
{
	if(DispatchToPopupOrDialog(button))
		return;
	if(!main_window->isActiveWindow())
		return;
	emit ButtonPressed(button);
}
