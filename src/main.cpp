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

#ifdef DEBUGGING

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
		//coordinator.StartGame("C:/Program Files (x86)/StarCraft II/Maps/InterloperLE.SC2Map");
		coordinator.StartGame("Interloper LE");


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
struct ConnectionOptions
{
	int32_t GamePort;
	int32_t StartPort;
	std::string ServerAddress;
};

void ParseArguments(int argc, char *argv[], ConnectionOptions &connect_options)
{
	sc2::ArgParser arg_parser(argv[0]);
	arg_parser.AddOptions({
		{ "-g", "--GamePort", "Port of client to connect to", false },
		{ "-o", "--StartPort", "Starting server port", false },
		{ "-l", "--LadderServer", "Ladder server address", false },
	});
	arg_parser.Parse(argc, argv);
	std::string GamePortStr;
	if (arg_parser.Get("GamePort", GamePortStr)) {
		connect_options.GamePort = atoi(GamePortStr.c_str());
	}
	std::string StartPortStr;
	if (arg_parser.Get("StartPort", StartPortStr)) {
		connect_options.StartPort = atoi(StartPortStr.c_str());
	}
	arg_parser.Get("LadderServer", connect_options.ServerAddress);
}

//*************************************************************************************************
int main(int argc, char* argv[]) {
	ConnectionOptions Options;
	ParseArguments(argc, argv, Options);

	sc2::Coordinator coordinator;
	if (!coordinator.LoadSettings(argc, argv)) {
		return 1;
	}
	if (!coordinator.LoadSettings(argc, argv)) {
		return 1;
	}

	// Add the custom bot, it will control the players.
	CCBot bot;
	coordinator.SetParticipants({
		CreateParticipant(sc2::Race::Terran, &bot),
	});

	// Start the game.

	// Step forward the game simulation.
	std::cout << "Connecting to port " << Options.GamePort << std::endl;
	coordinator.Connect(Options.GamePort);
	coordinator.SetupPorts(2, Options.StartPort, false);
	// Step forward the game simulation.
	coordinator.JoinGame();
	coordinator.SetTimeoutMS(10000);
	std::cout << " Successfully joined game" << std::endl;
	while (coordinator.Update()) {
	}

	return 0;
}
#endif
