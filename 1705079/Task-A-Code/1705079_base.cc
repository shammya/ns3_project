
// author :  kazi wasif amin shammo 1705079
// Experimented Network Topology
//
//                          10.3.1.0
//        n3  n2  n1  n0   ============   n4   n5   n6   n7
//        |   |   |   |   point-to-point  |    |    |    |
//       ==============                   ================
//       LAN 10.1.1.0                       LAN 10.2.1.0

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/point-to-point-layout-module.h"
#include "ns3/traffic-control-module.h"
#include "ns3/flow-monitor-module.h"

#include <iostream>
#include <iomanip>
#include <map>

using namespace ns3;



int main(int argc, char *argv[])
{
    // default values of the parameters
    uint32_t nLeftLeafCount = 20;
    uint32_t nRightLeafCount = 20;
    uint32_t maxPackets = 500;
    bool modeBytes = false;
    uint32_t queueDiscLimitPackets = 100;
    double minTh = 50;
    double maxTh = 75;
    uint32_t pktSize = 256;
    std::string appDataRate = "10Mbps";
    std::string queueDiscType = "RED";
    uint16_t port = 5001;
    std::string bottleNeckLinkBw = "5Mbps";
    std::string bottleNeckLinkDelay = "100ms";

    // options for arguments
    CommandLine cmd(__FILE__);
    cmd.AddValue("nLeftLeafCount", "Number of left side leaf nodes", nLeftLeafCount);
    cmd.AddValue("nRightLeafCount", "Number of right side leaf nodes", nRightLeafCount);
    cmd.AddValue("maxPackets", "Max Packets allowed in the device queue", maxPackets);
    cmd.AddValue("queueDiscLimitPackets", "Max Packets allowed in the queue disc",
                 queueDiscLimitPackets);
    cmd.AddValue("queueDiscType", "Set Queue disc type to RED", queueDiscType);
    cmd.AddValue("appPktSize", "Set OnOff App Packet Size", pktSize);
    cmd.AddValue("appDataRate", "Set OnOff App DataRate", appDataRate);
    cmd.AddValue("modeBytes", "Set Queue disc mode to Packets (false) or bytes (true)", modeBytes);

    cmd.AddValue("redMinTh", "RED queue minimum threshold (in packets)", minTh);
    cmd.AddValue("redMaxTh", "RED queue maximum threshold (in packets)", maxTh);
    cmd.Parse(argc, argv);

    // default configuration
    Config::SetDefault("ns3::OnOffApplication::PacketSize", UintegerValue(pktSize));
    Config::SetDefault("ns3::OnOffApplication::DataRate", StringValue(appDataRate));
    Config::SetDefault("ns3::DropTailQueue<Packet>::MaxSize",
                       StringValue(std::to_string(maxPackets) + "p"));

    if (!modeBytes)
    {
        Config::SetDefault(
            "ns3::RedQueueDisc::MaxSize",
            QueueSizeValue(QueueSize(QueueSizeUnit::PACKETS, queueDiscLimitPackets)));
    }
    else
    {
        Config::SetDefault(
            "ns3::RedQueueDisc::MaxSize",
            QueueSizeValue(QueueSize(QueueSizeUnit::BYTES, queueDiscLimitPackets * pktSize)));
        minTh *= pktSize;
        maxTh *= pktSize;
    }

    // default configuration for queue implementation
    Config::SetDefault("ns3::RedQueueDisc::MinTh", DoubleValue(minTh));
    Config::SetDefault("ns3::RedQueueDisc::MaxTh", DoubleValue(maxTh));
    Config::SetDefault("ns3::RedQueueDisc::LinkBandwidth", StringValue(bottleNeckLinkBw));
    Config::SetDefault("ns3::RedQueueDisc::LinkDelay", StringValue(bottleNeckLinkDelay));
    Config::SetDefault("ns3::RedQueueDisc::MeanPktSize", UintegerValue(pktSize));

    //  point-to-point link helpers
    PointToPointHelper bottleNeckLink;
    bottleNeckLink.SetDeviceAttribute("DataRate", StringValue(bottleNeckLinkBw));
    bottleNeckLink.SetChannelAttribute("Delay", StringValue(bottleNeckLinkDelay));

    PointToPointHelper pointToPointLeaf;
    pointToPointLeaf.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPointLeaf.SetChannelAttribute("Delay", StringValue("1ms"));

    PointToPointDumbbellHelper d(nLeftLeafCount, pointToPointLeaf,
                                 nRightLeafCount, pointToPointLeaf,
                                 bottleNeckLink);

    // protocol stack installation
    InternetStackHelper stack;
    for (uint32_t i = 0; i < d.LeftCount(); ++i)
    {
        stack.Install(d.GetLeft(i));
    }
    for (uint32_t i = 0; i < d.RightCount(); ++i)
    {
        stack.Install(d.GetRight(i));
    }

    stack.Install(d.GetLeft());
    stack.Install(d.GetRight());

    TrafficControlHelper tchBottleneck;
    QueueDiscContainer queueDiscs;
    tchBottleneck.SetRootQueueDisc("ns3::RedQueueDisc");
    tchBottleneck.Install(d.GetLeft()->GetDevice(0));
    queueDiscs = tchBottleneck.Install(d.GetRight()->GetDevice(0));

    // ip address assignment
    d.AssignIpv4Addresses(Ipv4AddressHelper("10.1.1.0", "255.255.255.0"),
                          Ipv4AddressHelper("10.2.1.0", "255.255.255.0"),
                          Ipv4AddressHelper("10.3.1.0", "255.255.255.0"));

    // on|off application installation

    Address sinkLocalAddress(InetSocketAddress(Ipv4Address::GetAny(), port));
    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", sinkLocalAddress);
    ApplicationContainer sinkApps;
    for (uint32_t i = 0; i < d.LeftCount(); ++i)
    {
        sinkApps.Add(packetSinkHelper.Install(d.GetLeft(i)));
    }
    sinkApps.Start(Seconds(0.0));
    sinkApps.Stop(Seconds(25.0));

    OnOffHelper clientHelper("ns3::TcpSocketFactory", Address());
    clientHelper.SetAttribute("OnTime", StringValue("ns3::UniformRandomVariable[Min=0.|Max=1.]"));
    clientHelper.SetAttribute("OffTime", StringValue("ns3::UniformRandomVariable[Min=0.|Max=1.]"));
    ApplicationContainer clientApps;
    for (uint32_t i = 0; i < d.RightCount(); ++i)
    {
        // on|off app installation to send packets
        AddressValue remoteAddress(InetSocketAddress(d.GetLeftIpv4Address(i), port));
        clientHelper.SetAttribute("Remote", remoteAddress);
        clientApps.Add(clientHelper.Install(d.GetRight(i)));
    }
    clientApps.Start(Seconds(2.0)); // Start 2 second after sink
    clientApps.Stop(Seconds(10.0)); // Stop before the sink

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // performance metrics calculation

    AsciiTraceHelper asciiHelper;
    pointToPointLeaf.EnableAsciiAll(asciiHelper.CreateFileStream("p2p_ascii.tr"));
    bottleNeckLink.EnableAsciiAll(asciiHelper.CreateFileStream("bottle_neck_ascii.tr"));

    Ptr<FlowMonitor> flow_monitor;
    FlowMonitorHelper flow_helper;
    flow_monitor = flow_helper.InstallAll();

    std::cout << "Running the simulation" << std::endl;
    Simulator::Stop(Seconds(25.0));
    Simulator::Run();

    uint32_t sentPackets = 0;
    uint32_t receivedPackets = 0;
    uint32_t lostPackets = 0;
    uint32_t flow_count = 0;
    float avgThroughput = 0.0;
    Time delay;

    flow_monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flow_helper.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = flow_monitor->GetFlowStats();
    for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i)
    {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
        // std::cout << "id : " << i->first << " src : " << t.sourceAddress << " des : " << t.destinationAddress << " ";
        // std::cout << "data : " << (i->second.rxBytes) * 8.0 / 1024 / 1024 << "Mbps" << std::endl;
        NS_LOG_UNCOND("\t\tFlow ID = " << i->first);
        NS_LOG_UNCOND("sorce address = " << t.sourceAddress << " destination address = " << t.destinationAddress);
        NS_LOG_UNCOND("sent packects = " << i->second.txPackets);
        NS_LOG_UNCOND("received packets = " << i->second.rxPackets);
        NS_LOG_UNCOND("lost packets = " << i->second.txPackets - i->second.rxPackets);
        NS_LOG_UNCOND("packet delivery ratio = " << i->second.rxPackets * 100 / i->second.txPackets << "%");
        NS_LOG_UNCOND("packet loss ratio = " << (i->second.txPackets - i->second.rxPackets) * 100 / i->second.txPackets << "%");
        NS_LOG_UNCOND("delay = " << i->second.delaySum);
        NS_LOG_UNCOND("throughput = " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1024 << "Kbps");

        sentPackets = sentPackets + (i->second.txPackets);
        receivedPackets = receivedPackets + (i->second.rxPackets);
        lostPackets = lostPackets + (i->second.txPackets - i->second.rxPackets);
        avgThroughput = avgThroughput + (i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1024);
        delay = delay + (i->second.delaySum);

        flow_count++;
    }

    avgThroughput = avgThroughput / flow_count;
    NS_LOG_UNCOND("\t ****Total Results of the simulation****" << std::endl);
    NS_LOG_UNCOND("Total sent packets  =" << sentPackets);
    NS_LOG_UNCOND("Total Received Packets =" << receivedPackets);
    NS_LOG_UNCOND("Total Lost Packets =" << lostPackets);
    NS_LOG_UNCOND("Packet Loss ratio =" << ((lostPackets * 100) / sentPackets) << "%");
    NS_LOG_UNCOND("Packet delivery ratio =" << ((receivedPackets * 100) / sentPackets) << "%");
    NS_LOG_UNCOND("Average Throughput =" << avgThroughput << "Kbps");
    NS_LOG_UNCOND("Average End to End Delay =" << delay / flow_count);
    NS_LOG_UNCOND("Total flow count " << flow_count);

    flow_monitor->SerializeToXmlFile("flow_monitor_stats.xml", true, true);

    QueueDisc::Stats st = queueDiscs.Get(0)->GetStats();

    if (st.GetNDroppedPackets(RedQueueDisc::UNFORCED_DROP) == 0)
    {
        std::cout << "There should be some unforced drops" << std::endl;
        exit(1);
    }

    if (st.GetNDroppedPackets(QueueDisc::INTERNAL_QUEUE_DROP) != 0)
    {
        std::cout << "There should be zero drops due to queue full" << std::endl;
        exit(1);
    }

    std::cout << std::endl
              << "$$$ status showing bottleneck queue disc $$$" << std::endl;
    std::cout << st << std::endl;
    std::cout << "Destroying the simulation" << std::endl;

    Simulator::Destroy();
    return 0;
}
