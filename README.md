# Buffered File I/O with O_PREAPPEND Flag

This project involves implementing a buffered I/O library in C that includes support for a custom flag (`O_PREAPPEND`) to enable writing to the beginning of a file without overriding its existing content. The solution wraps standard file operations (`open`, `read`, `write`) and manages read/write buffers for efficient I/O handling.

## Objective

The goal is to understand and implement buffered I/O functionalities, including:

- Efficient buffered read and write operations.
- Handling a custom `O_PREAPPEND` flag for specialized file operations.
- Maintaining synchronization between the buffer and file offsets.

---

## Implementation Details

### Buffered I/O Operations

1. **Buffered File Structure**:
   - Use the `buffered_file_t` structure to manage:
     - File descriptor.
     - Read/write buffers and their positions.
     - Buffer sizes.
     - Flags and a custom `preappend` indicator.

2. **Buffered Open**:
   - Initialize `buffered_file_t`.
   - Allocate memory for buffers.
   - Strip `O_PREAPPEND` from the flags before invoking the original `open` function.

3. **Buffered Write**:
   - Write data to the buffer.
   - Flush the buffer to the file only when it is full or when switching between read and write operations.

4. **Buffered Read**:
   - Read data from the buffer, replenishing it when empty by reading from the file descriptor.

5. **Flush**:
   - Write any pending data from the write buffer to the file to ensure consistency.

6. **Close**:
   - Flush buffers before closing the file to prevent data loss.

---

### O_PREAPPEND Flag Logic

1. **Write with O_PREAPPEND**:
   - Temporarily read the file's existing content.
   - Write new data to the beginning of the file.
   - Append the original content after the new data.

2. **Flush with O_PREAPPEND**:
   - Ensure that buffer data adheres to the preappend logic during flush operations.

3. **Reading and Writing**:
   - Flush buffers before switching between reading and writing to maintain synchronization.

---

### Buffered File Structure
The `buffered_file_t` structure includes:
- Buffers for reading and writing.
- Buffer positions and sizes.
- Flags and a `preappend` indicator.

### Functions
- `buffered_open`: Initialize and configure the buffered file.
- `buffered_write`: Write data to the buffer, handling `O_PREAPPEND` logic.
- `buffered_read`: Read data from the buffer.
- `buffered_flush`: Ensure all pending writes are applied to the file.
- `buffered_close`: Flush buffers and close the file descriptor.

### Synchronization
- Maintain proper transitions between reading and writing.
- Ensure that file offsets are updated correctly.

---
