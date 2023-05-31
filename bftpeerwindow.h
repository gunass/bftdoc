/*
 * bftpeerwindow.h
 *
 * This class should be the GUI view & controller for the BftPeer object (model)
 * It displays peer information (replica status, election status...) in a nice readable way
 * It also allows the user to trigger a crash, Byzantine fault, etc.
 *
 * Probably uses GTK... if I can get it to compile
 * In the mean time, the view is left in the CLI
 *
 *  Created on: 10 Apr 2023
 *      Author: alex
 */

#ifndef BFTPEERWINDOW_H_
#define BFTPEERWINDOW_H_


class BftPeerWindow
{
public:
	BftPeerWindow();
	~BftPeerWindow();
private:

};





#endif /* BFTPEERWINDOW_H_ */
