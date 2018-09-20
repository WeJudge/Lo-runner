#include "special_judge.h"
#include <sys/resource.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>
#include "limit.h"

const char *last_run_err;
#define RAISE_RUN(err) {last_run_err = err;return -1;}


int special_checker(struct Runobj *runobj, struct Result *rst) {
	pid_t pid;
	int fd_err[2];
	if (pipe2(fd_err, O_NONBLOCK))
		RAISE1("run :pipe2(fd_err) failure");

	pid = vfork();
	#define RAISE_EXIT(err) {int r = write(fd_err[1],err,strlen(err));_exit(r);}
	
	if (pid < 0) {
		close(fd_err[0]);
		close(fd_err[1]);
		RAISE1("run : vfork failure");
	}
	else if (pid == 0) {
		// child thread
		close(fd_err[0]);
		
		if (runobj->fd_in != -1)
			if (dup2(runobj->fd_in, STDIN_FILENO) == -1)
				RAISE_EXIT("dup2 stdin failure!")
		
		if (runobj->fd_out != -1)
			if (dup2(runobj->fd_out, STDOUT_FILENO) == -1)
				RAISE_EXIT("dup2 stdin failure!")
	
		if (runobj->fd_err != -1)
		    if (dup2(runobj->fd_err, STDERR_FILENO) == -1)
			RAISE_EXIT("dup2 stderr failure")
			
		if (spj_reslimit(runobj) == -1)
			RAISE_EXIT(last_limit_err)
			
		execvp(runobj->special_judge_checker[0], (char * const *) runobj->special_judge_checker);

		RAISE_EXIT("execvp failure")
	}else{
		int r;
		char errbuffer[100] = { 0 };

		close(fd_err[1]);
		r = read(fd_err[0], errbuffer, 90);
		close(fd_err[0]);
		if (r > 0) {
			waitpid(pid, NULL, WNOHANG);
			RAISE(errbuffer);
			return -1;
		}

		int status;
		int rel;
		waitpid(pid, &status, WUNTRACED);
		if (WIFSIGNALED(status)) {
			switch (WTERMSIG(status)) {
				case SIGXCPU:
				case SIGALRM:
					rst->judge_result = RE;		//SPJERR1
					break;
				case SIGKILL:
					rst->judge_result = RE;    //SPJERR1
					break;
				default:
					rst->judge_result = RE;		// SPJERR2
					break;
			}
		}else{
			rel = WEXITSTATUS(status);
			switch (rel) {
				case CHECKER_AC:
					rst->judge_result = AC;
					break;
				case CHECKER_PE:
					rst->judge_result = PE;
					break;
				case CHECKER_WA:
					rst->judge_result = WA;
					break;
				case CHECKER_OLE:
					rst->judge_result = OLE;
					break;
				case CHECK_CONVERTED:
					rst->judge_result = SPJFIN;
					break;
				default:
					rst->judge_result = SPJERR2;
					break;
			}
		}
		return 0;
	}
}

int interactive_judge(struct Runobj *runobj, struct Result *rst){
	pid_t pid1, pid2;
	int fd_err[2];
	int fdjudge[2], fdtarget[2];
	if (pipe2(fd_err, O_NONBLOCK))
		RAISE1("run :pipe2(fd_err) failure");

		// create two pipe for interactive
		if(pipe(fdjudge) < 0){
			RAISE1("run : pipe(fdjudge) failure");
		}
		if(pipe(fdtarget) < 0){
			RAISE1("run : pipe(fdtarget) failure");
		}
		

	pid1 = vfork();
	if (pid1 < 0) {
		close(fd_err[0]);
		close(fd_err[1]);
		RAISE1("run : vfork failure");
	}

	if (pid1 == 0) {
		close(0);
		close(1);
		if(dup2(fdtarget[0], STDIN_FILENO) < 0)	{
			RAISE_EXIT("dup2 stderr failure");
		}
		if(dup2(fdjudge[1], STDOUT_FILENO) < 0){
			RAISE_EXIT("dup2 stderr failure");
		}
		if (setResLimit(runobj) == -1)
			RAISE_EXIT(last_limit_err);
		// fork target program thread
		execvp(runobj->args[0], (char * const *) runobj->args);
		RAISE_EXIT("execvp failure");
	}else{
		pid2 = vfork();
		if (pid2 < 0) {
			RAISE1("run : vfork failure");
		} 
		else if(pid2 == 0){
			// fork judger program thread
			close(0);
			close(1);
			if(dup2(fdjudge[0], STDIN_FILENO) < 0)	{
				RAISE_EXIT("dup2 stderr failure");
			}
			if(dup2(fdtarget[1], STDOUT_FILENO) < 0){
				RAISE_EXIT("dup2 stderr failure");
			}
			
			if (spji_reslimit(runobj) == -1)
				RAISE_EXIT(last_limit_err);
				
			execvp(runobj->special_judge_checker[0], (char * const *) runobj->special_judge_checker);
			RAISE_EXIT("execvp failure");
		} else {
			int status;
			struct rusage ru;
			int rel;
			// for judger
			if(wait4(pid2, &status, WUNTRACED, &ru) < 0)
				RAISE_RUN("wait4 failure");
			if (WIFSIGNALED(status)) {
				switch (WTERMSIG(status)) {
					case SIGXCPU:
					case SIGALRM:
						rst->judge_result = RE;
						break;
					case SIGKILL:
						rst->judge_result = RE;
						break;
					default:
						rst->judge_result = RE;
						break;
				}
			}else{
				rel = WEXITSTATUS(status);
				switch (rel) {
					case CHECKER_AC:
						rst->judge_result = AC;
						break;
					case CHECKER_PE:
						rst->judge_result = PE;
						break;
					case CHECKER_WA:
						rst->judge_result = WA;
						break;
					case CHECKER_OLE:
						rst->judge_result = OLE;
						break;
//					case CHECK_CONVERTED:
//						rst->judge_result = SPJFIN;
//						break;
					default:
						rst->judge_result = RE;
						break;
				}
			}
//			if(rst->judge_result != SPJFIN)
//				return 0;

//
//			// for target
//			if(wait4(pid1, &status, WUNTRACED, &ru) < 0)
//				RAISE_RUN("wait4 failure");
//			rst->time_used = ru.ru_utime.tv_sec * 1000
//					+ ru.ru_utime.tv_usec / 1000
//					+ ru.ru_stime.tv_sec * 1000
//					+ ru.ru_stime.tv_usec / 1000;    
//			
//			rst->memory_used = ru.ru_minflt * (sysconf(_SC_PAGESIZE) / 1024);
//			
//			if (WIFSIGNALED(status)) {
//				switch (WTERMSIG(status)) {
//					case SIGSEGV:
//						if (rst->memory_used > runobj->memory_limit)
//							 rst->judge_result = MLE;
//						else
//							 rst->judge_result = RE;
//						break;
//					case SIGALRM:
//					case SIGVTALRM:
//					case SIGXCPU:
//						rst->judge_result = TLE;
//						break;
//					case SIGKILL:
//						if(rst->time_used > (runobj->time_limit - 100)){
//							rst->judge_result = TLE;
//						}else{
//							rst->judge_result = MLE;
//						}
//						break;
//					default:
//						rst->judge_result = RE;
//						break;
//				}
//				rst->re_signum = WTERMSIG(status);
//				kill(pid2, 9);		// Stop judger
//			} else {
//				if (rst->time_used > runobj->time_limit)
//					rst->judge_result = TLE;
//				else if (rst->memory_used > runobj->memory_limit)
//					rst->judge_result = MLE;
//				else
//					rst->judge_result = SPJFIN;
//			}	

		}
	}
	return 0;
}