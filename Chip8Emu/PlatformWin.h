#pragma once

// Please note that the win32 platform implementation uses SDL 2.0 for audio, rendering and input.
// https://www.libsdl.org/

#ifdef WIN32

#include <iostream>
#include "Chip8Emu/EmuTypes.h"

namespace SynchingFeeling
{
	template<typename T>
	void fail(T arg)
	{
		std::cerr << arg;
	}

	template <typename... T>
	void fail(char* message, T... args)
	{
		std::cerr << message;
		fail(args...);
		std::cerr << std::endl;
	}

	template <typename... T>
	void assert(const bool precondition, char* message, T... args)
	{
		if (!precondition)
		{
			fail(message, args...);
		}
	}

	static const UChar kInvalidKey = 0xFF;

	void platformInit(const Int32 pixelsWidth, const Int32 pixelsHeight, const Int32 screenWidth, const Int32 screenHeight);
	void platformDeInit();
	void platformDraw(const void* gfx, const Int32 width, const Int32 height);
	bool platformPollInput(UChar& inOutKeyPressed, Char& inOutShouldUpdateCycleRate);
	void platformUpdateAudio();
	void platformPlaySound();
	void platformStopSound();
	bool platformCanUpdate(UInt32& inOutTicksIntoYield, const UInt32 yieldTimeMS);
}

#endif //#ifdef WIN32