#include "../updater.hpp"
#include <cstdio>
#include <string>
int main(int argc,char** argv){
 if(argc!=3)return 2;
 if(!verify_update_file(argv[1],argv[2])){std::fprintf(stderr,"Valid update was rejected.\n");return 1;}
 std::string wrong=argv[2];wrong[0]=wrong[0]=='0'?'1':'0';
 if(verify_update_file(argv[1],wrong)){std::fprintf(stderr,"Invalid digest was accepted.\n");return 1;}
 std::puts("Update executable format and SHA-256 verification passed.");return 0;
}
