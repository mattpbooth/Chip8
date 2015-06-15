// Chip8EmuApp.cpp : Defines the entry point for the console application.
//
#include <iostream>
#include <string>
#include <tchar.h>

#include "Chip8Emu/Emu.h"

using namespace std;
using namespace SynchingFeeling;

int _tmain(int argc, _TCHAR *argv[])
{
	if (argc != 2)
	{
		cout << "Chip8Emu (Interpreter)" << endl;
		cout << " - Requires one argument, which should be the game to load." << endl;
	}
	else
	{
		wstring wideGame(argv[1]);
		mainLoop(string(wideGame.begin(), wideGame.end()).c_str());
	}
	return 0;
}

