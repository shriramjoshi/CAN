
/**********************************
 * FILE NAME: Node.cpp
 * This the main class in CAN Protocol
 * Which servers as an individual node in network.
 * The 2 threads run in parallel for sending messages to other nodes in network
 * as well as receive any incoming messages on network. Here the mongo DB is used to store files.
 * BOOST C++ library is used for Socket programming.
 *
 * Created by Shriram Joshi on 5/1/17
 * DESCRIPTION: Definition of all Node related class
 *
 * Copyright © 2017 Balakrishna. All rights reserved.
 **********************************/
 
#include "Node.hpp"
#include "Message.hpp"
#include "Client.hpp"
#include <boost/thread.hpp>
#include <boost/chrono.hpp>
#include <boost/random.hpp>
#include <cstdlib>
#include <vector>
#include <set>
#include <iostream>
#include <memory>
#include <mutex>


Node::Node(boost::asio::io_service& io_service, int port)
{
    sndMsgsQ = new SharedQueue<std::pair<std::pair<std::string, std::string>, q_elt>>();
    rcvMsgsQ = new SharedQueue<q_elt>();
    server = new Server{io_service, port, rcvMsgsQ};
    self_address.init(this->getLocalIpAddress(), port);
    long long curtime =  boost::chrono::duration_cast<boost::chrono::milliseconds>
    (boost::chrono::system_clock::now().time_since_epoch()).count();
    memberList.emplace_back(self_address, ++heartbeat, curtime, self_zone);		// add self entry to member_list entry
}

Node::~Node()
{
	delete sndMsgsQ;
	delete rcvMsgsQ;
	delete server;
}

void Node::recv() 
{
	std::mutex recv_mutex;
	while(true)
	{
		if(!rcvMsgsQ->empty())
		{
			std::unique_ptr<char[]> data(new char[rcvMsgsQ->front().getElement().length()]);
			memcpy(&data[0], rcvMsgsQ->front().getElement().c_str(), rcvMsgsQ->front().getElement().length());
			size_t size = rcvMsgsQ->front().getSize();
			rcvMsgsQ->pop_front();
			
			int msgType = 0;
			memcpy(&msgType, &data[0], sizeof(int));
			//std::cout << "Received " << i << std::endl;
			switch(msgType)
			{
				case 0:
				{
					std::vector<MemberListEntry> memberList;
					this->getMemberList(std::move(data));
					std::vector<MemberListEntry>::iterator it_beg = this->memberList.begin();
					std::vector<MemberListEntry>::iterator it_end = this->memberList.end();
					for(; it_beg != it_end; ++it_beg) 
					{
						insertEntry(memberList, (*it_beg).getAddress(), (*it_beg).heartbeat, (*it_beg).timestamp);
					}
					break;
					break;
				}
				case 1:
		        {
		        	short xValue, yValue;
		            memcpy(&xValue, (short*)(&data[0] + sizeof(int) + (sizeof(char) * 4) + sizeof(short)), sizeof(short));
		            memcpy(&yValue, (short*)(&data[0] + sizeof(int) + (sizeof(char) * 4) + (sizeof(short) * 2)), sizeof(short));
		            boost_geometry::point_xy<short> pt;
		            boost::geometry::assign_values(pt, xValue, yValue);
		            if(self_zone.isCoordinateInZone(pt))
		            {
		                char addrA, addrB, addrC, addrD;
		                short port;
		                Zone new_zone = self_zone.splitZone();
		                memcpy(&addrA, (char*)(&data[0] + sizeof(int)), sizeof(char));
		                memcpy(&addrB, (char*)(&data[0] + sizeof(int) + sizeof(char) * 1), sizeof(char));
		                memcpy(&addrC, (char*)(&data[0] + sizeof(int) + sizeof(char) * 2), sizeof(char));
		                memcpy(&addrD, (char*)(&data[0] + sizeof(int) + sizeof(char) * 3), sizeof(char));
		                memcpy(&port, (short*)(&data[0] + sizeof(int) + sizeof(char) * 4), sizeof(short));
		                Address addr(addrA, addrB, addrC, addrD, port);
		                long long curtime =  boost::chrono::duration_cast<boost::chrono::milliseconds>
                        (boost::chrono::system_clock::now().time_since_epoch()).count();
                        
		                this->memberList.emplace_back(addr, 1, curtime, new_zone);	//heartbeat set to 1 by default
		                this->memberList.at(0).setZone(self_zone.p1, self_zone.p2, self_zone.p3, self_zone.p4);
		                
                        this->pushMessage(MsgType::JOINREP, std::move(new_zone), addr.to_string(), addr.port_to_string());

                        std::vector<MemberListEntry> temp;
                        for( size_t i = 0; i < memberList.size(); i++ )
                        {
                            if(self_address == memberList[i].getAddress() || isNeighbour(memberList[i].getZone()))
                            {
                                temp.push_back(memberList[i]);
                            }
                        }
                        std::swap( memberList, temp );
                        std::cout << "\n<----- JOINREQ ---- received (Coordinate in Zone)" << std::endl;
                        displayInfo(self_address, memberList);
		            }
					else
		            {
		            	short max_distance = MAX_COORDINATE;
		                std::vector<MemberListEntry>::iterator it_beg = memberList.begin();
		                std::vector<MemberListEntry>::iterator it_end = memberList.end();
		                std::string ipAddress;
		                std::string port;
		                for(; it_beg != it_end; ++it_beg)
		                {
		                    short distance = (*it_beg).findMinDistance(pt);
		                    if(distance < max_distance && self_address.port != (*it_beg).getAddress().port)
		                    {
		                        ipAddress =  (*it_beg).getAddress().to_string();
		                        port = (*it_beg).getAddress().port_to_string();
		                    }
		                }
		                std::string sData(&data[0], &data[0] + size);
		                q_elt element(sData, size);
		                auto addr = std::make_pair(ipAddress, port);
		                auto sndMsg = std::make_pair(addr, element);
		                sndMsgsQ->push_back(sndMsg);
                        
                        std::cout << "\n-----  JOINREQ ----> forwarded (Coordinate not in Zone) ----> [PORT #] " << port << std::endl;
                        displayInfo(self_address, memberList);
		            }
                    break;
		        }
		        case 2:
		        {
		            std::cout << "\n<----- JOINREP ---- received:" << std::endl;
		            short p1_x, p1_y, p2_x, p2_y, p3_x, p3_y, p4_x, p4_y;
		            memcpy(&p1_x, &data[0] + sizeof(int) + (sizeof(char) * 4) + sizeof(short), sizeof(short));
		            memcpy(&p1_y, &data[0] + sizeof(int) + (sizeof(char) * 4) + sizeof(short) * 2, sizeof(short));
		            memcpy(&p2_x, &data[0] + sizeof(int) + (sizeof(char) * 4) + sizeof(short) * 3, sizeof(short));
		            memcpy(&p2_y, &data[0] + sizeof(int) + (sizeof(char) * 4) + sizeof(short) * 4, sizeof(short));
		            memcpy(&p3_x, &data[0] + sizeof(int) + (sizeof(char) * 4) + sizeof(short) * 5, sizeof(short));
		            memcpy(&p3_y, &data[0] + sizeof(int) + (sizeof(char) * 4) + sizeof(short) * 6, sizeof(short));
		            memcpy(&p4_x, &data[0] + sizeof(int) + (sizeof(char) * 4) + sizeof(short) * 7, sizeof(short));
		            memcpy(&p4_y, &data[0] + sizeof(int) + (sizeof(char) * 4) + sizeof(short) * 8, sizeof(short));
		           
		            boost::geometry::assign_values(self_zone.p1, p1_x, p1_y);
		            boost::geometry::assign_values(self_zone.p2, p2_x, p2_y);
		            boost::geometry::assign_values(self_zone.p3, p3_x, p3_y);
		            boost::geometry::assign_values(self_zone.p4, p4_x, p4_y);
		            memberList.at(0).setZone(self_zone.p1, self_zone.p2, self_zone.p3, self_zone.p4);
		            
                    this->inGroup = true;
		            getMemberList(std::move(data));
                    
                    std::cout << "\n<----- JOINREP ---- received:" << std::endl;
                    displayInfo(self_address, memberList);
                }
	            break;
		            
		        case 3:
		        {
		        	short p1_x, p1_y, p2_x, p2_y, p3_x, p3_y, p4_x, p4_y;
		            memcpy(&p1_x, (short*)(&data[0] + sizeof(int) + sizeof(char) * 4 + sizeof(short)), sizeof(short));
		            memcpy(&p1_y, (short*)(&data[0] + sizeof(int) + sizeof(char) * 4 + sizeof(short) * 2), sizeof(short));
		            
		            memcpy(&p2_x, (short*)(&data[0] + sizeof(int) + sizeof(char) * 4 + sizeof(short) * 3), sizeof(short));
		            memcpy(&p2_y, (short*)(&data[0] + sizeof(int) + sizeof(char) * 4 + sizeof(short) * 4), sizeof(short));
		            
		            memcpy(&p3_x, (short*)(&data[0] + sizeof(int) + sizeof(char) * 4 + sizeof(short) * 5), sizeof(short));
		            memcpy(&p3_y, (short*)(&data[0] + sizeof(int) + sizeof(char) * 4 + sizeof(short) * 6), sizeof(short));
		            
		            memcpy(&p4_x, (short*)(&data[0] + sizeof(int) + sizeof(char) * 4 + sizeof(short) * 7), sizeof(short));
		            memcpy(&p4_y, (short*)(&data[0] + sizeof(int) + sizeof(char) * 4 + sizeof(short) * 8), sizeof(short));
		            
		            Zone newZone;
		            boost::geometry::assign_values(newZone.p1, p1_x, p1_y);
		            boost::geometry::assign_values(newZone.p2, p2_x, p2_y);
		            boost::geometry::assign_values(newZone.p3, p3_x, p3_y);
		            boost::geometry::assign_values(newZone.p4, p4_x, p4_y);
		            
		            //Merge Zone
		            self_zone.mergeZone(newZone);
		            
		            // update membership list
		            getMemberList(std::move(data));
                    
                    std::cout << "\n <----- LEAVEREQ received --- " << std::endl;
                    displayInfo(self_address, memberList);

		        }
		        break;
		        
		        default:
		            break;
			}
		}
	}
}

void Node::sendLoop()
{
	std::mutex snd_mutex;
	while(true)
	{
		if(!sndMsgsQ->empty())
		{
			std::lock_guard<std::mutex> sndGuard(snd_mutex);
			boost::asio::io_service io_service;
			auto address = sndMsgsQ->front().first.first;
		    auto port = sndMsgsQ->front().first.second;
		    Client client(io_service, address, port);
			client.write(sndMsgsQ->front().second.getElement());
			sndMsgsQ->pop_front();
		}
	}
}

size_t Node::size_of_message(MsgType type)
{
    size_t msgsize = sizeof(int) + (4 * sizeof(char) ) + sizeof(short);
    if(type == MsgType::JOINREQ)
    {
        msgsize += (2 * sizeof(short));	//Requester's X & Y coordinates
    }
    else if(type == MsgType::JOINREP)
    {
        msgsize += (sizeof(short) * 8) /*zone coordinates*/ + sizeof(short) /* number of entries in memberlist*/;
        msgsize += (sizeof(char) * 4 + sizeof(short) + sizeof(long) + sizeof(long long) + sizeof(short) * 8) * memberList.size();
    }
    return msgsize;
}

void Node::pushMessage(MsgType type, Zone zone, std::string toAddr, std::string toPort)
{
	//#define JOINREP_ 2
	size_t size = size_of_message(type);
    std::unique_ptr<char[]> msgBlock(new char[size]);
    int msgType = static_cast<int>(type);
    // Copy Message Type
    memcpy(&msgBlock[0], &msgType, sizeof(int));
    
    // Copy sending node's ip-address & port 
    memcpy((char*)(&msgBlock[0] + sizeof(int)), &self_address.addrA, sizeof(char));
    memcpy((char*)(&msgBlock[0] + sizeof(int)) + sizeof(char) * 1, &self_address.addrB, sizeof(char));
    memcpy((char*)(&msgBlock[0] + sizeof(int)) + sizeof(char) * 2, &self_address.addrC, sizeof(char));
    memcpy((char*)(&msgBlock[0] + sizeof(int)) + sizeof(char) * 3, &self_address.addrD, sizeof(char));
    memcpy((char*)(&msgBlock[0] + sizeof(int)) + sizeof(char) * 4, &self_address.port, sizeof(short));
    
    if(type == MsgType::JOINREQ)
    {
    	memcpy((char*)(&msgBlock[0] + sizeof(int)) + (sizeof(char) * 4) + sizeof(short), &point.x(), sizeof(short));
        memcpy((char*)(&msgBlock[0] + sizeof(int)) + (sizeof(char) * 4) + sizeof(short) * 2, &point.y(), sizeof(short));
    }
    else if(type == MsgType::JOINREP)
    {
    	memcpy((char*)(&msgBlock[0] + sizeof(int)) + (sizeof(char) * 4) + sizeof(short), &point.x(), sizeof(short));
        memcpy((char*)(&msgBlock[0] + sizeof(int)) + (sizeof(char) * 4) + sizeof(short) * 2, &point.y(), sizeof(short));
    	memcpy((char*)(&msgBlock[0] + sizeof(int)) + (sizeof(char) * 4) + sizeof(short), &zone.p1.x(), sizeof(short));
        memcpy((char*)(&msgBlock[0] + sizeof(int)) + (sizeof(char) * 4) + sizeof(short) * 2, &zone.p1.y(), sizeof(short));

        memcpy((char*)(&msgBlock[0] + sizeof(int)) + (sizeof(char) * 4) + sizeof(short) * 3, &zone.p2.x(), sizeof(short));
        memcpy((char*)(&msgBlock[0] + sizeof(int)) + (sizeof(char) * 4) + sizeof(short) * 4, &zone.p2.y(), sizeof(short));
        
        memcpy((char*)(&msgBlock[0] + sizeof(int)) + (sizeof(char) * 4) + sizeof(short) * 5, &zone.p3.x(), sizeof(short));
        memcpy((char*)(&msgBlock[0] + sizeof(int)) + (sizeof(char) * 4) + sizeof(short) * 6, &zone.p3.y(), sizeof(short));

        memcpy((char*)(&msgBlock[0] + sizeof(int)) + (sizeof(char) * 4) + sizeof(short) * 7, &zone.p4.x(), sizeof(short));
        memcpy((char*)(&msgBlock[0] + sizeof(int)) + (sizeof(char) * 4) + sizeof(short) * 8, &zone.p4.y(), sizeof(short));
		
		//short size_membership_list = memberList.size();
		//memcpy((char*)(&msgBlock[0] + sizeof(int)) + sizeof(char) + (sizeof(char) * 4) + sizeof(short) * 9, &size_membership_list, sizeof(short));
        //fillMemberShipList(hdr);
    }
    std::string data(&msgBlock[0], &msgBlock[0] + size);
    q_elt el(std::move(data), size);
    auto addr = std::make_pair(toAddr, toPort);
    auto sndMsg = std::make_pair(addr, el);
    sndMsgsQ->push_back(sndMsg);
}

void Node::displayInfo(Address& addr, std::vector<MemberListEntry>& member_list)
{
    std::cout << "\nIP Address: " << addr.to_string() << std::endl;
    std::cout << "Port: " << addr.port_to_string() << std::endl;
    std::cout << "Neighbours: " <<  member_list.size() << std::endl;
    std::vector<MemberListEntry>::iterator it_beg = member_list.begin();
    std::vector<MemberListEntry>::iterator it_end = member_list.end();
    for(int index = 1; it_beg != it_end; ++it_beg, ++index)
    {
        std::cout << index << ". IP Address: " << (*it_beg).getAddress().to_string() << " ";
        std::cout << "Port: " << (*it_beg).getAddress().port_to_string() << std::endl;
        std::cout << "Zone: " << (*it_beg).getZone().to_String() << std::endl;
    }
}

void Node::getMemberList(std::unique_ptr<char[]> data) 
{
	short size = 0;
	memcpy(&size, &data[0] + sizeof(int) + (sizeof(char) * 4) + sizeof(short) * 9, sizeof(short));
 	char* ptr = &data[0] + sizeof(int) + (sizeof(char) * 4) + sizeof(short) * 10;
	for(int i = 0; i < size; ++i) 
	{
		MemberListEntry entry;
		memcpy(&entry.getAddress().addrA, ptr, sizeof(char));
		memcpy(&entry.getAddress().addrB, ptr + sizeof(char), sizeof(char));
		memcpy(&entry.getAddress().addrC, ptr + sizeof(char) * 2, sizeof(char));
		memcpy(&entry.getAddress().addrD, ptr + sizeof(char) * 3, sizeof(char));
		memcpy(&entry.getAddress().port, ptr + sizeof(char) * 4, sizeof(short));
		memcpy(&entry.heartbeat, ptr + sizeof(char) * 4 + sizeof(short), sizeof(long));
		memcpy(&entry.timestamp, ptr + sizeof(char) * 4 + sizeof(short) + sizeof(long), sizeof(long long));
		
		
		short p1_x, p1_y, p2_x, p2_y, p3_x, p3_y, p4_x, p4_y;
		memcpy(&p1_x, ptr + sizeof(char) * 4 + sizeof(short) + sizeof(long) + sizeof(long long), sizeof(short));
		memcpy(&p1_y, ptr + sizeof(char) * 4 + sizeof(short) + sizeof(long) + sizeof(long long) + sizeof(short), sizeof(short));
		memcpy(&p2_x, ptr + sizeof(char) * 4 + sizeof(short) + sizeof(long) + sizeof(long long) + (sizeof(short) * 2), sizeof(short));
		memcpy(&p2_y, ptr + sizeof(char) * 4 + sizeof(short) + sizeof(long) + sizeof(long long) + (sizeof(short) * 3), sizeof(short));
		
		memcpy(&p3_x, ptr + sizeof(char) * 4 + sizeof(short) + sizeof(long) + sizeof(long long) + (sizeof(short) * 4), sizeof(short));
		memcpy(&p3_y, ptr + sizeof(char) * 4 + sizeof(short) + sizeof(long) + sizeof(long long) + (sizeof(short) * 5), sizeof(short));
		
		memcpy(&p4_x, ptr + sizeof(char) * 4 + sizeof(short) + sizeof(long) + sizeof(long long) + (sizeof(short) * 6), sizeof(short));
		memcpy(&p4_y, ptr + sizeof(char) * 4 + sizeof(short) + sizeof(long) + sizeof(long long) + (sizeof(short) * 7), sizeof(short));
		
		boost::geometry::assign_values(entry.zone.p1, p1_x, p1_y);
		boost::geometry::assign_values(entry.zone.p2, p2_x, p2_y);
		boost::geometry::assign_values(entry.zone.p3, p3_x, p3_y);
		boost::geometry::assign_values(entry.zone.p4, p4_x, p4_y);
		
		entry.setZone(entry.zone.p1, entry.zone.p2, entry.zone.p3, entry.zone.p4);
		ptr += (4 * sizeof(char)) + sizeof(short) + sizeof(long) + sizeof(long long) + (sizeof(short) * 8);
    	if(isNeighbour(entry.getZone()))
    	{
            if(self_address == entry.getAddress()) { continue; }
            memberList.emplace_back(entry.getAddress(), entry.heartbeat, entry.timestamp, entry.zone);
    	}
	}
	return;
}

/**
 * FUNCTION NAME: insertEntry
 *
 * DESCRIPTION: Creates member list entry & inserts into vector
*/
void Node::insertEntry(std::vector<MemberListEntry>& memberList, Address& address, long heartbeat, long long timestamp) {
	
    std::vector<MemberListEntry>::iterator it_beg = memberList.begin();
	std::vector<MemberListEntry>::iterator it_end = memberList.end();
	MemberListEntry entry(address, heartbeat, timestamp);
	
    if(address == (*it_beg).getAddress()) return;
	if(it_beg == it_end) 
	{
		entry.timestamp = boost::chrono::duration_cast<boost::chrono::milliseconds>
        (boost::chrono::system_clock::now().time_since_epoch()).count();
		memberList.push_back(entry);
	}
	else 
	{
		for(; it_beg != it_end; ++it_beg) 
		{
			if(entry.getAddress() == (*it_beg).getAddress())
			{
				if((*it_beg).bDeleted == true)
				{
					//if neighbour was previously deleted
					if((*it_beg).timestamp < entry.timestamp && entry.heartbeat > (*it_beg).heartbeat)
					{
                        (*it_beg).timestamp = boost::chrono::duration_cast<boost::chrono::milliseconds>
                        (boost::chrono::system_clock::now().time_since_epoch()).count();
                        (*it_beg).heartbeat = entry.heartbeat;
                        return;
					}	
				}
				else if(entry.heartbeat > (*it_beg).heartbeat)
				{
					(*it_beg).timestamp = boost::chrono::duration_cast<boost::chrono::milliseconds>
                    (boost::chrono::system_clock::now().time_since_epoch()).count();
					(*it_beg).heartbeat = entry.heartbeat;
					return;
				}
			}
		}
		entry.timestamp = boost::chrono::duration_cast<boost::chrono::milliseconds>
        (boost::chrono::system_clock::now().time_since_epoch()).count();
		memberList.push_back(entry);
	}
	return;
}

void Node::accept_user_input()
{
    std::string ipAddress, port;
    Zone zone;
    short option = 0;
    while(true)
    {
        std::cout << "____________MENU____________" << std::endl;
        std::cout << "1. Create Network" << std::endl;
        std::cout << "2. Join Network" << std::endl;
        std::cout << "3. Leave Network" << std::endl;
        std::cout << "4. View Network" << std::endl;
        std::cout << "Enter choice: ";
        std::cin >> option;
        
        switch(option)
        {
            case 1: inGroup = true;
                    std::cout << "\n******* Network Created ********\n" << std::endl;
                break;
            case 2:
                if(inGroup)
                {
                    //displayInfo(self_address, memberList);
                    std::cout << "\n******* Connected ********\n" << std::endl;
                    break;
                }
                std::cout << "\nIp Address: ";
                std::cin >> ipAddress;
                std::cout << "Port: ";
                std::cin >> port;
                
                pushMessage(MsgType::JOINREQ, zone, ipAddress, port);
                inGroup = true;
                break;
            case 3:
            {
                if(!inGroup)
                {
                    std::cout << "\n******* Disconnected ********\n" << std::endl;
                    break;
                }
                std::vector<MemberListEntry>::iterator it_beg = memberList.begin();
                std::vector<MemberListEntry>::iterator it_end = memberList.end();
                for(; it_beg != it_end; ++it_beg)
                {
                    if(self_zone.canMergeZone((*it_beg).getZone()))
                    {
                        std::string addr = (*it_beg).getAddress().to_string();
                        std::string port = (*it_beg).getAddress().port_to_string();
                        pushMessage(MsgType::LEAVEREQ, self_zone, addr, port);
                        inGroup = false;
                        break;
                    }
                }
                std::cout << "\n******* Disconnected ********\n" << std::endl;
            }
                break;
            case 4:
                //displayInfo(self_address, memberList);
                break;
        }
    }
}

int main(int argc, char* argv[])
{
	if(argc != 2)
	{
		std::cerr << "Node <port>" << std::endl;
        return -1;
	}
	boost::asio::io_service io_service;
    Node node(io_service, atoi(argv[1]));
    
    boost::thread t1([&io_service](){io_service.run();});
    //boost::thread t2(boost::bind(&Node::init_mem_protocol, &node));
	boost::thread* t3 = new boost::thread(boost::bind(&Node::recv, boost::ref(node)));
	boost::thread* t4 = new boost::thread(boost::bind(&Node::sendLoop, boost::ref(node)));
	boost::thread t5(boost::bind(&Node::accept_user_input, &node));
    
    
    t1.join();
    //t2.join();
    t3->join();
    t4->join();
    t5.join();

	return 0;
}
