#ifndef DREW_OPGP_KEY_H
#define DREW_OPGP_KEY_H

#ifdef __cplusplus
extern "C" {
#endif

#include <drew/drew.h>
#include <drew-opgp/parser.h>

typedef struct drew_opgp_sig_s *drew_opgp_sig_t;
typedef struct drew_opgp_uid_s *drew_opgp_uid_t;
typedef struct drew_opgp_key_s *drew_opgp_key_t;
typedef struct drew_opgp_prefs_s drew_opgp_prefs_t;

// Has this signature been checked?
#define DREW_OPGP_SIGNATURE_CHECKED			(1 << 0)
// Has this signature been hashed and checked against the left two?
#define DREW_OPGP_SIGNATURE_HASH_CHECK		(1 << 1)
// Did the signature validate?
#define DREW_OPGP_SIGNATURE_VALIDATED		(1 << 2)
// Has this signature been revoked?
#define DREW_OPGP_SIGNATURE_REVOKED			(1 << 3)
// Has this signature expired?
#define DREW_OPGP_SIGNATURE_EXPIRED			(1 << 4)
// Is this signature consistent?
#define DREW_OPGP_SIGNATURE_CONSISTENT		(1 << 5)
// Is this signature incomplete?
#define DREW_OPGP_SIGNATURE_INCOMPLETE		(1 << 6)
// Is this signature corrupt?
#define DREW_OPGP_SIGNATURE_CORRUPT			(1 << 7)
// Is this signature a self-signature?
#define DREW_OPGP_SIGNATURE_SELF_SIG		(1 << 8)
// Is this signature irrevocable?
#define DREW_OPGP_SIGNATURE_IRREVOCABLE		(1 << 9)
// Is this signature local?
#define DREW_OPGP_SIGNATURE_LOCAL			(1 << 10)
// Is this signature to be ignored?
// This is for algorithms that we do not have or key formats we do not support.
#define DREW_OPGP_SIGNATURE_IGNORED			(1 << 11)

#define DREW_OPGP_KEY_STATE_SYNCHRONIZED	(1 << 0)
#define DREW_OPGP_KEY_STATE_IGNORED			(1 << 1)

#define DREW_OPGP_SYNCHRONIZE_FORCE						(1 << 0)
#define DREW_OPGP_SYNCHRONIZE_BASIC						(1 << 1)
#define DREW_OPGP_SYNCHRONIZE_HASH_SIGS					(1 << 2)
#define DREW_OPGP_SYNCHRONIZE_VALIDATE_SELF_SIGNATURES	(1 << 3)
#define DREW_OPGP_SYNCHRONIZE_ALL						(~(1 << 0))

// A fingerprint, MD5 or SHA-1.
typedef uint8_t drew_opgp_fp_t[20];
// A hash value (could be as large as SHA-512).
typedef uint8_t drew_opgp_hash_t[64];
// A key ID.
typedef uint8_t drew_opgp_keyid_t[8];

DREW_SYM_PUBLIC
int drew_opgp_key_new(drew_opgp_key_t *key, const drew_loader_t *ldr);
DREW_SYM_PUBLIC
int drew_opgp_key_free(drew_opgp_key_t *key);
DREW_SYM_PUBLIC
int drew_opgp_key_clone(drew_opgp_key_t *newp, drew_opgp_key_t old);
DREW_SYM_PUBLIC
int drew_opgp_key_has_secret(drew_opgp_key_t key);
DREW_SYM_PUBLIC
int drew_opgp_key_has_usable_secret(drew_opgp_key_t key);
DREW_SYM_PUBLIC
int drew_opgp_key_can_sign(drew_opgp_key_t key);
DREW_SYM_PUBLIC
int drew_opgp_key_can_encrypt(drew_opgp_key_t key);
DREW_SYM_PUBLIC
int drew_opgp_key_is_revoked(drew_opgp_key_t key, int flags);
DREW_SYM_PUBLIC
int drew_opgp_key_is_expired(drew_opgp_key_t key, int flags);
DREW_SYM_PUBLIC
int drew_opgp_key_can_do(drew_opgp_key_t key, int flags);
DREW_SYM_PUBLIC
int drew_opgp_key_get_version(drew_opgp_key_t key);
DREW_SYM_PUBLIC
int drew_opgp_key_get_type(drew_opgp_key_t key);
DREW_SYM_PUBLIC
int drew_opgp_key_get_subkeys(drew_opgp_key_t key, drew_opgp_key_t *subkeys);
DREW_SYM_PUBLIC
int drew_opgp_key_generate(drew_opgp_key_t key, uint8_t algo, size_t nbits,
		size_t order, time_t expires);
DREW_SYM_PUBLIC
int drew_opgp_key_get_fingerprint(drew_opgp_key_t key, drew_opgp_fp_t fp);
DREW_SYM_PUBLIC
int drew_opgp_key_get_id(drew_opgp_key_t key, drew_opgp_id_t id);
DREW_SYM_PUBLIC
int drew_opgp_key_get_keyid(drew_opgp_key_t key, drew_opgp_keyid_t keyid);
DREW_SYM_PUBLIC
int drew_opgp_key_synchronize(drew_opgp_key_t key, int flags);
DREW_SYM_PUBLIC
int drew_opgp_key_load_public(drew_opgp_key_t key,
		const drew_opgp_packet_t *pkts, size_t npkts);
DREW_SYM_PUBLIC
int drew_opgp_key_load_private(drew_opgp_key_t key,
		const drew_opgp_packet_t *pkts, size_t npkts);
DREW_SYM_PUBLIC
int drew_opgp_key_store_public(drew_opgp_key_t key,
		const drew_opgp_packet_t *pkts, size_t npkts);
DREW_SYM_PUBLIC
int drew_opgp_key_store_private(drew_opgp_key_t key,
		const drew_opgp_packet_t *pkts, size_t npkts);
DREW_SYM_PUBLIC
int drew_opgp_key_get_preferences(drew_opgp_key_t key, int type,
		drew_opgp_prefs_t *prefs);
DREW_SYM_PUBLIC
int drew_opgp_key_get_user_ids(drew_opgp_key_t key, drew_opgp_uid_t **uids);


DREW_SYM_PUBLIC
int drew_opgp_uid_get_text(drew_opgp_uid_t uid, const char **p);
DREW_SYM_PUBLIC
int drew_opgp_uid_get_signatures(drew_opgp_uid_t uid, drew_opgp_sig_t **sigs);

#ifdef __cplusplus
}
#endif

#endif
