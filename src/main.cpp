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
#include <chrono>
#include <thread>

#include "CCBot.h"

#ifdef LADDEREXE
bool useDebug = false;
bool useAutoObserver = false;
#elif VSHUMAN
bool useDebug = false;
bool useAutoObserver = false;
#else
bool useDebug = false;
bool useAutoObserver = false;
#endif


#ifdef LADDEREXE
#include "LadderInterface.h"

int main(int argc, char* argv[])
{
	RunBot(argc, argv, new CCBot(), sc2::Race::Terran);

	return 0;
}
#elif VSHUMAN

class Human : public sc2::Agent {
public:
	void OnGameStart() final {
	}
};

struct ConnectionOptions
{
	sc2::Race HumanRace;
	std::string map;
	bool rt;
};

static void ParseArguments(int argc, char *argv[], ConnectionOptions &connect_options)
{
	sc2::ArgParser arg_parser(argv[0]);
	arg_parser.AddOptions({
		{ "-h", "--HumanRace", "Race of the human player"},
		{ "-m", "--Map", "MapName.SC2Map" },
		{ "-t", "--WithRealtime", "Play with realtime on (1) or off (0)" }
	});
	arg_parser.Parse(argc, argv);

	std::string race;
	arg_parser.Get("HumanRace", race);
	if (race == "Zerg")
	{
		connect_options.HumanRace = sc2::Zerg;
	}
	else if (race == "Protoss")
	{
		connect_options.HumanRace = sc2::Protoss;
	}
	else if (race == "Terran")
	{
		connect_options.HumanRace = sc2::Terran;
	}
	else
	{
		std::cout << std::endl << "Could not read race option. Please provide it via --HumanRace \"Zerg/Protoss/Terran\". Using default: Zerg" << std::endl << std::endl;
		connect_options.HumanRace = sc2::Zerg;
	}

	if (!arg_parser.Get("Map", connect_options.map))
	{
		std::cout << "Could not read map option. Please provide it via --Map \"InterloperLE.SC2Map\". Using default: InterloperLE.SC2Map" << std::endl << std::endl;
		connect_options.map = "InterloperLE.SC2Map";
	}
	std::string realTime;
	if (arg_parser.Get("WithRealtime", realTime))
	{
		if (realTime == "true" || realTime == "True" || realTime == "1")
		{

			std::cout << "Real time on. Not extensively tested." << std::endl << std::endl;
			connect_options.rt = true;
		}
		else
		{
			connect_options.rt = false;
		}
	}
	else
	{
		connect_options.rt = false;
	}
}


int main(int argc, char* argv[])
{
	ConnectionOptions Options;
	ParseArguments(argc, argv, Options);

	int stepSize = 1;
	// Add the custom bot, it will control the players.
	CCBot bot;
	Human human;
	sc2::Coordinator coordinator;
	if (!coordinator.LoadSettings(argc, argv))
	{
		std::cout << "Unable to find or parse settings." << std::endl;
		return 1;
	}
	// WARNING: Bot logic has not been thorougly tested on step sizes > 1
	//		  Setting this = N means the bot's onFrame gets called once every N frames
	//		  The bot may crash or do unexpected things if its logic is not called every frame
	coordinator.SetStepSize(stepSize);
	coordinator.SetRealtime(Options.rt);
	coordinator.SetMultithreaded(true);
	coordinator.SetParticipants({
		CreateParticipant(Options.HumanRace, &human),
		CreateParticipant(sc2::Race::Terran, &bot)
	});
	// Start the game.
	coordinator.LaunchStarcraft();
	coordinator.StartGame(Options.map);

	// Step forward the game simulation.
	while (coordinator.Update())
	{
	}
	coordinator.LeaveGame();
	std::this_thread::sleep_for(std::chrono::milliseconds(1000));
	return 0;
}
#else //DEBUG
int main(int argc, char* argv[])
{
	std::vector<std::string> maps = { "BelShirVestigeLE.SC2Map","ProximaStationLE.SC2Map","OdysseyLE.SC2Map","AscensiontoAiurLE.SC2Map","MechDepotLE.SC2Map","NewkirkPrecinctTE.SC2Map" };
	std::vector<sc2::Race> opponents = { sc2::Race::Terran, sc2::Race::Zerg, sc2::Race::Protoss};
	std::map<std::string, sc2::Point2D> mapScore;
	std::map<sc2::Race, sc2::Point2D> raceScore;
	
	while (true)
	{
		for (const auto & map : maps)
		{
			for (const auto & opponent : opponents)
			{
				// Add the custom bot, it will control the players.
				CCBot bot;
				//CCBot bot2;
				sc2::Coordinator coordinator;
				if (!coordinator.LoadSettings(argc, argv))
				{
					std::cout << "Unable to find or parse settings." << std::endl;
					return 1;
				}
				// WARNING: Bot logic has not been thorougly tested on step sizes > 1
				//		  Setting this = N means the bot's onFrame gets called once every N frames
				//		  The bot may crash or do unexpected things if its logic is not called every frame
				coordinator.SetStepSize(1);
				coordinator.SetRealtime(false);
				coordinator.SetMultithreaded(true);
				coordinator.SetParticipants({
					CreateParticipant(sc2::Race::Terran, &bot),
					CreateComputer(opponent, sc2::Difficulty::CheatInsane)
				});
				// Start the game.
				coordinator.LaunchStarcraft();
				coordinator.StartGame(map);

				// Step forward the game simulation.
				while (coordinator.Update())
				{
				}
				if (bot.Observation() && bot.Observation()->GetResults().front().result == sc2::GameResult::Win)
				{
					mapScore[map] += sc2::Point2D(1, 0);
					raceScore[opponent] += sc2::Point2D(1, 0);
				}
				else
				{
					mapScore[map] += sc2::Point2D(0, 1);
					raceScore[opponent] += sc2::Point2D(0, 1);
				}
				std::cout << std::endl;
				for (const auto & rs : raceScore)
				{
					std::cout << Util::GetStringFromRace(rs.first) << " = " << rs.second.x << " : " << rs.second.y << " (" << roundf(rs.second.x/(rs.second.x+ rs.second.y) * 100.0f) / 100.0f<<"%)"<< std::endl;
				}
				std::cout << std::endl;
				for (const auto & ms : mapScore)
				{
					std::cout << ms.first << " = " << ms.second.x << " : " << ms.second.y << " (" << roundf(ms.second.x / (ms.second.x + ms.second.y) * 100.0f) / 100.0f << "%)" << std::endl;
				}
				std::cout << std::endl << std::endl;
				coordinator.LeaveGame();
				std::this_thread::sleep_for(std::chrono::milliseconds(1000));
			}
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(5000));
	}
	return 0;
}
#endif