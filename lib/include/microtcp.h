#ifndef LIB_MICROTCP_H_
#define LIB_MICROTCP_H_

#include <stddef.h>
#include <stdint.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h> // IWYU pragma: keep
#include "core/receive_ring_buffer.h"
#include "microtcp_helper_macros.h"

typedef struct microtcp_segment microtcp_segment_t;
typedef struct send_queue send_queue_t;

/**
 * microTCP header structure
 * NOTE: DO NOT CHANGE!
 */
// clang-format off
typedef struct
{
        uint32_t seq_number; /**< Sequence number */
        uint32_t ack_number; /**< ACK number */
        uint16_t control;    /**< Control bits (e.g. SYN, ACK, FIN) */
#ifdef UNORTHODOX_MODE
        uint32_t window; /**< Window size in bytes (offers higher throughput) */
#else
        uint16_t window; /**< Window size in bytes */
#endif /* UNORTHODOX_MODE */
        uint32_t data_len; /**< Payload length in bytes */
#ifndef UNORTHODOX_MODE
        uint32_t future_use0; /**< 32-bits for future use */
        uint32_t future_use1; /**< 32-bits for future use */
        uint32_t future_use2; /**< 32-bits for future use */
#endif /* UNORTHODOX_MODE */

        uint32_t checksum; /**< CRC-32 checksum, see crc32() in utils folder */
} microtcp_header_t;
#define MICROTCP_HEADER_SIZE (sizeof(microtcp_header_t))

/*
 * Several useful constants
 */
#define MICROTCP_ACK_TIMEOUT_US 200000

#ifndef UNORTHODOX_MODE
#define MICROTCP_MSS 1400ULL
#else /* ifdef UNORTHODOX_MODE */
#define MICROTCP_MSS 9000ULL /* Basically we prefer NIC fragmentation, compared to doing more sendto() systems calls (improves performance). */
#endif /* UNORTHODOX_MODE */

#define MICROTCP_MTU (MICROTCP_MSS + sizeof(microtcp_header_t))

#ifdef UNORTHODOX_MODE
#define MICROTCP_RECVBUF_LEN (1 << 16) /* TODO correct to: 32 MBytes (offers higher throughput) */
#else
#define MICROTCP_RECVBUF_LEN 8192 /* 8 KBytes. */
#endif /* UNORTHODOX_MODE */

#define MICROTCP_WIN_SIZE MICROTCP_RECVBUF_LEN
#define MICROTCP_INIT_CWND (3 * MICROTCP_MSS)
#define MICROTCP_INIT_SSTHRESH MICROTCP_WIN_SIZE

_Static_assert(IS_POWER_OF_2(MICROTCP_RECVBUF_LEN), STRINGIFY(MICROTCP_RECVBUF_LEN) " must be a power of 2 number");
// clang-format on

/**
 * Possible states of the microTCP socket
 *
 * NOTE: You can insert any other possible state
 * for your own convenience
 */
typedef enum
{
        RESET = 1 << 0,
        INVALID = 1 << 1,
        CLOSED = 1 << 2, /* when there is no connection. Like when Creating the socket, or after ending a connection. */
        LISTEN = 1 << 3,
        ESTABLISHED = 1 << 4,
        CLOSING_BY_PEER = 1 << 5,
        CLOSING_BY_HOST = 1 << 6,
} microtcp_state_t;

/**
 * This is the microTCP socket structure. It holds all the necessary
 * information of each microTCP socket.
 *
 * NOTE: Fill free to insert additional fields.
 */
typedef struct
{
        int sd;                 /* The underline UDP socket descriptor. */
        microtcp_state_t state; /* The state of the microTCP socket. */
        size_t curr_win_size;   /* The current window size. */
        size_t peer_win_size;

        receive_ring_buffer_t *bytestream_rrb; /* a.k.a `recvbuf`, used to store and reassmble bytes of incoming packets. */

        size_t cwnd;
        size_t ssthresh;

        uint32_t seq_number;       /* Keep the state of the sequence number. */
        uint32_t ack_number;       /* Keep the state of the ack number. */
        uint64_t packets_sent;     /* Packets that were sent from socket. */
        uint64_t packets_lost;     /* Packets that were sent from socket, but (probably) lost.*/
        uint64_t packets_received; /* Packets that were received from socket.  */
        uint64_t bytes_sent;       /* Bytes that were sent from socket. */
        uint64_t bytes_lost;       /* Bytes that were sent from socket, but (probably) lost. */
        uint64_t bytes_received;   /* Bytes that were received from socket. */

        /* Instead of allocating buffers all the time, constructing and receiving
         * MicroTCP segments, we allocate 3 buffers that do all immediate receiving
         * and sending. Although the buffers are dynamically allocated. This happens
         * after the 3-way handshake is complete, and  they are deallocated, either
         * with a successful call to microtcp_shutdown(), or with the (custom made)
         * microtcp_close_socket(). Thus we avoid dynamic memory allocations during
         * receiving and sending data, which hinder transmissions speeds.
         */
        microtcp_segment_t *segment_build_buffer;
        void *bytestream_build_buffer;
        send_queue_t *send_queue;

        /* During data transfering only receiver thread has access. (deprecated IDEA) */
        microtcp_segment_t *segment_receive_buffer;
        void *bytestream_receive_buffer;
        struct sockaddr *peer_address;
        _Bool data_reception_with_finack;
} microtcp_sock_t;

microtcp_sock_t microtcp_socket(int _domain, int _type, int _protocol);

int microtcp_bind(microtcp_sock_t *_socket, const struct sockaddr *_address, socklen_t _address_len);
/**
 * @param socket the socket structure
 * @param address pointer to store the address information of the connected peer
 * @param address_len the length of the address structure.
 * @return 0 on success, -1 on failure.
 */
int microtcp_connect(microtcp_sock_t *socket, const struct sockaddr *address, socklen_t address_len);

/**
 * Blocks waiting for a new connection from a remote peer.
 *
 * @param socket the socket structure
 * @param address pointer to store the address information of the connected peer
 * @param address_len the length of the address structure.
 * @return ATTENTION despite the original accept() this function returns
 * 0 on success or -1 on failure
 */
int microtcp_accept(microtcp_sock_t *socket, struct sockaddr *address, socklen_t address_len);

int microtcp_shutdown(microtcp_sock_t *socket, int how);

ssize_t microtcp_send(microtcp_sock_t *socket, const void *buffer, size_t length, int flags);

ssize_t microtcp_recv(microtcp_sock_t *socket, void *buffer, size_t length, int flags);

/* Part of the extended API(). */
ssize_t microtcp_recv_timed(microtcp_sock_t *_socket, void *_buffer, size_t _length, struct timeval _max_idle_time);

void microtcp_close(microtcp_sock_t *socket);

#endif /* LIB_MICROTCP_H_ */
