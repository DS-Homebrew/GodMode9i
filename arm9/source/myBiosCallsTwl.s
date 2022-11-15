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
	.global my_swiRSAInitHeapTWL
	.global my_swiRSADecryptRAWTWL
	.global my_swiRSADecryptTWL
	.global my_swiRSADecryptPGPTWL
	.global my_swiSHA1InitTWL
	.global my_swiSHA1UpdateTWL
	.global my_swiSHA1FinalTWL
	.global my_swiSHA1CalcTWL
	.global my_swiSHA1VerifyTWL

	.arm

@---------------------------------------------------------------------------------
my_swiRSAInitHeapTWL:
@---------------------------------------------------------------------------------
	swi	0x200000
	bx	lr

@---------------------------------------------------------------------------------
my_swiRSADecryptRAWTWL:
@---------------------------------------------------------------------------------
	swi	0x210000
	bx	lr

@---------------------------------------------------------------------------------
my_swiRSADecryptTWL:
@---------------------------------------------------------------------------------
	swi	0x220000
	bx	lr

@---------------------------------------------------------------------------------
my_swiRSADecryptPGPTWL:
@---------------------------------------------------------------------------------
	swi	0x230000
	bx	lr

@---------------------------------------------------------------------------------
my_swiSHA1InitTWL:
@---------------------------------------------------------------------------------
	swi	0x240000
	bx	lr

@---------------------------------------------------------------------------------
my_swiSHA1UpdateTWL:
@---------------------------------------------------------------------------------
	swi	0x250000
	bx	lr

@---------------------------------------------------------------------------------
my_swiSHA1FinalTWL:
@---------------------------------------------------------------------------------
	swi	0x260000
	bx	lr

@---------------------------------------------------------------------------------
my_swiSHA1CalcTWL:
@---------------------------------------------------------------------------------
	swi	0x270000
	bx	lr

@---------------------------------------------------------------------------------
my_swiSHA1VerifyTWL:
@---------------------------------------------------------------------------------
	swi	0x280000
	bx	lr
