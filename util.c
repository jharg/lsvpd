#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <dirent.h>
#include <sys/queue.h>
#include <inttypes.h>

/*=======================*
 * Utility
 *=======================*/
void dump(const void *buf, int len)
{
  int i, j, c;
  const uint8_t *pb = buf;
  
  for (i=0; i<len; i+=16) {
    printf("%.5x ", i);
    for (j=0; j<16; j++) {
      printf("%.2x ", pb[i+j]);
    }
    printf("  ");
    for (j=0; j<16; j++) {
      c = pb[i+j];
      if (i+j > len)
	c = 'Z';
      printf("%c", (c < ' ' || c > 'z') ? '.' : c);
    }
    printf("\n");
  }
}

const char *_rstripspc(const char *v)
{
  int l;

  l = strlen(v);
  while (l > 0 && v[l-1] == ' ')
    l--;
  return v+l;
}

const char *_stripspc(const char *v)
{
  while (*v == ' ')
    v++;
  return v;
}

const char *_basename(const char *path)
{
  char *r;

  if ((r = strrchr(path, '/')) != NULL)
    return r+1;
  return path;
}

void _scandir(const char *path, int (*fn)(const char *, void *), void *arg)
{
  char vpath[PATH_MAX];
  DIR *dir;
  struct dirent *de;

  if ((dir = opendir(path)) == NULL)
    return;
  while ((de = readdir(dir)) != NULL) {
    if (!strcmp(de->d_name, ".") ||
	!strcmp(de->d_name, ".."))
      continue;
    snprintf(vpath, sizeof(vpath), "%s/%s", path, de->d_name);
    fn(vpath, arg);
  }
  closedir(dir);
}

void physmemcpy(void *dest, uint32_t src, int len)
{
  int fd;

  memset(dest, 0, len);
  if ((fd = open("/dev/mem", O_RDONLY)) >= 0) {
    pread(fd, dest, len, src);
    close(fd);
  }
}

int physmemcmp(void *dest, uint32_t src, int len)
{
  void *buf;

  buf = alloca(len);
  physmemcpy(buf, src, len);
  return memcmp(dest, buf, len);
}

uint32_t smscan(uint32_t start, uint32_t end, int klen, void *key, int step)
{
  while (start < end) {
    if (!physmemcmp(key, start, klen))
      return start;
    start += step;
  }
  return 0;
}
