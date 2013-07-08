ARCH ?= none	# catch bugs
CFLAGS_USER += -static -fomit-frame-pointer -g -fPIC
LIBNAME = parlib

SRCDIR := 
OBJDIR := $(SRCDIR)obj
INCDIR = $(SRCDIR)include

INCS = -I. -I$(INCDIR) 
FINALLIBA = $(OBJDIR)/lib$(LIBNAME).a
FINALLIBSO = $(OBJDIR)/lib$(LIBNAME).so

uc = $(shell echo $(1) | tr a-z A-Z)

LIBUCNAME := $(call uc, $(LIBNAME))
HEADERS := $(shell find $(INCDIR) -name *.h)
CFILES  := $(wildcard $(SRCDIR)*.c)
CFILES  += $(wildcard $(SRCDIR)$(ARCH)/*.c)
SFILES  := $(wildcard $(SRCDIR)$(ARCH)/*.S)
OBJS    := $(patsubst %.c, $(OBJDIR)/%.o, $(CFILES)) \
           $(patsubst %.S, $(OBJDIR)/%.o, $(SFILES))

all: $(FINALLIBA) $(FINALLIBSO)
	@:

$(OBJDIR)/$(ARCH)/%.o: $(SRCDIR)$(ARCH)/%.S $(HEADERS)
	@echo + as [$(LIBUCNAME)] $<
	@mkdir -p $(@D)
	$(Q)$(CC) $(CFLAGS_USER) $(INCS) -o $@ -c $<

$(OBJDIR)/%.o: $(SRCDIR)%.c $(HEADERS)
	@echo + cc [$(LIBUCNAME)] $<
	@mkdir -p $(@D)
	$(Q)$(CC) $(CFLAGS_USER) $(INCS) -o $@ -c $<

$(FINALLIBA): $(OBJS)
	@echo + ar [$(LIBUCNAME)] $@
	@mkdir -p $(@D)
	$(Q)$(AR) rc $@ $(OBJS)

$(FINALLIBSO): $(FINALLIBA)
	@echo + ld [$(LIBUCNAME)] $@
	@mkdir -p $(@D)
	$(Q)$(CC) -shared -nostdlib -o $(FINALLIBSO) \
	          -Wl,-soname,$(notdir $(FINALLIBSO)) \
	          -Wl,--whole-archive,$(FINALLIBA)

install: $(FINALLIBA) $(FINALLIBSO)
	@cp $(FINALLIBA) $(XCC_TARGET_ROOT)/lib/
	@cp $(FINALLIBSO) $(XCC_TARGET_ROOT)/lib/
	@cp -R $(INCDIR)/* $(XCC_TARGET_ROOT)/sys-include/
	@rm -rf $(XCC_TARGET_ROOT)/sys-include/parlib  
	@ln -fs . $(XCC_TARGET_ROOT)/sys-include/parlib  

clean: 
	@echo + clean [$(LIBUCNAME)]
	$(Q)rm -rf $(FINALLIBA)
	$(Q)rm -rf $(FINALLIBSO)
	$(Q)rm -rf $(OBJDIR)
	
