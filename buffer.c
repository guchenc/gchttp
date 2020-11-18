#include "buffer.h"

const char* CRLF = "\r\n";

struct buffer* buffer_new()
{
    struct buffer* buff = malloc(sizeof(struct buffer));
    if (buff == NULL)
        goto failed;

    buff->size = INIT_BUFFER_SIZE + CHEAP_PREPEND_SIZE;
    buff->data = malloc(buff->size);
    if (buff->data == NULL)
        goto failed;

    buff->readIdx = CHEAP_PREPEND_SIZE;
    buff->writeIdx = CHEAP_PREPEND_SIZE;

    return buff;

failed:
    if (buff->data != NULL)
        free(buff->data);
    if (buff != NULL)
        free(buff);
    return NULL;
}

size_t buffer_readable_size(struct buffer* buff)
{
    return buff->writeIdx - buff->readIdx;
}

size_t buffer_writeable_size(struct buffer* buff)
{
    return buff->size - buff->writeIdx;
}

size_t buffer_prependable_size(struct buffer* buff)
{
    return buff->readIdx;
}

static void ensure_space(struct buffer* buff, size_t need)
{
    size_t writeableSize = buffer_writeable_size(buff);
    if (writeableSize >= need) // back space enough
        return;

    size_t readableSize = buffer_readable_size(buff);
    size_t prependableSize = buffer_prependable_size(buff);

    if (writeableSize + prependableSize >= need) { // avaliable space enough, trigger move copy
        memcpy(buff->data + CHEAP_PREPEND_SIZE, buff + buff->readIdx, readableSize);
        buff->readIdx = CHEAP_PREPEND_SIZE;
        buff->writeIdx = buff->readIdx + readableSize;
    } else { // avaliable space not enough, trigger realloc.
        // TODO: how about combine prependable space in this step
        size_t nsize = buff->size;
        while (nsize < buff->size + need) nsize <<= 1;
        LOG(LT_INFO, "buffer size incresing from %zu to %zu", buff->size, nsize);
        char* tmp = realloc(buff->data, nsize);
        assert(tmp != NULL);
        buff->data = tmp;
        buff->size = nsize;
    }
}

size_t buffer_append(struct buffer* buff, const void* data, size_t size)
{
    if (data == NULL)
        return 0;
    ensure_space(buff, size);
    memcpy(&buff->data[buff->writeIdx], data, size);
    buff->writeIdx += size;
    return size;
}

size_t buffer_append_char(struct buffer* buff, char c)
{
    ensure_space(buff, 1);
    buff->data[buff->writeIdx++] = c;
    return 1;
}

size_t buffer_append_string(struct buffer* buff, const char* str)
{
    if (str == NULL)
        return 0;
    size_t len = strlen(str);
    /* ensure_space(buff, len);
     * strncpy(&buff->data[buff->writeIdx], str, len); // exclude '\0'
     * buff->writeIdx += len; */
    buffer_append(buff, str, len); // exclude '\0'
    return len;
}

char* buffer_find_CRLF(struct buffer* buff)
{
    char* crlf = memmem(&buff->data[buff->readIdx], buffer_readable_size(buff), CRLF, 2);
    return crlf;
}

char buffer_read_char(struct buffer* buff)
{
    char c = buff->data[buff->readIdx++];
    if (buff->readIdx == buff->writeIdx) { // no readable bytes, reset readIdx writeIdx to front
        buff->readIdx = CHEAP_PREPEND_SIZE;
        buff->writeIdx = CHEAP_PREPEND_SIZE;
    }
    return c;
}

ssize_t buffer_read_fd(struct buffer* buff, int fd)
{
    assert(fd > 0);
    char stackBuffer[INIT_BUFFER_SIZE]; // NOTE: temporary stack buffer struct iovec vec[2];
    size_t writeableSize = buffer_writeable_size(buff);
    struct iovec vec[2];
    vec[0].iov_base = &buff->data[buff->writeIdx];
    vec[0].iov_len = writeableSize;
    vec[1].iov_base = stackBuffer;
    vec[1].iov_len = sizeof(stackBuffer);
    ssize_t n = readv(fd, vec, 2);
    if (n < 0) {
        LOG(LT_ERROR, "failed to readv fd %d, %s", fd, strerror(errno));
        return -1;
    } else if (n <= writeableSize) { // read no bytes to tmpBuffer
        buff->writeIdx += n;
    } else { // read n - writeableSize bytes to tmpBuffer
        buff->writeIdx = buff->size;
        buffer_append(buff, stackBuffer, n - writeableSize);
    }
    return n;
}

void buffer_cleanup(struct buffer* buff)
{
    if (buff->data != NULL)
        free(buff->data);
    if (buff != NULL)
        free(buff);
}
