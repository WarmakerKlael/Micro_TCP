#ifndef LIB_MICROTCP_H_
#define LIB_MICROTCP_H_

#if !defined(__STDC_VERSION__) || __STDC_VERSION__ < 201112L
#error C11 or newer required!
#endif /* __STDC_VERSION__ */

#if !defined(__GNUC__)
#error GNU extensions required!
#endif /* __GNUC__ */

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
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
#ifdef OPTIMIZED_MODE
typedef struct
{
        uint32_t seq_number; /**< Sequence number */
        uint32_t ack_number; /**< ACK number */
        uint16_t control;    /**< Control bits (e.g. SYN, ACK, FIN) */
        uint32_t window;     /**< Window size in bytes (offers higher throughput) */
        uint32_t data_len;   /**< Payload length in bytes */
        uint32_t checksum;   /**< CRC-32 checksum, see crc32() in utils folder */
} microtcp_header_t;
#define RRB_MAX_SIZE 2147483648UL /* Based on window bit-width (limiting factor). */
#else                             /* #ifndef OPTIMIZED_MODE */

typedef struct
{
        uint32_t seq_number;  /**< Sequence number */
        uint32_t ack_number;  /**< ACK number */
        uint16_t control;     /**< Control bits (e.g. SYN, ACK, FIN) */
        uint16_t window;      /**< Window size in bytes */
        uint32_t data_len;    /**< Payload length in bytes */
        uint32_t future_use0; /**< 32-bits for future use */
        uint32_t future_use1; /**< 32-bits for future use */
        uint32_t future_use2; /**< 32-bits for future use */
        uint32_t checksum;    /**< CRC-32 checksum, see crc32() in utils folder */
} microtcp_header_t;
#define RRB_MAX_SIZE 32768UL      /* Based on window bit-width (limiting factor). */
/* TODO reset window to 16bit. */
/* TODO reset window to 16bit. */
/* TODO reset window to 16bit. */
/* TODO reset window to 16bit. */
#endif                            /* OPTIMIZED_MODE */
#define MICROTCP_HEADER_SIZE (sizeof(microtcp_header_t))

#define FIELD_OF_TYPE_EXISTS(_type, _field) sizeof(((_type *)NULL)->_field)
_Static_assert(FIELD_OF_TYPE_EXISTS(microtcp_header_t, seq_number), "Type `microtcp_header_t` is missing field: `seq_number`.");
_Static_assert(FIELD_OF_TYPE_EXISTS(microtcp_header_t, ack_number), "Type `microtcp_header_t` is missing field: `ack_number`.");
_Static_assert(FIELD_OF_TYPE_EXISTS(microtcp_header_t, control), "Type `microtcp_header_t` is missing field: `control`.");
_Static_assert(FIELD_OF_TYPE_EXISTS(microtcp_header_t, window), "Type `microtcp_header_t` is missing field: `window`.");
_Static_assert(FIELD_OF_TYPE_EXISTS(microtcp_header_t, data_len), "Type `microtcp_header_t` is missing field: `data_len`.");
_Static_assert(FIELD_OF_TYPE_EXISTS(microtcp_header_t, checksum), "Type `microtcp_header_t` is missing field: `checksum`.");

/*
 * Several useful constants
 */
#define MICROTCP_ACK_TIMEOUT_US 200000
#define MICROTCP_MSS 1400ULL
#define MICROTCP_MTU (MICROTCP_MSS + sizeof(microtcp_header_t))
#ifdef OPTIMIZED_MODE
#define MICROTCP_RECVBUF_LEN 1048576 /* 1 MByte. */
#else
#define MICROTCP_RECVBUF_LEN 8192 /* 8 KBytes. */
#endif                            /* OPTIMIZED_MODE */
#define MICROTCP_WIN_SIZE MICROTCP_RECVBUF_LEN
#define MICROTCP_INIT_CWND (3 * MICROTCP_MSS)

#define MICROTCP_INIT_SSTHRESH MICROTCP_WIN_SIZE

_Static_assert(IS_POWER_OF_2(MICROTCP_RECVBUF_LEN), STRINGIFY(MICROTCP_RECVBUF_LEN) " must be a power of 2 number");

/**
 * Possible states of the microTCP socket:
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

#ifdef LOG_TRAFFIC_MODE
        FILE *inbound_traffic_log;
        FILE *outbound_traffic_log;
#endif /* LOG_TRAFFIC_MODE */
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
