//
//  NetworkManager.cpp
//
//  Created by Collin B. Stuart on 2011-10-17.
//
//

#import "NetworkManager.h"
#import "Crypto.h"
#include "rapidxml.hpp"
#include "rapidxml_print.hpp"
#include <thread>

void _threadWork(string urlString, string pathString, string postParams, std::function<void(string responseString)> callback)
{
    NetworkManager *theClient = NULL;
    try
    {
        boost::asio::io_service io_service;
        
        boost::asio::ssl::context ctx(boost::asio::ssl::context::sslv23); //v23 = Generic SSL/TLS
        ctx.set_default_verify_paths();
        
        NetworkManager c(io_service, ctx, urlString, pathString, callback, postParams);
        theClient = &c;
        io_service.run();
    }
    catch (std::exception& e)
    {
        std::cout << "Exception: " << e.what() << "\n";
    }
}

NetworkManager* NetworkManager::getRequest(string urlString, string paramString, std::function<void(string responseString)> callback)
{
    std::thread thread(_threadWork, urlString, paramString, "", callback);
    thread.detach();
    return NULL;
}

NetworkManager* NetworkManager::postRequest(string urlString, string pathString, string postParams, std::function<void(string responseString)> callback)
{
    std::thread thread(_threadWork, urlString, pathString, postParams, callback);
    thread.detach();
    return NULL;
}

void NetworkManager::handle_resolve(const boost::system::error_code& err,
                    tcp::resolver::iterator endpoint_iterator)
{
    if (!err)
    {
        // Attempt a connection to each endpoint in the list until we
        // successfully establish a connection.
        boost::asio::async_connect(socket_.lowest_layer(), endpoint_iterator,
                                   boost::bind(&NetworkManager::handle_connect, this,
                                               boost::asio::placeholders::error));
    }
    else
    {
        std::cout << "Error: " << err.message() << "\n";
    }
}

void NetworkManager::handle_connect(const boost::system::error_code& err)
{
    if (!err)
    {
        
        socket_.async_handshake(boost::asio::ssl::stream_base::client,
                                boost::bind(&NetworkManager::handle_handshake, this,
                                            boost::asio::placeholders::error));
        /*
        // The connection was successful. Send the request.
        boost::asio::async_write(socket_, request_,
                                 boost::bind(&NetworkManager::handle_write_request, this,
                                             boost::asio::placeholders::error));
         */
    }
    else
    {
        std::cout << "Error: " << err.message() << "\n";
    }
}

void NetworkManager::handle_handshake(const boost::system::error_code& error)
{
    if (!error)
    {
//        std::cout << "Enter message: ";
//        static char const raw[] = "POST / HTTP/1.1\r\nHost: www.example.com\r\nConnection: close\r\n\r\n";
//        
//        static_assert(sizeof(raw)<=sizeof(request_), "too large");
//        
//        size_t request_length = strlen(raw);
//        std::copy(raw, raw+request_length, request_);
//        
//        {
//            // used this for debugging:
//            std::ostream hexos(std::cout.rdbuf());
//            for(auto it = raw; it != raw+request_length; ++it)
//                hexos << std::hex << std::setw(2) << std::setfill('0') << std::showbase << ((short unsigned) *it) << " ";
//            std::cout << "\n";
//        }
        
        boost::asio::async_write(socket_, request_,
                                 boost::bind(&NetworkManager::handle_write_request, this,
                                             boost::asio::placeholders::error));
    }
    else
    {
        std::cout << "Handshake failed: " << error.message() << "\n";
    }
}

void NetworkManager::handle_write_request(const boost::system::error_code& err)
{
    if (!err)
    {
        // Read the response status line. The response_ streambuf will
        // automatically grow to accommodate the entire line. The growth may be
        // limited by passing a maximum size to the streambuf constructor.
        boost::asio::async_read_until(socket_, response_, "\r\n",
                                      boost::bind(&NetworkManager::handle_read_status_line, this,
                                                  boost::asio::placeholders::error));
    }
    else
    {
        std::cout << "Error: " << err.message() << "\n";
    }
}

void NetworkManager::handle_read_status_line(const boost::system::error_code& err)
{
    if (!err)
    {
        // Check that response is OK.
        std::istream response_stream(&response_);
        std::string http_version;
        response_stream >> http_version;
        unsigned int status_code;
        response_stream >> status_code;
        std::string status_message;
        std::getline(response_stream, status_message);
        if (!response_stream || http_version.substr(0, 5) != "HTTP/")
        {
            std::cout << "Invalid response\n";
            return;
        }
        if (status_code != 200)
        {
            std::cout << "Response returned with status code ";
            std::cout << status_code << "\n";
            return;
        }
        
        // Read the response headers, which are terminated by a blank line.
        boost::asio::async_read_until(socket_, response_, "\r\n\r\n",
                                      boost::bind(&NetworkManager::handle_read_headers, this,
                                                  boost::asio::placeholders::error));
    }
    else
    {
        std::cout << "Error: " << err << "\n";
    }
}

void NetworkManager::handle_read_headers(const boost::system::error_code& err)
{
    if (!err)
    {
        // Process the response headers.
        std::istream response_stream(&response_);
        std::string header;
        while (std::getline(response_stream, header) && header != "\r")
        {
            //std::cout << header << "\n";
        }
        //std::cout << "\n";
        
        // Start reading remaining data until EOF.
        boost::asio::async_read(socket_, response_,
                                boost::asio::transfer_at_least(1),
                                boost::bind(&NetworkManager::handle_read_content, this,
                                            boost::asio::placeholders::error));
    }
    else
    {
        std::cout << "Error: " << err << "\n";
    }
}

void NetworkManager::handle_read_content(const boost::system::error_code& err)
{    
    if (err == boost::asio::error::eof)
    {
        // Write all of the data that has been read so far.

        _responseString << &response_;
        
        _dispatchOnMainThreadWithResponseString(_responseString.str());
    }
    else if (!err)
    {
        // Write all of the data that has been read so far.
        //cout << &response_ << endl;
        
        //cout << _responseString.str() << endl;
        _responseString << &response_;
        
        // Continue reading remaining data until EOF.
        boost::asio::async_read(socket_, response_,
                                boost::asio::transfer_at_least(1),
                                boost::bind(&NetworkManager::handle_read_content, this,
                                            boost::asio::placeholders::error));

    }
}

void Go(pair<function<void(string)>, string> *thePair)
{
    if (thePair)
    {
        string dataString = thePair->second;
        function<void(string)> f = thePair->first;
        f(dataString);
        delete thePair;
        thePair = NULL;
    }
}

void NetworkManager::_dispatchOnMainThreadWithResponseString(string dataString)
{
    pair< function<void(string)>, string > *thePair = new pair< function<void(string)>, string >;
    function<void(string)> f(_callbackFunction);
    thePair->first = f;
    thePair->second = dataString;
    
    //Pass in the callback function as context so it doesn't get destroyed
    dispatch_async_f(dispatch_get_main_queue(), thePair, (dispatch_function_t)Go);
}