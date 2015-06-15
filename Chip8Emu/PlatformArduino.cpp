#ifdef ARDUINO

#include "PlatformArduino.h"

#include <avr/pgmspace.h>
#include <SPI.h>
#include <Adafruit_GFX.h>    // Core graphics library
#include <Adafruit_TFTLCD.h> // Hardware-specific library
#include <SD.h>

#define LCD_CS A3 // Chip Select goes to Analog 3
#define LCD_CD A2 // Command/Data goes to Analog 2
#define LCD_WR A1 // LCD Write goes to Analog 1
#define LCD_RD A0 // LCD Read goes to Analog 0
#define LCD_RESET A4 // Can alternately just connect to Arduino's reset pin

using namespace std;

namespace SynchingFeeling
{
	namespace
	{
		// Input Pins
		static const Int32 kPin_0 = 0;
		static const Int32 kPin_1 = 0; 
		static const Int32 kPin_2 = 0;
		static const Int32 kPin_3 = 0;
		static const Int32 kPin_4 = 0;
		static const Int32 kPin_5 = 0;
		static const Int32 kPin_6 = 0;
		static const Int32 kPin_7 = 0;
		static const Int32 kPin_8 = 0;
		static const Int32 kPin_9 = 0;
		static const Int32 kPin_A = 0;
		static const Int32 kPin_B = 0;
		static const Int32 kPin_C = 0;
		static const Int32 kPin_D = 0;
		static const Int32 kPin_E = 0;
		static const Int32 kPin_F = 0;
		static const Int32 kAudioPin = 0;
		static const Int32 kSDChipSelect = 4;


		// Pin-Key mappings
		//
		// 0123	123C
		// 4567 456D
		// 89AB 789E
		// CDEF A0BF
		static const Int32 kKeyMappings[] =
		{
			kPin_D, // 0
			kPin_0, // 1
			kPin_1, // 2
			kPin_2, // 3
			kPin_4, // 4
			kPin_5, // 5
			kPin_6, // 6
			kPin_8, // 7
			kPin_9, // 8
			kPin_A, // 9
			kPin_C, // A
			kPin_E, // B
			kPin_3, // C
			kPin_7, // D
			kPin_B, // E
			kPin_F  // F
		};
		static const Int32 kKeyMappingsSize = (sizeof(kKeyMappings) / sizeof(Int32));

		static File gFile;
		static Adafruit_TFTLCD gTft(LCD_CS, LCD_CD, LCD_WR, LCD_RD, LCD_RESET);
		static Int32 gLetterboxWidth;
		static Int32 gLetterboxHeight;

	} // namespace

	Char readStaticChar(const Address address)
	{
		return pgm_read_byte_near(address);
	}

	UChar readStaticUChar(const Address address)
	{
		return pgm_read_byte_near(address);
	}

	Int16 readStaticInt16(const Address address)
	{
		return pgm_read_word_near(address);
	}

	UInt16 readStaticUInt16(const Address address)
	{
		return pgm_read_word_near(address);
	}

	Int32 readStaticInt32(const Address address)
	{
		return pgm_read_dword_near(address);
	}

	UInt32 readStaticUInt32(const Address address)
	{
		return pgm_read_dword_near(address);
	}

	void fail(const char* message, ...)
	{
		char buffer[64];
		va_list arglist;
		va_start(arglist, message);
		va_end(arglist);

		Serial.print("FAIL: ");
		Serial.println(buffer);
	}

	void log(const char* message)
	{
		Serial.println(message);
	}

	void platformInit(const Int32 pixelsWidth, const Int32 pixelsHeight, const Int32, const Int32)
	{
		Serial.begin(9600);
		// TODO:
		//pinMode(ledPin, OUTPUT);

		// We're limited by the hardware for screen size...
		const Int32 kWidth = gTft.width();
		const Int32 kHeight = gTft.height();
		gLetterboxWidth = kWidth - pixelsWidth;
		gLetterboxHeight = kHeight - pixelsHeight;
		
		// Leave analogue 0 disconnected... we can use it to seed.
		randomSeed(analogRead(0));

		Serial.print("Initializing SD card...");
		pinMode(SS, OUTPUT);

		if (!SD.begin(kSDChipSelect))
		{
			Serial.println("initialization failed!");
			return;
		}	
	}

	void platformDeInit()
	{
		// Do nothing.
	}

	void platformDraw(const void* gfx, const Int32 width, const Int32 height)
	{
		for (Int32 x = 0; x < width; ++x)
		{
			for (Int32 y = 0; y < height; ++y)
			{
				//const UChar* currentByte = reinterpret_cast<const UChar>(gfx)[x + (y * gLetterboxWidth)];
				gTft.fillRect(x, y, 1, 1, 0xFFFF);
			}
		}
	}

	bool platformPollInput(UChar& inOutKeyPressed, Char& inOutShouldUpdateCycleRate)
	{
		//TODO:
		//val = digitalRead(inPin); 

		return false;
	}

	void platformUpdateAudio()
	{
		// do nothing
	}

	void platformPlaySound()
	{
		digitalWrite(kAudioPin, HIGH);
	}

	void platformStopSound()
	{
		digitalWrite(kAudioPin, LOW);
	}

	bool platformCanUpdate(UInt32& inOutTicksIntoYield, const UInt32 yieldTimeMS)
	{
		UInt32 ticks = millis();
		if (inOutTicksIntoYield == 0)
		{
			inOutTicksIntoYield = ticks;
			return false;
		}

		if ((ticks - inOutTicksIntoYield) >= yieldTimeMS)
		{
			// accrue any error
			inOutTicksIntoYield = inOutTicksIntoYield + yieldTimeMS;
			return true;
		}

		return false;
	}

	void platformLoadGame(const char* gameName, char* readBuffer, const UInt32 readSize)
	{
		gFile = SD.open("PONG2", FILE_READ);
		Int32 readSizeMutable = readSize;
		while (gFile.available()) 
		{
			const Int32 tell = gFile.read(readBuffer, readSizeMutable);
			readBuffer += tell;
			readSizeMutable -= tell;
		}
		gFile.close();
	}

	UChar platformRand(const UChar mask)
	{
		return random(mask);
	}

} // namespace SynchingFeeling

#endif //#ifdef ARDUINO