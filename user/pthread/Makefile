ARCH ?= none # catch bugs
CFLAGS_USER += -static -fomit-frame-pointer -g -fPIC
LIBNAME = pthread

SRCDIR := 
OBJDIR := $(SRCDIR)obj
INCDIR = $(SRCDIR).

INCS = -I$(INCDIR) 
FINALLIBA = $(OBJDIR)/lib$(LIBNAME).a
FINALLIBSO = $(OBJDIR)/lib$(LIBNAME).so

uc = $(shell echo $(1) | tr a-z A-Z)

LIBUCNAME := $(call uc, $(LIBNAME))
HEADERS := $(wildcard $(SRCDIR)*.h)
CFILES  := $(wildcard $(SRCDIR)*.c)
OBJS    := $(patsubst %.c, $(OBJDIR)/%.o, $(CFILES))

all: $(FINALLIBA) $(FINALLIBSO)
	@:

$(OBJDIR)/%.o: $(SRCDIR)%.c $(HEADERS)
	@echo + cc [$(LIBUCNAME)] $<
	@mkdir -p $(@D)
	$(Q)$(CC) $(CFLAGS_USER) $(INCS) -o $@ -c $<

$(FINALLIBA): $(OBJS)
	@echo + ar [$(LIBUCNAME)] $@
	@mkdir -p $(@D)
	$(Q)$(AR) rc $@ $(OBJS)

$(FINALLIBSO): $(OBJS)
	@echo + ld [$(LIBUCNAME)] $@
	@mkdir -p $(@D)
	$(Q)$(CC) -shared -nostdlib -o $(FINALLIBSO) \
	          -Wl,-soname,$(notdir $(FINALLIBSO)) \
	          -Wl,--whole-archive,$(FINALLIBA)

install: $(FINALLIBA) $(FINALLIBSO)
	@cp $(FINALLIBA) $(XCC_TARGET_ROOT)/lib/
	@cp $(FINALLIBSO) $(XCC_TARGET_ROOT)/lib/
	@cp -R $(INCDIR)/*.h $(XCC_TARGET_ROOT)/sys-include/

clean: 
	@echo + clean [$(LIBUCNAME)]
	$(Q)rm -rf $(FINALLIBA)
	$(Q)rm -rf $(FINALLIBSO)
	$(Q)rm -rf $(OBJDIR)
	
