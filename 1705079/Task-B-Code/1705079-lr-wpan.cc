#include <fstream>
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/sixlowpan-module.h"
#include "ns3/lr-wpan-module.h"
#include "ns3/spectrum-module.h"
#include "ns3/propagation-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/traffic-control-module.h"

// Default Network Topology
//
//  lrwpan 2001:a::
//                 RO
//  *    *    *    *
//  |    |    |    |    2001:b::
//  n7   n6   n5  n4 --------------  n0  n1   n2   n3
//                   point-to-point  |    |    |    |
//                                   *    *    *    *
//
//                                   LRWPAN 2001:c::

using namespace std;
using namespace ns3;

int main(int argc, char **argv)
{

    double minTh = 20;
    double maxTh = 35;
    uint32_t pktSize = 256;
    std::string appDataRate = "5Mbps";
    std::string queueDiscType = "RED";
    uint16_t port = 5001;
    std::string bottleNeckLinkBw = "2Mbps";
    std::string bottleNeckLinkDelay = "50ms";
    uint32_t totalNode = 100;
    uint32_t wpanCount = totalNode / 2 - 1;
    uint32_t totalFlow = 20;
    int packetsPerSecond = 300;
    int multiplier = 1;

    uint32_t packetSize = 50;
    std::string p2pDataRate("2Mbps");
    std::string p2pDelay("30ms");
    uint32_t duration = 50;
    double maxRange = 150 * multiplier;

    Packet::EnablePrinting();
    CommandLine cmd(__FILE__);

    srand(time(0));
    cmd.AddValue("totalNode", "vary total nodes", totalNode);
    cmd.AddValue("totalFlow", "vary total flows", totalFlow);
    cmd.AddValue("packetsPerSecond", "vary packets per second", packetsPerSecond);
    cmd.AddValue("multiplier", "vary coverage area", multiplier);

    cmd.Parse(argc, argv);

    Config::SetDefault("ns3::TcpSocket::SegmentSize", UintegerValue(packetSize));
    Config::SetDefault("ns3::RangePropagationLossModel::MaxRange", DoubleValue(maxRange));
    Ptr<RangePropagationLossModel> propModel = CreateObject<RangePropagationLossModel>();
    Ptr<ConstantSpeedPropagationDelayModel> delayModel = CreateObject<ConstantSpeedPropagationDelayModel>();

    // default queue configuration
    Config::SetDefault("ns3::RedQueueDisc::MinTh", DoubleValue(minTh));
    Config::SetDefault("ns3::RedQueueDisc::MaxTh", DoubleValue(maxTh));
    Config::SetDefault("ns3::RedQueueDisc::LinkBandwidth", StringValue(bottleNeckLinkBw));
    Config::SetDefault("ns3::RedQueueDisc::LinkDelay", StringValue(bottleNeckLinkDelay));
    Config::SetDefault("ns3::RedQueueDisc::MeanPktSize", UintegerValue(pktSize));

    // p2p nodes creation
    NodeContainer p2pNodes;
    p2pNodes.Create(2);

    PointToPointHelper p2pHelper;
    p2pHelper.SetDeviceAttribute("DataRate", StringValue(p2pDataRate));
    p2pHelper.SetChannelAttribute("Delay", StringValue(p2pDelay));

    NetDeviceContainer p2pDevices;
    p2pDevices = p2pHelper.Install(p2pNodes);
    InternetStackHelper internetv6;

    // lrwpan left side
    NodeContainer wpanNodesLeft;
    wpanNodesLeft.Add(p2pNodes.Get(0));
    wpanNodesLeft.Create(wpanCount);

    // lrwpan left side
    NodeContainer wpanNodesRight;
    wpanNodesRight.Add(p2pNodes.Get(1));
    wpanNodesRight.Create(wpanCount);

    MobilityHelper mobilityLeft;
    mobilityLeft.SetPositionAllocator("ns3::GridPositionAllocator",
                                      "MinX", DoubleValue(10.0),
                                      "MinY", DoubleValue(10.0),
                                      "DeltaX", DoubleValue(20.0),
                                      "DeltaY", DoubleValue(20.0),
                                      "GridWidth", UintegerValue(1),
                                      "LayoutType", StringValue("RowFirst"));
    mobilityLeft.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobilityLeft.Install(wpanNodesLeft);

    // left lrwpan helper setup
    LrWpanHelper lrWpanHelperLeft;
    Ptr<SingleModelSpectrumChannel> channelLeft = CreateObject<SingleModelSpectrumChannel>();
    channelLeft->AddPropagationLossModel(propModel);
    channelLeft->SetPropagationDelayModel(delayModel);
    lrWpanHelperLeft.SetChannel(channelLeft);
    // Add and install the LrWpanNetDevice for each node
    NetDeviceContainer lrwpanDevicesLeft = lrWpanHelperLeft.Install(wpanNodesLeft);

    // Fake PAN association and short address assignment.
    // This is needed because the lr-wpan module does not provide (yet)
    // a full PAN association procedure.
    lrWpanHelperLeft.AssociateToPan(lrwpanDevicesLeft, 0);
    internetv6.Install(wpanNodesLeft);

    // six low pan
    SixLowPanHelper sixLowPanHelperLeft;
    NetDeviceContainer sixLowPanDevicesLeft = sixLowPanHelperLeft.Install(lrwpanDevicesLeft);

    MobilityHelper mobilityright;
    mobilityright.SetPositionAllocator("ns3::GridPositionAllocator",
                                       "MinX", DoubleValue(20.0),
                                       "MinY", DoubleValue(20.0),
                                       "DeltaX", DoubleValue(30.0),
                                       "DeltaY", DoubleValue(50.0),
                                       "GridWidth", UintegerValue(300),
                                       "LayoutType", StringValue("RowFirst"));
    mobilityright.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobilityright.Install(wpanNodesRight);

    // right lrwpan helper setup
    LrWpanHelper lrWpanHelperRight;
    Ptr<SingleModelSpectrumChannel> channelRight = CreateObject<SingleModelSpectrumChannel>();
    channelRight->AddPropagationLossModel(propModel);
    channelRight->SetPropagationDelayModel(delayModel);
    lrWpanHelperRight.SetChannel(channelRight);
    // Add and install the LrWpanNetDevice for each node
    NetDeviceContainer lrwpanDevicesRight = lrWpanHelperRight.Install(wpanNodesRight);

    lrWpanHelperRight.AssociateToPan(lrwpanDevicesRight, 0);
    internetv6.Install(wpanNodesRight);
    // six low pan
    SixLowPanHelper sixLowPanHelperRight;
    NetDeviceContainer sixLowPanDevicesRight = sixLowPanHelperRight.Install(lrwpanDevicesRight);

    // NS_LOG_INFO("Configure Addresses");
    Ipv6AddressHelper ipv6;
    // address for p2p router nodes
    ipv6.SetBase(Ipv6Address("2001:a::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer routerInterfaces;
    routerInterfaces = ipv6.Assign(p2pDevices);
    routerInterfaces.SetForwarding(0, true);
    routerInterfaces.SetDefaultRouteInAllNodes(0);
    routerInterfaces.SetForwarding(1, true);
    routerInterfaces.SetDefaultRouteInAllNodes(1);
    // address for wpan left nodes
    ipv6.SetBase(Ipv6Address("2001:b::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer wpanInterfacesLeft;
    wpanInterfacesLeft = ipv6.Assign(sixLowPanDevicesLeft);
    wpanInterfacesLeft.SetForwarding(0, true);
    wpanInterfacesLeft.SetDefaultRouteInAllNodes(0);
    // address for wpan right nodes
    ipv6.SetBase(Ipv6Address("2001:c::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer wpanInterfacesRight;
    wpanInterfacesRight = ipv6.Assign(sixLowPanDevicesRight);
    wpanInterfacesRight.SetForwarding(0, true);
    wpanInterfacesRight.SetDefaultRouteInAllNodes(0);

    TrafficControlHelper tchBottleneck;
    QueueDiscContainer queueDiscs;
    tchBottleneck.SetRootQueueDisc("ns3::RedQueueDisc");
    tchBottleneck.Install(lrwpanDevicesLeft.Get(0));
    queueDiscs = tchBottleneck.Install(lrwpanDevicesRight.Get(0));
    for (uint32_t i = 0; i < totalFlow; i++)
    {
        // choose pair
        int idx = rand() % wpanCount;
        // create sink
        PacketSinkHelper sink("ns3::TcpSocketFactory", Inet6SocketAddress(Ipv6Address::GetAny(), port));
        // create source
        OnOffHelper source("ns3::TcpSocketFactory", Inet6SocketAddress(wpanInterfacesLeft.GetAddress(idx, 1), port));
        source.SetAttribute("PacketSize", UintegerValue(packetSize));
        source.SetAttribute("MaxBytes", UintegerValue(0));
        source.SetConstantRate(DataRate(packetsPerSecond * packetSize * 8));

        // install sink in right station's ith node
        ApplicationContainer sinkApps = sink.Install(wpanNodesRight.Get(idx));
        Ptr<PacketSink> temp = StaticCast<PacketSink>(sinkApps.Get(0));
        sinkApps.Start(Seconds(1.0));
        sinkApps.Stop(Seconds(duration - 1));
        port++;

        // install source in left station's ith node
        ApplicationContainer sourceApp = source.Install(wpanNodesLeft.Get(idx));
        sourceApp.Start(Seconds(2.0));
        sourceApp.Stop(Seconds(duration - 1));
    }

    // Address sinkLocalAddress(Inet6SocketAddress(Ipv6Address::GetAny(), port));
    // PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", sinkLocalAddress);
    // ApplicationContainer sinkApps;
    // for (uint32_t i = 0; i < wpanCount; ++i)
    // {
    //     sinkApps.Add(packetSinkHelper.Install(wpanNodesLeft.Get(i)));
    // }
    // sinkApps.Start(Seconds(0.0));
    // sinkApps.Stop(Seconds(25.0));

    // OnOffHelper clientHelper("ns3::TcpSocketFactory", Address());
    // clientHelper.SetAttribute("OnTime", StringValue("ns3::UniformRandomVariable[Min=0.|Max=1.]"));
    // clientHelper.SetAttribute("OffTime", StringValue("ns3::UniformRandomVariable[Min=0.|Max=1.]"));
    // ApplicationContainer clientApps;
    // for (uint32_t i = 0; i < wpanCount; ++i)
    // {
    //     // on|off app installation to send packets
    //     AddressValue remoteAddress(Inet6SocketAddress(wpanInterfacesRight.GetAddress(i, 0), port));
    //     clientHelper.SetAttribute("Remote", remoteAddress);
    //     clientApps.Add(clientHelper.Install(wpanNodesRight.Get(i)));
    // }
    // clientApps.Start(Seconds(2.0)); // Start 2 second after sink
    // clientApps.Stop(Seconds(10.0)); // Stop before the sink

    Ptr<FlowMonitor> flow_monitor;
    FlowMonitorHelper flow_helper;
    flow_monitor = flow_helper.InstallAll();

    std::cout << "Running the simulation" << std::endl;
    Simulator::Stop(Seconds(25.0));
    Simulator::Run();

    // uint32_t sentPackets = 0;
    // uint32_t receivedPackets = 0;
    // uint32_t lostPackets = 0;
    // uint32_t flow_count = 0;
    // float avgThroughput = 0.0;
    // Time delay;

    // flow_monitor->CheckForLostPackets();
    // Ptr<Ipv6FlowClassifier> classifier = DynamicCast<Ipv6FlowClassifier>(flow_helper.GetClassifier());
    // std::map<FlowId, FlowMonitor::FlowStats> stats = flow_monitor->GetFlowStats();
    // for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i)
    // {
    //     Ipv6FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
    //     // std::cout << "id : " << i->first << " src : " << t.sourceAddress << " des : " << t.destinationAddress << " ";
    //     // std::cout << "data : " << (i->second.rxBytes) * 8.0 / 1024 / 1024 << "Mbps" << std::endl;
    //     NS_LOG_UNCOND("\t\tFlow ID = " << i->first);
    //     NS_LOG_UNCOND("sorce address = " << t.sourceAddress << " destination address = " << t.destinationAddress);
    //     NS_LOG_UNCOND("sent packects = " << i->second.txPackets);
    //     NS_LOG_UNCOND("received packets = " << i->second.rxPackets);
    //     NS_LOG_UNCOND("lost packets = " << i->second.txPackets - i->second.rxPackets);
    //     NS_LOG_UNCOND("packet delivery ratio = " << i->second.rxPackets * 100 / i->second.txPackets << "%");
    //     NS_LOG_UNCOND("packet loss ratio = " << (i->second.txPackets - i->second.rxPackets) * 100 / i->second.txPackets << "%");
    //     NS_LOG_UNCOND("delay = " << i->second.delaySum);
    //     NS_LOG_UNCOND("throughput = " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1024 << "Kbps");

    //     sentPackets = sentPackets + (i->second.txPackets);
    //     receivedPackets = receivedPackets + (i->second.rxPackets);
    //     lostPackets = lostPackets + (i->second.txPackets - i->second.rxPackets);
    //     avgThroughput = avgThroughput + (i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1024);
    //     delay = delay + (i->second.delaySum);

    //     flow_count++;
    // }

    // avgThroughput = avgThroughput / flow_count;
    // NS_LOG_UNCOND("\t ****Total Results of the simulation****" << std::endl);
    // NS_LOG_UNCOND("Total sent packets  =" << sentPackets);
    // NS_LOG_UNCOND("Total Received Packets =" << receivedPackets);
    // NS_LOG_UNCOND("Total Lost Packets =" << lostPackets);
    // NS_LOG_UNCOND("Packet Loss ratio =" << ((lostPackets * 100) / sentPackets) << "%");
    // NS_LOG_UNCOND("Packet delivery ratio =" << ((receivedPackets * 100) / sentPackets) << "%");
    // NS_LOG_UNCOND("Average Throughput =" << avgThroughput << "Kbps");
    // NS_LOG_UNCOND("Average End to End Delay =" << delay / flow_count);
    // NS_LOG_UNCOND("Total flow count " << flow_count);

    flow_monitor->SerializeToXmlFile("1705079-wpan-flowmon1.xml", true, true);

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

    Simulator::Destroy();
    return 0;
}