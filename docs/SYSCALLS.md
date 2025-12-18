# System Calls

Zirvium implements a UNIX-like system call interface. System calls provide the interface between user space programs and the kernel.

## Calling Convention

System calls use the following registers:
- **RAX**: System call number
- **RDI**: Argument 1
- **RSI**: Argument 2
- **RDX**: Argument 3
- **R10**: Argument 4
- **R8**: Argument 5
- **R9**: Argument 6

Return value is in **RAX**.

## Available System Calls

### File Operations

#### sys_read (0)
Read from a file descriptor.
```c
ssize_t read(int fd, void *buf, size_t count);
```
- **fd**: File descriptor
- **buf**: Buffer to read into
- **count**: Number of bytes to read
- **Returns**: Number of bytes read, or -1 on error

#### sys_write (1)
Write to a file descriptor.
```c
ssize_t write(int fd, const void *buf, size_t count);
```
- **fd**: File descriptor (1=stdout, 2=stderr)
- **buf**: Buffer to write from
- **count**: Number of bytes to write
- **Returns**: Number of bytes written, or -1 on error

#### sys_open (2)
Open a file.
```c
int open(const char *path, int flags);
```
- **path**: Path to file
- **flags**: O_RDONLY, O_WRONLY, O_RDWR, O_CREAT
- **Returns**: File descriptor, or -1 on error

#### sys_close (3)
Close a file descriptor.
```c
int close(int fd);
```
- **fd**: File descriptor
- **Returns**: 0 on success, -1 on error

### Process Management

#### sys_fork (4)
Create a new process.
```c
pid_t fork(void);
```
- **Returns**: 0 in child, child PID in parent, -1 on error

#### sys_exec (5)
Execute a program.
```c
int exec(const char *path, char *const argv[]);
```
- **path**: Path to executable
- **argv**: Argument vector
- **Returns**: -1 on error (does not return on success)

#### sys_exit (6)
Terminate the calling process.
```c
void exit(int status);
```
- **status**: Exit status code

#### sys_getpid (7)
Get process ID.
```c
pid_t getpid(void);
```
- **Returns**: Current process ID

#### sys_kill (8)
Send signal to a process.
```c
int kill(pid_t pid, int sig);
```
- **pid**: Process ID
- **sig**: Signal number
- **Returns**: 0 on success, -1 on error

## Error Codes

System calls return -1 on error. Error codes:
- **EBADF**: Bad file descriptor
- **EINVAL**: Invalid argument
- **ENOMEM**: Out of memory
- **ENOENT**: No such file or directory
- **EPERM**: Operation not permitted

## Example Usage

```c
// Write to stdout
const char *msg = "Hello, World!\n";
write(1, msg, 14);

// Create a new process
pid_t pid = fork();
if (pid == 0) {
    // Child process
    exit(0);
} else if (pid > 0) {
    // Parent process
}

// Get current process ID
pid_t my_pid = getpid();
```
