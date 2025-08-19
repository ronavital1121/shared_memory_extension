#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/memlayout.h"

#define NCHILD 4
#define MSG_COUNT 10
#define MAX_MSG_LEN 64
#define SHARED_PAGE_SIZE 4096

extern void* map_shared_pages(int src_pid, int dst_pid, void* addr, int npages);

uint32 encode_header(uint16 index, uint16 len) {
  return ((uint32)index << 16) | len;
}

void decode_header(uint32 header, uint16* index, uint16* len) {
  *index = header >> 16;
  *len = header & 0xFFFF;
}

void int_to_str(int val, char* buf, int* len) {
  char tmp[16];
  int i = 0;
  if (val == 0) {
    buf[0] = '0';
    *len = 1;
    return;
  }
  while (val > 0) {
    tmp[i++] = '0' + (val % 10);
    val /= 10;
  }
  for (int j = 0; j < i; j++)
    buf[j] = tmp[i - 1 - j];
  *len = i;
}

int format_msg(char* buf, int index, int msg_num) {
  const char* prefix = "Child ";
  const char* mid = ": message ";
  int pos = 0;

  for (int i = 0; prefix[i]; i++)
    buf[pos++] = prefix[i];

  int len;
  char temp[16];
  int_to_str(index, temp, &len);
  for (int i = 0; i < len; i++)
    buf[pos++] = temp[i];

  for (int i = 0; mid[i]; i++)
    buf[pos++] = mid[i];

  int len2;
  int_to_str(msg_num, temp, &len2);
  for (int i = 0; i < len2; i++)
    buf[pos++] = temp[i];

  return pos;
}

void writer(void* shmem, int index) {
  uint8* ptr = (uint8*)shmem;

  for (int i = 0; i < MSG_COUNT; i++) {
    char msg[MAX_MSG_LEN];
    int len = format_msg(msg, index, i);

    uint8* cur = ptr;
    while (1) {
      if ((uint64)(cur - ptr) + len + 4 > SHARED_PAGE_SIZE)
        exit(0);  // Out of space

      uint32* header = (uint32*)cur;
      uint32 old = __sync_val_compare_and_swap(header, 0, encode_header(index, (uint16)len));
      if (old == 0) {
        memcpy(cur + 4, msg, len);
        break;
      } else {
        uint16 skip_len;
        decode_header(old, 0, &skip_len);
        cur += 4 + skip_len;
        cur = (uint8*)(((uint64)cur + 3) & ~3);
      }
    }

    sleep(10);
  }

  exit(0);
}

void reader(void* shmem) {
  uint8* cur = (uint8*)shmem;
  uint8* end = cur + SHARED_PAGE_SIZE;

  while (cur + 4 <= end) {
    uint32 header = *(uint32*)cur;
    if (header == 0) break;

    uint16 index, len;
    decode_header(header, &index, &len);

    if (cur + 4 + len > end) break;

    char msg[MAX_MSG_LEN + 1];
    memcpy(msg, cur + 4, len);
    msg[len] = '\0';

    printf("[Child %d] %s\n", index, msg);

    cur += 4 + len;
    cur = (uint8*)(((uint64)cur + 3) & ~3);
  }
}

int main(int argc, char* argv[]) {
  // Allocate a shared buffer from parent ordinary memory
  // Let's allocate an aligned page-sized buffer
  // Use sbrk to allocate at least 4096 bytes
  void* shared_buf = (void*)sbrk(SHARED_PAGE_SIZE);
  if ((uint64)shared_buf % SHARED_PAGE_SIZE != 0) {
    // Align to next page boundary if needed (optional)
    shared_buf = (void*)(((uint64)shared_buf + SHARED_PAGE_SIZE - 1) & ~(SHARED_PAGE_SIZE - 1));
  }

  if (!shared_buf) {
    printf("Failed to allocate shared buffer\n");
    exit(1);
  }

  // Zero the buffer before sharing (optional)
  memset(shared_buf, 0, SHARED_PAGE_SIZE);

  int parent_pid = getpid();

  for (int i = 0; i < NCHILD; i++) {
    int pid = fork();
    if (pid < 0) {
      printf("fork failed\n");
      exit(1);
    }

    if (pid == 0) {
      // Child maps shared buffer from parent
      void* mapped = map_shared_pages(getpid(), parent_pid, shared_buf, 1);
      if (!mapped) {
        printf("Child %d: failed to map shared memory\n", i);
        exit(1);
      }
      writer(mapped, i);
    } else {
      // Parent maps shared buffer to child
      if (!map_shared_pages(parent_pid, pid, shared_buf, 1)) {
        printf("Parent: failed to map to child %d\n", pid);
        exit(1);
      }
    }
  }

  for (int i = 0; i < NCHILD; i++) {
    wait(0);
  }

  printf("---- LOG OUTPUT ----\n");
  reader(shared_buf);

  exit(0);
}
