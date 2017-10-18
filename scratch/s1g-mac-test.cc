/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2009 MIRKO BANCHI
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
*/


#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/internet-module.h"
#include <iostream>
#include <fstream>
#include <stdio.h>
#include <stdlib.h>
#include <ctime>
#include <fstream>
#include <sys/stat.h>
#include "ns3/rps.h"
#include <utility> // std::pair
#include <map>
#include <sstream>

using namespace std;
using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("s1g-wifi-network-le");

uint32_t AssocNum = 0;
int64_t AssocTime=0;
uint32_t StaNum=0;
string traffic_filepath;
uint32_t payloadLength;
NetDeviceContainer staDeviceCont;
const int MaxSta = 2048;

class assoc_record
{
public:
    assoc_record ();
    bool GetAssoc ();
    void SetAssoc (std::string context, Mac48Address address);
    void UnsetAssoc (std::string context, Mac48Address address);
    void setstaid (uint16_t id);
 private:
    bool assoc;
    uint16_t staid;
};

assoc_record::assoc_record ()
{
    assoc = false;
    staid = 65535;
}

void
assoc_record::setstaid (uint16_t id)
{
    staid = id;
}

void
assoc_record::SetAssoc (std::string context, Mac48Address address)
{
   assoc = true;
}

void
assoc_record::UnsetAssoc (std::string context, Mac48Address address)
{
  assoc = false;
}

bool
assoc_record::GetAssoc ()
{
    return assoc;
}

typedef std::vector <assoc_record * > assoc_recordVector;
assoc_recordVector assoc_vector;


uint32_t
GetAssocNum ( )
{
  AssocNum = 0;
  for (assoc_recordVector::const_iterator index = assoc_vector.begin (); index != assoc_vector.end (); index++)
    {
      if ((*index)->GetAssoc ())
        {
            AssocNum++;
        }
    }
  return AssocNum;
}


void
UdpTraffic ( uint16_t num, uint32_t Size, string path, NetDeviceContainer staDevice)
{
  StaNum = num;
  payloadLength = Size;
  traffic_filepath = path;
  staDeviceCont = staDevice;
}

ApplicationContainer serverApp;
uint32_t AppStartTime = 0;
uint32_t ApStopTime = 0;

std::map<uint16_t, float> traffic_sta;


void CheckAssoc (uint32_t Nsta, double simulationTime, NodeContainer wifiApNode, NodeContainer  wifiStaNode, Ipv4InterfaceContainer apNodeInterface)
{


    if  (GetAssocNum () == Nsta)
      {
        std::cout << "Assoc Finished, AssocNum=" << AssocNum << ", time = " << Simulator::Now ().GetMicroSeconds () << std::endl;
        //std::cout << "00:00:00:00:00:09,Assoc Finished," << Simulator::Now ().GetSeconds () << std::endl;
        //Application start time
        Ptr<UniformRandomVariable> m_rv = CreateObject<UniformRandomVariable> ();
        //UDP flow
        UdpServerHelper myServer (9);
        //ApplicationContainer serverApp;
        serverApp = myServer.Install (wifiApNode);
        serverApp.Start (Seconds (0));

        UdpClientHelper myClient (apNodeInterface.GetAddress (0), 9); //address of remote node
        myClient.SetAttribute ("MaxPackets", UintegerValue (4294967295u));
        myClient.SetAttribute ("PacketSize", UintegerValue (payloadLength));

          traffic_sta.clear ();
          
          ifstream trafficfile (traffic_filepath.c_str());
                  
           if (trafficfile.is_open())
                 {
                  uint16_t sta_id;
                  float  sta_traffic;
                  for (uint16_t kk=0; kk< Nsta; kk++)
                    {
                      trafficfile >> sta_id;
                      trafficfile >> sta_traffic;
                      traffic_sta.insert (std::make_pair(sta_id,sta_traffic)); //insert data
                      //cout << "sta_id = " << sta_id << " sta_traffic = " << sta_traffic << "\n";
                    }
                  trafficfile.close();
                }
           else cout << "Unable to open file \n";
                     
         
          double randomStart = 0.0;
          
          for (std::map<uint16_t,float>::iterator it=traffic_sta.begin(); it!=traffic_sta.end(); ++it)
              {
                  std::ostringstream intervalstr;
                  intervalstr << (payloadLength*8)/(it->second * 1000000);
                  std::string intervalsta = intervalstr.str();

                  myClient.SetAttribute ("Interval", TimeValue (Time (intervalsta))); //packets/s
                  randomStart = m_rv->GetValue (0, (payloadLength*8)/(it->second * 1000000));
                  ApplicationContainer clientApp = myClient.Install (wifiStaNode.Get(it->first));

                  clientApp.Start (Seconds (1 + randomStart));
                  clientApp.Stop (Seconds (simulationTime+1)); //
               }
          
              AppStartTime=Simulator::Now ().GetSeconds () + 1;
              Simulator::Stop (Seconds (simulationTime+1));
      }
    else
      {
        Simulator::Schedule(Seconds(1.0), &CheckAssoc, Nsta, simulationTime, wifiApNode, wifiStaNode, apNodeInterface);
      }

}



void
PopulateArpCache ()
{
    Ptr<ArpCache> arp = CreateObject<ArpCache> ();
    arp->SetAliveTimeout (Seconds(3600 * 24 * 365));
    for (NodeList::Iterator i = NodeList::Begin(); i != NodeList::End(); ++i)
    {
        Ptr<Ipv4L3Protocol> ip = (*i)->GetObject<Ipv4L3Protocol> ();
        NS_ASSERT(ip !=0);
        ObjectVectorValue interfaces;
        ip->GetAttribute("InterfaceList", interfaces);
        for(ObjectVectorValue::Iterator j = interfaces.Begin(); j !=
            interfaces.End (); j ++)
        {
            Ptr<Ipv4Interface> ipIface = (j->second)->GetObject<Ipv4Interface> ();
            NS_ASSERT(ipIface != 0);
            Ptr<NetDevice> device = ipIface->GetDevice();
            NS_ASSERT(device != 0);
            Mac48Address addr = Mac48Address::ConvertFrom(device->GetAddress ());
            for(uint32_t k = 0; k < ipIface->GetNAddresses (); k ++)
            {
                Ipv4Address ipAddr = ipIface->GetAddress (k).GetLocal();
                if(ipAddr == Ipv4Address::GetLoopback())
                    continue;
                ArpCache::Entry * entry = arp->Add(ipAddr);
                entry->MarkWaitReply(0);
                entry->MarkAlive(addr);
                //std::cout << "Arp Cache: Adding the pair (" << addr << "," << ipAddr << ")" << std::endl;
            }
        }
    }
    for (NodeList::Iterator i = NodeList::Begin(); i != NodeList::End(); ++i)
    {
        Ptr<Ipv4L3Protocol> ip = (*i)->GetObject<Ipv4L3Protocol> ();
        NS_ASSERT(ip !=0);
        ObjectVectorValue interfaces;
        ip->GetAttribute("InterfaceList", interfaces);
        for(ObjectVectorValue::Iterator j = interfaces.Begin(); j !=interfaces.End (); j ++)
        {
            Ptr<Ipv4Interface> ipIface = (j->second)->GetObject<Ipv4Interface> ();
            ipIface->SetAttribute("ArpCache", PointerValue(arp));
        }
    }
}

uint16_t ngroup;
uint16_t nslot; 
/*RPSVector configureRAW (RPSVector rpslist, string RAWConfigFile)
{
    uint16_t NRPS = 0;
    uint16_t NRAWPERBEACON = 0;
    uint16_t Value = 0;
    uint32_t page = 0;
    uint32_t aid_start = 0;
    uint32_t aid_end = 0;
    uint32_t rawinfo = 0;
    
    ifstream myfile (RAWConfigFile);
    //1. get info from config file
    
    //2. define RPS
    if (myfile.is_open())
    {
        myfile >> NRPS;
        
        for (uint16_t kk=0; kk< NRPS; kk++)   // number of beacons covering all raw groups
        {
            RPS *m_rps = new RPS;
            myfile >> NRAWPERBEACON;
            ngroup = NRAWPERBEACON;
            for (uint16_t i=0; i< NRAWPERBEACON; i++)  // raw groups in one beacon		
            {
                //RPS *m_rps = new RPS;
                RPS::RawAssignment *m_raw = new RPS::RawAssignment;

                myfile >> Value;
                m_raw->SetRawControl (Value);//support paged STA or not
                myfile >> Value;
                m_raw->SetSlotCrossBoundary (Value);
                myfile >> Value;
                m_raw->SetSlotFormat (Value);
                myfile >> Value;
                m_raw->SetSlotDurationCount (Value);
                myfile >> Value;            
                nslot = Value;
                m_raw->SetSlotNum (Value);
                myfile >> page;
                myfile >> aid_start;
                myfile >> aid_end;
                rawinfo = (aid_end << 13) | (aid_start << 2) | page;
                m_raw->SetRawGroup (rawinfo);

                m_rps->SetRawAssignment(*m_raw);
                delete m_raw;		            
            }
            rpslist.rpsset.push_back (m_rps);
        }
        myfile.close();
    }
    else cout << "Unable to open file \n";
    
    return rpslist;
}*/

RPSVector configureRAW (RPSVector rpslist, string RAWConfigPath)
{
    uint16_t NRPS = 0;
    uint16_t Value = 0;
    uint32_t page = 0;
    uint32_t aid_start = 0;
    uint32_t aid_end = 0;
    uint32_t rawinfo = 0;
    
    ifstream myfile (RAWConfigPath.c_str());
    //1. get info from config file
    
    //2. define RPS
    if (myfile.is_open())
    {
        myfile >> NRPS;
        ngroup = NRPS;
        for (uint16_t kk=0; kk< NRPS; kk++)
        {
            RPS *m_rps = new RPS;
            RPS::RawAssignment *m_raw = new RPS::RawAssignment;
            
            myfile >> Value;
            m_raw->SetRawControl (Value);//support paged STA or not
            myfile >> Value;
            m_raw->SetSlotCrossBoundary (Value);
            myfile >> Value;
            m_raw->SetSlotFormat (Value);
            myfile >> Value;
            m_raw->SetSlotDurationCount (Value);
            myfile >> Value;
            nslot = Value;
            m_raw->SetSlotNum (Value);
            
            myfile >> page;
            myfile >> aid_start;
            myfile >> aid_end;
            rawinfo = (aid_end << 13) | (aid_start << 2) | page;
            m_raw->SetRawGroup (rawinfo);
            
            m_rps->SetRawAssignment(*m_raw);
            
            rpslist.rpsset.push_back (m_rps);
        }
        myfile.close();
    }
    else cout << "Unable to open file \n";
    
    return rpslist;
}

ns3::int64x64_t energyTx = 0;
ns3::int64x64_t energyRx = 0;
ns3::int64x64_t energyIdle = 0;
double energySleep;

Time timeTx;
Time timeRx;
Time timeIdle;
Time timeSleep;

//ns3::int64x64_t throughouputArray [MaxSta];
Time timeIdleArray [MaxSta];
Time timeRxArray [MaxSta];
Time timeTxArray [MaxSta];
Time timeSleepArray [MaxSta];
double dist[MaxSta];

//it prints the information regarding the state of the device
void 
PhyStateTrace (std::string context, Time start, Time duration, enum WifiPhy::State state)
{   
    /*Get the number of the node from the context*/
    /*context = "/NodeList/"+strSTA+"/DeviceList/'*'/Phy/$ns3::YansWifiPhy/State/State"*/
    unsigned first = context.find("t/");
    unsigned last = context.find("/D");
    string strNew = context.substr ((first+2),(last-first-2));
    
    int node = std::stoi(strNew);
    
    /*Create file to store states*/
    /*Add folder "StateLog" in directory*/
    /*std::stringstream ss;
    ss << "./StateLog/state_" << node << ".log";
    
    ofstream myfile;
    string dropfile = ss.str();
    myfile.open (dropfile, ios::out | ios::app);*/
    
    //prints on the terminal
    //std::cout << "node "  << strNew << "   " << Simulator::Now().GetSeconds() << "    state=" << state << " start=" << start << " duration=" << duration << std::endl;
    
    if  (GetAssocNum () == StaNum)
    {
        switch (state)
        {
            case WifiPhy::State::IDLE: //Idle
                timeIdle = timeIdle + duration;
                timeIdleArray[node] = timeIdleArray[node] + duration;
                break;
            case WifiPhy::State::RX: //Rx
                timeRx = timeRx + duration;
                timeRxArray[node] = timeRxArray[node] + duration;
                break;
            case WifiPhy::State::TX: //Tx
                timeTx = timeTx + duration;
                timeTxArray[node] = timeTxArray[node] + duration;
                break;
            case WifiPhy::State::SLEEP: //Sleep  
                timeSleep = timeSleep + duration;
                timeSleepArray[node] = timeSleepArray[node] + duration;
                break;        
        }
    }
    
    //NS_LOG_UNCOND (" STA " << node << " state " << state << " time " << start.GetSeconds());
    //prints on a .log file
    //myfile << Simulator::Now().GetSeconds() << "    state=" << state << " start=" << start << " duration=" << duration << std::endl;
    //myfile.close();
}

int main (int argc, char *argv[])
{
  LogComponentEnable ("UdpServer", LOG_INFO);


  double simulationTime = 10;
  uint32_t seed = 1;
  uint32_t  payloadSize = 256;
  uint32_t Nsta = 1;
  uint32_t NRawSta = 1;
  uint32_t SlotFormat=1;
  uint32_t  NRawSlotCount = 829;
  uint32_t  NRawSlotNum = 1;
  uint32_t NGroup = 1;
  uint32_t BeaconInterval = 102400;
  bool OutputPosition = true;
  string DataMode = "OfdmRate7_8MbpsBW2MHz";
  double datarate = 7.8;
  double bandWidth = 2;
  double Totaltraffic;
  string UdpInterval="0.2";
  string rho="250.0";
  string folder="./scratch/";
  string file="./scratch/mac-sta.txt";
  string pcapfile="./scratch/mac-s1g-slots";
  string TrafficPath;
  string Outputpath;
  uint16_t Nactive;
  double poissonrate;
  bool S1g1MfieldEnabled;
  string RAWConfigPath;
  
  double trafficPoissonrate = 9999;
  double trafficMaxRate = 0.1;
    

  CommandLine cmd;
  cmd.AddValue ("seed", "random seed", seed);
  cmd.AddValue ("simulationTime", "Simulation time in seconds", simulationTime);
  cmd.AddValue ("payloadSize", "Size of payload", payloadSize);
  cmd.AddValue ("Nsta", "number of total stations", Nsta);
  cmd.AddValue ("NRawSta", "number of stations supporting RAW", NRawSta);
  cmd.AddValue ("SlotFormat", "format of NRawSlotCount", SlotFormat);
  cmd.AddValue ("NRawSlotCount", "number of stations supporting RAW", NRawSlotCount);
  cmd.AddValue ("NRawSlotNum", "number of stations supporting RAW", NRawSlotNum);
  cmd.AddValue ("NGroup", "number of RAW group", NGroup);
  cmd.AddValue ("BeaconInterval", "Beacon interval time in us", BeaconInterval);
  cmd.AddValue ("DataMode", "Date mode", DataMode);
  cmd.AddValue ("datarate", "data rate in Mbps", datarate);
  cmd.AddValue ("bandWidth", "bandwidth in MHz", bandWidth);
  cmd.AddValue ("UdpInterval", "traffic mode", UdpInterval);
  cmd.AddValue ("Totaltraffic", "Total traffic", Totaltraffic);
  cmd.AddValue ("rho", "maximal distance between AP and stations", rho);
  cmd.AddValue ("folder", "folder where result files are placed", folder);
  cmd.AddValue ("file", "files containing reslut information", file);
  cmd.AddValue ("pcapfile", "files containing reslut information", pcapfile);
  cmd.AddValue ("Outputpath", "files path of each stations", Outputpath);
  cmd.AddValue ("TrafficPath", "files path of traffic file", TrafficPath);
  cmd.AddValue ("Nactive", "Num of active station", Nactive);
  cmd.AddValue ("poissonrate", "poisson rate", poissonrate);
  cmd.AddValue ("S1g1MfieldEnabled", "S1g1MfieldEnabled", S1g1MfieldEnabled);
  cmd.AddValue ("trafficPoissonrate", "poission rate of stations which change traffic", trafficPoissonrate);
  cmd.AddValue ("trafficMaxRate", "Max traffic change", trafficMaxRate);
  cmd.AddValue ("RAWConfigPath", "RAW Config file Path", RAWConfigPath);


  cmd.Parse (argc,argv);

  RngSeedManager::SetSeed (seed);

  NodeContainer wifiStaNode;
  wifiStaNode.Create (Nsta);
  NodeContainer wifiApNode;
  wifiApNode.Create (1);

  YansWifiChannelHelper channel = YansWifiChannelHelper ();
  channel.AddPropagationLoss ("ns3::LogDistancePropagationLossModel","Exponent", DoubleValue(3.76) ,"ReferenceLoss", DoubleValue(8.0), "ReferenceDistance", DoubleValue(1.0));
  channel.SetPropagationDelay ("ns3::ConstantSpeedPropagationDelayModel");

  YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
  phy.SetErrorRateModel ("ns3::YansErrorRateModel");
  phy.SetChannel (channel.Create ());
  phy.Set ("ShortGuardEnabled", BooleanValue (false));
  phy.Set ("ChannelWidth", UintegerValue (bandWidth));
  phy.Set ("EnergyDetectionThreshold", DoubleValue (-110.0));
  phy.Set ("CcaMode1Threshold", DoubleValue (-113.0));
  phy.Set ("TxGain", DoubleValue (0.0));
  phy.Set ("RxGain", DoubleValue (0.0));
  phy.Set ("TxPowerLevels", UintegerValue (1));
  phy.Set ("TxPowerEnd", DoubleValue (0.0));
  phy.Set ("TxPowerStart", DoubleValue (0.0));
  phy.Set ("RxNoiseFigure", DoubleValue (6.8));
  phy.Set ("LdpcEnabled", BooleanValue (true));
  phy.Set ("S1g1MfieldEnabled", BooleanValue (S1g1MfieldEnabled));

  WifiHelper wifi = WifiHelper::Default ();
  wifi.SetStandard (WIFI_PHY_STANDARD_80211ah);
  S1gWifiMacHelper mac = S1gWifiMacHelper::Default ();



  Ssid ssid = Ssid ("ns380211ah");
  StringValue DataRate;
  DataRate = StringValue (DataMode);

    wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager","DataMode", DataRate, "ControlMode", DataRate);

  mac.SetType ("ns3::StaWifiMac",
                "Ssid", SsidValue (ssid),
               "ActiveProbing", BooleanValue (false));

  NetDeviceContainer staDevice;
  staDevice = wifi.Install (phy, mac, wifiStaNode);

  RPSVector rps;
  rps = configureRAW (rps, RAWConfigPath);
    
  mac.SetType ("ns3::ApWifiMac",
                 "Ssid", SsidValue (ssid),
                 "BeaconInterval", TimeValue (MicroSeconds(BeaconInterval)),
                 "NRawStations", UintegerValue (NRawSta),
                 "RPSsetup", RPSVectorValue (rps));

  NetDeviceContainer apDevice;
  phy.Set ("TxGain", DoubleValue (3.0));
  phy.Set ("RxGain", DoubleValue (3.0));
  phy.Set ("TxPowerLevels", UintegerValue (1));
  phy.Set ("TxPowerEnd", DoubleValue (30.0));
  phy.Set ("TxPowerStart", DoubleValue (30.0));
  phy.Set ("RxNoiseFigure", DoubleValue (6.8));

  apDevice = wifi.Install (phy, mac, wifiApNode);

  //lenght of the queue (it was 10)
  Config::Set ("/NodeList/*/DeviceList/0/$ns3::WifiNetDevice/Mac/$ns3::RegularWifiMac/BE_EdcaTxopN/Queue/MaxPacketNumber", UintegerValue(10));
  Config::Set ("/NodeList/*/DeviceList/0/$ns3::WifiNetDevice/Mac/$ns3::RegularWifiMac/BE_EdcaTxopN/Queue/MaxDelay", TimeValue (NanoSeconds (6000000000000)));
 
  // mobility.
  MobilityHelper mobility;
  mobility.SetPositionAllocator ("ns3::UniformDiscPositionAllocator",
                                       "X", StringValue ("1000.0"),
                                       "Y", StringValue ("1000.0"),
                                       "rho", StringValue (rho));
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(wifiStaNode);


  MobilityHelper mobilityAp;
  Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();
  positionAlloc->Add (Vector (1000.0, 1000.0, 0.0));
  mobilityAp.SetPositionAllocator (positionAlloc);
  mobilityAp.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobilityAp.Install(wifiApNode);
  
  
   /* Internet stack*/
  InternetStackHelper stack;
  stack.Install (wifiApNode);
  stack.Install (wifiStaNode);

  Ipv4AddressHelper address;

  address.SetBase ("192.168.0.0", "255.255.0.0");
  Ipv4InterfaceContainer staNodeInterface;
  Ipv4InterfaceContainer apNodeInterface;

  staNodeInterface = address.Assign (staDevice);
  apNodeInterface = address.Assign (apDevice);

  //trace association
  for (uint16_t kk=0; kk< Nsta; kk++)
    {
      std::ostringstream STA;
      STA << kk;
      std::string strSTA = STA.str();

      assoc_record *m_assocrecord=new assoc_record;
      m_assocrecord->setstaid (kk);
      Config::Connect ("/NodeList/"+strSTA+"/DeviceList/0/$ns3::WifiNetDevice/Mac/$ns3::RegularWifiMac/$ns3::StaWifiMac/Assoc", MakeCallback (&assoc_record::SetAssoc, m_assocrecord));
      Config::Connect ("/NodeList/"+strSTA+"/DeviceList/0/$ns3::WifiNetDevice/Mac/$ns3::RegularWifiMac/$ns3::StaWifiMac/DeAssoc", MakeCallback (&assoc_record::UnsetAssoc, m_assocrecord));
      assoc_vector.push_back (m_assocrecord);
    }  
  
    Simulator::ScheduleNow(&UdpTraffic, Nsta,payloadSize, TrafficPath, staDevice);

    Simulator::Schedule(Seconds(1), &CheckAssoc, Nsta, simulationTime, wifiApNode, wifiStaNode,apNodeInterface);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables ();
    PopulateArpCache ();
     
    //./OptimalRawGroup/traffic/data-8-0.3.txt
    //unsigned een = TrafficPath.find("-");
    //unsigned twee = TrafficPath.find(".t");
    //string strTraffic = TrafficPath.substr ((een+8),(twee-een-8));
    
          
      if  (OutputPosition)
        {
          int i =0;
          while (i < Nsta)
            {
               Ptr<MobilityModel> mobility = wifiStaNode.Get (i)->GetObject<MobilityModel>();
               Vector position = mobility->GetPosition();
               //std::cout << "Sta node#" << i << ", " << "position = " << position << std::endl;
               dist[i] = mobility -> GetDistanceFrom (wifiApNode.Get (0)->GetObject<MobilityModel>());
               i++;
            }
          Ptr<MobilityModel> mobility1 = wifiApNode.Get (0)->GetObject<MobilityModel>();
          Vector position = mobility1->GetPosition();
          //std::cout << "AP node, position = " << position << std::endl;
        }
    
    /*Print of the state of the stations*/
    for (uint32_t i=0; i< Nsta; i++)
    {
        std::ostringstream STA;
        STA << i;
        std::string strSTA = STA.str();
        
        Config::Connect ("/NodeList/"+strSTA+"/DeviceList/*/Phy/$ns3::YansWifiPhy/State/State", MakeCallback(&PhyStateTrace));
    }
    
      Simulator::Run ();
      Simulator::Destroy ();

      double throughput = 0;
      //UDP
      uint32_t totalPacketsThrough = DynamicCast<UdpServer> (serverApp.Get (0))->GetReceived ();
      throughput = totalPacketsThrough * payloadSize * 8 / (simulationTime * 1000000.0);
      
      std::cout << "datarate" << "\t" << "throughput" << std::endl;
      std::cout << datarate << "\t" << throughput << " Mbit/s" << std::endl;
      
      timeRx = timeRx/Nsta;
      timeIdle = timeIdle/Nsta;
      timeTx = timeTx/Nsta;
      timeSleep = timeSleep/Nsta;  
      
    
    ofstream risultati;    
    string addressresults = Outputpath + "/moreinfo.txt";
    risultati.open (addressresults.c_str(), ios::out | ios::trunc); 
    
    //std::cout << "PATH " << addressresults << std::endl;
    risultati << "Sta node#      distance        timerx      timeidle        timetx      timesleep      totenergy" << std::endl;           
    int i = 0;
    ns3::int64x64_t totenergycons = 0;
    string spazio = "         ";
    
    /*i =0;
          while (i < Nsta)
            {  
              Time tot = timeRxArray[i]+timeIdleArray[i]+timeTxArray[i]+timeSleepArray[i];
               std::cout << "Sta node#" << i << ", " << "time TOT:" << tot.GetSeconds() << std::endl;
               i++;
            }*/
    
    i = 0;
    while (i < Nsta)
      {
        totenergycons = (timeRxArray[i].GetSeconds() * 4.4) + (timeIdleArray[i].GetSeconds() * 4.4) + (timeTxArray[i].GetSeconds() * 7.2);
         risultati << i << spazio << dist[i] << spazio << timeRxArray[i].GetSeconds() << spazio << timeIdleArray[i].GetSeconds() << spazio << timeTxArray[i].GetSeconds() << spazio << timeSleepArray[i].GetSeconds() << spazio << totenergycons << std::endl;
         totenergycons = 0;
         i++;
      }
    
    risultati.close();      
      
return 0;
}