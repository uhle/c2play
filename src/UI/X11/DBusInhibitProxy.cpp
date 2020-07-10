/*
*
* Copyright (C) 2020 Thomas Uhle <uhle@users.noreply.github.com>.
* All rights reserved.
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2, as
* published by the Free Software Foundation.
*
* This program is distributed in the hope that it will be useful, but WITHOUT
* ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
* FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
* more details.
*
*/

#include "DBusInhibitProxy.h"


uint32_t DBusInhibitProxy::Inhibit(uint32_t toplevel_xid)
{
	DBus::CallMessage call;
	call.member("Inhibit");

	DBus::MessageIter msg_it = call.writer();

	switch (api)
	{
		case DBusInhibitAPI::XDG:
			msg_it.append_string("c2play");
			msg_it.append_string("playing a video");
			break;

		case DBusInhibitAPI::GSM:
			msg_it.append_string("c2play");
			msg_it.append_uint32(toplevel_xid);
			msg_it.append_string("playing a video");
			msg_it.append_uint32(12 /* INHIBIT_SUSPEND_SESSION | INHIBIT_MARK_SESSION_IDLE */);
			break;

		default:
			return -1;
	}

	DBus::Message reply = invoke_method(call);

	if (reply.is_error())
	{
		return -1;
	}

	msg_it = reply.reader();

	return msg_it.get_uint32();
}

void DBusInhibitProxy::UnInhibit(uint32_t cookie)
{
	if (cookie == (uint32_t)-1)
		return;

	DBus::CallMessage call;

	switch (api)
	{
		case DBusInhibitAPI::XDG:
			call.member("UnInhibit");
			break;

		case DBusInhibitAPI::GSM:
			call.member("Uninhibit");
			break;

		default:
			return;
	}

	DBus::MessageIter msg_it = call.writer();
	msg_it.append_uint32(cookie);

	invoke_method_noreply(call);
}

DBusInhibitProxies DBusInhibitProxy::DiscoverDBus(DBus::Connection& connection)
{
	DBusInhibitProxies proxies;

	if (connection.connected())
	{
		if (connection.has_name("org.freedesktop.PowerManagement"))  // KDE, XFCE
		{
			proxies.emplace_back(std::make_unique<DBusInhibitProxy>(connection,
				"/org/freedesktop/PowerManagement/Inhibit",
				"org.freedesktop.PowerManagement",
				"org.freedesktop.PowerManagement.Inhibit",
				DBusInhibitAPI::XDG));
		}

		if (connection.has_name("org.freedesktop.ScreenSaver"))  // DDE, KDE, light-locker
		{
			DBus::CallMessage call;
			call.interface("org.freedesktop.DBus.Introspectable");
			call.member("Introspect");
			call.destination("org.freedesktop.ScreenSaver");

			// KDE uses a different object path for unknown reason, so we need to check that.
			call.path("/ScreenSaver");

			if (connection.send_blocking(call).is_error())
			{
				// standard path according to freedesktop.org
				call.path("/org/freedesktop/ScreenSaver");
			}

			proxies.emplace_back(std::make_unique<DBusInhibitProxy>(connection,
				call.path(),
				"org.freedesktop.ScreenSaver",
				"org.freedesktop.ScreenSaver",
				DBusInhibitAPI::XDG));
		}
		else if (connection.has_name("org.xfce.ScreenSaver"))  // XFCE
		{
			proxies.emplace_back(std::make_unique<DBusInhibitProxy>(connection,
				"/org/xfce/ScreenSaver",
				"org.xfce.ScreenSaver",
				"org.xfce.ScreenSaver",
				DBusInhibitAPI::XDG));
		}
		else if (connection.has_name("org.mate.ScreenSaver"))  // MATE
		{
			proxies.emplace_back(std::make_unique<DBusInhibitProxy>(connection,
				"/org/mate/ScreenSaver",
				"org.mate.ScreenSaver",
				"org.mate.ScreenSaver",
				DBusInhibitAPI::XDG));
		}
		else if (connection.has_name("org.gnome.ScreenSaver"))  // older GNOME
		{
			proxies.emplace_back(std::make_unique<DBusInhibitProxy>(connection,
				"/org/gnome/ScreenSaver",
				"org.gnome.ScreenSaver",
				"org.gnome.ScreenSaver",
				DBusInhibitAPI::XDG));
		}

		if (connection.has_name("org.gnome.SessionManager"))  // GNOME, Budgie, Pantheon, UKUI, Unity, newer MATE, ...
		{
			proxies.emplace_back(std::make_unique<DBusInhibitProxy>(connection,
				"/org/gnome/SessionManager",
				"org.gnome.SessionManager",
				"org.gnome.SessionManager",
				DBusInhibitAPI::GSM));
		}
		else if (connection.has_name("org.mate.SessionManager"))  // older MATE
		{
			proxies.emplace_back(std::make_unique<DBusInhibitProxy>(connection,
				"/org/mate/SessionManager",
				"org.mate.SessionManager",
				"org.mate.SessionManager",
				DBusInhibitAPI::GSM));
		}
	}

	return proxies;
}
