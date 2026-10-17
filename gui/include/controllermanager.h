// SPDX-License-Identifier: LicenseRef-AGPL-3.0-only-OpenSSL

#ifndef CHIAKI_CONTROLLERMANAGER_H
#define CHIAKI_CONTROLLERMANAGER_H

#include <chiaki/controller.h>

#include <QObject>
#include <QSet>
#include <QMap>
#include <QString>

#ifdef CHIAKI_GUI_ENABLE_SDL_GAMECONTROLLER
#include <SDL.h>
#endif

class Controller;

enum class ControllerLayout
{
	Xbox,
	SteamDeck,
	PlayStation
};

class ControllerManager : public QObject
{
	Q_OBJECT

	friend class Controller;

	private:
#ifdef CHIAKI_GUI_ENABLE_SDL_GAMECONTROLLER
		QSet<SDL_JoystickID> available_controllers;
#endif
		QMap<int, Controller *> open_controllers;

		void ControllerClosed(Controller *controller);

	private slots:
		void UpdateAvailableControllers();
		void HandleEvents();
		void ControllerEvent(int device_id);

	public:
		static ControllerManager *GetInstance();

		ControllerManager(QObject *parent = nullptr);
		~ControllerManager();

		QSet<int> GetAvailableControllers();
		Controller *OpenController(int device_id);

	signals:
		void AvailableControllersUpdated();

		/**
		 * Emitted for every button press of any opened controller, no matter who opened it
		 * @param chiaki_button ChiakiControllerButton
		 */
		void ButtonPressed(int device_id, uint32_t chiaki_button);
};

class Controller : public QObject
{
	Q_OBJECT

	friend class ControllerManager;

	private:
		Controller(int device_id, ControllerManager *manager);

		void UpdateState();

		ControllerManager *manager;
		int id;

#ifdef CHIAKI_GUI_ENABLE_SDL_GAMECONTROLLER
		SDL_GameController *controller;
#endif

	public:
		~Controller();

		bool IsConnected();
		int GetDeviceID();
		QString GetName();

		/**
		 * @return controller name without the GUID, for display
		 */
		QString GetDisplayName();
		ControllerLayout GetLayout();
		ChiakiControllerState GetState();
		void SetRumble(uint8_t left, uint8_t right);

	signals:
		void StateChanged();
};

#endif // CHIAKI_CONTROLLERMANAGER_H
