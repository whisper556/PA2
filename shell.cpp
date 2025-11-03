#include <iostream>

#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <vector>
#include <string>
#include <cstring>

#include <cstdlib>

#include <ctime>
#include <climits> 

#include "Tokenizer.h"
#include <fcntl.h> // For O_RDONLY, O_WRONLY, O_CREAT, O_TRUNC
// all the basic colours for a shell prompt
#define RED     "\033[1;31m"
#define GREEN	"\033[1;32m"
#define YELLOW  "\033[1;33m"
#define BLUE	"\033[1;34m"
#define WHITE	"\033[1;37m"
#define NC      "\033[0m"

using namespace std;


string trim (const string in) {
    int i = in.find_first_not_of(" \n\r\t");
    int j = in.find_last_not_of(" \n\r\t");

    if (i >= 0 && j >= i) {
        return in.substr(i, j-i+1);
    }
    return ""; // Return an empty string if it's all whitespace
}


int main () {
    
     char path_buf[PATH_MAX + 1];
     if(getcwd(path_buf, sizeof(path_buf)) == NULL ) {
	     perror("getcwd (initial) ");
	     exit(1);
     }
     
     string old_pwd = path_buf;

     for (;;) {
        // need date/time, username, and absolute path to current dir
           time_t now = time(nullptr);
        struct tm* tstruct = localtime(&now);
        char time_buf[80];
        strftime(time_buf, sizeof(time_buf), "%b %d %H:%M:%S", tstruct);

        // --- Get current working directory ---
        char path_buf[PATH_MAX + 1];
        if (getcwd(path_buf, sizeof(path_buf)) == nullptr) {
            strncpy(path_buf, "/?", sizeof(path_buf));
        }
        path_buf[PATH_MAX] = '\0';
        string path = path_buf; // Full path only

        // --- Determine prompt symbol ---
        string prompt_symbol = (geteuid() == 0) ? "#" : "$";
		
		const char* user = getenv("USER");
        string username = (user != nullptr) ? user : "user";


        // --- Print prompt ---
        cout << WHITE  << time_buf << " "
       << GREEN  << username << NC << ":"   // print username
       << BLUE   << path << NC
       << prompt_symbol << " ";


        // get user inputted command
        string input;
        getline(cin, input);
        
	if(trim(input).empty()){
		continue;
	}


        if (input == "exit") {  // print exit message and break out of infinite loop
            cout << RED << "Now exiting shell..." << endl << "Goodbye" << NC << endl;
            break;
        }

        // get tokenized commands from user input
        Tokenizer tknr(input);
        if (tknr.hasError()) {  // continue to next prompt if input had an error
            continue;
        }

	if (tknr.commands.empty() || tknr.commands.at(0)->args.empty()) {
       		 continue; // User just hit enter, show new prompt
        }
        // // print out every command token-by-token on individual lines
        // // prints to cerr to avoid influencing autograder
        // for (auto cmd : tknr.commands) {
        //     for (auto str : cmd->args) {
        //         cerr << "|" << str << "| ";
        //     }
        //     if (cmd->hasInput()) {
        //         cerr << "in< " << cmd->in_file << " ";
        //     }
        //     if (cmd->hasOutput()) {
        //         cerr << "out> " << cmd->out_file << " ";
        //     }
        //     cerr << endl;
        // }
         
	int num_cmds = tknr.commands.size();

	if(num_cmds == 1) {

	    Command* cmd = tknr.commands.at(0);
       if (cmd->args.at(0) == "cd") {
    char current_path[PATH_MAX + 1];
    if (getcwd(current_path, sizeof(current_path)) == NULL) {
        perror("getcwd");
        continue;
    }

    string target_path;

    // "cd -" case
    if (cmd->args.size() > 1 && cmd->args.at(1) == "-") {
        const char* oldpwd_env = getenv("OLDPWD");

        if (oldpwd_env == nullptr) {
            cerr << "bash: cd: OLDPWD not set" << endl;
            continue;
        }

        // Update OLDPWD before changing directory
        setenv("OLDPWD", current_path, 1);
        old_pwd = current_path;

        if (chdir(oldpwd_env) < 0) {
            perror("chdir");
        } else {
            // Print *new* working directory (absolute path only)
            char new_path[PATH_MAX + 1];
            if (getcwd(new_path, sizeof(new_path)) != NULL) {
                cout << new_path << endl << flush;
            }
        }
        continue;
    }

    // "cd" or "cd <path>"
    if (cmd->args.size() > 1) {
        target_path = cmd->args.at(1);
    } else {
        const char* home_dir = getenv("HOME");
        if (home_dir == nullptr) {
            cerr << "Error: HOME environment variable not set." << endl;
            continue;
        }
        target_path = home_dir;
    }

    if (chdir(target_path.c_str()) < 0) {
        perror("chdir");
    } else {
        setenv("OLDPWD", current_path, 1);
        old_pwd = current_path;
    }

    continue;
}

                // fork to create child

		cerr << "DEBUG: CMD:";
for (auto& arg : cmd->args) cerr << " " << arg;
if (cmd->hasInput()) cerr << " < " << cmd->in_file;
if (cmd->hasOutput()) cerr << " > " << cmd->out_file;
cerr << endl;

		pid_t pid = fork();
		if (pid < 0) {  // error check
		    perror("fork");
		    exit(2);
		}

		if (pid == 0) {  // if child, exec to run command
		    // run single commands with no arguments

		    Command* cmd = tknr.commands.at(0);
		    //char* args[] = {(char*) tknr.commands.at(0)->args.at(0).c_str(), nullptr};
		   

		   if(cmd->hasInput()){
			  int fd_in = open(cmd->in_file.c_str(), O_RDONLY);

			 if(fd_in < 0){
				perror("open (input)");
				exit(3);
			}

			dup2(fd_in, STDIN_FILENO);
			
			close(fd_in);
		   }

if (cmd->hasOutput()) {
    if (cmd->out_file.empty()) {
        cerr << "Error: output redirection with no file specified\n";
        exit(4);
    }
    int fd_out = open(cmd->out_file.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd_out < 0) {
        perror("open(output)");
        exit(4);
    }
    dup2(fd_out, STDOUT_FILENO);
    close(fd_out);
}


		    vector<char*> args;
		    for(const string& s : cmd->args){
			    args.push_back(const_cast<char*>(s.c_str()));
		    } 

		    args.push_back(nullptr);

		    if (execvp(args[0], args.data()) < 0) {  // error check
			perror("execvp");
			exit(2);
		    }
		}
		else {  // if parent, wait for child to finish
		  if(!cmd->isBackground()){
			 int status = 0;
			 waitpid(pid, &status,0); 
			 if(status > 1){
				 exit(status);
			}
		  }
		}

    }

	else if(num_cmds > 1){
		vector<pid_t> child_pids;
		int in_fd = STDIN_FILENO;
		int p[2];

		for(int i = 0;i<num_cmds;i++){
			Command* cmd = tknr.commands.at(i);

			if( i < num_cmds - 1){
				if(pipe(p) < 0){
					perror("pipe");
					exit(5);
				}
			}

			pid_t pid = fork();

			if(pid < 0){
				perror("fork");
				exit(6);
			}

			if(pid == 0){

				if(in_fd != STDIN_FILENO){
					dup2(in_fd, STDIN_FILENO);
					close(in_fd);
				}

				if(i == 0 && cmd->hasInput()){
					int fd_in = open(cmd->in_file.c_str(), O_RDONLY);
					if(fd_in < 0){ perror("open(input)"); exit(3);}
					dup2(fd_in, STDIN_FILENO);
					close(fd_in);
				}

				if(i< num_cmds - 1){
					dup2(p[1], STDOUT_FILENO);
					close(p[0]);
					close(p[1]);
				} else {

				    if(cmd->hasOutput()){
					    int fd_out = open(cmd->out_file.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
					    if(fd_out < 0){ perror("open(output)"); exit(4);}
					    dup2(fd_out, STDOUT_FILENO);
					    close(fd_out);
				    }

			      }

			vector<char*> args_cstr;
			for(const string& s : cmd->args){
				args_cstr.push_back(const_cast<char*>(s.c_str()));
			}

			args_cstr.push_back(nullptr);
			if(execvp(args_cstr[0], args_cstr.data()) < 0 ) {
				perror("execvp");
				exit(7);
			}
		      } else {

			 if(in_fd != STDIN_FILENO) {
				 close(in_fd);
			}

			 if(i < num_cmds - 1){
				 close(p[1]);
				 in_fd = p[0];
			}

			child_pids.push_back(pid);
		}
	    }

		if(in_fd != STDIN_FILENO){
			close(in_fd);
		}


		Command* last_cmd = tknr.commands.at(num_cmds - 1);
		if(!last_cmd->isBackground()){
			for(pid_t pid : child_pids){
				int status;
				waitpid(pid, &status,0);
			}
		}


	 }





    }
}
