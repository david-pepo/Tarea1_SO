Modo de uso: Explica que el código se compila simplemente ejecutando make en la terminal, lo que utiliza las flags estrictas de C17 exigidas (-Wall -Wextra -std=c17). Luego, indica que se ejecuta con ./planificador plan.txt K (donde K es el límite).

Funciones implementadas: Menciona que usaste un arreglo de structs Actividad para modelar el DAG, fork() para la creación concurrente, pipes (fd_in, fd_out) para el paso de mensajes, y sigaction para la interrupción SIGINT.

Justificación de diseño: Este es el punto clave. Explica que decidiste que el Padre actúe como "Broker" (intermediario) de mensajes abriendo y cerrando pipes dinámicamente justo antes y después de cada fork(). Justifica que esto se hizo para cumplir con el requisito de "Carga de estrés (10000 actividades)" evitando el error de límite de descriptores de archivos (EMFILE) en el sistema operativo.
