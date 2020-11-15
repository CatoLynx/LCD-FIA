Import("env")

env.Replace(
    UPLOADCMD=""" gdb -ex "target remote fia.local:3333" -ex "load .pio/build/$PIOENV/firmware.elf" -ex "monitor reset run" -ex "detach" -ex "quit" """
)