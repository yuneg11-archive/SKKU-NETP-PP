/*
 * Copyright (c) 2013 ResiliNets, ITTC, University of Kansas
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
 * Authors: Justin P. Rohrer, Truc Anh N. Nguyen <annguyen@ittc.ku.edu>, Siddharth Gangadhar <siddharth@ittc.ku.edu>
 *
 * James P.G. Sterbenz <jpgs@ittc.ku.edu>, director
 * ResiliNets Research Group  http:// wiki.ittc.ku.edu/resilinets
 * Information and Telecommunication Technology Center (ITTC)
 * and Department of Electrical Engineering and Computer Science
 * The University of Kansas Lawrence, KS USA.
 *
 * Work supported in part by NSF FIND (Future Internet Design) Program
 * under grant CNS-0626918 (Postmodern Internet Architecture),
 * NSF grant CNS-1050226 (Multilayer Network Resilience Analysis and Experimentation on GENI),
 * US Department of Defense (DoD), and ITTC at The University of Kansas.
 *
 * “TCP Westwood(+) Protocol Implementation in ns-3”
 * Siddharth Gangadhar, Trúc Anh Ngọc Nguyễn , Greeshma Umapathi, and James P.G. Sterbenz,
 * ICST SIMUTools Workshop on ns-3 (WNS3), Cannes, France, March 2013
 */

#include <iostream>
#include <fstream>
#include <string>
#include <list>

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/error-model.h"
#include "ns3/enum.h"
#include "ns3/event-id.h"
#include "ns3/flow-monitor-helper.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/traffic-control-module.h"

#define LOG_INTERVAL 100

using namespace ns3;
using namespace std;

NS_LOG_COMPONENT_DEFINE("FinalProject");

void LogTcpThroughput(const int flowNum, Ptr<PacketSink> sink, const uint64_t lastTotalRx) {
    uint64_t currentTotalRx = sink->GetTotalRx();
    uint64_t throughput = (currentTotalRx - lastTotalRx) * 8 * (1000 / LOG_INTERVAL) / 1000;
    NS_LOG_UNCOND(Simulator::Now().GetSeconds() << " s > thr " << flowNum << "(tcp) " << throughput << " Kbps");
    Simulator::Schedule(MilliSeconds(LOG_INTERVAL), &LogTcpThroughput, flowNum, sink, currentTotalRx);
}

void LogUdpThroughput(const int flowNum, Ptr<UdpServer> server, const uint64_t lastTotalRx) {
    uint64_t currentTotalRx = server->GetTotalRx();
    uint64_t throughput = (currentTotalRx - lastTotalRx) * 8 * (1000 / LOG_INTERVAL) / 1000;
    NS_LOG_UNCOND(Simulator::Now().GetSeconds() << " s > thr " << flowNum << "(udp) " << throughput << " Kbps");
    Simulator::Schedule(MilliSeconds(LOG_INTERVAL), &LogUdpThroughput, flowNum, server, currentTotalRx);
}

void LogUdpDelay(string flowNum, Time delay) {
    NS_LOG_UNCOND(Simulator::Now().GetSeconds() << " s > delay " << flowNum << "(udp) " << delay.GetMilliSeconds() << " ms");
}

void LogUdpTrendline(string flowNum, double oldValue, double newValue) {
    NS_LOG_UNCOND(Simulator::Now().GetSeconds() << " s > trendline " << flowNum << "(udp) " << newValue);
}

void LogUdpInterval(string flowNum, Time oldValue, Time newValue) {
    NS_LOG_UNCOND(Simulator::Now().GetSeconds() << " s > interval " << flowNum << "(udp) " << newValue.GetMicroSeconds());
}

void LogUdpLost(string flowNum, uint32_t oldValue, uint32_t newValue) {
    NS_LOG_UNCOND(Simulator::Now().GetSeconds() << " s > lost " << flowNum << "(udp) " << newValue);
}

void LogUdpTargetInterval(string flowNum, Time oldValue, Time newValue) {
    NS_LOG_UNCOND(Simulator::Now().GetSeconds() << " s > target " << flowNum << "(udp) " << newValue.GetMicroSeconds());
}

int main(int argc, char *argv[]) {
    /* NOTICE
    * You should use following logs for only debugging. Please disable all logs when submit!
    * For printing out throughput and propagation delay, you should use tracing system, not log components.
    */
    // LogComponentEnable("UdpServer", (LogLevel)(LOG_LEVEL_ALL|LOG_PREFIX_NODE|LOG_PREFIX_TIME));
    // LogComponentEnable("UdpClient", (LogLevel)(LOG_LEVEL_ALL|LOG_PREFIX_NODE|LOG_PREFIX_TIME));
    // LogComponentEnable("BulkSendApplication", (LogLevel)(LOG_LEVEL_ALL|LOG_PREFIX_NODE|LOG_PREFIX_TIME));
    // LogComponentEnable("PacketSink", (LogLevel)(LOG_LEVEL_ALL|LOG_PREFIX_NODE|LOG_PREFIX_TIME));

    string topologyFilename, flowFilename;
    uint32_t simulationTime = 100;
    CommandLine cmd;
    cmd.AddValue("topo_file", "The name of topology configuration file", topologyFilename);
    cmd.AddValue("flow_file", "The name of flow configuration file", flowFilename);
    cmd.AddValue("sim_time", "Simulation Time", simulationTime);
    cmd.Parse(argc, argv);

    // TCP Configuration --> Do not modify
    Config::SetDefault("ns3::TcpSocket::RcvBufSize", UintegerValue(1 << 21));
    Config::SetDefault("ns3::TcpSocket::SndBufSize", UintegerValue(1 << 21));
    Config::SetDefault("ns3::TcpSocket::SegmentSize", UintegerValue(1000));
    Config::SetDefault("ns3::TcpL4Protocol::SocketType", TypeIdValue(TypeId::LookupByName("ns3::TcpNewReno")));

    // Set Topology
    std::ifstream topologyFile;
    uint32_t nodeNum, switchNum, linkNum;
    topologyFile.open(topologyFilename.c_str());
    topologyFile >> nodeNum >> switchNum >> linkNum;

    NodeContainer nodes;
    nodes.Create(nodeNum);

    // To distinguish switch nodes from host nodes
    std::vector<uint32_t> nodeType(nodeNum, 0);
    for (uint32_t i = 0; i < switchNum; i++) {
        uint32_t sid;
        topologyFile >> sid;
        nodeType[sid] = 1;
    }

    InternetStackHelper internet;
    internet.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.0.0.0", "255.255.255.0");

    // Assume that each host nodes (server/client nodes) connects to a single switch in this project
    std::vector<Ipv4Address> serverAddresses(nodeNum, Ipv4Address());

    for (uint32_t i = 0; i < linkNum; i++) {
        uint32_t src, dst;
        string bandwidth, linkDelay;
        topologyFile >> src >> dst >> bandwidth >> linkDelay;

        DataRate rate(bandwidth);
        Time delay(linkDelay);

        PointToPointHelper p2p;
        p2p.SetDeviceAttribute("DataRate", StringValue(bandwidth));
        p2p.SetChannelAttribute("Delay", StringValue(linkDelay));
        p2p.SetQueue("ns3::DropTailQueue", "MaxSize", QueueSizeValue(QueueSize("50p")));

        // p2p.EnablePcapAll("Test");

        NetDeviceContainer devices = p2p.Install(nodes.Get(src), nodes.Get(dst));

        /* Install Traffic Controller
        * Do not disable traffic controller.
        * Packets are enqueued in a new efficient queue instead of Netdevice's droptail queue.
        * But if you want to use different Queue Disc like CoDel and RED, you can use it!
        * Max Size should be the same as 50p
        * You should write the reason why use other queue disc than PfifoFastQueueDisc in report and presentation ppt material.
        */
        TrafficControlHelper tch;
        tch.SetRootQueueDisc("ns3::PfifoFastQueueDisc", "MaxSize", QueueSizeValue(QueueSize("50p"))); // ns3 default queue disc
        // tch.SetRootQueueDisc("ns3::CoDelQueueDisc", "MaxSize", QueueSizeValue(QueueSize("50p")); // CoDel queue disc
        // tch.SetRootQueueDisc("ns3::RedQueueDisc", "MaxSize", QueueSizeValue(QueueSize("50p")); // RED queue disc
        // tch.SetRootQueueDisc("ns3::...")
        tch.Install(devices);

        Ipv4InterfaceContainer ipv4 = address.Assign(devices);
        address.NewNetwork();

        // Store addresses of servers for installing applications later.
        if (nodeType[src] == 0 || nodeType[dst] == 0) {
            uint32_t hostIndex = nodeType[src] ? 1 : 0;
            uint32_t hostId = nodeType[src] ? dst : src;
            serverAddresses[hostId] = ipv4.GetAddress(hostIndex);
        }
    }

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Set Flows
    std::ifstream flowFile;
    uint32_t flowNum;
    flowFile.open(flowFilename.c_str());
    flowFile >> flowNum;

    list< Ptr<Application> > apps;
    list<Time> appsStart;
    list<bool> appsType;

    for (uint32_t i = 0; i < flowNum; i++) {
        string protocol;
        uint32_t src, dst, port, maxPacketCount;
        double startTime;

        flowFile >> protocol >> src >> dst >> port >> maxPacketCount >> startTime;

        // Do not modify parameters of TCP
        if (protocol == "TCP") {
            Address sinkLocalAddress(InetSocketAddress(Ipv4Address::GetAny(), port));
            PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", sinkLocalAddress);
            sinkHelper.SetAttribute("Protocol", TypeIdValue(TcpSocketFactory::GetTypeId()));
            ApplicationContainer sinkApp = sinkHelper.Install(nodes.Get(dst));
            sinkApp.Start(Seconds(startTime));

            AddressValue remoteAddress(InetSocketAddress(serverAddresses[dst], port));
            BulkSendHelper ftp("ns3::TcpSocketFactory", Address());
            ftp.SetAttribute("Remote", remoteAddress);
            ftp.SetAttribute("SendSize", UintegerValue(1000));
            ftp.SetAttribute("MaxBytes", UintegerValue(maxPacketCount * 1000));
            ApplicationContainer sourceApp = ftp.Install(nodes.Get(src));
            sourceApp.Start(Seconds(startTime));

            // Set up Tcp Troughput Trace
            // Simulator::Schedule(Seconds(startTime) + MilliSeconds(LOG_INTERVAL), &LogTcpThroughput, i, StaticCast<PacketSink>(sinkApp.Get(0)), 0);

            apps.push_back(sinkApp.Get(0));
            appsStart.push_back(Seconds(startTime));
            appsType.push_back(false);
        } else {
            // You can add/remove/change parameters of UDP
            UdpServerHelper server(port);
            ApplicationContainer serverApp = server.Install(nodes.Get(dst));
            serverApp.Start(Seconds(startTime));

            UdpClientHelper client(serverAddresses[dst], port);
            client.SetAttribute("MaxPackets", UintegerValue(maxPacketCount));
            // client.SetAttribute("Interval", TimeValue(MilliSeconds(1))); // Managed by application-level congestion controller
            client.SetAttribute("PacketSize", UintegerValue(1000)); // Do not modify
            ApplicationContainer clientApp = client.Install(nodes.Get(src));
            clientApp.Start(Seconds(startTime));

            // Set up Udp Troughput Trace
            // Simulator::Schedule(Seconds(startTime) + MilliSeconds(LOG_INTERVAL), &LogUdpThroughput, i, StaticCast<UdpServer>(serverApp.Get(0)), 0);

            // Set up Udp Delay Trace
            // serverApp.Get(0)->TraceConnect("Delay", to_string(i), MakeCallback(&LogUdpDelay));

            // Set up Udp Trendline Slope Trace
            // clientApp.Get(0)->TraceConnect("TrendlineSlope", to_string(i), MakeCallback(&LogUdpTrendline));
            // clientApp.Get(0)->TraceConnect("Interval", to_string(i), MakeCallback(&LogUdpInterval));
            // clientApp.Get(0)->TraceConnect("Lost", to_string(i), MakeCallback(&LogUdpLost));
            // clientApp.Get(0)->TraceConnect("TargetInterval", to_string(i), MakeCallback(&LogUdpTargetInterval));

            apps.push_back(serverApp.Get(0));
            appsStart.push_back(Seconds(startTime));
            appsType.push_back(true);
        }
    }

    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();

    list< Ptr<Application> >::iterator appsIter = apps.begin();
    list<Time>::iterator appsStartIter = appsStart.begin();
    list<bool>::iterator appsTypeIter = appsType.begin();
    int cnt = 0;

    // Print throughput and delay
    for ( ; appsIter != apps.end() && appsTypeIter != appsType.end() && appsStartIter != appsStart.end();
            appsIter++, appsStartIter++, appsTypeIter++, cnt++) {
        if (*appsTypeIter) {
            // UDP
            uint64_t totalRx = (StaticCast<UdpServer>(*appsIter))->GetTotalRx();
            Time duration = Seconds(simulationTime) - (*appsStartIter);
            NS_LOG_UNCOND("(UDP)" << cnt << ": Throughput " << (totalRx * 8) / (duration.GetSeconds() * 1000) << " Kbps");
            NS_LOG_UNCOND("(UDP)" << cnt << ": Delay      " << (StaticCast<UdpServer>(*appsIter))->GetDelayAvg().GetMilliSeconds() << " ms");
        } else {
            // TCP
            uint64_t totalRx = (StaticCast<PacketSink>(*appsIter))->GetTotalRx();
            Time duration = Seconds(simulationTime) - (*appsStartIter);
            NS_LOG_UNCOND("(TCP)" << cnt << ": Throughput " << (totalRx * 8) / (duration.GetSeconds() * 1000) << " Kbps");
        }
    }

    Simulator::Destroy();
    return 0;
}
