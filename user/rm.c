#include<lib.h>

int main(int argc, char** argv) {
    int have_r=0,have_f=0;
    int r;
    ARGBEGIN {
	default:
		printf("usage: rm [-r] [-f] [file...]\n");
        exit(1);
	case 'r':
        have_r=1;
        break;
	case 'f':
		have_f=1;
		break;
	}
	ARGEND

     for (int i = 0; i < argc; i++) {
        char* path=argv[i];
        struct File f=((struct Filefd*)num2fd(open(path,O_TRUNC|O_RDWR)))->f_file;
    
        if(!have_r && !have_f) { 
            if(f.f_type==FTYPE_DIR){
                printf("rm: cannot remove '%s': Is a directory\n", path);
                exit(1);
            }else{
                if((r=remove(path)) == -E_NOT_FOUND) {
                    printf("rm: cannot remove '%s': No such file or directory\n", path);
                    exit(1);
                }
            }
        }else if(have_r&&!have_f) { 
            if((r=remove(path))== -E_NOT_FOUND) {
                printf("rm: cannot remove '%s': No such file or directory\n", path);
                exit(1);
            }
        }else if(have_r&&have_f){
            if((r=remove(path))== -E_NOT_FOUND) {
                exit(0);
            }
        }
     }
    return 0;
}