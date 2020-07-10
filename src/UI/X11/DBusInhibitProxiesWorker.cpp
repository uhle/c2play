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

#include "DBusInhibitProxiesWorker.h"
#include "DBusInhibitProxy.h"

#include <utility>
#include <queue>
#include <cstdio>


DBusInhibitProxiesWorker::DBusInhibitProxiesWorker(uint32_t toplevel_xid)
	: toplevel_xid(toplevel_xid), inhibitServices(false), isRunning(true), thread(std::bind(&DBusInhibitProxiesWorker::DoWork, this))
{
	thread.Start();
}

DBusInhibitProxiesWorker::~DBusInhibitProxiesWorker()
{
	waitCondition.Lock();

	isRunning.store(false, std::memory_order_release);

	waitCondition.Signal();
	waitCondition.Unlock();

	thread.Join();
}

void DBusInhibitProxiesWorker::DoWork()
{
	// D-Bus discovery
	DBus::default_dispatcher = new DBus::BusDispatcher();
	DBus::Connection connection = DBus::Connection::SessionBus();
	connection.exit_on_disconnect(false);

	DBusInhibitProxies proxies = DBusInhibitProxy::DiscoverDBus(connection);

	if (!proxies.empty())
	{
		if (proxies.size() == 1)
		{
			printf("DBusWorker: D-Bus service discovered: %s\n", proxies[0]->service().c_str());
		}
		else
		{
			printf("DBusWorker: %zu D-Bus services discovered:", proxies.size());

			for (const DBusInhibitProxyUPTR& proxy : proxies)
			{
				printf(" %s", proxy->service().c_str());
			}

			printf("\n");
		}
	}

	std::queue<uint32_t> cookies;


	// event loop
	waitCondition.Lock();

	while (isRunning.load(std::memory_order_acquire) && connection.connected())
	{
		bool shouldInhibitServices = inhibitServices.load(std::memory_order_acquire);

		waitCondition.Unlock();

		if (shouldInhibitServices == cookies.empty())
		{
			if (shouldInhibitServices)
			{
				for (const DBusInhibitProxyUPTR& proxy : proxies)
				{
					cookies.push(proxy->Inhibit(toplevel_xid));

					if (cookies.back() == (uint32_t)-1)
					{
						printf("WARNING: Could not inhibit service %s\n", proxy->service().c_str());
					}
				}
			}
			else
			{
				for (const DBusInhibitProxyUPTR& proxy : proxies)
				{
					proxy->UnInhibit(cookies.front());
					cookies.pop();
				}
			}
		}

		waitCondition.Lock();

		waitCondition.WaitForSignal();
	}

	waitCondition.Unlock();


	// clean up
	if (!cookies.empty())
	{
		for (const DBusInhibitProxyUPTR& proxy : proxies)
		{
			proxy->UnInhibit(cookies.front());
			cookies.pop();
		}
	}

	proxies.clear();
	connection.flush();
	connection.disconnect();

	delete DBus::default_dispatcher;
	DBus::default_dispatcher = nullptr;
}

void DBusInhibitProxiesWorker::SendInhibitMessage()
{
	waitCondition.Lock();

	inhibitServices.store(true, std::memory_order_release);

	waitCondition.Signal();
	waitCondition.Unlock();
}

void DBusInhibitProxiesWorker::SendUnInhibitMessage()
{
	waitCondition.Lock();

	inhibitServices.store(false, std::memory_order_release);

	waitCondition.Signal();
	waitCondition.Unlock();
}
