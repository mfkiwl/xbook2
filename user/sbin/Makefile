sinclude ../../scripts/env.mk

LIBS_DIR	:= ../libs

X_ASFLAGS	+= -f elf -I $(DIR_XLIBC)/indlude 
X_CFLAGS    += -fno-builtin -Wall -Wunused -fno-PIE -m32 -fno-stack-protector -std=gnu99
X_INCDIRS	+= $(LIBS_DIR)/xlibc/include $(LIBS_DIR)/jpeg $(LIBS_DIR)/libpng $(LIBS_DIR)/gapi/include \
				$(LIBS_DIR)/zlib $(LIBS_DIR)/iniparser

X_LDFLAGS	:= -no-pie -m elf_i386 -e _start -Ttext 0x00001000
X_LIBDIRS	+= $(LIBS_DIR)/xlibc $(LIBS_DIR)/jpeg $(LIBS_DIR)/libpng $(LIBS_DIR)/gapi $(LIBS_DIR)/zlib \
				$(LIBS_DIR)/iniparser

LD			:= ld -m elf_i386
AS			:=	nasm

export AS LD

MODULE      +=  init
MODULE      +=  login