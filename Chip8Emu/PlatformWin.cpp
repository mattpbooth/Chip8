#ifdef WIN32

// Please note that the win32 platform implementation uses SDL 2.0 for audio, rendering and input.
// https://www.libsdl.org/

#include "Chip8Emu/PlatformWin.h"

#include <iostream>
#include <fstream>
#include <random>

#include <SDL.h>

using namespace std;

namespace SynchingFeeling
{
	namespace
	{
		// SDL seems reluctant to let us use SDL_PIXELFORMAT_INDEX1LSB
		// We'll use a 3 byte texture format instead: SDL_PIXELFORMAT_RGB24 as it takes uint8 array of data.
		static const UInt32 kPixelFormatEnum = SDL_PIXELFORMAT_RGB24;

		// Minimum 'good' audio lower bound, from SDL2.0 docs says 512 (0x200)
		// Buuut that's at odds with the audio sample time and we have if adhering to the SDL freqency formula (only dogs will hear)
		// This seems to work pretty well actually, even though its much lower than the recommended size.
		static const UInt16 kAudioSamplesSize = 0x10;
		// Seems to need to be less than the minimum timer length ((1/60))
		// Any longer and some of the more subtle sounds are inaudiable
		static const Int32 kAudioSampleTimeInMs = 10; 		
		static const UChar kAudioSampleAmplitude = 0x10;

		static SDL_Window* gWindow;
		static SDL_Renderer* gRenderer;
		static SDL_Texture* gTexture;
		static UChar* gRenderTexture;
		static UInt32 gRenderTextureSize;
		static enum gPixelFormatEnum;
		static SDL_PixelFormat* gPixelFormat;
		static SDL_AudioSpec* gObtainedAudioSpec;
		static SDL_AudioStatus gAudioStatus;
		static mt19937 gRandGen;


		// Key mappings
		//
		// 1234	123C
		// qwer 456D
		// asdf 789E
		// zcxv A0BF
		static const Int32 kKeyMappings[] =
		{
			SDLK_c, // 0
			SDLK_1, // 1
			SDLK_2, // 2
			SDLK_3, // 3
			SDLK_q, // 4
			SDLK_w, // 5
			SDLK_e, // 6
			SDLK_a, // 7
			SDLK_s, // 8
			SDLK_d, // 9
			SDLK_z, // A
			SDLK_x, // B
			SDLK_4, // C
			SDLK_r, // D
			SDLK_f, // E
			SDLK_v  // F
		};
		static const Int32 kKeyMappingsSize = (sizeof(kKeyMappings) / sizeof(Int32));
	
	} // namespace

	Char readStaticChar(const Address address)
	{
		return static_cast<Char>(*&address);
	}

	UChar readStaticUChar(const Address address)
	{
		return static_cast<UChar>(*&address);
	}
	
	Int16 readStaticInt16(const Address address)
	{
		return static_cast<Int16>(*&address);
	}

	UInt16 readStaticUInt16(const Address address)
	{
		return static_cast<UInt16>(*&address);
	}
	
	Int32 readStaticInt32(const Address address)
	{
		return static_cast<Int32>(*&address);
	}
	
	UInt32 readStaticUInt32(const Address address)
	{
		return static_cast<UInt32>(*&address);
	}

	void platformInit(const Int32 pixelsWidth, const Int32 pixelsHeight, const Int32 screenWidth, const Int32 screenHeight)
	{
		if (SDL_Init(SDL_INIT_TIMER | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER | SDL_INIT_VIDEO) < 0)
		{
			fail("SDL could not initialize! SDL_Error: ", SDL_GetError());
			return;
		}

		gWindow = SDL_CreateWindow("Chip8Emu", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, screenWidth, screenHeight, SDL_WINDOW_SHOWN);
		if (!gWindow)
		{
			fail("Window could not be created! SDL_Error: ", SDL_GetError());
			return;
		}

		gRenderer = SDL_CreateRenderer(gWindow, -1, 0);
		if (!gRenderer)
		{
			fail("Renderer could not be created! SDL_Error: ", SDL_GetError());
			return;
		}

		
		gTexture = SDL_CreateTexture(gRenderer, kPixelFormatEnum, SDL_TEXTUREACCESS_STATIC, pixelsWidth, pixelsHeight);
		if (!gTexture)
		{
			fail("Texture could not be created! SDL_Error: ", SDL_GetError());
		}

		gPixelFormat = SDL_AllocFormat(kPixelFormatEnum);
		if (!gPixelFormat)
		{
			gRenderTexture = nullptr;
			fail("Could not create pixel format from enum! SDL_Error: ", SDL_GetError());
		}
		else
		{
			gRenderTextureSize = pixelsWidth * pixelsHeight * gPixelFormat->BytesPerPixel;
			gRenderTexture = new UChar[gRenderTextureSize];
		}

		// Populate a memory buffer with a waveform.
		// This goes wide so let's just create the waveform inline.
		SDL_AudioSpec desiredAudioSpec;
		desiredAudioSpec.callback = [](void*, Uint8* buffer, int bufferSize)
		{
			SDL_LockAudio();
			
			Char sign = 1;
			for (auto i = 0; i < bufferSize; ++i)
			{
				UChar squareWave = (0x80 + (sign * kAudioSampleAmplitude));
				buffer[i] = squareWave;
				sign = -sign;
			}
			
			SDL_UnlockAudio();
		};

		// mono
		desiredAudioSpec.channels = 1;
		desiredAudioSpec.format = AUDIO_U8;
		desiredAudioSpec.padding = 0;
		desiredAudioSpec.userdata = nullptr;

		// freq = (samples * 1000) / ms
		desiredAudioSpec.samples = kAudioSamplesSize;
		desiredAudioSpec.freq = (kAudioSamplesSize * 1000) / kAudioSampleTimeInMs;

		if(0 != SDL_OpenAudio(&desiredAudioSpec, gObtainedAudioSpec))
		{
			fail("Could not initialise audio! SDL_Error: ", SDL_GetError());
		}

		// Seed rand
		const auto seed = random_device()();
		gRandGen.seed(seed);
	}

	void platformDeInit()
	{
		SDL_CloseAudio();

		delete[] gRenderTexture;
		gRenderTexture = nullptr;

		SDL_FreeFormat(gPixelFormat);
		gPixelFormat = nullptr;
		
		SDL_DestroyTexture(gTexture);
		gTexture = nullptr;

		SDL_DestroyRenderer(gRenderer);
		gTexture = nullptr;

		SDL_DestroyWindow(gWindow);
		gWindow = nullptr;

		SDL_Quit();
	}

	void platformDraw(const void* gfx, const Int32 width, const Int32 height)
	{
		if (!gWindow || !gRenderer || !gTexture || !gRenderTexture)
		{
			fail("platformDraw failed due to setup errors.");
			return;
		}

		for (auto i = 0; i < width * height; ++i)
		{
			const auto currentByte = reinterpret_cast<const UChar*>(gfx)[i];
			gRenderTexture[(i * gPixelFormat->BytesPerPixel)] = currentByte;
			gRenderTexture[((i * gPixelFormat->BytesPerPixel) + 1)] = currentByte;
			gRenderTexture[((i * gPixelFormat->BytesPerPixel) + 2)] = currentByte;
		}

		if (0 != SDL_UpdateTexture(gTexture, nullptr, reinterpret_cast<const void*>(gRenderTexture), width * gPixelFormat->BytesPerPixel))
		{
			fail("SDL_UpdateTexture failed! SDL_Error: ", SDL_GetError());
			return;
		}

		if(0 != SDL_RenderClear(gRenderer))
		{
			fail("SDL_RenderClear failed! SDL_Error: ", SDL_GetError());
			return;
		}

		if (0 != SDL_RenderCopy(gRenderer, gTexture, nullptr, nullptr))
		{
			fail("SDL_RenderCopy failed! SDL_Error: ", SDL_GetError());
			return;
		}

		// No return
		SDL_RenderPresent(gRenderer);
	}

	bool platformPollInput(UChar& inOutKeyPressed, Char& inOutShouldUpdateCycleRate)
	{
		// poll for new
		SDL_Event e;
		bool shouldQuit = false;
		const auto eventResult = SDL_PollEvent(&e);
		if (eventResult == 0)
		{ 
			return false;
		}

		switch (e.type)
		{
			case SDL_QUIT:
				shouldQuit = true;
				break;

			case SDL_KEYDOWN:
				for (UChar i = 0; i < kKeyMappingsSize; ++i)
				{
					if (kKeyMappings[i] == e.key.keysym.sym)
					{
						inOutKeyPressed = i;
						break;
					}
				}

				switch (e.key.keysym.sym)
				{
					// increment cycle rate
					case SDLK_PLUS:
					case SDLK_EQUALS:
						inOutShouldUpdateCycleRate = 1;
						break;

					// decrement cycle rate
					case SDLK_MINUS:
					case SDLK_UNDERSCORE:
						inOutShouldUpdateCycleRate = -1;
						break;
				}
				break;

			case SDL_KEYUP:
			{
				for (UChar i = 0; i < kKeyMappingsSize; ++i)
				{
					if (kKeyMappings[i] == e.key.keysym.sym)
					{
						inOutKeyPressed = kInvalidKey;
						break;
					}
				}
				break;
			}

		}
		return shouldQuit;
	}

	void platformUpdateAudio()
	{
		gAudioStatus = SDL_GetAudioStatus();
	}

	void platformPlaySound()
	{
		if (gAudioStatus != SDL_AUDIO_PLAYING)
		{
			SDL_PauseAudio(0);
		}
	}

	void platformStopSound()
	{
		if (gAudioStatus == SDL_AUDIO_PLAYING)
		{
			SDL_PauseAudio(1);
		}
	}

	bool platformCanUpdate(UInt32& inOutTicksIntoYield, const UInt32 yieldTimeMS)
	{
		UInt32 ticks = SDL_GetTicks();
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
		ifstream stream;
		stream.open(gameName, ios::in | ios::binary);
		if (!stream.is_open())
		{
			fail("Failed to open game: ", gameName);
			return;
		}

		stream.seekg(0, ios::beg);
		stream.read(readBuffer, readSize);
		stream.close();
	}

	UChar platformRand(const UChar mask)
	{
		uniform_int_distribution<> dist(0, mask);
		return static_cast<UChar>(dist(gRandGen));
	}


} // namespace SynchingFeeling

#endif //#ifdef WIN32