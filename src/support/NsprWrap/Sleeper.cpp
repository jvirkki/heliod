/*
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS HEADER.
 *
 * Copyright 2008 Sun Microsystems, Inc. All rights reserved.
 *
 * THE BSD LICENSE
 *
 * Redistribution and use in source and binary forms, with or without 
 * modification, are permitted provided that the following conditions are met:
 *
 * Redistributions of source code must retain the above copyright notice, this
 * list of conditions and the following disclaimer. 
 * Redistributions in binary form must reproduce the above copyright notice, 
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution. 
 *
 * Neither the name of the  nor the names of its contributors may be
 * used to endorse or promote products derived from this software without 
 * specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER 
 * OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, 
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; 
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, 
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR 
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF 
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Implementation of the Sleeper class. 
 *
 * @author     $Author: arvinds $
 * @version    $Revision: 1.1.2.2 $
 */

#include "Sleeper.h"

Sleeper::Sleeper(void)
{
    sleeper_ = NULL;

    // Create the condition variable used to implement a timed wait
    sleeperLock_ = new CriticalSection();
    PR_ASSERT(sleeperLock_ != NULL);

    sleeper_ = new ConditionVar(*sleeperLock_);
    PR_ASSERT(sleeper_ != NULL);
}

Sleeper::~Sleeper()
{
    if (sleeper_)
    {
        delete sleeper_;
        sleeper_ = NULL;
    }

    if (sleeperLock_)
    {
        delete sleeperLock_;
        sleeperLock_ = NULL;
    }
}


void
Sleeper::sleep(const PRIntervalTime timeOut)
{
    if (sleeper_ && sleeperLock_)
    {
        sleeperLock_->acquire();
        sleeper_->timedWait(timeOut);
        sleeperLock_->release();
    }
}
