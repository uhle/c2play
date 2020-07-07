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

#include "InPin.h"

#include "OutPin.h"
#include "Element.h"

void  InPin::WorkThread()
{
	//printf("InPin: WorkTread started.\n");

	while (true)
	{
		//// Scope the shared pointer
		//{
		//	ElementSPTR owner = Owner().lock();
		//	if (owner && owner->State() == MediaState::Play)
		//	{
		//		DoWork();
		//	}
		//}

		DoWork();

		waitCondition.Lock();
		waitCondition.WaitForSignal();
		waitCondition.Unlock();
	}

	//printf("InPin: WorkTread exited.\n");
}

	void InPin::ReturnAllBuffers()
	{
		ElementSPTR parent = Owner().lock();

		sourceMutex.Lock();

		//if (!source)
		//	throw InvalidOperationException("Can not return buffers to a null source");

		if (source)
		{
			BufferUPTR buffer;
			while (processedBuffers.TryPop(&buffer))
			{
				source->AcceptProcessedBuffer(std::move(buffer));
			}

			waitCondition.Lock();

			while (filledBuffers.TryPop(&buffer))
			{
				source->AcceptProcessedBuffer(std::move(buffer));
			}

			waitCondition.Unlock();
		}

		sourceMutex.Unlock();

		parent->Log("InPin::ReturnAllBuffers completed\n");
	}


	void InPin::DoWork()
	{
		// Work should not block in the thread
	}


	InPin::InPin(const ElementWPTR& owner, const PinInfoSPTR& info)
		: Pin(PinDirectionEnum::In, owner, info)
	{
	}

	InPin::~InPin()
	{
		if (source)
		{
			ReturnAllBuffers();
		}
	}

	bool InPin::TryGetFilledBuffer(BufferUPTR* buffer)
	{
		waitCondition.Lock();

		bool result = filledBuffers.TryPop(buffer);

		waitCondition.Unlock();

		return result;
	}

	void InPin::PushProcessedBuffer(BufferUPTR&& buffer)
	{
		processedBuffers.Push(std::move(buffer));
	}

	void InPin::ReturnProcessedBuffers()
	{
		ElementSPTR parent = Owner().lock();

		sourceMutex.Lock();

		//if (!source)
		//	throw InvalidOperationException("Can not return buffers to a null source");

		if (source)
		{
			BufferUPTR buffer;
			while (processedBuffers.TryPop(&buffer))
			{
				source->AcceptProcessedBuffer(std::move(buffer));

				if (Owner().expired() || parent.get() == nullptr)
				{
					printf("InPin::ReturnProcessedBuffers: WARNING parent expired.\n");
				}
				else
				{
					parent->Log("InPin::ReturnProcessedBuffers returned a buffer\n");
				}
			}
		}

		sourceMutex.Unlock();

		parent->Log("InPin::ReturnProcessedBuffers completed\n");
	}



	void InPin::AcceptConnection(const OutPinSPTR& source)
	{
		if (!source)
			throw ArgumentNullException();

		if (this->source)
		{
			throw InvalidOperationException("A source is already connected.");
		}


		ElementSPTR parent = Owner().lock();
		//ElementSPTR parent = Owner().lock();
		//if (parent->IsExecuting())
		//	throw InvalidOperationException("Can not connect while executing.");


		sourceMutex.Lock();
		this->source = source;
		sourceMutex.Unlock();


		pinThread = std::make_shared<Thread>(std::function<void()>(std::bind(&InPin::WorkThread, this)));
		pinThread->Start();

		parent->Wake();

		parent->Log("InPin::AcceptConnection completed\n");
	}

	void InPin::Disconnect(const OutPinSPTR& source)
	{
		OutPinSPTR safeSource = this->source;

		if (!source)
			throw ArgumentNullException();

		if (!safeSource)
			throw InvalidOperationException("No source is connected.");

		if (safeSource != source)
			throw InvalidOperationException("Attempt to disconnect a source that is not connected.");


		ElementSPTR parent = Owner().lock();
		//ElementSPTR parent = Owner().lock();
		//if (parent->IsExecuting())
		//	throw InvalidOperationException("Can not disconnect while executing.");


		pinThread->Cancel();
		pinThread->Join();
		pinThread = nullptr;


		ReturnAllBuffers();


		sourceMutex.Lock();
		this->source = nullptr;
		sourceMutex.Unlock();

		parent->Log("InPin::Disconnect completed\n");
	}

	void InPin::ReceiveBuffer(BufferUPTR&& buffer)
	{
		if (!buffer)
			throw ArgumentNullException("InPin::ReceiveBuffer: empty buffer");


		ElementSPTR parent = Owner().lock();


		waitCondition.Lock();

		filledBuffers.Push(std::move(buffer));

		// Wake the work thread
		waitCondition.Signal();

		waitCondition.Unlock();


		if (parent)
		{
			parent->Wake();
		}


		BufferReceived.Invoke(this, EventArgs::Empty());

		parent->Log("InPin::ReceiveBuffer completed\n");
	}


	void InPin::Flush()
	{
		ElementSPTR parent = Owner().lock();


		ReturnAllBuffers();

		parent->Log("InPin::Flush completed\n");
	}
