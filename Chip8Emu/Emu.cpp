#include "Chip8Emu/Emu.h"

#include <iostream>
#include <functional>
#include <fstream>
#include <random>

#include "Chip8Emu/EmuTypes.h"

#ifdef WIN32
#include "PlatformWin.h"
#endif

using namespace std;

namespace SynchingFeeling
{
	namespace
	{
		// Enums
		// Should we modify underflow/overflow registers?
		enum class ESetFlowRegister
		{
			Yes,
			No
		};

		// What should we set the underflow/overflow register to on detection?
		enum class EValueSetOnFlowDetect
		{
			True,
			False
		};

		// Should we increment PC?
		enum class EIncrementPC
		{
			Yes,
			No
		};

		// Should we quit
		enum class EQuit
		{
			Yes,
			No
		};

		// Memory map
		enum class MemoryMapIndex : UChar
		{
			Interpreter,
			FontSet,
			PRG,
			Max
		};

		// local types
		typedef bool DrawFlag;
		typedef function <EIncrementPC(const UShort)> OpCodeFunction;

		// general consts
		static const Int32 kGFXWidth = 64;							// pixels in one screen's width
		static const Int32 kGFXHeight = 32;							// pixels in one screen's height
		static const Int32 kScreenScale = 10;						// Pixel upscale to window
		static const UShort kPCStart = 0x200;						// PC start pos in memory
		static const UShort	kDefaultOpCode = 0x00;					// Erroneous UShort
		static const UShort kDefaultSpecialReg = 0x00;				// Initial special register value
		static const UInt32 kByteMinOne = 8 - 1;					// For shifting logic 
		static const UChar kFontCharacterHeight = 5;				// How many pixels high is a single font character?
		static const UChar kSizeOfKeypressCooldownBuffer = 0x0F;	// One for each key
		
		// Timings
		static const UInt32 kTimerUpdateRateMS = static_cast<UInt32>((1.0f / 60.f) * 1000.0f);	// 60Hz, ish.
		static const UInt32 kCycleUpdateRateBase = static_cast<UInt32>((1.0f / 60.f) * 100.0f);	// 600Hz per cycle, ish.
		static const UInt32 kCycleUpdateRateDelta = static_cast<UInt32>((1.0f / 60.f) * 100.0f);// +=600Hz, ish.

		// endian specifics
		static const UShort kEndianCheck = 0x1234;
		static const bool kLittleEndian =	
			*(reinterpret_cast<const UChar*>(&kEndianCheck)) != 0x12;
		inline UShort ShortSwap(const UShort& s)
		{
			return (kLittleEndian) ? (((s & 0xFF) << 8) + ((s >> 8) & 0xFF)) : s;
		}

		// masks and shifts
		static inline UShort maskShiftF000(const UShort in){ return ((in & 0xF000) >> 12); }
		static inline UShort maskShift0F00(const UShort in){ return ((in & 0x0F00) >> 8); }
		static inline UShort maskShift00F0(const UShort in){ return ((in & 0x00F0) >> 4); }
		static inline UShort maskShift000F(const UShort in){ return ((in & 0x000F)); }

		// In memory font
		static const UChar gFontSet[] =
		{
			0xF0, 0x90, 0x90, 0x90, 0xF0, // 0
			0x20, 0x60, 0x20, 0x20, 0x70, // 1
			0xF0, 0x10, 0xF0, 0x80, 0xF0, // 2
			0xF0, 0x10, 0xF0, 0x10, 0xF0, // 3
			0x90, 0x90, 0xF0, 0x10, 0x10, // 4
			0xF0, 0x80, 0xF0, 0x10, 0xF0, // 5
			0xF0, 0x80, 0xF0, 0x90, 0xF0, // 6
			0xF0, 0x10, 0x20, 0x40, 0x40, // 7
			0xF0, 0x90, 0xF0, 0x90, 0xF0, // 8
			0xF0, 0x90, 0xF0, 0x10, 0xF0, // 9
			0xF0, 0x90, 0xF0, 0x90, 0x90, // A
			0xE0, 0x90, 0xE0, 0x90, 0xE0, // B
			0xF0, 0x80, 0x80, 0x80, 0xF0, // C
			0xE0, 0x90, 0x90, 0x90, 0xE0, // D
			0xF0, 0x80, 0xF0, 0x80, 0xF0, // E
			0xF0, 0x80, 0xF0, 0x80, 0x80  // F
		};

		struct MemoryMapRange
		{
			const UShort mMin;
			const UShort mMax;
			MemoryMapRange(const UShort min, const UShort max)
				: mMin(min)
				, mMax(max)
			{}

			void operator=(const MemoryMapRange& rhs) = delete;
			MemoryMapRange(const MemoryMapRange& rhs) = delete;
		};

		static const MemoryMapRange kMemoryMapRange[static_cast<int>(MemoryMapIndex::Max)] =
		{
			MemoryMapRange(0x000, 0x1FF),	// Interpreter
			MemoryMapRange(0x050, 0x0A0),	// Fontset
			MemoryMapRange(0x200, 0xFFF)	// PRG 
		};

		// globals
		static UChar gMemory[1024 * 4];									// 4KiB memory
		static UChar gV[16];											// Registers V0-VE (+carry)
		static UShort gI;												// Index reg (0x000-0xFFF)
		static UShort gPC;												// Program Counter (0x000-0xFFF)
		static UShort gSP;												// Stack Pointer (0x000-0xFFF)
		static UShort gStack[16];										// Stack
		static UChar gGfx[kGFXWidth * kGFXHeight];						// 2048 pixels, b&w
		static UChar gDelayTimer;										// 60hz countdown - delay
		static UChar gSoundTimer;										// 60hz countdown - sound
		static UChar gKeyPress;											// Current Keypresses 0-15
		static UChar gKeyPressCoolDown[kSizeOfKeypressCooldownBuffer];	// Cooldowns for each key
		static UShort gOpCode;											// Current UShort
		static mt19937 gRandGen;										// Random number generator;
		static DrawFlag gDrawFlag;										// Draw flag
		static UInt32 gTimerTicksSinceLastUpdate;						// The timer for the... timers.
		static UInt32 gCycleTicksSinceLastUpdate;						// The timer for the emulation cycles
		static UInt32 gCycleUpdateRateModifierMS;						// The update rate for emulation cycles modifier

		// anonymous inline methods
		inline UChar& getVX(const UShort opCode)
		{
			return gV[maskShift0F00(opCode)];
		}

		inline UChar& getVY(const UShort opCode)
		{
			return gV[maskShift00F0(opCode)];
		}

		inline void incrementPC()
		{
			gPC += sizeof(UShort);
		}

		inline void setRegisterImmediate(UShort& reg, const UShort address)
		{
			reg = address;
		}

		inline void modifyRegisterFlow(const UShort regProxy, const ESetFlowRegister setFlowRegister, const EValueSetOnFlowDetect valueSet)
		{
			// Overflow / underflow is variable
			if (setFlowRegister == ESetFlowRegister::Yes)
			{
				UChar setValueOnDetectLookup[] = { 0x01, 0x01 }; 
				if (valueSet == EValueSetOnFlowDetect::False)
				{
					setValueOnDetectLookup[0] = 0x00;
				}
				else
				{
					setValueOnDetectLookup[1] = 0x00;
				}
				gV[0xF] = (regProxy / 0xFF != 0) ? setValueOnDetectLookup[0] : setValueOnDetectLookup[1];
			}
		}

		inline void addToRegister(UChar& reg, const UChar value, const ESetFlowRegister setFlowRegister)
		{
			UShort sum = reg + value;
			modifyRegisterFlow(sum, setFlowRegister, EValueSetOnFlowDetect::True);
			reg = sum % 0x100;
		}

		inline void subtractFromRegister(UChar& reg, const UChar value, const ESetFlowRegister setFlowRegister)
		{
			UShort minus = reg - value;
			modifyRegisterFlow(minus, setFlowRegister, EValueSetOnFlowDetect::False);
			reg = minus % 0x100;
		}

		inline bool isKeyPressed(const UChar key)
		{
			if (gKeyPress != kInvalidKey)
			{
				return (gKeyPress == key);
			}
			return false;
		}

		inline void setPCImmediate(const UShort address)
		{
			setRegisterImmediate(gPC, address);
		}

		inline void setIImmediate(const UShort address)
		{
			setRegisterImmediate(gI, address);
		}

		inline void cls()
		{
			memset(&gGfx, 0, kGFXWidth* kGFXHeight);
		}

		inline void pushStack()
		{
			gStack[gSP] = gPC;
			++gSP;
		}

		inline void popStack()
		{
			setPCImmediate(gStack[--gSP]);
		}

		// The VM
		static OpCodeFunction gVM[] =
		{
			[](const UShort opCode)
			{
				switch (opCode)
				{
					// 00E0
					// Clear the screen
					case 0x00E0:
						cls();
						gDrawFlag = true;
						return EIncrementPC::Yes;

					// 00EE
					// Return from subroutine 
					case 0x00EE:
						popStack();
						return EIncrementPC::Yes;

					// 0NNN
					// Execute machine language subroutine at address NNN
					default:
						fail("0NNN not implemented");
						return EIncrementPC::Yes;
				}
			},
			// 1NNN
			// Jump to address NNN
			[](const UShort opCode)
			{
				setPCImmediate(opCode & 0x0FFF);
				return EIncrementPC::No;
			},
			// 2NNN
			// Call subroutine at NNN
			[](const UShort opCode)
			{
				pushStack();
				setPCImmediate(opCode & 0x0FFF);
				return EIncrementPC::No;
			},
			// 3XNN
			// Skip the next instruction if VX == NN
			[](const UShort opCode)
			{
				const auto vx = getVX(opCode);
				if (vx == (opCode & 0x00FF))
				{
					incrementPC();
				}
				return EIncrementPC::Yes;
			},
			// 4XNN
			// Skip the next instruction if VX != NN
			[](const UShort opCode)
			{
				const auto vx = getVX(opCode);
				if (vx != (opCode & 0x00FF))
				{
					incrementPC();
				}
				return EIncrementPC::Yes;
			},
			// 5XY0
			// Skip the next instruction if VX == VY
			[](const UShort opCode)
			{
				const auto vx = getVX(opCode);
				const auto vy = getVY(opCode);
				if (vx == vy)
				{
					incrementPC();
				}
				return EIncrementPC::Yes;
			},
			// 6XNN
			// Sets VX to NN
			[](const UShort opCode)
			{
				auto& v = getVX(opCode);
				v = opCode & 0x00FF;
				return EIncrementPC::Yes;
			},
			// 7XNN
			// Adds NN to VX
			[](const UShort opCode)
			{
				auto& v = getVX(opCode);
				addToRegister(v, (opCode & 0x00FF), ESetFlowRegister::No);
				return EIncrementPC::Yes;
			},
			[](const UShort opCode)
			{
				switch (opCode & 0x000F)
				{
					// 8XY0
					// Sets VX to the value of VY.
					case 0x0:
					{
						auto& vx = getVX(opCode);
						vx = getVY(opCode);
						return EIncrementPC::Yes;
					}

					// 8XY1
					// Sets VX to VX or VY.
					case 0x1:
					{
						auto& vx = getVX(opCode);
						vx = vx | getVY(opCode);
						return EIncrementPC::Yes;
					}

					// 8XY2
					// Sets VX to VX and VY.
					case 0x2:
					{
						auto& vx = getVX(opCode);
						vx = vx & getVY(opCode);
						return EIncrementPC::Yes;
					}

					// 8XY3	
					// Sets VX to VX xor VY.
					case 0x3:
					{
						auto& vx = getVX(opCode);
						vx = vx ^ getVY(opCode);
						return EIncrementPC::Yes;
					}

					// 8XY4	
					// Add the value of register VY to register VX
					// Set VF to 01 if a carry occurs
					// Set VF to 00 if a carry does not occur
					case 0x4:
					{
						auto& vx = getVX(opCode);
						auto& vy = getVY(opCode);
						addToRegister(vx, vy, ESetFlowRegister::Yes);
						return EIncrementPC::Yes;
					}

					// 8XY5
					// Subtract the value of register VY from register VX
					// Set VF to 00 if a borrow occurs
					// Set VF to 01 if a borrow does not occur
					case 0x5:
					{
						auto& vx = getVX(opCode);
						auto& vy = getVY(opCode);
						subtractFromRegister(vx, vy, ESetFlowRegister::Yes);
						return EIncrementPC::Yes;
					}

					// 8XY6
					// Store the value of register VY shifted right one bit in register VX
					// Set register VF to the least significant bit prior to the shift
					case 0x6:
					{
						auto& vx = getVX(opCode);
						auto& vy = getVY(opCode);
						vx = vy >> 0x01;
						vy = vy & 0x01;
						return EIncrementPC::Yes;
					}

					// 8XY7
					// Set register VX to the value of VY minus VX
					// Set VF to 00 if a borrow occurs
					// Set VF to 01 if a borrow does not occur
					case 0x7:
					{
						auto& vx = getVX(opCode);
						auto yCpy = getVY(opCode);
						auto xCpy = vx;
						subtractFromRegister(yCpy, xCpy, ESetFlowRegister::Yes);
						vx = yCpy;
						return EIncrementPC::Yes;
					}

					// 8XYE
					// Store the value of register VY shifted left one bit in register VX
					// Set register VF to the most significant bit prior to the shift
					case 0xE:
					{
						auto& vx = getVX(opCode);
						auto& vy = getVY(opCode);
						vx = vy << 0x01;
						vy = vy & 0x80;
						return EIncrementPC::Yes;
					}

					default:
						fail("Invalid opcode:", opCode);
						return EIncrementPC::No;
				}
			},
			// 9XY0
			// Skips the next instruction if VX doesn't equal VY.
			[](const UShort opCode)
			{
				const auto vx = getVX(opCode);
				const auto vy = getVY(opCode);
				if (vx != vy)
				{
					incrementPC();
				}
				return EIncrementPC::Yes;
			},
			// ANNN	
			// Sets I to the address NNN.
			[](const UShort opCode)
			{
				setIImmediate(opCode & 0x0FFF);
				return EIncrementPC::Yes;
			},
			// BNNN
			// Jumps to the address NNN plus V0.
			[](const UShort opCode)
			{
				setPCImmediate(gV[0] + (opCode & 0x0FFF));
				return EIncrementPC::Yes;
			},
			// CXNN	
			// Sets VX to a random number, masked by NN.
			[](const UShort opCode)
			{
				auto& vx = getVX(opCode);
				uniform_int_distribution<> dist(0, opCode & 0xFF);
				vx = static_cast<UChar>(dist(gRandGen));
				return EIncrementPC::Yes;
			},
			// DXYN	
			// Draw a sprite at position VX, VY with N bytes of sprite data starting at the address stored in I
			// Set VF to 01 if any set pixels are changed to unset, and 00 otherwise
			[](const UShort opCode)
			{
				auto vx = getVX(opCode);
				auto vy = getVY(opCode);

				const auto height = opCode & 0x000F;
				bool flagCollision = false;
				for (auto i = 0; i < height; ++i)
				{
					const auto gfxIndex = vx + (vy * kGFXWidth) + (i * kGFXWidth);

					const auto byteToSet = gMemory[gI + i];
					for (auto j = 0; j <= kByteMinOne; ++j)
					{
						const UInt32 shiftedBit = kByteMinOne - j;
						const UInt32 gfxMemoryIndex = gfxIndex + j;
						const UChar bitToSet = ((byteToSet & (1 << shiftedBit)) >> shiftedBit);
						const UChar existingByte = gGfx[gfxMemoryIndex];
						const UChar existingBit = ((existingByte & (1 << shiftedBit)) >> shiftedBit);
						if (existingBit != 0 && bitToSet != 0)
						{
							flagCollision = true;
						}
						gGfx[gfxMemoryIndex] ^= (bitToSet) ? 0xFF : 0x00;
					}
				}

				// Collision
				gV[0xF] = (flagCollision) ? 0x01 : 0x00;
				gDrawFlag = true;
				return EIncrementPC::Yes;
			},
			[](const UShort opCode)
			{
				switch (opCode & 0x00FF)
				{
					// EX9E
					// Skips the next instruction if the key stored in VX is pressed.
					case 0x009E:
					{
						const auto vx = getVX(opCode);
						if (isKeyPressed(vx))
						{
							incrementPC();
						}
						return EIncrementPC::Yes;
					}

					// EXA1
					// Skips the next instruction if the key stored in VX isn't pressed.
					case 0x00A1:
					{
						const auto vx = getVX(opCode);
						if (!isKeyPressed(vx))
						{
							incrementPC();
						}
						return EIncrementPC::Yes;
					}

					default:
						fail("Invalid opcode: ", opCode);
						return EIncrementPC::No;
				}
			},
			[](const UShort opCode)
			{
				switch (opCode & 0x00FF)
				{
					// FX07	
					// Sets VX to the value of the delay timer
					case 0x0007:
					{
						auto& vx = getVX(opCode);
						vx = gDelayTimer;
						return EIncrementPC::Yes;
					}

					// FX0A	
					// A key press is awaited, and then stored in VX.
					case 0x000A:
					{
						if (gKeyPress != kInvalidKey)
						{
							auto& vx = getVX(opCode);
							vx = gKeyPress;
							return EIncrementPC::Yes;
						}
						return EIncrementPC::No;
					}

					// FX15	
					// Sets the delay timer to VX.
					case 0x0015:
						gDelayTimer = getVX(opCode);
						return EIncrementPC::Yes;

					// FX18	
					// Sets the sound timer to VX.
					case 0x0018:
						gSoundTimer = getVX(opCode);
						return EIncrementPC::Yes;

					// FX1E	
					// Adds VX to I.[3]
					case 0x001E:
						gI += getVX(opCode);
						return EIncrementPC::Yes;

					// FX29	
					// Sets I to the location of the sprite for the character in VX.
					// Characters 0 - F(in hexadecimal) are represented by a 4x5 font.
					case 0x0029:
					{
						const auto vx = getVX(opCode);
						const MemoryMapRange& fontSetMemoryRange = kMemoryMapRange[static_cast<UChar>(MemoryMapIndex::FontSet)];
						gI = fontSetMemoryRange.mMin + (vx * kFontCharacterHeight);
						return EIncrementPC::Yes;
					}

					// FX33	
					// Stores the Binary - coded decimal representation of VX, with the most significant of three digits at the address in I, 
					// the middle digit at I plus 1, and the least significant digit at I plus 2.
					// (In other words, take the decimal representation of VX, place the hundreds digit in memory at location in I,
					// the tens digit at location I + 1, and the ones digit at location I + 2.)
					case 0x0033:
					{
						const auto vx = getVX(opCode);
						gMemory[gI] = vx / 100;
						gMemory[gI + 1] = (vx / 10) % 10;
						gMemory[gI + 2] = (vx % 10) % 10;
					}
					return EIncrementPC::Yes;

					// FX55	
					// Stores V0 to VX in memory starting at address I.
					case 0x0055:
					{
						const auto x = maskShift0F00(opCode);
						for (auto i = 0; i <= x; ++i)
						{
							gMemory[gI + i] = gV[i];
						}
						return EIncrementPC::Yes;
					}

					// FX65	
					// Fills V0 to VX with values from memory starting at address I.
					case 0x0065:
					{
						const auto x = maskShift0F00(opCode);
						for (auto i = 0; i <= x; ++i)
						{
							gV[i] = gMemory[gI + i];
						}
						return EIncrementPC::Yes;
					}

					default:
						fail("Invalid opcode: ", opCode);
						return EIncrementPC::No;
				}
			}
		};

		void initialise()
		{
			// init
			gPC = kPCStart;
			gOpCode = kDefaultOpCode;
			gI = kDefaultSpecialReg;
			gSP = kDefaultSpecialReg;
			gKeyPress = kInvalidKey;
			memset(gKeyPressCoolDown, 0, kSizeOfKeypressCooldownBuffer);

			// Seed rand
			const auto seed = random_device()();
			gRandGen.seed(seed);

			// load font from memory
			const MemoryMapRange& fontSetMemoryRange = kMemoryMapRange[static_cast<UChar>(MemoryMapIndex::FontSet)];
			memcpy(&gMemory[fontSetMemoryRange.mMin], gFontSet, sizeof(gFontSet));

			// Any platform specifics, resolution should be 2:1
			platformInit(kGFXWidth, kGFXHeight, kGFXWidth * kScreenScale, kGFXHeight * kScreenScale);

			gTimerTicksSinceLastUpdate = 0;
			gCycleTicksSinceLastUpdate = 0;
			gDrawFlag = false;
			gCycleUpdateRateModifierMS = 0;
		}

		void deInitialise()
		{
			platformDeInit();
		}

		void loadGame(const std::string& gameName)
		{ 
			// Read into prg memory
			ifstream stream;
			stream.open(gameName, ios::in | ios::binary);
			if (!stream.is_open())
			{
				fail("Failed to open game: ", gameName.c_str());
				return;
			}

			const MemoryMapRange& prgMemoryMapRange = kMemoryMapRange[static_cast<UChar>(MemoryMapIndex::PRG)];
			stream.seekg(0, ios::beg);
			stream.read(reinterpret_cast<char*>(&gMemory[prgMemoryMapRange.mMin]), prgMemoryMapRange.mMax - prgMemoryMapRange.mMin);
			stream.close();
		}
		
		bool canEmulateCycle()
		{
			return platformCanUpdate(gCycleTicksSinceLastUpdate, (kCycleUpdateRateBase + gCycleUpdateRateModifierMS));
		}
		void emulateCycle()
		{
			if (!canEmulateCycle())
			{
				return;
			}

			// Fetch
			UShort op;
			memcpy(&op, &gMemory[gPC], sizeof(UShort));
			op = ShortSwap(op);

			// Decode and Execute.
			// Return code will let us know if we need to increment the PC.
			if (gVM[maskShiftF000(op)](op) == EIncrementPC::Yes)
			{
				incrementPC();
			}
		}

		void draw()
		{
			platformDraw(reinterpret_cast<void*>(&gGfx[0]), kGFXWidth, kGFXHeight);
		}

		bool canUpdateTimers()
		{
			return platformCanUpdate(gTimerTicksSinceLastUpdate, kTimerUpdateRateMS);
		}

		EQuit pollInput()
		{
			Char shouldUpdateCycleRate = 0;
			EQuit quit = (platformPollInput(gKeyPress, shouldUpdateCycleRate)) ? EQuit::Yes : EQuit::No;

			// Update cycle update rate based on input (we can +/- this at runtime dependent on how well current game performs that way)
			if (shouldUpdateCycleRate < 0)
			{
				gCycleUpdateRateModifierMS += kCycleUpdateRateDelta;
			}
			else if(shouldUpdateCycleRate > 0)
			{
				if (gCycleUpdateRateModifierMS > kCycleUpdateRateDelta)
				{
					gCycleUpdateRateModifierMS -= kCycleUpdateRateDelta;
				}
			}

			return quit;
		}

		void updateTimers()
		{
			// The sound timer logic could trigger by being set directly.
			if (gSoundTimer > 0)
			{
				platformPlaySound();
			}
			else
			{
				platformStopSound();
			}

			// Timers are supposed to tick at 60Hz
			if (canUpdateTimers())
			{
				if (gDelayTimer > 0)
				{
					--gDelayTimer;
				}

				if (gSoundTimer > 0)
				{
					--gSoundTimer;
				}
			}
		}

		void updateAudio()
		{
			platformUpdateAudio();
		}

	} // namespace

	void mainLoop(const string& gameName)
	{
		initialise();
		loadGame(gameName);
		EQuit quit = EQuit::No;
		while (quit == EQuit::No)
		{
			emulateCycle();
			updateTimers();
			updateAudio();
			if (gDrawFlag)
			{
				draw();
				gDrawFlag = false;
			}
			quit = pollInput();
		}
		deInitialise();
	}

} // namespace SynchingFeeling
