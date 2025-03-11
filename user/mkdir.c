#include<lib.h>

int create_dir(char *path) {
    char *end;
    int r;

    for(;*path=='/';path++);
    end = path;
    
    for(;*end&&*end!='/';end++);

    if (*end) {
        *end = 0; 
        if ((r = create_dir(path)) < 0) return r;
        *end = '/'; 
    }

    // 创建当前文件
    if ((r = create(path, FTYPE_DIR)) < 0 && r != -E_FILE_EXISTS) {
        return r;
    }
    return 0;
}

int main(int argc, char **argv) {
    int have_p=0;
	ARGBEGIN {
        case 'p':
            have_p=1;
            break;
        default:
            printf("usage: mkdir [-p] [dir...]\n");
            exit(1);
	}
	ARGEND


	if (argc == 0) 
    {
        printf("usage: mkdir [-p] [dir...]\n");
        exit(1);
    }else{
         for (int i = 0; i < argc; i++) {
            int r = create(argv[i], FTYPE_DIR);
            if (r == -E_FILE_EXISTS) { //目录已经存在
                if (!have_p) {
                    printf("mkdir: cannot create directory '%s': File exists\n", argv[i]);
                    exit(1);
                }
            }else if (r == -E_NOT_FOUND) { //父目录不存在
                if (have_p) {
                    r = create_dir(argv[i]);
                    if(r== -E_FILE_EXISTS) exit(0);
                }
                else { 
                    printf("mkdir: cannot create directory '%s': No such file or directory\n", argv[i]);
                    exit(1);
                }
            } 
        }
    }
	return 0;
}


