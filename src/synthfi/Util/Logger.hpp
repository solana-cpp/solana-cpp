#pragma once

#include <boost/date_time/posix_time/posix_time_types.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/keywords/severity.hpp>
#include <boost/log/sources/severity_logger.hpp>
#include <boost/log/support/date_time.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/utility/manipulators/add_value.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/utility/setup/console.hpp>

#include <boost/log/utility/setup/filter_parser.hpp>
#include <boost/log/utility/setup/formatter_parser.hpp>

namespace Synthfi
{

using SynthfiLogger = boost::log::trivial::logger_type;

// Macro that includes severity, filename and line number.
#define SYNTHFI_LOG( logger, sev ) \
    BOOST_LOG_STREAM_SEV( logger, sev ) \
            << boost::log::add_value( "Line", __LINE__ ) \
            << boost::log::add_value( "File", __FILE__ ) \
            << boost::log::add_value( "Function", __FUNCTION__ )

// Macros to log to local logger.
#define SYNTHFI_LOG_TRACE( logger ) SYNTHFI_LOG( logger, boost::log::trivial::severity_level::trace )
#define SYNTHFI_LOG_DEBUG( logger ) SYNTHFI_LOG( logger, boost::log::trivial::severity_level::debug )
#define SYNTHFI_LOG_INFO( logger ) SYNTHFI_LOG( logger, boost::log::trivial::severity_level::info )
#define SYNTHFI_LOG_ERROR( logger ) SYNTHFI_LOG( logger, boost::log::trivial::severity_level::error )

// Macros to log to global logger.
#define SYNTHFI_LOG_TRACE_GLOBAL( ) SYNTHFI_LOG( boost::log::trivial::logger::get( ), boost::log::trivial::severity_level::trace )
#define SYNTHFI_LOG_DEBUG_GLOBAL( ) SYNTHFI_LOG( boost::log::trivial::logger::get( ), boost::log::trivial::severity_level::debug )
#define SYNTHFI_LOG_INFO_GLOBAL( ) SYNTHFI_LOG( boost::log::trivial::logger::get( ), boost::log::trivial::severity_level::info )
#define SYNTHFI_LOG_ERROR_GLOBAL( ) SYNTHFI_LOG( boost::log::trivial::logger::get( ), boost::log::trivial::severity_level::error )

inline void init_logger( boost::log::trivial::severity_level logLevel )
{
    auto consoleSink = boost::log::add_console_log
    (
        std::cout,
        boost::log::keywords::format = boost::log::expressions::stream <<
        "["   << boost::log::expressions::format_date_time< boost::posix_time::ptime >( "TimeStamp", "%Y-%m-%d %H:%M:%S.%f" ) <<
        "] [" << std::left << std::setw( 5 ) << std::setfill(' ') << boost::log::trivial::severity <<
        "] "  << boost::log::expressions::smessage <<
        " ("  << boost::log::expressions::attr< std::string >( "File" ) <<
        ":"   << boost::log::expressions::attr< int >( "Line" ) <<
        ":"   << boost::log::expressions::attr< std::string >( "Function" ) <<
        ")",
        boost::log::keywords::filter = boost::log::trivial::severity >= logLevel
    );

    boost::log::add_common_attributes( );
}

} // namespace Synthfi
