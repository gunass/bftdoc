
#include <iostream>
#include "config.h"
#include <vector>
#include <string>

#include "bftpeer.h"
#include "datatypes.h"
#include "bftclient.h"

using namespace std;

locale loc;

int main(int argc, char **argv) {
	std::cout << endl << "Welcome to BFTDOC, the distributed Byzantine fault tolerant document editor" << std::endl;
	std::cout << "Version " << bftdoc_VERSION_MAJOR << "." << bftdoc_VERSION_MINOR << std::endl;

    /*
     * A proper input to this program might look like:
     *
     * $ ./bftdoc 4001
     * This starts a replica on port 4001. Replicas are allowed on ports 4000-4003 only at the moment.
     *
     * $ ./bftdoc 5000 client
     * This starts a client application on port 5000.
     * The client is a little buggy but is stateless. It is the only interactive interface.
     *
     * The client will ask for a command. The command can ONLY be one of four things:
     * >>> append
     * >>> delete
     * >>> view
     * >>> elect
     * "append" and "delete" will prompt the user for a second value, i.e.
     *
     * >>> append
     * What would you like to append?
     * >>> Hello, world!
     * Successfully appended "Hello, world!" to document.
     *
     * For append, the second value is taken as a string to append to the document.
     * For delete, the second value is taken to be a number of characters to delete from the end of the document.
     * Don't get cute and try to pass in a float.
     *
     * "view" requests a copy of the document, as a string, from the nearest available node.
     * "elect" multicasts a message to all nodes to transition to candidate state and fight for master.
     *
     * All client inputs are sent to all peers 4000-4003 in sequential order, and stops as soon as one
     * message is successful.
     *
     * # ./bftdoc 4003 byzantine
     * This initiates a replica that performs byzantine actions with a custom probability.
     * As byzantine actions, the node will make random election votes, change its own state,
     * impersonate the master, spoof messages from clients, etc.
     */

    // Configure system for <= 100 peers and <= 10 clients
    if (PEERPORT_MAX - PEERPORT_MIN >= 100)
    {
        cout << "ERROR: system has bad peer port config, max 100 peers" << endl;
        return 1;
    }
    if (CLIENTPORT_MAX - CLIENTPORT_MIN >= 10)
    {
        cout << "ERROR: system has bad client port config, max 10 clients" << endl;
        return 1;
    }

	if( argc < 2 )
	{
		cout << "Port number not supplied, please supply "
                << PEERPORT_MIN << "-" << PEERPORT_MAX <<
                " or "
                << CLIENTPORT_MIN << "-" << CLIENTPORT_MAX <<
                "for clients" << endl;
		return -1;
	}

	auto peerPort = argv[1];
	auto iPeerPort = stoi(peerPort);

    // Here topology means "node type", for historical reasons.
	_TOPOLOGY topology;

	if( argc < 3 )
	{
		cout << "Node type not supplied, using default REPLICA" << endl;
		topology = REPLICA;
	}
	else
	{
        auto arg = argv[2];
        if( !strcmp(arg, "byzantine") )
        {
            topology = BYZANTINE;
        }
        else if( !strcmp(arg, "client"))
        {
            topology = CLIENT;
        }
        else
        {
            cout << "Node type not supported (yet), using default REPLICA" << endl;
            topology = REPLICA;
        }
	}

    if( topology != CLIENT )
    {
        if( iPeerPort < PEERPORT_MIN || iPeerPort > PEERPORT_MAX )
        {
            cout << "Peer port invalid, please retry in range " << PEERPORT_MIN << "-" << PEERPORT_MAX << endl;
            return -1;
        }
        cout << endl;
        BftPeer peer(iPeerPort, topology);
        peer.start();
    }
    else {
        if (iPeerPort < CLIENTPORT_MIN || iPeerPort > CLIENTPORT_MAX) {
            cout << "Client port invalid, please retry in range " << CLIENTPORT_MIN << "-" << CLIENTPORT_MAX << endl;
            return -1;
        }

        while (true)
        {
            string requestType;
            string requestBody;
            cout << "Enter your command: (append, delete, view, elect)" << endl;
            cout << ">>> ";
            cin >> requestType;
            if( requestType == _VIEW_DOC_INPUT || requestType == _ELECT_INPUT )
            {
                BftClient client(iPeerPort, requestType.c_str(), _NULL_FIELD);
                continue;
            }
            else if ( requestType == _APPEND_INPUT )
            {
                cout << "What would you like to append?" << endl;
            }
            else if ( requestType == _DELETE_INPUT )
            {
                cout << "How many chars to delete?" << endl;
            }
            else
            {
                cout << "Bad input, retry (use a single word)" << endl << endl;
                continue;
            }
            cout << ">>> ";
            cin.ignore();
            getline(cin, requestBody);
            BftClient client(iPeerPort, requestType.c_str(), requestBody.c_str());

        }
    }

    //cout << "Have a nice day." << endl;
    return 0;

}




















