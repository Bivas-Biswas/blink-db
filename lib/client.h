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
    int buffer_size;   ///< Buffer size for reading responses.
    std::string ip_addr;  ///< IP address of the Blink server.
    char *buffer = nullptr; ///< Dynamic buffer for receiving data.
    int port = -1; ///< Port number of the Blink server.

    /**
     * @brief Constructor for Client class.
     * @param _ip_addr IP address of the Blink server.
     * @param _port Port number of the Blink server.
     * @param _buffer_size Size of the buffer for reading responses.
     */
    Client(std::string _ip_addr, int _port, int _buffer_size)
        : ip_addr(_ip_addr), port(_port), buffer_size(_buffer_size)
    {
        buffer = new char[buffer_size];
    }

    /**
     * @brief Destructor to free allocated memory.
     */
    ~Client()
    {
        delete[] buffer;
    }

    /**
     * @brief Initializes the connection to the Blink server.
     * @return Socket descriptor or -1 on failure.
     */
    int server_init()
    {
        sock = 0;
        struct sockaddr_in serv_addr;

        // Create socket
        if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        {
            std::cerr << "Socket creation error" << std::endl;
            return -1;
        }

        serv_addr.sin_family = AF_INET;
        serv_addr.sin_port = htons(port);

        // Convert IPv4 address from text to binary
        if (inet_pton(AF_INET, ip_addr.c_str(), &serv_addr.sin_addr) <= 0)
        {
            std::cerr << "Invalid address / Address not supported" << std::endl;
            return -1;
        }

        // Connect to server
        if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
        {
            std::cerr << "Connection failed" << std::endl;
            return -1;
        }

        return 1;
    }

    /**
     * @brief Sends a SET command to store a key-value pair.
     * @param _key The key.
     * @param _value The value.
     * @return Response from the Blink server.
     */
    std::string set(const std::string &_key, const std::string &_value)
    {
        return decode_resp(send_req(encode_command("SET " + _key + " " + _value)));
    }

    /**
     * @brief Sends a GET command to retrieve the value of a key.
     * @param _key The key.
     * @return Response from the Blink server.
     */
    std::string get(const std::string &_key)
    {
        return decode_resp(send_req(encode_command("GET " + _key)));
    }

    /**
     * @brief Sends a DEL command to delete a key.
     * @param _key The key.
     * @return Response from the Blink server.
     */
    std::string del(const std::string &_key)
    {
        return decode_resp(send_req(encode_command("DEL " + _key)));
    }

    /**
     * @brief Closes the connection to the Blink server.
     */
    void close_server()
    {
        close(sock);
    }

private:
    std::vector<std::string> command; ///< Stores parsed command arguments.
    int sock; ///< Socket descriptor.

    /**
     * @brief Sends an encoded command to the Blink server and retrieves the response.
     * @param _encoded Encoded RESP command.
     * @return Server response as a string.
     */
    std::string send_req(const std::string &_encoded)
    {
        send(sock, _encoded.c_str(), _encoded.length(), 0);

        int bytes_read = read(sock, buffer, buffer_size);
        if (bytes_read <= 0)
        {
            return "-1";
        }
        return std::string(buffer, bytes_read);
    }

    /**
     * @brief Encodes a command into RESP format.
     * @param _input The command string.
     * @return Encoded RESP command.
     */
    std::string encode_command(const std::string &_input)
    {
        command.clear();
        std::istringstream iss(_input);
        std::string token;
        while (iss >> token)
        {
            command.push_back(token);
        }

        std::ostringstream result;
        result << "*" << command.size() << "\r\n";
        for (const auto &arg : command)
        {
            result << "$" << arg.length() << "\r\n" << arg << "\r\n";
        }
        return result.str();
    }

    /**
     * @brief Decodes a RESP response.
     * @param _response The raw RESP response string.
     * @return Decoded human-readable response.
     */
    std::string decode_resp(const std::string &_response)
    {
        if (_response.empty())
        {
            return "Empty response";
        }

        if (_response == "-1")
        {
            return "Server disconnected";
        }

        char type = _response[0];
        std::string content = _response.substr(1);

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
            return "Unknown response type: " + _response;
        }
    }
};

#endif // CLIENT_BLINK_H
