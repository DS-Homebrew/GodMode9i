/*---------------------------------------------------------------------------------

	DSi sha1 functions

	Copyright (C) 2017
		Dave Murphy (WinterMute)

	This software is provided 'as-is', without any express or implied
	warranty.  In no event will the authors be held liable for any
	damages arising from the use of this software.

	Permission is granted to anyone to use this software for any
	purpose, including commercial applications, and to alter it and
	redistribute it freely, subject to the following restrictions:

	1.	The origin of this software must not be misrepresented; you
		must not claim that you wrote the original software. If you use
		this software in a product, an acknowledgment in the product
		documentation would be appreciated but is not required.
	2.	Altered source versions must be plainly marked as such, and
		must not be misrepresented as being the original software.
	3.	This notice may not be removed or altered from any source
		distribution.

---------------------------------------------------------------------------------*/
/*!	\file sha1.h
	\brief DSi SHA1 functions.
*/

#ifndef MY_SHA1_H_INCLUDE
#define MY_SHA1_H_INCLUDE

#ifdef __cplusplus
extern "C" {
#endif

#include <nds/ndstypes.h>
#include <nds/sha1.h>
#include <stddef.h>

/**
 * \brief          SHA-1 context setup
 *
 * \param ctx      context to be initialized
 */
void my_swiSHA1Init(swiSHA1context_t *ctx);

/**
 * \brief          SHA-1 process buffer
 *
 * \param ctx      SHA-1 context
 * \param data     buffer to process
 * \param len      length of data
 */
void my_swiSHA1Update(swiSHA1context_t *ctx, const void *data, size_t len);

/**
 * \brief          SHA-1 final digest
 *
 * \param digest   buffer to hold SHA-1 checksum result
 * \param ctx      SHA-1 context
 */
void my_swiSHA1Final(void *digest, swiSHA1context_t *ctx);

/**
 * \brief          SHA-1 checksum
 *
 * \param digest   buffer to hold SHA-1 checksum result
 * \param data     buffer to process
 * \param len      length of data
 */
void my_swiSHA1Calc(void *digest, const void *data, size_t len);

/**
 * \brief          SHA-1 verify
 *
 * \param digest1  buffer containing hash to verify
 * \param digest2  buffer containing hash to verify
 */
void my_swiSHA1Verify(const void *digest1, const void *digest2);

#ifdef __cplusplus
}
#endif


#endif // MY_SHA1_H_INCLUDE