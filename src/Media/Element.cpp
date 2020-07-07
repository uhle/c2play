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

#include "Element.h"





	void Element::SetExecutionState(ExecutionStateEnum newState)
	{
		pthread_mutex_lock(&executionWaitMutex);

		ExecutionStateEnum current = ExecutionState();

		while (newState != current)
		{
			bool isInvalidOperation = false;

			switch (current)
			{
				case ExecutionStateEnum::WaitingForExecute:
					isInvalidOperation = (newState != ExecutionStateEnum::Initializing);
					break;

				case ExecutionStateEnum::Initializing:
					isInvalidOperation = (newState != ExecutionStateEnum::Idle);
					break;

				case ExecutionStateEnum::Executing:
					isInvalidOperation = (newState != ExecutionStateEnum::Idle) &&
						(newState != ExecutionStateEnum::Terminating);
					break;

				case ExecutionStateEnum::Idle:
					isInvalidOperation = (newState != ExecutionStateEnum::Executing) &&
						(newState != ExecutionStateEnum::Terminating);
					break;

				case ExecutionStateEnum::Terminating:
					isInvalidOperation = (newState != ExecutionStateEnum::WaitingForExecute);
					break;

				default:
					break;
			}

			if (isInvalidOperation)
			{
				pthread_mutex_unlock(&executionWaitMutex);
				throw InvalidOperationException();
			}

			if (executionState.compare_exchange_weak(current, newState, std::memory_order_acq_rel))
			{
				break;
			}
		}

		// Signal even if already set
		//executionStateWaitCondition.Signal();

		if (pthread_cond_broadcast(&executionWaitCondition) != 0)
		{
			throw Exception("Element::SignalExecutionWait - pthread_cond_broadcast failed.");
		}

		pthread_mutex_unlock(&executionWaitMutex);

		//Wake();

		Log("Element: %s Set ExecutionState=%d\n", name.c_str(), (int)newState);
	}

	bool Element::IsRunning() const
	{
		return isRunning.load(std::memory_order_acquire);
	}



	Element::Element()
		: isRunning(false), state(MediaState::Pause), executionState(ExecutionStateEnum::WaitingForExecute)
	{
	}



	void Element::Initialize()
	{
	}

	void Element::DoWork()
	{
		Log("Element (%s) DoWork exited.\n", name.c_str());
	}

	void Element::Idling()
	{
	}

	void Element::Idled()
	{
	}

	void Element::Terminating()
	{
	}



	void Element::State_Executing()
	{
		pthread_mutex_lock(&waitMutex);

		// Executing state
		// state == MediaState::Play
		//while (desiredExecutionState == ExecutionStateEnum::Executing)
		while (IsRunning())
		{
			while (canSleep.test_and_set(std::memory_order_acquire))
			{
				if (ExecutionState() != ExecutionStateEnum::Idle)
				{
					Log("Element (%s) InternalWorkThread sleeping.\n", name.c_str());
					SetExecutionState(ExecutionStateEnum::Idle);
				}

				pthread_cond_wait(&waitCondition, &waitMutex);
			}

			if (!IsRunning())
				break;


			pthread_mutex_unlock(&waitMutex);

			//playPauseMutex.Lock();

			if (State() == MediaState::Play)
			{
				if (ExecutionState() != ExecutionStateEnum::Executing)
				{
					Log("Element (%s) InternalWorkThread woke.\n", name.c_str());
					SetExecutionState(ExecutionStateEnum::Executing);
				}

				DoWork();
			}

			//playPauseMutex.Unlock();

			pthread_mutex_lock(&waitMutex);
		}

		pthread_mutex_unlock(&waitMutex);
	}


	void Element::InternalWorkThread()
	{
		Log("Element (%s) InternalWorkThread entered.\n", name.c_str());

		Thread::SetCancelTypeDeferred(false);
		Thread::SetCancelEnabled(true);

		SetExecutionState(ExecutionStateEnum::Initializing);
		Initialize();

		/*SetExecutionState(ExecutionStateEnum::Idle);*/

#if 0
		ExecutionStateEnum executionState = ExecutionState();

		while (executionState == ExecutionStateEnum::Idle ||
			executionState == ExecutionStateEnum::Executing)
		{
			// Executing state
			while (ExecutionState() == ExecutionStateEnum::Executing)
			{
				DoWork();

				pthread_mutex_lock(&waitMutex);

				if (ExecutionState() == ExecutionStateEnum::Executing)
				{
					Log("Element (%s) InternalWorkThread sleeping.\n", name.c_str());

					while (canSleep.test_and_set(std::memory_order_acquire))
					{
						pthread_cond_wait(&waitCondition, &waitMutex);
					}

					Log("Element (%s) InternalWorkThread woke.\n", name.c_str());
				}

				pthread_mutex_unlock(&waitMutex);

			}


			// Idle State
			pthread_mutex_lock(&executionWaitMutex);

			if (ExecutionState() == ExecutionStateEnum::Idle)
			{
				Log("Element %s Idling\n", name.c_str());

				Idling();

				while (ExecutionState() == ExecutionStateEnum::Idle)
				{
					//executionStateWaitCondition.WaitForSignal();
					pthread_cond_wait(&executionWaitCondition, &executionWaitMutex);
				}

				Idled();

				Log("Element %s resumed from Idle\n", name.c_str());
			}

			pthread_mutex_unlock(&executionWaitMutex);


			executionState = ExecutionState();
		}


		// Termination
		SetExecutionState(ExecutionStateEnum::WaitingForExecute);
		WaitForExecutionState(ExecutionStateEnum::WaitingForExecute);

		Terminating();
#endif

		//while (true)
		//{
		//	Log("Element (%s) InternalWorkThread - Idle\n", name.c_str());
		//	SetExecutionState(ExecutionStateEnum::Idle);
		//	State_Idle();

		//	if (desiredExecutionState == ExecutionStateEnum::Terminating)
		//		break;

		//	Log("Element (%s) InternalWorkThread - Executing\n", name.c_str());
		//	SetExecutionState(ExecutionStateEnum::Executing);
		//	State_Executing();

		//	if (desiredExecutionState == ExecutionStateEnum::Terminating)
		//		break;
		//}


		SetExecutionState(ExecutionStateEnum::Idle);
		State_Executing();


		Log("Element (%s) InternalWorkThread - Terminating\n", name.c_str());
		SetExecutionState(ExecutionStateEnum::Terminating);
		Terminating();

		SetExecutionState(ExecutionStateEnum::WaitingForExecute);

		Log("Element (%s) InternalWorkThread exited.\n", name.c_str());
	}


	void Element::AddInputPin(const InPinSPTR& pin)
	{
		inputs.Add(pin);
	}
	void Element::ClearInputPins()
	{
		inputs.Clear();
	}

	void Element::AddOutputPin(const OutPinSPTR& pin)
	{
		outputs.Add(pin);
	}
	void Element::ClearOutputPins()
	{
		outputs.Clear();
	}



	InPinCollection* Element::Inputs()
	{
		return &inputs;
	}
	OutPinCollection* Element::Outputs()
	{
		return &outputs;
	}

	ExecutionStateEnum Element::ExecutionState() const
	{
		return executionState.load(std::memory_order_acquire);
	}

	bool Element::IsExecuting() const
	{
		//return desiredExecutionState == ExecutionStateEnum::Executing;
		return State() == MediaState::Play &&
			ExecutionState() == ExecutionStateEnum::Executing;
	}

	std::string Element::Name() const
	{
		return name;
	}
	void Element::SetName(const std::string& name)
	{
		this->name = name;
	}

	bool Element::LogEnabled() const
	{
		return logEnabled;
	}
	void Element::SetLogEnabled(bool value)
	{
		logEnabled = value;
	}

	MediaState Element::State() const
	{
		return state.load(std::memory_order_acquire);
	}
	void Element::SetState(MediaState value)
	{
		MediaState state = State();

		if (state != value)
		{
			ChangeState(state, value);
		}
	}



	Element::~Element()
	{
		Log("Element %s destructed.\n", name.c_str());
	}



	void Element::Execute()
	{
		if (ExecutionState() != ExecutionStateEnum::WaitingForExecute)
		{
			throw InvalidOperationException();
		}


		isRunning.store(true, std::memory_order_release);
		thread.Start();


		Log("Element (%s) Execute.\n", name.c_str());
	}

	void Element::Wake()
	{
		pthread_mutex_lock(&waitMutex);

		canSleep.clear(std::memory_order_release);

		//pthread_cond_signal(&waitCondition);
		if (pthread_cond_broadcast(&waitCondition) != 0)
		{
			throw Exception("Element::Wake - pthread_cond_broadcast failed.");
		}

		pthread_mutex_unlock(&waitMutex);

		Log("Element (%s) Wake.\n", name.c_str());
	}

	void Element::Terminate()
	{
		ExecutionStateEnum executionState = ExecutionState();

		if (executionState != ExecutionStateEnum::Executing &&
			executionState != ExecutionStateEnum::Idle)
		{
			throw InvalidOperationException();
		}

		SetState(MediaState::Pause);
		WaitForExecutionState(ExecutionStateEnum::Idle);

		Flush();

		pthread_mutex_lock(&waitMutex);

		canSleep.clear(std::memory_order_release);
		isRunning.store(false, std::memory_order_release);

		if (pthread_cond_broadcast(&waitCondition) != 0)
		{
			throw Exception("Element::Terminate - pthread_cond_broadcast failed.");
		}

		pthread_mutex_unlock(&waitMutex);

		WaitForExecutionState(ExecutionStateEnum::WaitingForExecute);


		//thread.Cancel();

		Log("Element (%s) thread.Join().\n", name.c_str());
		thread.Join();

		Log("Element (%s) Terminate.\n", name.c_str());
	}


	void Element::ChangeState(MediaState oldState, MediaState newState)
	{
		//playPauseMutex.Lock();

		while (!state.compare_exchange_weak(oldState, newState, std::memory_order_acq_rel));

		//playPauseMutex.Unlock();

		//if (ExecutionState() == ExecutionStateEnum::Executing ||
		//	ExecutionState() == ExecutionStateEnum::Idle)
		//{
		//	// Note: Can not WaitForExecutionState since its
		//	// called inside the thread making it block forever.
		//	switch (newState)
		//	{
		//		case MediaState::Pause:
		//			ChangeExecutionState(ExecutionStateEnum::Idle);
		//			//WaitForExecutionState(ExecutionStateEnum::Idle);
		//			break;

		//		case MediaState::Play:
		//			ChangeExecutionState(ExecutionStateEnum::Executing);
		//			//WaitForExecutionState(ExecutionStateEnum::Executing);
		//			break;

		//		default:
		//			throw NotSupportedException();
		//	}
		//}
		//else
		//{
		//	throw InvalidOperationException();
		//}

		// Check the result of the atomic compare-exchange operation.
		// Just wake up if state has really changed.
		if (oldState != newState)
		{
			Wake();

			Log("Element (%s) ChangeState oldState=%d newState=%d.\n", name.c_str(), (int)oldState, (int)newState);
		}
	}



	void Element::WaitForExecutionState(ExecutionStateEnum state)
	{
		//ExecutionStateEnum executionState;
		//while ((executionState = ExecutionState()) != state)
		//{
		//printf("Element %s: WaitForExecutionState - executionState=%d, waitingFor=%d\n",
		//	name.c_str(), (int)executionState, (int)state);

		//	executionStateWaitCondition.WaitForSignal();
		//}

		//Wake();

		pthread_mutex_lock(&executionWaitMutex);

		while (ExecutionState() != state)
		{
			pthread_cond_wait(&executionWaitCondition, &executionWaitMutex);
		}

		pthread_mutex_unlock(&executionWaitMutex);

		//printf("Element %s: Finished WaitForExecutionState - executionState=%d\n",
		//	name.c_str(), (int)state);
	}

	void Element::Flush()
	{
		if (State() != MediaState::Pause)
			throw InvalidOperationException();

		WaitForExecutionState(ExecutionStateEnum::Idle);

		inputs.Flush();
		outputs.Flush();


		Log("Element (%s) Flush exited.\n", name.c_str());
	}

	// DEBUG
	void Element::Log(const char* message, ...)
	{
		if (logEnabled)
		{
			struct timeval tp;
			gettimeofday(&tp, NULL);
			double ms = tp.tv_sec + tp.tv_usec * 0.0001;

			char text[1024];
			sprintf(text, "[%s : %f] %s", Name().c_str(), ms, message);


			va_list argptr;
			va_start(argptr, message);
			vfprintf(stderr, text, argptr);
			va_end(argptr);
		}
	}
