/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright(c) 2007,2008,2009 INRIA, UDCAST
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
#include "udp-client.h"
#include <cstdlib>
#include <cstdio>
#include <ostream>

#define SMOOTH(x, y, xr, yr) ((((x) * (xr)) + ((y) * (yr))) / ((xr) + (yr)))

// Receiver feedback message structure
typedef union {
    uint8_t buf[12];
    struct {
        int64_t recvTime;
        uint32_t lost;
    };
} message_t;

namespace ns3 {

    NS_LOG_COMPONENT_DEFINE("UdpClient");
    NS_OBJECT_ENSURE_REGISTERED(UdpClient);

    TypeId UdpClient::GetTypeId(void) {
        static TypeId tid = TypeId("ns3::UdpClient")
            .SetParent<Application>()
            .SetGroupName("Applications")
            .AddConstructor<UdpClient>()
            .AddAttribute("MaxPackets",
                          "The maximum number of packets the application will send",
                          UintegerValue(100),
                          MakeUintegerAccessor(&UdpClient::m_count),
                          MakeUintegerChecker<uint32_t>())
            .AddAttribute("RemoteAddress",
                          "The destination Address of the outbound packets",
                          AddressValue(),
                          MakeAddressAccessor(&UdpClient::m_peerAddress),
                          MakeAddressChecker())
            .AddAttribute("RemotePort", "The destination port of the outbound packets",
                          UintegerValue(100),
                          MakeUintegerAccessor(&UdpClient::m_peerPort),
                          MakeUintegerChecker<uint16_t>())
            .AddAttribute("PacketSize",
                          "Size of packets generated. The minimum packet size is 12 bytes which is the size of the header carrying the sequence number and the time stamp.",
                          UintegerValue(1024),
                          MakeUintegerAccessor(&UdpClient::m_size),
                          MakeUintegerChecker<uint32_t>(12,65507))
            .AddTraceSource("TrendlineSlope", "A trendline slope when packet has been received",
                            MakeTraceSourceAccessor(&UdpClient::m_trendlineSlope),
                            "ns3::TracedValueCallback::Double")
            .AddTraceSource("Interval", "The time to wait between packets",
                            MakeTraceSourceAccessor(&UdpClient::m_interval),
                            "ns3::TracedValueCallback::Time")
            .AddTraceSource("Lost", "The number of lost packets when packet has been received",
                            MakeTraceSourceAccessor(&UdpClient::m_lostTrace),
                            "ns3::TracedValueCallback::Uint32")
            .AddTraceSource("TargetInterval", "The number of lost packets when packet has been received",
                            MakeTraceSourceAccessor(&UdpClient::m_targetInterval),
                            "ns3::TracedValueCallback::Time")
        ;
        return tid;
    }

    UdpClient::UdpClient() {
        NS_LOG_FUNCTION(this);
        m_sent = 0;
        m_socket = 0;
        m_sendEvent = EventId();
        m_interval = MicroSeconds(500);
        m_trendlineSlope = 0;
        m_delayMin = MilliSeconds(1000);
        m_delayMax = MilliSeconds(0);
        m_targetInterval = MilliSeconds(1000);
        m_delayMinInterval = MicroSeconds(10000);
        m_delayMaxInterval = MicroSeconds(200);
        m_totalLost = 0;
        m_lostTrace = 0;
    }

    UdpClient::~UdpClient() {
        NS_LOG_FUNCTION(this);
    }

    void UdpClient::SetRemote(Address ip, uint16_t port) {
        NS_LOG_FUNCTION(this << ip << port);
        m_peerAddress = ip;
        m_peerPort = port;
    }

    void UdpClient::SetRemote(Address addr) {
        NS_LOG_FUNCTION(this << addr);
        m_peerAddress = addr;
    }

    void UdpClient::DoDispose(void) {
        NS_LOG_FUNCTION(this);
        Application::DoDispose();
    }

    void UdpClient::StartApplication(void) {
        NS_LOG_FUNCTION(this);

        if (m_socket == 0) {
            TypeId tid = TypeId::LookupByName("ns3::UdpSocketFactory");
            m_socket = Socket::CreateSocket(GetNode(), tid);
            if (Ipv4Address::IsMatchingType(m_peerAddress) == true) {
                if (m_socket->Bind() == -1) {
                    NS_FATAL_ERROR("Failed to bind socket");
                }
                m_socket->Connect(InetSocketAddress(Ipv4Address::ConvertFrom(m_peerAddress), m_peerPort));
            } else if (Ipv6Address::IsMatchingType(m_peerAddress) == true) {
                if (m_socket->Bind6() == -1) {
                    NS_FATAL_ERROR("Failed to bind socket");
                }
                m_socket->Connect(Inet6SocketAddress(Ipv6Address::ConvertFrom(m_peerAddress), m_peerPort));
            } else if (InetSocketAddress::IsMatchingType(m_peerAddress) == true) {
                if (m_socket->Bind() == -1) {
                    NS_FATAL_ERROR("Failed to bind socket");
                }
                m_socket->Connect(m_peerAddress);
            } else if (Inet6SocketAddress::IsMatchingType(m_peerAddress) == true) {
                if (m_socket->Bind6() == -1) {
                    NS_FATAL_ERROR("Failed to bind socket");
                }
                m_socket->Connect(m_peerAddress);
            } else {
                NS_ASSERT_MSG(false, "Incompatible address type: " << m_peerAddress);
            }
        }

        m_socket->SetRecvCallback(MakeCallback(&UdpClient::HandleRead, this));
        m_socket->SetAllowBroadcast(true);
        m_sendEvent = Simulator::Schedule(Seconds(0.0), &UdpClient::Send, this);
    }

    void UdpClient::StopApplication(void) {
        NS_LOG_FUNCTION(this);
        Simulator::Cancel(m_sendEvent);

        if (m_socket != 0) {
            m_socket->SetRecvCallback(MakeNullCallback<void, Ptr<Socket> >());
        }
    }

    void UdpClient::Send(void) {
        NS_LOG_FUNCTION(this);
        NS_ASSERT(m_sendEvent.IsExpired());
        UdpCcHeader header;
        header.SetSeq(m_sent);
        header.SetInterval(m_interval);
        Ptr<Packet> p = Create<Packet>(m_size - header.GetSerializedSize());
        p->AddHeader(header);

        std::stringstream peerAddressStringStream;
        if (Ipv4Address::IsMatchingType(m_peerAddress)) {
            peerAddressStringStream << Ipv4Address::ConvertFrom(m_peerAddress);
        } else if (Ipv6Address::IsMatchingType(m_peerAddress)) {
            peerAddressStringStream << Ipv6Address::ConvertFrom(m_peerAddress);
        }

        if ((m_socket->Send(p)) >= 0) {
            ++m_sent;
            NS_LOG_INFO("TraceDelay TX " << m_size <<
                        " bytes to " << peerAddressStringStream.str() <<
                        " Uid: " << p->GetUid() <<
                        " Time: " <<(Simulator::Now()).GetSeconds());
        } else {
            NS_LOG_INFO("Error while sending " << m_size <<
                        " bytes to " << peerAddressStringStream.str());
        }

        if (m_sent < m_count) {
            m_sendEvent = Simulator::Schedule(m_interval, &UdpClient::Send, this);
        }
    }

    void UdpClient::HandleRead(Ptr<Socket> socket) {
        NS_LOG_FUNCTION(this << socket);
        Ptr<Packet> packet;
        Address from;
        Address localAddress;
        while ((packet = socket->RecvFrom(from))) {
            message_t msg;
            UdpCcHeader header;
            packet->RemoveHeader(header);
            packet->CopyData(msg.buf, 12);
            ControlSend(msg.lost, header.GetTs(), Time(msg.recvTime), header.GetInterval());

            if (InetSocketAddress::IsMatchingType(from)) {
                NS_LOG_INFO("At time " << Simulator::Now().GetSeconds() << "s client received " << packet->GetSize() <<
                            " bytes from " <<InetSocketAddress::ConvertFrom(from).GetIpv4() <<
                            " port " << InetSocketAddress::ConvertFrom(from).GetPort());
            } else if (Inet6SocketAddress::IsMatchingType(from)) {
                NS_LOG_INFO("At time " << Simulator::Now().GetSeconds() << "s client received " << packet->GetSize() <<
                            " bytes from " << Inet6SocketAddress::ConvertFrom(from).GetIpv6() <<
                            " port " << Inet6SocketAddress::ConvertFrom(from).GetPort());
            }
        }
    }

    void UdpClient::UpdateInterval(Time newInterval) {
        // Change send interval when new interval is within lower & upper bound
        if (MicroSeconds(200) <= newInterval && newInterval <= MicroSeconds(10000)) {
            m_interval = SMOOTH(m_interval, newInterval, 9, 1);
        }
    }

    void UdpClient::ControlSend(uint32_t lost, Time sendTime, Time recvTime, Time sendInterval) {
        m_sendTimeList.push_back(sendTime);
        m_recvTimeList.push_back(recvTime);

        // Calculate packet loss
        m_lostTrace = lost - m_totalLost;
        m_totalLost = lost;

        // Calculate moving send interval when the packet was sent
        m_recvIntervalAvg = SMOOTH(m_recvIntervalAvg, sendInterval, 9, 1);

        if (m_sendTimeList.size() >= LIST_SIZE_LOWER_LIMIT) {
            if (m_sendTimeList.size() > LIST_SIZE_UPPER_LIMIT) {
                // Adjust(fix) calculation window
                m_sendTimeList.pop_front();
                m_recvTimeList.pop_front();
            }

            // Calculate trendline slope and current delay(smoothed)
            std::list<Time>::iterator sendIter = m_sendTimeList.begin();
            std::list<Time>::iterator recvIter = m_recvTimeList.begin();

            Time prevSendTime(*sendIter), prevRecvTime(*recvIter);
            sendIter++, recvIter++;
            Time baseSendTime(*sendIter), baseRecvTime(*recvIter);

            Time smoothedDelay(prevRecvTime - prevSendTime);
            Time accumulatedDelayDelta(0);
            Time smoothedDelayDelta(0);

            std::list<Time> x, y;
            Time xSum(0), ySum(0);

            for ( ; sendIter != m_sendTimeList.end() && recvIter != m_recvTimeList.end(); sendIter++, recvIter++) {
                smoothedDelay = SMOOTH(smoothedDelay, *recvIter - *sendIter, 9, 1);
                accumulatedDelayDelta += ((*recvIter - prevRecvTime) - (*sendIter - prevSendTime));
                smoothedDelayDelta = SMOOTH(smoothedDelayDelta, accumulatedDelayDelta, 9, 1);

                x.push_back(*recvIter - baseRecvTime);
                y.push_back(smoothedDelayDelta);

                xSum += *recvIter - baseRecvTime;
                ySum += smoothedDelayDelta;

                prevSendTime = *sendIter;
                prevRecvTime = *recvIter;
            }

            Time xAvg(xSum / x.size());
            Time yAvg(ySum / y.size());

            double numerator = 0.0;
            double denominator = 0.0;
            std::list<Time>::iterator xIter = x.begin();
            std::list<Time>::iterator yIter = y.begin();

            for (; xIter != x.end() && yIter != y.end(); xIter++, yIter++) {
                numerator += ((*xIter - xAvg) * (*yIter - yAvg)).GetDouble();
                denominator += ((*xIter - xAvg) * (*xIter - xAvg)).GetDouble();
            }
            m_trendlineSlope = numerator / denominator;

            // Calculate delay range and average delay
            if (m_delayMin >= smoothedDelay) {
                m_delayMin = smoothedDelay;
            }
            if (m_delayMax <= smoothedDelay) {
                m_delayMax = smoothedDelay;
            }
            Time delayAvg = (m_delayMax + m_delayMin) / 2;

            // Calculate interval range and target interval
            if (smoothedDelay <= m_delayMin * 100 / 97 && m_delayMinInterval > m_recvIntervalAvg) {
                m_delayMinInterval = SMOOTH(m_delayMinInterval, m_recvIntervalAvg, 95, 5);
            }
            if ((smoothedDelay >= m_delayMax * 97 / 100 && m_delayMaxInterval < m_recvIntervalAvg) || m_lostTrace.Get() == 0) {
                m_delayMaxInterval = SMOOTH(m_delayMaxInterval, m_recvIntervalAvg, 9, 1);
            }
            m_targetInterval = (m_delayMaxInterval + m_delayMinInterval) / 2;

            // Loss-based and Delay-based control
            if (m_lostTrace.Get() > 0) {
                // Lost packets -> Increase interval = Decrease throughput
                if (m_lostTrace.Get() > 10) {
                    UpdateInterval(m_interval * 100 / 70);
                } else {
                    UpdateInterval(m_interval * 100 / (100 - 3 * m_lostTrace.Get()));
                }
            } else if (smoothedDelay <= m_delayMin * 100 / 95) {
                // Too low congestion -> Decrease interval = Increase throughput
                UpdateInterval(m_interval * 95 / 100);
            } else if (smoothedDelay > m_delayMax * 95 / 100) {
                // Too high congestion -> Increase interval = Decrease throughput
                UpdateInterval(m_interval * 100 / 85);
            } else {
                if (smoothedDelay > delayAvg * 100 / 80) {
                    // Above target delay
                    // Delay increases -> Increase interval = Decrease throughput
                    if (m_trendlineSlope > 0.05) {
                        UpdateInterval(m_interval * 100 / 95);
                    } else if (m_trendlineSlope >= -0.01) {
                        UpdateInterval(m_interval * 100 / 97);
                    }
                    // Delay decreases -> Decrease interval = Increase throughput
                    if (m_trendlineSlope < -0.10) {
                        UpdateInterval(m_interval * 96 / 100);
                    } else if (m_trendlineSlope < -0.05) {
                        UpdateInterval(m_interval * 98 / 100);
                    }
                } else if (smoothedDelay < delayAvg * 80 / 100) {
                    // Below target delay
                    // Delay increases -> Increase interval = Decrease throughput
                    if (m_trendlineSlope > 0.10) {
                        UpdateInterval(m_interval * 100 / 95);
                    } else if (m_trendlineSlope > 0.05) {
                        UpdateInterval(m_interval * 100 / 97);
                    }
                    // Delay decreases -> Decrease interval = Increase throughput
                    if (m_trendlineSlope < -0.05) {
                        UpdateInterval(m_interval * 96 / 100);
                    } else if (m_trendlineSlope <= 0.01) {
                        UpdateInterval(m_interval * 98 / 100);
                    }
                } else {
                    // Within target delay -> Hold interval = Hold throughput
                    UpdateInterval(SMOOTH(m_interval, ((m_delayMaxInterval + m_delayMinInterval) / 2) * 100 / 97, 5, 5));
                }
            }
        } else {
            // Bootstrap stage -> Decrease interval = Increase throughput
            UpdateInterval(m_interval * 75 / 100);
        }
    }

} // Namespace ns3
