#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"
#include "kernel/fcntl.h"

/*
	find <start-path> <filename>
*/
void find(char* start_path, char* filename) {
	char buf[512], *p;
	int fd;
	struct dirent de;
	struct stat st;

	if((fd = open(start_path, O_RDONLY)) < 0){
		fprintf(2, "find: cannot open %s\n", start_path);
		return;
	}

	if(fstat(fd, &st) < 0){
		fprintf(2, "ls: cannot stat %s\n", start_path);
		close(fd);
		return;
	}

	switch(st.type){
	case T_FILE:
		// 如果当前是文件
		fprintf(2, "Usage: find dir file\n");
		exit(1);

	case T_DIR:
		if(strlen(start_path) + 1 + DIRSIZ + 1 > sizeof buf){
			printf("find: path too long\n");
			break;
		}
		strcpy(buf, start_path);
		p = buf+strlen(buf);
		*p++ = '/';
		// 遍历该目录下所有的项目
		while(read(fd, &de, sizeof(de)) == sizeof(de)){
			// 忽略"."和".."
			if(de.inum == 0 || strcmp(de.name, ".") == 0 || strcmp(de.name, "..") == 0)
				continue;
			// 拼接子目录/文件的完整路径
			memmove(p, de.name, DIRSIZ);
			p[DIRSIZ] = 0;
			if(stat(buf, &st) < 0){
				printf("find: cannot stat %s\n", buf);
				continue;
			}
		
			// 分情况讨论
			// 如果是目录，就接着递归
			if (st.type == T_DIR) {
				find(buf, filename);
			} else if (st.type == T_FILE) {
				if (strcmp(filename, de.name) == 0) {
					printf("%s\n", buf);
				}
			}
		}
		break;
	}
	close(fd);
}

int main(int argc, char* argv[]) {
	if (argc < 3) {
		fprintf(2, "Usage: find dir file\n");
		exit(1);
	}
	find(argv[1], argv[2]);
	return 0;
}
