#ifndef OSG2VSG_EXPORT_H
#define OSG2VSG_EXPORT_H

#if (defined(_MSC_VER) || defined(__CYGWIN__) || defined(__MINGW32__))
    #if defined(osg2vsg_EXPORTS)
        #define OSG2VSG_EXPORT __declspec(dllexport)
    #elif defined(OSG2VSG_SHARED_LIBRARY)
        #define OSG2VSG_EXPORT __declspec(dllimport)
    #else
        #define OSG2VSG_EXPORT
    #endif
#else
    #define OSG2VSG_EXPORT
#endif

#endif
