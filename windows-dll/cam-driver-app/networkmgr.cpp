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
static SOCKET listen_socket = NULL, beacon_socket = NULL;
static struct addrinfo* multicast_addrinfo = NULL;

static constexpr auto GOOD = 0, UNDEFINED = -1, UNINITIALIZED = -2;
static int status = UNINITIALIZED;
static WSAData *wsa = NULL;

static bool recv_timeouted_last_time = true;

bool decompress(size_t src_len, uint8_t* dst, size_t dst_len) { // TODO: change this to use HRESULT
    
    // from https://github.com/richgel999/jpeg-compressor
    int actual_comps, width, height;
    uint8_t* dec = jpgd::decompress_jpeg_image_from_memory(
        (uint8_t*)(imgbuf),
        src_len,
        &width,
        &height,
        &actual_comps,
        3
    );
    if (actual_comps != 3) {
        free(dec);
        return false; // non RGB
    }
    const size_t dec_len = width * height * 3;
    if (dec_len > dst_len) {
        cda::logln("ABORT. Requested to write in buffer not capable of storing the image data.");
        cda::log("dec_len: ");
        cda::logln(std::to_string(dec_len));
        cda::log("dst_len: ");
        cda::logln(std::to_string(dst_len));
        free(dec);
        return false; // error writing
    }

    memcpy(dst, dec, dec_len); // TODO: optimize amount of memcopies
    free(dec);
    return true;
}

HRESULT process(int readbytes, uint8_t *dst, size_t dst_len) {
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
        return E_UNEXPECTED;
    }

    frame_recieved[frame_index] = true;
    const auto offset = frame_index * PAYLOAD_SIZE; // all non-last frames use 65000 bytes of payload

    memcpy(imgbuf + offset, recvbuf + header_size, frame_size);

    if (last_frame) {
        transmission_id = 0;

        // check if every frame was recieved
        for (auto i = 0; i < frame_index; ++i) {
            if (!frame_recieved[i]) {
                return E_ABORT;
            }
        }

        if (decompress(offset + frame_size, dst, dst_len)) {
            const auto cur_time = std::chrono::steady_clock::now();
            const auto diff = std::chrono::duration_cast<std::chrono::milliseconds>(cur_time - last_time).count();
            //cda::logln("Delta last frame received: " + std::to_string(diff));
            last_time = cur_time;
            return S_OK;
        }
    }
    return E_ABORT;
}

static constexpr auto* send_data_buf = "BEGIN----v1.0----multicast----cam----server----END"; // see spec
static const auto      SEND_DATA_BUF_LEN = strlen(send_data_buf);
static constexpr auto  MULTICAST_IP = "239.123.234.45"; // see spec
static constexpr auto  MULTICAST_PORT = "50685";          // see spec


HRESULT cda::setup() {
    if (status == UNDEFINED && cda::cleanup() != S_OK) {
        cda::logln("CLEANUP FAILED AND SETUP CAN'T PROCEED");
        abort();
    }
    int rc = 0;

    wsa = new WSAData;
    if ((rc = WSAStartup(MAKEWORD(2, 2), wsa)) != S_OK) {
        cda::logln(std::string("WSAStartup failed with error code ") + std::to_string(rc));
        status = UNDEFINED; 
        return rc;
    }

    //---LISTENER--INIT--BEGIN---//
    // Create socket
    if ((listen_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == INVALID_SOCKET) {
        cda::logln(std::string("Creating the listening socket failed with error ") + std::to_string(WSAGetLastError()));
        status = UNDEFINED; 
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
        cda::logln(std::string("Addrinfo returned an error: ") + std::to_string(WSAGetLastError()));
        status = UNDEFINED; 
        return rc;
    }

    // Bind the socket
    if ((rc = bind(listen_socket, listen_addrinfo->ai_addr, sizeof(sockaddr))) == SOCKET_ERROR) {
        freeaddrinfo(listen_addrinfo);
        cda::logln(std::string("Bind failed with error ") + std::to_string(WSAGetLastError()));
        status = UNDEFINED; 
        return rc;
    }
    freeaddrinfo(listen_addrinfo);

    // set the timeout
    int timeout_ms = 500;
    if ((rc = setsockopt(listen_socket, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout_ms, sizeof(timeout_ms))) == SOCKET_ERROR) {
        cda::logln(std::string("Requesting the timeout interval returned an error: ") + std::to_string(WSAGetLastError()));
        status = UNDEFINED; 
        return rc;
    }
    //---LISTENER--INIT--END---//

    //---BEACON--INIT--BEGIN---//
    // Create socket
    if ((beacon_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == INVALID_SOCKET) {
        cda::logln(std::string("Creating the beacon socket failed with error ") + std::to_string(WSAGetLastError()));
        status = UNDEFINED;
        return rc;
    }
    
    // Check if we can open socket and retreive ai_addr
    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family =   AF_INET;            // IPv4
    hints.ai_protocol = IPPROTO_UDP;        // Sending UDP
    hints.ai_socktype = SOCK_DGRAM;         // Sending UDP_Datagrams
    
    if ((rc = getaddrinfo(MULTICAST_IP, MULTICAST_PORT, &hints, &multicast_addrinfo)) != S_OK) {
        freeaddrinfo(multicast_addrinfo);
        cda::logln(std::string("Creating the beacon socket failed with error ") + std::to_string(WSAGetLastError()));
        status = UNDEFINED;
        return rc;
        
    }
    //---BEACON--INIT--END---//
    status = GOOD;
    return S_OK;
}

HRESULT cda::cleanup() {
    int rc = 0; 
    if (listen_socket != NULL && (rc = closesocket(listen_socket)) == SOCKET_ERROR) {
        cda::logln(std::string("closesocket(listen_socket) failed with error ") + std::to_string(WSAGetLastError()));
        status = UNDEFINED; 
        return rc;
    }
    listen_socket = NULL;

    if (beacon_socket != NULL && (rc = closesocket(beacon_socket)) == SOCKET_ERROR) {
        cda::logln(std::string("closesocket(beacon_socket) failed with error ") + std::to_string(WSAGetLastError()));
        status = UNDEFINED; 
        return rc;
    }
    beacon_socket = NULL;

    if (wsa != NULL && (rc = WSACleanup()) == SOCKET_ERROR) {
        cda::logln(std::string("WSACleanup failed with error ") + std::to_string(WSAGetLastError()));
        status = UNDEFINED; 
        return rc;
    }
    wsa = NULL;

    if (multicast_addrinfo != NULL) {
        freeaddrinfo(multicast_addrinfo);
    }
    multicast_addrinfo = NULL;
    
    recv_timeouted_last_time = true;

    status = UNINITIALIZED;
    return S_OK;
}

/* returns the number of bytes sent */
HRESULT cda::send_beacon() {
    if (status != GOOD) {
        cda::logln(std::string("send_beacon was called but the status was not good: ") + std::to_string(status));
        return E_ABORT;
    }
    if (!recv_timeouted_last_time) {
        return E_ABORT;
    }
    cda::logln("Trying to send beacon.");
    int rc;
    if ((rc = sendto(
        beacon_socket,
        send_data_buf,
        SEND_DATA_BUF_LEN,
        0,
        multicast_addrinfo->ai_addr,
        sizeof(sockaddr)
    )) == SOCKET_ERROR) {
        cda::logln(std::string("Sending the beacon message failed with error ") + std::to_string(WSAGetLastError()));
        status = UNDEFINED;
        return SOCKET_ERROR;
    }
    if (rc == 0) {
        cda::logln("Sending the beacon failed. No bytes were sent.\n");
    }
    return rc;
}

HRESULT cda::recv_img(uint8_t *dst, size_t dst_len) {
    if (status != GOOD) {
        cda::logln(std::string("recv_img was called but the status was not good: ") + std::to_string(status));
        return E_FAIL;
    }
    const auto start = std::chrono::steady_clock::now();
    while(true) {
        // blocking call. time-limit ensures we proceed even if no packets arrive in time
        const auto bytes_reveived = recv(listen_socket, recvbuf, UDP_CAP, 0);
        if (bytes_reveived == 0) {
            cda::logln("Reveived 0 bytes. The Connection was closed.");
            status = UNDEFINED;
            return E_FAIL;  // connection was closed (how?)
        }

        if (bytes_reveived == SOCKET_ERROR) {
            const int err = WSAGetLastError();
            if (err != WSAETIMEDOUT) {
                cda::logln(std::string("Receiving message failed with error ") + std::to_string(err));
                status = UNDEFINED;
                return E_FAIL;
            }
        }
        else {
            int rc;
            if ((rc = process(bytes_reveived, dst, dst_len)) == S_OK) {
                if (recv_timeouted_last_time) {
                    cda::logln("Receiving images...");
                }
                recv_timeouted_last_time = false;
                return S_OK;
            }
            if (rc == E_UNEXPECTED) {
                cda::logln("processing encountered an unexpected error.");
                status = UNDEFINED;
                return E_UNEXPECTED;
            }
        }

        const auto end = std::chrono::steady_clock::now();
        const auto delta = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
        if (delta > 500) {
            recv_timeouted_last_time = true;
            return E_ABORT;
        }
    }
}