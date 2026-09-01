#ifndef JAVASTATSMANAGERITF_H_
#define JAVASTATSMANAGERITF_H_

//-----------------------------------------------------------------------------
// JavaStatsManagerItf
//
// Inteface class to access jvm related functionality in webserver core. As the
// core is not linked with jvm. This interface helps core to make any jvm
// related calls. This will grow with java side need of stats.
//-----------------------------------------------------------------------------

class JavaStatsManagerItf
{
    public:
        virtual void poll(void) = 0;
        virtual void updateJvmMgmtStats(void) = 0;
        virtual void processReconfigure(void) = 0;
};


#endif

