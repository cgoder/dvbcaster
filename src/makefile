
# **********************************************
# _BIG_ENDIAN Macro for big endian cpu 
# **********************************************

LIBNAME  = libdvbcast

CROSS_TOOL_PREFIX = arm-none-linux-gnueabi-
CXX = $(CROSS_TOOL_PREFIX)g++
CC  = $(CROSS_TOOL_PREFIX)gcc
AR  = $(CROSS_TOOL_PREFIX)ar
LD  = $(CROSS_TOOL_PREFIX)ld
RANLIB  = $(CROSS_TOOL_PREFIX)ranlib

CC_OPTS += -D_REENTRANT -o0 -lpthread 

SRCDIR += ./
SRC_C += $(foreach dir,$(SRCDIR), $(wildcard $(dir)/*.c))
SRC_CPP += $(foreach dir, $(SRCDIR), $(wildcard $(dir)/*.cpp))

INCDIR += ./
INCLUDE = $(foreach dir,$(INCDIR), -I$(dir))

STATIC_OBJ_C0  = $(patsubst %.c, %.o, $(SRC_C))
STATIC_OBJ_C1  = $(patsubst %.cpp, %.o, $(SRC_CPP))

STATIC_OBJ_C  = $(foreach file, $(STATIC_OBJ_C0), $(file) )
STATIC_OBJ_CPP = $(foreach file, $(STATIC_OBJ_C1), $(file))


# **********************************************

libs: $(STATIC_OBJ_C) $(STATIC_OBJ_CPP)
#	@$(AR) cru $(LIBNAME).a $(STATIC_OBJ_C) $(STATIC_OBJ_CPP)
	@$(CXX) $(STATIC_OBJ_C) $(STATIC_OBJ_CPP) -fPIC -shared -o $(LIBNAME).so
#	@$(CXX) $(STATIC_OBJ_C) $(STATIC_OBJ_CPP) $(CC_OPTS) -o $(LIBNAME).elf
	@echo "make dvbcast lib ok ......."

$(STATIC_OBJ_C) : %.o : %.c 
	@echo "begin compiling for c file   ........." 
	@$(CXX) $(INCLUDE) $(CC_OPTS) -c $< -o $@ 
	@echo "make c file obj ok....."

$(STATIC_OBJ_CPP) : %.o : %.cpp
	@echo "begin compiling for cpp file....."
	@$(CXX) $(INCLUDE) $(CC_OPTS) -c $< -o $@
	@echo "make cpp file objs ok...."

clean :
	@rm -f $(STATIC_OBJ_C)
	@rm -f $(STATIC_OBJ_CPP)
	@rm -rf $(LIBNAME).a
	
