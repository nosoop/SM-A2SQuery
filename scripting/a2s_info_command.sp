/**
 * Sample plugin for the A2S include
 */
#pragma semicolon 1
#include <sourcemod>
#include <socket>

#pragma newdecls required
#include <a2s>

#define PLUGIN_VERSION "1.0.0"
public Plugin myinfo = {
    name = "A2S_INFO Query Command",
    author = "nosoop",
    description = "Provides a command to perform A2S_INFO queries.",
    version = PLUGIN_VERSION,
    url = "https://github.com/nosoop/SM-A2SQuery"
}

public void OnPluginStart() {
	RegAdminCmd("sm_a2s_info_query", AdminCmd_A2SInfoQuery, ADMFLAG_ROOT,
			"Performs an A2S_INFO query on a server.");
}

public Action AdminCmd_A2SInfoQuery(int client, int argc) {
	int port = 27015;
	
	if (argc < 2) {
		if (argc < 1) {
			ReplyToCommand(client, "Usage:  sm_a2s_info_query [host] [port]");
			return Plugin_Handled;
		}
	} else {
		char strPort[8];
		GetCmdArg(2, strPort, sizeof(strPort));
		
		port = StringToInt(strPort);
		port = port < 0 ? 0 : ( port > 65535 ? 65535 : port );
	}
	
	char host[PLATFORM_MAX_PATH];
	GetCmdArg(1, host, sizeof(host));
	
	if (A2SInfo_Query(host, port, A2SInfo_QueryCallback, client)) {
		if (GetCmdReplySource() == SM_REPLY_TO_CHAT) {
			PrintToChat(client, "See console for output.");
		}
	} else {
		ReplyToCommand(client, "Could not perform the query.");
	}
	return Plugin_Handled;
}

public void A2SInfo_QueryCallback(A2SInfo_DataPack response, int client) {
	if (response != null) {
		char serverName[256], mapName[256], gameDir[16], gameName[64];
		char serverType[32], environment[32];
		
		KeyValues kv = response.ToKeyValues();
		
		kv.GetString(A2S_INFO_SERVER_NAME, serverName, sizeof(serverName));
		PrintToConsole(client, "hostname: %s", serverName);
		
		kv.GetString(A2S_INFO_MAP_NAME, mapName, sizeof(mapName));
		PrintToConsole(client, "map     : %s", mapName);
		
		kv.GetString(A2S_INFO_GAME_DIR, gameDir, sizeof(gameDir));
		kv.GetString(A2S_INFO_GAME_NAME, gameName, sizeof(gameName));
		PrintToConsole(client, "game    : %s (game dir: %s)", gameName, gameDir);
		
		PrintToConsole(client, "appid   : %d", kv.GetNum(A2S_INFO_APPID) );
		
		PrintToConsole(client, "players : %d total, %d bots (%d max)",
				kv.GetNum(A2S_INFO_NUM_PLAYERS), kv.GetNum(A2S_INFO_NUM_BOTS),
				kv.GetNum(A2S_INFO_MAX_PLAYERS) );
		
		A2SInfo_GetEnvironment(kv.GetNum(A2S_INFO_ENVIRONMENT), environment, sizeof(environment));
		A2SInfo_GetServerType(kv.GetNum(A2S_INFO_SERVER_TYPE), serverType, sizeof(serverType));
		PrintToConsole(client, "type    : %s (%s)", environment, serverType);
		
		PrintToConsole(client, "visible : %s", kv.GetNum(A2S_INFO_VISIBILITY) == 0 ? "public" : "private");
		
		PrintToConsole(client, "vac     : %s", kv.GetNum(A2S_INFO_VAC) == 1 ? "true" : "false");
		
		delete kv;
	} else {
		PrintToConsole(client, "Could not get response.");
	}
	delete response;
}

void A2SInfo_GetEnvironment(char environment, char[] buffer, int size) {
	strcopy(buffer, size, "Unknown Environment");
	switch (environment) {
		case 'l': {
			strcopy(buffer, size, "Linux");
		}
		case 'w': {
			strcopy(buffer, size, "Windows");
		}
		case 'm', 'o': {
			strcopy(buffer, size, "Mac");
		}
	}
}

void A2SInfo_GetServerType(char type, char[] buffer, int size) {
	strcopy(buffer, size, "Unknown Server Type");
	switch (type) {
		case 'd': {
			strcopy(buffer, size, "Dedicated Server");
		}
		case 'l': {
			strcopy(buffer, size, "Listen Server");
		}
		case 'p': {
			// note:  TF2's relays also respond as if it was a dedicated server?
			strcopy(buffer, size, "SourceTV Relay");
		}
	}
}
