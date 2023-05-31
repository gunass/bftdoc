//
// Created by Alex on 30/4/2023.
//

#include "datatypes.h"
#include "bftpeer.h"
#include "util.h"

#ifndef BFTDOC_BFTCLIENT_H
#define BFTDOC_BFTCLIENT_H


class BftClient {

public:
    _PEERID peerId;
    util ut;

    BftClient(int iPeerPort, const char* requestType, const char* requestBody);
    void start();
};


#endif
