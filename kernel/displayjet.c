#include <kernel/displayjet.h>
#include <kernel/kernel.h>
#include <kernel/memory.h>
#include <kernel/string.h>
#include <kernel/admin.h>
#include <kernel/error.h>

/* DisplayJet state */
static displayjet_session_t *sessions = NULL;
static displayjet_access_request_t *pending_requests = NULL;
static displayjet_access_key_t *active_keys = NULL;

static uint32_t next_buffer_id = 1;
static uint64_t next_request_id = 1;
static uint64_t next_key_id = 1;
static uint64_t total_buffers = 0;
static uint64_t total_access_requests = 0;
static uint64_t access_granted = 0;
static uint64_t access_denied = 0;

/* Simple AES-256 encryption simulation (for demonstration) */
/* In production, use a real AES implementation */
static void displayjet_xor_encrypt(void *data, size_t size, const uint8_t *key) {
    if (!data || !key || size == 0) return;
    
    uint8_t *bytes = (uint8_t*)data;
    for (size_t i = 0; i < size; i++) {
        bytes[i] ^= key[i % DISPLAYJET_KEY_SIZE];
    }
}

/* Generate cryptographic key */
static void displayjet_generate_key(uint8_t *key) {
    if (!key) return;
    
    /* Generate pseudo-random key based on timestamps and counters */
    /* In production, use a proper CSPRNG */
    for (int i = 0; i < DISPLAYJET_KEY_SIZE; i++) {
        key[i] = (uint8_t)((next_key_id * 17 + i * 31 + total_access_requests) & 0xFF);
    }
}

/* Initialize DisplayJet subsystem */
void displayjet_init(void) {
    sessions = NULL;
    pending_requests = NULL;
    active_keys = NULL;
    
    next_buffer_id = 1;
    next_request_id = 1;
    next_key_id = 1;
    
    total_buffers = 0;
    total_access_requests = 0;
    access_granted = 0;
    access_denied = 0;
    
    kprintf("DisplayJet: Zero-Trust Display Server initialized\n");
    kprintf("            - Encrypted buffer allocation enabled\n");
    kprintf("            - One-time access keys enabled\n");
    kprintf("            - Kernel-level access control active\n");
    kprintf("            - Secure user prompt tunnel ready\n");
}

/* Find session by PID */
static displayjet_session_t *displayjet_find_session(pid_t pid) {
    displayjet_session_t *session = sessions;
    while (session) {
        if (session->pid == pid) return session;
        session = session->next;
    }
    return NULL;
}

/* Create display session */
displayjet_session_t *displayjet_create_session(pid_t pid) {
    displayjet_session_t *session = displayjet_find_session(pid);
    if (session) {
        kprintf("DisplayJet: Session already exists for PID %d\n", pid);
        return session;
    }
    
    session = (displayjet_session_t*)kmalloc(sizeof(displayjet_session_t));
    if (!session) {
        error_report("DisplayJet", E_NOMEM, "Failed to allocate session");
        return NULL;
    }
    
    session->pid = pid;
    session->buffer_count = 0;
    session->buffers = NULL;
    session->flags = 0;
    session->next = sessions;
    sessions = session;
    
    kprintf("DisplayJet: Created session for PID %d\n", pid);
    return session;
}

/* Destroy display session */
void displayjet_destroy_session(pid_t pid) {
    displayjet_session_t *session = sessions;
    displayjet_session_t *prev = NULL;
    
    while (session) {
        if (session->pid == pid) {
            /* Free all buffers */
            displayjet_buffer_t *buffer = session->buffers;
            while (buffer) {
                displayjet_buffer_t *next = buffer->next;
                if (buffer->encrypted_data) kfree(buffer->encrypted_data);
                if (buffer->decrypted_data) kfree(buffer->decrypted_data);
                kfree(buffer);
                buffer = next;
            }
            
            /* Remove from list */
            if (prev) {
                prev->next = session->next;
            } else {
                sessions = session->next;
            }
            
            kfree(session);
            kprintf("DisplayJet: Destroyed session for PID %d\n", pid);
            return;
        }
        prev = session;
        session = session->next;
    }
}

/* Allocate encrypted display buffer */
uint32_t displayjet_allocate_buffer(pid_t pid, size_t width, size_t height, 
                                    uint32_t format, uint32_t flags) {
    displayjet_session_t *session = displayjet_find_session(pid);
    if (!session) {
        session = displayjet_create_session(pid);
        if (!session) return 0;
    }
    
    if (session->buffer_count >= DISPLAYJET_MAX_BUFFERS) {
        error_report("DisplayJet", E_NOSPACE, "Maximum buffers reached");
        return 0;
    }
    
    /* Calculate buffer size based on format */
    size_t bytes_per_pixel = 4; /* Default RGBA8888 */
    switch (format) {
        case DISPLAYJET_FORMAT_RGB565: bytes_per_pixel = 2; break;
        case DISPLAYJET_FORMAT_RGB888: bytes_per_pixel = 3; break;
        case DISPLAYJET_FORMAT_RGBA8888:
        case DISPLAYJET_FORMAT_ARGB8888: bytes_per_pixel = 4; break;
    }
    
    size_t size = width * height * bytes_per_pixel;
    
    /* Allocate buffer structure */
    displayjet_buffer_t *buffer = (displayjet_buffer_t*)kmalloc(sizeof(displayjet_buffer_t));
    if (!buffer) {
        error_report("DisplayJet", E_NOMEM, "Failed to allocate buffer structure");
        return 0;
    }
    
    /* Allocate encrypted memory */
    buffer->encrypted_data = kmalloc(size);
    if (!buffer->encrypted_data) {
        kfree(buffer);
        error_report("DisplayJet", E_NOMEM, "Failed to allocate buffer memory");
        return 0;
    }
    
    buffer->decrypted_data = NULL; /* Only decrypt on access */
    buffer->buffer_id = next_buffer_id++;
    buffer->owner_pid = pid;
    buffer->width = width;
    buffer->height = height;
    buffer->format = format;
    buffer->flags = flags | DISPLAYJET_FLAG_ENCRYPTED;
    buffer->size = size;
    buffer->timestamp = total_buffers++;
    buffer->access_count = 0;
    
    /* Generate encryption key */
    displayjet_generate_key(buffer->encryption_key);
    
    /* Clear buffer memory */
    memset(buffer->encrypted_data, 0, size);
    
    /* Add to session */
    buffer->next = session->buffers;
    session->buffers = buffer;
    session->buffer_count++;
    
    kprintf("DisplayJet: Allocated buffer %u for PID %d (%lux%lu, %lu bytes)\n",
            buffer->buffer_id, pid, (unsigned long)width, (unsigned long)height, 
            (unsigned long)size);
    
    return buffer->buffer_id;
}

/* Find buffer by ID */
static displayjet_buffer_t *displayjet_find_buffer(pid_t pid, uint32_t buffer_id) {
    displayjet_session_t *session = displayjet_find_session(pid);
    if (!session) return NULL;
    
    displayjet_buffer_t *buffer = session->buffers;
    while (buffer) {
        if (buffer->buffer_id == buffer_id) return buffer;
        buffer = buffer->next;
    }
    return NULL;
}

/* Free display buffer */
int displayjet_free_buffer(pid_t pid, uint32_t buffer_id) {
    displayjet_session_t *session = displayjet_find_session(pid);
    if (!session) {
        error_set_last(E_NOENT);
        return E_NOENT;
    }
    
    displayjet_buffer_t *buffer = session->buffers;
    displayjet_buffer_t *prev = NULL;
    
    while (buffer) {
        if (buffer->buffer_id == buffer_id) {
            /* Remove from list */
            if (prev) {
                prev->next = buffer->next;
            } else {
                session->buffers = buffer->next;
            }
            
            /* Free memory */
            if (buffer->encrypted_data) kfree(buffer->encrypted_data);
            if (buffer->decrypted_data) kfree(buffer->decrypted_data);
            kfree(buffer);
            
            session->buffer_count--;
            kprintf("DisplayJet: Freed buffer %u from PID %d\n", buffer_id, pid);
            return E_SUCCESS;
        }
        prev = buffer;
        buffer = buffer->next;
    }
    
    error_set_last(E_NOENT);
    return E_NOENT;
}

/* Update buffer content */
int displayjet_update_buffer(pid_t pid, uint32_t buffer_id, 
                            const void *data, size_t size) {
    displayjet_buffer_t *buffer = displayjet_find_buffer(pid, buffer_id);
    if (!buffer) {
        error_set_last(E_NOENT);
        return E_NOENT;
    }
    
    if (!data || size == 0 || size > buffer->size) {
        error_set_last(E_INVAL);
        return E_INVAL;
    }
    
    /* Copy data to encrypted buffer */
    memcpy(buffer->encrypted_data, data, size);
    
    /* Encrypt the data */
    displayjet_xor_encrypt(buffer->encrypted_data, size, buffer->encryption_key);
    
    return E_SUCCESS;
}

/* Encrypt buffer */
void displayjet_encrypt_buffer(displayjet_buffer_t *buffer) {
    if (!buffer || !buffer->encrypted_data) return;
    displayjet_xor_encrypt(buffer->encrypted_data, buffer->size, buffer->encryption_key);
}

/* Decrypt buffer */
void displayjet_decrypt_buffer(displayjet_buffer_t *buffer) {
    if (!buffer || !buffer->encrypted_data) return;
    displayjet_xor_encrypt(buffer->encrypted_data, buffer->size, buffer->encryption_key);
}

/* Request access to another process's buffer */
uint64_t displayjet_request_access(pid_t requester_pid, pid_t owner_pid, 
                                   uint32_t buffer_id, const char *reason) {
    /* Verify buffer exists */
    displayjet_buffer_t *buffer = displayjet_find_buffer(owner_pid, buffer_id);
    if (!buffer) {
        error_report("DisplayJet", E_NOENT, "Buffer not found");
        return 0;
    }
    
    /* Create access request */
    displayjet_access_request_t *request = 
        (displayjet_access_request_t*)kmalloc(sizeof(displayjet_access_request_t));
    if (!request) {
        error_report("DisplayJet", E_NOMEM, "Failed to create access request");
        return 0;
    }
    
    request->request_id = next_request_id++;
    request->requester_pid = requester_pid;
    request->owner_pid = owner_pid;
    request->buffer_id = buffer_id;
    request->reason = reason;
    request->timestamp = total_access_requests++;
    request->status = DISPLAYJET_ACCESS_PENDING;
    request->next = pending_requests;
    pending_requests = request;
    
    kprintf("DisplayJet: Access request %lu from PID %d to PID %d's buffer %u\n",
            (unsigned long)request->request_id, requester_pid, owner_pid, buffer_id);
    kprintf("            Reason: %s\n", reason ? reason : "(no reason)");
    
    /* Prompt user through secure tunnel */
    displayjet_prompt_user(request);
    
    return request->request_id;
}

/* Prompt user for access decision (secure tunnel) */
int displayjet_prompt_user(displayjet_access_request_t *request) {
    if (!request) return E_INVAL;
    
    char prompt[256];
    kprintf("\n");
    kprintf("╔════════════════════════════════════════════════════════════╗\n");
    kprintf("║         DISPLAYJET SECURITY PROMPT                        ║\n");
    kprintf("╠════════════════════════════════════════════════════════════╣\n");
    kprintf("║ Process %d wants to access display buffer %u              \n", 
            request->requester_pid, request->buffer_id);
    kprintf("║ owned by Process %d                                       \n", 
            request->owner_pid);
    if (request->reason) {
        kprintf("║ Reason: %-50s║\n", request->reason);
    }
    kprintf("╠════════════════════════════════════════════════════════════╣\n");
    kprintf("║ This grants ONE-TIME access to encrypted content          ║\n");
    kprintf("║ Access key will be valid for single use only              ║\n");
    kprintf("╠════════════════════════════════════════════════════════════╣\n");
    kprintf("║ Allow access? (Kernel will wait for response)             ║\n");
    kprintf("║ [Y] Yes, grant one-time access                            ║\n");
    kprintf("║ [N] No, deny access                                       ║\n");
    kprintf("╚════════════════════════════════════════════════════════════╝\n");
    
    /* In a real implementation, this would wait for secure input */
    /* For now, auto-approve for demonstration */
    kprintf("DisplayJet: [AUTO-APPROVED FOR DEMO] User response: YES\n\n");
    
    return E_SUCCESS;
}

/* Grant access and generate one-time key */
displayjet_access_key_t *displayjet_grant_access(uint64_t request_id) {
    /* Find request */
    displayjet_access_request_t *request = pending_requests;
    while (request) {
        if (request->request_id == request_id) break;
        request = request->next;
    }
    
    if (!request) {
        error_report("DisplayJet", E_NOENT, "Request not found");
        return NULL;
    }
    
    /* Verify buffer still exists */
    displayjet_buffer_t *buffer = displayjet_find_buffer(request->owner_pid, 
                                                         request->buffer_id);
    if (!buffer) {
        error_report("DisplayJet", E_NOENT, "Buffer no longer exists");
        return NULL;
    }
    
    /* Create one-time access key */
    displayjet_access_key_t *key = 
        (displayjet_access_key_t*)kmalloc(sizeof(displayjet_access_key_t));
    if (!key) {
        error_report("DisplayJet", E_NOMEM, "Failed to create access key");
        return NULL;
    }
    
    /* Generate unique one-time key */
    displayjet_generate_key(key->key);
    key->key_id = next_key_id++;
    key->requester_pid = request->requester_pid;
    key->owner_pid = request->owner_pid;
    key->buffer_id = request->buffer_id;
    key->timestamp = buffer->access_count++;
    key->used = 0;
    key->next = active_keys;
    active_keys = key;
    
    /* Update request status */
    request->status = DISPLAYJET_ACCESS_GRANTED;
    access_granted++;
    
    kprintf("DisplayJet: Granted one-time access key %lu to PID %d\n",
            (unsigned long)key->key_id, request->requester_pid);
    kprintf("            Key valid for single use only\n");
    
    return key;
}

/* Verify one-time key */
int displayjet_verify_key(uint64_t key_id, const uint8_t *provided_key) {
    if (!provided_key) return 0;
    
    displayjet_access_key_t *key = active_keys;
    while (key) {
        if (key->key_id == key_id && !key->used) {
            /* Verify key matches */
            if (memcmp(key->key, provided_key, DISPLAYJET_KEY_SIZE) == 0) {
                return 1;
            }
        }
        key = key->next;
    }
    return 0;
}

/* Decrypt buffer using one-time key */
void *displayjet_decrypt_buffer_once(displayjet_access_key_t *access_key) {
    if (!access_key || access_key->used) {
        error_report("DisplayJet", E_PERM, "Invalid or expired access key");
        return NULL;
    }
    
    /* Find buffer */
    displayjet_buffer_t *buffer = displayjet_find_buffer(access_key->owner_pid,
                                                         access_key->buffer_id);
    if (!buffer) {
        error_report("DisplayJet", E_NOENT, "Buffer not found");
        return NULL;
    }
    
    /* Allocate temporary decrypted buffer */
    if (!buffer->decrypted_data) {
        buffer->decrypted_data = kmalloc(buffer->size);
        if (!buffer->decrypted_data) {
            error_report("DisplayJet", E_NOMEM, "Failed to allocate decrypt buffer");
            return NULL;
        }
    }
    
    /* Decrypt content */
    memcpy(buffer->decrypted_data, buffer->encrypted_data, buffer->size);
    displayjet_xor_encrypt(buffer->decrypted_data, buffer->size, buffer->encryption_key);
    
    /* Mark key as used (one-time only!) */
    access_key->used = 1;
    
    kprintf("DisplayJet: Decrypted buffer %u for PID %d (ONE-TIME)\n",
            buffer->buffer_id, access_key->requester_pid);
    kprintf("            Access key %lu is now INVALID\n", 
            (unsigned long)access_key->key_id);
    
    return buffer->decrypted_data;
}

/* Revoke access key */
void displayjet_revoke_key(uint64_t key_id) {
    displayjet_access_key_t *key = active_keys;
    displayjet_access_key_t *prev = NULL;
    
    while (key) {
        if (key->key_id == key_id) {
            if (prev) {
                prev->next = key->next;
            } else {
                active_keys = key->next;
            }
            kfree(key);
            kprintf("DisplayJet: Revoked access key %lu\n", (unsigned long)key_id);
            return;
        }
        prev = key;
        key = key->next;
    }
}

/* Print DisplayJet status */
void displayjet_print_status(void) {
    kprintf("\n");
    kprintf("╔════════════════════════════════════════════════════════════╗\n");
    kprintf("║         DISPLAYJET STATUS                                  ║\n");
    kprintf("╠════════════════════════════════════════════════════════════╣\n");
    kprintf("║ Total Buffers:           %lu\n", (unsigned long)total_buffers);
    kprintf("║ Access Requests:         %lu\n", (unsigned long)total_access_requests);
    kprintf("║ Access Granted:          %lu\n", (unsigned long)access_granted);
    kprintf("║ Access Denied:           %lu\n", (unsigned long)access_denied);
    kprintf("╠════════════════════════════════════════════════════════════╣\n");
    
    /* Count active sessions */
    uint32_t session_count = 0;
    displayjet_session_t *session = sessions;
    while (session) {
        session_count++;
        session = session->next;
    }
    kprintf("║ Active Sessions:         %u\n", session_count);
    
    /* Count active keys */
    uint32_t key_count = 0;
    displayjet_access_key_t *key = active_keys;
    while (key) {
        if (!key->used) key_count++;
        key = key->next;
    }
    kprintf("║ Valid One-Time Keys:     %u\n", key_count);
    kprintf("╚════════════════════════════════════════════════════════════╝\n\n");
}

/* Get buffer count for PID */
uint32_t displayjet_get_buffer_count(pid_t pid) {
    displayjet_session_t *session = displayjet_find_session(pid);
    return session ? session->buffer_count : 0;
}

/* Get access count for buffer */
uint32_t displayjet_get_access_count(pid_t pid, uint32_t buffer_id) {
    displayjet_buffer_t *buffer = displayjet_find_buffer(pid, buffer_id);
    return buffer ? buffer->access_count : 0;
}

/* Secure tunnel functions (placeholder for real implementation) */
int displayjet_secure_tunnel_send(const char *message) {
    if (!message) return E_INVAL;
    /* In production: Send through encrypted kernel<->user tunnel */
    kprintf("[SECURE_TUNNEL] >> %s\n", message);
    return E_SUCCESS;
}

int displayjet_secure_tunnel_receive(char *response, size_t size) {
    if (!response || size == 0) return E_INVAL;
    /* In production: Receive through encrypted kernel<->user tunnel */
    strlcpy(response, "YES", size);
    return E_SUCCESS;
}
