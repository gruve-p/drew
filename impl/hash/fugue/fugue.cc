/*-
 * Copyright © 2011 brian m. carlson
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <endian.h>

#include "fugue.hh"
#include "testcase.hh"
#include "util.hh"
#include "hash-plugin.hh"

extern "C" {
PLUGIN_STRUCTURE(fugue256, Fugue256)
PLUGIN_STRUCTURE(fugue224, Fugue224)
PLUGIN_DATA_START()
PLUGIN_DATA(fugue256, "Fugue-256")
PLUGIN_DATA(fugue224, "Fugue-224")
PLUGIN_DATA_END()
PLUGIN_INTERFACE(fugue)

static int fugue256test(void *, const drew_loader_t *)
{
	int res = 0;

	using namespace drew;
	
	res |= !HashTestCase<Fugue256>("\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0a\x0b\x0c\x0d\x0e\x0f\x10\x11\x12\x13\x14\x15\x16\x17\x18\x19\x1a\x1b\x1c\x1d\x1e\x1f\x20\x21\x22\x23\x24\x25\x26\x27\x28\x29\x2a\x2b\x2c\x2d\x2e\x2f\x30\x31\x32\x33\x34\x35\x36\x37\x38\x39\x3a\x3b\x3c\x3d\x3e\x3f\x40", 1).Test("3b3c5551d9da76e55e9f1f927a88de9bddf082021783f2ea4663558c65a01630");
	res <<= 1;
	res |= !HashTestCase<Fugue256>("", 0).Test("d6ec528980c130aad1d1acd28b9dd8dbdeae0d79eded1fca72c2af9f37c2246f");
	res <<= 1;
	res |= !HashTestCase<Fugue256>("\xcc", 1).Test("b894eb2df58162f6c48d495f156e73bd086dd13db407ee38781177bb23d129bb");

	return res;
}

static int fugue224test(void *, const drew_loader_t *)
{
	int res = 0;

	using namespace drew;
	
	res |= !HashTestCase<Fugue224>("\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0a\x0b\x0c\x0d\x0e\x0f\x10\x11\x12\x13\x14\x15\x16\x17\x18\x19\x1a\x1b\x1c\x1d\x1e\x1f\x20\x21\x22\x23\x24\x25\x26\x27\x28\x29\x2a\x2b\x2c\x2d\x2e\x2f\x30\x31\x32\x33\x34\x35\x36\x37\x38\x39\x3a\x3b\x3c\x3d\x3e\x3f\x40", 1).Test("495659c190b12f491b66c43a8d9004aaa1437c60e402045dbe2a645c");
	res <<= 1;
	res |= !HashTestCase<Fugue224>("", 0).Test("e2cd30d51a913c4ed2388a141f90caa4914de43010849e7b8a7a9ccd");
	res <<= 1;
	res |= !HashTestCase<Fugue224>("\xcc", 1).Test("34602ea95b2b9936b9a04ba14b5dc463988df90b1a46f90dd716b60f");

	return res;
}
}

typedef drew::Fugue256::endian_t E;

drew::Fugue256::Fugue256()
{
	Reset();
}

void drew::Fugue256::Reset()
{
	memset(m_hash, 0, sizeof(*m_hash) * 22);
	m_hash[22] = 0xe952bdde;
	m_hash[23] = 0x6671135f;
	m_hash[24] = 0xe0d4f668;
	m_hash[25] = 0xd2b0b594;
	m_hash[26] = 0xf96c621d;
	m_hash[27] = 0xfbf929de;
	m_hash[28] = 0x9149e899;
	m_hash[29] = 0x34f8c248;
	Initialize();
}

drew::Fugue224::Fugue224()
{
	Reset();
}

void drew::Fugue224::Reset()
{
	memset(m_hash, 0, sizeof(*m_hash) * 23);
	m_hash[23] = 0xf4c9120d;
	m_hash[24] = 0x6286f757;
	m_hash[25] = 0xee39e01c;
	m_hash[26] = 0xe074e3cb;
	m_hash[27] = 0xa1127c62;
	m_hash[28] = 0x9a43d215;
	m_hash[29] = 0xbd8d679a;
	Initialize();
}

static const uint8_t sbox[] = {
    0x63, 0x7c, 0x77, 0x7b, 0xf2, 0x6b, 0x6f, 0xc5,
    0x30, 0x01, 0x67, 0x2b, 0xfe, 0xd7, 0xab, 0x76,
    0xca, 0x82, 0xc9, 0x7d, 0xfa, 0x59, 0x47, 0xf0,
    0xad, 0xd4, 0xa2, 0xaf, 0x9c, 0xa4, 0x72, 0xc0,
    0xb7, 0xfd, 0x93, 0x26, 0x36, 0x3f, 0xf7, 0xcc,
    0x34, 0xa5, 0xe5, 0xf1, 0x71, 0xd8, 0x31, 0x15,
    0x04, 0xc7, 0x23, 0xc3, 0x18, 0x96, 0x05, 0x9a,
    0x07, 0x12, 0x80, 0xe2, 0xeb, 0x27, 0xb2, 0x75,
    0x09, 0x83, 0x2c, 0x1a, 0x1b, 0x6e, 0x5a, 0xa0,
    0x52, 0x3b, 0xd6, 0xb3, 0x29, 0xe3, 0x2f, 0x84,
    0x53, 0xd1, 0x00, 0xed, 0x20, 0xfc, 0xb1, 0x5b,
    0x6a, 0xcb, 0xbe, 0x39, 0x4a, 0x4c, 0x58, 0xcf,
    0xd0, 0xef, 0xaa, 0xfb, 0x43, 0x4d, 0x33, 0x85,
    0x45, 0xf9, 0x02, 0x7f, 0x50, 0x3c, 0x9f, 0xa8,
    0x51, 0xa3, 0x40, 0x8f, 0x92, 0x9d, 0x38, 0xf5,
    0xbc, 0xb6, 0xda, 0x21, 0x10, 0xff, 0xf3, 0xd2,
    0xcd, 0x0c, 0x13, 0xec, 0x5f, 0x97, 0x44, 0x17,
    0xc4, 0xa7, 0x7e, 0x3d, 0x64, 0x5d, 0x19, 0x73,
    0x60, 0x81, 0x4f, 0xdc, 0x22, 0x2a, 0x90, 0x88,
    0x46, 0xee, 0xb8, 0x14, 0xde, 0x5e, 0x0b, 0xdb,
    0xe0, 0x32, 0x3a, 0x0a, 0x49, 0x06, 0x24, 0x5c,
    0xc2, 0xd3, 0xac, 0x62, 0x91, 0x95, 0xe4, 0x79,
    0xe7, 0xc8, 0x37, 0x6d, 0x8d, 0xd5, 0x4e, 0xa9,
    0x6c, 0x56, 0xf4, 0xea, 0x65, 0x7a, 0xae, 0x08,
    0xba, 0x78, 0x25, 0x2e, 0x1c, 0xa6, 0xb4, 0xc6,
    0xe8, 0xdd, 0x74, 0x1f, 0x4b, 0xbd, 0x8b, 0x8a,
    0x70, 0x3e, 0xb5, 0x66, 0x48, 0x03, 0xf6, 0x0e,
    0x61, 0x35, 0x57, 0xb9, 0x86, 0xc1, 0x1d, 0x9e,
    0xe1, 0xf8, 0x98, 0x11, 0x69, 0xd9, 0x8e, 0x94,
    0x9b, 0x1e, 0x87, 0xe9, 0xce, 0x55, 0x28, 0xdf,
    0x8c, 0xa1, 0x89, 0x0d, 0xbf, 0xe6, 0x42, 0x68,
    0x41, 0x99, 0x2d, 0x0f, 0xb0, 0x54, 0xbb, 0x16
};

static const uint8_t mul4[] = {
	0x00, 0x04, 0x08, 0x0c, 0x10, 0x14, 0x18, 0x1c, 
	0x20, 0x24, 0x28, 0x2c, 0x30, 0x34, 0x38, 0x3c, 
	0x40, 0x44, 0x48, 0x4c, 0x50, 0x54, 0x58, 0x5c, 
	0x60, 0x64, 0x68, 0x6c, 0x70, 0x74, 0x78, 0x7c, 
	0x80, 0x84, 0x88, 0x8c, 0x90, 0x94, 0x98, 0x9c, 
	0xa0, 0xa4, 0xa8, 0xac, 0xb0, 0xb4, 0xb8, 0xbc, 
	0xc0, 0xc4, 0xc8, 0xcc, 0xd0, 0xd4, 0xd8, 0xdc, 
	0xe0, 0xe4, 0xe8, 0xec, 0xf0, 0xf4, 0xf8, 0xfc, 
	0x1b, 0x1f, 0x13, 0x17, 0x0b, 0x0f, 0x03, 0x07, 
	0x3b, 0x3f, 0x33, 0x37, 0x2b, 0x2f, 0x23, 0x27, 
	0x5b, 0x5f, 0x53, 0x57, 0x4b, 0x4f, 0x43, 0x47, 
	0x7b, 0x7f, 0x73, 0x77, 0x6b, 0x6f, 0x63, 0x67, 
	0x9b, 0x9f, 0x93, 0x97, 0x8b, 0x8f, 0x83, 0x87, 
	0xbb, 0xbf, 0xb3, 0xb7, 0xab, 0xaf, 0xa3, 0xa7, 
	0xdb, 0xdf, 0xd3, 0xd7, 0xcb, 0xcf, 0xc3, 0xc7, 
	0xfb, 0xff, 0xf3, 0xf7, 0xeb, 0xef, 0xe3, 0xe7, 
	0x36, 0x32, 0x3e, 0x3a, 0x26, 0x22, 0x2e, 0x2a, 
	0x16, 0x12, 0x1e, 0x1a, 0x06, 0x02, 0x0e, 0x0a, 
	0x76, 0x72, 0x7e, 0x7a, 0x66, 0x62, 0x6e, 0x6a, 
	0x56, 0x52, 0x5e, 0x5a, 0x46, 0x42, 0x4e, 0x4a, 
	0xb6, 0xb2, 0xbe, 0xba, 0xa6, 0xa2, 0xae, 0xaa, 
	0x96, 0x92, 0x9e, 0x9a, 0x86, 0x82, 0x8e, 0x8a, 
	0xf6, 0xf2, 0xfe, 0xfa, 0xe6, 0xe2, 0xee, 0xea, 
	0xd6, 0xd2, 0xde, 0xda, 0xc6, 0xc2, 0xce, 0xca, 
	0x2d, 0x29, 0x25, 0x21, 0x3d, 0x39, 0x35, 0x31, 
	0x0d, 0x09, 0x05, 0x01, 0x1d, 0x19, 0x15, 0x11, 
	0x6d, 0x69, 0x65, 0x61, 0x7d, 0x79, 0x75, 0x71, 
	0x4d, 0x49, 0x45, 0x41, 0x5d, 0x59, 0x55, 0x51, 
	0xad, 0xa9, 0xa5, 0xa1, 0xbd, 0xb9, 0xb5, 0xb1, 
	0x8d, 0x89, 0x85, 0x81, 0x9d, 0x99, 0x95, 0x91, 
	0xed, 0xe9, 0xe5, 0xe1, 0xfd, 0xf9, 0xf5, 0xf1, 
	0xcd, 0xc9, 0xc5, 0xc1, 0xdd, 0xd9, 0xd5, 0xd1, 
};
static const uint8_t mul5[] = {
	0x00, 0x05, 0x0a, 0x0f, 0x14, 0x11, 0x1e, 0x1b, 
	0x28, 0x2d, 0x22, 0x27, 0x3c, 0x39, 0x36, 0x33, 
	0x50, 0x55, 0x5a, 0x5f, 0x44, 0x41, 0x4e, 0x4b, 
	0x78, 0x7d, 0x72, 0x77, 0x6c, 0x69, 0x66, 0x63, 
	0xa0, 0xa5, 0xaa, 0xaf, 0xb4, 0xb1, 0xbe, 0xbb, 
	0x88, 0x8d, 0x82, 0x87, 0x9c, 0x99, 0x96, 0x93, 
	0xf0, 0xf5, 0xfa, 0xff, 0xe4, 0xe1, 0xee, 0xeb, 
	0xd8, 0xdd, 0xd2, 0xd7, 0xcc, 0xc9, 0xc6, 0xc3, 
	0x5b, 0x5e, 0x51, 0x54, 0x4f, 0x4a, 0x45, 0x40, 
	0x73, 0x76, 0x79, 0x7c, 0x67, 0x62, 0x6d, 0x68, 
	0x0b, 0x0e, 0x01, 0x04, 0x1f, 0x1a, 0x15, 0x10, 
	0x23, 0x26, 0x29, 0x2c, 0x37, 0x32, 0x3d, 0x38, 
	0xfb, 0xfe, 0xf1, 0xf4, 0xef, 0xea, 0xe5, 0xe0, 
	0xd3, 0xd6, 0xd9, 0xdc, 0xc7, 0xc2, 0xcd, 0xc8, 
	0xab, 0xae, 0xa1, 0xa4, 0xbf, 0xba, 0xb5, 0xb0, 
	0x83, 0x86, 0x89, 0x8c, 0x97, 0x92, 0x9d, 0x98, 
	0xb6, 0xb3, 0xbc, 0xb9, 0xa2, 0xa7, 0xa8, 0xad, 
	0x9e, 0x9b, 0x94, 0x91, 0x8a, 0x8f, 0x80, 0x85, 
	0xe6, 0xe3, 0xec, 0xe9, 0xf2, 0xf7, 0xf8, 0xfd, 
	0xce, 0xcb, 0xc4, 0xc1, 0xda, 0xdf, 0xd0, 0xd5, 
	0x16, 0x13, 0x1c, 0x19, 0x02, 0x07, 0x08, 0x0d, 
	0x3e, 0x3b, 0x34, 0x31, 0x2a, 0x2f, 0x20, 0x25, 
	0x46, 0x43, 0x4c, 0x49, 0x52, 0x57, 0x58, 0x5d, 
	0x6e, 0x6b, 0x64, 0x61, 0x7a, 0x7f, 0x70, 0x75, 
	0xed, 0xe8, 0xe7, 0xe2, 0xf9, 0xfc, 0xf3, 0xf6, 
	0xc5, 0xc0, 0xcf, 0xca, 0xd1, 0xd4, 0xdb, 0xde, 
	0xbd, 0xb8, 0xb7, 0xb2, 0xa9, 0xac, 0xa3, 0xa6, 
	0x95, 0x90, 0x9f, 0x9a, 0x81, 0x84, 0x8b, 0x8e, 
	0x4d, 0x48, 0x47, 0x42, 0x59, 0x5c, 0x53, 0x56, 
	0x65, 0x60, 0x6f, 0x6a, 0x71, 0x74, 0x7b, 0x7e, 
	0x1d, 0x18, 0x17, 0x12, 0x09, 0x0c, 0x03, 0x06, 
	0x35, 0x30, 0x3f, 0x3a, 0x21, 0x24, 0x2b, 0x2e, 
};
static const uint8_t mul6[] = {
	0x00, 0x06, 0x0c, 0x0a, 0x18, 0x1e, 0x14, 0x12, 
	0x30, 0x36, 0x3c, 0x3a, 0x28, 0x2e, 0x24, 0x22, 
	0x60, 0x66, 0x6c, 0x6a, 0x78, 0x7e, 0x74, 0x72, 
	0x50, 0x56, 0x5c, 0x5a, 0x48, 0x4e, 0x44, 0x42, 
	0xc0, 0xc6, 0xcc, 0xca, 0xd8, 0xde, 0xd4, 0xd2, 
	0xf0, 0xf6, 0xfc, 0xfa, 0xe8, 0xee, 0xe4, 0xe2, 
	0xa0, 0xa6, 0xac, 0xaa, 0xb8, 0xbe, 0xb4, 0xb2, 
	0x90, 0x96, 0x9c, 0x9a, 0x88, 0x8e, 0x84, 0x82, 
	0x9b, 0x9d, 0x97, 0x91, 0x83, 0x85, 0x8f, 0x89, 
	0xab, 0xad, 0xa7, 0xa1, 0xb3, 0xb5, 0xbf, 0xb9, 
	0xfb, 0xfd, 0xf7, 0xf1, 0xe3, 0xe5, 0xef, 0xe9, 
	0xcb, 0xcd, 0xc7, 0xc1, 0xd3, 0xd5, 0xdf, 0xd9, 
	0x5b, 0x5d, 0x57, 0x51, 0x43, 0x45, 0x4f, 0x49, 
	0x6b, 0x6d, 0x67, 0x61, 0x73, 0x75, 0x7f, 0x79, 
	0x3b, 0x3d, 0x37, 0x31, 0x23, 0x25, 0x2f, 0x29, 
	0x0b, 0x0d, 0x07, 0x01, 0x13, 0x15, 0x1f, 0x19, 
	0x2d, 0x2b, 0x21, 0x27, 0x35, 0x33, 0x39, 0x3f, 
	0x1d, 0x1b, 0x11, 0x17, 0x05, 0x03, 0x09, 0x0f, 
	0x4d, 0x4b, 0x41, 0x47, 0x55, 0x53, 0x59, 0x5f, 
	0x7d, 0x7b, 0x71, 0x77, 0x65, 0x63, 0x69, 0x6f, 
	0xed, 0xeb, 0xe1, 0xe7, 0xf5, 0xf3, 0xf9, 0xff, 
	0xdd, 0xdb, 0xd1, 0xd7, 0xc5, 0xc3, 0xc9, 0xcf, 
	0x8d, 0x8b, 0x81, 0x87, 0x95, 0x93, 0x99, 0x9f, 
	0xbd, 0xbb, 0xb1, 0xb7, 0xa5, 0xa3, 0xa9, 0xaf, 
	0xb6, 0xb0, 0xba, 0xbc, 0xae, 0xa8, 0xa2, 0xa4, 
	0x86, 0x80, 0x8a, 0x8c, 0x9e, 0x98, 0x92, 0x94, 
	0xd6, 0xd0, 0xda, 0xdc, 0xce, 0xc8, 0xc2, 0xc4, 
	0xe6, 0xe0, 0xea, 0xec, 0xfe, 0xf8, 0xf2, 0xf4, 
	0x76, 0x70, 0x7a, 0x7c, 0x6e, 0x68, 0x62, 0x64, 
	0x46, 0x40, 0x4a, 0x4c, 0x5e, 0x58, 0x52, 0x54, 
	0x16, 0x10, 0x1a, 0x1c, 0x0e, 0x08, 0x02, 0x04, 
	0x26, 0x20, 0x2a, 0x2c, 0x3e, 0x38, 0x32, 0x34, 
};
static const uint8_t mul7[] = {
	0x00, 0x07, 0x0e, 0x09, 0x1c, 0x1b, 0x12, 0x15, 
	0x38, 0x3f, 0x36, 0x31, 0x24, 0x23, 0x2a, 0x2d, 
	0x70, 0x77, 0x7e, 0x79, 0x6c, 0x6b, 0x62, 0x65, 
	0x48, 0x4f, 0x46, 0x41, 0x54, 0x53, 0x5a, 0x5d, 
	0xe0, 0xe7, 0xee, 0xe9, 0xfc, 0xfb, 0xf2, 0xf5, 
	0xd8, 0xdf, 0xd6, 0xd1, 0xc4, 0xc3, 0xca, 0xcd, 
	0x90, 0x97, 0x9e, 0x99, 0x8c, 0x8b, 0x82, 0x85, 
	0xa8, 0xaf, 0xa6, 0xa1, 0xb4, 0xb3, 0xba, 0xbd, 
	0xdb, 0xdc, 0xd5, 0xd2, 0xc7, 0xc0, 0xc9, 0xce, 
	0xe3, 0xe4, 0xed, 0xea, 0xff, 0xf8, 0xf1, 0xf6, 
	0xab, 0xac, 0xa5, 0xa2, 0xb7, 0xb0, 0xb9, 0xbe, 
	0x93, 0x94, 0x9d, 0x9a, 0x8f, 0x88, 0x81, 0x86, 
	0x3b, 0x3c, 0x35, 0x32, 0x27, 0x20, 0x29, 0x2e, 
	0x03, 0x04, 0x0d, 0x0a, 0x1f, 0x18, 0x11, 0x16, 
	0x4b, 0x4c, 0x45, 0x42, 0x57, 0x50, 0x59, 0x5e, 
	0x73, 0x74, 0x7d, 0x7a, 0x6f, 0x68, 0x61, 0x66, 
	0xad, 0xaa, 0xa3, 0xa4, 0xb1, 0xb6, 0xbf, 0xb8, 
	0x95, 0x92, 0x9b, 0x9c, 0x89, 0x8e, 0x87, 0x80, 
	0xdd, 0xda, 0xd3, 0xd4, 0xc1, 0xc6, 0xcf, 0xc8, 
	0xe5, 0xe2, 0xeb, 0xec, 0xf9, 0xfe, 0xf7, 0xf0, 
	0x4d, 0x4a, 0x43, 0x44, 0x51, 0x56, 0x5f, 0x58, 
	0x75, 0x72, 0x7b, 0x7c, 0x69, 0x6e, 0x67, 0x60, 
	0x3d, 0x3a, 0x33, 0x34, 0x21, 0x26, 0x2f, 0x28, 
	0x05, 0x02, 0x0b, 0x0c, 0x19, 0x1e, 0x17, 0x10, 
	0x76, 0x71, 0x78, 0x7f, 0x6a, 0x6d, 0x64, 0x63, 
	0x4e, 0x49, 0x40, 0x47, 0x52, 0x55, 0x5c, 0x5b, 
	0x06, 0x01, 0x08, 0x0f, 0x1a, 0x1d, 0x14, 0x13, 
	0x3e, 0x39, 0x30, 0x37, 0x22, 0x25, 0x2c, 0x2b, 
	0x96, 0x91, 0x98, 0x9f, 0x8a, 0x8d, 0x84, 0x83, 
	0xae, 0xa9, 0xa0, 0xa7, 0xb2, 0xb5, 0xbc, 0xbb, 
	0xe6, 0xe1, 0xe8, 0xef, 0xfa, 0xfd, 0xf4, 0xf3, 
	0xde, 0xd9, 0xd0, 0xd7, 0xc2, 0xc5, 0xcc, 0xcb, 
};

#if 0
void dump(const uint32_t *s)
{
	for (size_t i = 0; i < 30; i++)
		printf("%02zu: %08x\n", i, s[i]);
}
#endif

inline void tix_256(uint32_t *s, uint32_t p)
{
	s[10] ^= s[0];
	s[0] = p;
	s[8] ^= s[0];
	s[1] ^= s[24];
}

inline void ror_256(uint32_t *t, const uint32_t *s, const size_t off)
{
	const size_t bufsize = 30;
	for (size_t i = 0; i < bufsize; i++)
		t[i] = s[(i+bufsize-off) % bufsize];
}

inline void cmix_256(uint32_t *s)
{
	s[ 0] ^= s[4];
	s[ 1] ^= s[5];
	s[ 2] ^= s[6];
	s[15] ^= s[4];
	s[16] ^= s[5];
	s[17] ^= s[6];
}

#define OFF(x, r) ((x+30-r) % 30)
inline void ror3cmix_256(uint32_t *t, const uint32_t *s)
{
	t[ 0] = s[OFF( 0, 3)] ^ s[OFF(4, 3)];
	t[ 1] = s[OFF( 1, 3)] ^ s[OFF(5, 3)];
	t[ 2] = s[OFF( 2, 3)] ^ s[OFF(6, 3)];
	t[ 3] = s[OFF( 3, 3)];
	t[ 4] = s[OFF( 4, 3)];
	t[ 5] = s[OFF( 5, 3)];
	t[ 6] = s[OFF( 6, 3)];
	t[ 7] = s[OFF( 7, 3)];
	t[ 8] = s[OFF( 8, 3)];
	t[ 9] = s[OFF( 9, 3)];
	t[10] = s[OFF(10, 3)];
	t[11] = s[OFF(11, 3)];
	t[12] = s[OFF(12, 3)];
	t[13] = s[OFF(13, 3)];
	t[14] = s[OFF(14, 3)];
	t[15] = s[OFF(15, 3)] ^ s[OFF(4, 3)];
	t[16] = s[OFF(16, 3)] ^ s[OFF(5, 3)];
	t[17] = s[OFF(17, 3)] ^ s[OFF(6, 3)];
	t[18] = s[OFF(18, 3)];
	t[19] = s[OFF(19, 3)];
	t[20] = s[OFF(20, 3)];
	t[21] = s[OFF(21, 3)];
	t[22] = s[OFF(22, 3)];
	t[23] = s[OFF(23, 3)];
	t[24] = s[OFF(24, 3)];
	t[25] = s[OFF(25, 3)];
	t[26] = s[OFF(26, 3)];
	t[27] = s[OFF(27, 3)];
	t[28] = s[OFF(28, 3)];
	t[29] = s[OFF(29, 3)];
}
#undef OFF

inline void substitute(uint32_t *s)
{
	uint8_t *a = (uint8_t *)s;

	for (size_t i = 0; i < 16; i++)
		a[i] = sbox[a[i]];
}

#if DREW_BYTE_ORDER == DREW_BIG_ENDIAN
#define B(x) t[x]
#define BS(x) s[x]
#else
#define B(x) t[(x) ^ 3]
#define BS(x) s[(x) ^ 3]
#endif
inline void smix(uint32_t *sb, uint32_t *tb)
{
	uint8_t *t = (uint8_t *)tb, *s = (uint8_t *)sb;
	substitute(tb);

	BS( 0) = B(0) ^ mul4[B(1)] ^ mul7[B(2)] ^ B(3) ^ B(4) ^ B(8) ^ B(12);
	BS( 1) = B(1) ^ B(4) ^ B(5) ^ mul4[B(6)] ^ mul7[B(7)] ^ B(9) ^ B(13);
	BS( 2) = B(2) ^ B(6) ^ mul7[B(8)] ^ B(9) ^ B(10) ^ mul4[B(11)] ^ B(14);
	BS( 3) = B(3) ^ B(7) ^ B(11) ^ mul4[B(12)] ^ mul7[B(13)] ^ B(14) ^ B(15);

	BS( 4) = mul4[B(5)] ^ mul7[B(6)] ^ B(7) ^ B(8) ^ B(12);
	BS( 5) = B(1) ^ B(8) ^ mul4[B(10)] ^ mul7[B(11)] ^ B(13);
	BS( 6) = B(2) ^ B(6) ^ mul7[B(12)] ^ B(13) ^ mul4[B(15)];
	BS( 7) = mul4[B(0)] ^ mul7[B(1)] ^ B(2) ^ B(7) ^ B(11);

	BS( 8) = mul7[B(4)] ^ mul6[B(8)] ^ mul4[B(9)] ^ mul7[B(10)] ^ B(11) ^
		mul7[B(12)];
	BS( 9) = mul7[B(1)] ^ mul7[B(9)] ^ B(12) ^ mul6[B(13)] ^ mul4[B(14)] ^
		mul7[B(15)];
	BS(10) = mul7[B(0)] ^ B(1) ^ mul6[B(2)] ^ mul4[B(3)] ^ mul7[B(6)] ^
		mul7[B(14)];
	BS(11) = mul7[B(3)] ^ mul4[B(4)] ^ mul7[B(5)] ^ B(6) ^ mul6[B(7)] ^
		mul7[B(11)];

	BS(12) = mul4[B(4)] ^ mul4[B(8)] ^ mul5[B(12)] ^ mul4[B(13)] ^ mul7[B(14)] ^
		B(15);
	BS(13) = B(0) ^ mul5[B(1)] ^ mul4[B(2)] ^ mul7[B(3)] ^ mul4[B(9)] ^
		mul4[B(13)];
	BS(14) = mul4[B(2)] ^ mul7[B(4)] ^ B(5) ^ mul5[B(6)] ^ mul4[B(7)] ^
		mul4[B(14)];
	BS(15) = mul4[B(3)] ^ mul4[B(7)] ^ mul4[B(8)] ^ mul7[B(9)] ^ B(10) ^
		mul5[B(11)];
}

inline void smix_256(uint32_t *sb, uint32_t *tb)
{
	smix(sb, tb);
	memcpy(sb+4, tb+4, sizeof(*sb) * (30 - 4));
}

void drew::Fugue256Transform::Transform(uint32_t *state, const uint8_t *block)
{
	uint32_t p;
	uint32_t tmp[30];

	p = E::Convert<uint32_t>(block);
	tix_256(state, p);
	ror3cmix_256(tmp, state);
	smix_256(state, tmp);
	ror3cmix_256(tmp, state);
	smix_256(state, tmp);
}

void drew::Fugue256Transform::Pad(uint32_t *state, uint8_t *buf,
		uint32_t len[2])
{
	size_t off = len[0] & 3;
	uint8_t block[8] = {0};

	if (off) {
		memcpy(block, buf, off);
		Transform(state, block);
	}

	len[1] <<= 3;
	len[1] |= (len[0] >> (32 - 3));
	len[0] <<= 3;
	std::swap(len[0], len[1]);
	
	E::Copy(block, len, 8);
	Transform(state, block);
	Transform(state, block+4);
}

void drew::Fugue256Transform::Final(uint32_t *state)
{
	for (size_t i = 0; i < 10; i++) {
		uint32_t tmp[30];
		ror3cmix_256(tmp, state);
		smix_256(state, tmp);
	}


	for (size_t i = 0; i < 13; i++) {
		uint32_t tmp[30];
		state[ 4] ^= state[0];
		state[15] ^= state[0];
		ror_256(tmp, state, 15);
		smix_256(state, tmp);
		state[ 4] ^= state[0];
		state[16] ^= state[0];
		ror_256(tmp, state, 14);
		smix_256(state, tmp);
	}

	state[ 4] ^= state[0];
	state[15] ^= state[0];
}

void drew::Fugue256::Pad()
{
	Fugue256Transform::Pad(m_hash, m_buf, m_len);
}

void drew::Fugue224::Pad()
{
	Fugue256Transform::Pad(m_hash, m_buf, m_len);
}

void drew::Fugue256::GetDigest(uint8_t *digest, size_t len, bool nopad)
{
	if (!nopad)
		Pad();

	Fugue256Transform::Final(m_hash);

	E::CopyCarefully(digest, m_hash+1, std::min<size_t>(len, 16));
	if (len > 16)
		E::CopyCarefully(digest+16, m_hash+15, std::min<size_t>(len-16, 16));
}

void drew::Fugue224::GetDigest(uint8_t *digest, size_t len, bool nopad)
{
	if (!nopad)
		Pad();

	Fugue256Transform::Final(m_hash);

	E::CopyCarefully(digest, m_hash+1, std::min<size_t>(len, 16));
	if (len > 16)
		E::CopyCarefully(digest+16, m_hash+15, std::min<size_t>(len-16, 12));
}
