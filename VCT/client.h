#pragma once
#include "framework.h"
#include "util.h"

#include "include/Base64.h"
#include "include/json.hpp"
using json = nlohmann::json;

const std::vector<std::string> ranks = { "Unrated",
                                         "Unrated",
                                         "Unrated",
                                         "Iron 1",
                                         "Iron 2",
                                         "Iron 3",
                                         "Bronze 1",
                                         "Bronze 2",
                                         "Bronze 3",
                                         "Silver 1",
                                         "Silver 2",
                                         "Silver 3",
                                         "Gold 1",
                                         "Gold 2",
                                         "Gold 3",
                                         "Platinum 1",
                                         "Platinum 2",
                                         "Platinum 3",
                                         "Diamond 1",
                                         "Diamond 2",
                                         "Diamond 3",
                                         "Immortal 1",
                                         "Immortal 2",
                                         "Immortal 3",
                                         "Radiant" };

enum GameState
{
    NONE = 0,
    MENUS,
    INGAME,
    PREGAME
};

struct lockfile_t
{
    std::string name;
    std::string pid;
    int port;
    std::string password;
    std::string protocol;
};

struct player_t
{
    std::string puuid;
    std::string name;
    int level;
    int rank;
    int rr;
    int peakRank;
    std::string team;
    std::string agent;
    int leaderboardPos;
};

class Client
{
    lockfile_t lockfile;
    cpr::Header headers;
    std::string puuid;

public:
    GameState currentGameState;
    std::vector<std::tuple<std::string, int, int, int>> stats;

    Client()
    {
        if (this->parseLockfile())
        {
            this->getGameState();
        }
    }

    bool parseLockfile()
    {
        auto lockfileDir = std::string(getenv("LOCALAPPDATA")) + R"(\Riot Games\Riot Client\Config\lockfile)";
        auto lockfileStr = util::readFile(lockfileDir);
        if (!lockfileStr.empty())
        {
            auto data = util::split(lockfileStr, ":");
            if (data.size() == 5)
            {
                lockfile = { data[0], data[1], std::stoi(data[2]), data[3], data[4] };
                return true;
            }
        }

        return false;
    }

    auto local_api_get(std::string endpoint)
    {
        auto url = "https://127.0.0.1:" + std::to_string(lockfile.port) + endpoint;

        static cpr::SslOptions sslOpts = cpr::Ssl(cpr::ssl::VerifyPeer { false }); // NEEDED!

        return cpr::Get(
            cpr::Url { url }, sslOpts,
            cpr::Authentication { "riot", lockfile.password });
    }

    cpr::Header getHeaders()
    {
        if (headers.size() == 0)
        {
            auto res = local_api_get("/entitlements/v1/token");

            if (res.status_code == 200)
            {
                auto j = json::parse(res.text);

                puuid = j["subject"];
                headers = cpr::Header {
                    { "Authorization", "Bearer " + j["accessToken"].get<std::string>() },
                    { "X-Riot-Entitlements-JWT", j["token"].get<std::string>() },
                    { "X-Riot-ClientPlatform", "ew0KCSJwbGF0Zm9ybVR5cGUiOiAiUEMiLA0KCSJwbGF0Zm9ybU9TIjogIldpbmRvd3MiLA0KCSJwbGF0Zm9ybU9TVmVyc2lvbiI6ICIxMC4wLjE5MDQyLjEuMjU2LjY0Yml0IiwNCgkicGxhdGZvcm1DaGlwc2V0IjogIlVua25vd24iDQp9" },
                    { "X-Riot-ClientVersion", "release-04.05-23-687347" }, // TODO GET THIS AUTO!!
                    { "User-Agent", "ShooterGame/19 Windows/10.0.19042.1.256.64bit" }
                };
            }
        }

        return headers;
    }

    auto glz_api_get(std::string endpoint)
    {
        auto url = "https://glz-eu-1.eu.a.pvp.net" + endpoint;

        static cpr::SslOptions sslOpts = cpr::Ssl(cpr::ssl::VerifyPeer { false });

        return cpr::Get(
            cpr::Url { url }, sslOpts, getHeaders());
    }

    auto pd_api_get(std::string endpoint)
    {
        auto url = "https://pd.eu.a.pvp.net" + endpoint;

        static cpr::SslOptions sslOpts = cpr::Ssl(cpr::ssl::VerifyPeer { false });

        return cpr::Get(
            cpr::Url { url }, sslOpts, getHeaders());
    }

    auto pd_api_put(std::string endpoint, std::string body)
    {
        auto url = "https://pd.eu.a.pvp.net" + endpoint;

        static cpr::SslOptions sslOpts = cpr::Ssl(cpr::ssl::VerifyPeer { false });

        return cpr::Put(
            cpr::Url { url }, sslOpts, getHeaders(), cpr::Body { body },
            cpr::Header { { "Content-Type", "application/json" } });
    }

    std::string getActiveSeasonId()
    {
        auto url = "https://shared.eu.a.pvp.net/content-service/v3/content";

        static cpr::SslOptions sslOpts = cpr::Ssl(cpr::ssl::VerifyPeer { false });

        auto res = cpr::Get(
            cpr::Url { url }, sslOpts, getHeaders());

        if (res.status_code == 200)
        {
            auto j = json::parse(res.text);

            for (auto& [key, season] : j["Seasons"].items())
            {
                if (season["IsActive"].get<bool>())
                {
                    return season["ID"].get<std::string>();
                }
            }
        }

        return "";
    }

    std::string getCoregameMatchId()
    {
        auto res = glz_api_get("/core-game/v1/players/" + this->puuid);
        if (res.status_code == 200)
        {
            auto j = json::parse(res.text);
            return j["MatchID"].get<std::string>();
        }

        return "";
    }

    auto getCoregameStats()
    {
        auto matchId = this->getCoregameMatchId();
        if (!matchId.empty())
        {
            auto res = glz_api_get("/core-game/v1/matches/" + matchId);
            if (res.status_code == 200)
            {
                auto j = json::parse(res.text);
                return j;
            }
        }

        return json::object();
    }

    auto getMatchPlayers()
    {
        auto stats = this->getCoregameStats();

        static auto currentSeason = this->getActiveSeasonId();

        // auto queueType = stats["MatchmakingData"]["QueueID"].get<std::string>();

        // printf("%s\n", queueType.c_str());

        auto playersJ = stats["Players"];
        std::vector<std::string> puuids;
        std::vector<player_t> players;

        if (!playersJ.is_null())
        {
            for (auto& [key, player] : playersJ.items())
            {
                player_t player_data = {};

                player_data.puuid = player["Subject"].get<std::string>();
                player_data.agent = player["CharacterID"].get<std::string>();
                player_data.level = player["PlayerIdentity"]["AccountLevel"].get<int>();
                player_data.team = player["TeamID"].get<std::string>();

                puuids.push_back(player_data.puuid);
                players.push_back(player_data);
            }

            // std::reverse(puuids.begin(), puuids.end());
            json puuidsJ(puuids);

            auto res = pd_api_put("/name-service/v2/players", puuidsJ.dump());
            if (res.status_code == 200)
            {
                // printf("%s\n", res.text.c_str());
                auto j = json::parse(res.text);

                for (auto& [key, nameData] : j.items())
                {
                    for (auto& player : players)
                    {
                        if (player.puuid == nameData["Subject"].get<std::string>())
                        {
                            player.name = nameData["GameName"].get<std::string>() + "#" + nameData["TagLine"].get<std::string>();

                            if (!currentSeason.empty())
                            {
                                auto res = pd_api_get("/mmr/v1/players/" + player.puuid);
                                if (res.status_code == 200)
                                {
                                    auto j = json::parse(res.text);

                                    // printf("%s\n", j.dump().c_str());

                                    auto data = j["QueueSkills"]["competitive"]["SeasonalInfoBySeasonID"][currentSeason];

                                    auto rankTierJ = data["CompetitiveTier"];
                                    auto rankedRatingJ = data["RankedRating"];
                                    auto leaderboardPositionJ = data["LeaderboardRank"];

                                    if (!rankTierJ.is_null())
                                    {
                                        player.rank = rankTierJ.get<int>();
                                    }

                                    if (!rankedRatingJ.is_null())
                                    {
                                        player.rr = rankedRatingJ.get<int>();
                                    }

                                    if (!leaderboardPositionJ.is_null())
                                    {
                                        player.leaderboardPos = leaderboardPositionJ.get<int>();
                                    }

                                    // PEAK RANK
                                }
                            }

                            break;
                        }
                    }
                }
            }
        }

        return players;
    }

    auto parseStats()
    {
        auto players = this->getMatchPlayers();

        if (players.size() > 0)
        {
            stats.clear();

            for (auto& player : players)
            {
                printf("Name: %s | Rank: %s | RR: %i | Leaderboard Position: %i\n", player.name.c_str(), ranks[player.rank].c_str(), player.rr, player.leaderboardPos);

                // toRender.push_back(rankIcons[rankTier]);

                stats.push_back({ player.name, player.rank, player.rr, player.leaderboardPos });
            }
        }
    }

    GameState getGameState()
    {
        if (headers.size() == 0)
        {
            this->getHeaders();
        }

        if (headers.size() != 0)
        {
            auto res = local_api_get("/chat/v4/presences");

            if (res.status_code == 200)
            {
                auto j = json::parse(res.text);
                auto presences = j["presences"];
                for (auto& [key, presence] : presences.items())
                {
                    if (presence["puuid"] == this->puuid)
                    {
                        std::string privateDecoded;
                        Base64::Decode(presence["private"].get<std::string>(), privateDecoded);
                        auto privateJson = json::parse(privateDecoded);

                        auto sessionLoopState = privateJson["sessionLoopState"].get<std::string>();

                        switch (util::str2int(sessionLoopState.c_str()))
                        {
                        case util::str2int("MENUS"):
                            return GameState::MENUS;
                        case util::str2int("INGAME"):
                            return GameState::INGAME;
                        case util::str2int("PREGAME"):
                            return GameState::PREGAME;
                        default:
                            return GameState::NONE;
                        }
                    }
                }
            }
        }

        return GameState::NONE;
    }

    void onStateChanged(GameState newState)
    {
        this->currentGameState = newState;

        // system("CLS");

        switch (newState)
        {
        case GameState::INGAME:
        {
            printf("Ingame!\n");
            parseStats();
            break;
        }

        case GameState::PREGAME:
            printf("Pregame!\n");
            break;
        case GameState::MENUS:
            printf("Menus!\n");
            break;
        }
    }
};
