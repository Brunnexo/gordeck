// SPDX-License-Identifier: LicenseRef-AGPL-3.0-only-OpenSSL

#include <controllermanager.h>

#include <QCoreApplication>
#include <moderndialog.h>
#include <QByteArray>
#include <QFile>
#include <QTimer>

#ifdef CHIAKI_GUI_ENABLE_SDL_GAMECONTROLLER
#include <SDL.h>
#endif

static QSet<QString> chiaki_motion_controller_guids({
	// Sony on Linux
	"03000000341a00003608000011010000",
	"030000004c0500006802000010010000",
	"030000004c0500006802000010810000",
	"030000004c0500006802000011010000",
	"030000004c0500006802000011810000",
	"030000006f0e00001402000011010000",
	"030000008f0e00000300000010010000",
	"050000004c0500006802000000010000",
	"050000004c0500006802000000800000",
	"050000004c0500006802000000810000",
	"05000000504c415953544154494f4e00",
	"060000004c0500006802000000010000",
	"030000004c050000a00b000011010000",
	"030000004c050000a00b000011810000",
	"030000004c050000c405000011010000",
	"030000004c050000c405000011810000",
	"030000004c050000cc09000000010000",
	"030000004c050000cc09000011010000",
	"030000004c050000cc09000011810000",
	"03000000c01100000140000011010000",
	"050000004c050000c405000000010000",
	"050000004c050000c405000000810000",
	"050000004c050000c405000001800000",
	"050000004c050000cc09000000010000",
	"050000004c050000cc09000000810000",
	"050000004c050000cc09000001800000",
	// Sony on iOS
	"050000004c050000cc090000df070000",
	// Sony on Android
	"050000004c05000068020000dfff3f00",
	"030000004c050000cc09000000006800",
	"050000004c050000c4050000fffe3f00",
	"050000004c050000cc090000fffe3f00",
	"050000004c050000cc090000ffff3f00",
	"35643031303033326130316330353564",
	// Sony on Mac OSx
	"030000004c050000cc09000000000000",
	"030000004c0500006802000000000000",
	"030000004c0500006802000000010000",
	"030000004c050000a00b000000010000",
	"030000004c050000c405000000000000",
	"030000004c050000c405000000010000",
	"030000004c050000cc09000000010000",
	"03000000c01100000140000000010000",
	// Sony on Windows
	"030000004c050000a00b000000000000",
	"030000004c050000c405000000000000",
	"030000004c050000cc09000000000000",
	"03000000250900000500000000000000",
	"030000004c0500006802000000000000",
	"03000000632500007505000000000000",
	"03000000888800000803000000000000",
	"030000008f0e00001431000000000000",
});

static ControllerManager *instance = nullptr;

#define UPDATE_INTERVAL_MS 4

ControllerManager *ControllerManager::GetInstance()
{
	if(!instance)
		instance = new ControllerManager(qApp);
	return instance;
}

ControllerManager::ControllerManager(QObject *parent)
	: QObject(parent)
{
#ifdef CHIAKI_GUI_ENABLE_SDL_GAMECONTROLLER
	SDL_SetMainReady();
#ifdef SDL_HINT_JOYSTICK_HIDAPI_STEAMDECK
	// Never let SDL open the Steam Deck controller's raw HID node: the hid-steam kernel driver
	// removes its evdev gamepad (the one we actually use) as soon as any process opens hidraw.
	// Motion comes from the kernel's "Steam Deck Motion Sensors" evdev device through setsu.
	SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_STEAMDECK, "0");
#endif
	if(SDL_Init(SDL_INIT_GAMECONTROLLER) < 0)
	{
		const char *err = SDL_GetError();
		ModernDialog::Message(nullptr, ModernDialog::Icon::Error, tr("Controllers unavailable"),
				tr("Failed to initialized SDL Gamecontroller: %1").arg(err ? err : ""));
	}

	auto timer = new QTimer(this);
	connect(timer, &QTimer::timeout, this, &ControllerManager::HandleEvents);
	timer->start(UPDATE_INTERVAL_MS);
#endif

	UpdateAvailableControllers();
}

ControllerManager::~ControllerManager()
{
#ifdef CHIAKI_GUI_ENABLE_SDL_GAMECONTROLLER
	SDL_Quit();
#endif
}

void ControllerManager::UpdateAvailableControllers()
{
#ifdef CHIAKI_GUI_ENABLE_SDL_GAMECONTROLLER
	QSet<SDL_JoystickID> current_controllers;
	for(int i=0; i<SDL_NumJoysticks(); i++)
	{
		if(!SDL_IsGameController(i))
			continue;

		// We'll try to identify pads with Motion Control
		SDL_JoystickGUID guid = SDL_JoystickGetGUID(SDL_JoystickOpen(i));
		char guid_str[256];
		SDL_JoystickGetGUIDString(guid, guid_str, sizeof(guid_str));

		if(chiaki_motion_controller_guids.contains(guid_str))
		{
			SDL_Joystick *joy = SDL_JoystickOpen(i);
			if(joy)
			{
				bool no_buttons = SDL_JoystickNumButtons(joy) == 0;
				SDL_JoystickClose(joy);
				if(no_buttons)
					continue;
			}
		}

		current_controllers.insert(SDL_JoystickGetDeviceInstanceID(i));
	}

	if(current_controllers != available_controllers)
	{
		available_controllers = current_controllers;
		emit AvailableControllersUpdated();
	}
#endif
}

void ControllerManager::HandleEvents()
{
#ifdef CHIAKI_GUI_ENABLE_SDL_GAMECONTROLLER
	SDL_Event event;
	while(SDL_PollEvent(&event))
	{
		switch(event.type)
		{
			case SDL_JOYDEVICEADDED:
			case SDL_JOYDEVICEREMOVED:
				UpdateAvailableControllers();
				break;
			case SDL_CONTROLLERBUTTONDOWN:
			{
				uint32_t chiaki_button = 0;
				switch(event.cbutton.button)
				{
					case SDL_CONTROLLER_BUTTON_A: chiaki_button = CHIAKI_CONTROLLER_BUTTON_CROSS; break;
					case SDL_CONTROLLER_BUTTON_B: chiaki_button = CHIAKI_CONTROLLER_BUTTON_MOON; break;
					case SDL_CONTROLLER_BUTTON_X: chiaki_button = CHIAKI_CONTROLLER_BUTTON_BOX; break;
					case SDL_CONTROLLER_BUTTON_Y: chiaki_button = CHIAKI_CONTROLLER_BUTTON_PYRAMID; break;
					case SDL_CONTROLLER_BUTTON_DPAD_LEFT: chiaki_button = CHIAKI_CONTROLLER_BUTTON_DPAD_LEFT; break;
					case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: chiaki_button = CHIAKI_CONTROLLER_BUTTON_DPAD_RIGHT; break;
					case SDL_CONTROLLER_BUTTON_DPAD_UP: chiaki_button = CHIAKI_CONTROLLER_BUTTON_DPAD_UP; break;
					case SDL_CONTROLLER_BUTTON_DPAD_DOWN: chiaki_button = CHIAKI_CONTROLLER_BUTTON_DPAD_DOWN; break;
					case SDL_CONTROLLER_BUTTON_LEFTSHOULDER: chiaki_button = CHIAKI_CONTROLLER_BUTTON_L1; break;
					case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER: chiaki_button = CHIAKI_CONTROLLER_BUTTON_R1; break;
					case SDL_CONTROLLER_BUTTON_START: chiaki_button = CHIAKI_CONTROLLER_BUTTON_OPTIONS; break;
					case SDL_CONTROLLER_BUTTON_BACK: chiaki_button = CHIAKI_CONTROLLER_BUTTON_SHARE; break;
					default: break;
				}
				if(chiaki_button)
					emit ButtonPressed(event.cbutton.which, chiaki_button);
				if(open_controllers.contains(event.cbutton.which))
					emit open_controllers[event.cbutton.which]->SdlButtonPressed(event.cbutton.button);
				ControllerEvent(event.cbutton.which);
				break;
			}
			case SDL_CONTROLLERBUTTONUP:
				ControllerEvent(event.cbutton.which);
				break;
			case SDL_CONTROLLERAXISMOTION:
				ControllerEvent(event.caxis.which);
				break;
#if SDL_VERSION_ATLEAST(2, 0, 14)
			case SDL_CONTROLLERSENSORUPDATE:
				if(open_controllers.contains(event.csensor.which))
					open_controllers[event.csensor.which]->HandleSensorEvent(event.csensor);
				break;
#endif
		}
	}
#endif
}

void ControllerManager::ControllerEvent(int device_id)
{
	if(!open_controllers.contains(device_id))
		return;
	open_controllers[device_id]->UpdateState();
}

QSet<int> ControllerManager::GetAvailableControllers()
{
#ifdef CHIAKI_GUI_ENABLE_SDL_GAMECONTROLLER
	return available_controllers;
#else
	return {};
#endif
}

Controller *ControllerManager::OpenController(int device_id)
{
	if(open_controllers.contains(device_id))
		return nullptr;
	auto controller = new Controller(device_id, this);
	open_controllers[device_id] = controller;
	return controller;
}

void ControllerManager::ControllerClosed(Controller *controller)
{
	open_controllers.remove(controller->GetDeviceID());
}

Controller::Controller(int device_id, ControllerManager *manager) : QObject(manager)
{
	this->id = device_id;
	this->manager = manager;
	chiaki_orientation_tracker_init(&orient_tracker);
	has_motion = false;

#ifdef CHIAKI_GUI_ENABLE_SDL_GAMECONTROLLER
	controller = nullptr;
	for(int i=0; i<SDL_NumJoysticks(); i++)
	{
		if(SDL_JoystickGetDeviceInstanceID(i) == device_id)
		{
			controller = SDL_GameControllerOpen(i);
			break;
		}
	}
#if SDL_VERSION_ATLEAST(2, 0, 14)
	if(controller)
	{
		if(SDL_GameControllerHasSensor(controller, SDL_SENSOR_ACCEL))
			SDL_GameControllerSetSensorEnabled(controller, SDL_SENSOR_ACCEL, SDL_TRUE);
		if(SDL_GameControllerHasSensor(controller, SDL_SENSOR_GYRO))
			SDL_GameControllerSetSensorEnabled(controller, SDL_SENSOR_GYRO, SDL_TRUE);
	}
#endif
#endif
}

Controller::~Controller()
{
#ifdef CHIAKI_GUI_ENABLE_SDL_GAMECONTROLLER
	if(controller)
		SDL_GameControllerClose(controller);
#endif
	manager->ControllerClosed(this);
}

void Controller::UpdateState()
{
	emit StateChanged();
}

#ifdef CHIAKI_GUI_ENABLE_SDL_GAMECONTROLLER
#if SDL_VERSION_ATLEAST(2, 0, 14)
void Controller::HandleSensorEvent(const SDL_ControllerSensorEvent &event)
{
	// SDL gives the accelerometer in m/s^2 and the gyroscope in rad/s, both already in the
	// axis convention the PlayStation expects. The tracker keeps the last value of each.
	uint32_t timestamp_us = event.timestamp * 1000;
	switch(event.sensor)
	{
		case SDL_SENSOR_ACCEL:
			chiaki_orientation_tracker_update(&orient_tracker,
					orient_tracker.gyro_x, orient_tracker.gyro_y, orient_tracker.gyro_z,
					event.data[0] / SDL_STANDARD_GRAVITY, event.data[1] / SDL_STANDARD_GRAVITY, event.data[2] / SDL_STANDARD_GRAVITY,
					timestamp_us);
			has_motion = true;
			break;
		case SDL_SENSOR_GYRO:
			chiaki_orientation_tracker_update(&orient_tracker,
					event.data[0], event.data[1], event.data[2],
					orient_tracker.accel_x, orient_tracker.accel_y, orient_tracker.accel_z,
					timestamp_us);
			has_motion = true;
			UpdateState(); // one feedback per gyro sample, accel samples ride along
			break;
		default:
			break;
	}
}
#endif
#endif

bool Controller::IsConnected()
{
#ifdef CHIAKI_GUI_ENABLE_SDL_GAMECONTROLLER
	return controller && SDL_GameControllerGetAttached(controller);
#else
	return false;
#endif
}

int Controller::GetDeviceID()
{
#ifdef CHIAKI_GUI_ENABLE_SDL_GAMECONTROLLER
	return id;
#else
	return -1;
#endif
}

QString Controller::GetName()
{
#ifdef CHIAKI_GUI_ENABLE_SDL_GAMECONTROLLER
	if(!controller)
		return QString();
	SDL_Joystick *js = SDL_GameControllerGetJoystick(controller);
	SDL_JoystickGUID guid = SDL_JoystickGetGUID(js);
	char guid_str[256];
	SDL_JoystickGetGUIDString(guid, guid_str, sizeof(guid_str));
	return QString("%1 (%2)").arg(SDL_JoystickName(js), guid_str);
#else
	return QString();
#endif
}

QString Controller::GetDisplayName()
{
#ifdef CHIAKI_GUI_ENABLE_SDL_GAMECONTROLLER
	if(!controller)
		return QString();
	return QString::fromUtf8(SDL_GameControllerName(controller));
#else
	return QString();
#endif
}

#define VALVE_VENDOR_ID 0x28de
#define STEAM_DECK_PRODUCT_ID 0x1205
#define STEAM_VIRTUAL_GAMEPAD_PRODUCT_ID 0x11ff

static bool RunningOnSteamDeck()
{
	static int cached = -1;
	if(cached < 0)
	{
		auto ReadFile = [](const char *path) {
			QFile file(path);
			if(!file.open(QIODevice::ReadOnly))
				return QString();
			return QString::fromUtf8(file.readAll()).trimmed();
		};
		QString vendor = ReadFile("/sys/devices/virtual/dmi/id/board_vendor");
		QString product = ReadFile("/sys/devices/virtual/dmi/id/product_name");
		// Jupiter is the LCD model, Galileo the OLED model
		cached = vendor == "Valve" && (product == "Jupiter" || product == "Galileo") ? 1 : 0;
	}
	return cached == 1;
}

ControllerLayout Controller::GetLayout()
{
#ifdef CHIAKI_GUI_ENABLE_SDL_GAMECONTROLLER
	if(!controller)
		return ControllerLayout::Xbox;

	switch(SDL_GameControllerGetType(controller))
	{
		case SDL_CONTROLLER_TYPE_PS3:
		case SDL_CONTROLLER_TYPE_PS4:
		case SDL_CONTROLLER_TYPE_PS5:
			return ControllerLayout::PlayStation;
		default:
			break;
	}

	if(GetDisplayName().contains("Steam Deck", Qt::CaseInsensitive))
		return ControllerLayout::SteamDeck;

	// With Steam Input, the built-in controls appear as Steam's virtual gamepad
	if(SDL_GameControllerGetVendor(controller) == VALVE_VENDOR_ID && RunningOnSteamDeck())
	{
		uint16_t product = SDL_GameControllerGetProduct(controller);
		if(product == STEAM_DECK_PRODUCT_ID || product == STEAM_VIRTUAL_GAMEPAD_PRODUCT_ID)
			return ControllerLayout::SteamDeck;
	}
#endif
	return ControllerLayout::Xbox;
}

ChiakiControllerState Controller::GetState()
{
	ChiakiControllerState state;
	chiaki_controller_state_set_idle(&state);
#ifdef CHIAKI_GUI_ENABLE_SDL_GAMECONTROLLER
	if(!controller)
		return state;

	state.buttons |= SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_A) ? CHIAKI_CONTROLLER_BUTTON_CROSS : 0;
	state.buttons |= SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_B) ? CHIAKI_CONTROLLER_BUTTON_MOON : 0;
	state.buttons |= SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_X) ? CHIAKI_CONTROLLER_BUTTON_BOX : 0;
	state.buttons |= SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_Y) ? CHIAKI_CONTROLLER_BUTTON_PYRAMID : 0;
	state.buttons |= SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_DPAD_LEFT) ? CHIAKI_CONTROLLER_BUTTON_DPAD_LEFT : 0;
	state.buttons |= SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_DPAD_RIGHT) ? CHIAKI_CONTROLLER_BUTTON_DPAD_RIGHT : 0;
	state.buttons |= SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_DPAD_UP) ? CHIAKI_CONTROLLER_BUTTON_DPAD_UP : 0;
	state.buttons |= SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_DPAD_DOWN) ? CHIAKI_CONTROLLER_BUTTON_DPAD_DOWN : 0;
	state.buttons |= SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_LEFTSHOULDER) ? CHIAKI_CONTROLLER_BUTTON_L1 : 0;
	state.buttons |= SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER) ? CHIAKI_CONTROLLER_BUTTON_R1 : 0;
	state.buttons |= SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_LEFTSTICK) ? CHIAKI_CONTROLLER_BUTTON_L3 : 0;
	state.buttons |= SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_RIGHTSTICK) ? CHIAKI_CONTROLLER_BUTTON_R3 : 0;
	state.buttons |= SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_START) ? CHIAKI_CONTROLLER_BUTTON_OPTIONS : 0;
	state.buttons |= SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_BACK) ? CHIAKI_CONTROLLER_BUTTON_SHARE : 0;
	state.buttons |= SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_GUIDE) ? CHIAKI_CONTROLLER_BUTTON_PS : 0;
	state.l2_state = (uint8_t)(SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_TRIGGERLEFT) >> 7);
	state.r2_state = (uint8_t)(SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_TRIGGERRIGHT) >> 7);
	state.left_x = SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_LEFTX);
	state.left_y = SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_LEFTY);
	state.right_x = SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_RIGHTX);
	state.right_y = SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_RIGHTY);

	if(has_motion)
		chiaki_orientation_tracker_apply_to_controller_state(&orient_tracker, &state);
#endif
	return state;
}

bool Controller::IsSdlButtonPressed(int sdl_button)
{
#ifdef CHIAKI_GUI_ENABLE_SDL_GAMECONTROLLER
	if(!controller)
		return false;
	return SDL_GameControllerGetButton(controller, (SDL_GameControllerButton)sdl_button) != 0;
#else
	return false;
#endif
}

void Controller::SetRumble(uint8_t left, uint8_t right)
{
	SetRumble16((uint16_t)left << 8, (uint16_t)right << 8);
}

void Controller::SetRumble16(uint16_t left, uint16_t right)
{
#ifdef CHIAKI_GUI_ENABLE_SDL_GAMECONTROLLER
	if(!controller)
		return;
	SDL_GameControllerRumble(controller, left, right, 5000);
#endif
}
