The following text is an exert from the [Assembly language](https://en.wikipedia.org/wiki/Assembly_language) article.

*In computer programming, assembly language (or assembler language), sometimes abbreviated asm, is any low-level programming language in which there is a very strong correspondence between the instructions in the language and the architecture's machine code instructions. Assembly language usually has one statement per machine instruction (1:1), but constants, comments, assembler directives, and symbolic labels of, e.g., memory locations, registers, and macros are generally also supported.*

## Intel 8080 Assembly Language Programming Manual

Refer to the [Intel 8080 Assembly Language Programming Manual](00-Manuals.md#8080-assembly-language-development) for more about the language and its implementation.

!!! warning "Intel 8080 Assembly Language Symbol Length Limitation"

    The biggest **gotcha** is that all identifiers/symbols are unique to 6 characters. For example, the following two variables are treated as the same symbol:

    ```asm
    VARIAB1: DB 0
    VARIAB2: DB 0  
    ``` 
    
    **Both are treated as `VARIAB`. So, be careful when naming variables, functions, and other symbols as you get no assembler warnings or errors and your application will behave unexpectedly.**

## Assemble assembly applications

The CP/M disk image includes two demo assembly applications, **DEMO.ASM** and **SLEEP.ASM**. Follow these steps to edit, assemble, and load the demo file:

### The DEMO.ASM example

1. List the *DEMO.ASM* file

    ```cpm
    type sleep.asm
    ```

    ```asm
            ORG     0100H           ; CP/M base of TPA (transient program area)
            OUT     30
            MVI     C,09H           ; Print string function
            LXI     D,MESSAGE       ; Point
            CALL    0005H           ; Call bdos
            RET                     ; To cp/m
    MESSAGE:DB      0DH,0AH,'Hello, World!',0DH,0AH,'$'
            END
    ```

1. Assemble the *DEMO.ASM* file:

    ```cpm
    asm demo
    ```

1. Load and link the assembled code:

    ```cpm
    load demo
    ```

1. Run the demo application:

    ```cpm
    demo
    ```

## Edit a file with Word-Master

in the following steps use the [Word-Master text editor](https://github.com/AzureSphereCloudEnabledAltair8800/Altair8800.manuals/blob/master/Word-Master_Manual.pdf) to edit a file. It's highly recommended to [edit files with Visual Studio Code](01-Editing-files.md) and then copy the file to the Altair filesystem using the CP/M gf command.

1. Edit the *DEMO.ASM* file with Word-Master:

    ```cpm
    wm demo.asm
    ```

1. Switch the web terminal to character input mode by selecting **Ctrl+L**.

1. Edit the *demo.asm* file. For example, change *Hello, World!* text to your name.

1. Save your updates to the *demo.asm* file:

    1. Select the **Esc** key.
    1. Select **E** to exit. Your file changes are saved to disk.

1. Switch the web terminal to line input mode by selecting **Ctrl+L**.
1. Then assemble, load, and run the updated demo.asm application.

## Sleep assembly example

The SLEEP.ASM example uses Intel 8080 input and output port instructions. The *SLEEP.ASM* application sets a sleep period of 2 seconds using output port 30, waits on input port 30 for the delay period to expire, and then publishes weather data to Azure IoT Central. For more information about Intel 8080 IO port mappings, refer to [Intel 8080 input and output ports](https://github.com/gloveboxes/Altair8800.Emulator.UN-X/wiki#intel-8080-input-and-output-ports).

1. List the *SLEEP.ASM* file

    ```cpm
    type sleep.asm
    ```

    ```asm
          ORG 0100H   ;CP/M base of TPA (transient program area)
          MVI C,09H   ;Print string function
          LXI D,MSG   ;Point to waiting message
          CALL 0005H  ;Call bdos
          MVI A,2     ;Move 2 to the accumulator to set a 2 second delay
          OUT 30      ;Start timer
    LOOP: IN 30       ;Get delay timer state into the accumulator
          CPI 00H     ;If accumulator equal to 0 then timer has expired
          JZ BACK     ;Jump on zero
          JMP LOOP
    BACK: MVI C,09H   ;Print string function
          LXI D,PUB   ;Point to publish message
          CALL 0005H  ;Call bdos
          MVI A,0H    ;Move zero to the accumulator
          OUT 32      ;Publish to Azure IoT Central
          MVI C,09H   ;Print string function
          LXI D,FINI  ;Point to Finished message
          CALL 0005H  ;Call Bdos
          RET
    MSG:  DB 'Sleeping 2 seconds$'
    FINI: DB 0DH,0AH,'Finished$'
    PUB:  DB 0DH,0AH,'Publishing to Azure IoT Central$' 
          END
    ```

1. Assemble the *SLEEP.ASM* file:

    ```bash
    asm sleep
    ```

1. Load and link the assembled code:

    ```bash
    load sleep
    ```

1. Run the demo application:

    ```bash
    sleep
    ```
