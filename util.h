//
// Created by Alex on 30/4/2023.
//
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
#ifndef BFTDOC_UTIL_H
#define BFTDOC_UTIL_H

using namespace std;

class util {

public:

    _PEERID peerId;
    int* termNumber;

    util(_PEERID peerid, int* term)
    {
        peerId.port = peerid.port;
        peerId.up = peerid.up;
        termNumber = term;
    }

    util()
    {
        peerId.port = 0;
        peerId.up = 0;
    }

    void db(const string &debugPrint) {
        timeval t{0, 0};
        timeval tz{0, 0};
        gettimeofday(&t, &tz);

        string sec_time_string = to_string(t.tv_sec).substr(6);
        string usec_time_string = to_string(t.tv_usec).substr(0, 4);

        cout << "(" << *termNumber << ")[" << sec_time_string << ":" << usec_time_string << "] " << debugPrint << endl;
    }

    void tr(const string &tracePrint)
    {
        if( LOGLEVEL >= 1 )
        {
            db("        TRACE:" + tracePrint);
        }
    }

    static void random_sleep()
    {
        auto sleepModifier = std::rand() % (_ELECTION_TIMEOUT_MAX - _LONG_TIMEOUT.tv_sec);
        sleep(_LONG_TIMEOUT.tv_sec + sleepModifier);
    }

//! @brief  Send the specified string, optionally putting the reply in buffer (nullptr for no reply)
    int send_receive(_PEERID sendPort, const string &requestString, char *buffer) {
        // Client code derived from https://www.geeksforgeeks.org/socket-programming-cc/
        int client_fd;
        if ((client_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
            db("Socket creation error");
            return 1;
        }

        struct sockaddr_in serv_addr;
        serv_addr.sin_family = AF_INET;
        serv_addr.sin_port = htons(sendPort.port);

        if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0) {
            db("Server format wrong or something");
            close(client_fd);
            return -1;
        }

        int status;
        if ((status = connect(client_fd, (struct sockaddr *) &serv_addr, sizeof(serv_addr))) < 0) {
            close(client_fd);
            return 1;
        }

        // FIXME: this socket timeout does not work.
        setsockopt(client_fd, SOL_SOCKET, SO_RCVTIMEO, &_LONG_TIMEOUT, sizeof(_LONG_TIMEOUT));

        int valread = 0;
        tr("Sending : " + requestString);
        const char *requestStringC = requestString.c_str();
        send(client_fd, requestStringC, strlen(requestStringC), 0);

        sendPort.up = 1;

        if (buffer != nullptr) {
            valread = recv(client_fd, buffer, 1024, 0);
        } else {
            close(client_fd);
            return 0;
        }

        close(client_fd);

        if (valread <= 0) {
            return 1;
        } else {
            return 0;
        }

    }

    _EVENT_WITH_SIG unmarshal_sig_from_event(string messageBody)
    {
        _EVENT_WITH_SIG ews = {"", -1, ""};
        int i = 0;

        string trueBody;
        for( ; messageBody[i] != _FIELD_SEPARATOR[0] ; i++ )
        {
            trueBody += messageBody[i];
        }
        i++;

        string clientPort;
        for( ; messageBody[i] != _FIELD_SEPARATOR[0] ; i++ )
        {
            clientPort += messageBody[i];
        }
        i++;

        string clientSig;
        for( ; i < messageBody.length() ; i++ )
        {
            clientSig += messageBody[i];
        }
        i++;

        ews.body = trueBody;
        ews.clientPort = stoi(clientPort);
        sanitise_signature(clientSig);
        ews.clientSig = clientSig;

        tr("Original message body was " + trueBody);
        tr("Original message port was " + clientPort);
        tr("Original message sign was " + clientSig);

        return ews;
    }

    _EVENT_WITH_SIG unmarshal_sig_from_event_safe(string messageBody)
    {
        try {
            return unmarshal_sig_from_event(messageBody);
        }
        catch (std::exception&)
        {
            return {"", -1, ""};
        }
    }

    _SIGNEDVOTE unmarshal_vote(string message)
    {
        _SIGNEDVOTE vote = {0,false, -1, _NULL_FIELD};
        const char* messageC = message.c_str();
        int i = 0;

        string response;
        for( ; messageC[i] != _VOTE_SEPARATOR[0] ; i++ )
        {
            response += messageC[i];
        }
        i++;

        string claimedPeer;
        for( ; messageC[i] != _VOTE_SEPARATOR[0] ; i++ )
        {
            claimedPeer += messageC[i];
        }
        i++;

        string term;
        for( ; messageC[i] != _VOTE_SEPARATOR[0] ; i++ )
        {
            term += messageC[i];
        }
        i++;

        string sig;
        for( ; i < message.length() ; i++ )
        {
            sig += messageC[i];
        }

        vote.yesNo = !strcmp(response.c_str(), _CANDIDATE_YES_REPLY);
        vote.port = stoi(claimedPeer);
        vote.term = stoi(term);
        vote.sig = sig;
        return vote;
    }

    _SIGNEDVOTE unmarshal_vote_safe(string message)
    {
        _SIGNEDVOTE vote = {-1, 0, -1, ""};
        try {
            vote = unmarshal_vote(message);
        } catch (std::exception&)
        {
            vote = {-1, 0, -1, ""};
        }
        return vote;
    }


    vector<_SIGNEDVOTE> read_votes_from_string(string votesString)
    {
        vector<_SIGNEDVOTE> votesVector;
        int i = 0;
        while( i < votesString.length() )
        {
            int j = i;
            char buffer[20];
            while( votesString[j] != _FIELD_SEPARATOR[0] )
            {
                j++;
            }
            votesVector.push_back(unmarshal_vote_safe(votesString.substr(i, j)));
            i = j + 1;
        }
        return votesVector;
    }

    string vote_to_string(_SIGNEDVOTE vote)
    {
        string result;
        result += vote.yesNo ? _CANDIDATE_YES_REPLY : _CANDIDATE_NO_REPLY;
        result += _VOTE_SEPARATOR;
        result += to_string(vote.port);
        result += _VOTE_SEPARATOR;
        result += to_string(vote.term);
        result += _VOTE_SEPARATOR;
        result += vote.sig;
        return result;
    }

    void signVote(_SIGNEDVOTE* vote)
    {
        string unsignedVote = vote->yesNo ? _CANDIDATE_YES_REPLY : _CANDIDATE_NO_REPLY;
        unsignedVote += _VOTE_SEPARATOR;
        unsignedVote += to_string(vote->port);
        unsignedVote += _VOTE_SEPARATOR;
        unsignedVote += to_string(*termNumber + 1);
        unsignedVote += _VOTE_SEPARATOR;

        vote->sig = sign(unsignedVote);
    }

    string compose_vote_record(vector<_SIGNEDVOTE> votes)
    {
        string comp;
        for(auto v : votes)
        {
            comp += vote_to_string(v);
            comp += _FIELD_SEPARATOR;
        }
        return comp;
    }

    string compose_message_vote(const char* reply)
    {
        string message = reply;
        message += _VOTE_SEPARATOR;
        message += to_string(peerId.port);
        message += _VOTE_SEPARATOR;
        message += to_string(*termNumber);
        message += _VOTE_SEPARATOR;
        message += sign(message);
        message += _VOTE_SEPARATOR;
        return message;
    }

    //! @brief Used to relay messages... or impersonate another peer!
    string compose_message_as(string messageType, string messagePeer, int term, string messageBody)
    {
        string message = messageType;
        message += _FIELD_SEPARATOR;
        message += messagePeer;
        message += _FIELD_SEPARATOR;
        message += to_string(term);
        message += _FIELD_SEPARATOR;
        message += messageBody;
        message += _SIGNT_SEPARATOR;
        message += sign(message);
        message += _FIELD_SEPARATOR;
        return message;
    }

    //! @brief Used to compose a simple UNSIGNED message consisting only of TYPE:PEER:TERM: i.e. PING
    string compose_message(const char *messageType, int term) {
        string message = messageType;
        message += _FIELD_SEPARATOR;
        message += to_string(peerId.port);
        message += _FIELD_SEPARATOR;
        message += to_string(term);
        message += _FIELD_SEPARATOR;
        return message;
    }

    //! @brief Used to compose a message consisting of TYPE:PEER:TERM:SIGN:BODY:
    string compose_message(const char *messageType, int term, const string& messageBody)
    {
        string message = compose_message(messageType, term);
        message += messageBody;
        message += _SIGNT_SEPARATOR;
        message += sign(message);
        message += _FIELD_SEPARATOR;
        return message;
    }

    //! @brief Used to compose a message consisting of TYPE:PEER:TERM:(SQNC:BODY):SIGN:
    string compose_message_event(const char *messageType, int term, int sequenceNumber, const string& messageBody)
    {
        string message = compose_message(messageType, term);
        message += to_string(sequenceNumber);
        message += _FIELD_SEPARATOR;
        message += messageBody;
        message += _SIGNT_SEPARATOR;
        message += sign(message);
        message += _FIELD_SEPARATOR;
        return message;
    }

    _MESSAGE unmarshal_message_safe(const string& messageString)
    {
        _MESSAGE message = {_NULL_FIELD, _NULL_FIELD, _NULL_FIELD, _NULL_FIELD, _NULL_FIELD};
        try {
            message = unmarshal_message(messageString);
            return message;
        }
        catch(std::exception&)
        {
            db("Bad message format, ignoring");
            return message;
        }
    }

    //! @brief Convert a colon-delimited message string into a _MESSAGE struct
    _MESSAGE unmarshal_message(const string& messageString)
    {
        const char* message = messageString.c_str();
        string type;
        int i = 0 ;
        for( ; message[i] != _FIELD_SEPARATOR[0]; ++i )
        {
            type += message[i];
        }
        ++i;
        tr("Type is " + type);

        string peer;
        for( ; message[i] != _FIELD_SEPARATOR[0]; ++i )
        {
            peer += message[i];
        }
        ++i;
        tr("Peer is " + peer);

        string term;
        for( ; message[i] != _FIELD_SEPARATOR[0]; ++i )
        {
            term += message[i];
        }
        ++i;
        tr("Term is " + term);

        string body;
        for( ; message[i] != _SIGNT_SEPARATOR[0]; ++i )
        {
            body += message[i];
        }
        ++i;
        tr("Body is " + body);

        string sign;
        for( ; message[i] != _FIELD_SEPARATOR[0]; ++i )
        {
            sign += message[i];
        }
        ++i;
        tr("Sign is " + sign);

        _MESSAGE messageStruct = {type, peer, term, body, sign};
        return messageStruct;
    }

//! @brief  Ping a given node
//! @return 1 if peer cannot be reached, else 0
    int ping(_PEERID peer)
    {
        char buffer[1024] = {0};

        string pingRequest = compose_message(_PING, 0, "");

        if (send_receive(peer, pingRequest, buffer))
        {
            return 1;
        }
        else
        {
            return 0;
        }
    }

    //! @brief Compute an incremental hash for each log, which is the sum of all chars, XORed with previous hash
    static int incremental_hash(const _LOGEVENT& previousLog, const _LOGEVENT& newLog)
    {
        int previousHash = previousLog.hash;
        int newLogHash = string_sum(log_to_string(newLog));

        return previousHash ^ newLogHash;
    }

    //! @brief compute an initial hash using a shared constant
    static int incremental_hash_init(const _LOGEVENT& newLog)
    {
        int previousHash = 12345; // Random constant
        int newLogHash = string_sum(log_to_string(newLog));
        return previousHash ^ newLogHash;
    }


    //! @brief Convert a log event to string representation (for debugging only)
    static string log_to_string(const _LOGEVENT& log)
    {
        std::string output;

        output += std::to_string(log.sequent);
        output += _FIELD_SEPARATOR;
        output += std::to_string(log.term);
        output += _FIELD_SEPARATOR;
        output += log.event;
        output += _FIELD_SEPARATOR;
        output += log.data;
        output += _FIELD_SEPARATOR;
        output += to_string(log.hash);
        output += _FIELD_SEPARATOR;
        output += to_string(log.clientPort);
        output += _FIELD_SEPARATOR;
        output += log.clientSig;
        return output;
    }

    //! @brief Convert a message into a string. UNSAFE. Be careful.
    static string message_to_string(const _MESSAGE m)
    {
        string output;

        output += m.messageType;
        output += _FIELD_SEPARATOR;
        output += m.messagePort;
        output += _FIELD_SEPARATOR;
        output += m.messageTerm;
        output += _FIELD_SEPARATOR;
        output += m.messageBody;
        output += _SIGNT_SEPARATOR;
        output += m.messageSign;
        output += _FIELD_SEPARATOR;

        return output;
    }

    //! @brief Sum a string's char values
    static int string_sum(const string& str)
    {
        int sum = 0;
        for( auto i = str.cbegin(); i < str.cend(); i++ )
        {
            sum += (*i);
        }
        return sum;
    }

    //! @brief Given a log an a sequence number, return a matching log event iff it exists
    //! @return a valid log event if one exists, else a log event with term = -1
    static _LOGEVENT find_sequent_in_log(const vector<_LOGEVENT>& events, int seq)
    {
        for( auto& i : events )
        {
            if( i.sequent == seq )
            {
                return i;
            }
        }
        _LOGEVENT nullEvent = {-1,-1,_NULL_FIELD,"",-1};
        return nullEvent;
    }

    //! @brief Strip the field separator from messages (:) and signature separator ($)
    static void sanitise(std::string& s)
    {
        s.erase(std::remove(s.begin(), s.end(), _FIELD_SEPARATOR[0]), s.end());
    }

    static void sanitise_signature(std::string& s)
    {
        s.erase(std::remove(s.begin(), s.end(), _SIGNT_SEPARATOR[0]), s.end());
        s.erase(std::remove(s.begin(), s.end(), _VOTE_SEPARATOR[0]), s.end());
    }

    //! @brief Extremely simple crypto protocol - returns a secret key corresponding to the given public key.
    //! Public keys are just port numbers, private keys are a simple multiple. For this demo, we assume the
    //! byzantine nodes are not clever enough to figure this out
    static int generate_secret_key(int publicKey)
    {
        return (publicKey * 100);
    }

    //! @brief Generate a simple signature for the given message/private key pair
    int sign_message(const string& message, int privateKey)
    {
        int messageSum = string_sum(message);
        return messageSum + privateKey;
    }

    //! @brief Sign the message to string
    string sign(const string& message)
    {
        string sig = to_string(sign_message(message, generate_secret_key(peerId.port)));
        return sig;
    }


    //! @brief True iff the public key matches the signature
    bool verify_signature(const string& message, const string& signature, int publicKey)
    {
        tr("Verifying message " + message);
        tr("Using signature   " + signature);
        tr("Claimed origin is " + to_string(publicKey));
        int modE = stoi(signature) % publicKey;
        int modG = string_sum(message) % publicKey;
        if( modE != modG ) db("Signature was not verified: " + message + signature);
        return (modE == modG);
    }

    bool verify_vote(const _SIGNEDVOTE& vote)
    {
        int claimedPeer = vote.port;
        string saniSig = vote.sig;
        sanitise_signature(saniSig);
        string unsignedVote = (vote.yesNo ? _CANDIDATE_YES_REPLY : _CANDIDATE_NO_REPLY);
        unsignedVote += _VOTE_SEPARATOR;
        unsignedVote += to_string(claimedPeer);
        unsignedVote += _VOTE_SEPARATOR;
        unsignedVote += to_string(vote.term);
        unsignedVote += _VOTE_SEPARATOR;

        bool verdict = verify_signature(unsignedVote, vote.sig, claimedPeer);
        tr("Claimed vote from " + to_string(claimedPeer) + " has verdict " + (verdict ? "YES" : "NO"));
        return verdict;
    }

    bool verify_message(const _MESSAGE& message)
    {
        int claimedPeer = stoi(message.messagePort);
        string saniSig = message.messageSign;
        sanitise_signature(saniSig);
        string unsignedMessage = message.messageType
                + _FIELD_SEPARATOR
                + message.messagePort
                + _FIELD_SEPARATOR
                + message.messageTerm
                + _FIELD_SEPARATOR
                + message.messageBody
                + _SIGNT_SEPARATOR;

        return verify_signature(unsignedMessage, saniSig, claimedPeer);
    }


};


#endif