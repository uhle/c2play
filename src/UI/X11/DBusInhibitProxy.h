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

#pragma once

#include <dbus-c++/dbus.h>

#include <memory>
#include <vector>


enum class DBusInhibitAPI
{
	Unkown = 0,
	XDG,		/* freedesktop.org style */
	GSM		/* GNOME session manager */
};


class DBusInhibitProxy;

typedef std::unique_ptr<DBusInhibitProxy> DBusInhibitProxyUPTR;
typedef std::vector<DBusInhibitProxyUPTR> DBusInhibitProxies;


class DBusInhibitProxy : public DBus::ObjectProxy, public DBus::InterfaceProxy
{
	DBusInhibitAPI api;


public:

	DBusInhibitProxy(DBus::Connection& connection, const DBus::Path& path, const char* cname, const char* interface, DBusInhibitAPI api)
		: DBus::ObjectProxy(connection, path, cname), DBus::InterfaceProxy(interface), api(api)
	{
	}


	uint32_t Inhibit(uint32_t toplevel_xid = 0);
	void UnInhibit(uint32_t cookie);


	static DBusInhibitProxies DiscoverDBus(DBus::Connection& connection);

};

