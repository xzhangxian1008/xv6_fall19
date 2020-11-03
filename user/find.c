#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"
#include "kernel/param.h"


#define MAX_FILES 32

char l_dir[DIRSIZ] = ".";
char p_dir[DIRSIZ] = "..";

// 0: equal
int
memcmp(const void *s1, const void *s2, uint n)
{
  const char *p1 = s1, *p2 = s2;
  while (n-- > 0) {
    if (*p1 != *p2) {
      return *p1 - *p2;
    }
    p1++;
    p2++;
  }
  return 0;
}

// 0: match !0: not match
int
is_match(char *str1, char *str2) {
	if (strlen(str1) != strlen(str2)) {
		return 1;
	}

	return memcmp(str1, str2, strlen(str1));
}

char*
get_name(char *path)
{
  static char buf[DIRSIZ+1];
  char *p;

  // Find first character after last slash.
  for(p=path+strlen(path); p >= path && *p != '/'; p--)
    ;
  p++;

  // Return blank-padded name.
  if(strlen(p) >= DIRSIZ)
    return p;
  memmove(buf, p, strlen(p));
	memset(buf+strlen(p), 0, 1);
  return buf;
}

// 0: match  !0: not match
int
find(char *path, char *target_file) {
  char buf[512], *p;
  int fd;
  struct dirent de;
  struct stat st;

  if((fd = open(path, 0)) < 0){
    fprintf(2, "find: cannot open %s\n", path);
    return 0;
  }

  if(fstat(fd, &st) < 0){
    fprintf(2, "find: cannot stat %s\n", path);
    close(fd);
    return 0;
  }

  if (st.type == T_DIR) {
    if(strlen(path) + 1 + DIRSIZ + 1 > sizeof buf){
      printf("find: path too long\n");
      return 0;
    }
    strcpy(buf, path);
    p = buf+strlen(buf);
    *p++ = '/';
    while(read(fd, &de, sizeof(de)) == sizeof(de)){
      if(de.inum == 0)
        continue;
      memmove(p, de.name, DIRSIZ);
      p[DIRSIZ] = 0;
      if(stat(buf, &st) < 0){
        printf("find: cannot stat %s\n", buf);
        continue;
      }

			// compare
			if (st.type == T_FILE) {
				char *file = get_name(buf);
				if (!is_match(file, target_file)) {
          int len = strlen(buf);
          buf[len] = '\n';
          buf[len+1] = 0;
          write(1, buf, strlen(buf));
				}
			} else if (st.type == T_DIR) {
				char *file = get_name(buf);
				if (!is_match(file, l_dir) || !is_match(file, p_dir)) {
					continue;
				}
				if (!find(buf, target_file)) {
					return 0;
				}
			}
    }
  }
  close(fd);

	return 1;
}

int
main(int argc, char *argv[])
{
  if(argc <= 2){
    printf("find: need more parameters\n");
    exit();
  }

  find(argv[1], argv[2]);

  exit();
}