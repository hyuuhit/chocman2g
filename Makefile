CC		= gcc
CFLAG	= -fPIC -Wall -g
INC		=
LIB 	= -lpthread -rdynamic -ldl

BIN		= Cheart

OO		= connector.o mem.o logging.o server_conf.o setting.o set_proc_title.o main.o msg_queue.o thread.o net.o ae.o echo_demo/echo.o

default:$(BIN)

install:
	install $(BIN) ../bin

$(BIN):$(OO)
	@echo
	@echo "Compiling $@ <== $(OO) ..."
	$(CC) -o $@ $(OO) $(LIB) $(CFLAG)

$(OO):%.o:%.c
	@echo
	@echo "Compiling $@ <== $< ..."
	$(CC) -c -o $@ $< $(INC) $(CFLAG)

clean:
	-rm -f $(BIN) $(OO)
