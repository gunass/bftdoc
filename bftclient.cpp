//
// Created by Alex on 30/4/2023.
//

#include "bftclient.h"



BftClient::BftClient(int iPeerPort, const char* requestType, const char* requestBody)
{
    peerId = {iPeerPort, 1};
    int zero = 0;
    ut = util(peerId, &zero);
    bool needsBody = false;
    bool isElection = false;
    string requestTypeParsed = _NULL_FIELD;

    if( !strcmp(requestType, _APPEND_INPUT.c_str()) )
    {
        requestTypeParsed = _APPEND;
        needsBody = true;
    }
    else if( !strcmp(requestType, _DELETE_INPUT.c_str()) )
    {
        requestTypeParsed = _DELETE;
        try {
            stoi(requestBody);
        } catch (std::exception&)
        {
            ut.db("WARNING: keyword was DELT but input was NaN, ignoring");
            return;
        }
        needsBody = true;
    }
    else if( !strcmp(requestType, _VIEW_DOC_INPUT.c_str()) )
    {
        requestTypeParsed = _VIEW_DOC;
    }
    else if( !strcmp(requestType, _ELECT_INPUT.c_str()) )
    {
        isElection = true;
        requestTypeParsed = _FORCE_ELECTION;
    }

    string message = ut.compose_message(requestTypeParsed.c_str(), 0, requestBody);

    if( (strlen(requestBody) == 0 || !strcmp(requestBody, _NULL_FIELD)) && needsBody )
    {
        ut.db("WARNING: message body is blank or NULL. Ignoring...");
        return;
    }

    ut.db("Sending : " + string(message));

    _PEERID peerToSend = {PEERPORT_MIN, 1};
    int i = 0;
    char reply[1024] = {0};
    int numPeers = PEERPORT_MAX - PEERPORT_MIN + 1;
    int timeout = numPeers; // One send is all we need
    int t = 0;

    if( isElection )
    {
        // For an election request, we want to start an election at every node
        for(; i < numPeers; ++i )
        {
            peerToSend.port = PEERPORT_MIN + i ;
            ut.send_receive(peerToSend, message, nullptr);
        }
        ut.db("Issued election request to all peers");
        return;
    }

    while( ut.send_receive(peerToSend, message, reply) && t < timeout)
    {
        // Continue to request replicas until one replies
        i = (i + 1) % (numPeers);
        peerToSend.port = 4000 + i;
        ++t;
    }

    if( t >= timeout )
    {
        ut.db("Timeout. No peers available or bad request");
        return;
    }

    if( strcmp(reply, _NULL_FIELD))
    {
        ut.db("Successful request to peer " + to_string(peerToSend.port));
        ut.db("Reply is : " + string(reply));
    }
    else
    {
        ut.db("Request failed");
    }

}

void BftClient::start()
{
    return;
}
