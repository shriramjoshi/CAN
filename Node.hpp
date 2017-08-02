#ifndef NODE_HPP
#define NODE_HPP

#include "Server.hpp"
#include "Member.hpp"
#include "Message.hpp"
#include "Logger.hpp"
#include "SharedQueue.hpp"
#include <boost/asio.hpp>
#include <utility>

#define TREMOVE 50000
#define TFAIL 10000

namespace boost_geometry = boost::geometry::model::d2;

class Node : public Member
{
	private:
		Server* server;
		SharedQueue<std::pair<std::pair<std::string, std::string>, q_elt>>* sndMsgsQ;
		SharedQueue<q_elt>* rcvMsgsQ;
		
	public:
		Node(boost::asio::io_service&, int); 
		~Node();

		//thread functions
		void init_mem_protocol(void);	
		void accept_user_input(void);
        void pushMessage(MsgType, Zone,  std::string = "", std::string = "");
        void recv(void);
		void sendLoop(void);
		
	private:
		void getMemberList(std::unique_ptr<char[]>, MsgType);
		void insertEntry(Address&, long, long long, Zone&);
		size_t size_of_message(MsgType);
        void displayInfo(Address&, std::vector<MemberListEntry>&);
        short getRandomReceivers(void);
        void fillMemberShipList(const char*, MsgType);
};

#endif  /* NODE_HPP */
