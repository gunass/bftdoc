/*
 * bftpeer.cpp
 *
 *  Created on: 10 Apr 2023
 *      Author: alex
 */

#include "bftpeer.h"
#include "datatypes.h"
#include <iostream>
#include <unistd.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <arpa/inet.h>

using namespace std;

//! @brief Constructor, init with port and topology (default 0/CONNECTED)
BftPeer::BftPeer(int lPeerPort, _TOPOLOGY lTopology)
{
	peerId = {lPeerPort, 1};
	topology = lTopology;
	state = FOLLOWER;
    masterId = {0, 0};
    logEvents = {};
	connectPeers();
    term = 0;
    ut = util(peerId, &term);
    secretKey = ut.generate_secret_key(lPeerPort);
}

//! @brief destructor
BftPeer::~BftPeer()
{
	//stub
}

//! @brief  Populate the vector of connected peers, depending on topology
void BftPeer::connectPeers()
{
	for( auto i = PEERPORT_MIN; i <= PEERPORT_MAX; ++i )
    {
        _PEERID newPeer{i, 0};
        connectedPeers.push_back(newPeer);
    }
}

//! @brief  Accept a message, process, reply
void BftPeer::handle_message(int inSock, const _MESSAGE& message)
{

    if( message.messagePort == to_string(masterId.port) )
    {
        masterId.up = 1; // Re-confirm the master on every message
    }

    if( !strcmp(message.messageType.c_str(), _APPEND)
        || !strcmp(message.messageType.c_str(), _DELETE)
        || !strcmp(message.messageType.c_str(), _FORCE_ELECTION)
        || !strcmp(message.messageType.c_str(), _VIEW_DOC))
    {
        handle_message_client(inSock, message);
        return;
    }

    if( !strcmp(message.messageType.c_str(), _PING) )
    {
        send(inSock, _PING, strlen(_PING), 0);
        return;
    }

    if( !strcmp(message.messageType.c_str(), _CANDIDATE_REQUEST) )
    {
        handle_message_candidate(inSock, message);
        return;
    }
    if( !strcmp(message.messageType.c_str(), _MASTER_BROADCAST) )
    {
        handle_message_master(inSock, message);
        return;
    }
    if( !strcmp(message.messageType.c_str(), _REPLAY_REQUEST) )
    {
        if( masterId.port == peerId.port )
        {
            handle_message_replay(inSock, message);
            return;
        }
        return;
    }
}

//! @brief   Start a blocking server socket and pass messages to processor
int startServer(BftPeer* peer)
{
	peer->myServerSock = socket(AF_INET, SOCK_STREAM, 0);
	if( peer->myServerSock < 0 )
	{
		peer->ut.db("Socket creation failed, exiting");
		return 1;
	}

	// Set addressing parameters
	struct sockaddr_in sockAddr;
	int addressLength = sizeof(sockAddr);
	memset(&sockAddr, 0, addressLength);
	sockAddr.sin_family = AF_INET;
	sockAddr.sin_addr.s_addr = INADDR_ANY;
	sockAddr.sin_port = htons(peer->peerId.port);

	// Attempt to bind
	int bindSuccess = ::bind(peer->myServerSock, (struct sockaddr*)&sockAddr, addressLength);
	if( bindSuccess < 0 )
	{
		peer->ut.db("Bind failed, exiting");
		return 1;
	}


	// Listen
	int listenSuccess = listen(peer->myServerSock, SOMAXCONN);
	if( listenSuccess < 0 )
	{
		peer->ut.db("Listen failed, exiting");
		return 1;
	}


	// Do the server thing
	int inSock;
	int valread;
	while ( true )
	{
		// Blocking accept
        char buffer[1024] = {0};
		inSock = accept(peer->myServerSock, (struct sockaddr*)&sockAddr, (socklen_t*)&addressLength);
		valread = read(inSock, buffer, 1024);

        if(valread <= 0)
        {
            peer->ut.db("Bad message. Ignoring");
            continue;
        }

		// Unmarshal the message and its header
        // XXXX:YYYY:Z*
        // X = message type
        // Y = sending peer's ID/port
        // Z = message body if there is one
        peer->ut.tr("Received : " + string(buffer));
        _MESSAGE m = peer->ut.unmarshal_message_safe(buffer);

        bool verifiedMessage = peer->ut.verify_message(m);

        if( !verifiedMessage )
        {
            peer->ut.db("WARNING: Message could not be verified and was ignored:");
            peer->ut.db("    " + peer->ut.message_to_string(m));
            continue;
        }

        peer->handle_message(inSock, m);
	}
}

//! @brief If we are the master, send back a log event for the peer
void BftPeer::handle_message_replay(int inSock, const _MESSAGE& message)
{
    int replayIndex = stoi(message.messageTerm); // Term field is reused here for log sequent
    ut.db("Received a replay request for index " + message.messageTerm);
    if(logEvents.size() < replayIndex )
    {   // Replay request exceeds the master's log size, so it cannot be legitimate
        ut.db("Replay request is oversized, ignoring");
        return;
    }
    // Find the log event matching the sequent
    auto maybe = ut.find_sequent_in_log(logEvents, replayIndex);
    if( maybe.term == -1 )
    {   // If a matching log entry is not found, must ignore the message
        ut.db("Replay request is missing from our log, ignoring " + message.messageTerm);
        return;
    }
    ut.db("Sending log event: " + ut.log_to_string(maybe));
    unicast_log_event_as_master(maybe, stoi(message.messagePort));
}

void BftPeer::unicast_log_event_as_master(const _LOGEVENT& logEvent, int peerPort)
{
    // Send the replay
    string message = ut.compose_message(logEvent.event.c_str(),
                                        logEvent.term,
                                       (to_string(logEvent.sequent) +
                                        _FIELD_SEPARATOR +
                                        logEvent.data +
                                        _FIELD_SEPARATOR +
                                        to_string(logEvent.clientPort) +
                                        _FIELD_SEPARATOR +
                                        logEvent.clientSig));

    _PEERID toSend = {peerPort, 1};
    ut.send_receive(toSend, message, nullptr);
}

//! @brief Send the event to the selected group of peers
void BftPeer::multicast_log_event_as_master(const _LOGEVENT& event, const vector<_PEERID>& toSend)
{
    for (auto i: toSend)
    {
        if (i.port == peerId.port) continue;
        unicast_log_event_as_master(event, i.port);
    }
}

//! @brief Handle a message from a prospective candidate
void BftPeer::handle_message_candidate(int inSock, const _MESSAGE& message)
{
    ut.db("Peer " + message.messagePort + " is standing for election.");
    const char* voteResult = _CANDIDATE_NO_REPLY;

    if( byz() ) // Randomly send yes/no responses, do not check anything
    {
        string reply;
        if( flip() )
        {
            ut.db("### BYZ: Randomly rejecting vote");
            voteResult = _CANDIDATE_NO_REPLY;
        }
        else
        {
            ut.db("### BYZ: Randomly confirming vote");
            voteResult = _CANDIDATE_YES_REPLY;
        }
    }
    else if( masterId.up )
    {
        // This node believes the master is alive, so reject vote
        ut.db("We currently believe the master is " + to_string(masterId.port) + " so we reject the vote.");
        voteResult = _CANDIDATE_NO_REPLY;
        voteInProgress = false;
    }
    else if( voteInProgress )
    {
        // This node believes the master is down but an election is ongoing, so reject vote
        ut.db("Election is ongoing, so rejecting the vote.");
        voteResult = _CANDIDATE_NO_REPLY;
    }
    else if( stoi(message.messageTerm) <= term )
    {
        // This node believes the term in the candidate message is stale, so reject vote
        ut.db("This election is for an outdated term ("+message.messageTerm+"<="+to_string(term)+"), so rejecting the vote.");
        voteResult = _CANDIDATE_NO_REPLY;
        voteInProgress = false;
    }
    else
    {
        // This node believes the vote is legitimate, and will support it.
        ut.db("Supporting the vote.");
        voteResult = _CANDIDATE_YES_REPLY;
        voteInProgress = true;
    }

    string reply = ut.compose_message_vote(voteResult);
    send(inSock, reply.c_str(), reply.length(), 0);
}

//! @brief Handle a master announce message. Only the first announce is processed
void BftPeer::handle_message_master(int inSock, const _MESSAGE& message)
{
    if( stoi(message.messagePort) == masterId.port )
    {
        // This is our known master - reconfirm up and record a local timestamp
        masterId.up = 1;
        ::time(&lastMasterAnnounceSecs);
        return;
    }

    ut.db("Peer " + message.messagePort + " has announced it is the master.");
    bool verifiedElection = verify_votes(message.messageBody);

    if( !verifiedElection )
    {
        ut.db("Peer's votes are illegitimate or insufficient to beat the current master");
        voteInProgress = false;
        return;
    }
    else if( stoi(message.messageTerm) < term )
    {
        ut.db("Peer's vote announcement is outdated, discarding (" + message.messageTerm + ")");
        voteInProgress = false;
        return;
    }
    else if( stoi(message.messageTerm) == term && state == CANDIDATE )
    {
        ut.db("Peer's vote announcement was faster ("+message.messageTerm+"), halting the election");
        state = FOLLOWER;
        term = stoi(message.messageTerm);
    }
    else if( masterId.port == peerId.port )
    {
        ut.db("We are the master, so ignoring");
        return;
    }
    else if( masterId.up )
    {
        ut.db("The master is known to be up, so ignoring");
        return;
    }

    voteInProgress = false;

    term = stoi(message.messageTerm);
    masterId.port = stoi(message.messagePort);
    masterId.up = 1;
    if (masterId.port != peerId.port)
    {
        state = FOLLOWER;
    }
}

//! @brief Threaded method which rapidly requests all missing log events
//! @param topSequent peer's latest successfully received sequent, always -1 if log empty
//! @param events Event log of peer
//! @param masterId _PEERID struct for master
//! @param ut Pointer to peer's utility class. Sadly, nonstatic
void request_all_replays(int topSequent, const vector<_LOGEVENT>& events, _PEERID masterId, util* ut)
{
    vector<int> missing;

    // This could be O(n) ;)
    for( int i = 0; i < topSequent; i++)
    {
        _LOGEVENT maybe = ut->find_sequent_in_log(events, i);
        if( maybe.term == -1 )
        {
            missing.push_back(i);
        }
    }

    ut->db("Found " + to_string(missing.size()) + " missing log events.");

    for( auto i : missing )
    {
        string msg = ut->compose_message(_REPLAY_REQUEST, i, "");
        ut->db("Requesting: " + msg);
        ut->send_receive(masterId, msg, nullptr);
    }
}

//! @brief Ask the master for a replay. Called if we notice inconsistent logs
void BftPeer::request_replay(int topSequent)
{
    thread replayRequester(request_all_replays, topSequent, logEvents, masterId, &ut);
    replayRequester.detach();
}

//! @brief Handle a message from a client or a relayed event from the master
void BftPeer::handle_message_client(int inSock, const _MESSAGE& message)
{
    if( stoi(message.messagePort) == masterId.port )
    {
        process_master_log_event(inSock, message);
    }
    // This message has come directly from a peer
    if( stoi(message.messagePort) >= CLIENTPORT_MIN && stoi(message.messagePort) <= CLIENTPORT_MAX )
    {
        // Client has requested to view the document at this replica
        if( !strcmp(message.messageType.c_str(), _VIEW_DOC) )
        {
            process_client_view_request(inSock, message);
        }

        // Client has requested to force an election
        if( !strcmp(message.messageType.c_str(), _FORCE_ELECTION) )
        {
            process_client_elect_request(inSock, message);
        }

        // This node is the master, so sequence the request and broadcast it to all peers
        if( masterId.port == peerId.port )
        {
            process_client_request_as_master(inSock, message);
        }
        else // This node is not the master, so if it's an editing request, pass it to the master
        {
            relay_message_to_master(inSock, message);
        }
    }
}

//! @brief As a peer, process an AppendEntries log event announce from master
void BftPeer::process_master_log_event(int inSock, const _MESSAGE& message)
{
    // This log event has been sent by the master (or at least claimed)
    ut.db("Log event received from master: " + ut.message_to_string(message));

    // Get the subfields
    int eventSequent = stoi(message.messageBody.substr(0,1));

    // Determine if the log event is greater than expected
    if( eventSequent > lastSequent + 1 )
    {
        ut.db("WARNING: received event out of sequence ("+to_string(eventSequent)+")");
        ut.db("    Requesting a replay of missed log events");
        request_replay(eventSequent);
    }
    // Determine if the log event is smaller than expected
    else if( eventSequent <= lastSequent )
    {
        // Must assume event is a duplicate
        ut.db("WARNING: received event for sequence number " + to_string(eventSequent) + " is old. Replay?");
    }
    // Determine if the sequent already exists in log
    else if( ut.find_sequent_in_log(logEvents, eventSequent).term != -1 )
    {
        // This should never be triggered and signifies a critical error, kept only for safety
        ut.db("ERROR: Duplicate detected, ignore");
        return;
    }

    string eventData = message.messageBody.substr(2, message.messageBody.length());

    _EVENT_WITH_SIG ews = ut.unmarshal_sig_from_event_safe(eventData); // Extract the client signature from the master log event

    //Verify the client's signature for this message
    _MESSAGE messageAsSent = {
            message.messageType,
            to_string(ews.clientPort),
            "0",
            ews.body,
            ews.clientSig
    };

    if( !ut.verify_message(messageAsSent) )
    {
        ut.db("ERROR: log event could not be verified as legitimate");
        ut.db("Original message : " + ut.message_to_string(message));
        ut.db("Reconstructed    : " + ut.message_to_string(messageAsSent));
        return;
    }

    ut.sanitise(ews.body);
    if( ews.body.length() <= 0)
    {
        ut.db("ERROR: received event is empty");
        return;
    }
    _LOGEVENT event = {eventSequent,
                       stoi(message.messageTerm),
                       message.messageType.c_str(),
                       ews.body.c_str(),
                       0,
                       ews.clientPort,
                       ews.clientSig};

    // Apply the event (note this function will buffer nonsequential events)
    apply_event(event);
}

//! @brief As a peer, process a client election request (not logged)
void BftPeer::process_client_elect_request(int inSock, _MESSAGE message)
{
    // Immediately declare the master down and call an election, even if we are the master
    ut.db("Client has requested an election");
    masterId = {0,0};
    state = CANDIDATE;
    send(inSock, _PING, strlen(_PING), 0);
}

//! @brief As a peer, process a client view request (not logged)
void BftPeer::process_client_view_request(int inSock, _MESSAGE message)
{
    ut.db("View request received, sending document: " + document);
    if( document.empty() ) // Can't send an empty string, client will think we have failed
    {
        send(inSock, _EMPTY_DOC, strlen(_EMPTY_DOC), 0);
        return;
    }
    send(inSock, document.c_str(), document.length(), 0);
}

//! @brief As the master, verify and process the client's request and multicast it to all peers
void BftPeer::process_client_request_as_master(int inSock, _MESSAGE message)
{
    if ( !process_client_request(message) )
    {
        ut.db("Acknowledging message with " + string(_PING));
        send(inSock, _PING, 4, 0);

        // Now relay the message to all peers
        _LOGEVENT event = {lastSequent,
                           term,
                           message.messageType,
                           message.messageBody,
                           0,
                           stoi(message.messagePort),
                           message.messageSign};

        multicast_log_event_as_master(event, connectedPeers);
        return;
    }
}

void BftPeer::relay_message_to_master(int inSock, _MESSAGE message)
{
    if( !strcmp(message.messageType.c_str(), _APPEND) || !strcmp(message.messageType.c_str(), _DELETE) )
    {
        // If we are not the master, resend the message to the master
        ut.db("Relaying client message to master");

        // Dodgy string reformat of message
        string relayMessage = ut.message_to_string(message);

        // Byzantine action: modify message in transit
        if( byz() )
        {
            ut.db("### BYZ: modifying message in transit");
            if( flip() )
            {
                // Substitute a command to delete the entire document
                message.messageType = _DELETE;
                message.messageType = document.length();
                relayMessage = ut.message_to_string(message);
            }
            else
            {
                // Vandalise the document
                message.messageBody = "Haha! Can't catch me!";
                relayMessage = ut.message_to_string(message);
            }
            ut.send_receive(masterId, relayMessage, nullptr);
            // Ping back to the client so they think it succeeded
            send(inSock, _PING, strlen(_PING), 0);
            return;
        }

        char buffer[1024];
        if (ut.send_receive(masterId, relayMessage, buffer))
        {
            ut.db("Failed to relay message to master");
            //masterId.up = 0;
            // Notify peer that master has failed?
            send(inSock, _NULL_FIELD, strlen(_NULL_FIELD), 0);
            return;
        }

        // If communicating with master was OK, reply with whatever the master replies with
        ut.db("Relaying master message to client: " + string(buffer));
        send(inSock, buffer, strlen(buffer), 0);
        return;
    }
}

//! @brief Hash and insert new log event
void BftPeer::put_in_log(_LOGEVENT logEvent)
{
    if(logEvents.empty())
    {
        logEvent.hash = ut.incremental_hash_init(logEvent);
    }
    else
    {
        _LOGEVENT previous = ut.find_sequent_in_log(logEvents, lastSequent);
        if( previous.term == -1 )
        {
            ut.db("Critical error: previous log event was not found");
            return;
        }
        logEvent.hash = ut.incremental_hash(logEvents.back(), logEvent);
    }

    ut.db("Appending log event " + ut.log_to_string(logEvent));
    logEvents.push_back(logEvent);
    lastSequent = logEvent.sequent;
}

//! @brief As the MASTER, process the client's request
//! @return 0 if request OK, 1 otherwise
int BftPeer::process_client_request(_MESSAGE message)
{
    ut.sanitise(message.messageBody);
    ut.db("Log event received from a client. Processing...");

    if( !strcmp(message.messageType.c_str(), _DELETE) && stoi(message.messageBody) > document.length() )
    {
        ut.db("WARNING: delete request is invalid, ignoring");
        return 1;
    }
    if( message.messageBody.length() <= 0 )
    {
        ut.db("Message has length 0, ignoring");
        return 1;
    }

    _LOGEVENT event = {lastSequent + 1, // Create the log event with client port/sign details.
                       term,                    // Note the sig has already been verified by this node.
                       message.messageType,
                       message.messageBody,
                       0,
                       stoi(message.messagePort),
                       message.messageSign};
    apply_event(event);

    return 0;
}

//! @brief Attempt to apply the given event to the document
void BftPeer::apply_event(_LOGEVENT event)
{
    // Test if sequential
    if( event.sequent > lastSequent + 1 )
    {
        ut.db("Nonsequential event " + to_string(event.sequent) + " received, buffering");
        logEventsBuffer.push_back(event);
        return;
    }

    // Event is valid and sequential. Process accordingly
    if( string(_DELETE) == event.event )
    {
        if( stoi(event.data) > document.length() )
        {
            ut.db("ERROR: delete event is too long to process, ignoring");
            return;
        }
        document = document.substr(0, document.length() - stoi(event.data));
        ut.db("Deleted " + string(event.data) + " characters from document");
        put_in_log(event);
    }
    else if( event.event == string(_APPEND) )
    {
        document += event.data;
        ut.db("Appended \"" + string(event.data) + "\" to document");
        put_in_log(event);
    }

    ut.db("Document: " + document);

    if( !logEventsBuffer.empty() )
    {
        ut.db("Checking for queued log events in buffer...");
        auto maybe = ut.find_sequent_in_log(logEventsBuffer, event.sequent + 1);

        if( maybe.term == -1 )
        {
            ut.db("No subsequent log entry found");
            return;
        }
        ut.db("Found event in buffer: " + ut.log_to_string(maybe));
        apply_event(maybe);
    }
}

//! @brief  True if votes are valid, else false. For verifying the master announce message
bool BftPeer::verify_votes(const string& votes)
{
    int votesCount = 0;
    float threshold = ((float) num_peers_alive()) / 2;

    string vote;

    vector<_SIGNEDVOTE> votesVector = ut.read_votes_from_string(votes);

    for( auto v : votesVector )
    {
        if( v.yesNo && ut.verify_vote(v) )
        {
            votesCount++;
            if( v.term > term + 1 )
            {
                ut.db("WARNING: this vote skips terms (Current="+to_string(term)+", Vote="+to_string(v.term)+")");
            }
        }
    }

    ut.db("Claim: " + to_string(votesCount) + " votes");
    return votesCount >= threshold;
}

//! @brief  Election loop for peers
int startElector(BftPeer* peer)
{
	while(true)
    {
        if( peer->byz() ) peer->byzantine_action();
		if( peer->state == LEADER ) peer->remain_master(); sleep(_SHORT_TIMEOUT.tv_sec);
		if( peer->state == FOLLOWER ) peer->remain_follower(); sleep(_LONG_TIMEOUT.tv_sec);
		if( peer->state == CANDIDATE ) peer->initiate_election(); peer->ut.random_sleep();
	}

}

//! @brief Randomly perform a given action
void BftPeer::byzantine_action()
{
    int numActions = 4;
    int action = (rand() % numActions);

    switch(action){
        case 0 : {
            ut.db("### BYZ: Sending a false full delete request");
            _LOGEVENT event = {lastSequent + 1, term, _DELETE, to_string(document.length())};
            if( logEvents.empty() )
            {
                event.hash = ut.incremental_hash_init(event);
            }
            else
            {
                event.hash = ut.incremental_hash(logEvents.back(), event);
            }
            multicast_log_event_as_master(event, connectedPeers);
            break;
        }
        case 1 : {
            //ut.db("### BYZ: Randomising port"); // This is not a particularly interesting scenario...
            // Randomise our port
            //peerId.port = (rand() % 4) + PEERPORT_MIN;
            break;
        }
        case 2 : {
            ut.db("### BYZ: Distributing false election request");
            string message = ut.compose_message_as(_FORCE_ELECTION, to_string(CLIENTPORT_MIN), 0, "");
            ut.send_receive(masterId, message, nullptr);
            break;
        }
        case 3 : {
            ut.db("### BYZ: Randomising state");
            int triFlip = (rand() % 3);
            switch(triFlip){
                case 0 : {state = LEADER; masterId.port = peerId.port; break;}
                case 1 : {state = FOLLOWER; break;}
                case 2 : {state = CANDIDATE; masterId.up = 0; lastMasterAnnounceSecs = 0; break;}
            }
        }

    }
}

//! @brief  Compute the number of peers considered "up"
int BftPeer::num_peers_alive()
{
    int alive;
    for( auto i : connectedPeers )
    {
        if( i.up > 0 ) ++alive;
    }
    return alive;
}

//! @brief  Refresh "up" peers (UNUSED)
void BftPeer::update_alive()
{
    for( auto i : connectedPeers )
    {
        if( i.port == peerId.port )
        {
            peerId.up = 1;
            continue;
        }

        if( ut.ping(i) )
        {
            i.up = 0;
            ut.db("Peer " + to_string(i.port) + " suspected to be down");
        }
        else
        {
            i.up = 1;
            ut.db("Peer " + to_string(i.port) + " known to be up");
        }
    }
}

//! @brief Activities to do if one is the master
void BftPeer::remain_master()
{
    // If we are the leader, do nothing
    ut.db("MASTER");
    announce_master();
}

//! @brief Activities to do if one is a follower
void BftPeer::remain_follower()
{
    // If we are a follower, ensure that the leader is still awake
    bool masterSuspectDown = masterId.up == 0;
    if( !masterSuspectDown && (::time(nullptr) - lastMasterAnnounceSecs) > _LONG_TIMEOUT.tv_sec)
    {
        masterSuspectDown = ut.ping(masterId);
    }

    if( masterSuspectDown )
    {
        masterId.port = 0;
        masterId.up = 0;
        ut.db("Master is suspected down. Transitioning to candidate");
        state = CANDIDATE;
    }
    else
    {
        masterId.up = 1;
        ut.db("FOLLOWER (" + to_string(masterId.port) + ")");
        sleep(_LONG_TIMEOUT.tv_sec);
    }

    if( byz() )
    {
        ut.db("### BYZ: changing state");
        if( flip() )
        {
            state = CANDIDATE;
        }
        else
        {
            state = LEADER;
        }
    }

}

//! @brief Activities to do if one is a candidate
void BftPeer::initiate_election()
{
    ut.db("CANDIDATE");
    voteInProgress = true;
    masterVoteRecord.clear();
    int yesVotes = 0;
    string peerIdMsg = ut.compose_message(_CANDIDATE_REQUEST, term + 1, "");

    // Candidate for election. Seek votes from peers
    for( auto i = connectedPeers.begin(); i < connectedPeers.end(); ++i)
    {
        if( (*i).port == peerId.port )
        {
            ut.tr("Recorded yes vote from " + to_string((*i).port));
            _SIGNEDVOTE v = {peerId.port, true, term+1, ""};
            ut.signVote(&v);
            yesVotes++;
            masterVoteRecord.push_back(v);
            continue;
        }
        char buffer[1024] = {0};
        auto response = ut.send_receive(*i, peerIdMsg, buffer);
        if( response > 0 )
        {
            // Declare peer is down
            ut.tr("Recorded null vote from " + to_string((*i).port));
            (*i).up = 0;
            continue;
        }

        _SIGNEDVOTE v = ut.unmarshal_vote_safe(buffer);

        if( v.yesNo && ut.verify_vote(v) )
        {
            ut.tr("Recorded yes vote from " + to_string((*i).port));
            (*i).up = 1;
            yesVotes++;
            masterVoteRecord.push_back(v);
        }
        else if ( !v.yesNo && ut.verify_vote(v) )
        {
            //no action for a no vote, but mark that node alive
            ut.tr("Recorded no vote from " + to_string((*i).port));
            (*i).up = 1;
        }
    }
    float alivePeers = num_peers_alive();
    ut.tr("Number of peers alive: " + to_string(alivePeers));
    ut.tr("Number of votes      : " + to_string(yesVotes));

    ut.db("The following peers voted yes:");
    for( auto i : masterVoteRecord ) ut.db("   +" + to_string(i.port));

    if( yesVotes > alivePeers / 2)
    {
        // Check that we have not received a valid election announcement during the vote (or been reverted to follower)
        if( state != CANDIDATE )
        {
            ut.db("Our state has changed, ignoring the election results");
            voteInProgress = false;
            return;
        }

        ++term;
        ut.db("Candidacy succeeded with " + to_string(yesVotes) + " votes out of a possible "
            + to_string((int) alivePeers) + ". Assuming leadership of term " + to_string(term));
        state = LEADER;
        masterId = peerId;
        announce_master();
    }
    else
    {
        ut.db("The vote has failed.");

    }
    voteInProgress = false;
}

//! @brief  Perform the master announcement after an election win
void BftPeer::announce_master()
{
    auto masterAnnounceString = ut.compose_message(_MASTER_BROADCAST, term, ut.compose_vote_record(masterVoteRecord));

    for( auto i : connectedPeers )
    {
        if( i.port != peerId.port )
        {
            ut.send_receive(i, masterAnnounceString, nullptr);
        }
    }
}

//! @brief If node is byzantine, return true with a customisable probability. Used to generate unspecified behaviour
bool BftPeer::byz()
{
    if( topology != BYZANTINE ) return false;

    // Percent chance node performs byzantine behaviour
    int byzProbPercent = BYZANTINE_ACTION_PROBABILITY;
    int roll = rand() % 100;

    if( byzProbPercent > roll )
    {
        ut.db("### BYZ: Byzantine behaviour was triggered");
        return true;
    }
    return false;
}

bool BftPeer::flip()
{
    return (rand() % 2);
}

//! @brief  Open the server port, then start the elector process
int BftPeer::start()
{

	thread bftServer(startServer,this);
	thread elector(startElector, this);

	bftServer.join();

	return 0;
}




















