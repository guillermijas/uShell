/**
 *
 *   Guillermo Mora Cordero, 2º Informática A, üshell.
 *
 **/

#include "job_control.h"
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <termios.h>
#include <unistd.h>
#define MAX_LINE 256 

job *lista;
historial *hist;

/*Prototipos de los métodos*/

void my_sigchld(int signum);
void put_in_foreground(job *j, int stopped);
void put_job_in_background (job *j);
void stop_job_in_background(job *j);
void kill_job(job *j);
void modificar_job(job *lista, job * j, enum job_state modifState);
void add_proceso_listaTrabajo(job *nuevo, job *lista);


/*Métodos*/

void my_sigchld(int signum){

	int i, istatus, info, pid_wait;
	enum status status_res;
	job* jb, aux;

	printf("SIGCHLD recibido\n");
	for(i = 1; i<=list_size(lista); i++){
		jb = get_item_bypos(lista, i);
		pid_wait = waitpid(jb->pgid, &istatus, WUNTRACED | WNOHANG);

		if(pid_wait == jb->pgid){
			status_res = analyze_status(istatus, &info);
			printf("Wait realizado para trabajo en background: %s, Estado: %s\n", jb->command, status_strings[status_res]);
			block_SIGCHLD();
			if (status_res == SUSPENDED)
				modificar_job(lista, jb, STOPPED);
			else if (status_res == EXITED || status_res == SIGNALED){
				if(jb->state == RESPAWNABLE){
					/*pedir ayuda con esto*/printf("Entra aqui, deberia abrirse otra ventana\n");
				}else{
					delete_job(lista, jb);
					i--;
				}
		    }
			unblock_SIGCHLD();
			printf("%c[%d;%dm\nüsh > %c[%dm",27,1,32,27,0);
		}//print_job_list(lista); //-> debug
	}

	fflush(stdout);
	return;
}

void put_job_in_background (job *j){
	/* Enviar SIGCONT */
	block_SIGCHLD();
	if (killpg(j->pgid, SIGCONT))
		perror ("kill (SIGCONT)");
	block_SIGCHLD();
}

void stop_job_in_background(job *j){
	/* Enviar SIGSTOP */
	block_SIGCHLD();
	if (killpg(j->pgid, SIGSTOP))
		perror ("kill (SIGSTOP)");
	block_SIGCHLD();
}

void kill_job(job *j){
	/* Enviar SIGKILL */
	block_SIGCHLD();
	if (killpg(j->pgid, SIGKILL))
		perror ("kill (SIGKILL)");
	unblock_SIGCHLD();
}

void modificar_job(job *lista, job * j, enum job_state modifState){
	block_SIGCHLD();
	j->state = modifState;
	modify_job(lista, j);
	unblock_SIGCHLD();
}

void put_job_in_foreground(job *j){
	int pid_fork;
	int estado, infor;
	enum status status_res;
	job *nuevo, *aux;

	pid_fork = j->pgid;
	if(j->state==STOPPED)
		put_job_in_background(j);

	/* Dar terminal al hijo */
	set_terminal(pid_fork);

	/* Padre espera SIGCHLD */
	waitpid(j->pgid, &estado, WUNTRACED);
	ignore_terminal_signals();
	set_terminal(getpid()); /*Recupera por el ignore_signals, que incluyen SIGTTIN y SIGTTOU*/

	status_res = analyze_status(estado ,&infor);
	printf("Foreground pid: %d, Orden: %s, Estado: %s\n", pid_fork, j->command, status_strings[status_res]);

	/*Sacarlo de la lista si EXITED o SIGNALED, si se suspende, dejarlo modificado*/
	block_SIGCHLD();
	aux = get_item_bypid(lista, pid_fork);
	if (status_res == SUSPENDED)
		modificar_job(lista, aux, STOPPED);
	if (status_res == EXITED || status_res == SIGNALED)
		delete_job(lista, aux);
	unblock_SIGCHLD();
}

void add_proceso_listaTrabajo(job *nuevo, job *lista){
	block_SIGCHLD();
	add_job(lista, nuevo);
	unblock_SIGCHLD();
	//print_job_list(lista); //debug
	//printf("%d\n",list_size(lista)); //debug
}

ejecutarProceso(int pid, int background, int respawn, char *args[MAX_LINE/2], char inputBuffer[MAX_LINE]){
    new_process_group(pid);
    restore_terminal_signals();
    if(!background && !respawn)
	    set_terminal(getpid());
	
    execvp(args[0], args);
    printf("Error, comando desconocido: %s. Fallo en execv\n", inputBuffer);
    exit(EXIT_FAILURE);
}

// -----------------------------------------------------------------------
//                            MAIN
// -----------------------------------------------------------------------

int main(void){
	char inputBuffer[MAX_LINE]; /* buffer to hold the command entered */
	int background;             /* equals 1 if a command is followed by '&' */
	int respawn;         /* equals 1 if a command is followed by '#' */
	char *args[MAX_LINE/2];     /* command line (of 256) has max of 128 arguments */
	int pid_fork, pid_wait;     /* pid for created and waited process */
	int status;                 /* status returned by wait */
	enum status status_res;     /* status processed by analyze_status() */
	int info;				    /* info processed by analyze_status() */
	job *nuevo, *aux;
	struct termios conf_ini; 	/*aqui se guarda la configuracion de terminal*/
	int pid_terminal;
	int bucle = 1;

	pid_terminal = STDIN_FILENO;
	tcgetattr(pid_terminal, &conf_ini);
	printf("\e[1;1H\e[2J"); // "Clear" terminal (en realidad pone varios espacios y baja el scroll)
	printf("ü shell. Para ver los comandos disponibles, \"com\"\n");
	signal(SIGCHLD, my_sigchld);
	lista = new_list("Lista de trabajos");
	hist = new_historial("Historial de Procesos");


	while (bucle){  /* Program terminates normally inside get_command() after ^D is typed*/
		ignore_terminal_signals(); //Ignorar señales ^C, ^Z, SIGTTIN, SIGTTOU...
		printf("%c[%d;%dm\nüsh > %c[%dm",27,1,32,27,0); //cambio de color, opcional
		fflush(stdout);
		get_command(inputBuffer, MAX_LINE, args, &background, &respawn);  /* get next command */
		//printf("Comando= %s, bg= %d\n", inputBuffer, background);
		add_to_historial(hist, *args);
		if(args[0]==NULL) continue;   // if empty command

		else if(strcmp(args[0], "hola") == 0)
			printf("Hola mundo\n");

		else if(strcmp(args[0], "com") == 0){
			printf("Comandos disponibles:\n");
			printf("- hola -> escribe \"Hola mundo\"\n");
			printf("- com -> muestra los comandos disponibles\n");
			printf("- cd [directorio] -> cambia el entorno al directorio indicado\n");
			printf("- jobs -> muestra los trabajos ejecutados desde la üsh\n");
			printf("- fg -> trae a foreground el primer proceso de la lista de trabajos\n");
			printf("- fg [numJob] -> trae a foreground el trabajo indicado\n");
			printf("- bg -> trae a background el primer proceso de la lista de trabajos\n");
			printf("- bg [numJob] -> trae a background el trabajo indicado\n");
			printf("- stop -> suspende el primer proceso de la lista de trabajos\n");
			printf("- stop [numJob] -> suspende el trabajo indicado\n");
			printf("- kill -> fuerza el cierre del primer proceso de la lista de trabajos\n");
			printf("- kill [numJob] -> fuerza el cierre del trabajo indicado\n");
			printf("- historial -> muestra todo el historial");
			printf("- historial [num] -> muestra la posicion [num] del historial");
			printf("- exit -> cierra todos los trabajos y sale de üshell\n");
		}

		else if(strcmp(args[0], "cd") == 0){
			if(args[1]==NULL){
				printf("Error: Es necesario indicar el directorio.\n");
			}
			else{
				if (chdir(args[1])!=0){
					perror("Error de directorio");
				}
			}
		}

		else if(strcmp(args[0], "jobs")==0)
			print_job_list(lista);

		else if(strcmp(args[0], "bg") == 0){
			if(!empty_list(lista)){
				if(args[1]==NULL){
					if(lista->next->state == BACKGROUND)
						printf("El proceso ya se encuentra en background.\n");
					else{
						modificar_job(lista, lista->next, BACKGROUND);
						put_job_in_background(lista->next);
					}
				}
				else{
					int numJob = atoi(args[1]);
					aux = get_item_bypos(lista, numJob);
					if(aux == NULL){
						printf("Error: No se encuentra el trabajo.\n");
					}else{
						if(aux->state == BACKGROUND)
							printf("El proceso ya se encuentra en background.\n");
						else{
							modificar_job(lista, aux, BACKGROUND);
							put_job_in_background(aux);
						}
					}
				}
			}else{
				printf("Error: lista de trabajos vacia.\n");
			}
		}

		else if(strcmp(args[0], "stop") == 0){
			if(!empty_list(lista)){
				if(args[1]==NULL){
					if(lista->next->state != STOPPED){
						modificar_job(lista, lista->next, STOPPED);
						stop_job_in_background(lista->next);
					}else
						printf("Trabajo actualmente detenido.\n");
				}
				else{
					int numJob = atoi(args[1]);
					aux = get_item_bypos(lista, numJob);
					if(aux == NULL)
						printf("Error: No se encuentra el trabajo.\n");
					else{
						if(aux->state != STOPPED){
							modificar_job(lista, aux, STOPPED);
							stop_job_in_background(aux);
						}else
							printf("Trabajo actualmente detenido.\n");
					}
				}
			}else{
				printf("Error: lista de trabajos vacia.\n");
			}
		}

		else if(strcmp(args[0], "fg") == 0){
			if(!empty_list(lista)){
				if(args[1]==NULL)
					put_job_in_foreground(lista->next); //no hace falta comprobar si el primero esta en fg
				else{
					int numJob = atoi(args[1]);
					aux = get_item_bypos(lista, numJob);
					if(aux == NULL)
						printf("Error: No se encuentra el trabajo.\n");
					else
						put_job_in_foreground(aux);
				}
			}else{
				printf("Error: lista de trabajos vacia.\n");
			}
		}

		else if(strcmp(args[0], "kill") == 0){
			if(!empty_list(lista)){
				if(args[1]==NULL)
					kill_job(lista->next);
				else{
					int numJob = atoi(args[1]);
					aux = get_item_bypos(lista, numJob);
					if(aux == NULL)
						printf("Error: No se encuentra el trabajo.\n");
					else
						kill_job(aux);
				}
			}else{
				printf("Error: lista de trabajos vacia.\n");
			}
		}

		else if(strcmp(args[0], "exit")==0){
			while(!empty_list(lista))
				kill_job(lista->next);
			bucle = 0;
		}

		else if(strcmp(args[0], "historial") == 0){
			if(args[1]==NULL)
				print_historial(hist);
			else{
				int numHist = atoi(args[1]);
				if(history_position(hist, numHist)!=NULL)
					print_item_historial(history_position(hist, numHist));
				else
					printf("El historial no tiene esa entrada");
			}
		}

		else{ // EJECUTA

			pid_fork = fork();
			if(pid_fork == -1){
				printf("Error en fork()\n");
			}
			else if(pid_fork > 0){
				new_process_group(pid_fork);
				nuevo = new_job(pid_fork, inputBuffer, respawn==1? RESPAWNABLE : background==1? BACKGROUND : FOREGROUND);

				//---------Meter en la lista de trabajos----------------//
				add_proceso_listaTrabajo(nuevo, lista);

				if(!background && !respawn){
					put_job_in_foreground(nuevo);
					tcsetattr(pid_terminal, TCSANOW, &conf_ini);
				}else if(background){
					printf("Proceso en Background\n");
					fflush(stdout);
				}else if(respawn){
					printf("Proceso respawnable en Background\n");
					fflush(stdout);
				}
			}else{
				ejecutarProceso(getpid(), background, respawn, args, inputBuffer);
			}// FIN EJECUTA
		}
	}
	return 0;
}





