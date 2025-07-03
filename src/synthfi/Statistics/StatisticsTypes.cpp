#include "synthfi/Statistics/StatisticsTypes.hpp"
#include "synthfi/Util/Utils.hpp"

#include <boost/asio/awaitable.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/redirect_error.hpp>

using namespace std::chrono_literals;

namespace asio = boost::asio;

namespace Synthfi
{
namespace Statistics
{

static constexpr std::chrono::milliseconds counter_publisher_interval( ) { return 10s; };

StatisticsPublisherConfig tag_invoke( json_to_tag< StatisticsPublisherConfig >, simdjson::ondemand::value config )
{
    simdjson::ondemand::object configObject( config );

    StatisticsPublisherConfig statisticsPublisherConfig;

    statisticsPublisherConfig.influxAddress = std::string( configObject[ "influxAddress" ].get_string( ).value( ) );
    statisticsPublisherConfig.influxPort = configObject[ "influxPort" ].get_uint64( );
    statisticsPublisherConfig.influxApiToken = std::string( configObject[ "influxApiToken" ].get_string( ).value( ) );
    statisticsPublisherConfig.bucket = std::string( configObject[ "bucket" ].get_string( ).value( ) );

    return statisticsPublisherConfig;
}

template< class ValueType, class PublisherType >
Counter< ValueType, PublisherType >::Counter
(
    std::string_view measurementName,
    std::vector< std::pair< std::string, std::string > > tags,
    std::string_view fieldKey,
    ValueType fieldValue,
    boost::asio::strand< boost::asio::io_context::executor_type > * strand,
    PublisherType * publisher
)
    : _counter( fieldValue )
    , _strand( strand )
    , _publisher( publisher )
    , _publishTimer( *_strand, counter_publisher_interval( ) )
{
    _measurement.measurementName = measurementName;
    _measurement.tags = std::move( tags );
    _measurement.fields = { {  std::string( fieldKey ), InfluxDataType( fieldValue ) } };

    co_spawn( *_strand, do_publish( ), boost::asio::detached );
}

template< class ValueType, class PublisherType >
asio::awaitable< void > Counter< ValueType, PublisherType >::do_publish( )
{
    boost::system::error_code errorCode;
    while ( true && !errorCode )
    {
        co_await _publisher->publish_statistic
        (
            _measurement,
            boost::asio::use_awaitable
        );

        if ( errorCode == boost::asio::error::operation_aborted )
        {
            co_return;
        }
        else if ( errorCode )
        {
            throw SynthfiError( "Counter publish timer error" );
        }

        co_await _publishTimer.async_wait( boost::asio::redirect_error( boost::asio::use_awaitable, errorCode ) );
        _publishTimer.expires_from_now( counter_publisher_interval( ) );
    }

    co_return;
}

template< class ValueType, class PublisherType >
void Counter< ValueType, PublisherType >::increment( ValueType fieldValue )
{
    co_spawn( *_strand, do_increment( std::move( fieldValue ) ), boost::asio::detached );
}

template< class ValueType, class PublisherType >
asio::awaitable< void > Counter< ValueType, PublisherType >::do_increment( ValueType increment )
{
    _counter += increment;
    _measurement.fields.front( ).second = InfluxDataType( _counter );

    co_return;
}

template< class ValueType, class PublisherType >
void Counter< ValueType, PublisherType >::set( ValueType fieldValue )
{
    co_spawn( *_strand, do_set( std::move( fieldValue ) ), boost::asio::detached );
}

template< class ValueType, class PublisherType >
asio::awaitable< void > Counter< ValueType, PublisherType >::do_set( ValueType fieldValue )
{
    BOOST_ASSERT_MSG( fieldValue > _counter, "Counters should monotonically increase" );

    _counter = fieldValue;
    _measurement.fields.front( ).second = InfluxDataType( _counter );

    co_return;
}

} // namespace Statistics
} // namespace Synthfi

#include "synthfi/Statistics/StatisticsPublisher/StatisticsPublisher.hpp"

namespace Synthfi
{
namespace Statistics
{

template class Counter< double, StatisticsPublisher >;
template class Counter< uint64_t, StatisticsPublisher>;
template class Counter< int64_t, StatisticsPublisher >;
template class Counter< bool, StatisticsPublisher >;

} // namespace Statistics
} // namespace Synthfi
