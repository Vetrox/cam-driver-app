#include <chrono>

#include <ws2tcpip.h>
#include <stdint.h>

#include "jpgd.h"
#include "logger.h"
#include "networkmgr.h"

static constexpr auto PORT = "50684";
static constexpr size_t header_size = 2;
static constexpr auto PAYLOAD_SIZE = 65000;
static constexpr auto UDP_CAP = PAYLOAD_SIZE + header_size;
/* The Buffer into which each packet gets put */
static auto recvbuf = new char[UDP_CAP];

static constexpr auto IMG_BUF_CAP = 1024 * 1024; // 1 MiB
/* The Buffer into which the frames get copied */
static auto imgbuf = new char[IMG_BUF_CAP];

static constexpr auto FRAME_CAP = 20;
static bool frame_recieved[FRAME_CAP]; // max FRAME_CAP frames allowed
static char transmission_id = 0;

static auto last_time = std::chrono::steady_clock::now();
SOCKET listen_socket = NULL, beacon_socket = NULL;
struct addrinfo* multicast_addrinfo = NULL;

bool decompress(RGBImg* img, size_t img_byte_size) {
    // from https://github.com/richgel999/jpeg-compressor
    int actual_comps;
    img->buf = jpgd::decompress_jpeg_image_from_memory(
        (uint8_t*)(imgbuf),
        img_byte_size,
        &img->width,
        &img->height,
        &actual_comps,
        3
    );
    if (actual_comps != 3) return false; // non RGB
    return true;
}

int addrinfo(struct addrinfo** result) {
   
}

void process(int readbytes) {
    const size_t frame_size = readbytes - header_size;

    const auto& trans_id = recvbuf[0];
    const uint8_t& frame_index = recvbuf[1] & 0b0111'1111; // last frame has highest bit set.
    const bool last_frame = (recvbuf[1] & 0b1000'0000) != 0;
    if (trans_id != transmission_id) {
        // interfearing transmissions
        transmission_id = trans_id;
        ZeroMemory(frame_recieved, FRAME_CAP); // assume we have not gotten any frames
    }
    if (!last_frame && frame_size != PAYLOAD_SIZE || frame_index >= FRAME_CAP) { // should never happen things
        abort();
    }

    frame_recieved[frame_index] = true;
    const auto offset = frame_index * PAYLOAD_SIZE; // all non-last frames use 65000 bytes of payload

    memcpy(imgbuf + offset, recvbuf + header_size, frame_size);

    if (last_frame) {
        transmission_id = 0;

        // check if every frame was recieved
        for (auto i = 0; i < frame_index; ++i) {
            if (!frame_recieved[i]) {
                return;
            }
        }

        // the image is ready for decompressing
        if (buffered_image == NULL) {
            RGBImg* img = new RGBImg;
            if (decompress(img, offset + frame_size)) {
                buffered_image = img;
                
                const auto cur_time = std::chrono::steady_clock::now();
                const auto diff = std::chrono::duration_cast<std::chrono::milliseconds>(cur_time - last_time).count();
                cda::log("Delta last frame received: " + std::to_string(diff) + "\n");
                last_time = cur_time;
            }
        }
    }
}

void listen(SOCKET listen_socket) {
    int bytes_recieved;
    do {
        // hold connection until client closes it
        bytes_recieved = recv(listen_socket, recvbuf, UDP_CAP, 0);
        if (bytes_recieved > 0) {
            process(bytes_recieved);
        }
    } while (running);
}

static boolean winsock_initialized = false;

// CALL THIS IN A SEPARATE THREAD: TODO: don't call in separate thread. instead call in ::FillBuffer
//void setup() {
//    Sleep(500);
//    if (!running) {
//        return; // cancel the fluctuative start and stops.
//    }
//
//    int iResult = 0;
//    if (!winsock_initialized) {
//        WSADATA wsaData;
//        iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
//    }
//    if (iResult == 0) {
//        winsock_initialized = true;
//        // Create socket
//        auto listen_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
//        if (listen_socket != INVALID_SOCKET) {
//            // Check if we can open socket and retreive ai_addr
//            struct addrinfo* result = NULL;
//            iResult = addrinfo(&result);
//            if (iResult == 0) {
//                // Bind the socket
//                iResult = bind(listen_socket, result->ai_addr, sizeof(sockaddr));
//                freeaddrinfo(result);
//                if (iResult != SOCKET_ERROR) {
//                    // set the timeout
//                    int timeout_ms = 10;
//                    iResult = setsockopt(listen_socket, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout_ms, sizeof(timeout_ms));
//                    if (iResult != SOCKET_ERROR) {
//                        listen(listen_socket);
//                    }
//                }
//                closesocket(listen_socket);
//            }
//        }
//    }
//    WSACleanup();
//}

constexpr auto* send_data_buf = "BEGIN----v1.0----multicast----cam----server----END"; // see spec
const auto      SEND_DATA_BUF_LEN = strlen(send_data_buf);
constexpr auto  MULTICAST_IP = "239.123.234.45"; // see spec
constexpr auto  MULTICAST_PORT = "50685";          // see spec

/**
* START IN SEPARATE THREAD
* Sends a multicast UDP message to advertise this host to the phone
*/
//void setup_beacon() {
//    
//
//    Sleep(500);
//    if (!running) {
//        return; // cancel the fluctuative start and stops.
//    }
//
//    int iResult = 0;
//    if (!winsock_initialized) {
//        WSADATA wsaData;
//        iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
//    }
//    // Initialize Winsock
//    if (iResult == 0) {
//        // Create socket
//        auto beacon_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
//        if (beacon_socket != INVALID_SOCKET) {
//            // Check if we can open socket and retrieve ai_addr
//            struct addrinfo hints;
//            struct addrinfo* multicast_addrinfo = NULL;
//            ZeroMemory(&hints, sizeof(hints));
//            hints.ai_flags = 0;
//            hints.ai_family = AF_INET;              // IPv4
//            hints.ai_protocol = IPPROTO_UDP;        // Sending UDP
//            hints.ai_socktype = SOCK_DGRAM;         // Sending UDP_Datagrams
//            iResult = getaddrinfo(MULTICAST_IP, MULTICAST_PORT, &hints, &multicast_addrinfo);
//
//            if (iResult == 0) {
//                do {
//                    iResult = sendto(
//                        beacon_socket,
//                        send_data_buf,
//                        SEND_DATA_BUF_LEN,
//                        0,
//                        multicast_addrinfo->ai_addr,
//                        sizeof(sockaddr)
//                    );
//                    Sleep(500);
//                } while (running); // THREAD NEEDED
//            }
//            freeaddrinfo(multicast_addrinfo);
//        }
//        closesocket(beacon_socket);
//    }
//    WSACleanup();
//}

HRESULT cda::setup() {
    int rc = 0;

    WSADATA wsaData;
    if ((rc = WSAStartup(MAKEWORD(2, 2), &wsaData)) != S_OK) {
        cda::log(std::string("WSAStartup failed with error code: ") + std::to_string(WSAGetLastError()));
        return rc;
    }

    //---LISTENER--INIT--BEGIN---//
    // Create socket
    if ((listen_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == INVALID_SOCKET) {
        cda::log(std::string("Creating the listening socket failed with error ") + std::to_string(WSAGetLastError()));
        return rc;
    }

    // Check if we can open socket and retreive ai_addr
    struct addrinfo hints;
    struct addrinfo* listen_addrinfo = NULL;
    ZeroMemory(&hints, sizeof(hints));
    hints.ai_flags = AI_PASSIVE;       // Listening as Server
    hints.ai_family = AF_INET;         // Listening over IPv4
    hints.ai_protocol = IPPROTO_UDP;   // Listening for UDP
    hints.ai_socktype = SOCK_DGRAM;    // Listening for UDP_Datagrams

    if ((rc = getaddrinfo(NULL, PORT, &hints, &listen_addrinfo)) != S_OK) {
        cda::log(std::string("Addrinfo returned an error: ") + std::to_string(WSAGetLastError()));
        return rc;
    }

    // Bind the socket
    if ((rc = bind(listen_socket, listen_addrinfo->ai_addr, sizeof(sockaddr))) == SOCKET_ERROR) {
        freeaddrinfo(listen_addrinfo);
        cda::log(std::string("Bind failed with error ") + std::to_string(WSAGetLastError()));
        return rc;
    }
    freeaddrinfo(listen_addrinfo);

    // set the timeout
    int timeout_ms = 10;
    if ((rc = setsockopt(listen_socket, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout_ms, sizeof(timeout_ms))) == SOCKET_ERROR) {
        cda::log(std::string("Requesting the timeout interval returned an error: ") + std::to_string(WSAGetLastError()));
        return rc;
    }
    //---LISTENER--INIT--END---//

    //---BEACON--INIT--BEGIN---//
    // Create socket
    if ((beacon_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == INVALID_SOCKET) {
        cda::log(std::string("Creating the beacon socket failed with error ") + std::to_string(WSAGetLastError()));
        return rc;
    }
    
    // Check if we can open socket and retreive ai_addr
    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family =   AF_INET;            // IPv4
    hints.ai_protocol = IPPROTO_UDP;        // Sending UDP
    hints.ai_socktype = SOCK_DGRAM;         // Sending UDP_Datagrams
    
    if ((rc = getaddrinfo(MULTICAST_IP, MULTICAST_PORT, &hints, &multicast_addrinfo)) != S_OK) {
        freeaddrinfo(multicast_addrinfo);
        cda::log(std::string("Creating the beacon socket failed with error ") + std::to_string(WSAGetLastError()));
        return rc;
        
    }
    //---BEACON--INIT--END---//

    // TODO: Set good flag
    return S_OK;
}

HRESULT cda::cleanup() {
    int rc = 0; 
    if ((rc = closesocket(listen_socket)) == SOCKET_ERROR) {
        cda::log(std::string("closesocket(listen_socket) failed with error ") + std::to_string(WSAGetLastError()));
        return rc;
    }

    if ((rc = closesocket(beacon_socket)) == SOCKET_ERROR) {
        cda::log(std::string("closesocket(beacon_socket) failed with error ") + std::to_string(WSAGetLastError()));
        return rc;
    }

    if ((rc = WSACleanup()) == SOCKET_ERROR) {
        cda::log(std::string("WSACleanup failed with error ") + std::to_string(WSAGetLastError()));
        return rc;
    }

    freeaddrinfo(multicast_addrinfo);

    return S_OK;
}

/* returns the number of bytes sent */
int cda::send_beacon() {
    return sendto(
        beacon_socket,
        send_data_buf,
        SEND_DATA_BUF_LEN,
        0,
        multicast_addrinfo->ai_addr,
        sizeof(sockaddr)
    );
}

int cda::recv_img() {
    // TODO: implement
}