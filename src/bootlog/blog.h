/* boot console capture buffer, shared by bootlog.c (writer, C:\AUTO) and bootdump.c (reader, XaAES autorun) */
#define BLOG_MAGIC 0x424c4f47L                     /* 'BLOG' */
#define BLOG_CAP 65536L
struct blog { long magic, len, cap; char data[BLOG_CAP]; };
