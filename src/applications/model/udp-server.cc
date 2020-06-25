/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 *  Copyright(c) 2007,2008,2009 INRIA, UDCAST
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Author: Amine Ismail <amine.ismail@sophia.inria.fr>
 *                      <amine.ismail@udcast.com>
 */

#include "ns3/log.h"
#include "ns3/ipv4-address.h"
#include "ns3/nstime.h"
#include "ns3/inet-socket-address.h"
#include "ns3/inet6-socket-address.h"
#include "ns3/socket.h"
#include "ns3/simulator.h"
#include "ns3/socket-factory.h"
#include "ns3/packet.h"
#include "ns3/uinteger.h"
#include "ns3/udp-cc-header.h"
#include "packet-loss-counter.h"
#include "udp-server.h"

typedef union {
    uint8_t buf[12];
    struct {
        int64_t recvTime;
        uint32_t lost;
    };
} message_t;

namespace ns3 {

    NS_LOG_COMPONENT_DEFINE("UdpServer");
    NS_OBJECT_ENSURE_REGISTERED(UdpServer);

    TypeId UdpServer::GetTypeId(void) {
        static TypeId tid = TypeId("ns3::UdpServer")
            .SetParent<Application>()
            .SetGroupName("Applications")
            .AddConstructor<UdpServer>()
            .AddAttribute("Port",
                          "Port on which we listen for incoming packets.",
                          UintegerValue(100),
                          MakeUintegerAccessor(&UdpServer::m_port),
                          MakeUintegerChecker<uint16_t>())
            .AddAttribute("PacketWindowSize",
                          "The size of the window used to compute the packet loss. This value should be a multiple of 8.",
                          UintegerValue(32),
                          MakeUintegerAccessor(&UdpServer::GetPacketWindowSize,
                                               &UdpServer::SetPacketWindowSize),
                          MakeUintegerChecker<uint16_t>(8,256))
            .AddTraceSource("Rx", "A packet has been received",
                            MakeTraceSourceAccessor(&UdpServer::m_rxTrace),
                            "ns3::Packet::TracedCallback")
            .AddTraceSource("RxWithAddresses", "A packet has been received",
                            MakeTraceSourceAccessor(&UdpServer::m_rxTraceWithAddresses),
                            "ns3::Packet::TwoAddressTracedCallback")
            .AddTraceSource("Delay", "A delay value when packet has been received",
                            MakeTraceSourceAccessor(&UdpServer::m_delayTrace),
                            "ns3::Time::TracedCallback")
        ;
        return tid;
    }

    UdpServer::UdpServer() : m_lossCounter(0) {
        NS_LOG_FUNCTION(this);
        m_received = 0;
        m_totalRx = 0;
        m_lastFeedback = Time(0);
        m_totalDelay = Time(0);
        m_totalDelayCount = 0;
   }

    UdpServer::~UdpServer() {
        NS_LOG_FUNCTION(this);
    }

    uint64_t UdpServer::GetTotalRx() const {
        NS_LOG_FUNCTION(this);
        return m_totalRx;
    }

    uint16_t UdpServer::GetPacketWindowSize() const {
        NS_LOG_FUNCTION(this);
        return m_lossCounter.GetBitMapSize();
    }

    void UdpServer::SetPacketWindowSize(uint16_t size) {
        NS_LOG_FUNCTION(this << size);
        m_lossCounter.SetBitMapSize(size);
    }

    uint32_t UdpServer::GetLost(void) const {
        NS_LOG_FUNCTION(this);
        return m_lossCounter.GetLost();
    }

    uint64_t UdpServer::GetReceived(void) const {
        NS_LOG_FUNCTION(this);
        return m_received;
    }

    Time UdpServer::GetDelayAvg(void) const {
        return m_totalDelay / m_totalDelayCount;
    }

    void UdpServer::DoDispose(void) {
        NS_LOG_FUNCTION(this);
        Application::DoDispose();
    }

    void UdpServer::StartApplication(void) {
        NS_LOG_FUNCTION(this);

        if (m_socket == 0) {
            TypeId tid = TypeId::LookupByName("ns3::UdpSocketFactory");
            m_socket = Socket::CreateSocket(GetNode(), tid);
            InetSocketAddress local = InetSocketAddress(Ipv4Address::GetAny(), m_port);
            if (m_socket->Bind(local) == -1) {
                NS_FATAL_ERROR("Failed to bind socket");
            }
        }

        m_socket->SetRecvCallback(MakeCallback(&UdpServer::HandleRead, this));

        if (m_socket6 == 0) {
            TypeId tid = TypeId::LookupByName("ns3::UdpSocketFactory");
            m_socket6 = Socket::CreateSocket(GetNode(), tid);
            Inet6SocketAddress local = Inet6SocketAddress(Ipv6Address::GetAny(), m_port);
            if (m_socket6->Bind(local) == -1) {
                NS_FATAL_ERROR("Failed to bind socket");
            }
        }

        m_socket6->SetRecvCallback(MakeCallback(&UdpServer::HandleRead, this));
    }

    void UdpServer::StopApplication() {
        NS_LOG_FUNCTION(this);

        if (m_socket != 0) {
            m_socket->SetRecvCallback(MakeNullCallback<void, Ptr<Socket> >());
        }
    }

    void UdpServer::HandleRead(Ptr<Socket> socket) {
        NS_LOG_FUNCTION(this << socket);
        Ptr<Packet> packet;
        Address from;
        Address localAddress;
        while ((packet = socket->RecvFrom(from))) {
            socket->GetSockName(localAddress);
            m_rxTrace(packet);
            m_rxTraceWithAddresses(packet, from, localAddress);
            if (packet->GetSize() > 0) {
                m_totalRx += packet->GetSize();

                UdpCcHeader header;
                packet->RemoveHeader(header);
                m_delayTrace(Simulator::Now() - header.GetTs());
                m_totalDelay += (Simulator::Now() - header.GetTs());
                m_totalDelayCount++;

                uint32_t currentSequenceNumber = header.GetSeq();
                if (InetSocketAddress::IsMatchingType(from)) {
                    NS_LOG_INFO("TraceDelay: RX " << packet->GetSize() <<
                                " bytes from "<< InetSocketAddress::ConvertFrom(from).GetIpv4() <<
                                " Sequence Number: " << currentSequenceNumber <<
                                " Uid: " << packet->GetUid() <<
                                " TXtime: " << header.GetTs() <<
                                " RXtime: " << Simulator::Now() <<
                                " Delay: " << Simulator::Now() - header.GetTs());
                } else if (Inet6SocketAddress::IsMatchingType(from)) {
                    NS_LOG_INFO("TraceDelay: RX " << packet->GetSize() <<
                                " bytes from "<< Inet6SocketAddress::ConvertFrom(from).GetIpv6() <<
                                " Sequence Number: " << currentSequenceNumber <<
                                " Uid: " << packet->GetUid() <<
                                " TXtime: " << header.GetTs() <<
                                " RXtime: " << Simulator::Now() <<
                                " Delay: " << Simulator::Now() - header.GetTs());
                }

                m_lossCounter.NotifyReceived(currentSequenceNumber);
                m_received++;

                // Feedback every 5 ms
                if (Simulator::Now() - m_lastFeedback > MilliSeconds(5)) {
                    m_lastFeedback = Simulator::Now();
                    message_t msg;
                    msg.recvTime = Simulator::Now().GetInteger();
                    msg.lost = GetLost();

                    Ptr<Packet> feedbackPacket = Create<Packet>(msg.buf, 12);
                    feedbackPacket->AddHeader(header);
                    socket->SendTo(feedbackPacket, 0, from);
                }
            }
        }
    }

} // Namespace ns3
