# Copyright (c) 2015, EPFL/Blue Brain Project
#                     Stefan.Eilemann@epfl.ch
#
# This file is part of Brion <https://github.com/BlueBrain/Brion>
#

set(SPIKECONVERTER_HEADERS)
set(SPIKECONVERTER_SOURCES spikeConverter.cpp)
set(SPIKECONVERTER_LINK_LIBRARIES Brion)

common_application(spikeConverter)

set(MORPHOLOGYCONVERTER_HEADERS)
set(MORPHOLOGYCONVERTER_SOURCES morphologyConverter.cpp)
set(MORPHOLOGYCONVERTER_LINK_LIBRARIES Brion ${Boost_PROGRAM_OPTIONS_LIBRARY})

common_application(morphologyConverter)

set(COMPARTMENTCONVERTER_HEADERS)
set(COMPARTMENTCONVERTER_SOURCES compartmentConverter.cpp)
set(COMPARTMENTCONVERTER_LINK_LIBRARIES Brion ${Boost_PROGRAM_OPTIONS_LIBRARY})
if(BBPTESTDATA_FOUND)
  list(APPEND COMPARTMENTCONVERTER_LINK_LIBRARIES BBPTestData)
endif()
if(MPI_FOUND)
  list(APPEND COMPARTMENTCONVERTER_LINK_LIBRARIES ${MPI_LIBRARIES})
endif()

common_application(compartmentConverter)
