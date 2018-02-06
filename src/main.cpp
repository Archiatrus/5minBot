#include "sc2api/sc2_api.h"
#include "sc2lib/sc2_lib.h"
#include "sc2utils/sc2_manage_process.h"
#include "sc2utils/sc2_arg_parser.h"
#include "rapidjson/document.h"
#include "JSONTools.h"
#include "Util.h"

#include <iostream>
#include <string>
#include <random>
#include <cmath>

#include "CCBot.h"
#include "LadderInterface.h"


#ifndef LADDEREXE
bool useDebug = true;
bool useAutoObserver = false;
#else
bool useDebug = false;
bool useAutoObserver = true;
#endif

#ifndef LADDEREXE

int main(int argc, char* argv[]) 
{
	int stepSize = 1;
	while (true)
	{
		// Add the custom bot, it will control the players.
		CCBot bot;
		
		sc2::Coordinator coordinator;
		if (!coordinator.LoadSettings(argc, argv))
		{
			std::cout << "Unable to find or parse settings." << std::endl;
			return 1;
		}
		// WARNING: Bot logic has not been thorougly tested on step sizes > 1
		//          Setting this = N means the bot's onFrame gets called once every N frames
		//          The bot may crash or do unexpected things if its logic is not called every frame
		coordinator.SetStepSize(stepSize);
		coordinator.SetRealtime(false);
		coordinator.SetMultithreaded(true);
		coordinator.SetParticipants({
			CreateParticipant(sc2::Race::Terran, &bot),
			//sc2::PlayerSetup(sc2::PlayerType::Observer,Util::GetRaceFromString(enemyRaceString)),
			CreateComputer(sc2::Race::Protoss, sc2::Difficulty::CheatInsane)
		});
		// Start the game.
		coordinator.LaunchStarcraft();
		//coordinator.StartGame("C:/Program Files (x86)/StarCraft II/Maps/AcolyteLE.SC2Map");
		//coordinator.StartGame("Interloper LE");
		coordinator.StartGame("Proxima Station LE");


		// Step forward the game simulation.
		while (coordinator.Update())
		{
		}
		if (bot.Control()->SaveReplay("C:/Users/D/Documents/StarCraft II/Accounts/115842237/2-S2-1-1338490/Replays/Multiplayer/asdf.Sc2Replay"))
		{
			std::cout << "REPLAY SUCESS" << "replay/asdf.Sc2Replay" << std::endl;
		}
		else
		{
			std::cout << "REPLAY FAIL" << "replay/asdf.Sc2Replay" << std::endl;
		}
		coordinator.LeaveGame();
		Sleep(1000);
	}
    return 0;
}

#else
//*************************************************************************************************
int main(int argc, char* argv[])
{
	RunBot(argc, argv, new CCBot(),sc2::Race::Terran);

	return 0;
}
#endif
