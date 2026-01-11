# Geekhouse ESP

This project is a modification of the previous [Geekhouse](https://github.com/pavelanni/geekhouse).
There two main differences:

- It's written in C with FreeRTOS
- It's written for ESP32 (specifically, ESP32-C3)

## GPIOs

| Type   | ID  | GPIO | Params        |
| ------ | --- | ---- | ------------- |
| LED    | 1   | 2    | yellow, roof  |
| LED    | 2   | 3    | white, garden |
| Sensor | 1   | 1    | water, roof   |
| Sensor | 2   | 0    | light, roof   |
