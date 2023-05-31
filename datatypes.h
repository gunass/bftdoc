/*
 * datatypes.h
 *
 * This file contains misc data types & definitions
 *
 *  Created on: 10 Apr 2023
 *      Author: alex
 */

#ifndef DATATYPES_H_
#define DATATYPES_H_

#include <string>
#include <sys/time.h>

// Global constants
static const std::string _LOCALHOST = "127.0.0.1";
static const struct timeval _SHORT_TIMEOUT = {1,0};
static const struct timeval _LONG_TIMEOUT  = {2,0};
static const int _ELECTION_TIMEOUT_MAX = 3;

// Message protocol constants
static const char* _FIELD_SEPARATOR = ":";
static const char* _SIGNT_SEPARATOR = "$";
static const char* _VOTE_SEPARATOR = "%";
static const char* _CANDIDATE_NO_REPLY = "NOCANDO";
static const char* _CANDIDATE_YES_REPLY = "YESWECAND";
static const char* _NULL_FIELD = "NULL";

// Message headers, locked to 4 characters
static const char* _PING = "PING";
static const char* _MASTER_BROADCAST = "MSTR";
static const char* _CANDIDATE_REQUEST = "CAND";
static const char* _APPEND = "APND";
static const char* _DELETE = "DELT";
static const char* _FORCE_ELECTION = "ELCT";
static const char* _VIEW_DOC = "VIEW";
static const char* _EMPTY_DOC = "EMPT";
static const char* _REPLAY_REQUEST = "RPLY";

// Human readable message types (for clients)
static const std::string _VIEW_DOC_INPUT = "view";
static const std::string _APPEND_INPUT = "append";
static const std::string _DELETE_INPUT = "delete";
static const std::string _ELECT_INPUT = "elect";

// Types
struct _PEERID
        {int port;
    int up;};

struct _SIGNEDVOTE
{
    int port;
    bool yesNo;
    int term;
    std::string sig;
};

struct _MESSAGE {std::string messageType;
    std::string messagePort;
    std::string messageTerm;
    std::string messageBody;
    std::string messageSign;};

struct _LOGEVENT {
    int sequent;
    int term;
    std::string event;
    std::string data;
    int hash = 0;
    int clientPort;
    std::string clientSig;
};

struct _EVENT_WITH_SIG {
    std::string body;
    int clientPort;
    std::string clientSig;
};



// Enum types
enum _TOPOLOGY { // This will probably go unused
	REPLICA,
    BYZANTINE,
    CLIENT
};

enum _REPLICASTATE {
	FOLLOWER,
	CANDIDATE,
	LEADER
};



#endif /* DATATYPES_H_ */
