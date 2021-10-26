/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
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
 */
#include <fstream>
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/stats-module.h"
#include <time.h>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("MyTCPExample");

class MyApp : public Application
{
public:
    MyApp ();
    virtual ~MyApp ();
    //should add number of nodes here
    void Setup (Ptr<Socket> socket, Address address, uint32_t packetSize, uint32_t nPackets, DataRate dataRate);

private:
    virtual void StartApplication (void);
    virtual void StopApplication (void);

    void ScheduleTx (void);
    void SendPacket (void);

    void ScheduleSIM (void);
    void Sim_send(void);
    void Sim_receive(void);
    void ASTRASimfn(void);

    Ptr<Socket>     m_socket;
    Address         m_peer;
    uint32_t        m_packetSize;
    uint32_t        m_nPackets;
    DataRate        m_dataRate;
    EventId         m_sendEvent;
    EventId         m_sendSimEvent;
    bool            m_running;
    uint32_t        m_packetsSent;
};

MyApp::MyApp ()
    : m_socket (0),
    m_peer (),
    m_packetSize (0),
    m_nPackets (0),
    m_dataRate (0),
    m_sendEvent (),
    m_sendSimEvent (),
    m_running (false),
    m_packetsSent (0)
{
}

MyApp::~MyApp ()
{
  m_socket = 0;
}
//why not ptr<address> ?
void
MyApp::Setup(Ptr<Socket> socket, Address address, uint32_t packetSize, uint32_t nPackets, DataRate dataRate)
{
    m_socket = socket;
    m_peer = address;
    m_packetSize = packetSize;
    m_nPackets = nPackets;
    m_dataRate = dataRate;
}

void
MyApp::StartApplication (void)
{
    m_running = true;
    m_packetsSent = 0;
    if (InetSocketAddress::IsMatchingType (m_peer))
    {
      m_socket->Bind ();
    }
    else
    {
      m_socket->Bind6 ();
    }
    m_socket->Connect (m_peer);
    SendPacket ();
    //call sim_send here if want to inform after sending multiple packets 
    //where to call sim_Schedule
}

void
MyApp::StopApplication (void)
{
    m_running = false;
    if (m_sendEvent.IsRunning ())
    {
      Simulator::Cancel (m_sendEvent);
    }
    if (m_socket)
    {
      m_socket->Close ();
    }
}

void
MyApp::SendPacket (void)
{
    Ptr<Packet> packet = Create<Packet> (m_packetSize);
    std::cout<<Simulator::Now ().GetSeconds ()<<"send single packet starts"<<"\n";
    m_socket->Send (packet);
    std::cout<<Simulator::Now ().GetSeconds ()<<"send single packet ends"<<"\n";
    if (++m_packetsSent < m_nPackets)
    {
      ScheduleTx ();
      std::cout<<Simulator::Now ().GetSeconds ()<<"scheduled of packet done"<<"\n";
      //Is this the right place to call schedule sim
      ScheduleSIM ();
      std::cout<<Simulator::Now ().GetSeconds ()<<"scheduled of astrasim fn done done"<<"\n";
      //call sim_send here if want to inform after sending single packet
      Sim_send();
      std::cout<<Simulator::Now ().GetSeconds ()<<"sim send called"<<"\n";
    }
}

void
MyApp::ScheduleTx (void)
{
    if (m_running){
        Time tNext (Seconds (m_packetSize * 8 / static_cast<double> (m_dataRate.GetBitRate ())));
        m_sendEvent = Simulator::Schedule (tNext, &MyApp::SendPacket, this);
    }
}
//see Will schedule fn - take astra sim fn inputs
void
MyApp::ScheduleSIM(void)
{
    if (m_running){
        Time delta (Seconds (static_cast<double> (2)));//how to calculate this
        m_sendSimEvent = Simulator::Schedule(delta, &MyApp::ASTRASimfn, this); //pass fn pointer here
    }
}
//see will fn sim_send
void
MyApp::Sim_send(void)
{
    //call sim send fn
    std::cout<<"hello word, Sim_send is called\n";
}

void
MyApp::Sim_receive(void)
{
    //call sim receive fn once receive of packet is done
    std::cout<<"hello word, Sim_receive is called\n";
}

void
MyApp::ASTRASimfn(void)
{
    std::cout<<"hello word, astra sim fn is called\n";
}

static void
CwndChange (Ptr<OutputStreamWrapper> stream, uint32_t oldCwnd, uint32_t newCwnd)
{
    NS_LOG_UNCOND (Simulator::Now ().GetSeconds () << "\t" << newCwnd);
    *stream->GetStream () << Simulator::Now ().GetSeconds () << "\t" << oldCwnd << "\t" << newCwnd << std::endl;
}

static void
RxDrop (Ptr<PcapFileWrapper> file, Ptr<const Packet> p)
{
    NS_LOG_UNCOND ("RxDrop at " << Simulator::Now ().GetSeconds ());
    file->Write (Simulator::Now (), p);
}

int
main (int argc, char *argv[])
{
    bool useV6 = false;
    CommandLine cmd;
    cmd.AddValue ("useIpv6", "Use Ipv6", useV6);
    cmd.Parse (argc, argv);

    //can specify it in command line
    NodeContainer nodes;
    nodes.Create (2);

    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
    pointToPoint.SetChannelAttribute ("Delay", StringValue ("2ms"));

    NetDeviceContainer devices;
    devices = pointToPoint.Install (nodes);

    // Ptr<RateErrorModel> em = CreateObject<RateErrorModel> ();
    // em->SetAttribute("ErrorRate", DoubleValue (0.00001));
    // devices.Get(1)->SetAttribute("ReceiveErrorModel",PointerValue (em));

    InternetStackHelper stack;
    stack.Install(nodes);

    uint16_t sinkPort = 8080;
    Address sinkAddress;
    Address anyAddress;
    std::string probeName;
    std::string probeTrace;

    if(useV6 == false)
    {
        Ipv4AddressHelper address;
        address.SetBase("10.1.1.0","255.255.255.0");
        Ipv4InterfaceContainer interfaces = address.Assign(devices);
        sinkAddress = InetSocketAddress(interfaces.GetAddress (1), sinkPort);
        anyAddress = InetSocketAddress(Ipv4Address::GetAny(), sinkPort); //what this any address will do 
        probeName = "ns3::Ipv4PacketProbe";
        probeTrace = "/NodeList/*/$ns3::Ipv4L3Protocol/Tx";
    }

    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory",anyAddress); //why not sink address
    ApplicationContainer sinkApps = packetSinkHelper.Install( nodes.Get(1));
    sinkApps.Start (Seconds (0.));
    sinkApps.Stop (Seconds (20.0));

    Ptr<Socket> ns3TcpSocket = Socket::CreateSocket (nodes.Get(0), TcpSocketFactory::GetTypeId());

    Ptr<MyApp> app = CreateObject<MyApp> ();
    app->Setup(ns3TcpSocket, sinkAddress, 1040, 1000,  DataRate ("1Mbps"));
    nodes.Get(0)->AddApplication(app);
    app->SetStartTime(Seconds(1.));
    app->SetStopTime(Seconds(20.));

    AsciiTraceHelper asciiTraceHelper;
    Ptr<OutputStreamWrapper> stream = asciiTraceHelper.CreateFileStream("myTCP.cwnd");
    ns3TcpSocket->TraceConnectWithoutContext("CongestionWindow",MakeBoundCallback(&CwndChange, stream));

    pointToPoint.EnablePcapAll("myTCP");
    PcapHelper pcapHelper;
    Ptr<PcapFileWrapper> file = pcapHelper.CreateFile("cwnd.pcap", std::ios::out, PcapHelper::DLT_PPP);
    devices.Get(1)->TraceConnectWithoutContext("PhyRxDrop", MakeBoundCallback(&RxDrop,file));
    
    //FileHelper fileHelper;
    //fileHelper.configureFile("myTCP-packet-byte-count", FileAggregator::FORMATED);
    //fileHelper.Set2dFormat ("Time (Seconds) = %.3e\tPacket Byte Count = %.0f");
    //fileHelper.WriteProbe (probeName,probeTrace,"OutputBytes");


    Simulator::Stop (Seconds (20));
    Simulator::Run ();
    Simulator::Destroy ();

    return 0;

}
