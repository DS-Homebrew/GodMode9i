#pragma once

#include <assert.h>
#include <stdint.h>

#ifdef _MSC_VER
#pragma pack(push, 1)
#define PACKED
#elif ! defined PACKED
#define PACKED __attribute__ ((__packed__))
#endif

#define RSA_2048_LEN (2048/8)

// most, if not all, are big endian

// http://problemkaputt.de/gbatek.htm#dsisdmmcdsiwareticketsandtitlemetadata
// http://dsibrew.org/wiki/Ticket
// http://wiibrew.org/wiki/Ticket
typedef struct {
	uint8_t sig_type[4];
	uint8_t sig[RSA_2048_LEN];
	uint8_t padding0[0x3c];
	char issuer[0x40];
	uint8_t ecdh[0x3c];
	uint8_t padding1[3];
	uint8_t encrypted_title_key[0x10];
	uint8_t unknown0;
	uint8_t ticket_id[8];
	uint8_t console_id[4];
	uint8_t title_id[8];
	uint8_t unknown1[2];
	uint8_t version[2];
	uint8_t permitted_titles_mask[4];
	uint8_t permit_mask[4];
	uint8_t title_export_allowed;
	uint8_t common_key_index;
	uint8_t unknown[0x30];
	uint8_t content_access_permissions[0x40];
	uint8_t padding2[2];
	uint8_t time_limits[2 * 8 * sizeof(uint32_t)];
} PACKED ticket_v0_t;

static_assert(sizeof(ticket_v0_t) == 0x2a4, "invalid sizeof(ticket_v0_t)");

// http://dsibrew.org/wiki/Tmd
// http://wiibrew.org/wiki/Title_metadata
typedef struct {
	uint8_t sig_type[4];
	uint8_t sig[RSA_2048_LEN];
	uint8_t padding0[0x3c];
	char issuer[0x40];
	uint8_t version;
	uint8_t ca_crl_version;
	uint8_t signer_crl_version;
	uint8_t padding1;
	uint8_t system_version[8];
	uint8_t title_id[8];
	uint8_t title_type[4];
	uint8_t group_id[2];
	uint8_t public_save_size[4];
	uint8_t private_save_size[4];
	uint8_t padding2[8];
	uint8_t parent_control[0x10];
	uint8_t padding3[0x1e];
	uint8_t access_rights[4];
	uint8_t title_version[2];
	uint8_t num_content[2];
	uint8_t boot_index[2];
	uint8_t padding4[2];
} PACKED tmd_header_v0_t;

static_assert(sizeof(tmd_header_v0_t) == 0x1e4, "invalid sizeof(tmd_header_v0_t)");

typedef struct {
	uint8_t content_id[4];
	uint8_t index[2];
	uint8_t type[2];
	uint8_t size[8];
	uint8_t sha1[20];
} PACKED tmd_content_v0_t;

static_assert(sizeof(tmd_content_v0_t) == 0x24, "invalid sizeof(tmd_contend_v0_t)");

// used in ticket encryption
// http://problemkaputt.de/gbatek.htm#dsiesblockencryption

#define AES_CCM_MAC_LEN 0x10
#define AES_CCM_NONCE_LEN 0x0c

typedef struct {
	uint8_t ccm_mac[AES_CCM_MAC_LEN];
	union {
		struct {
			uint8_t fixed_3a;
			uint8_t nonce[AES_CCM_NONCE_LEN];
			uint8_t len24be[3];
		};
		struct {
			uint8_t padding[AES_CCM_NONCE_LEN];
			// defined for convenience, it's still big endian with only 24 effective bits
			// read it as 32 bit big endian and discard the highest 8 bits
			uint8_t len32be[4];
		};
		uint8_t encrypted[0x10];
	};
} PACKED es_block_footer_t;

static_assert(sizeof(es_block_footer_t) == 0x20, "invalid sizeof(es_block_footer_t)");

// used in cert.sys
// http://problemkaputt.de/gbatek.htm#dsisdmmcfirmwaredevkpandcertsyscertificatefiles
// "DSi SD/MMC Firmware dev.kp and cert.sys Certificate Files"
typedef struct {
	uint32_t signature_type;
	uint8_t signature[RSA_2048_LEN];
	uint8_t padding0[0x3c];
	char signature_name[0x40];
	uint32_t key_type;
	char key_name[0x40];
	uint32_t key_flags;
	uint8_t rsa_key[RSA_2048_LEN];
	uint8_t rsa_exp[4];
	uint8_t padding1[0x34];
} PACKED cert_t;

static_assert(sizeof(cert_t) == 0x300, "invalid sizeof(cert_t)");

#ifdef _MSC_VER
#pragma pack(pop)
#endif
#undef PACKED
