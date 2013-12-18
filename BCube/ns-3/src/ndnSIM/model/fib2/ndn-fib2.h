/* -*- Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil -*- */
/*
 * Copyright (c) 2011-2013 University of California, Los Angeles
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
 * Author: Alexander Afanasyev <alexander.afanasyev@ucla.edu>
 */

#ifndef _NDN_FIB2_H_
#define	_NDN_FIB2_H_

#include "ns3/simple-ref-count.h"
#include "ns3/node.h"

#include "ns3/ndn-fib2-entry.h"

namespace ns3 {
namespace ndn {

class Interest;
typedef Interest InterestHeader;

/**
 * \ingroup ndn
 * \brief Class implementing FIB functionality
 */
class Fib2 : public Object
{
public:
  /**
   * \brief Interface ID
   *
   * \return interface ID
   */
  static TypeId GetTypeId ();
  /**
   * @brief Default constructor
   */
  Fib2 () {}
  
  /**
   * @brief Virtual destructor
   */
  virtual ~Fib2 () { };
  
  /**
   * \brief Perform longest prefix match
   *
   * \todo Implement exclude filters
   *
   * \param interest Interest packet header
   * \returns If entry found a valid iterator (Ptr<fib::Entry>) will be returned, otherwise End () (==0)
   */
  virtual Ptr<fib2::Entry>
  LongestPrefixMatch (const Interest &interest) = 0;

  /**
   * @brief Get FIB entry for the prefix (exact match)
   *
   * @param prefix Name for FIB entry
   * @returns If entry is found, a valid iterator (Ptr<fib::Entry>) will be returned. Otherwise End () (==0)
   */
  virtual Ptr<fib2::Entry>
  Find (const Name &prefix) = 0;
  
  /**
   * \brief Add or update FIB entry
   *
   * If the entry exists, metric will be updated. Otherwise, new entry will be created
   *
   * Entries in FIB never deleted. They can be invalidated with metric==NETWORK_UNREACHABLE
   *
   * @param name	Prefix
   * @param face	Forwarding face
   * @param metric	Routing metric
   */
  virtual Ptr<fib2::Entry>
  Add (const Name &prefix, Ptr<Face> face, int32_t metric) = 0;

  /**
   * \brief Add or update FIB entry using smart pointer to prefix
   *
   * If the entry exists, metric will be updated. Otherwise, new entry will be created
   *
   * Entries in FIB never deleted. They can be invalidated with metric==NETWORK_UNREACHABLE
   *
   * @param name	Smart pointer to prefix
   * @param face	Forwarding face
   * @param metric	Routing metric
   */
  virtual Ptr<fib2::Entry>
  Add (const Ptr<const Name> &prefix, Ptr<Face> face, int32_t metric) = 0;

  /**
   * @brief Remove FIB entry
   *
   * ! ATTENTION ! Use with caution.  All PIT entries referencing the corresponding FIB entry will become invalid.
   * So, simulation may crash.
   *
   * @param name	Smart pointer to prefix
   */
  virtual void
  Remove (const Ptr<const Name> &prefix) = 0;

  // /**
  //  * @brief Invalidate FIB entry ("Safe" version of Remove)
  //  *
  //  * All faces for the entry will be assigned maximum routing metric and NDN_FIB_RED status   
  //  * @param name	Smart pointer to prefix
  //  */
  // virtual void
  // Invalidate (const Ptr<const Name> &prefix) = 0;

  /**
   * @brief Invalidate all FIB entries
   */
  virtual void
  InvalidateAll () = 0;
  
  /**
   * @brief Remove all references to a face from FIB.  If for some enty that face was the only element,
   * this FIB entry will be removed.
   */
  virtual void
  RemoveFromAll (Ptr<Face> face) = 0;

  /**
   * @brief Print out entries in FIB
   */
  virtual void
  Print (std::ostream &os) const = 0;

  /**
   * @brief Get number of entries in FIB
   */
  virtual uint32_t
  GetSize () const = 0;

  /**
   * @brief Return first element of FIB (no order guaranteed)
   */
  virtual Ptr<const fib2::Entry>
  Begin () const = 0;

  /**
   * @brief Return first element of FIB (no order guaranteed)
   */
  virtual Ptr<fib2::Entry>
  Begin () = 0;  

  /**
   * @brief Return item next after last (no order guaranteed)
   */
  virtual Ptr<const fib2::Entry>
  End () const = 0;

  /**
   * @brief Return item next after last (no order guaranteed)
   */
  virtual Ptr<fib2::Entry>
  End () = 0;

  /**
   * @brief Advance the iterator
   */
  virtual Ptr<const fib2::Entry>
  Next (Ptr<const fib2::Entry>) const = 0;

  /**
   * @brief Advance the iterator
   */
  virtual Ptr<fib2::Entry>
  Next (Ptr<fib2::Entry>) = 0;

  ////////////////////////////////////////////////////////////////////////////
  ////////////////////////////////////////////////////////////////////////////
  ////////////////////////////////////////////////////////////////////////////
  
  /**
   * @brief Static call to cheat python bindings
   */
  static inline Ptr<Fib2>
  GetFib2 (Ptr<Object> node);

  ////////////////////////////////////////////////////////////////////////////
  ////////////////////////////////////////////////////////////////////////////
  ////////////////////////////////////////////////////////////////////////////
  
private:
  Fib2 (const Fib2&) {} ; ///< \brief copy constructor is disabled
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

std::ostream& operator<< (std::ostream& os, const Fib2 &fib2);

Ptr<Fib2>
Fib2::GetFib2 (Ptr<Object> node)
{
  return node->GetObject<Fib2> ();
}

} // namespace ndn
} // namespace ns3

#endif // _NDN_FIB_H_