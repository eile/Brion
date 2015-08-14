/* Copyright (c) 2013-2015, EPFL/Blue Brain Project
 *                          Daniel Nachbaur <daniel.nachbaur@epfl.ch>
 *
 * This file is part of Brion <https://github.com/BlueBrain/Brion>
 *
 * This library is free software; you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License version 3.0 as published
 * by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for more
 * details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef BRION_TYPES
#define BRION_TYPES

#include <brion/enums.h>

#include <boost/algorithm/string.hpp>
#include <boost/multi_array.hpp>
#include <boost/shared_ptr.hpp>
#include <lunchbox/types.h>
#include <set>
#include <map>

#define VMMLIB_CUSTOM_CONFIG
#ifndef NDEBUG
#  define VMMLIB_SAFE_ACCESSORS
#endif
#undef VMMLIB_ALIGN
#define VMMLIB_ALIGN( var ) var
#pragma warning(push)
#pragma warning(disable : 4996)
#  include <vmmlib/vmmlib.hpp>
#pragma warning(pop)

/** @namespace brion Blue Brain File IO classes */
namespace brion
{

using namespace enums;
class BlueConfig;
class Circuit;
class CompartmentReport;
class CompartmentReportPlugin;
class Mesh;
class Morphology;
class SpikeReport;
class SpikeReportPlugin;
class Synapse;
class SynapseReport;
class SynapseSummary;
class Target;

typedef vmml::vector< 2, int32_t > Vector2i; //!< A two-component int32 vector
typedef vmml::vector< 3, float > Vector3f;   //!< A three-component float vector
typedef vmml::vector< 4, float > Vector4f;   //!< A four-component float vector
typedef vmml::vector< 3, double > Vector3d; //!< A three-component double vector
typedef vmml::vector< 4, double > Vector4d; //!< A four-component double vector

typedef std::vector< size_t > size_ts;
typedef std::vector< int32_t > int32_ts;
typedef std::vector< uint16_t > uint16_ts;
typedef std::vector< uint32_t > uint32_ts;
typedef std::vector< uint64_t > uint64_ts;
typedef std::vector< float > floats;
typedef std::vector< Vector2i > Vector2is;
typedef std::vector< Vector3f > Vector3fs;
typedef std::vector< Vector4f > Vector4fs;
typedef std::vector< Vector3d > Vector3ds;
typedef std::vector< Vector4d > Vector4ds;
typedef std::vector< SectionType > SectionTypes;
typedef std::vector< Target > Targets;
typedef boost::shared_ptr< int32_ts > int32_tsPtr;
typedef boost::shared_ptr< uint16_ts > uint16_tsPtr;
typedef boost::shared_ptr< uint32_ts > uint32_tsPtr;
typedef boost::shared_ptr< floats > floatsPtr;
typedef boost::shared_ptr< Vector2is > Vector2isPtr;
typedef boost::shared_ptr< Vector3fs > Vector3fsPtr;
typedef boost::shared_ptr< Vector4fs > Vector4fsPtr;
typedef boost::shared_ptr< Vector3ds > Vector3dsPtr;
typedef boost::shared_ptr< Vector4ds > Vector4dsPtr;
typedef boost::shared_ptr< SectionTypes > SectionTypesPtr;

/** Ordered set of GIDs of neurons. */
typedef std::set< uint32_t > GIDSet;

typedef GIDSet::const_iterator GIDSetCIter;
typedef GIDSet::iterator GIDSetIter;

/** The offset for the voltage per section for each neuron, uin64_t max for
 *  sections with no compartments.
 */
typedef std::vector< uint64_ts > SectionOffsets;

/** The number of compartments per section for each neuron. */
typedef std::vector< uint16_ts > CompartmentCounts;

/** Data matrix storing NeuronAttributes for each neuron. */
typedef boost::multi_array< std::string, 2 > NeuronMatrix;

/** Data matrix storing SynapseAttributes for each neuron. */
typedef boost::multi_array< float, 2 > SynapseMatrix;

/** Data matrix storing GID, numEfferent, numAfferent for each neuron. */
typedef boost::multi_array< uint32_t, 2 > SynapseSummaryMatrix;

/** Offsets within a report */
typedef std::vector< size_t > Offsets;

/** Number of elements for a list of entities */
typedef std::vector< size_t > Counts;

/** A list of Spikes events per cell gid, indexed by spikes times. */
typedef std::multimap< float, uint32_t > Spikes;

/** A spike */
typedef std::pair< float, uint32_t > Spike;

/** A value for undefined timestamps */

const float UNDEFINED_TIMESTAMP = std::numeric_limits< float >::max();
const float RESTING_VOLTAGE = -67.; //!< Resting voltage in mV
const float MINIMUM_VOLTAGE = -90.; //!< Lowest voltage after hyperpolarisation

using lunchbox::Strings;
using lunchbox::URI;
}

// if you have a type T in namespace N, the operator << for T needs to be in
// namespace N too
namespace boost
{
template< typename T >
inline std::ostream& operator << ( std::ostream& os,
                                   const boost::multi_array< T, 2 >& data )
{
    for( size_t i = 0; i < data.shape()[0]; ++i )
    {
        for( size_t j = 0; j < data.shape()[1]; ++j )
            os << data[i][j] << " ";
        os << std::endl;
    }
    return os;
}
}

namespace std
{
template< class T, class U > inline
std::ostream& operator << ( std::ostream& os, const std::pair< T, U >& pair )
{
    return os << "[ " << pair.first << ", " << pair.second << " ]";
}
}

#endif
