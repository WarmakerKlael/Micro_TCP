#ifndef MINIREDIS_COMMON_SOURCE_CODE_H
#define MINIREDIS_COMMON_SOURCE_CODE_H

/* SOURCE CODE IN HEADER????? WHAT??? DID YOU GO CRAZY??? Relax... buddy.. code file contains only static inline functions with INCLUDE-GUARD. */

#include <time.h>
#include "demo_common.h"
#include "microtcp_prompt_util.h"
#include "settings/microtcp_settings.h"
#include "microtcp_helper_functions.h"
#include "smart_assert.h"

#define KB (1ULL << 10)
#define MB (1ULL << 20)
#define GB (1ULL << 30)
#define TB (1ULL << 40)
#define PB (1ULL << 50)
#define EB (1ULL << 60)

extern struct timeval max_response_idle_time;

struct data_size
{
        const double size;
        const char *unit;
};

static __always_inline struct data_size get_formatted_byte_count(const size_t _byte_count)
{
        if (_byte_count < KB)
                return (struct data_size){.size = _byte_count, .unit = "B"};
        if (_byte_count < MB)
                return (struct data_size){.size = _byte_count / (double)KB, .unit = "KB"};
        if (_byte_count < GB)
                return (struct data_size){.size = _byte_count / (double)MB, .unit = "MB"};
        if (_byte_count < TB)
                return (struct data_size){.size = _byte_count / (double)GB, .unit = "GB"};
        if (_byte_count < PB)
                return (struct data_size){.size = _byte_count / (double)TB, .unit = "TB"};
        return (struct data_size){.size = _byte_count / (long double)PB, .unit = "PB"};
}

static __always_inline struct data_size get_formatted_bit_count(const size_t _byte_count)
{
#define BITS_PER_BYTE 8
        const size_t _bit_count = BITS_PER_BYTE * _byte_count;
        if (_bit_count < KB)
                return (struct data_size){.size = _bit_count, .unit = "b"};
        if (_bit_count < MB)
                return (struct data_size){.size = _bit_count / (double)KB, .unit = "Kb"};
        if (_bit_count < GB)
                return (struct data_size){.size = _bit_count / (double)MB, .unit = "Mb"};
        if (_bit_count < TB)
                return (struct data_size){.size = _bit_count / (double)GB, .unit = "Gb"};
        if (_bit_count < PB)
                return (struct data_size){.size = _bit_count / (double)TB, .unit = "Tb"};
        return (struct data_size){.size = _bit_count / (long double)PB, .unit = "Pb"};
#undef BITS_PER_BYTE
}

struct __attribute__((packed)) registry_node_serialization_info /* We are packing, because we want to pass this info through network packets. */
{
        size_t file_name_size;
        size_t file_size;
        size_t download_counter;
        time_t time_of_arrival;
};

enum io_type
{
        IO_READ,
        IO_WRITE
};
static inline void prompt_to_configure_microtcp(void);
static status_t miniredis_terminate_connection(microtcp_sock_t *_utcp_socket);
static void set_max_response_idle_time(size_t _max_idle_time_multiplier);
static void create_directory(const char *_directory_name);

static __always_inline FILE *open_file_for_binary_io(const char *_file_name, enum io_type _io_type);
static __always_inline uint8_t *allocate_message_buffer(size_t _message_buffer_size);

static __always_inline status_t receive_file(microtcp_sock_t *_socket, uint8_t *_message_buffer, size_t _message_buffer_size,
                                             FILE *_file_ptr, size_t _file_size, const char *_file_name);
static __always_inline status_t receive_and_write_file_part(microtcp_sock_t *_socket, FILE *_file_ptr, const char *_file_name,
                                                            uint8_t *_file_buffer, size_t _file_part_size);
static __always_inline status_t finalize_file(FILE **_file_ptr_address, const char *_staging_file_name, const char *_export_file_name);

static __always_inline status_t send_request_header(microtcp_sock_t *_socket, const miniredis_header_t *_header_ptr);
static __always_inline status_t send_filename(microtcp_sock_t *_socket, const char *_file_name);
static __always_inline status_t send_file(microtcp_sock_t *const _socket, uint8_t *const _message_buffer, size_t _message_buffer_size,
                                          FILE *const _file_ptr, const char *const _file_name);

static __always_inline void cleanup_file_transfer_resources(FILE **const _file_ptr_address, uint8_t **const _message_buffer_address,
                                                            char **const _file_name_address, miniredis_header_t **const _header_ptr_address);

static __always_inline void cleanup_file_sending_resources(FILE **_file_ptr_address, uint8_t **_message_buffer_address,
                                                           char **_file_name_address, miniredis_header_t **_header_ptr_address);

static __always_inline void cleanup_file_receiving_resources(FILE **_file_ptr_address, uint8_t **_message_buffer_address,
                                                             char **_file_name_address, miniredis_header_t **_header_ptr_address);

static inline void prompt_to_configure_microtcp(void)
{
        const char *const prompt = "Would you like to configure MicroTCP [y/N]? ";
        char *prompt_answer_buffer = NULL;
#define DEFAULT_ANSWER 'N'
        while (true)
        {
                PROMPT_WITH_YES_NO(prompt, DEFAULT_ANSWER, prompt_answer_buffer);
                to_lowercase(prompt_answer_buffer);
                if (strcmp(prompt_answer_buffer, "yes") == 0 || strcmp(prompt_answer_buffer, "y") == 0)
                {
                        configure_microtcp_settings();
                        break;
                }
                if (strcmp(prompt_answer_buffer, "no") == 0 || strcmp(prompt_answer_buffer, "n") == 0)
                        break;
                clear_line();
        }
        free(prompt_answer_buffer);
#undef DEFAULT_ANSWER
}

static status_t miniredis_terminate_connection(microtcp_sock_t *const _utcp_socket)
{
        const _Bool shutdown_succeeded = microtcp_shutdown(_utcp_socket, SHUT_RDWR) == MICROTCP_SHUTDOWN_SUCCESS;
        microtcp_close(_utcp_socket);
        return shutdown_succeeded ? SUCCESS : FAILURE;
}

static __always_inline status_t receive_file(microtcp_sock_t *const _socket, uint8_t *const _message_buffer, const size_t _message_buffer_size,
                                             FILE *const _file_ptr, const size_t _file_size, const char *const _file_name)
{
        size_t written_bytes_count = 0;
        struct data_size file_size_data_format = get_formatted_byte_count(_file_size);
        while (written_bytes_count != _file_size)
        {
                const size_t file_part_size = MIN(_file_size - written_bytes_count, _message_buffer_size);
                struct timeval time_before_download;
                gettimeofday(&time_before_download, NULL);
                if (receive_and_write_file_part(_socket, _file_ptr, _file_name, _message_buffer, file_part_size) == FAILURE)
                        LOG_ERROR_RETURN(FAILURE, "In function `%s()`, function `%s()` returned FAILURE.", __func__, STRINGIFY(receive_and_write_file_part));
                written_bytes_count += file_part_size;
                const struct data_size received_data_size = get_formatted_byte_count(written_bytes_count);
                const struct data_size download_per_sec = get_formatted_bit_count(get_transferred_bytes_per_sec(time_before_download, file_part_size));
                LOG_APP_INFO("File: %s, %.2lf%s/%.2lf%s downloaded; Download speed: %.2lf%sps", _file_name,
                             received_data_size.size, received_data_size.unit,
                             file_size_data_format.size, file_size_data_format.unit,
                             download_per_sec.size, download_per_sec.unit);
        }
        return SUCCESS;
}

static __always_inline status_t receive_and_write_file_part(microtcp_sock_t *const _socket, FILE *const _file_ptr, const char *const _file_name,
                                                            uint8_t *const _file_buffer, const size_t _file_part_size)
{
        DEBUG_SMART_ASSERT(_socket != NULL, _file_ptr != NULL, _file_name != NULL, _file_buffer != NULL, _file_part_size < SSIZE_MAX);
        const ssize_t received_bytes = microtcp_recv_timed(_socket, _file_buffer, _file_part_size, max_response_idle_time);
        if (received_bytes == MICROTCP_RECV_FAILURE)
                LOG_APP_ERROR_RETURN(FAILURE, "Internal failure occurs on %s().", __func__);
        DEBUG_SMART_ASSERT(received_bytes >= 0, received_bytes < SSIZE_MAX);
        if (received_bytes != (ssize_t)_file_part_size)
                LOG_APP_ERROR_RETURN(FAILURE, "Response from `%s()` stalled; %zd/%zu bytes of file-part received.",
                                     STRINGIFY(microtcp_recv_timed), received_bytes, _file_part_size);
        DEBUG_SMART_ASSERT(received_bytes > 0);
        if (fwrite(_file_buffer, 1, received_bytes, _file_ptr) < (size_t)received_bytes)
                LOG_APP_ERROR_RETURN(FAILURE, "File: %s failed writing file-part, %s = %zd, errno(%d): %s.",
                                     _file_name, STRINGIFY(received_bytes), received_bytes, errno, strerror(errno));
        if (fflush(_file_ptr) != 0)
                LOG_APP_ERROR_RETURN(FAILURE, "Failed to flush file-part to disk, errno(%d): %s.", errno, strerror(errno));
        return SUCCESS;
}

static __always_inline status_t finalize_file(FILE **const _file_ptr_address, const char *const _staging_file_name, const char *const _export_file_name)
{
        DEBUG_SMART_ASSERT(_file_ptr_address != NULL);
        DEBUG_SMART_ASSERT(*_file_ptr_address != NULL);
        fclose(*_file_ptr_address);
        *_file_ptr_address = NULL;
        if (rename(_staging_file_name, _export_file_name) != 0)
                LOG_APP_ERROR_RETURN(FAILURE, "Failed renaming file from `%s` to `%s`, errno(%d): %s.",
                                     _staging_file_name, _export_file_name, errno, strerror(errno));
        return SUCCESS;
}

static __always_inline void cleanup_file_transfer_resources(FILE **const _file_ptr_address,
                                                            uint8_t **const _message_buffer_address,
                                                            char **const _file_name_address,
                                                            miniredis_header_t **const _header_ptr_address)
{
        DEBUG_SMART_ASSERT(_file_name_address != NULL,
                           _message_buffer_address != NULL,
                           _file_name_address != NULL,
                           _header_ptr_address != NULL);
        if (*_file_ptr_address != NULL)
        {
                fclose(*_file_ptr_address);
                *_file_ptr_address = NULL;
        }
        if (*_header_ptr_address != NULL)
                FREE_NULLIFY_LOG(*_header_ptr_address);
        if (*_file_name_address != NULL)
                FREE_NULLIFY_LOG(*_file_name_address);
        if (*_message_buffer_address != NULL)
                FREE_NULLIFY_LOG(*_message_buffer_address);
}

static __always_inline void cleanup_file_receiving_resources(FILE **const _file_ptr_address, uint8_t **const _message_buffer_address,
                                                             char **const _file_name_address, miniredis_header_t **const _header_ptr_address)
{
        cleanup_file_transfer_resources(_file_ptr_address, _message_buffer_address, _file_name_address, _header_ptr_address);
        if (access(STAGING_FILE_NAME, F_OK) == 0)
                if (remove(STAGING_FILE_NAME) != 0) /* We remove `staging_file_name` file. */
                        LOG_APP_ERROR("Failed to removing() %s in cleanup_stage, errno(%d): %s.", STAGING_FILE_NAME, errno, strerror(errno));
}

static __always_inline void cleanup_file_sending_resources(FILE **const _file_ptr_address, uint8_t **const _message_buffer_address,
                                                           char **const _file_name_address, miniredis_header_t **const _header_ptr_address)
{
        cleanup_file_receiving_resources(_file_ptr_address, _message_buffer_address, _file_name_address, _header_ptr_address);
}

static __always_inline status_t send_request_header(microtcp_sock_t *const _socket, const miniredis_header_t *const _header_ptr)
{
        DEBUG_SMART_ASSERT(_socket != NULL, _header_ptr != NULL);
        ssize_t send_ret_val = microtcp_send(_socket, _header_ptr, sizeof(*_header_ptr), 0);
        return send_ret_val == sizeof(*_header_ptr) ? SUCCESS : FAILURE;
}

static __always_inline status_t send_filename(microtcp_sock_t *const _socket, const char *const _file_name)
{
        DEBUG_SMART_ASSERT(_socket != NULL, _file_name != NULL);
        const size_t file_name_length = strlen(_file_name);
        DEBUG_SMART_ASSERT(file_name_length < SSIZE_MAX);
        ssize_t send_ret_val = microtcp_send(_socket, _file_name, file_name_length, 0);
        return send_ret_val == (ssize_t)file_name_length ? SUCCESS : FAILURE;
}

static __always_inline status_t send_file(microtcp_sock_t *const _socket, uint8_t *const _message_buffer, const size_t _message_buffer_size,
                                          FILE *const _file_ptr, const char *const _file_name)
{
        size_t file_bytes_sent_count = 0;
        size_t file_bytes_read;
        struct stat file_stats;
        if (stat(_file_name, &file_stats) != 0)
                LOG_APP_ERROR_RETURN(FAILURE, "File: %s failed stats-read, errno(%d): %s.", _file_name, errno, strerror(errno));

        const double _file_size_formatted = get_formatted_byte_count(file_stats.st_size).size;
        const char *_file_size_formatted_unit = get_formatted_byte_count(file_stats.st_size).unit;
        while ((file_bytes_read = fread(_message_buffer, 1, _message_buffer_size, _file_ptr)) > 0)
        {
                DEBUG_SMART_ASSERT(file_bytes_read < SSIZE_MAX);
                struct timeval time_before_upload;
                gettimeofday(&time_before_upload, NULL);

                if (microtcp_send(_socket, _message_buffer, file_bytes_read, 0) != (ssize_t)file_bytes_read)
                        LOG_APP_ERROR_RETURN(FAILURE, "microtcp_send() failed sending file-parts, aborting.");
                file_bytes_sent_count += file_bytes_read;
                const struct data_size upload_per_sec = get_formatted_bit_count(get_transferred_bytes_per_sec(time_before_upload, file_bytes_read));
                LOG_APP_INFO("File: %s, %.2lf%s/%.2lf%s uploaded; Upload speed:  %.2lf%sps", _file_name,
                             get_formatted_byte_count(file_bytes_sent_count).size, get_formatted_byte_count(file_bytes_sent_count).unit,
                             _file_size_formatted, _file_size_formatted_unit,
                             upload_per_sec.size, upload_per_sec.unit);
        }
        if (ferror(_file_ptr))
                LOG_APP_ERROR_RETURN(FAILURE, "Error occurred while reading file '%s', errno(%d): %s", _file_name, errno, strerror(errno));
        LOG_APP_INFO_RETURN(SUCCESS, "File `%s` sent.", _file_name);
}

static __always_inline FILE *open_file_for_binary_io(const char *const _file_name, const enum io_type _io_type)
{
        DEBUG_SMART_ASSERT(_file_name != NULL, _io_type == IO_READ || _io_type == IO_WRITE);
        if (_io_type == IO_READ && access(_file_name, F_OK) != 0)
                LOG_APP_ERROR_RETURN(NULL, "File: %s not found, errno(%d): %s.", _file_name, errno, strerror(errno));
        if (_io_type == IO_READ && access(_file_name, R_OK) != 0)
                LOG_APP_ERROR_RETURN(NULL, "File: %s read-permission missing, errno(%d): %s.", _file_name, errno, strerror(errno));

        const char *fopen_mode = _io_type == IO_READ ? "rb" : "wb";
        FILE *file_ptr = fopen(_file_name, fopen_mode);
        if (file_ptr == NULL)
                LOG_APP_ERROR_RETURN(NULL, "File: %s failed %s-open, errno(%d): %s.", _file_name,
                                     (_io_type == IO_READ ? "read" : "write"), errno, strerror(errno));
        return file_ptr;
}

static __always_inline uint8_t *allocate_message_buffer(const size_t _message_buffer_size)
{
        DEBUG_SMART_ASSERT(_message_buffer_size > 0);
        uint8_t *message_buffer = MALLOC_LOG(message_buffer, _message_buffer_size);
        if (message_buffer == NULL)
                LOG_APP_ERROR_RETURN(NULL, "Failed allocating message_buffer.");
        return message_buffer;
}

static void set_max_response_idle_time(const size_t _max_idle_time_multiplier)
{
#ifndef USEC_PER_SEC
#define USEC_PER_SEC 1000000
#endif /* USEC_PER_SEC */
        const struct timeval microtcp_stall_time_limit = get_microtcp_stall_time_limit();
        max_response_idle_time.tv_sec = (microtcp_stall_time_limit.tv_sec * _max_idle_time_multiplier) +
                                        (microtcp_stall_time_limit.tv_usec * _max_idle_time_multiplier) / USEC_PER_SEC;
        max_response_idle_time.tv_usec = (microtcp_stall_time_limit.tv_usec * _max_idle_time_multiplier) % USEC_PER_SEC;
        if (max_response_idle_time.tv_sec < MIN_RESPONSE_IDLE_TIME.tv_sec)
                max_response_idle_time = MIN_RESPONSE_IDLE_TIME;
}

static void create_directory(const char *const _directory_name)
{
        SMART_ASSERT(_directory_name != NULL);
        if (mkdir(_directory_name, 0755) == 0)
                LOG_APP_INFO("Directory `%s` created.", _directory_name);
        else if (errno == EEXIST)
                LOG_APP_WARNING("Failed creating directory `%s`, exists already.", _directory_name);
        else
                LOG_APP_ERROR("Failed creating directory `%s`, errno(%d): %s.", _directory_name, errno, strerror(errno));
}

#endif /* MINIREDIS_COMMON_SOURCE_CODE_H */
