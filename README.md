Ampliaciones realizadas e instrucciones:

- Proceso respawnable:
    Escribir un comando terminado en #.
    Este proceso se ejecutará de nuevo por mucho que lo intentemos matar.
    Para que pierda el atributo respawnable hay que aplicarle fg o bg.
    
- time-out [segundos] [orden]:
    Ejectuta durante el tiempo indicado el proceso que indiquemos, cuando se
        cumpla ese tiempo, el proceso recibira una señal para terminarlo.
    Hecho con alarmas.
    Ejemplo -> time-out 2 xclock -update 1 (el reloj desaparecerá en 2 segundos)
    
- Historial:
    Mediante el comando his (en vez de historial, que es mas largo) nos saldrá una
        lista con las ordenes que hemos ejecutado. Aunque solo aparezca su nombre,
        tambien se guardan los modificadores.
    'his' seguido de un numero ejecutará de nuevo la instruccion en esa posicion del
        historial.
        
- Colores:
    Meramente estético, pero se agradece a la vista. Pensado para mostrarse sobre fondo negro.
     
