#include"vector_serialize.hpp"
#include"commands.hpp"
#include"service/function.hpp"
#include"service/client.hpp"
#include"commandline/parameter_parser.hpp"
#include"commandline/vt100.hpp"
#include<iostream>

using namespace fetch::service;
using namespace fetch::commandline;


int main(int argc, char const **argv) {
  ParamsParser params;
  params.Parse(argc, argv);
  
  if(params.arg_size() <= 1) {
    std::cout << "usage: ./" << argv[0] << " [command] [args ...]" << std::endl;
    return -1;
  }

  std::string command = params.GetArg(1);
  std::cout << std::endl;
  std::cout << "Executing command: " << command << std::endl;

  // Connecting to server
  uint16_t port = params.GetParam<uint16_t>("port", 8080);
  std::string host = params.GetParam("host", "localhost");

  std::cout << "Connecting to server " << host << " on " << port << std::endl;
  ServiceClient< fetch::network::TCPClient > client(host, port);
  client.Start();
  std::this_thread::sleep_for( std::chrono::milliseconds(300) );

  // Using the command protocol
  if(command == "connect" ) {
    if(params.arg_size() <= 2) {
      std::cout << "usage: ./" << argv[0] << " connect [host] [[port=8080]]" << std::endl;
      return -1;
    }
    
    std::string h = params.GetArg(2);    
    uint16_t p = params.GetArg<uint16_t>(3, 8080);
    std::cout << "Sending 'connect' command with parameters " << h << " " << p << std::endl;

    auto prom = client.Call(FetchProtocols::REMOTE_CONTROL, RemoteCommands::CONNECT, h, p);
    prom.Wait();
  }


  // Using the getinfo protocol
  if(command == "info" ) {

    std::cout << "Sending 'info' command with no parameters "<< std::endl;

    auto prom = client.Call(FetchProtocols::REMOTE_CONTROL, RemoteCommands::GET_INFO);
    std::cout << "Info about the node: " << std::endl;
    std::cout << prom.As< std::string >() << std::endl << std::endl;
  }


  if(command == "listen" ) {
    std::cout << "Listening to " << std::endl;
    auto handle1 = client.Subscribe(FetchProtocols::PEER_TO_PEER, PeerToPeerFeed::NEW_MESSAGE, new Function< void(std::string) >([](std::string const&msg) {
          std::cout << VT100::GetColor("blue", "default") << "Got message: " << msg << VT100::DefaultAttributes() <<  std::endl;
        }) );
    
    client.Subscribe(FetchProtocols::PEER_TO_PEER, PeerToPeerFeed::NEW_MESSAGE, new Function< void(std::string) >([](std::string const&msg) {
          std::cout << VT100::GetColor("red", "default") << "Got message 2: " << msg << VT100::DefaultAttributes() <<  std::endl;
        }) );    
    
    std::this_thread::sleep_for( std::chrono::milliseconds( 5000 ) );
    
    client.Unsubscribe( handle1 );
    std::this_thread::sleep_for( std::chrono::milliseconds( 5000 ) );
    // Ungracefully leaving

  }
  
  // Testing the send message
  if(command == "sendmsg" ) {
    std::string msg = params.GetArg(2);    
    std::cout << "Peer-to-peer command 'sendmsg' command with "<< msg << std::endl;
    
    auto prom = client.Call(FetchProtocols::PEER_TO_PEER, PeerToPeerCommands::SEND_MESSAGE, msg);

    prom.Wait();
  }

  // Testing the send message
  if(command == "messages" ) {
    std::cout << "Peer-to-peer command 'messages' command with no parameters "<< std::endl;
    auto prom = client.Call(FetchProtocols::PEER_TO_PEER, PeerToPeerCommands::GET_MESSAGES);

    std::vector< std::string > msgs =  prom.As< std::vector< std::string > >();
    for(auto &msg: msgs) {
      std::cout << "  - " << msg << std::endl;
    }
  }
  
  client.Stop(); 

  return 0;
}
