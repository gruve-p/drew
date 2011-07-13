#include "internal.h"

#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <map>
#include <vector>

#include <drew/drew.h>
#include <drew/plugin.h>

#include <drew-opgp/drew-opgp.h>
#include <drew-opgp/key.h>
#include <drew-opgp/keystore.h>

#include "structs.h"
#include "util.hh"

#define CHUNKSZ	64

#define DREW_OPGP_KEYSTORE_UNSYNCHRONIZED		(1 << 0)

#define TYPE_NIL	0
#define TYPE_KEY	1
#define TYPE_SIG	2
#define TYPE_UID	3
#define TYPE_SUB	4
#define TYPE_MPI	5

typedef BigEndian E;

// FIXME: clone each of these contexts and on destruction, free them.
struct Item
{
	Item() : type(TYPE_NIL), key(0), sig(0), uid(0), pub(0), mpi(0) {}
	Item(drew_opgp_key_t keyp) :
		type(TYPE_KEY), key(keyp), sig(0), uid(0), pub(0), mpi(0) {}
	Item(drew_opgp_sig_t sigp) :
		type(TYPE_SIG), key(0), sig(sigp), uid(0), pub(0), mpi(0) {}
	Item(drew_opgp_uid_t uidp) :
		type(TYPE_UID), key(0), sig(0), uid(uidp), pub(0), mpi(0) {}
	Item(pubkey_t *pubp) :
		type(TYPE_SUB), key(0), sig(0), uid(0), pub(pubp), mpi(0) {}
	Item(drew_opgp_mpi_t *mpip) :
		type(TYPE_MPI), key(0), sig(0), uid(0), pub(0), mpi(mpip) {}
	int type;
	drew_opgp_key_t key;
	drew_opgp_sig_t sig;
	drew_opgp_uid_t uid;
	pubkey_t *pub;
	drew_opgp_mpi_t *mpi;
};

struct DrewID
{
	DrewID()
	{
		Reset();
	}
	DrewID(drew_opgp_id_t idp)
	{
		memcpy(this->id, idp, sizeof(this->id));
	}
	bool operator <(const DrewID & kid) const
	{
		return memcmp(this->id, kid.id, sizeof(this->id)) < 0;
	}
	bool operator ==(const DrewID & kid) const
	{
		return !memcmp(this->id, kid.id, sizeof(this->id));
	}
	void Reset()
	{
		memset(id, 0, sizeof(id));
	}
	void Write(int fd)
	{
		write(fd, id, sizeof(id));
	}
	operator uint8_t *()
	{
		return id;
	}
	operator const uint8_t *() const
	{
		return id;
	}
	uint8_t &operator[](int offset)
	{
		return id[offset];
	}
	const uint8_t &operator[](int offset) const
	{
		return id[offset];
	}
	drew_opgp_id_t id;
};

typedef std::map<DrewID, Item> ItemStore;

typedef uint8_t chunk_t[64];

struct Chunk
{
	Chunk()
	{
		Reset();
	}
	void Reset()
	{
		memset(chunk, 0, sizeof(chunk));
	}
	operator uint8_t *()
	{
		return chunk;
	}
	uint8_t &operator[](int offset)
	{
		return chunk[offset];
	}
	chunk_t chunk;
};

struct KeyChunk : public Chunk
{
	KeyChunk()
	{
		this->Reset();
	}
};

class IDConverter
{
	public:
		void Add(DrewID id)
		{
			ids.push_back(id);
		}
		// Assumes that c is large enough and that ids.size() is even.
		void ToChunks(Chunk *c)
		{
			for (size_t i = 0, j = 0; i < ids.size(); i += 2, j++) {
				memcpy(c[j]+0x00, ids[i+0], 0x20);
				memcpy(c[j]+0x20, ids[i+1], 0x20);
			}
		}
	protected:
	private:
		std::vector<DrewID> ids;
};

class Backend
{
	public:
		// Is it possible to access an arbitrary item immediately upon opening?
		virtual bool IsRandomAccess() const = 0;
		virtual void Open(const char *, bool) = 0;
		virtual void Close() = 0;
		virtual int GetError() const = 0;
		virtual bool IsOpen() const = 0;
		virtual void WriteChunks(const KeyChunk &, const Chunk *, size_t) = 0;
		virtual void ReadKeyChunk(KeyChunk &) = 0;
		virtual Chunk *ReadChunks(const KeyChunk &, size_t &) = 0;
	protected:
	private:
};

class FileBackend : public Backend
{
	public:
		FileBackend() : fd(-1)
		{
		}
		~FileBackend()
		{
			Close();
		}
		virtual bool IsRandomAccess() const
		{
			return false;
		}
		virtual void Open(const char *filename, bool write)
		{
			if (write) {
				if ((fd = open(filename, O_WRONLY|O_CREAT|O_TRUNC, 0600)) < 0)
					error = -errno;
			}
			else if ((fd = open(filename, O_RDONLY)) < 0)
				error = -errno;
		}
		virtual void Close()
		{
			if (fd >= 0)
				close(fd);
			fd = -1;
		}
		virtual int GetError() const
		{
			return error;
		}
		virtual bool IsOpen() const
		{
			return fd >= 0;
		}
		virtual void WriteChunks(const KeyChunk &k, const Chunk *c,
				size_t nchunks)
		{
			write(fd, k.chunk, sizeof(k.chunk));
			for (size_t i = 0; i < nchunks; i++)
				write(fd, c[i].chunk, sizeof(c[i].chunk));
		}
		virtual void ReadKeyChunk(KeyChunk &k)
		{
			ssize_t res;
			res = read(fd, k.chunk, sizeof(k.chunk));
			if (res < 0)
				throw -errno;
			if (!res)
				throw 0;
		}
		virtual Chunk *ReadChunks(const KeyChunk &k, size_t &nchunks)
		{
			Chunk c, *cp;
			read(fd, c.chunk, sizeof(c.chunk));
			nchunks = E::Convert<uint32_t>(c.chunk) + 1;
			cp = new Chunk[nchunks];
			cp[0] = c;
			for (size_t i = 1; i < nchunks; i++)
				read(fd, cp[i], sizeof(cp[i].chunk));
			return cp;
		}
	protected:
		int fd;
		int error;
	private:	
};

struct drew_opgp_keystore_s {
	int major, minor;
	int flags;
	Backend *b;
	ItemStore items;
};

#define CHUNK_TYPE_HEADER			0xbc
#define CHUNK_TYPE_ID				0xbd
#define CHUNK_TYPE_FP16				0xbe
#define CHUNK_TYPE_FP20				0xbf

#define CHUNK_HEADER_CONSTANT		0xdb

#define CHUNK_ID_PUBKEY				0x69
#define CHUNK_ID_PUBSUBKEY			0x6a
#define CHUNK_ID_UID				0x6b
#define CHUNK_ID_SIG				0x6c
#define CHUNK_ID_MPI				0x6d

extern "C"
int drew_opgp_keystore_new(drew_opgp_keystore_t *ksp, const drew_loader_t *ldr)
{
	drew_opgp_keystore_t ks;
	ks = new drew_opgp_keystore_s;
	if (!ks)
		return -ENOMEM;

	ks->major = 0x00;
	ks->minor = 0x01;
	ks->b = 0;
	*ksp = ks;
	return 0;
}

extern "C"
int drew_opgp_keystore_free(drew_opgp_keystore_t *ksp)
{
	delete (*ksp)->b;
	delete *ksp;
	*ksp = 0;
	return 0;
}

extern "C"
int drew_opgp_keystore_set_backend(drew_opgp_keystore_t ks, const char *backend)
{
	delete ks->b;
	if (!strcmp(backend, "file"))
		ks->b = new FileBackend;
	else if (!strcmp(backend, "bdb"))
		return -DREW_ERR_NOT_IMPL;
	else
		return -DREW_ERR_INVALID;
	return 0;
}

extern "C"
int drew_opgp_keystore_load(drew_opgp_keystore_t ks, const char *filename)
{
	return -DREW_ERR_NOT_IMPL;
}

#define ROUND(x) (RoundUpToPowerOf2(x, 2))

/* There is intentionally no external documentation for this format.  Because
 * the format includes precomputed information, like whether a signature is
 * valid, it should not be read and written by external programs.
 *
 * The first chunk of each item starts with a 32-byte unique identifier field.
 * This is followed by a byte indicating the meaning of this field, whether the
 * field is an internal SHA-256 ID, a zero-padded 16- or 20-byte fingerprint, or
 * the special header (which is always all-zero).
 * 
 * All of the first chunk must be predictable, because this chunk will be stored
 * as the key in a database.  Other statistical and structural information must
 * be stored in the immediately following chunk, the info chunk.  For
 * efficiency, all 16-bit and 32-bit numbers are stored 4-byte aligned, and
 * 64-bit numbers are stored 8-byte aligned (including OpenPGP key IDs).  All
 * data is big-endian.
 */
static void store_pubkey(drew_opgp_keystore_t ks, const drew_opgp_id_t id,
		drew_opgp_key_t key, pubkey_t *pub, int type, size_t npubsubs)
{
	KeyChunk fchunk;
	memcpy(fchunk, id, sizeof(drew_opgp_id_t));
	fchunk[0x20] = CHUNK_TYPE_ID;
	fchunk[0x21] = type;
	fchunk[0x22] = ks->major;
	fchunk[0x23] = ks->minor;

	uint32_t nmpis = 0;
	for (size_t i = 0; i < DREW_OPGP_MAX_MPIS && pub->mpi[i].len; i++, nmpis++);
	uint32_t off = ROUND(pub->nuids) + ROUND(pub->nsigs) + ROUND(npubsubs)
		+ ROUND(nmpis);
	Chunk *c = new Chunk[off + 1];
	// Number of subsequent chunks.
	E::Convert(c[0], off);
	E::Convert<uint16_t>(c[0]+0x04, pub->state);
	c[0][0x05] = pub->ver;
	c[0][0x06] = pub->algo;
	E::Convert<uint32_t>(c[0]+0x08, pub->ctime);
	E::Convert<uint32_t>(c[0]+0x0c, pub->etime);
	E::Convert<uint32_t>(c[0]+0x10, pub->nuids);
	E::Convert<uint32_t>(c[0]+0x14, pub->nsigs);
	memcpy(c[0]+0x18, pub->keyid, 8);
	memcpy(c[0]+0x20, pub->fp, (pub->ver == 4 ? 20 : 16));
	E::Convert<uint32_t>(c[0]+0x34, npubsubs);
	E::Convert<uint32_t>(c[0]+0x38, nmpis);

	IDConverter idc;
	// Write the primary UID ID first, followed by the others.
	if (pub->nuids && pub->theuid)
		idc.Add(DrewID(pub->theuid->id));
	for (size_t i = 0; i < pub->nuids; i++)
		if (pub->theuid != pub->uids+i)
			idc.Add((pub->uids[i].id));
	if (pub->nuids & 1)
		idc.Add(DrewID());

	for (size_t i = 0; i < pub->nsigs; i++)
		idc.Add(DrewID(pub->sigs[i].id));
	if (pub->nsigs & 1)
		idc.Add(DrewID());

	for (size_t i = 0; i < npubsubs; i++)
		idc.Add(DrewID(key->pubsubs[i].id));
	if (pub->nsigs & 1)
		idc.Add(DrewID());

	for (size_t i = 0; i < nmpis; i++)
		idc.Add(DrewID(pub->mpi[i].id));
	if (nmpis & 1)
		idc.Add(DrewID());

	idc.ToChunks(c+1);
	ks->b->WriteChunks(fchunk, c, off+1);
	delete[] c;
}

static void store_subkey(drew_opgp_keystore_t ks, const drew_opgp_id_t id,
		pubkey_t *pub)
{
	if (!pub)
		return;
	store_pubkey(ks, id, NULL, pub, CHUNK_ID_PUBSUBKEY, 0);
}

static void store_key(drew_opgp_keystore_t ks, const drew_opgp_id_t id,
		drew_opgp_key_t key)
{
	if (!key)
		return;
	store_pubkey(ks, id, key, &key->pub, CHUNK_ID_PUBKEY, key->npubsubs);
}

static void store_mpi(drew_opgp_keystore_t ks, const drew_opgp_id_t id,
		drew_opgp_mpi_t *mpi)
{
	if (!mpi)
		return;
	KeyChunk fchunk;
	memcpy(fchunk, id, sizeof(drew_opgp_id_t));
	fchunk[0x20] = CHUNK_TYPE_ID;
	fchunk[0x21] = CHUNK_ID_MPI;
	fchunk[0x22] = ks->major;
	fchunk[0x23] = ks->minor;

	uint32_t nbytes = (mpi->len + 7) / 8;
	uint32_t nchunks = DivideAndRoundUp(nbytes, 0x40);
	Chunk *c = new Chunk[nchunks + 1];
	// Number of subsequent chunks.
	E::Convert(c[0], nchunks);
	E::Convert<uint32_t>(c[0]+0x04, mpi->len);
	E::Convert<uint32_t>(c[0]+0x08, nbytes);

	for (size_t i = 1, off = 0; i <= nchunks; i++, off += 0x40)
		memcpy(c[i], mpi->data+off, std::min<size_t>(0x40, nbytes-off));
	ks->b->WriteChunks(fchunk, c, nchunks+1);
	delete[] c;
}

static void store_sig(drew_opgp_keystore_t ks, const drew_opgp_id_t id,
		drew_opgp_sig_t sig)
{
	if (!sig)
		return;
	KeyChunk fchunk;
	memcpy(fchunk, id, sizeof(drew_opgp_id_t));
	fchunk[0x20] = CHUNK_TYPE_ID;
	fchunk[0x21] = CHUNK_ID_SIG;
	fchunk[0x22] = ks->major;
	fchunk[0x23] = ks->minor;

	uint32_t nmpis = 0;
	for (size_t i = 0; i < DREW_OPGP_MAX_MPIS && sig->mpi[i].len; i++, nmpis++);
	uint32_t hchunks = 0, uchunks = 0;
	for (size_t i = 0; i < sig->nhashed; i++)
		hchunks += DivideAndRoundUp(sig->hashed[i].len, 0x40);
	for (size_t i = 0; i < sig->nunhashed; i++)
		uchunks += DivideAndRoundUp(sig->unhashed[i].len, 0x40);
	uint32_t nhashedlens = DivideAndRoundUp(sig->nhashed * 4, 0x40);
	uint32_t nunhashedlens = DivideAndRoundUp(sig->nunhashed * 4, 0x40);
	uint32_t nhashed = DivideAndRoundUp(sig->hashedlen, 0x40);
	uint32_t nunhashed = DivideAndRoundUp(sig->unhashedlen, 0x40);
	uint32_t nchunks = nhashedlens + nunhashedlens + nhashed + nunhashed +
		hchunks + uchunks + ROUND(nmpis) + 3;
	Chunk *c = new Chunk[nchunks + 2];
	// Number of subsequent chunks.
	E::Convert(c[0], nchunks + 1);
	c[0][0x04] = sig->ver;
	c[0][0x05] = sig->type;
	c[0][0x06] = sig->pkalgo;
	c[0][0x07] = sig->mdalgo;
	E::Convert<uint32_t>(c[0]+0x08, sig->ctime);
	E::Convert<uint32_t>(c[0]+0x0c, sig->etime);
	memcpy(c[0]+0x10, sig->keyid, 8);
	E::Convert<uint32_t>(c[0]+0x18, sig->flags);
	E::Convert<uint32_t>(c[0]+0x20, sig->selfsig.keyflags);
	E::Convert<uint32_t>(c[0]+0x24, sig->selfsig.keyexp);
	c[0][0x28] = sig->left[0];
	c[0][0x29] = sig->left[1];
	// Two empty bytes.
	c[0][0x2c] = sig->selfsig.prefs[0].len;
	c[0][0x2d] = sig->selfsig.prefs[1].len;
	c[0][0x2e] = sig->selfsig.prefs[2].len;
	c[0][0x2f] = nmpis;
	E::Convert<uint32_t>(c[0]+0x30, sig->nhashed);
	E::Convert<uint32_t>(c[0]+0x34, sig->nunhashed);
	E::Convert<uint32_t>(c[0]+0x38, sig->hashedlen);
	E::Convert<uint32_t>(c[0]+0x3c, sig->unhashedlen);

	memcpy(c[1], sig->hash, 0x40);

	uint32_t off = 2, offset = 0;
	for (size_t i = 0; i < sig->nhashed; i++, offset += 0x4) {
		if (offset == 0x40) {
			off++;
			offset = 0x00;
		}
		E::Convert<uint16_t>(c[off]+offset+0x00, sig->hashed[i].len);
		c[off][offset+0x02] = sig->hashed[i].type |
			(sig->hashed[i].critical ? 0x80 : 0);
		c[off][offset+0x03] = sig->hashed[i].lenoflen;
	}
	if (offset)
		off++;
	offset = 0x00;

	for (size_t i = 0; i < sig->nunhashed; i++, offset += 0x4) {
		if (offset == 0x40) {
			off++;
			offset = 0x00;
		}
		E::Convert<uint16_t>(c[off]+offset+0x00, sig->unhashed[i].len);
		c[off][offset+0x02] = sig->unhashed[i].type |
			(sig->unhashed[i].critical ? 0x80 : 0);
		c[off][offset+0x03] = sig->unhashed[i].lenoflen;
	}
	if (offset)
		off++;
	offset = 0x00;

	for (size_t j = 0; j < sig->nhashed; j++)
		for (size_t i = off, offset = 0; offset < sig->hashed[j].len;
				i++, offset += 0x40)
			memcpy(c[i], sig->hashed[j].data+offset,
					std::min<size_t>(0x40, sig->hashed[j].len-offset));

	for (size_t j = 0; j < sig->nunhashed; j++)
		for (size_t i = off, offset = 0; offset < sig->unhashed[j].len;
				i++, offset += 0x40)
			memcpy(c[i], sig->unhashed[j].data+offset,
					std::min<size_t>(0x40, sig->unhashed[j].len-offset));

	for (size_t i = off, offset = 0; offset < sig->nhashed; i++, offset += 0x40)
		memcpy(c[i], sig->hasheddata+off,
				std::min<size_t>(0x40, sig->hashedlen-off));

	for (size_t i = off, offset = 0; offset < sig->nunhashed;
			i++, offset += 0x40)
		memcpy(c[i], sig->unhasheddata+off,
				std::min<size_t>(0x40, sig->unhashedlen-off));

	for (size_t i = 0; i < 3; i++, off++)
		memcpy(c[off], sig->selfsig.prefs[i].vals,
				sizeof(sig->selfsig.prefs[i].vals));

	IDConverter idc;
	for (size_t i = 0; i < nmpis; i++)
		idc.Add(DrewID(sig->mpi[i].id));
	if (nmpis & 1)
		idc.Add(DrewID());

	idc.ToChunks(c+off);
	ks->b->WriteChunks(fchunk, c, nchunks+2);
	delete[] c;
}

static void store_uid(drew_opgp_keystore_t ks, const drew_opgp_id_t id,
		drew_opgp_uid_t uid)
{
	if (!uid)
		return;
	KeyChunk fchunk;
	memcpy(fchunk, id, sizeof(drew_opgp_id_t));
	fchunk[0x20] = CHUNK_TYPE_ID;
	fchunk[0x21] = CHUNK_ID_UID;
	fchunk[0x22] = ks->major;
	fchunk[0x23] = ks->minor;

	uint32_t nchunks = ROUND(uid->nsigs) + DivideAndRoundUp(uid->len + 1, 0x40);
	Chunk *c = new Chunk[nchunks + 1];
	// Number of subsequent chunks.
	E::Convert(c[0], nchunks);
	E::Convert<uint32_t>(c[0]+0x04, uid->nsigs);
	E::Convert<uint32_t>(c[0]+0x08, uid->nselfsigs);
	E::Convert<uint32_t>(c[0]+0x0c, uid->len);

	IDConverter idc;
	if (uid->theselfsig && uid->nsigs)
		idc.Add(DrewID(uid->theselfsig->id));

	for (size_t i = 0; i < uid->nselfsigs; i++)
		idc.Add(DrewID(uid->selfsigs[i]->id));

	for (size_t i = 0; i < uid->nsigs; i++) {
		bool found = false;
		for (size_t j = 0; j < uid->nselfsigs; j++)
			if (uid->sigs+i == uid->selfsigs[j])
				found = true;
		if (!found)
			idc.Add(DrewID(uid->sigs[i].id));
	}
	if (uid->nsigs & 1)
		idc.Add(DrewID());

	for (size_t i = ROUND(uid->nsigs) + 1, off = 0; i <= nchunks;
			i++, off += 0x40)
		memcpy(c[i], uid->s+off, std::min<size_t>(0x40, (uid->len+1)-off));

	idc.ToChunks(c+1);
	ks->b->WriteChunks(fchunk, c, nchunks+1);
	delete[] c;
}

static void store_header(drew_opgp_keystore_t ks)
{
	KeyChunk chunk;
	chunk[0x20] = CHUNK_TYPE_HEADER;
	chunk[0x21] = CHUNK_HEADER_CONSTANT;
	chunk[0x22] = ks->major;
	chunk[0x23] = ks->minor;
	chunk[0x28] = 'D';
	chunk[0x29] = 'r';
	chunk[0x2a] = 'e';
	chunk[0x2b] = 'w';
	// Special signature.  Left part of the SHA-256 value of
	// "\xbc\xdb\x00\x00DrewEngine".
	chunk[0x30] = 0x6d;
	chunk[0x31] = 0x86;
	chunk[0x32] = 0x6b;
	chunk[0x33] = 0x1a;
	chunk[0x34] = 0xa1;
	chunk[0x35] = 0xb3;
	chunk[0x36] = 0x03;
	chunk[0x37] = 0x96;
	chunk[0x38] = 0x2f;
	chunk[0x39] = 0x54;
	chunk[0x3a] = 0x56;
	chunk[0x3b] = 0x38;
	chunk[0x3c] = 0xf8;
	chunk[0x3d] = 0x4c;
	chunk[0x3e] = 0x01;
	chunk[0x3f] = 0x6f;
	ks->b->WriteChunks(chunk, 0, 0);
}

extern "C"
int drew_opgp_keystore_store(drew_opgp_keystore_t ks, const char *filename)
{
	ks->b->Open(filename, true);

	if (!ks->b->IsOpen())
		return ks->b->GetError();

	store_header(ks);
	typedef ItemStore::iterator it_t;
	if (!ks->b->IsRandomAccess()) {
		// Because we need references to items to be resolvable, we store MPIs
		// first, then signatures, then user IDs, then pubkeys, and then keys.
		for (it_t it = ks->items.begin(); it != ks->items.end(); it++)
			store_mpi(ks, it->first.id, it->second.mpi);
		for (it_t it = ks->items.begin(); it != ks->items.end(); it++)
			store_sig(ks, it->first.id, it->second.sig);
		for (it_t it = ks->items.begin(); it != ks->items.end(); it++)
			store_uid(ks, it->first.id, it->second.uid);
		for (it_t it = ks->items.begin(); it != ks->items.end(); it++)
			store_subkey(ks, it->first.id, it->second.pub);
		for (it_t it = ks->items.begin(); it != ks->items.end(); it++)
			store_key(ks, it->first.id, it->second.key);
	}
	else {
		// We can efficiently access any arbitrary record, so use a one-pass
		// algorithm.
		for (it_t it = ks->items.begin(); it != ks->items.end(); it++) {
			store_mpi(ks, it->first.id, it->second.mpi);
			store_sig(ks, it->first.id, it->second.sig);
			store_uid(ks, it->first.id, it->second.uid);
			store_subkey(ks, it->first.id, it->second.pub);
			store_key(ks, it->first.id, it->second.key);
		}
	}
	return 0;
}

extern "C"
int drew_opgp_keystore_update_sigs(drew_opgp_keystore_t ks,
		drew_opgp_sig_t *sigs, size_t nsigs, int flags)
{
	for (size_t i = 0; i < nsigs; i++) {
		ks->items[DrewID(sigs[i]->id)] = Item(sigs[i]);
		for (size_t j = 0; j < DREW_OPGP_MAX_MPIS && sigs[i]->mpi[j].len;
				j++)
			ks->items[DrewID(sigs[i]->mpi[j].id)] =
				Item(sigs[i]->mpi+j);
	}
	return 0;
}

extern "C"
int drew_opgp_keystore_update_sig(drew_opgp_keystore_t ks, drew_opgp_sig_t sig,
		int flags)
{
	return drew_opgp_keystore_update_sigs(ks, &sig, 1, flags);
}

extern "C"
int drew_opgp_keystore_update_user_ids(drew_opgp_keystore_t ks,
		drew_opgp_uid_t *uids, size_t nuids, int flags)
{
	for (size_t i = 0; i < nuids; i++) {
		ks->items[DrewID(uids[i]->id)] = Item(uids[i]);
		for (size_t j = 0; j < uids[i]->nsigs; j++)
			drew_opgp_keystore_update_sig(ks, uids[i]->sigs+j, flags);
	}
	return 0;
}

extern "C"
int drew_opgp_keystore_update_user_id(drew_opgp_keystore_t ks,
		drew_opgp_uid_t uid, int flags)
{
	return drew_opgp_keystore_update_user_ids(ks, &uid, 1, flags);
}

extern "C"
int drew_opgp_keystore_add_keys(drew_opgp_keystore_t ks,
		drew_opgp_key_t *keys, size_t nkeys, int flags)
{
	for (size_t i = 0; i < nkeys; i++) {
		ks->items.insert(std::pair<DrewID, Item>(DrewID(keys[i]->id),
					Item(keys[i])));
	}
	return 0;
}

extern "C"
int drew_opgp_keystore_add_key(drew_opgp_keystore_t ks, drew_opgp_key_t key,
		int flags)
{
	return drew_opgp_keystore_add_keys(ks, &key, 1, flags);
}

extern "C"
int drew_opgp_keystore_update_keys(drew_opgp_keystore_t ks,
		drew_opgp_key_t *keys, size_t nkeys, int flags)
{
	for (size_t i = 0; i < nkeys; i++) {
		ks->items[DrewID(keys[i]->pub.id)] = Item(keys[i]);
		for (size_t j = 0; j < keys[i]->pub.nuids; j++)
			drew_opgp_keystore_update_user_id(ks, keys[i]->pub.uids+j, flags);
		for (size_t j = 0; j < DREW_OPGP_MAX_MPIS && keys[i]->pub.mpi[j].len;
				j++)
			ks->items[DrewID(keys[i]->pub.mpi[j].id)] =
				Item(keys[i]->pub.mpi+j);
	}
	return 0;
}

extern "C"
int drew_opgp_keystore_update_key(drew_opgp_keystore_t ks, drew_opgp_key_t key,
		int flags)
{
	return drew_opgp_keystore_update_keys(ks, &key, 1, flags);
}

extern "C"
int drew_opgp_keystore_lookup_by_id(drew_opgp_keystore_t ks,
		drew_opgp_key_t *key, drew_opgp_id_t id)
{
	ItemStore::iterator it;

	it = ks->items.find(DrewID(id));
	if (it == ks->items.end())
		return 0;
	if (key)
		*key = it->second.key;
	return 1;
}

// TODO: store a mapping from key ID to (sequence of) Drew ID in the store.
extern "C"
int drew_opgp_keystore_lookup_by_keyid(drew_opgp_keystore_t ks,
		drew_opgp_key_t *key, size_t nkeys, drew_opgp_keyid_t keyid)
{
	// TODO: look up subkeys, too.
	size_t nitems = 0;
	typedef ItemStore::iterator it_t;
	for (it_t it = ks->items.begin(); it != ks->items.end(); it++) {
		if (it->second.key && !memcmp(it->second.key->pub.keyid, keyid, 8)) {
			if (key && nitems < nkeys)
				key[nitems] = it->second.key;
			nitems++;
		}
	}
	return nitems;
}

extern "C"
int drew_opgp_keystore_get_keys(drew_opgp_keystore_t ks, drew_opgp_key_t *key,
		size_t nkeys)
{
	// TODO: look up subkeys, too.
	size_t nitems = 0;
	typedef ItemStore::iterator it_t;
	for (it_t it = ks->items.begin(); it != ks->items.end(); it++) {
		if (it->second.key) {
			if (key && nitems < nkeys)
				key[nitems] = it->second.key;
			nitems++;
		}
	}
	return nitems;
}

extern "C"
int drew_opgp_keystore_check(drew_opgp_keystore_t ks, int flags)
{
	return -DREW_ERR_NOT_IMPL;
}
