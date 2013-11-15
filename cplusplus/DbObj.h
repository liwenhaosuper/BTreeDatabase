

#if !defined(__dbobj_h_)
#define __dbobj_h_

#include "smartptrs.h"

#include"stdafx.h"

namespace Database
{
	// Convenience definition
#ifndef byte
	typedef char byte;
#endif

	// Class representing the sort of objects we deal with. This is analogous to
	// a DBT (database thang) in the Berkeley DB package.
	class DbObj : public Database::RefCount
	{
	public:
		// Default constructor
		DbObj(void)
		{
			_data = 0;
			_size = 0;
		}

		// Constructor taking a pointer and a size. Make a copy of the _data.
		DbObj(void* pd, size_t sz)
		{
			_size = sz;
			if (_size != 0)
			{
				_data = new byte[_size];
				memcpy(_data, pd, _size);
			}
		}

		// Constructor taking a std::string reference.
		DbObj(const std::string& s)
		{
			_size = s.length();
			_data = new byte[_size];
			memcpy(_data, s.c_str(), _size);
		}

		// Constructor taking a (possibly) null terminated string.
		DbObj(const char* ps, size_t sz = 0)
		{
			_size = (0 == sz) ? strlen(ps) : sz;
			_data = new byte[_size];
			memcpy(_data, ps, _size);
		}

		// Constructor taking a 32-bit unsigned int
		DbObj(unsigned long ul)
		{
			_size = sizeof(unsigned long);
			_data = new byte[_size];
			*((unsigned long*)_data) = ul;
		}

		// Constructor taking a 32-bit int
		DbObj(long l)
		{
			_size = sizeof(long);
			_data = new byte[_size];
			*((long*)_data) = l;
		}

		// Constructor taking a 16-bit unsigned int
		DbObj(unsigned short us)
		{
			_size = sizeof(unsigned short);
			_data = new byte[_size];
			*((unsigned short*)_data) = us;
		}

		// Constructor taking a 16-bit int
		DbObj(short s)
		{
			_size = sizeof(short);
			_data = new byte[_size];
			*((short*)_data) = s;
		}

		// Copy constructor. Call the assignment operator.
		DbObj(DbObj& obj)
		{
			_data = 0;
			_size = 0;
			operator=(obj);
		}

		// Destructor. Delete the pointer if the size is non-zero.
		~DbObj()
		{
			if (_size != 0)
			{
				delete[] _data;
				_data = 0;
				_size = 0;
			}
		}

		// Assignment operator. Make a copy of the other object.
		DbObj& operator=(DbObj& obj)
		{
			if (_size != 0)
			{
				delete[] _data;
			}
			if (obj._size > 0)
			{
				_size = obj._size;
				_data = new byte[_size];
				memcpy(_data, obj._data, _size);
			}
		}

	public:

		void* getData()  { return _data; }
		size_t getSize() const { return _size; }
		void setData(const void* pd, size_t sz)
		{
			if (_size)
			{
				delete[] _data;
			}
			_size = sz;
			_data = new byte[_size];
			memcpy(_data, pd, _size);
		}

	private:
		byte* _data;
		size_t _size;
	};

	typedef Database::Ptr<DbObj> DbObjPtr;
	typedef std::vector<DbObjPtr> DBOBJVECTOR;
	typedef std::list<DbObjPtr> DBOBJLIST;
};

#endif
