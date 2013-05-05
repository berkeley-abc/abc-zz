//
//     MiniRed/GlucoRed
//
//     Siert Wieringa 
//     siert.wieringa@aalto.fi
// (c) Aalto University 2012/2013
//
//

#ifndef VERSION_STRING
#ifdef MINIRED
#define VERSION_STRING "MiniRed"
#elif defined GLUCORED
#define VERSION_STRING "GlucoRed"
#else
#error "Neither MINIRED nor GLUCORED is defined"
#endif
#endif
