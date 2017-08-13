//
// server.cpp
// ~~~~~~~~~~
//
// Copyright (c) 2003-2010 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Copyright (c) 2017 Darrell Wright - Adapted callbacks to use lambda's
//

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <cstdlib>
#include <cstdint>
#include <iostream>

typedef boost::asio::ssl::stream<boost::asio::ip::tcp::socket> ssl_socket;

class session {
	ssl_socket m_socket;
	enum { max_length = 1024 };
	char m_data[max_length];

  public:
	session( boost::asio::io_service &io_service, boost::asio::ssl::context &context )
	    : m_socket{io_service, context}, m_data{0} {}

	ssl_socket::lowest_layer_type &socket( ) {
		return m_socket.lowest_layer( );
	}

	void start( ) {
		m_socket.async_handshake(
		    boost::asio::ssl::stream_base::server,
		    [this]( boost::system::error_code const &error ) { this->handle_handshake( error ); } );
	}

	void handle_handshake( boost::system::error_code const &error ) {
		if( !error ) {
			m_socket.async_read_some( boost::asio::buffer( m_data, max_length ),
			                          [this]( boost::system::error_code const &err, size_t bytes_transferred ) {
				                          handle_read( err, bytes_transferred );
			                          } );
		} else {
			delete this;
		}
	}

	void handle_read( boost::system::error_code const &error, size_t bytes_transferred ) {
		if( !error ) {
			boost::asio::async_write(
			    m_socket, boost::asio::buffer( m_data, bytes_transferred ),
			    [this]( boost::system::error_code const &err, auto const & ) { this->handle_write( err ); } );
		} else {
			delete this;
		}
	}

	void handle_write( boost::system::error_code const &error ) {
		if( !error ) {
			m_socket.async_read_some( boost::asio::buffer( m_data, max_length ),
			                          [this]( boost::system::error_code const &err, size_t bytes_transferred ) {
				                          handle_read( err, bytes_transferred );
			                          } );
		} else {
			delete this;
		}
	}
};

class server {
	boost::asio::io_service &m_io_service;
	boost::asio::ip::tcp::acceptor m_acceptor;
	boost::asio::ssl::context m_context;

  public:
	server( boost::asio::io_service &io_service, uint16_t port )
	    : m_io_service{io_service}
	    , m_acceptor{io_service, boost::asio::ip::tcp::endpoint( boost::asio::ip::tcp::v4( ), port )}
	    , m_context{io_service, boost::asio::ssl::context::tlsv12_server} {

		m_context.set_options( boost::asio::ssl::context::default_workarounds | boost::asio::ssl::context::no_sslv2 |
		                       boost::asio::ssl::context::no_sslv3 | boost::asio::ssl::context::single_dh_use );
		m_context.set_password_callback( [this]( auto, auto ) -> std::string { return this->get_password( ); } );
		m_context.use_certificate_chain_file( "server.pem" );
		m_context.use_private_key_file( "server.pem", boost::asio::ssl::context::pem );
		m_context.use_tmp_dh_file( "dh512.pem" );

		auto new_session = new session( m_io_service, m_context );
		m_acceptor.async_accept( new_session->socket( ), [this, new_session]( boost::system::error_code const &error ) {
			handle_accept( new_session, error );
		} );
	}

	std::string get_password( ) const {
		return "test";
	}

	void handle_accept( session *new_session, boost::system::error_code const &error ) {
		if( !error ) {
			new_session->start( );
			new_session = new session( m_io_service, m_context );
			m_acceptor.async_accept(
			    new_session->socket( ),
			    [this, new_session]( boost::system::error_code const &err ) { handle_accept( new_session, err ); } );
		} else {
			delete new_session;
		}
	}
};

int main( int argc, char *argv[] ) {
	try {
		if( argc != 2 ) {
			std::cerr << "Usage: server <port>\n";
			return EXIT_FAILURE;
		}

		boost::asio::io_service io_service;

		server s( io_service, static_cast<uint16_t>( strtol( argv[1], nullptr, 10 ) ) );

		io_service.run( );
	} catch( std::exception &e ) {
		std::cerr << "Exception: " << e.what( ) << "\n";
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
