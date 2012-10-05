#
# DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS HEADER.
#
# Copyright 2008 Sun Microsystems, Inc. All rights reserved.
#
# THE BSD LICENSE
#
# Redistribution and use in source and binary forms, with or without 
# modification, are permitted provided that the following conditions are met:
#
# Redistributions of source code must retain the above copyright notice, this
# list of conditions and the following disclaimer. 
# Redistributions in binary form must reproduce the above copyright notice, 
# this list of conditions and the following disclaimer in the documentation 
# and/or other materials provided with the distribution. 
#
# Neither the name of the  nor the names of its contributors may be
# used to endorse or promote products derived from this software without 
# specific prior written permission. 
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER 
# OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
# EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, 
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; 
# OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, 
# WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR 
# OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF 
# ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#

# Modified 2011 jyri@virkki.com

# SOLARIS rules

#
# AR[n]_TARGET, AR[n]_OBJS 
#

ifdef AR_TARGET
AR_OBJ_INT=$(addsuffix .$(OBJ),$(AR_OBJS))
REAL_AR_OBJS=$(addprefix $(OBJDIR)/,$(AR_OBJ_INT))
$(OBJDIR)/$(LIBPREFIX)$(AR_TARGET).$(STATIC_LIB_SUFFIX): $(REAL_AR_OBJS)
	$(RM) -f $@
	$(CC) -xar -o $@ $(REAL_AR_OBJS) $(AR_NONPARSED_OBJS)
endif

ifdef AR1_TARGET
AR1_OBJ_INT=$(addsuffix .$(OBJ),$(AR1_OBJS))
REAL_AR1_OBJS=$(addprefix $(OBJDIR)/,$(AR1_OBJ_INT))
$(OBJDIR)/$(LIBPREFIX)$(AR1_TARGET).$(STATIC_LIB_SUFFIX): $(REAL_AR1_OBJS)
	$(RM) -f $@
	$(CC) -xar -o $@ $(REAL_AR1_OBJS) $(AR1_NONPARSED_OBJS)
endif

ifdef AR2_TARGET
AR2_OBJ_INT=$(addsuffix .$(OBJ),$(AR2_OBJS))
REAL_AR2_OBJS=$(addprefix $(OBJDIR)/,$(AR2_OBJ_INT))
$(OBJDIR)/$(LIBPREFIX)$(AR2_TARGET).$(STATIC_LIB_SUFFIX): $(REAL_AR2_OBJS)
	$(RM) -f $@
	$(CC) -xar -o $@ $(REAL_AR2_OBJS) $(AR2_NONPARSED_OBJS)
endif

ifdef AR3_TARGET
AR3_OBJ_INT=$(addsuffix .$(OBJ),$(AR3_OBJS))
REAL_AR3_OBJS=$(addprefix $(OBJDIR)/,$(AR3_OBJ_INT))
$(OBJDIR)/$(LIBPREFIX)$(AR3_TARGET).$(STATIC_LIB_SUFFIX): $(REAL_AR3_OBJS)
	$(RM) -f $@
	$(CC) -xar -o $@ $(REAL_AR3_OBJS) $(AR3_NONPARSED_OBJS)
endif

#
# DLL[n]_MAPFILE
#

ifndef NO_SPARC_SOLARIS_OBJDIR_MAPFILE_RULE
$(OBJDIR)/%.$(MAPFILE_SUFFIX):%.$(VERSIONSFILE_SUFFIX) %.$(SPECFILE_SUFFIX)
	@$(MAKE_OBJDIR)
	$(SPEC2MAP) -l $(OBJDIR)/$*.$(DYNAMIC_LIB_SUFFIX) -o $@ -v $^
endif #NO_SPARC_SOLARIS_OBJDIR_MAPFILE_RULE

ifdef DLL_MAPFILE
DLL_EXTRA+=-M $(DLL_MAPFILE)
endif
ifdef DLL1_MAPFILE
DLL1_EXTRA+=-M $(DLL1_MAPFILE)
endif
ifdef DLL2_MAPFILE
DLL2_EXTRA+=-M $(DLL2_MAPFILE)
endif
ifdef DLL3_MAPFILE
DLL3_EXTRA+=-M $(DLL3_MAPFILE)
endif
ifdef DLL4_MAPFILE
DLL4_EXTRA+=-M $(DLL4_MAPFILE)
endif
ifdef DLL5_MAPFILE
DLL5_EXTRA+=-M $(DLL5_MAPFILE)
endif

