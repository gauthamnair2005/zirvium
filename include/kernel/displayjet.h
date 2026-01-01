#ifndef KERNEL_DISPLAYJET_H
#define KERNEL_DISPLAYJET_H

#include <kernel/types.h>
#include <kernel/process.h>

/* DisplayJet - Zero-Trust Secure Display Server */

/* Display buffer flags */
#define DISPLAYJET_FLAG_ENCRYPTED   0x0001
#define DISPLAYJET_FLAG_PROTECTED   0x0002
#define DISPLAYJET_FLAG_COMPOSITING 0x0004
#define DISPLAYJET_FLAG_VISIBLE     0x0008
#define DISPLAYJET_FLAG_SHARED      0x0010

/* Display buffer format */
#define DISPLAYJET_FORMAT_RGB888    0
#define DISPLAYJET_FORMAT_RGBA8888  1
#define DISPLAYJET_FORMAT_RGB565    2
#define DISPLAYJET_FORMAT_ARGB8888  3

/* Access request result */
#define DISPLAYJET_ACCESS_DENIED    0
#define DISPLAYJET_ACCESS_GRANTED   1
#define DISPLAYJET_ACCESS_PENDING   2

/* Encryption key size (AES-256) */
#define DISPLAYJET_KEY_SIZE         32

/* Maximum display buffers per process */
#define DISPLAYJET_MAX_BUFFERS      16

/* Forward declarations */
typedef struct displayjet_buffer displayjet_buffer_t;
typedef struct displayjet_session displayjet_session_t;
typedef struct displayjet_access_key displayjet_access_key_t;
typedef struct displayjet_access_request displayjet_access_request_t;

/* One-time access key */
struct displayjet_access_key {
    uint8_t key[DISPLAYJET_KEY_SIZE];
    uint64_t key_id;
    pid_t requester_pid;
    pid_t owner_pid;
    uint32_t buffer_id;
    uint64_t timestamp;
    uint32_t used;
    struct displayjet_access_key *next;
};

/* Display buffer (encrypted memory region) */
struct displayjet_buffer {
    uint32_t buffer_id;
    pid_t owner_pid;
    void *encrypted_data;
    void *decrypted_data;
    uint8_t encryption_key[DISPLAYJET_KEY_SIZE];
    size_t width;
    size_t height;
    uint32_t format;
    uint32_t flags;
    size_t size;
    uint64_t timestamp;
    uint32_t access_count;
    struct displayjet_buffer *next;
};

/* Display session per process */
struct displayjet_session {
    pid_t pid;
    uint32_t buffer_count;
    displayjet_buffer_t *buffers;
    uint32_t flags;
    struct displayjet_session *next;
};

/* Access request structure */
struct displayjet_access_request {
    uint64_t request_id;
    pid_t requester_pid;
    pid_t owner_pid;
    uint32_t buffer_id;
    const char *reason;
    uint64_t timestamp;
    uint32_t status;
    struct displayjet_access_request *next;
};

/* DisplayJet subsystem API */
void displayjet_init(void);

/* Session management */
displayjet_session_t *displayjet_create_session(pid_t pid);
void displayjet_destroy_session(pid_t pid);

/* Buffer management */
uint32_t displayjet_allocate_buffer(pid_t pid, size_t width, size_t height, 
                                    uint32_t format, uint32_t flags);
int displayjet_free_buffer(pid_t pid, uint32_t buffer_id);
int displayjet_update_buffer(pid_t pid, uint32_t buffer_id, 
                            const void *data, size_t size);

/* Access control (zero-trust) */
uint64_t displayjet_request_access(pid_t requester_pid, pid_t owner_pid, 
                                   uint32_t buffer_id, const char *reason);
int displayjet_prompt_user(displayjet_access_request_t *request);
displayjet_access_key_t *displayjet_grant_access(uint64_t request_id);
int displayjet_verify_key(uint64_t key_id, const uint8_t *key);
void *displayjet_decrypt_buffer_once(displayjet_access_key_t *access_key);
void displayjet_revoke_key(uint64_t key_id);

/* Encryption/Decryption */
void displayjet_encrypt_buffer(displayjet_buffer_t *buffer);
void displayjet_decrypt_buffer(displayjet_buffer_t *buffer);

/* Secure tunnel for user prompts */
int displayjet_secure_tunnel_send(const char *message);
int displayjet_secure_tunnel_receive(char *response, size_t size);

/* Statistics and monitoring */
void displayjet_print_status(void);
uint32_t displayjet_get_buffer_count(pid_t pid);
uint32_t displayjet_get_access_count(pid_t pid, uint32_t buffer_id);

#endif
