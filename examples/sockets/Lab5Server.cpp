/*
Author: James Root
Class: ECE 6122 R
Last Date Modified: 11/21/23

Description:

This file gives structure to a Server which handles client connections.
Clients will send messages to the server, which handles them based
upon defined behavior. Users can ask the server for the current message
and list of connected clients

*/


#include <iostream>
#include <cstdlib>
#include <SFML/Network.hpp>
#include <SFML/System.hpp>
#include <SFML/Network/SocketSelector.hpp>
#include <string.h>
#include <thread>
#include <vector>
#include <omp.h>
#include <functional>
#include <list>

// define packet structure
struct tcpMessage
{
    unsigned char nVersion;
    unsigned char nType;
    unsigned short nMsgLen;
    char chMsg[1000];
};

// define << operator for tcpMessage structure in packet class
// Inputs:
// - packet:    Packet value we ae putting message into
// - m:         Message we are putting into package
sf::Packet &operator<<(sf::Packet &packet, const tcpMessage &m)
{
    return packet << m.nVersion << m.nType << m.nMsgLen << m.chMsg;
}

// define >> operator for tcpMessage structure in packet class
// Inputs:
// - packet:    Packet value we ae putting into message
// - m:         Message we are putting values into
sf::Packet &operator>>(sf::Packet &packet, tcpMessage &m)
{
    return packet >> m.nVersion >> m.nType >> m.nMsgLen >> m.chMsg;
}


// Threaded function used for handling all input and output values from the terminal
// Inputs;
// - currMsg:           The current tcpMessage stored in the system
// - messageTransmit:   The bool of if a valid message is stored
// - clients:           list of current TcpSocket connections
void handleTerminal(tcpMessage *currMsg, bool *messageTransmit, std::list<sf::TcpSocket *> *clients)
{
    std::string input;
    // poll for user inputs
    while (true)
    {
        // get user message
        std::cout << "Please enter command: ";
        std::cin >> input;

        // user prompts for last message recieved
        if (input == "msg")
        {
            // check if we have recieved a valid message 
            if (*messageTransmit)
            {
                // if so, print it
                std::cout << "Last Message: " << currMsg->chMsg << std::endl;
            }
            else
            {
                // if not, tell the user.
                std::cout << "No valid message has been sent." << std::endl;
            }
        }
        // user prompts for list of connected clients
        else if (input == "clients")
        {
            std::cout << "Number of clients: " << clients->size() << std::endl;
            for (sf::TcpSocket *sock : *clients)
            {
                // print each item
                std::cout << "IP Address: " << sock->getRemoteAddress() << " | Port: " << sock->getRemotePort() << std::endl;
            }
        }
        // user prompts to exit
        else if (input == "exit")
        {
            return;
        }
        // invalid input given
        else
        {
            std::cout << "Invalid input! Please try again (Options: 'msg', 'clients', 'exit')." << std::endl;
        }
        // establish socketing values
    }
}

// method to handle establishing connections with clients
// Inputs;
// - listener:          Listening value for server port
// - clients:           list of current TcpSocket connections
// - selector:          SocketSelector for all server sockets
// - currMsg:           The current tcpMessage stored in the system
// - messageTransmit:   The bool of if a valid message is stored
void handleClients(sf::TcpListener *listener, std::list<sf::TcpSocket *> *clients, sf::SocketSelector *selector, tcpMessage *currMsg, bool *messageTransmit)
{
    // create endless loop
    while (true)
    {
        // programming logic taken from sf::Socket class reference
        // have selector wait for data
        if (selector->wait())
        {
            // test listener
            if (selector->isReady(*listener))
            {
                // we have a pending connetion
                sf::TcpSocket* client = new sf::TcpSocket;
                if (listener->accept(*client) == sf::Socket::Done)
                {
                    // add to client list
                    clients->push_back(client);

                    // add client to selector
                    selector->add(*client);
                }
                else
                {
                    // Error in connection, delete this socket
                    delete client;
                }
            }
            else
            {
                // listener socket not ready for new connection, test all connected clients
                for (std::list<sf::TcpSocket *>::iterator it = clients->begin(); it != clients->end(); ++it)
                {
                    sf::TcpSocket& client = **it;
                    if (selector->isReady(client))
                    {
                        // recieve data form the client
                        sf::Packet packet;
                        if (client.receive(packet) == sf::Socket::Done)
                        {
                            // unpack our package value
                            tcpMessage msg;
                            packet >> msg;
                            
                            // check if we need to ignore
                            if (msg.nVersion == 102)
                            {
                                // we have a valid message, save it to our values
                                *messageTransmit = true;
                                *currMsg = msg;

                                // check for nType specifics
                                if (msg.nType == 77)
                                {
                                    // send to all other connected clients 
                                    for (std::list<sf::TcpSocket *>::iterator otherit = clients->begin(); otherit != clients->end(); ++otherit)
                                    {
                                        sf::TcpSocket& otherclient = **otherit;
                                        // check if the otherit is not the current it
                                        if (otherclient.getRemotePort() != client.getRemotePort())
                                        {
                                            otherclient.send(packet);
                                        }
                                    }

                                }
                                else if (msg.nType == 201)
                                {
                                    // send reversed message back to client
                                    char revMsg[1000];
                                    for (unsigned int j = 0; j < 1000; j++)
                                    {
                                        // initialize char values
                                        revMsg[j] = '\0';
                                    }
                                    unsigned int i = 0;
                                    for (unsigned int j = 0; j < 1000; j++)
                                    {
                                        // reverse message values
                                        if (msg.chMsg[1000 - 1 - j] != '\0')
                                        {
                                            revMsg[i] = msg.chMsg[1000 - 1 - j];
                                            i++;
                                        }
                                    }
                                    tcpMessage temp = msg;
                                    // set reversed values to message
                                    for (unsigned int j = 0; j < 1000; j++)
                                    {
                                        // initialize char values
                                        temp.chMsg[j] = revMsg[j];
                                    }
                                    //put message into packet
                                    sf::Packet newPkt;
                                    newPkt << temp;
                                    // send back to only clent receieved from
                                    client.send(newPkt);
                                }
                            }

                            // check for exit message (nVersion = 0, nType = 1 )
                            if (msg.nVersion == 0 && msg.nType == 1)
                            {
                                // delete item from clients list
                                it = clients->erase(it);
                            }
                        }
                    }
                }
            }
        }
    }
}


// main method for running program logic
int main () {
    // long unsigned int num_threads = std::thread::hardware_concurrency();
    // long unsigned int basePort = 50001;

    // logic for message tracking
    tcpMessage currMsg;
    bool messageTransmit = false;
    // bool shutdown = false; // shared value of

    // create listening socket
    sf::TcpListener listener;
    listener.listen(55005);

    // instantiate list of clients shared between threads
    std::list<sf::TcpSocket *> clients;

    // define Selector and add the listener
    sf::SocketSelector selector;
    selector.add(listener);

    // thread controls server io
    sf::Thread t1(std::bind(&handleTerminal, &currMsg, &messageTransmit, &clients));
    sf::Thread t2(std::bind(&handleClients, &listener, &clients, &selector, &currMsg, &messageTransmit));

    t1.launch();
    t2.launch();

    // wait for t1 to end (user gives "exit" command)
    t1.wait();

    // end sub programs
    t2.terminate();
    
    return 0;
}
