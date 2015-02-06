//
//  NetworkManager.h
//
//  Created by Collin B. Stuart on 2011-10-17.
//
//

#ifndef test_NetworkManager_h
#define test_NetworkManager_h

#include <iostream>
#include <istream>
#include <ostream>
#include <string>
#include <functional>
#include <asio.hpp>
#include <bind.hpp>
#include <boost/asio/ssl.hpp>

using namespace std;
using boost::asio::ip::tcp;

enum NetworkManagerRequestType
{
    kNetworkManagerRequestTypeGet = 0,
    kNetworkManagerRequestTypePOST = 1
};

class NetworkManager
{
public:

    NetworkManager(boost::asio::io_service& io_service,
                   boost::asio::ssl::context& context,
                   const std::string& server,
                   const std::string& path,
                   std::function<void(string responseString)> callback,
                   string postParams) : resolver_(io_service), socket_(io_service, context)
    {
        
        socket_.set_verify_mode(boost::asio::ssl::verify_none); //use verify_peer for production code
        
        _callbackFunction = callback;
        _isPost = false;
        
        // Form the request. We specify the "Connection: close" header so that the
        // server will close the socket after transmitting the response. This will
        // allow us to treat all data up until the EOF as the content.
        std::ostream request_stream(&request_);
        if (postParams.empty()) //GET request
        {
            request_stream << "GET " << path << " HTTP/1.0\r\n";
            request_stream << "Host: " << server << "\r\n";
            request_stream << "Accept: */*\r\n";
            
            request_stream << "Connection: close\r\n\r\n";
        }
        else //POST request
        {
            _isPost = true;
            
            request_stream << "POST " << path << " HTTP/1.0\r\n";
            request_stream << "Host: " << server << "\r\n";
            
            request_stream << "Content-Type: application/x-www-form-urlencoded; charset=utf-8\r\n";
            request_stream << "Content-Length: " + std::to_string(postParams.length()) + "\r\n\r\n";
            request_stream << postParams << "\r\n\r\n";
        }

        
        // Start an asynchronous resolve to translate the server and service names
        // into a list of endpoints.
        tcp::resolver::query query(server, "https");
        resolver_.async_resolve(query,
                                boost::bind(&NetworkManager::handle_resolve, this,
                                            boost::asio::placeholders::error,
                                            boost::asio::placeholders::iterator));
    }
    
    static NetworkManager* getRequest(string urlString, string paramString, std::function<void(string responseString)> callback);
    static NetworkManager* postRequest(string urlString, string pathString, string postParams, std::function<void(string responseString)> callback);
    
    //Delegate
    void requestFinished(NetworkManager *request);
    
private:
    void handle_resolve(const boost::system::error_code& err,
                        tcp::resolver::iterator endpoint_iterator);
    
    void handle_connect(const boost::system::error_code& err);
    
    void handle_handshake(const boost::system::error_code& error);
    
    void handle_write_request(const boost::system::error_code& err);
    
    void handle_read_status_line(const boost::system::error_code& err);
    
    void handle_read_headers(const boost::system::error_code& err);
    
    void handle_read_content(const boost::system::error_code& err);
    
    void _dispatchOnMainThreadWithResponseString(string dataString);
    
    tcp::resolver resolver_;
    //tcp::socket socket_;
    boost::asio::ssl::stream<boost::asio::ip::tcp::socket> socket_;
    
    boost::asio::streambuf request_;
    boost::asio::streambuf response_;
    
    ostringstream _responseString;
    std::function<void(string responseString)> _callbackFunction;
    
    bool _isPost;
};

#endif
