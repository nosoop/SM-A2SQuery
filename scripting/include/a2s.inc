#if defined _a2s_util_included
	#endinput
#endif
#define _a2s_util_included

#include <socket>

#define A2S_INFO_QUERY_DATA "\xFF\xFF\xFF\xFF\x54Source Engine Query"

// Constants representing key strings used for the output KeyValues from A2SInfo_DataToKeyValues.
#define A2S_INFO_PROTOCOL "protocol"
#define A2S_INFO_SERVER_NAME "server_name"
#define A2S_INFO_MAP_NAME "map_name"
#define A2S_INFO_GAME_DIR "game_dir"
#define A2S_INFO_GAME_NAME "game_name"
#define A2S_INFO_APPID "appid"
#define A2S_INFO_NUM_PLAYERS "num_players"
#define A2S_INFO_MAX_PLAYERS "max_players"
#define A2S_INFO_NUM_BOTS "bots"
#define A2S_INFO_SERVER_TYPE "server_type"
#define A2S_INFO_ENVIRONMENT "environment"
#define A2S_INFO_VISIBILITY "visibility"
#define A2S_INFO_VAC "vac"

enum EResponseData {
	ResponseData_Byte = 0,
	ResponseData_Short,
	ResponseData_String,
};

static const EResponseData[] A2S_INFO_STRUCT = {
	ResponseData_Byte,		// A2S_INFO_PROTOCOL
	ResponseData_String,	// A2S_INFO_SERVER_NAME
	ResponseData_String,	// A2S_INFO_MAP_NAME
	ResponseData_String,	// A2S_INFO_GAME_DIR
	ResponseData_String,	// A2S_INFO_GAME_NAME
	ResponseData_Short,		// A2S_INFO_APPID
	ResponseData_Byte,		// A2S_INFO_NUM_PLAYERS
	ResponseData_Byte,		// A2S_INFO_MAX_PLAYERS
	ResponseData_Byte,		// A2S_INFO_NUM_BOTS
	ResponseData_Byte,		// A2S_INFO_SERVER_TYPE
	ResponseData_Byte,		// A2S_INFO_ENVIRONMENT
	ResponseData_Byte,		// A2S_INFO_VISIBILITY
	ResponseData_Byte,		// A2S_INFO_VAC
};

/**
 * The key strings used in A2SInfo_DataToKeyValues().
 * The strings' positions in the array corresponds to the data's position in the datapack.
 */
static const char A2S_INFO_KV[][] = {
	A2S_INFO_PROTOCOL, A2S_INFO_SERVER_NAME, A2S_INFO_MAP_NAME,
	A2S_INFO_GAME_DIR, A2S_INFO_GAME_NAME, A2S_INFO_APPID,
	A2S_INFO_NUM_PLAYERS, A2S_INFO_MAX_PLAYERS, A2S_INFO_NUM_BOTS,
	A2S_INFO_SERVER_TYPE, A2S_INFO_ENVIRONMENT, A2S_INFO_VISIBILITY,
	A2S_INFO_VAC,
};

/**
 * Called when an A2S_INFO query has completed or failed.
 * 
 * @param response		An A2SInfo_DataPack containing the response to the A2S_INFO query,
 *						or null on failure.
 * @param data			Arbitrary data passed to the callback from the A2SInfo_Query function.
 */
typedef A2SInfo_QueryCallback = function void(A2SInfo_DataPack response, any data);

/**
 * A DataPack containing the converted response from A2S_INFO.
 * 
 * Position 0 in the datapack contains the byte representing the server protocol,
 * and values after it correspond to those similar in the response format.
 * See https://developer.valvesoftware.com/wiki/Server_queries#A2S_INFO
 */
methodmap A2SInfo_DataPack < DataPack {
	public A2SInfo_DataPack() {
		return view_as<A2SInfo_DataPack>(new DataPack());
	}
	public KeyValues ToKeyValues() {
		return A2SInfo_DataToKeyValues(this);
	}
	public int GetPositionByKey(const char[] key, bool caseSensitive = true) {
		for (int i = 0; i < sizeof(A2S_INFO_KV); i++) {
			if (StrEqual(key, A2S_INFO_KV[i], caseSensitive)) {
				return i;
			}
		}
		return -1;
	}
}

/**
 * Performs an A2S_INFO query on the server at the given host / port.
 *
 * @param host			The hostname or IP address of the server to query.
 * @param port			The port the service to be queried is hosted on.
 * @param callback		The callback function for the query.
 * 
 * @return false if the socket creation failed, true otherwise
 */
stock bool A2SInfo_Query(const char[] host, int port, A2SInfo_QueryCallback callback, any data = -1) {
	Handle socket = SocketCreate(SOCKET_UDP, A2SInfo_OnSocketError);
	
	if (socket != INVALID_HANDLE) {
		DataPack dataPack = new DataPack();
		dataPack.WriteFunction(callback);
		dataPack.WriteCell(data);
		
		SocketSetArg(socket, dataPack);
		
		SocketConnect(socket, A2SInfo_OnSocketConnect, A2SInfo_OnSocketReceive, A2SInfo_OnSocketDisconnect,
				host, port );
		
		return true;
	}
	return false;
}

/**
 * Called when an A2S_INFO query's socket fails for some reason.
 */
public int A2SInfo_OnSocketError(Handle socket, const int errorType, const int errorNum, DataPack dataPack) {
	// Failed.  No data to pass.
	// TODO figure out what data is returned when SocketCreate fails and ignore in this callback if it's the case
	
	dataPack.Reset();
	
	A2SInfo_QueryCallback callback = view_as<A2SInfo_QueryCallback>(dataPack.ReadFunction());
	any data = dataPack.ReadCell();
	
	A2SInfo_Call_QueryCallback(callback, null, data);
	
	delete socket;
}

/**
 * Called when an A2S_INFO query's socket connects.
 * The socket sends the query on a successful connect.
 */
public int A2SInfo_OnSocketConnect(Handle socket, any data) {
	SocketSend(socket, A2S_INFO_QUERY_DATA, 25);
}

/**
 * Called when an A2S_INFO query's socket receives the query response.
 * The function parses the response, writing the data out to a DataPack and running the callback.
 */
public int A2SInfo_OnSocketReceive(Handle socket, const char[] response, int responseSize, DataPack dataPack) {
	char buffer[256];
	int intBuffer;
	
	// Validate Simple Response Format Header
	int offset;
	do {
		int nextOffset = A2S_SocketResponse_ReadByte(response, responseSize, offset, intBuffer);
		if (intBuffer != '\xFF') {
			ThrowError("Expected byte %X at offset %d, got %X instead", '\xFF', offset + nextOffset, intBuffer);
		}
		offset += nextOffset;
	} while (offset < 4);
	
	// Validate A2S_INFO Response Header
	offset += A2S_SocketResponse_ReadByte(response, responseSize, offset, intBuffer);
	if (intBuffer != '\x49') {
		ThrowError("Expected byte %X at offset %d, got %X instead", '\x49', offset - 1, intBuffer);
	}
	
	// Begin parsing out the response and writing the values out to the datapack.
	A2SInfo_DataPack results = new A2SInfo_DataPack(); 
	
	for (int i = 0; i < sizeof(A2S_INFO_STRUCT); i++) {
		switch (A2S_INFO_STRUCT[i]) {
			case ResponseData_Byte: {
				offset += A2S_SocketResponse_ReadByte(response, responseSize, offset, intBuffer);
				results.WriteCell(intBuffer);
			}
			case ResponseData_Short: {
				offset += A2S_SocketResponse_ReadShort(response, responseSize, offset, intBuffer);
				results.WriteCell(intBuffer);
			}
			case ResponseData_String: {
				offset += A2S_SocketResponse_ReadString(response, responseSize, offset, buffer, sizeof(buffer));
				results.WriteString(buffer);
			}
		}
	}
	// TODO do we want to handle The Ship?  also version / EDF
	
	dataPack.Reset();
	
	// Perform callback
	A2SInfo_QueryCallback callback = view_as<A2SInfo_QueryCallback>(dataPack.ReadFunction());
	any data = dataPack.ReadCell();
	
	delete dataPack;
	
	A2SInfo_Call_QueryCallback(callback, results, data);
	
	delete socket;
}

public int A2SInfo_OnSocketDisconnect(Handle socket, DataPack dataPack) {
	delete socket;
}

/**
 * Reads a char array from a given offset into a result char array until a null terminator is found
 * or either array has reached their length.
 * 
 * @param response		A char array containing data.
 * @param responseSize	The length of the response.
 * @param offset		The offset to begin reading from the response data.
 * @param result		A char array to store the result of reading.
 * @param resultLength	The maximum size that the result array can hold.
 * 
 * @return Number of bytes read.
 */
int A2S_SocketResponse_ReadString(const char[] response, int responseSize, int offset, char[] result, int resultLength) {
	int written;
	do {
		result[written++] = response[offset];
	} while (response[offset] != '\x0' && written < resultLength && offset++ < responseSize);
	
	return written;
}

/**
 * Reads the byte value at the given offset.
 * 
 * @param response		A char array containing data.
 * @param responseSize	The length of the response.
 * @param offset		The offset to begin reading from the response data.
 * @param value			An int reference to store the read byte value.
 * 
 * @return Number of bytes read (1 if successful, 0 if not).
 */
int A2S_SocketResponse_ReadByte(const char[] response, int responseSize, int offset, int &value) {
	if (offset < responseSize) {
		value = response[offset++];
		return 1;
	} else {
		ThrowError("Offset to retrieve value is larger than the size of the response");
		return 0;
	}
}

/**
 * Reads the short value at the given offset.
 * 
 * @param response		A char array containing data.
 * @param responseSize	The length of the response.
 * @param offset		The offset to begin reading from the response data.
 * @param value			An int reference to store the read short value.
 * 
 * @return Number of bytes read (2 if successful).
 */
int A2S_SocketResponse_ReadShort(const char[] response, int responseSize, int offset, int &value) {
	if (offset + 2 < responseSize) {
		value = 0;
		for (int i = 0; i < 2; i++) {
			// least significant bit is first (e.g., for TF2 the values read are 0xB8, 0x1)
			value |= (response[offset + i] & 0xFF) << (8 * i);
		}
		return 2;
	}
	return 0;
}

/**
 * Performs the callback to the A2SInfo_QueryCallback function given when calling A2SInfo_Query.
 */
void A2SInfo_Call_QueryCallback(A2SInfo_QueryCallback callback, A2SInfo_DataPack response, any data) {
	Call_StartFunction(INVALID_HANDLE, callback);
	Call_PushCell(response);
	Call_PushCell(data);
	
	Call_Finish();
}

/**
 * Creates a KeyValues handle and stores data from an A2SInfo_DataPack.
 * 
 * @return A KeyValues handle if the DataPack is not null and has been converted, null if the DataPack
 * does not exist.
 */
stock KeyValues A2SInfo_DataToKeyValues(A2SInfo_DataPack dataPack) {
	// TODO save position?
	
	if (dataPack != null) {
		KeyValues kv = new KeyValues("s2s_info");
		
		dataPack.Reset();		
		for (int i = 0; i < sizeof(A2S_INFO_KV); i++) {
			switch (A2S_INFO_STRUCT[i]) {
				case ResponseData_Byte, ResponseData_Short: {
					kv.SetNum(A2S_INFO_KV[i], dataPack.ReadCell());
				}
				case ResponseData_String: {
					DataPack_ReadStringToKeyValue(dataPack, kv, A2S_INFO_KV[i]);
				}
			}
		}
		
		return kv;
	} else {
		ThrowError("Invalid A2SInfo_DataPack handle");
		return null;
	}
}

/**
 * Reads the next string from a DataPack and stores it as a value in a KeyValues handle with the given key.
 */
stock void DataPack_ReadStringToKeyValue(A2SInfo_DataPack dataPack, KeyValues keyValues, const char[] key) {
	char strBuffer[256];
	dataPack.ReadString(strBuffer, sizeof(strBuffer));
	keyValues.SetString(key, strBuffer);
}
