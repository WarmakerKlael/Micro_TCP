#ifndef LIB_MICROTCP_H_
#define LIB_MICROTCP_H_

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdint.h>

/*
 * Several useful constants
 */
#define MICROTCP_ACK_TIMEOUT_US 200000
#define MICROTCP_MSS 1400
#define MICROTCP_RECVBUF_LEN 8192
#define MICROTCP_WIN_SIZE MICROTCP_RECVBUF_LEN
#define MICROTCP_INIT_CWND (3 * MICROTCP_MSS)
#define MICROTCP_INIT_SSTHRESH MICROTCP_WIN_SIZE

#define ACK_BIT (1 << 12)
#define RST_BIT (1 << 13)
#define SYN_BIT (1 << 14)
#define FIN_BIT (1 << 15)

/**
 * Possible states of the microTCP socket
 *
 * NOTE: You can insert any other possible state
 * for your own convenience
 */
typedef enum
{
        INVALID = -1,
        ANY = 0,
        CLOSED, /* When there is no connection. Like when Creating the socket, or after ending a connection. */
        LISTEN,
        ESTABLISHED,
        CLOSING_BY_PEER,
        CLOSING_BY_HOST
} mircotcp_state_t;

/**
 * This is the microTCP socket structure. It holds all the necessary
 * information of each microTCP socket.
 *
 * NOTE: Fill free to insert additional fields.
 */
typedef struct
{
        int sd;                 /* The underline UDP socket descriptor. */
        mircotcp_state_t state; /* The state of the microTCP socket. */
        size_t init_win_size;   /* The window size advertised at the 3-way handshake. */
        size_t curr_win_size;   /* The current window size. */
        size_t peer_win_size;

        void *recvbuf;

        size_t buf_fill_level; /* Amount of data in the buffer. */

        size_t cwnd;
        size_t ssthresh;

        size_t seq_number;         /* Keep the state of the sequence number. */
        size_t ack_number;         /* Keep the state of the ack number. */
        uint64_t packets_sent;     /* Packets that were sent from socket. */
        uint64_t packets_lost;     /* Packets that were sent from socket, but (probably) lost.*/
        uint64_t packets_received; /* Packets that were received from socket.  */
        uint64_t bytes_sent;       /* Bytes that were sent from socket. */
        uint64_t bytes_lost;       /* Bytes that were sent from socket, but (probably) lost. */
        uint64_t bytes_received;   /* Bytes that were received from socket. */
} microtcp_sock_t;

/**
 * microTCP header structure
 * NOTE: DO NOT CHANGE!
 */
typedef struct
{
        uint32_t seq_number;  /**< Sequence number */
        uint32_t ack_number;  /**< ACK number */
        uint16_t control;     /**< Control bits (e.g. SYN, ACK, FIN) */
        uint16_t window;      /**< Window size in bytes */
        uint32_t data_len;    /**< Data length in bytes (EXCLUDING header) */
        uint32_t future_use0; /**< 32-bits for future use */
        uint32_t future_use1; /**< 32-bits for future use */
        uint32_t future_use2; /**< 32-bits for future use */
        uint32_t checksum;    /**< CRC-32 checksum, see crc32() in utils folder */
} microtcp_header_t;

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

#endif /* LIB_MICROTCP_H_ */
