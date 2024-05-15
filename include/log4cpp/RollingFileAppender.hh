/*
 * RollingFileAppender.hh
 *
 * See the COPYING file for the terms of usage and distribution.
 */

#ifndef _LOG4CPP_ROLLINGFILEAPPENDER_HH
#define _LOG4CPP_ROLLINGFILEAPPENDER_HH

#include <log4cpp/Portability.hh>
#include <log4cpp/FileAppender.hh>
#include <string>
#include <stdarg.h>

namespace log4cpp {

    /**
       RollingFileAppender is a FileAppender that rolls over the logfile once
       it has reached a certain size limit.
       @since 0.3.1
    **/
    typedef void (*rollOverCallback)() ;
    class LOG4CPP_EXPORT RollingFileAppender : public FileAppender {
        public:
        RollingFileAppender(const std::string& name, 
                            const std::string& fileName,
                            size_t maxFileSize = 10*1024*1024, 
                            unsigned int maxBackupIndex = 1,
                            bool append = true,
                            mode_t mode = 00644);

        virtual void setMaxBackupIndex(unsigned int maxBackups);
        virtual unsigned int getMaxBackupIndex() const;
        virtual void setMaximumFileSize(size_t maxFileSize);
        virtual size_t getMaxFileSize() const;

        virtual void rollOver();
        void  setRollOverBeforeCallback(rollOverCallback roll_cb);
        void  setRollOverAfterCallback(rollOverCallback roll_cb);
        protected:
        virtual void _append(const LoggingEvent& event);

        unsigned int _maxBackupIndex;
        unsigned short int _maxBackupIndexWidth;	// keep constant index width by zeroing leading positions

        size_t _maxFileSize;
        rollOverCallback rollBeforeCb = NULL;
        rollOverCallback rollAfterCb = NULL;
    };
}

#endif // _LOG4CPP_ROLLINGFILEAPPENDER_HH
