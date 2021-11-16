// This is an incomplete & imprecice implementation of the
// standard file by the same name
#ifndef __MINGW_ENDIAN__
#define __MINGW_ENDIAN__

#ifdef MINGW // Only for cross compilation to mingw


#define __LITTLE_ENDIAN        1234
#define __BIG_ENDIAN           4321

#define __BYTE_ORDER __LITTLE_ENDIAN

#endif // MINGW
#endif