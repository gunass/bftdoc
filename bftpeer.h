/*
 * bftpeer.h
 *
 * This class is the concrete implementation of the BFT Raft algorithm.
 *
 *
 *  Created on: 10 Apr 2023
 *      Author: alex
 */

#ifndef BFTPEER_H_
#define BFTPEER_H_

#include "datatypes.h"
#include <vector>
#include "config.h"
#include "bftpeerwindow.h"
#include "util.h"
#include <thread>

using namespace std;

class BftPeer
{

public:
	BftPeer(int lPeerPort, _TOPOLOGY lTopology);
	~BftPeer();

	int start();

	void connectPeers();

	_REPLICASTATE state;
	_TOPOLOGY topology;

    util ut;

    void put_in_log(_LOGEVENT logEvent);
    int term;
    time_t lastMasterAnnounceSecs;
    bool replayRequested = false;

    void update_alive();

	_PEERID peerId;
	vector<_PEERID> connectedPeers;
	int myServerSock;

    vector<_SIGNEDVOTE> masterVoteRecord;
	_PEERID masterId;

    int secretKey;

    bool voteInProgress{false};
    int lastSequent = -1;

    /// Election methods
    void initiate_election();
    void remain_follower();
    void remain_master();
    void announce_master();
    void handle_message(int inSock, const _MESSAGE& m);
    bool verify_votes(const string& voteRecord);
    int num_peers_alive();


    bool byz();
    void byzantine_action();

private:

    vector<_LOGEVENT> logEvents;
    vector<_LOGEVENT> logEventsBuffer;
    string document;
    void apply_event(_LOGEVENT);

    bool flip();

    void handle_message_candidate(int inSock, const _MESSAGE& message);
    void handle_message_master(int inSock, const _MESSAGE& message);
    void handle_message_client(int inSock, const _MESSAGE& message);
    void handle_message_replay(int inSock, const _MESSAGE& message);

    void request_replay(int seq);

    int process_client_request(_MESSAGE message);

    void relay_message_to_master(int inSock, _MESSAGE message);

    void process_client_request_as_master(int inSock, _MESSAGE message);

    void process_client_view_request(int inSock, _MESSAGE message);

    void process_client_elect_request(int inSock, _MESSAGE message);

    void process_master_log_event(int inSock, const _MESSAGE& message);

    void unicast_log_event_as_master(const _LOGEVENT& logEvent, int peerPort);

    void multicast_log_event_as_master(const _LOGEVENT& event, const vector<_PEERID>& toSend);
};



#endif /* BFTPEER_H_ */
