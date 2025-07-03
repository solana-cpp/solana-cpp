#pragma once

#include "synthfi/Util/Fixed.hpp"
#include "synthfi/Util/JsonUtils.hpp"

#include <boost/asio/io_context.hpp>
#include <boost/asio/strand.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/high_resolution_timer.hpp>

#include <chrono>
#include <string>
#include <variant>

namespace Synthfi
{
namespace Statistics
{

using InfluxDataType = std::variant< double, uint64_t, int64_t, bool, std::string, FixedFloat >;

struct InfluxMeasurement
{
    std::string measurementName;
    std::vector< std::pair< std::string, std::string > > tags;
    std::vector< std::pair< std::string, InfluxDataType > > fields;
};

template< class ValueType, class PublisherType > requires std::is_arithmetic_v< ValueType >
class Counter 
{
public:
    Counter
    (
        std::string_view measurementName,
        std::vector< std::pair< std::string, std::string > > tags,
        std::string_view fieldKey,
        ValueType fieldValue,
        boost::asio::strand< boost::asio::io_context::executor_type > * strand,
        PublisherType * publisher
    );
    ~Counter( ) = default;

    void set( ValueType fieldValue );
    void increment( ValueType fieldValue );

private:
    boost::asio::awaitable< void > do_set( ValueType fieldValue );
    boost::asio::awaitable< void > do_increment( ValueType fieldValue );
    boost::asio::awaitable< void > do_publish( );

    ValueType _counter;

    InfluxMeasurement _measurement;

    boost::asio::strand< boost::asio::io_context::executor_type > * _strand;
    PublisherType * _publisher;
    boost::asio::high_resolution_timer _publishTimer;
};

struct StatisticsPublisherConfig
{
    std::string influxAddress;
    uint16_t influxPort;
    std::string influxApiToken;
    std::string bucket;

    friend StatisticsPublisherConfig tag_invoke( json_to_tag< StatisticsPublisherConfig >, simdjson::ondemand::value config );
};

} // namespace Statistics
} // namespace Synthfi
