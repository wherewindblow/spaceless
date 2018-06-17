protoc -I=. --cpp_out=. protocol.proto
# NOTE: Make sure command generator is under same directory with this shell.
./command_generator protocol.proto command.txt command.details
