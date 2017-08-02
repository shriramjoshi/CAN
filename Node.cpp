
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
					std::cout << "Heartbeat received" << std::endl;
					std::vector<MemberListEntry> memberList;
					getMemberList(std::move(data), MsgType::HEARTBEAT);
				}
				break;
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

                        SharedVector<MemberListEntry> temp;
                        /*for( size_t i = 0; i < memberList.size(); i++ )
                        {
                            if(self_address == memberList[i].getAddress() || isNeighbour(memberList[i].getZone()))
                            {
                                temp.push_back(memberList[i]);
                            }
                        }*/
                        //memberList.swap(temp);
                        std::cout << "\n<----- JOINREQ ---- received (Coordinate in Zone)" << std::endl;
                        std::cout << memberList.size() << std::endl;
                        //displayInfo(self_address, memberList);
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
                        //displayInfo(self_address, memberList);
		            }
                    
		        }
		        break;
		        case 2:
		        {
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
		            
                    getMemberList(std::move(data), MsgType::JOINREP);
                    
                    std::cout << "\n<----- JOINREP ---- received:" << std::endl;
                    std::cout << memberList.size() << std::endl;
                    //displayInfo(self_address, memberList);
                	this->inGroup = true;
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
		            //getMemberList(std::move(data));
                    
                    std::cout << "\n <----- LEAVEREQ received --- " << std::endl;
                    //displayInfo(self_address, memberList);

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
		    //std::cerr << "Addr " << address << std::endl;
		    Client client(io_service, address, port);
			client.write(sndMsgsQ->front().second.getElement());
			sndMsgsQ->pop_front();
		}
	}
}

size_t Node::size_of_message(MsgType type)
{
    size_t msgsize = sizeof(int) + (4 * sizeof(char) ) + sizeof(short);
    if(type == MsgType::HEARTBEAT)
    {
    	msgsize += (sizeof(char) * 4 + sizeof(short) + sizeof(long) + sizeof(long long) + sizeof(short) * 8) * memberList.size();
    }
    else if(type == MsgType::JOINREQ)
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
		
		short size_membership_list = memberList.size();
		memcpy((char*)(&msgBlock[0] + sizeof(int)) + (sizeof(char) * 4) + sizeof(short) * 9, &size_membership_list, sizeof(short));
        fillMemberShipList(msgBlock.get(), MsgType::JOINREP);
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

std::mutex mt;

void Node::getMemberList(std::unique_ptr<char[]> data, MsgType msgType)
{
	std::lock_guard<std::mutex> guard(mt);
	short size = 0;
	auto ptr = data.get();
	if(msgType == MsgType::JOINREP)
	{
		memcpy(&size, &data[0] + sizeof(int) + (sizeof(char) * 4) + sizeof(short) * 9, sizeof(short));
		ptr += sizeof(int) + (sizeof(char) * 4) + sizeof(short) * 10;
	}
	else if(msgType == MsgType::HEARTBEAT)
	{
		memcpy(&size, &data[0] + sizeof(int) + (sizeof(char) * 4) + sizeof(short), sizeof(short));
		ptr += sizeof(int) + (sizeof(char) * 4) + sizeof(short) * 2;
	}
	
	for(int i = 0; i < size;) 
	{
		//std::cerr << "1" << std::endl;
		MemberListEntry entry;
		memcpy(&entry.getAddress().addrA, ptr, sizeof(char));
		//std::cerr << "size after insert " << entry.getAddress().addrA << std::endl;
		memcpy(&entry.getAddress().addrB, ptr + sizeof(char), sizeof(char));
		//std::cerr << "size after insert " << entry.getAddress().addrB << std::endl;
		memcpy(&entry.getAddress().addrC, ptr + sizeof(char) * 2, sizeof(char));
		//std::cerr << "size after insert " << entry.getAddress().addrC << std::endl;
		memcpy(&entry.getAddress().addrD, ptr + sizeof(char) * 3, sizeof(char));
		//std::cerr << "size after insert " << entry.getAddress().addrD << std::endl;
		memcpy(&entry.getAddress().port, ptr + sizeof(char) * 4, sizeof(short));
		//std::cerr << "size after insert " << entry.getAddress().port << std::endl;
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
            insertEntry(entry.getAddress(), entry.heartbeat, entry.timestamp, entry.zone);
            
    	}
    	i += (4 * sizeof(char)) + sizeof(short) + sizeof(long) + sizeof(long long) + (sizeof(short) * 8);
    	//std::cerr << "2 " << i << std::endl;
    }
	return;
}

/**
 * FUNCTION NAME: insertEntry
 *
 * DESCRIPTION: Creates member list entry & inserts into vector
*/
void Node::insertEntry(Address& address, long heartbeat, long long timestamp, Zone& zone) {
	
    std::vector<MemberListEntry>::iterator it_beg = memberList.begin();
	std::vector<MemberListEntry>::iterator it_end = memberList.end();
	//MemberListEntry entry(address, heartbeat, timestamp);
	
    if(address == (*it_beg).getAddress()) return;
	if(it_beg == it_end) 
	{
		long long timestamp = boost::chrono::duration_cast<boost::chrono::milliseconds>
        (boost::chrono::system_clock::now().time_since_epoch()).count();
		memberList.emplace_back(address, heartbeat, timestamp, zone);
	}
	else 
	{
		for(; it_beg != it_end; ++it_beg) 
		{
			if(address == (*it_beg).getAddress())
			{
				if((*it_beg).bDeleted == true)
				{
					//if neighbour was previously deleted
					if((*it_beg).timestamp < timestamp && heartbeat > (*it_beg).heartbeat)
					{
                        (*it_beg).timestamp = boost::chrono::duration_cast<boost::chrono::milliseconds>
                        (boost::chrono::system_clock::now().time_since_epoch()).count();
                        (*it_beg).heartbeat = heartbeat;
                        return;
					}	
				}
				else if(heartbeat > (*it_beg).heartbeat)
				{
					(*it_beg).timestamp = boost::chrono::duration_cast<boost::chrono::milliseconds>
                    (boost::chrono::system_clock::now().time_since_epoch()).count();
					(*it_beg).heartbeat = heartbeat;
					return;
				}
			}
		}
		timestamp = boost::chrono::duration_cast<boost::chrono::milliseconds>
        (boost::chrono::system_clock::now().time_since_epoch()).count();
		memberList.emplace_back(address, heartbeat, timestamp, zone);
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
            case 1: 
            		inGroup = true;
                    std::cout << "\n******* Network Created ********\n" << std::endl;
                break;
            case 2:
                if(inGroup)
                {
                    ////displayInfo(self_address, memberList);
                    std::cout << "\n******* Connected ********\n" << std::endl;
                    break;
                }
                std::cout << "\nIp Address: ";
                std::cin >> ipAddress;
                std::cout << "Port: ";
                std::cin >> port;
                
                pushMessage(MsgType::JOINREQ, zone, ipAddress, port);
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
                ////displayInfo(self_address, memberList);
                break;
        }
    }
}

std::mutex mt1;
void Node::fillMemberShipList(const char* msg, MsgType msgType)
{
	size_t size = 0;
	std::lock_guard<std::mutex> guard(mt1);
	if(msgType == MsgType::JOINREP)
	{
		size = sizeof(int) + (4 * sizeof(char) ) + sizeof(short) * 10;
		//std::cerr << " JOINREPLY " << memberList.size() << std::endl;
	}
	else if(msgType == MsgType::HEARTBEAT)
		size = sizeof(int) + (4 * sizeof(char) ) + sizeof(short) * 2;
	auto ptr = msg + size;
	
	std::vector<MemberListEntry>::iterator it = memberList.begin();
	for(; it != memberList.end(); ++it) 
	{
		if(!(*it).bDeleted)  //If the neighbour is alive
		{
			MemberListEntry entry((*it).getAddress(), (*it).heartbeat, (*it).timestamp, (*it).zone);
			memcpy((char *)(&ptr[0]), &(entry.getAddress().addrA), sizeof(char));
			memcpy((char *)(&ptr[0] + sizeof(char)), &entry.getAddress().addrB, sizeof(char));
			memcpy((char *)(&ptr[0] + sizeof(char) * 2), &entry.getAddress().addrC, sizeof(char));
			memcpy((char *)(&ptr[0] + sizeof(char) * 3), &entry.getAddress().addrD, sizeof(char));
			memcpy((char* )(&ptr[0] + sizeof(char) * 4), &entry.getAddress().port, sizeof(short));
			memcpy((char* )(&ptr[0] + sizeof(char) * 4) + sizeof(short), &entry.heartbeat, sizeof(long));
			memcpy((char* )(&ptr[0] + sizeof(char) * 4) + sizeof(short) + sizeof(long), &entry.timestamp, sizeof(long long));
			
			// Copy zone of each node
			memcpy((char* )(&ptr[0] + (sizeof(char) * 4) + sizeof(short) + sizeof(long) 
			+ sizeof(long long)), &entry.zone.p1.x(), sizeof(short));
			
			memcpy((char* )(&ptr[0] + (sizeof(char) * 4) + sizeof(short) + sizeof(long)
			 + sizeof(long long) + sizeof(short)), &entry.zone.p1.y(), sizeof(short));
			 
			memcpy((char* )(&ptr[0] + (sizeof(char) * 4) + sizeof(short) + sizeof(long)
			 + sizeof(long long) + sizeof(short) * 2), &entry.zone.p2.x(), sizeof(short));
			 
			memcpy((char* )(&ptr[0] + (sizeof(char) * 4) + sizeof(short) + sizeof(long)
			 + sizeof(long long) + sizeof(short) * 3), &entry.zone.p2.y(), sizeof(short));
			 
			memcpy((char* )(&ptr[0] + (sizeof(char) * 4) + sizeof(short) + sizeof(long)
			 + sizeof(long long) + sizeof(short) * 4), &entry.zone.p3.x(), sizeof(short));
			 
			memcpy((char* )(&ptr[0] + (sizeof(char) * 4) + sizeof(short) + sizeof(long)
			 + sizeof(long long) + sizeof(short) * 5), &entry.zone.p3.y(), sizeof(short));
			 
			memcpy((char* )(&ptr[0] + (sizeof(char) * 4) + sizeof(short) + sizeof(long)
			 + sizeof(long long) + sizeof(short) * 6), &entry.zone.p4.x(), sizeof(short));
			 
			memcpy((char* )(&ptr[0] + (sizeof(char) * 4) + sizeof(short) + sizeof(long)
			 + sizeof(long long) + sizeof(short) * 7), &entry.zone.p4.y(), sizeof(short));
			
			ptr = ptr + (sizeof(char) * 4) + sizeof(short) + sizeof(long) + sizeof(long long) + sizeof(short) * 8;
		}
	}	
}

void Node::init_mem_protocol(void)
{
	while(true)
	{
		//First check if any removable node
		if(inGroup)
		{
			if(memberList.size() > 1)
			{
				std::vector<MemberListEntry>::iterator it = memberList.begin();
				std::vector<MemberListEntry>::iterator it_end = memberList.end();
				for( /* member list first entry is self node */++it; it != it_end; ++it) 
				{
		            long long curtime =  boost::chrono::duration_cast<boost::chrono::milliseconds>
		            (boost::chrono::system_clock::now().time_since_epoch()).count();
					if((!(*it).bDeleted) && ((curtime - (*it).timestamp) > (TREMOVE + TFAIL)))
					{
						(*it).bDeleted = true;
						std::cerr << "deleted " << (*it).getAddress().port_to_string() << std::endl;
					}
				}
			}
		
			//send membership list to neighbours
			if(memberList.size() > 1)	// first entry is self node entry
			{
				//std::cerr << "aya idhar" << std::endl;
				std::set<int> vec_receivers;
				std::set<int>::iterator it;
			  	std::pair<std::set<int>::iterator,bool> ret;
				short rand_receivers = getRandomReceivers();
				//Increment self heat beat by before sending to neighbours
				memberList.at(0).heartbeat++;
			
				for(int i = 0; i < rand_receivers; ++i) 
				{
					int rand_rcvr = getRandomReceivers() % memberList.size();
					//std::cerr << rand_rcvr << "$$$$$$$$$" << std::endl;
					ret = vec_receivers.insert(rand_rcvr);
					//No receiver gets heartbeat twice
					if(ret.second != false) 
					{
						MemberListEntry receiver = memberList.at(rand_rcvr);
						Address to(receiver.getAddress());
						//No self messaging
						if(!(to == this->self_address))
						{
							size_t msgsize = size_of_message(MsgType::HEARTBEAT);
							std::cout << msgsize << std::endl;
							std::unique_ptr<char[]> msg(new char[msgsize]);
							int msgType = static_cast<int>(MsgType::HEARTBEAT);
							memcpy((char *)(&msg[0]), &msgType, sizeof(int));
							memcpy((char *)(&msg[0] + sizeof(int)), &self_address.addrA, sizeof(char));
							memcpy((char *)(&msg[0] + sizeof(int) + sizeof(char)), &self_address.addrB, sizeof(char));
							memcpy((char *)(&msg[0] + sizeof(int) + sizeof(char)  * 2), &self_address.addrC, sizeof(char));
							memcpy((char *)(&msg[0] + sizeof(int) + sizeof(char) * 3), &self_address.addrD, sizeof(char));
							memcpy((char* )(&msg[0] + sizeof(int) + sizeof(char) * 4), &self_address.port, sizeof(short));
							short numListEntries = (short)memberList.size();
							//std::cerr << "****************" << numListEntries << std::endl;
							memcpy((char* )(&msg[0] + sizeof(int) + (sizeof(char) * 4) + sizeof(short)), &numListEntries, sizeof(short));
						
		                    fillMemberShipList(msg.get(), MsgType::HEARTBEAT);
							auto addressPair = std::make_pair(to.to_string(), to.port_to_string());
							//const char* buf = (const char*)msg;
							std::string sBuf(&msg[0], &msg[0] + msgsize);
							q_elt el(sBuf, msgsize);
							auto sndMsg = std::make_pair(addressPair, el);
							sndMsgsQ->push_back(sndMsg);
						}
					}
				}
			}
		}
		else {
			//std::cerr << "Not in group" << std::endl;
		}
		sleep(5);
	}
}

short Node::getRandomReceivers(void) 
{
	int max = memberList.size() - 1;
	std::time_t now = std::time(0);
    boost::random::mt19937 gen{static_cast<std::uint32_t>(now)};
    boost::random::uniform_int_distribution<> dist{0, max};
	return dist(gen);
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
    boost::thread t3(boost::bind(&Node::recv, boost::ref(node)));
	boost::thread t4(boost::bind(&Node::sendLoop, boost::ref(node)));
	boost::thread t5(boost::bind(&Node::accept_user_input, &node));
    boost::thread t2(boost::bind(&Node::init_mem_protocol, &node));
    
    t1.join();
    t2.join();
    t3.join();
    t4.join();
    t5.join();

	return 0;
}
