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

#ifndef BRION_SYNAPSEREPORT
#define BRION_SYNAPSEREPORT

#include <brion/types.h>
#include <boost/noncopyable.hpp>

namespace brion
{

namespace detail { class SynapseReport; }

/** Read & write access to a SynapseReport.
 *
 * The supported types are HDF5 (r) reports.
 *
 * Following RAII, this class is ready to use after the creation and will ensure
 * release of resources upon destruction.
 */
class SynapseReport : public boost::noncopyable
{
public:
    /** Open given URI to a synapse report for reading and/or writing.
     *
     * @param uri URI to synapse report. The report type is deduced from
     *        here.
     * @param mode the brion::AccessMode bitmask
     * @param gids the neurons of interest in READ_MODE
     * @throw std::runtime_error if synapse report could be opened for read
     *                           or write, cannot be overwritten or it is not
     *                           valid
     * @version 1.6
     */
    SynapseReport( const URI& uri, const int mode,
                   const GIDSet& gids = GIDSet( ));

    /** Close and destruct synapse report. @version 1.6 */
    ~SynapseReport();

    /** @name Read API */
    //@{
    /** @return the current considered GIDs. @version 1.6 */
    const GIDSet& getGIDs() const;

    /** Get the current offset for all synapse data of each neuron.
     *
     * For instance, getOffsets()[15] retrieves the lookup index in the
     * report array for neuron with index 15 in the GIDSet.
     *
     * @return the offset for each neuron
     * @version 1.6
     */
    const Offsets& getOffsets() const;

    /** Get the number of synapses for each neuron in the GID set.
     *
     * @return the synapse counts for each section for each neuron
     * @version 1.6
     */
    const Counts& getCounts() const;

    /** @return the current start time of the report. @version 1.6 */
    float getStartTime() const;

    /** @return the current end time of the report. @version 1.6 */
    float getEndTime() const;

    /** @return the sampling time interval of the report. @version 1.6 */
    float getTimestep() const;

    /** @return the data unit of the report. @version 1.6 */
    const std::string& getDataUnit() const;

    /** @return the time unit of the report. @version 1.6 */
    const std::string& getTimeUnit() const;

    /**
     * @return the size of a loaded report frame, i.e., sum(getCounts()).
     * @version 1.6
     */
    size_t getFrameSize() const;

    /** Load report values at the given time stamp.
     *
     * @param timestamp the time stamp of interest
     * @return the report values if found at timestamp, 0 otherwise
     * @version 1.6
     */
    floatsPtr loadFrame( const float timestamp ) const;

    /** Set the size of the stream buffer for loaded frames.
     *
     * Configures the number of simulation frame buffers for stream readers.
     * When the consumer is slower than the producer, the producer will block
     * once these buffers are exhausted, otherwise the consumer will block on
     * data availability. A minimum of 1 frame is buffered.
     *
     * @param size the new size of the frame buffer.
     * @version 1.6
     */
    void setBufferSize( const size_t size );

    /** @return the number of the simulation frame buffers. @version 1.6 */
    size_t getBufferSize() const;

    /** Clears all buffered frames to free memory. @version 1.6 */
    void clearBuffer();
    //@}

private:
    detail::SynapseReport* _impl;
};

}

#endif
