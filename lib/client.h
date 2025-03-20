#ifndef CLIENT_BLINK_H
#define CLIENT_BLINK_H

#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

/**
 * @class Client
 * @brief A class to interact with a Blink server using RESP (Blink Serialization Protocol).
 */
class Client
{
public:
    int buffer_size_;
    std::string ip_addr_;
    char *buffer_ = nullptr;
    int port_ = -1;

    /**
     * @brief Constructor for Client class.
     * @param ip_addr IP address of the Blink server.
     * @param port Port number of the Blink server.
     * @param buffer_size Size of the buffer for reading responses.
     */
    Client(std::string ip_addr, int port, int buffer_size)
        : ip_addr_(ip_addr)
    {
        port_ = port;
        buffer_size_ = buffer_size;
        buffer_ = new char[buffer_size_];
    }

    ~Client(){
        delete buffer_;
    }

    /**
     * @brief Initializes the connection to the Blink server.
     * @return Socket descriptor or -1 on failure.
     */
    int server_init()
    {
        sock_ = 0;
        struct sockaddr_in serv_addr;

        // Create socket
        if ((sock_ = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        {
            std::cerr << "Socket creation error" << std::endl;
            return -1;
        }

        serv_addr.sin_family = AF_INET;
        serv_addr.sin_port = htons(port_);

        // Convert IPv4 address from text to binary
        if (inet_pton(AF_INET, ip_addr_.c_str(), &serv_addr.sin_addr) <= 0)
        {
            std::cerr << "Invalid address / Address not supported" << std::endl;
            return -1;
        }

        // Connect to server
        if (connect(sock_, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
        {
            std::cerr << "Connection failed" << std::endl;
            return -1;
        }

        return 1;
    }

    /**
     * @brief Sends a SET command to store a key-value pair.
     * @param key The key.
     * @param value The value.
     * @return Response from the Blink server.
     */
    std::string set(const std::string &key, const std::string &value)
    {
        return decode_resp(send_req(encode_command("SET " + key + " " + value)));
    }

    /**
     * @brief Sends a GET command to retrieve the value of a key.
     * @param key The key.
     * @return Response from the Blink server.
     */
    std::string get(const std::string &key)
    {
        return decode_resp(send_req(encode_command("GET " + key)));
    }

    /**
     * @brief Sends a DEL command to delete a key.
     * @param key The key.
     * @return Response from the Blink server.
     */
    std::string del(const std::string &key)
    {
        return decode_resp(send_req(encode_command("DEL " + key)));
    }

    /**
     * @brief Closes the connection to the Blink server.
     */
    void close_server()
    {
        close(sock_);
    }

private:

    std::vector<std::string> command_;
    int sock_;

    /**
     * @brief Sends an encoded command to the Blink server and retrieves the response.
     * @param encoded Encoded RESP command.
     * @return Server response as a string.
     */
    std::string send_req(const std::string &encoded)
    {
        send(sock_, encoded.c_str(), encoded.length(), 0);

        int bytes_read = read(sock_, buffer_, buffer_size_);
        if (bytes_read <= 0)
        {
            return "-1";
        }
        return std::string(buffer_, bytes_read);
    }
    /**
     * @brief Encodes a command into RESP format.
     * @param input The command string.
     * @return Encoded RESP command.
     */
    std::string encode_command(const std::string &input)
    {
        command_.clear();
        std::istringstream iss(input);
        std::string token;
        while (iss >> token)
        {
            command_.push_back(token);
        }

        std::ostringstream result;
        result << "*" << command_.size() << "\r\n";
        for (const auto &arg : command_)
        {
            result << "$" << arg.length() << "\r\n" << arg << "\r\n";
        }
        return result.str();
    }

    /**
     * @brief Decodes a RESP response.
     * @param response The raw RESP response string.
     * @return Decoded human-readable response.
     */
    std::string decode_resp(const std::string &response)
    {
        if (response.empty())
        {
            return "Empty response";
        }

        if (response == "-1")
        {
            return "Server disconnected";
        }

        char type = response[0];
        std::string content = response.substr(1);

        switch (type)
        {
        case '+':
            return content.substr(0, content.length() - 2); // Simple string
        case '-':
            return "Error: " + content.substr(0, content.length() - 2); // Error
        case ':':
            return content.substr(0, content.length() - 2); // Integer
        case '$':
        {
            size_t newline = content.find("\r\n");
            int len = std::stoi(content.substr(0, newline));
            if (len == -1)
            {
                return "(nil)";
            }
            return content.substr(newline + 2, len);
        }
        case '*':
            return "Array response (parsing not implemented)";
        default:
            return "Unknown response type: " + response;
        }
    }
};

#endif // CLIENT_BLINK_H
