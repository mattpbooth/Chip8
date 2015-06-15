#pragma once

#ifdef ARDUINO

// Force from SRAM to Flash
#define FOR_STATIC_MEMORY PROGMEM

#include <Arduino.h>
#include "EmuTypes.h"

namespace SynchingFeeling
{
	// Flash mem types
	typedef prog_char StaticChar;
	typedef prog_uchar StaticUChar;
	typedef prog_int16_t StaticInt16;
	typedef prog_uint16_t StaticUInt16;
	typedef prog_int32_t StaticInt32;
	typedef prog_uint32_t StaticUInt32;

	inline Char readStaticChar(const Address address);
	inline UChar readStaticUChar(const Address address);
	inline Int16 readStaticInt16(const Address address);
	inline UInt16 readStaticUInt16(const Address address);
	inline Int32 readStaticInt32(const Address address);
	inline UInt32 readStaticUInt32(const Address address);

	void fail(const char* message, ...);
	void log(const char* message);
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

#endif // #ifdef ARDUINO