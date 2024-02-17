======================================================================
SYNOPSIS
    #include <sys/ipc.h>
    
    key_t ftok(const char* path, int id);

DESCRIPTION
    The ftok() function attempts to create a unique key suitable for use with
    the semget(2), and shmget(2) functions, given the path of an existing
    file and a user-selectable id.

    The specified path must specify an existing file that is accessible to
    the calling process or the call will fail. Also, note that links to
    files will return the same key, given the same id.

RETURN VALUES
    The ftok() function will return -1 if path does not exist or if it cannot
    be accessed by the calling process.

BUGS
    The returned key is computed based on the device minor number and inode
    of the specified path, in combination with the lower 8 bits of the given
    id. Thus, it is quite possible for the routine to return duplicate keys.
======================================================================

======================================================================
SYNOPSIS
    #include <sys/shm.h>

    int shmget(key_t key, size_t size, int shmflg);

DESCRIPTION
    shmget() returns the shared memory identifier associated with the key
    `key`.
    
    A shared memory segment is created if either key is equal to IPC_PRIVATE,
    or key does not have a shared memory segment identifier associated with
    it, and the IPC_CREAT bit is set in shmflg.

    If a new shared memory segment is created, the data structure associated
    with it (the shmid_ds structure, see shmctl(2)) is initialized as
    follows:

    •   shm_perm.cuid and shm_perm.uid are set to the effective uid of the
        calling process.
    •   shm_perm.gid and shm_perm.cgid are set to the effective gid of the
        calling process.
    •   shm_perm.mode is set to the lower 9 bits of shmflg.
    •   shm_lpid, shm_nattch, shm_atime, and shm_dtime are set to 0
    •   shm_ctime is set to the current time.
    •   shm_segsz is set to the value of size.
    •   The ftok(3) function may be used to generate a key from a pathname.

RETURN VALUES
    Upon successful completion a positive shared memory segment identifier is
    returned.  Otherwise, -1 is returned and the global variable errno is set
    to indicate the error.

ERRORS
    The shmget() system call will fail if:

    [EACCES]            A shared memory segment is already associated with key
                        and the caller has no permission to access it.

    [EEXIST]            Both IPC_CREAT and IPC_EXCL are set in shmflg, and a
                        shared memory segment is already associated with key.

    [EINVAL]            No shared memory segment is to be created, and a
                        shared memory segment exists for key, but the size of
                        the segment associated with it is less than size,
                        which is non-zero.

    [ENOENT]            IPC_CREAT was not set in shmflg and no shared memory
                        segment associated with key was found.
    
    [ENOMEM]            There is not enough memory left to created a shared
                        memory segment of the requested size.

    [ENOSPC]            A new shared memory identifier could not be created
                        because the system limit for the number of shared
                        memory identifiers has been reached.
======================================================================

======================================================================
SYNOPSIS
    #include <sys/shm.h>

    void* shmat(int shmid, const void *shmaddr, int shmflg);
    int shmdt(const void *shmaddr);

DESCRIPTION
    shmat() maps the shared memory segment associated with the shared memory identifier shmid into
    the address space of the calling process. The address at which the segment is mapped is
    determined by the shmaddr parameter. If it is equal to 0, the system will pick an address
    itself. Otherwise, an attempt is made to map the shared memory segment at the address shmaddr
    specifies. If SHM_RND is set in shmflg, the system will round the address down to a multiple of
    SHMLBA bytes (SHMLBA is defined in ⟨sys/shm.h⟩ ). A shared memory segment can be mapped read-
    only by specifying the SHM_RDONLY flag in shmflg.
    shmdt() unmaps the shared memory segment that is currently mapped at shmaddr from the calling process' address space. shmaddr must be a value returned by a prior shmat() call. A shared memory segment will remain existant until it is removed by a call to shmctl(2) with the IPC_RMID command.

RETURN VALUES
    shmat() returns the address at which the shared memory segment has been mapped into the calling
    process' address space when successful, shmdt() returns 0 on successful completion. Otherwise, a
    value of -1 is returned, and the global variable errno is set to indicate the error.

ERRORS
    The shmat() system call will fail if:

    [EACCES]            The calling process has no permission to access this shared memory segment.
    [EINVAL]            shmid is not a valid shared memory identifier.  shmaddr specifies an illegal
                        address.
    [EMFILE]            The number of shared memory segments has reached the system-wide limit.
    [ENOMEM]            There is not enough available data space for the calling process to map the
                        shared memory segment.

    The shmdt() system call will fail if:
    [EINVAL]            shmaddr is not the start address of a mapped shared memory segment.
======================================================================
