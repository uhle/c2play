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

#include "Thread.h"
#include "WaitCondition.h"

#include <atomic>


class DBusInhibitProxiesWorker
{
	uint32_t toplevel_xid;
	std::atomic<bool> inhibitServices;
	std::atomic<bool> isRunning;
	Thread thread;
	WaitCondition waitCondition;


public:

	DBusInhibitProxiesWorker(uint32_t toplevel_xid = 0);
	~DBusInhibitProxiesWorker();


	void DoWork();


	void SendInhibitMessage();
	void SendUnInhibitMessage();

};

