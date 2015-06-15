#pragma once

// Please note that the win32 platform implementation uses SDL 2.0 for audio, rendering and input.
// https://www.libsdl.org/

#ifdef WIN32

// Do nothing, static memory is fine as is
#define FOR_STATIC_MEMORY 

#ifdef DEBUG
#define ALLOW_LOG
#endif

#include <iostream>
#include "Chip8Emu/EmuTypes.h"

namespace SynchingFeeling
{
	// No change for win
	typedef Char StaticChar;
	typedef UChar StaticUChar;
	typedef Int16 StaticInt16;
	typedef UInt16 StaticUInt16;
	typedef Int32 StaticInt32;
	typedef UInt32 StaticUInt32;
	
	inline Char readStaticChar(const Address address);
	inline UChar readStaticUChar(const Address address);
	inline Int16 readStaticInt16(const Address address);
	inline UInt16 readStaticUInt16(const Address address);
	inline Int32 readStaticInt32(const Address address);
	inline UInt32 readStaticUInt32(const Address address);

	template<typename T>
	void fail(T arg)
	{
#ifdef ALLOW_LOG
		std::cerr << arg;
#endif
	}

	template <typename... T>
	void fail(char* message, T... args)
	{
#ifdef ALLOW_LOG
		std::cerr << message;
		fail(args...);
		std::cerr << std::endl;
#endif
	}

	template<typename T>
	void log(T arg)
	{
#ifdef ALLOW_LOG
		std::cout << arg;
#endif
	}

	template <typename... T>
	void log(char* message, T... args)
	{
#ifdef ALLOW_LOG
		std::cout << message;
		fail(args...);
		std::cout << std::endl;
#endif
	}

	template <typename... T>
	void assert(const bool precondition, char* message, T... args)
	{
#ifdef ALLOW_LOG
		if (!precondition)
		{
			fail(message, args...);
		}
#endif
	}

	void platformInit(const Int32 pixelsWidth, const Int32 pixelsHeight, const Int32 screenWidth, const Int32 screenHeight);
	void platformDeInit();
	void platformDraw(const void* gfx, const Int32 width, const Int32 height);
	bool platformPollInput(UChar& inOutKeyPressed, Char& inOutShouldUpdateCycleRate);
	void platformUpdateAudio();
	void platformPlaySound();
	void platformStopSound();
	bool platformCanUpdate(UInt32& inOutTicksIntoYield, const UInt32 yieldTimeMS);
	void platformLoadGame(const char* gameName, char* readBuffer, const UInt32 readSize);
	UChar platformRand(const UChar mask);
}

#endif //#ifdef WIN32