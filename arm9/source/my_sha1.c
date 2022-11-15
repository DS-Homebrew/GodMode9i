/*---------------------------------------------------------------------------------

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

#include <nds/sha1.h>
#include <nds/system.h>

extern bool bios9iEnabled;

void my_swiSHA1InitTWL(swiSHA1context_t *ctx);
void my_swiSHA1UpdateTWL(swiSHA1context_t *ctx, const void *data, size_t len);
void my_swiSHA1FinalTWL(void *digest, swiSHA1context_t *ctx);
void my_swiSHA1CalcTWL(void *digest, const void *data, size_t len);
void my_swiSHA1VerifyTWL(const void *digest1, const void *digest2);

//---------------------------------------------------------------------------------
void my_swiSHA1Init(swiSHA1context_t *ctx) {
//---------------------------------------------------------------------------------
	if (bios9iEnabled) my_swiSHA1InitTWL(ctx);
}

//---------------------------------------------------------------------------------
void my_swiSHA1Update(swiSHA1context_t *ctx, const void *data, size_t len) {
//---------------------------------------------------------------------------------
	if (bios9iEnabled) my_swiSHA1UpdateTWL(ctx, data, len);

}

//---------------------------------------------------------------------------------
void my_swiSHA1Final(void *digest, swiSHA1context_t *ctx) {
//---------------------------------------------------------------------------------
	if (bios9iEnabled) my_swiSHA1FinalTWL(digest, ctx);
}

//---------------------------------------------------------------------------------
void my_swiSHA1Calc(void *digest, const void *data, size_t len) {
//---------------------------------------------------------------------------------
	if (bios9iEnabled) my_swiSHA1CalcTWL(digest, data, len);
}

//---------------------------------------------------------------------------------
void my_swiSHA1Verify(const void *digest1, const void *digest2) {
//---------------------------------------------------------------------------------
	if (bios9iEnabled) my_swiSHA1VerifyTWL(digest1, digest2);
}
