/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2022 Pan Lab, Department of Computer Science, University of Victoria
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
 * Authors: Shengjie Shu <shengjies@uvic.ca>
 */


#include "ns3/object.h"
#include "ns3/log.h"
#include "ns3/uinteger.h"
#include "ns3/double.h"
#include "ns3/boolean.h"
#include "ns3/trace-source-accessor.h"
#include "ns3/nstime.h"
#include "quic-stream.h"
#include "ns3/node.h"
#include "ns3/string.h"

#include <algorithm>
#include <iostream>
#include <iomanip>
#include <iterator>
#include <vector>
#include <bitset>
#include <numeric>

#include "mp-quic-scheduler.h"
#include "ns3/random-variable-stream.h"



namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("MpQuicScheduler");

NS_OBJECT_ENSURE_REGISTERED (MpQuicScheduler);
// NS_OBJECT_ENSURE_REGISTERED (MpQuicSchedulerState);

// TypeId
// MpQuicSchedulerState::GetTypeId (void)
// {
//   static TypeId tid =
//     TypeId ("ns3::MpQuicSchedulerState")
//     .SetParent<Object> ()
//     .SetGroupName ("Internet")
//     .AddTraceSource ("MabReward",
//                      "The average reward get by mab",
//                      MakeTraceSourceAccessor (&MpQuicSchedulerState::m_reward),
//                      "ns3::TracedValueCallback::Uint32") 
// ;
//   return tid;
// }

// MpQuicSchedulerState::MpQuicSchedulerState ()
//   : Object ()
// {

// }

// MpQuicSchedulerState::MpQuicSchedulerState (const MpQuicSchedulerState &other)
//   : Object (other),
//     m_reward(other.m_reward)
// {
// }


TypeId
MpQuicScheduler::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::MpQuicScheduler")
    .SetParent<Object> ()
    .SetGroupName ("Internet")
    .AddAttribute ("SchedulerType",
                   "define the type of the scheduler",
                   IntegerValue (MIN_RTT),
                   MakeIntegerAccessor (&MpQuicScheduler::m_schedulerType),
                   MakeIntegerChecker<int16_t> ())
    .AddAttribute ("MabRate",
                   "define the rate of the MAB scheduler",
                   UintegerValue (100),
                   MakeUintegerAccessor (&MpQuicScheduler::m_rate),
                   MakeUintegerChecker<uint32_t> ())
    .AddAttribute ("BlestLambda",
                   "define the lambda of the BLEST",
                   UintegerValue (1000),
                   MakeUintegerAccessor (&MpQuicScheduler::m_lambda),
                   MakeUintegerChecker<uint16_t> ())
    .AddAttribute ("BlestVar",
                   "define the lambda of the BLEST",
                   UintegerValue (100),
                   MakeUintegerAccessor (&MpQuicScheduler::m_bVar),
                   MakeUintegerChecker<uint16_t> ())
    .AddAttribute ("Select",
                   "string of select",
                   UintegerValue (0),
                   MakeUintegerAccessor (&MpQuicScheduler::m_select),
                   MakeUintegerChecker<uint16_t> ())
    .AddTraceSource ("MabReward",
                     "The average reward get by mab",
                     MakeTraceSourceAccessor (&MpQuicScheduler::m_reward),
                     "ns3::TracedValueCallback::Uint32")               
     
  ;
  return tid;
}

MpQuicScheduler::MpQuicScheduler ()
  : Object (),
  m_socket(0),
  m_lastUsedPathId(0),
  m_rounds(0),
  m_reward(0),
  m_select(0)
{
  NS_LOG_FUNCTION_NOARGS ();
  m_lastUpdateRounds = 1;
  m_e = 0;
  m_rewards.push_back(0);
  m_rewardTemp.push_back(0);
  m_rewardTemp0.push_back(0);
  m_rewardAvg.push_back(0);
  // m_state = CreateObject<MpQuicSchedulerState> ();
  // m_state->TraceConnectWithoutContext ("MabReward", MakeCallback (&MpQuicScheduler::UpdateReward, this));
}

MpQuicScheduler::~MpQuicScheduler ()
{
  NS_LOG_FUNCTION_NOARGS ();
}



std::vector<double> 
MpQuicScheduler::GetNextPathIdToUse()
{
  m_subflows = m_socket->GetActiveSubflows();
  std::vector<double> tosend(m_subflows.size(), 0.0);
  if (m_subflows.empty())
  {
    // m_lastUsedPathId = 0;
    tosend.push_back(1.0);
    return tosend;
  }
  switch (m_schedulerType)
  {
    case ROUND_ROBIN:
      tosend = RoundRobin();
      break;

    case MIN_RTT:
      tosend = MinRtt();
      break;
    
    case BLEST:
      tosend = Blest();
      break;

    case ECF:
      tosend = Ecf();
      break;

    case MAB:
      tosend = Mab();
      break;

    case MAB_DELAY:
      tosend = MabDelay();
      break;

    default:
      tosend = RoundRobin();
      break;
      
  }

  return tosend;
}

std::vector<double>
MpQuicScheduler::RoundRobin()
{
  std::vector<double> tosend(m_subflows.size(), 0.0);
  if (m_subflows.size() <= 1){
    m_lastUsedPathId = 0;
    tosend[m_lastUsedPathId] = 1.0;
    return tosend;
  }

  m_lastUsedPathId = (m_lastUsedPathId + 1) % m_subflows.size();

  // std::cout<<"select 1"<<std::endl;

  // for (uint8_t i = 0; i < m_subflows.size(); i++)
  // {
  //   m_lastUsedPathId = (m_lastUsedPathId + 1) % m_subflows.size();
  //   if (m_socket->AvailableWindow (m_lastUsedPathId) > 0){
  //     break;
  //   }
  // }
  tosend[m_lastUsedPathId] = 1.0;
  return tosend;
}

std::vector<double>
MpQuicScheduler::MinRtt()
{
  NS_LOG_FUNCTION (this);
  std::vector<double> tosend(m_subflows.size(), 0.0);

  if (m_subflows.size() <= 1){
    m_lastUsedPathId = 0;
    tosend[m_lastUsedPathId] = 1.0;
    return tosend;
  }
  // std::cout<<"select 1"<<std::endl;
  if (m_subflows[1]->m_tcb->m_lastRtt.Get().GetSeconds() == 0) {
    m_lastUsedPathId = 1;
    tosend[m_lastUsedPathId] = 1.0;
    return tosend;
  }

  Time rttS;
  Time rttF;
  uint8_t fastPathId = 1;
  uint8_t slowPathId = 0;

  if (m_subflows[0]->m_tcb->m_lastRtt >= m_subflows[1]->m_tcb->m_lastRtt){
    rttS = m_subflows[0]->m_tcb->m_lastRtt;
    rttF = m_subflows[1]->m_tcb->m_lastRtt;
    slowPathId = 0;
    fastPathId = 1;
  } else {
    rttS = m_subflows[1]->m_tcb->m_lastRtt;
    rttF = m_subflows[0]->m_tcb->m_lastRtt;
    slowPathId = 1;
    fastPathId = 0;
  }

  if (m_socket->AvailableWindow (fastPathId) > 0){
    m_lastUsedPathId = fastPathId;
  }else {
    m_lastUsedPathId = slowPathId;
  }

  tosend[m_lastUsedPathId] = 1.0;
  return tosend;
}

void
MpQuicScheduler::SetSocket(Ptr<QuicSocketBase> sock)
{
  NS_LOG_FUNCTION (this);
  m_socket = sock;
}


std::vector<double>
MpQuicScheduler::Blest() //only allow two subflows
{
  NS_LOG_FUNCTION (this);
  std::vector<double> tosend(m_subflows.size(), 0.0);

  if (m_subflows.size() <= 1){
    m_lastUsedPathId = 0;
    tosend[m_lastUsedPathId] = 1.0;
    return tosend;
  }
  // std::cout<<"select 1"<<std::endl;
  if (m_subflows[1]->m_tcb->m_lastRtt.Get().GetSeconds() == 0) {
    m_lastUsedPathId = 1;
    tosend[m_lastUsedPathId] = 1.0;
    return tosend;
  }
  
  Time rttS;
  Time rttF;
  uint8_t fastPathId = 1;
  uint8_t slowPathId = 0;
  uint32_t mss = m_socket->GetSegSize();

  if (m_subflows[0]->m_tcb->m_lastRtt > m_subflows[1]->m_tcb->m_lastRtt){
    rttS = m_subflows[0]->m_tcb->m_lastRtt;
    rttF = m_subflows[1]->m_tcb->m_lastRtt;
    slowPathId = 0;
    fastPathId = 1;
  } else {
    rttS = m_subflows[1]->m_tcb->m_lastRtt;
    rttF = m_subflows[0]->m_tcb->m_lastRtt;
    slowPathId = 1;
    fastPathId = 0;
  }

  if (m_socket->AvailableWindow (fastPathId) > 0){
    m_lastUsedPathId = fastPathId;
  } else {
    double_t rtts = rttS.GetSeconds()/rttF.GetSeconds();
    double_t cwndF = m_subflows[fastPathId]->m_tcb->m_cWnd/mss;
    double_t X = mss * (cwndF + (rtts-1)/2) * rtts;
    double_t comp = m_socket->GetTxAvailable() - (m_socket->BytesInFlight(slowPathId)+mss);
    // double_t comp = m_subflows[fastPathId]->m_tcb->m_cWnd.Get() - m_socket->BytesInFlight(slowPathId) - mss;
    // std::cout<<"available: "<<m_socket->GetTxAvailable()<<" cwnd: "<<m_subflows[fastPathId]->m_tcb->m_cWnd.Get()<<" X: "<<X<<" comp: "<<comp<<std::endl;
    m_lambda = m_lambda + m_bVar;
    if(X * m_lambda > comp) { //not send on slow path
      m_lastUsedPathId = fastPathId;
    } else {
      m_lastUsedPathId = slowPathId;
    }
  }
  
  tosend[m_lastUsedPathId] = 1.0;
  return tosend;
  // std::cout <<"In use pid: " << unsigned(m_lastUsedPathId) <<std::endl;
}


std::vector<double>
MpQuicScheduler::Ecf() //only allow two subflows
{
  NS_LOG_FUNCTION (this);
  std::vector<double> tosend(m_subflows.size(), 0.0);
  if (m_subflows.size() <= 1){
    m_lastUsedPathId = 0;
    tosend[m_lastUsedPathId] = 1.0;
    return tosend;
  }

  // std::cout<<"select 1"<<std::endl;
  if (m_subflows[1]->m_tcb->m_lastRtt.Get().GetSeconds() == 0) {
    m_lastUsedPathId = (m_lastUsedPathId + 1) % m_subflows.size();
    tosend[m_lastUsedPathId] = 1.0;
    return tosend;
  } 
  
  
  Time rttS;
  Time rttF;
  uint8_t fastPathId = 1;
  uint8_t slowPathId = 0;

  if (m_subflows[0]->m_tcb->m_lastRtt > m_subflows[1]->m_tcb->m_lastRtt){
    rttS = m_subflows[0]->m_tcb->m_lastRtt;
    rttF = m_subflows[1]->m_tcb->m_lastRtt;
    slowPathId = 0;
    fastPathId = 1;
  } else {
    rttS = m_subflows[1]->m_tcb->m_lastRtt;
    rttF = m_subflows[0]->m_tcb->m_lastRtt;
    slowPathId = 1;
    fastPathId = 0;
  }

  if (m_socket->AvailableWindow (fastPathId) > 0){
    m_lastUsedPathId = fastPathId;
  }else {
    uint32_t k = m_socket->GetBytesInBuffer();
    double n = 1 + k/m_subflows[fastPathId]->m_tcb->m_cWnd.Get();
    double delta = max(m_subflows[fastPathId]->m_tcb->m_rttVar.GetSeconds(),m_subflows[slowPathId]->m_tcb->m_rttVar.GetSeconds());
    if (n*rttF.GetSeconds() < (1+m_waiting*1)*(rttS.GetSeconds()+delta)){
      if (k/m_subflows[slowPathId]->m_tcb->m_cWnd.Get() * rttS.GetSeconds() >= 2*rttF.GetSeconds()+delta){
        m_waiting = 1;
        m_lastUsedPathId = fastPathId;
        tosend[m_lastUsedPathId] = 1.0;
        return tosend;
      } else {
        m_lastUsedPathId = slowPathId;
      }
    } else {
      m_waiting = 0;
      m_lastUsedPathId = slowPathId;
    }
  }  

  tosend[m_lastUsedPathId] = 1.0;
  return tosend;
  // std::cout <<"In use pid: " << unsigned(m_lastUsedPathId) <<std::endl;
}


std::vector<double>
MpQuicScheduler::Mab()
{
  NS_LOG_FUNCTION (this);
  std::vector<double> tosend(m_subflows.size(), 0.0);
  if (m_subflows.size() < 2){
    m_lastUsedPathId = 0;
    tosend[m_lastUsedPathId] = 1.0;
    return tosend;
  }

  // std::cout<<"select 1"<<std::endl;
  uint32_t reward;
  double n;
  uint8_t rPid = 0;
  uint32_t mss = m_subflows[0]->m_tcb->m_cWnd.Get(); //m_socket->GetSegSize(); //
  Time rtt = m_subflows[0]->m_tcb->m_lastRtt;
  if (m_socket->AvailableWindow (0) > 0){
    reward = mss/rtt.GetSeconds(); // * 1/std::sqrt(m_lostPackets)
  } else {
    n = m_socket->GetBytesInBuffer()/m_subflows[0]->m_tcb->m_cWnd.Get();
    // n = 1 + m_socket->GetBytesInBuffer()/m_subflows[0]->m_tcb->m_cWnd.Get();
    reward = mss/rtt.GetSeconds() * n;
  }
  m_rewards[0] = (m_rewards[0]*(m_subflows[0]->m_rounds) + reward)/(m_subflows[0]->m_rounds+1);
  double delta = 1+ m_rounds * std::log(m_rounds) * std::log(m_rounds);
  double_t normal = std::sqrt(2*std::log(delta)/m_subflows[0]->m_rounds);
  double_t maxAction = m_rewards[0]/m_rate + normal;
  for (uint8_t pid = 1; pid < m_subflows.size(); pid++)
  {
    mss = m_subflows[pid]->m_tcb->m_cWnd.Get();
    rtt = m_subflows[pid]->m_tcb->m_lastRtt;
    if (m_socket->AvailableWindow (pid) > 0){
      reward = mss/rtt.GetSeconds(); // * 1/std::sqrt(m_lostPackets)
    } else {
      n = 1 + m_socket->GetBytesInBuffer()/m_subflows[pid]->m_tcb->m_cWnd.Get();
      reward = mss/(n*rtt.GetSeconds());
    }
    if(m_rewards.size() < m_subflows.size()){
      m_rewards.push_back(0);
    }
    m_rewards[pid] = (m_rewards[pid]*(m_subflows[pid]->m_rounds) + reward)/(m_subflows[pid]->m_rounds+1);
    normal = std::sqrt(2*std::log(delta)/m_subflows[pid]->m_rounds);
    double_t newAction = m_rewards[pid]/m_rate + normal;
    if (m_rewards[pid] == 0 && newAction == 1) { //when new path just established
      rPid = pid;
    }
    if (newAction >= maxAction){
      maxAction = newAction;
      rPid = pid;
    }
  }
  m_lastUsedPathId = rPid;
  m_subflows[m_lastUsedPathId]->m_rounds++;
  
  m_reward = m_rewards[rPid];
  m_rounds++;
  
  tosend[m_lastUsedPathId] = 1.0;
  return tosend;
}


std::vector<double>
MpQuicScheduler::MabDelay()
{
  NS_LOG_FUNCTION (this);
  std::vector<double> tosend(m_subflows.size(), 0.0);
  m_rounds++;
  uint8_t K = m_subflows.size();
  if(m_cost.size() < K)
  {
    m_cost.push_back(0.0);
    m_L.push_back(0.0);
    m_eL.push_back(0.0);
    m_p.push_back(0.0);
    for (uint8_t i = 0; i < K; i++)
    {
      m_p[i] = 1.0/K;
    }
  }

  if (K < 2)
  {
    m_lastUsedPathId = 0;
    tosend[m_lastUsedPathId] = 1.0;
    return tosend;
  }

  
  if (m_p[0] == 1.0/K)
  {
    Ptr<UniformRandomVariable> x = CreateObject<UniformRandomVariable> ();
    m_lastUsedPathId = x->GetInteger (0,K-1);
  }
  else
  {
    m_lastUsedPathId = std::max_element(m_p.begin(),m_p.end()) - m_p.begin();
  }
  
  return m_p;
  // std::cout<<"m_lastUsedPathId "<<int(m_lastUsedPathId) <<std::endl;
  
  // if (m_rewardTemp.size() < 2){
  //   m_lastUsedPathId = 1;
  //   return;
  // }
  
  // uint32_t reward;
  // uint8_t rPid = 0;
  // if (m_socket->AvailableWindow (0) > 0){
  //   reward = m_rewardTemp[0];
  // } else {
  //   reward = m_rewardTemp0[0];
  // }
  // m_rewardAvg[0] = m_rewardAvg[0]+(reward-m_rewardAvg[0])/(m_subflows[0]->m_rounds+1);
  // double_t delta = 1+ m_rounds * std::log(m_rounds) * std::log(m_rounds);
  // double_t normal = std::sqrt(2*std::log(delta)/m_subflows[0]->m_rounds);
  // double_t maxAction = m_rewardAvg[0]/m_rate + normal;
  // for (uint8_t pid = 1; pid < m_subflows.size(); pid++)
  // {
  //   if (m_socket->AvailableWindow (pid) > 0){
  //     reward = m_rewardTemp[0];
  //   } else {
  //     reward = m_rewardTemp0[0];
  //   }
  //   m_rewardAvg[pid] = m_rewardAvg[pid]+(reward-m_rewardAvg[pid])/(m_subflows[pid]->m_rounds+1);
  //   normal = std::sqrt(2*std::log(delta)/m_subflows[pid]->m_rounds);
  //   double_t newAction = m_rewardAvg[pid]/m_rate + normal;
  //   if (newAction >= maxAction){
  //     maxAction = newAction;
  //     rPid = pid;
  //   }
  // }
  // m_lastUsedPathId = rPid;
  // m_subflows[m_lastUsedPathId]->m_rounds++;
  // m_reward = m_rewardAvg[rPid];
  // m_rounds++;
}


void
MpQuicScheduler::UpdateRewardMab(uint8_t pathId, uint32_t lostOut, uint32_t inflight, uint32_t round)
{
  if (m_schedulerType != MAB_DELAY){
    return;
  }
  uint8_t K = m_subflows.size();
  m_missingRounds.insert(m_missingRounds.end(), m_rounds - round);
  // m_lastUpdateRounds = m_rounds;
  if (std::accumulate(m_missingRounds.begin(), m_missingRounds.end(), 0.0) / m_missingRounds.size() >= std::pow(2, m_e))
  {
    m_e = m_e+1;
    for (uint8_t i = 0; i < K; i++)
    {
      m_L[i] = 0.0;
    }
  }

  double ln = std::log(K);
  double eta = std::sqrt(ln/std::pow(2, m_e));
  // double eta = std::sqrt(ln/(K*m_rounds));
  // double eta = std::sqrt(ln/std::pow(2, 1));
  
  // uint32_t mss = m_subflows[0]->m_tcb->m_cWnd.Get(); //m_socket->GetSegSize(); //
  Time rtt = m_subflows[pathId]->m_tcb->m_lastRtt;
  m_cost[pathId] = rtt.GetSeconds() + (double)lostOut/10 + (double)inflight/m_subflows[pathId]->m_tcb->m_cWnd.Get()/10;
  if (m_cost[pathId] > 1){
    m_cost[pathId] = 1;
  }

  // m_L[pathId] = m_L[pathId] + eta * m_cost[pathId]/m_p[pathId];
  m_L[pathId] = eta * m_cost[pathId]/m_p[pathId];

  for (uint8_t i = 0; i < m_subflows.size(); i++)
  {
    m_eL[i] = std::exp(-m_L[i]);
  }

  double pSumTemp = std::accumulate(m_eL.begin(), m_eL.end(), 0.0);

  for (uint8_t i = 0; i < m_subflows.size(); i++)
  {
    m_p[i] = m_eL[i]/pSumTemp;
    // std::cout<<"path "<< int(i) <<" m_cost "<<m_cost[i] <<" m_L "<< m_L[i] << " m_p "<< m_p[i] <<std::endl;
  }

  // double n;
  // for (uint8_t pid = 0; pid < m_subflows.size(); pid++)
  // {
  //   if(m_rewardTemp.size() < m_subflows.size()){
  //     m_rewardTemp.push_back(0);
  //     m_rewardTemp0.push_back(0);
  //   }
  //   n = 1 + m_socket->GetBytesInBuffer()/m_subflows[0]->m_tcb->m_cWnd.Get();
  //   m_rewardTemp[pid] = mss/rtt.GetSeconds();
  //   m_rewardTemp0[pid] = mss/(n*rtt.GetSeconds());
  //   }
}


uint32_t 
MpQuicScheduler::GetCurrentRound()
{
  NS_LOG_FUNCTION (this);
  return m_rounds;
}


std::vector<double>
MpQuicScheduler::LocalOpt()
{
  NS_LOG_FUNCTION (this);
  std::vector<double> tosend(m_subflows.size(), 0.0);
  if (m_subflows.size() < 2){
    m_lastUsedPathId = 0;
    tosend[m_lastUsedPathId] = 1.0;
    return tosend;
  }
  bitset<12> binary(m_select);

  // std::cout<<"select 1"<<std::endl;
  if (m_rounds > 12){
    m_rounds = 0;
  }
  char temp = binary.to_string().at(m_rounds);
  string s;
  s.push_back(temp);
  m_lastUsedPathId = stoi(s);
  m_rounds++;
  // std::cout<<"m_lastUsedPathId "<<int(m_lastUsedPathId) <<std::endl;

  tosend[m_lastUsedPathId] = 1.0;
  return tosend;
}


void
MpQuicScheduler::SetNumOfLostPackets(uint16_t lost){
  m_lostPackets = lost;
}



} // namespace ns3
