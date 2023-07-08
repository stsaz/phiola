/** Fast CRC32 implementation using 8k table */

#include <memory.h>

#ifdef WORDS_BIGENDIAN
#	include "crc32_table_be.h"
#else
#	include "crc32_table_le.h"
#endif


/* liblzma/check/crc_macros.h */

#ifdef WORDS_BIGENDIAN
#	define A(x) ((x) >> 24)
#	define B(x) (((x) >> 16) & 0xFF)
#	define C(x) (((x) >> 8) & 0xFF)
#	define D(x) ((x) & 0xFF)

#	define S8(x) ((x) << 8)
#	define S32(x) ((x) << 32)

#else
#	define A(x) ((x) & 0xFF)
#	define B(x) (((x) >> 8) & 0xFF)
#	define C(x) (((x) >> 16) & 0xFF)
#	define D(x) ((x) >> 24)

#	define S8(x) ((x) >> 8)
#	define S32(x) ((x) >> 32)
#endif


/* liblzma/check/crc32_fast.c by Lasse Collin */

// If you make any changes, do some benchmarking! Seemingly unrelated
// changes can very easily ruin the performance (and very probably is
// very compiler dependent).
unsigned int crc32(const unsigned char *buf, size_t size, unsigned int crc)
{
	crc = ~crc;

#ifdef WORDS_BIGENDIAN
	crc = bswap32(crc);
#endif

	if (size > 8) {
		// Fix the alignment, if needed. The if statement above
		// ensures that this won't read past the end of buf[].
		while ((size_t)(buf) & 7) {
			crc = crc32_table[0][*buf++ ^ A(crc)] ^ S8(crc);
			--size;
		}

		// Calculate the position where to stop.
		const unsigned char *const limit = buf + (size & ~(size_t)(7));

		// Calculate how many bytes must be calculated separately
		// before returning the result.
		size &= (size_t)(7);

		// Calculate the CRC32 using the slice-by-eight algorithm.
		while (buf < limit) {
			crc ^= *(const unsigned int *)(buf);
			buf += 4;

			crc = crc32_table[7][A(crc)]
			    ^ crc32_table[6][B(crc)]
			    ^ crc32_table[5][C(crc)]
			    ^ crc32_table[4][D(crc)];

			const unsigned int tmp = *(const unsigned int *)(buf);
			buf += 4;

			// At least with some compilers, it is critical for
			// performance, that the crc variable is XORed
			// between the two table-lookup pairs.
			crc = crc32_table[3][A(tmp)]
			    ^ crc32_table[2][B(tmp)]
			    ^ crc
			    ^ crc32_table[1][C(tmp)]
			    ^ crc32_table[0][D(tmp)];
		}
	}

	while (size-- != 0)
		crc = crc32_table[0][*buf++ ^ A(crc)] ^ S8(crc);

#ifdef WORDS_BIGENDIAN
	crc = bswap32(crc);
#endif

	return ~crc;
}
