#include <Poco/Net/ServerSocket.h>
#include <Poco/Net/StreamSocket.h>
#include <Poco/Net/SocketStream.h>
#include <Poco/Net/SocketAddress.h>
#include <Poco/Net/HTTPClientSession.h>
#include <Poco/Net/HTTPRequest.h>
#include <Poco/Net/HTTPResponse.h>
#include <Poco/StreamCopier.h>
#include <Poco/URI.h>

#include <iostream>
#include <sstream>
#include <thread>

// Use namespaces for brevity
using namespace Poco;
using namespace Poco::Net;
using namespace std;

// ---------------------------
// Forward declaration
void relay(StreamSocket& from, StreamSocket& to);

// ---------------------------
// Relay data between sockets
void relay(StreamSocket& from, StreamSocket& to) {
    char buffer[4096];
    Timespan timeout(5, 0); // 5 seconds timeout

    while (true) {
        if (from.poll(timeout, Socket::SELECT_READ)) {
            int n = from.receiveBytes(buffer, sizeof(buffer));
            if (n > 0)
                to.sendBytes(buffer, n);
            else
                break; // EOF or connection closed
        }

        if (to.poll(timeout, Socket::SELECT_READ)) {
            int n = to.receiveBytes(buffer, sizeof(buffer));
            if (n > 0)
                from.sendBytes(buffer, n);
            else
                break;
        }
    }
}

// ---------------------------
// Handle client request
void handleClient(StreamSocket clientSocket) {
    try {
        SocketStream clientStream(clientSocket);
        std::string requestLine;
        std::getline(clientStream, requestLine);

        std::istringstream requestStream(requestLine);
        std::string method, url, version;
        requestStream >> method >> url >> version;

        if (method == "CONNECT") {
            // HTTPS request
            size_t colonPos = url.find(':');
            std::string host = (colonPos != std::string::npos) ? url.substr(0, colonPos) : url;
            int port = (colonPos != std::string::npos) ? std::stoi(url.substr(colonPos + 1)) : 443;

            StreamSocket serverSocket;
            serverSocket.connect(SocketAddress(host, port));

            clientStream << "HTTP/1.1 200 Connection Established\r\n\r\n";
            clientStream.flush();

            std::thread t1(relay, std::ref(clientSocket), std::ref(serverSocket));
            std::thread t2(relay, std::ref(serverSocket), std::ref(clientSocket));
            t1.join();
            t2.join();
        }
        else {
            // HTTP request
            URI uri(url);
            HTTPClientSession session(uri.getHost(), uri.getPort());

            std::string path = uri.getPathAndQuery();
            if (path.empty()) path = "/";

            HTTPRequest proxyRequest(method, path, version);
            session.sendRequest(proxyRequest);

            HTTPResponse proxyResponse;
            std::istream& rs = session.receiveResponse(proxyResponse);

            clientStream << "HTTP/" << proxyResponse.getVersion() << " "
                << static_cast<int>(proxyResponse.getStatus()) << " "
                << proxyResponse.getReason() << "\r\n";

            for (const auto& header : proxyResponse) {
                clientStream << header.first << ": " << header.second << "\r\n";
            }
            clientStream << "\r\n";

            StreamCopier::copyStream(rs, clientStream);
        }
    }
    catch (const std::exception& e) {
        std::cerr << "Client error: " << e.what() << std::endl;
    }
}

// ---------------------------
// Main entry point
int main() {
    try {
        ServerSocket server(8080);
        std::cout << "Proxy server running on port 8080..." << std::endl;

        while (true) {
            StreamSocket clientSocket = server.acceptConnection();
            std::thread(handleClient, std::move(clientSocket)).detach();
        }
    }
    catch (const std::exception& e) {
        std::cerr << "Server error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
