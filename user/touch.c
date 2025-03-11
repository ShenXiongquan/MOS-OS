#include<lib.h>

int main(int argc, char** argv) {
    if(argc == 2) {
        int r = create(argv[1], FTYPE_REG);
        // 如果文件已存在
        if(r == -E_FILE_EXISTS) 
            exit(0);
        else if(r == -E_NOT_FOUND) {
            // 如果父目录不存在
            printf("touch: cannot touch '%s': No such file or directory\n", argv[1]);
            exit(1);
        }
    }
    else{
        // 参数错误
        printf("usage: touch [filename]\n");
        exit(1);
    }
    return 0;
}