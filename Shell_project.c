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
#define ROJO "\x1b[31;1;1m"
#define NEGRO "\x1b[0m"
#define VERDE "\x1b[32;1;1m"
#define AZUL "\x1b[34;1;1m"
#define CYAN "\x1b[36;1;1m"
#define MARRON "\x1b[33;1;1m"
#define PURPURA "\x1b[35;1;1m"
#define BLANCO "\x1b[37;1;1m"

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

	printf("%sSIGCHLD recibido\n%s", ROJO, BLANCO);
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
                	job* aux;
                    int pid_fork;

                    if(info == EXIT_FAILURE){
                        delete_job(lista, jb);
				        i--;
                        continue;
                    }
                    
                    pid_fork = fork();
                    if(pid_fork == -1){
                        printf("Error en fork()\n");
                    }
                    else if(pid_fork > 0){
                        new_process_group(pid_fork);
                        aux = new_job(pid_fork, jb->args[0], RESPAWNABLE, jb->args);
                        add_proceso_listaTrabajo(aux, lista);
                        printf("Proceso respawnable en Background\n");
                        fflush(stdout);
                    }else{
                        new_process_group(getpid());
                        restore_terminal_signals();
                        execvp(jb->args[0], jb->args);
                        printf("Error, comando desconocido: %s. Fallo en execv\n", aux->args[0]);
                        exit(EXIT_FAILURE);
                    }
                }
				delete_job(lista, jb);
				i--;
		    }
			unblock_SIGCHLD();
			printf("\n%süsh > %s",VERDE, BLANCO );
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
	unblock_SIGCHLD();
}

void stop_job_in_background(job *j){
	/* Enviar SIGSTOP */
	block_SIGCHLD();
	if (killpg(j->pgid, SIGSTOP))
		perror ("kill (SIGSTOP)");
	unblock_SIGCHLD();
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
	args[0] = 0;
	pid_terminal = STDIN_FILENO;
	tcgetattr(pid_terminal, &conf_ini);
	printf("\e[1;1H\e[2J"); // "Clear" terminal (en realidad pone varios espacios y baja el scroll)
	printf("ü shell. Para ver los comandos disponibles, \"com\"\n");
	signal(SIGCHLD, my_sigchld);
	lista = new_list("Lista de trabajos", args);
	hist = new_historial("Historial de Procesos", args, FOREGROUND);


	while (bucle){  /* Program terminates normally inside get_command() after ^D is typed*/
		ignore_terminal_signals(); //Ignorar señales ^C, ^Z, SIGTTIN, SIGTTOU...
		printf("\n%süsh > %s", VERDE, BLANCO ); //cambio de color, opcional
		fflush(stdout);
		get_command(inputBuffer, MAX_LINE, args, &background, &respawn);  /* get next command */
		//printf("Comando= %s, bg= %d\n", inputBuffer, background);
		
		if(args[0]!=NULL){
		    add_to_historial(hist, args, respawn==1? RESPAWNABLE : background==1? BACKGROUND : FOREGROUND);
		}
		
		if(args[0]==NULL)  // if empty command
            continue;
            
		else if(strcmp(args[0], "hola") == 0)
			printf("Hola mundo\n");

		else if(strcmp(args[0], "com") == 0){
			printf("Comandos disponibles:\n");
			printf("- %shola%s -> %sescribe \"Hola mundo\"\n", CYAN, AZUL, BLANCO);
			printf("- %scom%s -> %smuestra los comandos disponibles\n", CYAN, AZUL, BLANCO);
			printf("- %scd [directorio]%s -> %scambia el entorno al directorio indicado\n", CYAN, AZUL, BLANCO);
			printf("- %sjobs%s -> %s muestra los trabajos ejecutados desde la üsh\n", CYAN, AZUL, BLANCO);
			printf("- %sfg%s -> %s trae a foreground el primer proceso de la lista de trabajos\n", CYAN, AZUL, BLANCO);
			printf("- %sfg [numJob]%s -> %s trae a foreground el trabajo indicado\n", CYAN, AZUL, BLANCO);
			printf("- %sbg%s -> %s trae a background el primer proceso de la lista de trabajos\n", CYAN, AZUL, BLANCO);
			printf("- %sbg [numJob]%s -> %s trae a background el trabajo indicado\n", CYAN, AZUL, BLANCO);
			printf("- %sstop%s -> %s suspende el primer proceso de la lista de trabajos\n", CYAN, AZUL, BLANCO);
			printf("- %sstop [numJob]%s -> %s suspende el trabajo indicado\n", CYAN, AZUL, BLANCO);
			printf("- %skill%s -> %s fuerza el cierre del primer proceso de la lista de trabajos\n", CYAN, AZUL, BLANCO);
			printf("- %skill [numJob]%s -> %s fuerza el cierre del trabajo indicado\n", CYAN, AZUL, BLANCO);
			printf("- %shis%s -> %s muestra todo el historial\n", CYAN, AZUL, BLANCO);
			printf("- %shis [num] %s-> %sejecuta la instruccion indicada guardada en el historial\n", CYAN, AZUL, BLANCO);
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
				modificar_job(lista, lista->next, BACKGROUND);
				kill_job(lista->next);
			bucle = 0;
		}

		else if(strcmp(args[0], "his") == 0){
			if(args[1]==NULL)
				print_historial(hist);
			else{
				int numHist = atoi(args[1]);
				if(history_position(hist, numHist)!=NULL){
					job* aux;
					historial* historaux = history_position(hist, numHist);
                    int pid_fork;
                    pid_fork = fork();
                    if(pid_fork == -1){
                        printf("Error en fork()\n");
                    }
                    else if(pid_fork > 0){
                        new_process_group(pid_fork);
                        aux = new_job(pid_fork, historaux->args[0], historaux->state, historaux->args);
                        background = historaux->state == BACKGROUND? 1 : 0;
                        respawn = historaux->state == RESPAWNABLE? 1 : 0;
                        add_proceso_listaTrabajo(aux, lista);
                        if(!background && !respawn){
					        put_job_in_foreground(aux);
					        tcsetattr(pid_terminal, TCSANOW, &conf_ini);
				        }else if(background){
					        printf("Proceso en Background\n");
					        fflush(stdout);
				        }else if(respawn){
					        printf("Proceso respawnable en Background\n");
					        fflush(stdout);
				        }
                    }else{
                        new_process_group(getpid());
                        restore_terminal_signals();
                        execvp(historaux->command, historaux->args);
                        printf("Error, comando desconocido: %s. Fallo en execv\n", historaux->args[0]);
                        exit(EXIT_FAILURE);
                    }
                }
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
				nuevo = new_job(pid_fork, inputBuffer, respawn==1? RESPAWNABLE : background==1? BACKGROUND : FOREGROUND, args);

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
                new_process_group(getpid());
                restore_terminal_signals();
                if(!background && !respawn)
                    set_terminal(getpid());
                execvp(args[0], args);
                printf("Error, comando desconocido: %s. Fallo en execv\n", inputBuffer);
                exit(EXIT_FAILURE);
			}// FIN EJECUTA
		}
	}
	return 0;
}
