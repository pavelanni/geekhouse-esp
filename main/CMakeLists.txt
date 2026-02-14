idf_component_register(
    SRCS
        "main.c"
        "actuators.c"
        "sensors.c"
        "sensor_task.c"
        "display_task.c"
        "stats_task.c"
        "sensor_data_shared.c"
        "reporter_task.c"
    INCLUDE_DIRS "."
)