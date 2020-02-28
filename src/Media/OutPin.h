/*
*
* Copyright (C) 2016 OtherCrashOverride@users.noreply.github.com.
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


#include "Pin.h"
#include "Thread.h"
#include "WaitCondition.h"
#include "Event.h"
#include "EventArgs.h"


//class InPin;
//typedef std::shared_ptr<InPin> InPinSPTR;

class BufferEventArgs : public EventArgs
{
	BufferUPTR buffer;


public:
	const BufferUPTR& Buffer() const
	{
		return buffer;
	}

	BufferUPTR&& MoveBuffer()
	{
		return std::move(buffer);
	}


	BufferEventArgs(BufferUPTR&& buffer)
		: buffer(std::forward<BufferUPTR>(buffer))
	{
	}

};



class OutPin : public Pin
{
	friend class Element;

	InPinSPTR sink;
	Mutex sinkMutex;
	//ThreadSafeQueue<BufferUPTR> filledBuffers;
	ThreadSafeQueue<BufferUPTR> availableBuffers;
	ThreadSPTR pinThread;
	WaitCondition waitCondition;


	void WorkThread();


protected:

	void AddAvailableBuffer(BufferUPTR&& buffer);
	virtual void DoWork();


public:
	Event<EventArgs> BufferReturned;

	bool IsConnected() const
	{
		return sink != nullptr;
	}


	OutPin(const ElementWPTR& owner, const PinInfoSPTR& info);

	virtual ~OutPin();


	void Wake();

	bool TryGetAvailableBuffer(BufferUPTR* outValue);
	void SendBuffer(BufferUPTR&& buffer);
	virtual void Flush() override;


	void Connect(const InPinSPTR& sink);
	void AcceptProcessedBuffer(BufferUPTR&& buffer);
};

//typedef std::shared_ptr<OutPin> OutPinSPTR;


class OutPinCollection : public PinCollection<OutPinSPTR>
{
public:
	OutPinCollection()
		: PinCollection()
	{
	}
};
