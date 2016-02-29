Redesign of the SpikeReport and Spikes container
============

The current design of the SpikeReport has several flaws that need to be
addressed:
* It doesn't allow partial loading for file based reports.
* It exposes too many details of the stream case which can probably be hidden.
* The container class is a std::multimap while it could simply be a sorted array
  because no insertion is needed.
This specification proposes a new API to address these defficiencies as well
as include new requirements that need to be fulfilled.

## Requirements

* Provide a container for spikes which is sorted by timestamp, storage
  efficient, and random accessible at the <sup>n</sup>th position in amortized
  constant time. This is basically a std::vector, but it will be read only
  access.

* Allow partial data loading for file access.

* Allow the selection of a subset of the cells present in the data source to be
  reported. In stream-based reports this information should be made available
  to the sender to allow filtering at the source. In the ZeroEQ implementation,
  the same channel needed for communicating the desired target could be used
  for the handshaking required to ensure that the first spikes are not missed.

* As few stream related details as possible should leak. This means that
  it should be easy to write client code that works the same for both file
  based and stream based reports without special conditions for stream based
  reports.

* For stream reports the user will be able to:
 * Retrieve all the data until the end of stream in a simple loop without doing
   any active wait (blocking calls are allowed).
 * Know if the end of stream has been reached.
 * Get all the data received so far without locking.
 * Get information about the latest timestamp reached by the source in the
   sender side even when there are no spikes at all (currently if the spike
   report is completely empty it's not possible to make any progress until the
   producer reaches the end of the report).

* When data for a time interval is requested, the time intervals are always
  closed on the left and open on the right.

## API

The classes presented below are not thread-safe except for const access.

### Spikes container

\code{.cpp}
Spikes
{
public:
    Spikes();
    ~Spikes();
    /*
     * Provided to allow Boost.Python to use return by value policies.
     * Constant complexity.
     */
    Spikes( const Spikes& rhs );
    /*
     * Provided to allow Boost.Python to use return by value policies.
     * Constant complexity.
     */
    Spikes& operator=( const Spikes& rhs );

    class const_iterator; // implementation defined

    const_iterator begin() const;
    const_iterator end() const;
    bool empty() const;
    size_t size() const;
    /* Get the spike at the nth position. No range check is performed */
    const std::pair< float, uint32_t >& operator[]( size_t index ) const;

    /*
     * Get the start of the time window in milliseconds.
     *
     * UNDEFINED_TIMESTAMP if the container cannot be referred to any time
     * window and it is empty. Otherwise, a time smaller or equal than the
     * smallest spike time in the container.
     *
     * @see SpikesReport::getSpikes.
     */
    float getStartTime() const;

    /*
     * Get the end of the time window in milliseconds.
     *
     * UNDEFINED_TIMESTAMP if the container cannot be referred to any time
     * window and it is empty. Otherwise, a time greater than the highest
     * spike time in the container.
     */
    float getEndTime() const;
}
\endcode

With this API, it's relatively easy to provide a Boost.Python wrapping of the
container inside a numpy array.

### SpikeReport

\code{.cpp}
SpikeReport : public boost::noncopyable
{
public:
    SpikeReport( const URI& uri, int mode );

    /*
     * Open a report in read mode with a subset selections.
     *
     * @param uri
     * @param subset The set of gids to be reported.
     */
    SpikeReport( const URI& uri, uint32_ts subset );

    ~SpikeReport();

    const URI& getURI();

    /*
     * Return a container with all the spikes contained in a time interval.
     *
     * @param startTime a value smaller than endTime or UNDEFINED_TIMESTAMP.
     *
     *        - If UNDEFINED_TIMESTAMP, the left edge of the interval will be
     *          considered -infinity. Spikes::getStartTime in the output
     *          will return the smallest spike time inside the container if
     *          not empty, otherwise it will return UNDEFINED_TIMESTAMP.
     *
     *        - For any other value, this is the smallest timestamp to be
     *          reported and the output Spikes container will return this time
     *          from Spikes::getStartTime.
     *
     * @param endTime a value greater than startTime or UNDEFINED_TIMESTAMP.
     *
     *        - If UNDEFINED_TIMESTAMP, this function will return all
     *          the spikes available with timestamp >= startTime.
     *          In file based reports, this is all the data from startTime until
     *          the end.
     *          In stream based reports:
     *          - If startTime is UNDEFINED_TIMESTAMP this call will return all
     *            the available data without blocking at all. If there are no
     *            data the output container will be empty and the both
     *            Spikes::getEndTime and Spikes::getStartTime will return
     *            UNDEFINED_TIMESTAMP.
     *          - For any other value, the call will block until an event with
     *            t > startTime arrives, the stream is closed or the timeout
     *            goes off, whatever happens first.
     *
     *        - For any other value, return all the spikes in the time interval
     *          [t, t') where t = startTime and t' > t.
     *          This will be the value returned by Spikes::getEndTime() on the
     *          result.
     *
     * In stream based reports calling this function will update the report end
     * time.
     *
     * In the case the timeout goes off this function will always return a
     * consistent slice of the spike report. This means that the output
     * container will not include any spikes with timestamp t unless all the
     * possible spikes with the same timestamp are also inside the container.
     *
     * The implementation will try it's best to not duplicate the data
     * requested if it was already loaded by a previous call, but no guarantees
     * are made. The recommended way of accessing the data is to always make
     * the startTime match the endTime of a previous call.
     *
     * @throws std::runtime_eror is the report was opened in write mode.
     */
    Spikes getSpikes( float startTime, float endTime,
                      const uint32_t timeout = LB_TIMEOUT_INDEFINITE );

    /*
     * For reports open in write mode, write the given spikes to the output.
     *
     * @throws std::runtime_eror if the report was opened in read mode.
     * @throws std::runtime_eror when writing to a stream if the spikes are not
     *         sorted or any timestamp is <= to the highest timestamp written
     *         by previous calls. This is required to enforce the
     *         post-conditions of getSpikes in the receiving end.
     */
    void writeSpikes( const const std::vector< float, uint32_t >& spikes );

    /* Overload of the method above provided for convenience */
    void writeSpikes( const Spikes& spikes );

    /*
     * Return the start time of the report if known, UNDEFINED_TIMESTAMP
     * otherwise.
     *
     * In file based reports it's the time of the first spike or undefined if
     * the report is empty.
     * In stream based reports it's always 0.
     */
    float getStartTime() const;

    /*
     * Return the end time of the report if known, UNDEFINED_TIMESTAMP
     * otherwise.
     *
     * In file based reports the end time is the timestamp of the last spike
     * plus an epsilon, or undefined if the report is empty or it cannot be
     * be decided without loading the whole data.
     * In stream based reports it's always undefined until the report is
     * closed. When the report is closed it will be the latest timestamp
     * reported by the sender or the time of the last spike + epsilon, whichever
     * is larger; or 0 if no information was received.
     *
     * To guarantee const-correctness the value returned by this function can
     * only change when getSpikes is called.
     */
    float getEndTime() const;

    /*
     * Closes the report.
     *
     * Only meaningful for stream based reports.
     * For reports opened in write mode it finishes the reporting. For reports
     * opened in read mode it disconnects from the source, any call waiting
     * in getSpikes will be unblocked and getEndTime will return the last known
     * timestamp.
     *
     * This function is thread-safe with getSpikes as it ensures that any
     * thread waiting there with an infinite timeout will eventually return.
     *
     * Implicitly called by the destructor, (but calling the destructor and
     * getSpikes at the same time is not safe!). Calling any other function
     * after the report has been closed has undefined behavior.
     */
    void close();

    /*
     * Clears all internal caching.
     *
     * Any Spikes container returned by this class will remain valid as
     * expected.
     *
     * The implementation is allowed to retain as much memory as needed until
     * all pre-existing Spikes containers are destroyed to guarantee this.
     */
    void clear();
}
\endcode

## Examples

### Copying a spike report from one URL to another

\code{.cpp}
SpikeReport source( "url1", brion::MODE_READ )
SpikeReport destination( "url2", brion::MODE_WRITE )

float last = 0;
// In file based reports this will iterates just once.
while( source.getEndTime() == brion::UNDEFINED_TIMESTAMP )
{
    const Spikes spikes = source.getSpikes( last, brion::UNDEFINED_TIMESTAMP );
    last = spikes.getEndTime();
    destitation.write( spikes );
}
\endcode

### Copying a spike report from one URL to another reading a maximum time window

\code{.cpp}
SpikeReport source( "url1", brion::MODE_READ )
SpikeReport destination( "url2", brion::MODE_WRITE )

float current = source.getStartTime();
float width = 10;
// If we have an empty file report, current == UNDEFINED_TIMESTAMP, and then
// the loop is not entered.
for( ; current + width < source.getEndTime( ); current += width )
{
    const Spikes spikes = source.getSpikes( current, current + width );
    last = spikes.getEndTime();
    destintation.write( spikes );
}
// Reading the last slice. This is guaranteed to return immediately in stream
// sources because the EOS has been reached.
destintation.write( source.getSpikes( current, report.getEndTime( )));
\endcode

## Implementation

TODO

brain::SpikeReport

Where does the pluging interface go? brion or brain.

brion::SpikeFile?
brion::SpikeStream?

## Issues

### 1: Why Spikes is a read-only class?

_Resolved: Yes_

By making it read-only, the implementation can share the same storage between
multiple containers that refer to the same source but with different ranges.
See also issues #1 and #2. For writing spikes the user can use a std::vector,
whose interface doesn't differ very much from a potential write interface in
Spikes.

### 2: Why the access time of the spikes container is amortized constant time instead of constant time?

_Resolved: Yes_

The rationale for amortized time is that it allows the implementation to use
data structures different from a single flat memory buffer per container. With
special data structures it's possible to implement zero-copy optimizations,
smarter internal storage and even internal threading while still ensuring
const-correctness. For example, imagine we have some client code that uses
OpenMP to speed up the processing of a report:

\code{.cpp}
#pragma omp parallel for shared(report)
for (int i = 0; i != slices; ++i)
{
    auto slice = getTimeSlice(i);
    const brion::Spikes& spikes = report.getSpikes(slice.first, slice.second);
    ... // Do something
}
\endcode

With strict constant time, the previous code cannot return any reference to
shared data from getSpikes because it may be relocated by subsequent calls from
other threads, hence the only sane alternative is to return a copy of the data.

By relaxing the time complexity requirements, it would be possible to use a
lunchbox::lfVector for example.

### 3: Can getSpikes return _const Spikes&_?

_Resolved: Yes_

No. That will imply that the container needs to be stored inside SpikeReport.
As a side-effect this requires Spikes to be copyable and the copies need to be
efficient, but thanks to issues #1 and #2 the implementation can provide
cheap copies by sharing.

### 4: Do you need a method to erase portions of the report?

_Resolved: Partially_

We don't have a use case for this yet, but it looks necessary for out-of-core
processing of very large reports. The mail goal for this method in the original
class was to clear the internal caching. Clearing the report can be very
complex without requiring duplication of data. The compromise solution is to
provide a method to clear the cache enterily. The implementation can easily
keep Spike containers returned by getSpikes valid with smart pointers.

### 5: For stream based reports should we block on getSpikes() or in the iterator ++ operator.

_Resolved: Yes_

It seems easier for the implementation to block on getSpikes than on
the operator increment. Blocking in the container requires synchronization
primitives to ensure const-correctness that wouldn't be otherwise needed.
Locking at the iterator increment requires having a separate function to
indicate the timeout, which is awkward, or not having a timeout at all for
an equivalent to waitUntil and also add the current functions to know until
which time is it possible to advance without blocking.

### 6: Why getStartTime() retuns 0 for stream based reports, but it can be undefined for empty file reports?

_Resolved: No_

Seems reasonable, but it's open to discussion.

### 7: Why getEndTime() retuns 0 for stream based reports closed before having received any data, but it can be undefined for empty file reports?

_Resolved: TBD_

Making the report a defined value >= start time is a good way way to identify
when the end of the report has been reached without having to add any extra
functions. A loop that reads pieces of a report until the end has been reached
will loop the same for files and streams with this approach. The only caveat
is that empty files need to be detected first by checking that both start and
end time are undefined.

### 8: Why Spikes::getEndTime() returns a time greater than the largest spike time?

_Resolved: Yes_

To ensure consistency with the specification of the time interval from
SpikeReport::getSpikes, which is open on the right. The time of the last
spike is still easy to recover because it is the last element in the container.

### 9: Do we need reverse interators in the Spikes container.

_Resolved: Yes_

Not for the moment, can be added later.

### 10: What happens with the old brion::SpikeReport

_Resolved: No_.

### 11: Why SpikeReport::getEndTime() returns last spike + epsilon for files?

_Resolved: Yes_

Because otherwise r.getSpikes( r.getStartTime(), r.getEndTime( )) would not
return the full data.

### 12: What happens with file based report where the spikes are sorted by time?

_Resolved: Yes_

Without loading the whole file it is not possible to return a correct output
from getSpikes, so the file will be loaded at construction.
