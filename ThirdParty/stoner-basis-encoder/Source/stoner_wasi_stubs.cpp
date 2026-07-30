#include <wasi/api.h>

#include <cstddef>
#include <cstdint>
#include <cstring>

extern "C"
{

__wasi_errno_t __imported_wasi_snapshot_preview1_fd_close(__wasi_fd_t)
{
    return __WASI_ERRNO_SUCCESS;
}

__wasi_errno_t __imported_wasi_snapshot_preview1_fd_fdstat_get(
    __wasi_fd_t,
    __wasi_fdstat_t*)
{
    return __WASI_ERRNO_BADF;
}

__wasi_errno_t __imported_wasi_snapshot_preview1_fd_prestat_get(
    __wasi_fd_t,
    __wasi_prestat_t*)
{
    return __WASI_ERRNO_BADF;
}

__wasi_errno_t __imported_wasi_snapshot_preview1_fd_prestat_dir_name(
    __wasi_fd_t,
    std::uint8_t*,
    __wasi_size_t)
{
    return __WASI_ERRNO_BADF;
}

__wasi_errno_t __imported_wasi_snapshot_preview1_fd_read(
    __wasi_fd_t,
    const __wasi_iovec_t*,
    std::size_t,
    __wasi_size_t* BytesRead)
{
    if (BytesRead)
    {
        *BytesRead = 0;
    }
    return __WASI_ERRNO_SUCCESS;
}

__wasi_errno_t __imported_wasi_snapshot_preview1_fd_seek(
    __wasi_fd_t,
    __wasi_filedelta_t,
    __wasi_whence_t,
    __wasi_filesize_t*)
{
    return __WASI_ERRNO_BADF;
}

__wasi_errno_t __imported_wasi_snapshot_preview1_fd_write(
    __wasi_fd_t,
    const __wasi_ciovec_t* Buffers,
    std::size_t BufferCount,
    __wasi_size_t* BytesWritten)
{
    __wasi_size_t Total = 0;
    for (std::size_t Index = 0; Index < BufferCount; ++Index)
    {
        Total += Buffers[Index].buf_len;
    }
    if (BytesWritten)
    {
        *BytesWritten = Total;
    }
    return __WASI_ERRNO_SUCCESS;
}

__wasi_errno_t __imported_wasi_snapshot_preview1_path_open(
    __wasi_fd_t,
    __wasi_lookupflags_t,
    const char*,
    __wasi_size_t,
    __wasi_oflags_t,
    __wasi_rights_t,
    __wasi_rights_t,
    __wasi_fdflags_t,
    __wasi_fd_t*)
{
    return __WASI_ERRNO_NOTCAPABLE;
}

void __imported_wasi_snapshot_preview1_proc_exit(__wasi_exitcode_t)
{
    __builtin_trap();
}

__wasi_errno_t __imported_wasi_snapshot_preview1_random_get(
    std::uint8_t* Buffer,
    __wasi_size_t Length)
{
    if (Buffer)
    {
        std::memset(Buffer, 0xA5, Length);
    }
    return __WASI_ERRNO_SUCCESS;
}

} // extern "C"
