#ifndef ENDIAN_HH
#define ENDIAN_HH

#include "util.h"

template<class T, size_t N>
struct AlignedBlock
{
	T data[N] ALIGNED_T;
};

inline bool IsAligned(const void *p, size_t mul)
{
	uintptr_t q = reinterpret_cast<uintptr_t>(p);
	return !(q & (mul - 1));
}

template<class T>
inline bool IsAligned(const void *p)
{
#if defined(__GNUC__)
	return IsAligned(p, __alignof__(T));
#else
	return IsAligned(p, sizeof(T));
#endif
}

template<class T>
inline size_t GetNeededAlignment()
{
#if defined(NEEDS_ALIGNMENT) && (NEEDS_ALIGNMENT-0 == 0)
	return 1;
#elif defined(__GNUC__)
	return __alignof__(T);
#else
	return sizeof(T);
#endif
}

template<class T>
inline bool IsSufficientlyAligned(const void *p)
{
	return IsAligned(p, GetNeededAlignment<T>());
}

inline int GetSystemEndianness()
{
#if BYTE_ORDER == BIG_ENDIAN
	return 4321;
#else
	return 1234;
#endif
}

template<class T>
inline T RotateLeft(T x, size_t n)
{
	return (x << n) | (x >> ((sizeof(T)*8) - n));
}

template<class T>
inline T RotateRight(T x, size_t n)
{
	return (x >> n) | (x << ((sizeof(T)*8) - n));
}

class Endian
{
	public:
		inline static void CopyBackwards(uint8_t *dest, const uint8_t *src,
				size_t len, const size_t sz)
		{
			for (size_t i = 0; i < len; ) {
				const size_t blk = i;
				for (size_t j = 0; i < len && j < sz; j++, i++)
					dest[blk+j] = src[blk+(sz-j-1)];
			}
		}
		template<class T>
		inline static uint8_t GetByte(T x, size_t n)
		{
			return x >> (n * 8);
		}
		template<class T>
		inline static uint8_t GetByte(const T *p, size_t n)
		{
			return GetByte(*p, n);
		}
		template<class T>
		inline static uint8_t GetArrayByte(const T *p, size_t n)
		{
			return GetByte(p[n/sizeof(T)], (n & (sizeof(T)-1)));
		}
};

class BigEndian : public Endian
{
	public:
		template<class T>
		inline static uint8_t *Copy(uint8_t *dest, const T *src, size_t len)
		{
			return Copy(dest, src, len, sizeof(T));
		}
		template<class T>
		inline static T *Copy(T *dest, const uint8_t *src, size_t len)
		{
			return Copy(dest, src, len, sizeof(T));
		}
		template<class T>
		inline static uint8_t *Copy(uint8_t *dest, const T *src, size_t len,
				const size_t sz)
		{
#if BYTE_ORDER == BIG_ENDIAN
			memcpy(dest, src, len);
#else
			CopyBackwards(dest, reinterpret_cast<const uint8_t *>(src), len,
					sz);
#endif
			return dest;
		}
		template<class T>
		inline static T *Copy(T *dest, const uint8_t *src, size_t len,
				const size_t sz)
		{
#if BYTE_ORDER == BIG_ENDIAN
			memcpy(dest, src, len);
#else
			CopyBackwards(reinterpret_cast<uint8_t *>(dest), src, len, sz);
#endif
			return dest;
		}
		inline static uint8_t *Copy(uint8_t *dest, const uint8_t *src,
				size_t len, const size_t sz)
		{
			memcpy(dest, src, len);
			return dest;
		}
		inline static uint8_t *Copy(uint8_t *dest, const uint8_t *src,
				size_t len)
		{
			memcpy(dest, src, len);
			return dest;
		}
		template<class T>
		inline static const T *CopyIfNeeded(T *buf, const uint8_t *p,
				size_t len)
		{
			if (GetEndianness() == GetSystemEndianness() && 
					IsSufficientlyAligned<T>(p))
				return reinterpret_cast<const T *>(p);
			else
				return Copy(buf, p, len);
		}
		inline static int GetEndianness()
		{
			return 4321;
		}
	protected:
	private:
};

class LittleEndian : public Endian
{
	public:
		template<class T>
		inline static uint8_t *Copy(uint8_t *dest, const T *src, size_t len)
		{
			return Copy(dest, src, len, sizeof(T));
		}
		template<class T>
		inline static T *Copy(T *dest, const uint8_t *src, size_t len)
		{
			return Copy(dest, src, len, sizeof(T));
		}
		template<class T>
		inline static uint8_t *Copy(uint8_t *dest, const T *src, size_t len,
				const size_t sz)
		{
#if BYTE_ORDER == LITTLE_ENDIAN
			memcpy(dest, src, len);
#else
			CopyBackwards(dest, reinterpret_cast<const uint8_t *>(src), len,
					sz);
#endif
			return dest;
		}
		template<class T>
		inline static T *Copy(T *dest, const uint8_t *src, size_t len,
				const size_t sz)
		{
#if BYTE_ORDER == LITTLE_ENDIAN
			memcpy(dest, src, len);
#else
			CopyBackwards(reinterpret_cast<uint8_t *>(dest), src, len, sz);
#endif
			return dest;
		}
		inline static uint8_t *Copy(uint8_t *dest, const uint8_t *src,
				size_t len, const size_t sz)
		{
			memcpy(dest, src, len);
			return dest;
		}
		inline static uint8_t *Copy(uint8_t *dest, const uint8_t *src,
				size_t len)
		{
			memcpy(dest, src, len);
			return dest;
		}
		template<class T>
		inline static const T *CopyIfNeeded(T *buf, const uint8_t *p,
				size_t len)
		{
			if (GetEndianness() == GetSystemEndianness() && 
					IsSufficientlyAligned<T>(p))
				return reinterpret_cast<const T *>(p);
			else
				return Copy(buf, p, len);
		}
		inline static int GetEndianness()
		{
			return 1234;
		}
	protected:
	private:
};

#if BYTE_ORDER == BIG_ENDIAN
typedef BigEndian NativeEndian;
#else
typedef LittleEndian NativeEndian;
#endif

#endif
