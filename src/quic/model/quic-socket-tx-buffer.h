/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2019 SIGNET Lab, Department of Information Engineering, University of Padova
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
 * Authors: Alvise De Biasio <alvise.debiasio@gmail.com>
 *          Federico Chiariotti <chiariotti.federico@gmail.com>
 *          Michele Polese <michele.polese@gmail.com>
 *          Davide Marcato <davidemarcato@outlook.com>
 *          
 */

#ifndef QUICSOCKETTXBUFFER_H
#define QUICSOCKETTXBUFFER_H

#include "ns3/object.h"
#include "ns3/traced-value.h"
#include "ns3/sequence-number.h"
#include "ns3/nstime.h"
#include "quic-subheader.h"
#include "ns3/packet.h"
#include "ns3/tcp-socket-base.h"

namespace ns3 {

/**
 * \ingroup quic
 *
 * \brief Item that encloses the application packet and some flags for it
 */
class QuicSocketTxItem
{
public:
  QuicSocketTxItem ();
  QuicSocketTxItem (const QuicSocketTxItem &other);

  /**
   * \brief Print the Item
   * \param os ostream
   */
  void Print (std::ostream &os) const;

  Ptr<Packet> m_packet;             //!< packet associated to this QuicSocketTxItem
  SequenceNumber32 m_packetNumber;  //!< sequence number
  bool m_lost;                      //!< true if the packet is lost
  bool m_retrans;                   //!< true if it is a retx
  bool m_sacked;                    //!< true if already acknowledged
  bool m_acked;                     //!< true if already passed to the application
  bool m_isStream;                  //!< true for frames of a stream (not control)
  bool m_isStream0;                 //!< true for a frame from stream 0
  Time m_lastSent;                  //!< time at which it was sent
  Time m_ackTime;                   //!< time at which the packet was first acked (if m_sacked is true)

};

/**
 * \ingroup quic
 *
 * \brief Tx socket buffer for QUIC
 */
class QuicSocketTxBuffer : public Object
{
public:
  /**
   * \brief Get the type ID.
   * \return the object TypeId
   */
  static TypeId GetTypeId (void);

  QuicSocketTxBuffer ();
  virtual ~QuicSocketTxBuffer (void);

  /**
   * Print the buffer information to a string,
   * including the list of sent packets
   *
   * \param os the std::ostream object
   */
  void Print (std::ostream & os) const;
  //friend std::ostream & operator<< (std::ostream & os, QuicSocketTxBuffer const & quicTxBuf);

  /**
   * Add a packet to the tx buffer
   *
   * \param p a smart pointer to a packet
   * \return true if the insertion was successful
   */
  bool Add (Ptr<Packet> p);

  /**
   * \brief Request the next packet to transmit
   *
   * \param numBytes the number of bytes of the next packet to transmit requested
   * \param seq the sequence number of the next packet to transmit
   * \return the next packet to transmit
   */
  Ptr<Packet> NextSequence (uint32_t numBytes, const SequenceNumber32 seq);

  /**
   * \brief Get a block of data not transmitted yet and move it into SentList
   *
   * \param numBytes number of bytes of the QuicSocketTxItem requested
   * \return the item that contains the right packet
   */
  QuicSocketTxItem* GetNewSegment (uint32_t numBytes);

  /**
   * Process an acknowledgment, set the packets in the send buffer as acknowledged, mark
   * lost packets (according to the QUIC IETF draft) and return pointers to the newly
   * acked packets
   *
   * \brief Process an ACK
   *
   * \param tcb The state of the socket (used for loss detection)
   * \param largestAcknowledged The largest acknowledged sequence number
   * \param additionalAckBlocks The sequence numbers that were just acknowledged
   * \param gaps The gaps in the acknowledgment
   * \return a vector containing the newly acked packets for congestion control purposes
   */
  std::vector<QuicSocketTxItem*> OnAckUpdate (Ptr<TcpSocketState> tcb, const uint32_t largestAcknowledged, const std::vector<uint32_t> &additionalAckBlocks, const std::vector<uint32_t> &gaps);

  /**
   * Get the max size of the buffer
   *
   * \return the maximum buffer size in bytes
   */
  uint32_t GetMaxBufferSize (void) const;

  /**
   * Set the max size of the buffer
   *
   * \param n the maximum buffer size in bytes
   */
  void SetMaxBufferSize (uint32_t n);

  /**
   * \brief Get all the packets marked as lost
   *
   * \return a vector containing the packets marked as lost
   */
  std::vector<QuicSocketTxItem*> DetectLostPackets ();

  /**
   * Compute the available space in the buffer
   *
   * \return the available space in the buffer
   */
  uint32_t Available (void) const;

  /**
   * Returns the total number of bytes in the application buffer
   *
   * \return the total number of bytes in the application buffer
   */
  uint32_t AppSize (void) const;

  /**
   * \brief Return total bytes in flight
   *
   * \returns total bytes in flight
   */
  uint32_t BytesInFlight () const;

  /**
   * Return the number of frames for stream 0 is in the buffer
   *
   * \return the number of frames for stream 0 is in the buffer
   */
  uint32_t GetNumFrameStream0InBuffer (void) const;

  /**
   * Return the next frame for stream 0 to be sent
   * and add this packet to the sent list
   *
   * \param seq the sequence number of the packet
   * \return a smart pointer to the packet, 0 if there are no packets from stream 0
   */
  Ptr<Packet> NextStream0Sequence (const SequenceNumber32 seq);

  /**
   * \brief Reset the sent list
   *
   * Move all but the first 'keepItems' packets from the sent list to the
   * appList.  By default, the HEAD of the sent list is kept and all others
   * moved to the appList.  All items kept on the sent list
   * are then marked as un-sacked, un-retransmitted, and lost.
   *
   * \param keepItems Keep a number of items at the front of the sent list
   */
  void ResetSentList (uint32_t keepItems = 1);

  /**
   * Mark a packet as lost
   * \param the sequence number of the packet
   * \return true if the packet is in the send buffer
   */
  bool MarkAsLost (const SequenceNumber32 seq);

  /**
   * Put the lost packets at the beginning of the application buffer to retransmit them
   * \param the sequence number of the retransmitted packet
   * \return the number of lost bytes
   */
  uint32_t Retransmission (SequenceNumber32 packetNumber);

private:
  typedef std::list<QuicSocketTxItem*> QuicTxPacketList;  //!< container for data stored in the buffer

  /**
   * \brief Discard acknowledged data from the sent list
   */
  void CleanSentList ();

  /**
   * \brief Merge two QuicSocketTxItem
   *
   * Merge t2 in t1. It consists in copying the lastSent field if t2 is more
   * recent than t1. Retransmitted field is copied only if it set in t2 but not
   * in t1. Sacked is copied only if it is true in both items.
   *
   * \param t1 first item
   * \param t2 second item
   */
  void MergeItems (QuicSocketTxItem &t1, QuicSocketTxItem &t2) const;

  // Available only for streams
  void SplitItems (QuicSocketTxItem &t1, QuicSocketTxItem &t2, uint32_t size) const;

  QuicTxPacketList m_appList;          //!< List of buffered application packets to be transmitted with additional info
  QuicTxPacketList m_sentList;         //!< List of sent packets with additional info
  uint32_t m_maxBuffer;                //!< Max number of data bytes in buffer (SND.WND)
  uint32_t m_appSize;                  //!< Size of all data in the application list
  uint32_t m_sentSize;                 //!< Size of all data in the sent list
  uint32_t m_numFrameStream0InBuffer;  //!< Number of Stream 0 frames buffered
};


} // namepsace ns3

#endif /* QUIC_SOCKET_TX_BUFFER_H */
