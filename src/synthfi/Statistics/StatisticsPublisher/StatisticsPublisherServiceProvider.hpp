#pragma once

#include "synthfi/Statistics/StatisticsTypes.hpp"

#include <boost/asio/async_result.hpp>
#include <boost/asio/io_context.hpp>

namespace Synthfi
{
namespace Statistics
{

template< class Service >
class StatisticsPublisherServiceProvider
{
public:
    StatisticsPublisherServiceProvider( Service & service, std::string_view publisherName )
        : _service( &service )
        , _publisherName( publisherName )
        , _publishCount( "statistics_write_count", { }, "count", 0UL, &_service->strand( ), this )
    { }

    StatisticsPublisherServiceProvider( boost::asio::io_context & ioContext, const StatisticsPublisherConfig & config, std::string_view publisherName )
        : _service( &boost::asio::make_service< Service >( ioContext, config ) )
        , _publisherName( publisherName )
        , _publishCount( "statistics_write_count", { }, "count", 0UL, &_service->strand( ), this )
    { }

    // Publishs a single measurement with a unique timestamp.
    template< boost::asio::completion_token_for< void( std::exception_ptr ) > CompletionToken = boost::asio::use_awaitable_t< > >
    auto publish_statistic
    (
        InfluxMeasurement measurement,
        CompletionToken && token = { }
    )
    {
        // Tag publisher name.
        measurement.tags.emplace_back( "publisherName", _publisherName );

        return boost::asio::async_initiate< CompletionToken, void( std::exception_ptr ) >
        (
            [ this, measurement = std::move( measurement ) ]< class Handler >( Handler && self )
            {
                _service->impl( ).publish_statistic
                (
                    std::move( measurement ),
                    [ self = std::make_shared< Handler >( std::forward< Handler >( self ) ) ]( std::exception_ptr ex )
                    {
                        ( *self )( ex );
                    }
                );
                this->_publishCount.increment( 1 );
            },
            token
        );
    }

    // Publishs a group of measurements sharing a unique timestamp.
    template< boost::asio::completion_token_for< void( std::exception_ptr ) > CompletionToken = boost::asio::use_awaitable_t< > >
    auto publish_statistics
    (
        std::vector< InfluxMeasurement > measurements,
        CompletionToken && token = { }
    )
    { 
        for ( auto & measurement : measurements )
        {
            // Tag publisher name.
            measurement.tags.emplace_back( "publisherName", _publisherName );
        }

        return boost::asio::async_initiate< CompletionToken, void( std::exception_ptr ) >
        (
            [ this, measurements = std::move( measurements ) ]< class Handler >( Handler && self )
            {
                auto measurementSize = measurements.size( );
                _service->impl( ).publish_statistics
                (
                    std::move( measurements ),
                    [ self = std::make_shared< Handler >( std::forward< Handler >( self ) ) ]( std::exception_ptr ex )
                    {
                        ( *self )( ex );
                    }
                );
                this->_publishCount.increment( measurementSize );
            },
            token
        );
    }

private:
    Service * _service;
    std::string _publisherName;
    Counter< uint64_t, StatisticsPublisherServiceProvider< Service > > _publishCount;
};

} // namespace Statistics
} // namespace Synthfi
