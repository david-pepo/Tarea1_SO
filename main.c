#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>    
#include <sys/wait.h>
#include <sys/types.h>
#include <signal.h>

#define MAX_LINE 256
#define MAX_DEPS 20
#define MAX_ACTIVIDADES 10000

typedef struct {
    char id[32];
    char nombre[100];
    int tiempo_ms;
    char dependencias[MAX_DEPS][32];
    int num_dependencias;
    
    int estado;    
    pid_t pid;     
    
    char mensaje_salida[150];
    int fd_in[2];             
    int fd_out[2];            
} Actividad;

Actividad plan[MAX_ACTIVIDADES];
int total_actividades = 0;

void inspeccion_seremi(int sig) {
    (void)sig; 
    
    printf("\n\n[!] ¡LLEGÓ LA SEREMI! (Señal SIGINT atrapada). Clausurando la ramada...\n");
    
    for (int i = 0; i < total_actividades; i++) {
        if (plan[i].estado == 1 && plan[i].pid > 0) {
            printf("[Seremi] Abortando actividad: %s (PID: %d)\n", plan[i].nombre, plan[i].pid);
            kill(plan[i].pid, SIGKILL);
        }
    }
    
    printf("[Seremi] Todas las actividades fueron canceladas. Fin del simulador.\n");
    exit(1);
}

int main(int argc, char *argv[]) {

    if (argc != 3) {
        fprintf(stderr, "Uso: %s <archivo_plan.txt> <K_concurrencia>\n", argv[0]);
        return 1;
    }

    struct sigaction sa;
    sa.sa_handler = inspeccion_seremi;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    
    if (sigaction(SIGINT, &sa, NULL) == -1) {
        perror("Error al registrar la señal SIGINT");
        return 1;
    }

    srand(time(NULL));

    FILE *file = fopen(argv[1], "r");
    if (!file) {
        perror("Error al abrir el archivo");
        return 1;
    }

    char buffer[MAX_LINE];
    
    printf("--- Construyendo el DAG en memoria ---\n");
    while (fgets(buffer, sizeof(buffer), file) && total_actividades < MAX_ACTIVIDADES) {
        buffer[strcspn(buffer, "\n")] = 0;
        if (strlen(buffer) == 0) continue;

        char *saveptr1, *saveptr2;
        
        char *id_str = strtok_r(buffer, ":", &saveptr1);
        char *nombre_str = strtok_r(NULL, ":", &saveptr1);
        char *tiempo_str = strtok_r(NULL, ":", &saveptr1);
        char *deps_str = strtok_r(NULL, ":", &saveptr1);

        if (!id_str || !nombre_str) continue; 

        strcpy(plan[total_actividades].id, id_str);
        strcpy(plan[total_actividades].nombre, nombre_str);

        int tiempo_ms = 0;
        if (tiempo_str != NULL && atoi(tiempo_str) > 0) {
            tiempo_ms = atoi(tiempo_str);
        } else {
            tiempo_ms = (rand() % 4901) + 100;
        }
        plan[total_actividades].tiempo_ms = tiempo_ms;

        plan[total_actividades].num_dependencias = 0;
        if (deps_str != NULL) {
            char *dep = strtok_r(deps_str, ",", &saveptr2);
            while (dep != NULL && plan[total_actividades].num_dependencias < MAX_DEPS) {
                while(*dep == ' ') dep++; 
                strcpy(plan[total_actividades].dependencias[plan[total_actividades].num_dependencias], dep);
                plan[total_actividades].num_dependencias++;
                dep = strtok_r(NULL, ",", &saveptr2);
            }
        }
        
        total_actividades++;
    }
    fclose(file);

    int K = atoi(argv[2]);
    if (K <= 0) K = 1;

    for (int i = 0; i < total_actividades; i++) {
        plan[i].estado = 0;
    }

    int terminadas = 0;
    int en_vuelo = 0;

    printf("\n--- Iniciando Simulación con K=%d ---\n", K);

    while (terminadas < total_actividades) {

        for (int i = 0; i < total_actividades && en_vuelo < K; i++) {
            if (plan[i].estado == 0) {
                
                int dependencias_listas = 1;
                for (int j = 0; j < plan[i].num_dependencias; j++) {
                    for (int k = 0; k < total_actividades; k++) {
                        if (strcmp(plan[i].dependencias[j], plan[k].id) == 0) {
                            if (plan[k].estado != 2) {
                                dependencias_listas = 0;
                            }
                            break;
                        }
                    }
                }

                if (dependencias_listas) {
                    plan[i].estado = 1;
                    en_vuelo++;

                    if (pipe(plan[i].fd_in) == -1 || pipe(plan[i].fd_out) == -1) {
                        perror("Error creando pipes");
                        exit(1);
                    }

                    for (int j = 0; j < plan[i].num_dependencias; j++) {
                        for (int k = 0; k < total_actividades; k++) {
                            if (strcmp(plan[i].dependencias[j], plan[k].id) == 0) {
                                write(plan[i].fd_in[1], plan[k].mensaje_salida, sizeof(plan[k].mensaje_salida));
                                break;
                            }
                        }
                    }

                    pid_t pid = fork();
                    if (pid == 0) {
			if (pid == 0) {
                      
                        struct sigaction sa_dfl;
                        sa_dfl.sa_handler = SIG_DFL;
                        sigemptyset(&sa_dfl.sa_mask);
                        sa_dfl.sa_flags = 0;
                        sigaction(SIGINT, &sa_dfl, NULL);

                        close(plan[i].fd_in[1]); 
                        close(plan[i].fd_out[0]);
                        close(plan[i].fd_in[1]);
                        close(plan[i].fd_out[0]);}

                        for (int j = 0; j < plan[i].num_dependencias; j++) {
                            char msg_recibido[100];
                            read(plan[i].fd_in[0], msg_recibido, sizeof(msg_recibido));
                            printf("[Hijo %d - %s] Recibí insumo: %s\n", getpid(), plan[i].nombre, msg_recibido);
                        }
                        close(plan[i].fd_in[0]);

                        printf("[Hijo %d] Iniciando %s (%d ms)\n", getpid(), plan[i].nombre, plan[i].tiempo_ms);
                        
                        struct timespec ts;
                        ts.tv_sec = plan[i].tiempo_ms / 1000;
                        ts.tv_nsec = (plan[i].tiempo_ms % 1000) * 1000000L;
                        nanosleep(&ts, NULL);
                        
                        char msg_exito[150];
                        snprintf(msg_exito, sizeof(msg_exito), "¡%s completado!", plan[i].nombre);
                        write(plan[i].fd_out[1], msg_exito, sizeof(msg_exito));
                        close(plan[i].fd_out[1]);

                        printf("[Hijo %d] Terminó %s\n", getpid(), plan[i].nombre);
                        exit(0); 
                    } else if (pid > 0) {
                        plan[i].pid = pid;
                        close(plan[i].fd_in[0]);
                        close(plan[i].fd_in[1]); 
                        close(plan[i].fd_out[1]); 
                    } else {
                        perror("Error al hacer fork");
                        exit(1);
                    }
                }
            }
        }

        if (en_vuelo > 0) {
            pid_t pid_terminado = wait(NULL);
            if (pid_terminado > 0) {
                for (int i = 0; i < total_actividades; i++) {
                    if (plan[i].pid == pid_terminado) {
                        read(plan[i].fd_out[0], plan[i].mensaje_salida, sizeof(plan[i].mensaje_salida));
                        close(plan[i].fd_out[0]);

                        plan[i].estado = 2; 
                        en_vuelo--;
                        terminadas++;
                        printf("[Padre] Registro que %s finalizó. (%d/%d completadas)\n", 
                               plan[i].nombre, terminadas, total_actividades);
                        break;
                    }
                }
            }
        }
    }

    printf("\n--- Simulación Completada Exitosamente ---\n");
    return 0;
}
