#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

#define SIZE 120

extern void* map_shared_pages(int src_pid, int dst_pid, void* src_va, int size);
extern int unmap_shared_pages(int pid, void* addr, int size);

int
main(int argc, char *argv[]) {
    int disable_unmap = 0;
    if (argc > 1 && strcmp(argv[1], "--no-unmap") == 0) {
        disable_unmap = 1;
    }

    int parentPid = getpid();
    char* pointer = malloc(SIZE);  // Memory to share

    int childPid = fork();
    if (childPid == 0) {
        // === Child Process ===
        int myPid = getpid();

        void* size_before = sbrk(0);
        printf("Child: Size before mapping: %p\n", size_before);

        sleep(3); // Wait for parent to write to shared memory

        char* sharedMem = map_shared_pages(parentPid, myPid, pointer, SIZE);
        if (sharedMem == (char *)-1) {
            printf("Child: map_shared_pages failed\n");
            exit(1);
        }

        void* size_after_map = sbrk(0);
        printf("Child: Size after mapping: %p\n", size_after_map);

        // Write to shared memory
        strcpy(sharedMem, "Hello daddy");

        if (!disable_unmap) {
            if (unmap_shared_pages(myPid, sharedMem, SIZE) == -1) {
                printf("Child: unmap_shared_pages failed\n");
                exit(1);
            }

            void* size_after_unmap = sbrk(0);
            printf("Child: Size after unmapping: %p\n", size_after_unmap);

            // Compare sizes
            if (size_after_unmap == size_before) {
                printf("Child: Memory size returned to original \n");
            } else {
                printf("Child: Memory size mismatch after unmap \n");
            }

            // Allocate again to verify memory is usable
            void* test_ptr = malloc(SIZE);
            void* size_after_malloc = sbrk(0);
            printf("Child: Size after malloc: %p\n", size_after_malloc);
            free(test_ptr);
        } else {
            printf("Child: Skipping unmap (testing kernel cleanup after exit)\n");
        }

        exit(0);

    } else {
        // === Parent Process ===
        strcpy(pointer, "Initial parent value");

        void* parent_before = sbrk(0);
        printf("Parent: Before child mapping: %p\n", parent_before);

        wait(0); // Wait for child to finish

            printf("Parent: Reads from shared memory: %s\n", pointer);

        void* parent_after = sbrk(0);
        printf("Parent: After child exit: %p\n", parent_after);

        // You could also print difference for clarity:
        printf("Parent: Memory diff after child: %ld bytes\n", (uint64)parent_after - (uint64)parent_before);

        printf("Parent: Skipping unmap_shared_pages (not mapped by parent)\n");
    }

    exit(0);
}
