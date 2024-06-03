/*
Author: James Root
Class: ECE 6122 R
Last Date Modified: 11/21/23

Description:

This file gives structure to a client which connects to a server. 
The client is dictated by user input, handling the client version
and also transmitting emssages of a certain type. Depending on the
message version and type, the message may be transmitted back to 
the client or other connected clients.

*/

#include <iostream>
#include <cstdlib>
#include <SFML/Network.hpp>
#include <SFML/System.hpp>
#include <vector>
#include <functional>
#include <thread>
#include <time.h>

//define print mutex
sf::Mutex printMutex;


// define packet structure
struct tcpMessage
{
    unsigned char nVersion;
    unsigned char nType;
    unsigned short nMsgLen;
    char chMsg[1000];
};

// define >> operator for tcpMessage structure in packet class
// Inputs:
// - packet:    Packet value we ae putting into message
// - m:         Message we are putting values into
sf::Packet& operator <<(sf::Packet& packet, const tcpMessage& m)
{
    return packet << m.nVersion << m.nType << m.nMsgLen << m.chMsg;
}

// Threaded function used for handling all input and output values from the terminal
// Inputs;
// - currMsg:           The current tcpMessage stored in the system
// - messageTransmit:   The bool of if a valid message is stored
// - clients:           list of current TcpSocket connections
sf::Packet &operator >>(sf::Packet &packet, tcpMessage &m)
{
    return packet >> m.nVersion >> m.nType >> m.nMsgLen >> m.chMsg;
}

void handleCommandLine(tcpMessage* msg, sf::TcpSocket* socket)
{
    std::string input;
    std::string message;
    unsigned char v = 0;
    unsigned char t = 0;
    // question loop
    while (true)
    {
        printMutex.lock();
        std::cout << "Please enter a command: ";
        printMutex.unlock();
        std::getline(std::cin, input);

        // std::cout << input << std::endl;

        // check input for action
        if (input.length() > 2 && input[0] == 'v' && input[1] == ' ')
        {
            // reset version number

            std::string potVerNo = input.substr(2);
            // check if only a number is given
            for (unsigned long i = 0; i < potVerNo.length(); i++)
            {
                if (!isdigit(potVerNo[i]))
                {
                    // invalid version no given, get input again
                    printMutex.lock();
                    std::cout << "Invalid input given. Please give inputs of (\"v [version no]\", \"t [type no] [message]\", or \"q\")" << std::endl;
                    printMutex.unlock();
                    continue;
                }
            }

            v = static_cast<unsigned char>(std::stoul(potVerNo));
        }
        else if (input.length() > 4 && input[0] == 't' && input[1] == ' ')
        {
            // send a message with given type number

            // check for formatting
            unsigned long i;
            for (i = 2; i < input.length(); i++)
            {
                // check for space character
                if (input[i] == ' ')
                {
                    // end of type number, exit
                    break;
                }
            }
            // check if at end of word (no message given)
            if (i >= input.length() - 1)
            {
                printMutex.lock();
                std::cout << "Invalid input given. Please give inputs of (\"v [version no]\", \"t [type no] [message]\", or \"q\")" << std::endl;
                printMutex.unlock();
                continue;
            }
            // record type value and message string
            std::string temp = input.substr(2, i - 2);
            t = static_cast<unsigned char>(std::stoul(temp));
            message = input.substr(i + 1);

            // begin message transmission
            msg->nType = t;
            msg->nVersion = v;
            for (unsigned long j = 0; j < 1000; j++)
            {
                // add each char of message onto the outgoing message
                if (j < message.length()) {
                    msg->chMsg[j] = message[j];
                }
                // if message copied fully, add on NULL items;
                else {
                    msg->chMsg[j] = '\0';
                }
            }
            // calculate the message length value
            for (unsigned long j = 0; j < 1000; j++)
            {
                if ( msg->chMsg[j] == '\0')
                {
                    msg->nMsgLen = static_cast<unsigned short>(j + 1);
                    break;
                }
            }

            // transmit message
            sf::Packet pack;
            pack << *msg;
            if (socket->send(pack) != sf::Socket::Done)
            {
                // error handling
                printMutex.lock();
                std::cout << "Transmission Failed. There may be a connection issue." << std::endl;
                printMutex.unlock();
            }
        }
        else if (input.length() == 1 && input[0] == 'q')
        {
            // send termination message
            msg->nType = 1;
            msg->nVersion = 0;

            // transmit message
            sf::Packet pack;
            pack << *msg;
            if (socket->send(pack) == sf::Socket::Done)
            {
                printMutex.lock();
                std::cout << "Successfully Disconnected!" << std::endl;
                printMutex.unlock();
            }
            // exit the function
            break;
        }
        else
        {
            // incorrect input given
            printMutex.lock();
            std::cout << "Invalid input given. Please give inputs of (\"v [version no]\", \"t [type no] [message]\", or \"q\")" << std::endl;
            printMutex.unlock();
        }
    }
}

// threaded method for handling all messages from the connected socket
// Inputs:
// - socket:    socket connection to server
void handleSocketInput(sf::TcpSocket* socket)
{   
    // loop of reception checking
    sf::Packet pack;
    while (true) {
        // check for data from the server
        if (socket->receive(pack) == sf::Socket::Done)
        {
            printMutex.lock();
            tcpMessage recMsg;
            pack >> recMsg;
            std::cout << std::endl << "Received Msg Type: " << static_cast<unsigned short>(recMsg.nType) << "; Msg: " << recMsg.chMsg << std::endl;
            // // print non NULL characters of message
            // for (int i = 0; (i < recMsg.nMsgLen) && (recMsg.chMsg[i] != '\0'); i++) 
            // {
            //     std::cout << recMsg.chMsg[i];
            // }
            // std::cout << std::endl;
            printMutex.unlock();
        }
    }
}

// Main method for running program logic
int main(int argc, char *argv[])
{
    // check if inputs are correct
    if (argc != 3)
    {
        std::cout << "Input Error: only the connection address and port may be given as input." << std::endl;
        return 1;
    }
    sf::IpAddress server = argv[1];
    unsigned short port = static_cast<unsigned short>(std::stoul(argv[2]));

    tcpMessage msg;

    // create socket for communicating with server
    sf::TcpSocket socket;
    if (socket.connect(server, port) != sf::Socket::Done)
    {
        // failed connection (likely a port issue)
        std::cout << "Connection via port " << port << " failed. Try again with a different port." << std::endl;
        return 1;
    }
    // connection succeeded
    std::cout << "Connected to port " << port << "." << std::endl;


    sf::Thread t2(std::bind(&handleSocketInput, &socket));
    sf::Thread t1(std::bind(&handleCommandLine, &msg, &socket));
    t2.launch();
    t1.launch();

    t1.wait();
    t2.terminate();

    return 0;
}