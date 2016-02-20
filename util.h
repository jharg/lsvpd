void dump(const void *buf, int len);
const char *_rstripspc(const char *v);
const char *_stripspc(const char *v);
const char *_basename(const char *path);
void _scandir(const char *path, int (*fn)(const char *, void *), void *arg);
void physmemcpy(void *dest, uint32_t src, int len);
int physmemcmp(void *dest, uint32_t src, int len);
uint32_t smscan(uint32_t start, uint32_t end, int klen, void *key, int step);
