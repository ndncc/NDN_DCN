/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil -*- */
/*
 * Copyright (c) 2011 University of California, Los Angeles
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
 * Author: Yuanjie Li <yuanjie.li@cs.ucla.edu>
 */

#include "ndn-bcube-l3-protocol.h"

#include "ns3/packet.h"
#include "ns3/node.h"
#include "ns3/log.h"
#include "ns3/callback.h"
#include "ns3/uinteger.h"
#include "ns3/trace-source-accessor.h"
#include "ns3/object-vector.h"
#include "ns3/pointer.h"
#include "ns3/simulator.h"
#include "ns3/random-variable.h"

#include "ns3/ndn-header-helper.h"
#include "ns3/ndn-pit.h"
#include "ns3/ndn-interest.h"
#include "ns3/ndn-content-object.h"
#include "ns3/names.h"

#include "ns3/ndn-face.h"
#include "ns3/ndn-forwarding-strategy.h"

#include "ndn-net-device-face.h"

#include <boost/foreach.hpp>
#include <algorithm>

NS_LOG_COMPONENT_DEFINE ("ndn.BCubeL3Protocol");

namespace ns3 {
namespace ndn {

const uint16_t BCubeL3Protocol::ETHERNET_FRAME_TYPE = 0x7777;

uint64_t BCubeL3Protocol::s_interestCounter = 0;
uint64_t BCubeL3Protocol::s_dataCounter = 0;

NS_OBJECT_ENSURE_REGISTERED (BCubeL3Protocol);

uint64_t
BCubeL3Protocol::GetInterestCounter ()
{
  return s_interestCounter;
}

uint64_t
BCubeL3Protocol::GetDataCounter ()
{
  return s_dataCounter;
}


TypeId
BCubeL3Protocol::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::ndn::BCubeL3Protocol")
    .SetGroupName ("ndn")
    .SetParent<Object> ()
    .AddConstructor<BCubeL3Protocol> ()
  ;
  return tid;
}

BCubeL3Protocol::BCubeL3Protocol()
: m_faceCounter (0)
{
  NS_LOG_FUNCTION (this);
}

BCubeL3Protocol::~BCubeL3Protocol ()
{
  NS_LOG_FUNCTION (this);
}

/*
 * This method is called by AddAgregate and completes the aggregation
 * by setting the node in the ndn stack
 */
void
BCubeL3Protocol::NotifyNewAggregate ()
{
  // not really efficient, but this will work only once
  if (m_node == 0)
    {
      m_node = GetObject<Node> ();
      if (m_node != 0)
        {
          // NS_ASSERT_MSG (m_pit != 0 && m_fib != 0 && m_contentStore != 0 && m_forwardingStrategy != 0,
          //                "PIT, FIB, and ContentStore should be aggregated before BCubeL3Protocol");
          NS_ASSERT_MSG (m_forwardingStrategy != 0,
                         "Forwarding strategy should be aggregated before BCubeL3Protocol");
        }
    }
  // if (m_pit == 0)
  //   {
  //     m_pit = GetObject<Pit> ();
  //   }
  // if (m_fib == 0)
  //   {
  //     m_fib = GetObject<Fib> ();
  //   }
  if (m_forwardingStrategy == 0)
    {
      m_forwardingStrategy = GetObject<ForwardingStrategy> ();
    }
  // if (m_contentStore == 0)
  //   {
  //     m_contentStore = GetObject<ContentStore> ();
  //   }

  Object::NotifyNewAggregate ();
}

void
BCubeL3Protocol::DoDispose (void)
{
  NS_LOG_FUNCTION (this);

  for (FaceList::iterator i = m_uploadfaces.begin (); i != m_uploadfaces.end (); ++i)
    {
      *i = 0;
    }
  for (FaceList::iterator i = m_downloadfaces.begin (); i != m_downloadfaces.end (); ++i)
    {
      *i = 0;
    }
  m_uploadfaces.clear ();
  m_downloadfaces.clear ();
  m_node = 0;

  // Force delete on objects
  m_forwardingStrategy = 0; // there is a reference to PIT stored in here

  Object::DoDispose ();
}

uint32_t
BCubeL3Protocol::AddFace (const Ptr<Face> &uploadface, const Ptr<Face> &downloadface)
{
	NS_ASSERT(m_faceCounter%2==0);
	
	//Add upload face
  uploadface->SetId (m_faceCounter); // sets a unique ID of the face. This ID serves only informational purposes

  // ask face to register in lower-layer stack
  uploadface->RegisterProtocolHandler (MakeCallback (&BCubeL3Protocol::Receive, this));

  m_uploadfaces.push_back (uploadface);
  m_faceCounter++;

  m_forwardingStrategy->AddFace (uploadface); // notify that face is added
  
  //Add download face
  downloadface->SetId (m_faceCounter); // sets a unique ID of the face. This ID serves only informational purposes

  // ask face to register in lower-layer stack
  downloadface->RegisterProtocolHandler (MakeCallback (&BCubeL3Protocol::Receive, this));

  m_downloadfaces.push_back (downloadface);
  m_faceCounter++;

  m_forwardingStrategy->AddFace (downloadface); // notify that face is added
  
  return downloadface->GetId ();
}

uint32_t
BCubeL3Protocol::AddAppFace (const Ptr<Face> &face)
{
	NS_ASSERT(m_faceCounter%2==0);
	//To be consistent with AddFace(), AppFace would "consume" ids, 
	//and the real id is the even number (same as upload face)
	face->SetId (m_faceCounter);
	
	face->RegisterProtocolHandler (MakeCallback (&BCubeL3Protocol::Receive, this));
	
	m_uploadfaces.push_back (face);
	m_downloadfaces.push_back (face);
		
	m_forwardingStrategy->AddFace (face); // notify that face is added
	
	m_faceCounter +=2;
	
	return face->GetId();
}

void
BCubeL3Protocol::RemoveFace (Ptr<Face> face)
{
  // ask face to register in lower-layer stack
  face->RegisterProtocolHandler (MakeNullCallback<void,const Ptr<Face>&,const Ptr<const Packet>&> ());
  Ptr<Pit> pit = GetObject<Pit> ();

  // just to be on a safe side. Do the process in two steps
  std::list< Ptr<pit::Entry> > entriesToRemoves;
  for (Ptr<pit::Entry> pitEntry = pit->Begin (); pitEntry != 0; pitEntry = pit->Next (pitEntry))
    {
      pitEntry->RemoveAllReferencesToFace (face);

      // If this face is the only for the associated FIB entry, then FIB entry will be removed soon.
      // Thus, we have to remove the whole PIT entry
      if (pitEntry->GetFibEntry ()->m_faces.size () == 1 &&
          pitEntry->GetFibEntry ()->m_faces.begin ()->GetFace () == face)
        {
          entriesToRemoves.push_back (pitEntry);
        }
    }
  BOOST_FOREACH (Ptr<pit::Entry> removedEntry, entriesToRemoves)
    {
      pit->MarkErased (removedEntry);
    }

  FaceList::iterator face_it = find (m_uploadfaces.begin(), m_uploadfaces.end(), face);
  if(face_it != m_uploadfaces.end())
  	m_uploadfaces.erase (face_it);
  	
  face_it = find (m_downloadfaces.begin(), m_downloadfaces.end(), face);
  if (face_it != m_downloadfaces.end ())
  	m_downloadfaces.erase (face_it);
  

  GetObject<Fib> ()->RemoveFromAll (face);
  m_forwardingStrategy->RemoveFace (face); // notify that face is removed
}

Ptr<Face>
BCubeL3Protocol::GetUploadFace (uint32_t index) const
{
  NS_ASSERT (0 <= index && index < 2*m_uploadfaces.size () && index%2==0);
  if(DynamicCast<NetDeviceFace>(m_uploadfaces[index/2])!=0)
  	return m_uploadfaces[index/2];
  else	//app-face
  	return 0;
}

Ptr<Face>
BCubeL3Protocol::GetDownloadFace (uint32_t index) const
{
  NS_ASSERT (0 <= index && index < 2*m_downloadfaces.size () && index%2==1);
  return m_downloadfaces[index/2];
}

Ptr<Face>
BCubeL3Protocol::GetFaceById (uint32_t index) const
{
  BOOST_FOREACH (const Ptr<Face> &face, m_uploadfaces) // this function is not supposed to be called often, so linear search is fine
    {
      if (face->GetId () == index)
        return face;
    }
  BOOST_FOREACH (const Ptr<Face> &face, m_downloadfaces) // this function is not supposed to be called often, so linear search is fine
    {
      if (face->GetId () == index)
        return face;
    }
  return 0;
}

Ptr<Face>
BCubeL3Protocol::GetUploadFaceByNetDevice (Ptr<NetDevice> netDevice) const
{
  BOOST_FOREACH (const Ptr<Face> &face, m_uploadfaces) // this function is not supposed to be called often, so linear search is fine
    {
      Ptr<NetDeviceFace> netDeviceFace = DynamicCast<NetDeviceFace> (face);
      if (netDeviceFace == 0) continue;

      if (netDeviceFace->GetNetDevice () == netDevice)
        return face;
    }  
  return 0;
}

Ptr<Face>
BCubeL3Protocol::GetDownloadFaceByNetDevice (Ptr<NetDevice> netDevice) const
{  
  BOOST_FOREACH (const Ptr<Face> &face, m_downloadfaces) // this function is not supposed to be called often, so linear search is fine
    {
      Ptr<NetDeviceFace> netDeviceFace = DynamicCast<NetDeviceFace> (face);
      if (netDeviceFace == 0) continue;

      if (netDeviceFace->GetNetDevice () == netDevice)
        return face;
    }
  return 0;
}

uint32_t
BCubeL3Protocol::GetNFaces (void) const
{
  return m_uploadfaces.size () + m_downloadfaces.size();
}

// Callback from lower layer
void
BCubeL3Protocol::Receive (const Ptr<Face> &face, const Ptr<const Packet> &p)
{
  if (!face->IsUp ())
    return;

  NS_LOG_DEBUG (*p);

  NS_LOG_LOGIC ("Packet from face " << *face << " received on node " <<  m_node->GetId ());
  
  
  Ptr<Packet> packet = p->Copy (); // give upper layers a rw copy of the packet
  try
    {
      HeaderHelper::Type type = HeaderHelper::GetNdnHeaderType (p);
      switch (type)
        {
        case HeaderHelper::INTEREST_NDNSIM:
          {
            s_interestCounter ++;
            Ptr<Interest> header = Create<Interest> ();

            // Deserialization. Exception may be thrown
            packet->RemoveHeader (*header);
            NS_ASSERT_MSG (packet->GetSize () == 0, "Payload of Interests should be zero");
						
						if(header->GetNack()==Interest::NORMAL_INTEREST)
						{
							//servers receive interest from download link
							if(std::find(m_downloadfaces.begin(), m_downloadfaces.end(), face) != m_downloadfaces.end())
							{
								//NS_LOG_UNCOND("BCubeL3Protocol: "<<Names::FindName(m_node)<<" receives interest from face="<<face->GetId());
								m_forwardingStrategy->OnInterest (face, header, p/*original packet*/);
							}
							else
								return;
								//m_forwardingStrategy->OnInterest (m_uploadfaces[face->GetId()/2], header, p/*original packet*/);
						}
						else
						//servers receive nack from upload link
							if(std::find(m_uploadfaces.begin(), m_uploadfaces.end(), face) != m_uploadfaces.end())
							{
								//NS_LOG_UNCOND("BCubeL3Protocol: "<<Names::FindName(m_node)<<" receives data from face="<<face->GetId());
								m_forwardingStrategy->OnInterest (face, header, p/*original packet*/);
							}
							else
								return;
								//m_forwardingStrategy->OnInterest (m_uploadfaces[face->GetId()/2], header, p/*original packet*/);
							
            //m_forwardingStrategy->OnInterest (face, header, p/*original packet*/);
            
            break;
          }
        case HeaderHelper::CONTENT_OBJECT_NDNSIM:
          {
          	
            s_dataCounter ++;
            Ptr<ContentObject> header = Create<ContentObject> ();

            static ContentObjectTail contentObjectTrailer; //there is no data in this object

            // Deserialization. Exception may be thrown
            packet->RemoveHeader (*header);
            packet->RemoveTrailer (contentObjectTrailer);
            
            //servers receive Data from upload link
						if(std::find(m_uploadfaces.begin(), m_uploadfaces.end(), face) != m_uploadfaces.end())
						{
							//NS_LOG_UNCOND("BCubeL3Protocol: Receive data from face="<<face->GetId()<<" node="<<m_node->GetId());
							m_forwardingStrategy->OnData (face, header, packet/*payload*/, p/*original packet*/);
						}
						else
							return;
							//m_forwardingStrategy->OnData (m_uploadfaces[face->GetId()/2], header, packet/*payload*/, p/*original packet*/);

            //m_forwardingStrategy->OnData (face, header, packet/*payload*/, p/*original packet*/);
            break;
          }
        case HeaderHelper::INTEREST_CCNB:
        case HeaderHelper::CONTENT_OBJECT_CCNB:
          NS_FATAL_ERROR ("ccnb support is broken in this implementation");
          break;
        }

      // exception will be thrown if packet is not recognized
    }
  catch (UnknownHeaderException)
    {
      NS_ASSERT_MSG (false, "Unknown NDN header. Should not happen");
      NS_LOG_ERROR ("Unknown NDN header. Should not happen");
      return;
    }
}


} //namespace ndn
} //namespace ns3
