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

PROXYOBJS+=route
PROXYOBJS+=channel
PROXYOBJS+=httpclient
PROXYOBJS+=proxyerror
PROXYOBJS+=reverse
PROXYOBJS+=url
PROXYOBJS+=stuff
PROXYOBJS+=putils

ifdef FEAT_PROXY
PROXYOBJS+=admutils
PROXYOBJS+=caccess
PROXYOBJS+=cache
PROXYOBJS+=cadmoper
PROXYOBJS+=cadmutil
PROXYOBJS+=calog
PROXYOBJS+=cif
PROXYOBJS+=cio
PROXYOBJS+=cpart
PROXYOBJS+=csect
PROXYOBJS+=cutils
ifdef XXX_MCC_PROXY
PROXYOBJS+=dnscache
endif
PROXYOBJS+=dthrottle
PROXYOBJS+=filter
PROXYOBJS+=filthtml
PROXYOBJS+=filtpipe
PROXYOBJS+=fs
PROXYOBJS+=glue
PROXYOBJS+=gopherclient
PROXYOBJS+=guess
PROXYOBJS+=hassle
PROXYOBJS+=hdrutils
ifdef XXX_MCC_PROXY
PROXYOBJS+=ldap_pool
endif
PROXYOBJS+=pauth
PROXYOBJS+=pacache
PROXYOBJS+=pauthcache
PROXYOBJS+=perrpage
PROXYOBJS+=phttp
PROXYOBJS+=pobj
PROXYOBJS+=proxy-fn
PROXYOBJS+=putils
PROXYOBJS+=retrieve
PROXYOBJS+=commonclient
PROXYOBJS+=connectclient
PROXYOBJS+=ftpclient
PROXYOBJS+=subfuncs
PROXYOBJS+=urlfilt
PROXYOBJS+=virtmap
PROXYOBJS+=worm
PROXYOBJS+=worm_obj
PROXYOBJS+=wormhelpers
PROXYOBJS+=worm_clist
PROXYOBJS+=worm_config
PROXYOBJS+=worm_q
PROXYOBJS+=bu_net
PROXYOBJS+=bu
PROXYOBJS+=common
PROXYOBJS+=bu_thrasher
PROXYOBJS+=gc
PROXYOBJS+=gclist
PROXYOBJS+=urldb
PROXYOBJS+=urldbhelpers
PROXYOBJS+=cachefilter
PROXYOBJS+=proxyfilter
PROXYOBJS+=robot
endif
PROXYOBJS+=host_dns_cache
