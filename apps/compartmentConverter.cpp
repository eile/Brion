/* Copyright (c) 2015, EPFL/Blue Brain Project
 *                     Stefan Eilemann <stefan.eilemann@epfl.ch>
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

#include <brion/brion.h>
#include <lunchbox/clock.h>
#include <lunchbox/mpi.h>
#include <lunchbox/sleep.h>
#ifdef BRION_USE_BBPTESTDATA
#  include <BBP/TestDatasets.h>
#endif
#ifdef LUNCHBOX_USE_MPI
#  include <mpi.h>
#endif

#include <boost/foreach.hpp>
#include <boost/program_options.hpp>
#include <boost/progress.hpp>

namespace po = boost::program_options;
using boost::lexical_cast;

#define REQUIRE_EQUAL( a, b )                           \
    if( (a) != (b) )                                    \
    {                                                   \
        std::cerr << #a << " != " << #b << std::endl;   \
        ::exit( EXIT_FAILURE );                         \
    }

#define REQUIRE( a )                                    \
    if( !(a) )                                          \
    {                                                   \
        std::cerr << #a << " failed" << std::endl;      \
        ::exit( EXIT_FAILURE );                         \
    }

template< class T > void requireEqualCollections( const T& a, const T& b )
{
    typename T::const_iterator i = a.begin();
    typename T::const_iterator j = b.begin();
    while( i != a.end() && j != b.end( ))
    {
        REQUIRE_EQUAL( *i, *j );
        ++i;
        ++j;
    }
    REQUIRE_EQUAL( i, a.end( ));
    REQUIRE_EQUAL( j, b.end( ));
}

/**
 * Convert a compartment report to an HDF5 report.
 *
 * @param argc number of arguments
 * @param argv argument list, use -h for help
 * @return EXIT_SUCCESS or EXIT_FAILURE
 */
int main( const int argc, char** argv )
{
    lunchbox::MPI mpi( argc, argv );

    po::options_description options( "Options" );

    options.add_options()
        ( "help,h", "Produce help message" )
        ( "version,v", "Show program name/version banner and exit" )
#ifdef BRION_USE_BBPTESTDATA
        ( "input,i", po::value< std::string >(), "Input report URI" )
#else
        ( "input,i", po::value< std::string >()->required(), "Input report URI")
#endif
        ( "output,o", po::value< std::string >(), "Output report URI" )
        ( "maxFrames,m", po::value< size_t >(),
          "Convert at most the given number of frames" )
        ( "compare,c", "Compare written report with input" )
        ( "dump,d", "Dump input report information (no output conversion)" )
        ;
    po::variables_map vm;

    try
    {
        po::store( po::parse_command_line( argc, argv, options ), vm );
        po::notify( vm );
    }
    catch( const boost::program_options::error& e )
    {
        std::cerr << "Command line parse error: " << e.what() << std::endl
                  << options << std::endl;
        return EXIT_FAILURE;
    }

    if( vm.count( "help" ))
    {
        std::cout << options << std::endl;
        return EXIT_SUCCESS;
    }

    if( vm.count( "version" ))
    {
        std::cout << "Brion compartment report converter "
                  << brion::Version::getString() << std::endl;
        return EXIT_SUCCESS;
    }

    std::string input;
    if( vm.count( "input" ))
        input = vm["input"].as< std::string >();

    const size_t maxFrames = vm.count( "maxFrames" ) == 1 ?
        vm["maxFrames"].as< size_t >() : std::numeric_limits< size_t >::max();

#ifdef BRION_USE_BBPTESTDATA
    if( input.empty( ))
        input = std::string( BBP_TESTDATA ) +
                "/local/simulations/may17_2011/Control/allCompartments.bbp";
#endif
    if( input.empty( ))
    {
        std::cout << "Missing input URI" << std::endl;
        return EXIT_FAILURE;
    }

    lunchbox::URI inURI( input );
    lunchbox::Clock clock;
    lunchbox::Clock totalClock;
    brion::CompartmentReport in( inURI, brion::MODE_READ );
    float loadTime = clock.getTimef();

    if( vm.count( "dump" ))
    {
        std::cout << "Compartment report " << inURI << ":" << std::endl
                  << "  Time: " << in.getStartTime() << ".."
                  << in.getEndTime() << " / " << in.getTimestep()
                  << " " << in.getTimeUnit() << std::endl
                  << "  " << in.getGIDs().size() << " neurons" << std::endl
                  << "  " << in.getFrameSize() << " compartments"
                  << std::endl;
        return EXIT_SUCCESS;
    }

    const float start = in.getStartTime();
    const float step = in.getTimestep();
    float end = in.getEndTime();
    const float maxEnd = start + maxFrames * step;
    end = std::min( end, maxEnd );
    const brion::CompartmentCounts& counts = in.getCompartmentCounts();
    const brion::GIDSet& gids = in.getGIDs();
    const int rank = mpi.getRank();
    const int nRanks = mpi.getSize();

    const lunchbox::URI outURI( vm.count( "output" ) == 1 ?
                                vm[ "output" ].as< std::string >() :
                                std::string( "out.h5" ));

    clock.reset();
    brion::CompartmentReport to( outURI, brion::MODE_OVERWRITE );
    to.writeHeader( start, end, step, in.getDataUnit(), in.getTimeUnit( ));

    {
        const float range = float( gids.size( )) / float( nRanks );
        const size_t startGID = rank * range;
        const size_t endGID = ( rank + 1 ) * range;
        size_t index = 0;

        BOOST_FOREACH( const uint32_t gid, gids )
        {
            if( index < startGID || index >= endGID )
                to.addGID( gid );
            else
                to.writeCompartments( gid, counts[ index ] );
            ++index;
        }
    }
    float writeTime = clock.getTimef();

    const size_t nFrames = (end - start) / step;
    boost::progress_display progress( nFrames );

#ifdef LUNCHBOX_USE_MPI
    // Prequeue work for ranks 1..n
    const int preQueue = std::max( 2, int( nFrames>>9 ) );
    const size_t startFrame = (nRanks-1) * preQueue;
    if( startFrame > nFrames )
    {
        std::cerr << "More MPI processes than work" << std::endl;
        return EXIT_FAILURE;
    }

    typedef std::map< int, MPI_Request > Requests;
    Requests requests;
    enum RequestTag
    {
        TAG_FRAME,
        TAG_FRAME_DONE
    };

    if( rank == 0 )
        for( int i = 1; i < nRanks; ++i )
            for( int j = 0; j < preQueue; ++j )
            {
                const int frame = (i-1) * preQueue + j;
                MPI_Isend( &frame, 1, MPI_INT, i, TAG_FRAME, MPI_COMM_WORLD,
                           &requests[frame] );
            }
    MPI_Request clientRequest;
    size_t framesDone = 0;
#else
    const size_t startFrame = 0;
#endif

    for( size_t i = startFrame; i < nFrames; ++i )
    {
#ifdef LUNCHBOX_USE_MPI
        int frame = i;
        if( rank == 0 )
        {
            if( nRanks > 1 )
            {
                int data = 0;
                MPI_Status status;
                MPI_Iprobe( MPI_ANY_SOURCE, TAG_FRAME_DONE, MPI_COMM_WORLD,
                            &data, &status );

                if( data )
                {
                    MPI_Recv( &frame, 1, MPI_INT, MPI_ANY_SOURCE,
                              TAG_FRAME_DONE, MPI_COMM_WORLD, &status );
                    MPI_Wait( &requests[frame], &status );
                    requests.erase( frame );

                    MPI_Isend( &i, 1, MPI_INT, status.MPI_SOURCE, TAG_FRAME,
                               MPI_COMM_WORLD, &requests[i] );
                    ++progress;
                    continue;
                }
            }
            // No worker idle, do it ourselves
        }
        else
        {
            // get frame to process
            MPI_Status status;
            if( i != startFrame )
                MPI_Wait( &clientRequest, &status );
            MPI_Recv( &frame, 1, MPI_INT, 0, TAG_FRAME, MPI_COMM_WORLD,
                      &status );
        }
        if( frame < 0 ) // done
            break;

        ++framesDone;
#endif

        const float t = start + frame * step;
        clock.reset();
        brion::floatsPtr data = in.loadFrame( t );
        loadTime += clock.getTimef();

        const brion::floats& voltages = *data.get();
        const brion::SectionOffsets& offsets = in.getOffsets();

        size_t index = 0;
        clock.reset();
        BOOST_FOREACH( const uint32_t gid, gids )
        {
            brion::floats cellVoltages;
            cellVoltages.reserve( in.getNumCompartments( index ));

            for( size_t j = 0; j < offsets[index].size(); ++j )
                for( size_t k = 0; k < counts[index][j]; ++k )
                    cellVoltages.push_back( voltages[ offsets[index][j]+k] );

            to.writeFrame( gid, cellVoltages, t );
            ++index;
        }
        writeTime += clock.getTimef();
        if( rank == 0 )
            ++progress;
#ifdef LUNCHBOX_USE_MPI
        else
        {
            MPI_Isend( &frame, 1, MPI_INT, 0, TAG_FRAME_DONE,
                       MPI_COMM_WORLD, &clientRequest );
        }
#endif
    }

#ifdef LUNCHBOX_USE_MPI
    // send stop frame
    if( rank == 0 )
        for( int i = 1; i < nRanks; ++i )
        {
            const int frame = -i;
            MPI_Isend( &frame, 1, MPI_INT, i, TAG_FRAME, MPI_COMM_WORLD,
                       &requests[frame] );
        }
#endif

    clock.reset();
    to.flush();
    writeTime += clock.resetTimef();
#ifdef LUNCHBOX_USE_MPI // Measure overall conversion time correctly
    if( rank == 0 )
    {
        typedef std::pair< const int, MPI_Request > RequestPair;
        BOOST_FOREACH( RequestPair& i, requests )
        {
            MPI_Status status;
            MPI_Wait( &i.second, &status );
        }
    }
    else
    {
        MPI_Status status;
        MPI_Wait( &clientRequest, &status );
    }

    MPI_Barrier( MPI_COMM_WORLD );
#endif
    const float idleTime = clock.getTimef();
    const float totalTime = totalClock.getTimef();

    lunchbox::sleep( rank ); // deinterlace print
    std::cout << "Converted " << inURI << " -> " << outURI << " in "
              << int( totalTime ) << "ms (r " << int( loadTime ) << " w "
              << int( writeTime ) << " i " << int( idleTime ) << ")"
#ifdef LUNCHBOX_USE_MPI
              << " proc " << rank << "/" << nRanks << " done " << framesDone
              << " frames"
#endif
              << std::endl;

    if( vm.count( "compare" ))
    {
        progress.restart( nFrames );
        brion::CompartmentReport result( outURI, brion::MODE_READ );

        REQUIRE_EQUAL( in.getStartTime(), result.getStartTime( ));
        REQUIRE_EQUAL( in.getEndTime(), result.getEndTime( ));
        REQUIRE_EQUAL( in.getTimestep(), result.getTimestep( ));
        REQUIRE_EQUAL( in.getFrameSize(), result.getFrameSize( ));
        requireEqualCollections( gids,  result.getGIDs( ));
        REQUIRE_EQUAL( in.getDataUnit(), result.getDataUnit( ));
        REQUIRE_EQUAL( in.getTimeUnit(), result.getTimeUnit( ));
        REQUIRE( !in.getDataUnit().empty( ));
        REQUIRE( !in.getTimeUnit().empty( ));

        const brion::SectionOffsets& offsets1 = in.getOffsets();
        const brion::SectionOffsets& offsets2 = result.getOffsets();
        const brion::CompartmentCounts& counts1 = in.getCompartmentCounts();
        const brion::CompartmentCounts& counts2 = result.getCompartmentCounts();

        REQUIRE_EQUAL( offsets1.size(), offsets2.size( ));
        REQUIRE_EQUAL( counts1.size(), counts2.size( ));

        for( size_t i = 0; i < offsets1.size(); ++i )
        {
            requireEqualCollections( offsets1[i], offsets2[i] );

            for( size_t j = 0; j < offsets1[i].size(); ++j )
                REQUIRE( offsets1[i][j] < in.getFrameSize() ||
                     offsets1[i][j] == std::numeric_limits< uint64_t >::max( ));
        }

        for( float t = start; t < end; t += step )
        {
            brion::floatsPtr frame1 = in.loadFrame( t );
            brion::floatsPtr frame2 = result.loadFrame( t );

            REQUIRE( frame1 );
            REQUIRE( frame2 );

            for( size_t i = 0; i < in.getFrameSize(); ++i )
                REQUIRE_EQUAL( (*frame1)[i], (*frame2)[i] );
            ++progress;
        }
    }

    return EXIT_SUCCESS;
}
