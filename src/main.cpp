#include "gui.hpp"
#include "config.hpp"
#include <cstdio>
#include <csignal>
#include <cstdlib>
#include <execinfo.h>

void SystemErrorHandler(int signum)
{
    const int len=1024;
    void *func[len];
    size_t size;
    int i;
    char **funs;

    signal(signum,SIG_DFL);
    size=backtrace(func,len);
    funs=(char**)backtrace_symbols(func,size);
    fprintf(stderr,"System error, Stack trace:\n");
    for(i=0;i<size;++i) fprintf(stderr,"%d %s \n",i,funs[i]);
    free(funs);
}



int main(int argc, char *argv[])
{

	signal(SIGSEGV,SystemErrorHandler);
    signal(SIGABRT,SystemErrorHandler);

    GUI::load_settings();
    GUI::init();
    GUI::run();

    return 0;
}
