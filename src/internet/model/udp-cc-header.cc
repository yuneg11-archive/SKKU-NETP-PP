/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2009 INRIA
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
 * Author: Mathieu Lacage <mathieu.lacage@sophia.inria.fr>
 */

#include "ns3/assert.h"
#include "ns3/log.h"
#include "ns3/header.h"
#include "ns3/simulator.h"
#include "udp-cc-header.h"

namespace ns3 {

    NS_LOG_COMPONENT_DEFINE("UdpCcHeader");
    NS_OBJECT_ENSURE_REGISTERED(UdpCcHeader);

    UdpCcHeader::UdpCcHeader() : m_seq(0),
                                 m_ts(Simulator::Now().GetTimeStep()),
                                 m_interval(0) {
        NS_LOG_FUNCTION(this);
    }

    void UdpCcHeader::SetSeq(uint32_t seq) {
        NS_LOG_FUNCTION(this << seq);
        m_seq = seq;
    }

    uint32_t UdpCcHeader::GetSeq(void) const {
        NS_LOG_FUNCTION(this);
        return m_seq;
    }

    Time UdpCcHeader::GetTs(void) const {
        NS_LOG_FUNCTION(this);
        return TimeStep(m_ts);
    }

    void UdpCcHeader::SetInterval(Time interval) {
        int64_t interval_value = interval.GetInteger();
        m_interval = *((uint64_t*)&interval_value);
    }

    Time UdpCcHeader::GetInterval(void) const {
        return Time(*((int64_t*)&m_interval));
    }

    TypeId UdpCcHeader::GetTypeId(void) {
        static TypeId tid = TypeId("ns3::UdpCcHeader")
            .SetParent<Header>()
            .SetGroupName("Internet")
            .AddConstructor<UdpCcHeader>()
        ;
        return tid;
    }

    TypeId UdpCcHeader::GetInstanceTypeId(void) const {
        return GetTypeId();
    }

    void UdpCcHeader::Print(std::ostream &os) const {
        NS_LOG_FUNCTION(this << &os);
        os << "(seq=" << m_seq << " time=" << TimeStep(m_ts).GetSeconds() << ")";
    }

    uint32_t UdpCcHeader::GetSerializedSize(void) const {
        NS_LOG_FUNCTION(this);
        return 4 + 8 + 8;
    }

    void UdpCcHeader::Serialize(Buffer::Iterator start) const {
        NS_LOG_FUNCTION(this << &start);
        Buffer::Iterator i = start;
        i.WriteHtonU32(m_seq);
        i.WriteHtonU64(m_ts);
        i.WriteHtonU64(m_interval);
    }

    uint32_t UdpCcHeader::Deserialize(Buffer::Iterator start) {
        NS_LOG_FUNCTION(this << &start);
        Buffer::Iterator i = start;
        m_seq = i.ReadNtohU32();
        m_ts = i.ReadNtohU64();
        m_interval = i.ReadNtohU64();
        return GetSerializedSize();
    }

} // namespace ns3
